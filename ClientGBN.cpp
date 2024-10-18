#include <stdlib.h>
#include <stdio.h> 
#include <WinSock2.h> 
#include <time.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include "afxres.h" 
#pragma comment(lib,"ws2_32.lib") 

using namespace std;

 
#define SERVER_PORT 8080 //接收数据的端口号 
#define SERVER_IP "127.0.0.1" // 服务器的IP地址 
#define savePath "E:\\Web\\experiment3\\Server2Client\\Server2ClientGBN.txt"
#define endure 3
 
const int BUFFER_LENGTH = 1026;     //缓冲区大小
const int SEQ_SIZE = 20;            //接收端序列号个数，为1~20
const int SEND_WIND_SIZE = 10;      //发送窗口大小
BOOL ack[SEQ_SIZE];                 //收到ack情况，对应0~19的ack 
int curSeq;                         //当前数据包的seq 
int curAck;                         //当前等待确认的ack 
int totalSeq;                       //收到的包的总数 
int totalPacket;                    //需要发送的包总数
int latestACK;                      //当前收到最新的ACK
int repeatACKCount;                 //收到重复的ACK数 
 
/****************************************************************/ 
/*  -time 从服务器端获取当前时间 
    -quit 退出客户端 
    -testGBN-Single [X] [Y]测试GBN协议实现可靠数据传输(单项) 
    -testGBN-Both [X] [Y]测试GBN协议实现可靠数据传输(双向)
        [X] [0,1] 模拟数据包丢失的概率 
        [Y] [0,1] 模拟ACK丢失的概率 
*/ 
/****************************************************************/ 
void printTips(){ 
    printf("**********************************************\n"); 
    printf("|     -time to get current time              |\n"); 
    printf("|     -quit to exit client                   |\n"); 
    printf("|     -testGBN-Single [X] [Y] to test the gbn  |\n"); 
    printf("|     -testGBN-Both [X] [Y] to test the gbn  |\n"); 
    printf("**********************************************\n"); 
} 
 
//************************************ 
// Method:    lossInLossRatio 
// FullName:  lossInLossRatio 
// Access:    public  
// Returns:   BOOL 
// Qualifier: 根据丢失率随机生成一个数字，判断是否丢失，丢失则返回TRUE，否则返回FALSE 
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio){ 
    int lossBound = (int) (lossRatio * 100); 
    int r = rand() % 101; 
    if(r <= lossBound){ 
        return TRUE; 
    } 
    return FALSE; 
}

//************************************ 
// Method:    seqIsAvailable 
// FullName:  seqIsAvailable 
// Access:    public  
// Returns:   bool 
// Qualifier: 当前序列号 curSeq 是否可用 
//************************************ 
bool seqIsAvailable(){ 
   int step; 
   step = curSeq - curAck; 
   step = step >= 0 ? step : step + SEQ_SIZE; 
   //序列号是否在当前发送窗口之内 
   if(step >= SEND_WIND_SIZE){ 
      return false; 
   } 
   if(ack[curSeq]){ 
      return true; 
   } 
   return false; 
} 

//************************************ 
// Method:    timeoutHandler 
// FullName:  timeoutHandler 
// Access:    public  
// Returns:   void 
// Qualifier: 超时重传处理函数，滑动窗口内的数据帧都要重传 
//************************************ 
void timeoutHandler(){ 
   int index; 
   for(int i = 0; i < SEND_WIND_SIZE; ++i){ 
      index = (i + curAck) % SEQ_SIZE; 
      ack[index] = TRUE; 
   }

   totalSeq -= curSeq >= curAck ? curSeq - curAck : (SEQ_SIZE - curAck) + curSeq; 
   curSeq = curAck; 
}

