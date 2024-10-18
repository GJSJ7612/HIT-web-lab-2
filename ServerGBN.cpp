#include <stdlib.h>
#include <iostream>
#include <time.h> 
#include <WinSock2.h> 
#include <fstream>
#include <cmath> 
#include "afxres.h" 

using namespace std;

#pragma comment(lib,"ws2_32.lib")
#define SERVER_PORT 8080           //端口号 
#define SERVER_IP  "127.0.0.1"        //IP地址
#define FILE_PATH "E:\\Web\\experiment3\\test.txt"
#define savePath "E:\\Web\\experiment3\\Client2Server\\Client2ServerGBN.txt"
#define endure 3

const int BUFFER_LENGTH = 1026;     //缓冲区大小，（以太网中UDP的数据帧中包长度应小于1480字节） 
const int SEND_WIND_SIZE = 10;      //发送窗口大小为10，GBN中应满足 W + 1 <= N（W为发送窗口大小，N为序列号个数） 
                                    //本例取序列号0...19共20个 
                                    //如果将窗口大小设为1，则为停-等协议 
 
const int SEQ_SIZE = 20;            //序列号的个数，从0~19共计20个 
                                    //由于发送数据第一个字节如果值为0，则数据会发送失败 
                                    //因此接收端序列号为1~20，与发送端一一对应 
 
BOOL ack[SEQ_SIZE];     //收到ack情况，对应0~19的ack 
int curSeq;             //当前数据包的seq 
int curAck;             //当前等待确认的ack 
int totalSeq;           //收到的包的总数 
int totalPacket;        //需要发送的包总数
int latestACK;          //当前收到最新的ACK
int repeatACKCount;     //收到重复的ACK数
 
