#include <stdio.h>
#include <fstream>

using namespace std;

int main(int argc, char* argv[]){
    fstream file;
    file.open("E:\\Web\\experiment3\\test.txt", ios::out);

    for(int k = 0; k < 10; k++){
        for (int i = 1; i <= 9; i++)
        {  
        string content = "";
        for(int j = 0; j < 1023; j++){
                content += to_string(i);
        }
        content += "\n";
        file.write(content.c_str(), content.length());
        }
    }
    
    return 0;
}