//************************************ 
// Method:    ackHandler 
// FullName:  ackHandler 
// Access:    public  
// Returns:   void 
// Qualifier: 收到ack，累积确认，取数据帧的第一个字节 
//由于发送数据时，第一个字节（序列号）为0（ASCII）时发送失败，因此加一了，此处需要减一还原 
// Parameter: char c 
//************************************ 
bool ackHandler(int index){ 
   //累计确认
   if(curAck <= index){ 
      for(int i = curAck; i <= index; ++i){ 
         ack[i] = TRUE; 
      } 
      curAck = (index + 1) % SEQ_SIZE; 
   }else{ //ack超过了最大值，回到了curAck的左边 
      for(int i = curAck; i < SEQ_SIZE; ++i){
         ack[i] = TRUE; 
      } 
      for(int i = 0; i<= index;++i){ 
         ack[i] = TRUE; 
      } 
      curAck = index + 1; 
   }

   //快重传(收到某重复 ACK 3次后立即重传)
   if (index == latestACK){
      repeatACKCount ++;
      if(repeatACKCount >= 3){
         repeatACKCount = 0;
         return false;
      }
   }else{
      latestACK = index;
      repeatACKCount = 0;
   }
   return true;
}
 
int main(int argc, char* argv[]) { 
    //加载套接字库（必须） 
    WORD wVersionRequested; 
    WSADATA wsaData; 
    //套接字加载时错误提示 
    int err; 
    //版本2.2 
    wVersionRequested = MAKEWORD(2, 2); 
    //加载dll文件Scoket库  
    err = WSAStartup(wVersionRequested, &wsaData); 
    if(err != 0){ 
        //找不到winsock.dll 
        printf("WSAStartup failed with error: %d\n", err); 
        return 1; 
    } 
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2){ 
        printf("Could not find a usable version of Winsock.dll\n"); 
        WSACleanup(); 
    }else{ 
        printf("The Winsock 2.2 dll was found okay\n"); 
    }

    SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0); 
    SOCKADDR_IN addrServer; 
    addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP); 
    addrServer.sin_family = AF_INET; 
    addrServer.sin_port = htons(SERVER_PORT); 

    //接收缓冲区 
    char buffer[BUFFER_LENGTH]; 
    ZeroMemory(buffer,sizeof(buffer)); 
    int len = sizeof(SOCKADDR); 

    //为了测试与服务器的连接，可以使用 -time 命令从服务器端获得当前时间 
    //其中[X]表示数据包丢失概率 [Y]表示ACK丢包概率 
    printTips(); 
    int ret; 
    int interval = 1;//收到数据包之后返回ack的间隔，默认为1表示每个都返回ack，0或者负数均表示所有的都不返回ack 
    char cmd[128]; 
    float packetLossRatio = 0.2; //默认包丢失率0.2 
    float ackLossRatio = 0.2; //默认ACK丢失率0.2 

    //用时间作为随机种子，放在循环的最外面 
    srand((unsigned)time(NULL)); 

    while(true){ 
        gets(buffer);
        ret = sscanf(buffer,"%s%f%f",&cmd,&packetLossRatio,&ackLossRatio); 

        //开始GBN测试，使用GBN协议实现UDP可靠文件传输 
        if(strcmp(cmd,"-testGBN-Single") == 0){ 
            printf("%s\n","Begin to test GBN protocol, please don't abort the process"); 
            printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n",packetLossRatio,ackLossRatio); 
            int waitCount = 0; 
            int stage = 0; 
            BOOL b; 
            unsigned char u_code;       //状态码 
            unsigned short seq;         //包的序列号 
            unsigned short recvSeq;     //接收窗口大小为1，已确认的序列号 
            unsigned short waitSeq;     //等待的序列号
            int totalPacketNum;         //待接收包的总数
            int receivedPacketNum = 0;  //总共收到的包数
            ofstream file(savePath);
            if (!file.is_open()) {
                cout << "Error opening file" << endl;
                return 1;
            }
            sendto(socketClient, "-testGBN-Single", strlen("-testGBN-Single")+1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 

            while (true){ 
                //等待server回复设置UDP为阻塞模式 
                recvfrom(socketClient,buffer,BUFFER_LENGTH,0,(SOCKADDR*)&addrServer, &len);
                switch(stage){ 

                    case 0://等待握手阶段 
                        u_code = (unsigned char)buffer[0]; 
                        if ((unsigned char)buffer[0] == 205) { 
                            printf("Ready for file transmission\n"); 
                            buffer[0] = 200; 
                            buffer[1] = '\0'; 
                            sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
                            stage = 1; 
                        } 
                        break;

                    case 1://接收包数阶段 
                        b = lossInLossRatio(packetLossRatio);   //模拟服务端发送包丢失
                        if(b){ 
                            printf("Total Packet Num Loss\n"); 
                            continue; 
                        }
                        totalPacketNum = buffer[1]; 
                        printf("Total Packet Num is %d\n", totalPacketNum);
                        buffer[0] = 255; 
                        buffer[1] = '\0';

                        b = lossInLossRatio(ackLossRatio);  //模拟ack丢失
                        if(b){ 
                            printf("The ack of 255 loss\n"); 
                            continue; 
                        }  
                        sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
                        stage = 2; 
                        recvSeq = 0; 
                        waitSeq = 1;
                        Sleep(500);
                        break;
                        

                    case 2://等待接收数据阶段 
                        seq = (unsigned char)buffer[0];
                        //屏蔽状态码为254的消息
                        if(seq == 254) continue;

                        //随机法模拟包是否丢失 
                        b = lossInLossRatio(packetLossRatio); 
                        if(b){ 
                            printf("The packet with a seq of %d loss\n",seq); 
                            continue; 
                        } 
                        printf("recv a packet with a seq of %d\n",seq);
                        if(!(waitSeq - seq)){  
                            ++waitSeq; 
                            if(waitSeq == 21){   
                                waitSeq = 1; 
                            }
                            //输出数据 
                            printf("%s\n",&buffer[1]);
                            file.write(&buffer[1], sizeof(buffer) - 2);
                            buffer[0] = seq; 
                            recvSeq = seq; 
                            buffer[1] = '\0';
                            receivedPacketNum ++; 
                        }else{ //如果当前一个包都没有收到，则等待Seq为1的数据包，不是则不返回ACK（因为并没有上一个正确的ACK） 
                            if(!recvSeq){ 
                                continue; 
                            } 
                            buffer[0] = recvSeq; 
                            buffer[1] = '\0'; 
                        } 

                        b = lossInLossRatio(ackLossRatio); 
                        if(b){ 
                            printf("The ack of %d loss\n",(unsigned char)buffer[0]); 
                            if(receivedPacketNum != totalPacketNum)continue; 
                        } 
                        else{
                            sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
                            printf("send a ack of %d\n",(unsigned char)buffer[0]);
                        }

                        if(receivedPacketNum == totalPacketNum){    
                            printf("Receive complete!!!\n");
                            file.close();
                            exit(0);
                        }  
                        break; 
                }
                Sleep(500); 
            }
        }

        //用户上传文件
        else if(strcmp(cmd,"-testGBN-Both") == 0){       

            int iMode = 1; //1：非阻塞，0：阻塞 
            ioctlsocket(socketClient, FIONBIO, (u_long FAR*) &iMode);//非阻塞设置

            sendto(socketClient, "-testGBN-Both", strlen("-testGBN-Both")+1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 

            //将测试数据读入内存 
            ifstream icin("E:\\Web\\experiment3\\test.txt"); 
            char data[1024 * 113]; 
            ZeroMemory(data,sizeof(data)); 
            streamsize bytesRead = icin.read(data, 1024 * 113).gcount();
            data[bytesRead] = '\0';
            icin.close(); 
            totalPacket = static_cast<int>(ceil((double)bytesRead / 1024));
            int recvSize ; 
            for(int i = 0; i < SEQ_SIZE; ++i){ 
                ack[i] = TRUE; 
            }

            BOOL b;
            ZeroMemory(buffer,sizeof(buffer)); 
            int waitCount = 0;
            int stage = 0;
            bool runFlag = true;
            int length = sizeof(SOCKADDR); 
            int repeatTime = 0;
            while(runFlag){ 
                switch(stage){ 

                    case 0://发送205阶段
                        buffer[0] = 205; 
                        sendto(socketClient, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
                        Sleep(100); 
                        stage = 1; 
                        break;

                    case 1://等待接收200阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始 
                        recvSize = recvfrom(socketClient,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrServer), &length); 
                        if(recvSize < 0){ 
                            ++waitCount; 
                            if(waitCount > 20){  //超时回到发送205阶段
                                printf("Timeout error\n");
                                stage = 0; 
                                break; 
                            } 
                            Sleep(500); 
                            continue; 
                        }else{ 
                            if((unsigned char)buffer[0] == 200){ 
                                curSeq = 0; 
                                curAck = 0; 
                                totalSeq = 0; 
                                waitCount = 0; 
                                stage = 2; 
                            } 
                        } 
                        break;
                    
                    case 2:
                        while(true){
                            buffer[0] = 254;
                            buffer[1] = totalPacket;
                            b = lossInLossRatio(packetLossRatio);   //模拟发包总数丢失
                            if(b){ 
                                printf("Total Packet Num Loss\n");  
                            }
                            sendto(socketClient, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
                            b = lossInLossRatio(ackLossRatio);  //模拟ack丢失
                            if(b){ 
                                printf("The ack of 255 loss\n"); 
                                continue; 
                            }
                            recvSize = recvfrom(socketClient,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrServer),&length);  
                            if((unsigned char)buffer[0] == 255){ 
                                printf("Begin a file transfer\n"); 
                                printf("File size is %dB, each packet is 1024B and packet total num is %d\n", bytesRead, totalPacket); 
                                curSeq = 0; 
                                curAck = 0; 
                                totalSeq = 0; 
                                waitCount = 0; 
                                stage = 3;
                                break; 
                            }
                            Sleep(500); 
                        }
                        break;

                    case 3:
                        if(seqIsAvailable() && totalSeq < totalPacket){ 
                            //发送给服务端的序列号从1开始
                            buffer[0] = curSeq + 1; 
                            ack[curSeq] = FALSE;
                            memcpy(&buffer[1], data + 1024 * totalSeq, 1024); 
                            printf("send a packet with a seq of %d\n",curSeq);
                            b = lossInLossRatio(packetLossRatio); 
                            if(b){ 
                                printf("The packet with a seq of %d loss\n", curSeq); 
                                continue; 
                            } 
                            sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
                            ++curSeq; 
                            curSeq %= SEQ_SIZE; 
                            ++totalSeq; 
                            Sleep(500); 
                        }

                        //等待Ack，若没有收到，则返回值为-1，计数器+1 
                        recvSize = recvfrom(socketClient,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrServer),&length);
                        b = lossInLossRatio(ackLossRatio); 
                        if(b){ 
                            printf("The ack of %d loss\n",(unsigned char)buffer[0] - 1);
                            recvSize  = -1;  
                        } 
                        if(recvSize < 0){ 
                            waitCount++; 
                            //20次等待ack则超时重传 
                            if (waitCount > 20){ 
                                printf("Timer out error.\n");
                                repeatTime++;

                                //未收到最后的确认，重发3次
                                if(totalSeq == totalPacket && repeatTime >= endure){
                                    printf("Transmission complete !!!\n");
                                    exit(0);
                                }


                                timeoutHandler(); 
                                waitCount = 0; 
                            } 
                        }else{
                            //收到ack
                            repeatTime = 0;
                            unsigned char index = (unsigned char)buffer[0] - 1; //序列号减一 
                            printf("Recv a ack of %d\n",index);

                            //收到了最后的确认，直接退出
                            if(totalSeq == totalPacket && index == totalPacket % SEQ_SIZE - 1){
                                printf("Transmission complete !!!\n");
                                runFlag = false;
                                exit(0);
                            }

                            //收到ack 
                            if(ackHandler(index)){
                                waitCount = 0;
                            }else{
                                cout << "Fast retransmission occurs!!!!" << endl;
                                timeoutHandler();
                            } 
                        } 
                        Sleep(500); 
                        break;
                } 
            } 
        }

        sendto(socketClient, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR)); 
        ret = recvfrom(socketClient,buffer,BUFFER_LENGTH,0,(SOCKADDR*)&addrServer, &len);
        printf("%s\n",buffer); 
        if(!strcmp(buffer,"Good bye!")){ 
            break; 
        } 
        printTips();
    } 
    //关闭套接字 
    closesocket(socketClient); 
    WSACleanup(); 
    return 0; 
}