//************************************ 
// Method:    getCurTime 
// FullName:  getCurTime 
// Access:    public  
// Returns:   void 
// Qualifier: 获取当前系统时间，结果存入ptime中 
// Parameter: char * ptime 
//************************************ 
void getCurTime(char *ptime){ 
    char buffer[128]; 
    memset(buffer,0,sizeof(buffer)); 
    time_t c_time; 
    struct tm *p; 
    time(&c_time); 
    p = localtime(&c_time); 
    sprintf_s(buffer,"%d/%d/%d %d:%d:%d", 
    p->tm_year + 1900, 
    p->tm_mon, 
    p->tm_mday, 
    p->tm_hour, 
    p->tm_min, 
    p->tm_sec); 
    strcpy_s(ptime,sizeof(buffer),buffer); 
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
   }
   
   else{ //ack超过了最大值，回到了curAck的左边 
      for(int i = curAck; i < SEQ_SIZE; ++i){
         ack[i] = TRUE; 
      } 
      for(int i = 0; i<= index;++i){ 
         ack[i] = TRUE; 
      } 
      curAck = index + 1; 
   }

   //快重传
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


 
//主函数 
int main(int argc, char* argv[]) 
{ 
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
   return -1; 
   } 
   if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2)  
   { 
      printf("Could not find a usable version of Winsock.dll\n"); 
      WSACleanup(); 
   }else{ 
      printf("The Winsock 2.2 dll was found okay\n"); 
   } 

   SOCKET sockServer = socket(AF_INET, SOCK_DGRAM,IPPROTO_UDP); 
    
   SOCKADDR_IN addrServer;  //服务器地址 
   //addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP); 
   addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//两者均可 
   addrServer.sin_family = AF_INET; 
   addrServer.sin_port = htons(SERVER_PORT); 
   
   if(bind(sockServer,(SOCKADDR*)&addrServer, sizeof(SOCKADDR))){ 
      err = GetLastError(); 
      printf("Could not bind the port %d for socket.Error code is %d\n",SERVER_PORT,err); 
      WSACleanup(); 
      return -1; 
   } 
 
   SOCKADDR_IN addrClient;  //客户端地址 
   int length = sizeof(SOCKADDR); 
   char buffer[BUFFER_LENGTH]; //数据发送接收缓冲区 
   ZeroMemory(buffer,sizeof(buffer)); 

   //将测试数据读入内存 
   ifstream icin; 
   icin.open(FILE_PATH); 
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

   //服务端主要业务
   while(true){ 

      //非阻塞接收，若没有收到数据，返回值为-1
      int iMode = 1; //1：非阻塞，0：阻塞 
      ioctlsocket(sockServer,FIONBIO, (u_long FAR*) &iMode);//非阻塞设置
      cout << "waiting for client" << endl;

      recvSize = recvfrom(sockServer,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrClient),&length); 
      if(recvSize < 0){ 
         Sleep(200); 
         continue; 
      } 
      printf("recv from client: %s\n",buffer); 

      //用户输入-time
      if(strcmp(buffer,"-time") == 0){ 
         getCurTime(buffer); 
      }
      
      //用户输入-quit
      else if(strcmp(buffer,"-quit") == 0){ 
         strcpy_s(buffer,strlen("Good bye!") + 1,"Good bye!"); 
      }
      
      //用户输入-testGBN-Single
      else if(strcmp(buffer,"-testGBN-Single") == 0){ 

         //进入gbn测试阶段 
         //首先server（server处于0状态）向client发送205状态码（server进入1状态） 
         //server 等待client回复200状态码，如果收到（server进入2状态），则开始传输文件，否则延时等待直至超时 
         ZeroMemory(buffer,sizeof(buffer)); 
         int recvSize; 
         int waitCount = 0;
         int repeatTime = 0;
         //设置套接字为非阻塞模式
         int iMode = 1; //1：非阻塞，0：阻塞 
         ioctlsocket(sockServer,FIONBIO, (u_long FAR*) &iMode);//非阻塞设置 
         printf("Begain to test GBN protocol,please don't abort the process\n");

         //加入了一个握手阶段 
         //首先服务器向客户端发送一个205大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据 
         //客户端收到205之后回复一个200大小的状态码，表示客户端准备好了，可以接收数据了 
         //服务器收到200状态码之后，就开始使用GBN发送数据了 
         printf("Shake hands stage\n"); 
         int stage = 0; 
         bool runFlag = true; 
         while(runFlag){ 
            switch(stage){ 

               case 0://发送205阶段
                  buffer[0] = 205; 
                  sendto(sockServer, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
                  Sleep(100); 
                  stage = 1; 
                  break; 

               case 1://等待接收200阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始 
                  recvSize = recvfrom(sockServer,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrClient),&length); 
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

               case 2://传递包数阶段
                  while(true){
                     buffer[0] = 254;
                     buffer[1] = totalPacket;
                     sendto(sockServer, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
                     recvSize = recvfrom(sockServer,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrClient),&length);  
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

               case 3://数据传输阶段 
                  if(seqIsAvailable() && totalSeq < totalPacket){ 
                     //发送给客户端的序列号从1开始
                     buffer[0] = curSeq + 1; 
                     ack[curSeq] = FALSE;
                     memcpy(&buffer[1], data + 1024 * totalSeq, 1024); 
                     printf("send a packet with a seq of %d\n",curSeq); 
                     sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
                     ++curSeq; 
                     curSeq %= SEQ_SIZE; 
                     ++totalSeq; 
                     Sleep(500); 
                  } 

                  //等待Ack，若没有收到，则返回值为-1，计数器+1 
                  recvSize = recvfrom(sockServer,buffer,BUFFER_LENGTH,0,((SOCKADDR*)&addrClient),&length); 
                  if(recvSize < 0){ 
                     waitCount++; 
                     //20次等待ack则超时重传 
                     if (waitCount > 20){ 
                        printf("Timer out error.\n");
                        repeatTime++;

                        //未收到最后的确认，重发3次
                        if(totalSeq == totalPacket && repeatTime >= endure){
                           printf("Transmission complete !!!\n");
                           memset(&addrClient, 0, sizeof(addrClient));
                           runFlag = false;
                           break;
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
                           memset(&addrClient, 0, sizeof(addrClient));
                           runFlag = false;
                           break;
                     }

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
      
      //用户输入-testGBN-Both
      else if(strcmp(buffer,"-testGBN-Both") == 0){

         //设置套接字为非阻塞模式
         int iMode = 0; //1：非阻塞，0：阻塞 
         ioctlsocket(sockServer,FIONBIO, (u_long FAR*) &iMode);//非阻塞设置 

         //进入客户端上传文件阶段 
         ZeroMemory(buffer,sizeof(buffer)); 
         int recvSize; 
         int waitCount = 0; 
         printf("Wait for the client to upload the file\n");

         int stage = 0;
         BOOL b; 
         unsigned char u_code;       //状态码 
         unsigned short seq;         //包的序列号 
         unsigned short recvSeq;     //接收窗口大小为1，已确认的序列号 
         unsigned short waitSeq;     //等待的序列号
         int totalPacketNum;         //待接收包的总数
         int receivedPacketNum = 0;  //总共收到的包数
         ofstream file(savePath);
         int len = sizeof(SOCKADDR);
         bool runFlag = true;

         while (runFlag){ 
            //等待server回复设置UDP为阻塞模式 
            recvfrom(sockServer,buffer,BUFFER_LENGTH,0,(SOCKADDR*)&addrClient, &len);
            switch(stage){ 

               case 0://等待握手阶段 
                  u_code = (unsigned char)buffer[0]; 
                  if ((unsigned char)buffer[0] == 205) { 
                        printf("Ready for file transmission\n"); 
                        buffer[0] = 200; 
                        buffer[1] = '\0'; 
                        sendto(sockServer, buffer, 2, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
                        stage = 1; 
                  } 
                  break;

               case 1://接收包数阶段 
                  totalPacketNum = buffer[1]; 
                  printf("Total Packet Num is %d\n", totalPacketNum);
                  buffer[0] = 255; 
                  buffer[1] = '\0';
                  sendto(sockServer, buffer, 2, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
                  stage = 2; 
                  recvSeq = 0; 
                  waitSeq = 1;
                  Sleep(500);
                  break;

               case 2:
                   seq = (unsigned char)buffer[0];
                     //屏蔽状态码为254的消息
                     if(seq == 254) continue;

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

                     sendto(sockServer, buffer, 2, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
                     printf("send a ack of %d\n",(unsigned char)buffer[0]);

                     //当客户端收到与预期包数相同的包后即可停止，可不管ACK是否成功被接收
                     if(receivedPacketNum == totalPacketNum){    
                           printf("Receive complete!!!\n");
                           memset(&addrClient, 0, sizeof(addrClient));
                           file.close();
                           runFlag = false;
                     }  
                     break;    
            }
         }
      }
      sendto(sockServer, buffer, strlen(buffer)+1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)); 
      Sleep(500); 
   }



   //关闭套接字，卸载库 
   closesocket(sockServer); 
   WSACleanup(); 
   return 0; 
}