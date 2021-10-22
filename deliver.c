/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

/*
 * 
 */

bool numberChecker(char *string){
    for(int i = 0; i < strlen(string); i++){
        if(isdigit(string[i]) == 0 && string[i] != '.'){
            return false;
        }
    }
    return true;
}

void reSendFile(int packet_frag, FILE *fp, int fd, int total_frag, struct sockaddr_in toAddr, char *fileName, int fileLengthRem, int RTT){
    int dataAddr;
    int sent;
    char message[2000];
    char filedata[2000];
    char buffer[2000];
    struct sockaddr_storage addrStorage;
    socklen_t addrStorageSize = sizeof(addrStorage);
    int count = 0;
    
    while(1){
        count++;
        fseek(fp, 0 , SEEK_SET);                                            //move the fp pointer to front of file
        fseek(fp, 1000*(packet_frag-1), SEEK_CUR);                              //move it back to original spot to resend the packet               
        fprintf(stderr, "Timed out! Timeout = %d us, frag no = %d\n", RTT*3, packet_frag); 
        fread(filedata, sizeof(char), 1000, fp);  
        if(packet_frag == total_frag){
            dataAddr = sprintf(message, "%d:%d:%d:%s:", total_frag, packet_frag, fileLengthRem, fileName);
            memcpy(&message[dataAddr], filedata, fileLengthRem);
            sent = sendto(fd, message, dataAddr + fileLengthRem, 0, (struct sockaddr*)&toAddr, sizeof(toAddr));
        }else{
            dataAddr = sprintf(message, "%d:%d:%d:%s:", total_frag, packet_frag, 1000, fileName);
            memcpy(&message[dataAddr], filedata, 1000);
            sent = sendto(fd, message, dataAddr + 1000, 0, (struct sockaddr*)&toAddr, sizeof(toAddr));
        }
        
        if(recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addrStorage, &addrStorageSize) == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                if(count>5){
                    exit(0);
                }
                strcpy(message, "");
                strcpy(buffer, "");
                strcpy(filedata, "");
                continue;
            }else {
                exit(0);
            }
        }
        strcpy(message, "");
        strcpy(buffer, "");
        strcpy(filedata, "");
        break;
    }
}

void sendFile(int fd, char* fileName, struct sockaddr_in toAddr, int RTT){
    char filedata[1000];
    unsigned int total_frag;
    struct sockaddr_storage addrStorage;
    socklen_t addrStorageSize = sizeof(addrStorage);
    char buffer[500] = "";
    
    FILE* fp = fopen(fileName, "r");
    if(fp == NULL){
        fprintf(stderr, "Open file failed\n");
        exit(1);
    }
    
    //to get the size of the file and the number of fragments
    fseek(fp, 0, SEEK_END);
    long int fileLength = ftell(fp);
    if(fileLength%1000 != 0){
        total_frag = (fileLength/1000) + 1;             // +1 at the end to account for remainders
    }else{
        total_frag = fileLength/1000;
    }
    fseek(fp, 0, SEEK_SET);
    int fileLengthRem = fileLength%1000;
    
    struct timeval timeoutVal;
    timeoutVal.tv_sec = 0;
    timeoutVal.tv_usec = RTT*3;
    
    if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal)) == -1){
        fprintf(stderr, "Failed to set timeout value on socket\n");
        exit(0);
    }
    
    char message[2000];
    int dataAddr;
    for(int packet_frag = 1; packet_frag <= total_frag; packet_frag++){
        int sent;        
        int bytesRead = fread(filedata, sizeof(char), 1000, fp);        
        if(packet_frag == total_frag){
            dataAddr = sprintf(message, "%d:%d:%d:%s:", total_frag, packet_frag, fileLengthRem, fileName);
            memcpy(&message[dataAddr], filedata, fileLengthRem);
            sent = sendto(fd, message, dataAddr + fileLengthRem, 0, (struct sockaddr*)&toAddr, sizeof(toAddr));
        }else{
            dataAddr = sprintf(message, "%d:%d:%d:%s:", total_frag, packet_frag, 1000, fileName);
            memcpy(&message[dataAddr], filedata, 1000);
            sent = sendto(fd, message, dataAddr + 1000, 0, (struct sockaddr*)&toAddr, sizeof(toAddr));
        }
        
        if(sent == -1){
            fprintf(stderr, "Failed to send packet\n");
            exit(1);
        }        
        
        if(recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addrStorage, &addrStorageSize) == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){  
                reSendFile(packet_frag, fp, fd, total_frag, toAddr, fileName, fileLengthRem, RTT);
            }else{
                fprintf(stderr, "Receiving failed\n");
                exit(0);
            }
        }
        
        strcpy(message, "");
        strcpy(filedata, "");
        strcpy(buffer, "");
    } 
    fprintf(stderr, "File transfer successful\n");
} 



int main(int argc, char** argv) {
    char *serverAddress;
    int portNum; 
    struct sockaddr_in toAddr;
    struct timeval start, RTT;
    
    if(argc != 3){
        return 0;
    }else{
        if(numberChecker(argv[1]) == true){
            serverAddress = argv[1];
        }else{     
            return 0;
        }      
        if(numberChecker(argv[2]) == true){
            portNum = atoi(argv[2]);
        }else{
            return 0;
        }
    }
    
    int fd;
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        exit(1);
    }
    //get input from user
    
    fprintf(stderr, "Input a message of the following form\n    ftp <file name>\n");
    char inputMessage[200], fileName[200];
    scanf("%s", inputMessage);
    scanf("%s", fileName);
    
    
    toAddr.sin_family = AF_INET;
    toAddr.sin_port = htons(portNum);
    toAddr.sin_addr.s_addr = inet_addr(serverAddress);
  
    if(access(fileName, F_OK) != -1){                 //if it opens that means the file exist
        gettimeofday(&start, NULL);
        if(sendto(fd, inputMessage, strlen(inputMessage)+1, 0, (struct sockaddr*)&toAddr, sizeof(toAddr)) == -1){
            fprintf(stderr, "Send fail\n");
            exit(1);
        }
    }else{        
        fprintf(stderr, "File doesn't exist\n");
        exit(1);
    }
    
    struct sockaddr_storage addrStorage;
    socklen_t addrStorageSize = sizeof(addrStorage);
    char buffer[500] = "";
    if(recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addrStorage, &addrStorageSize)==-1){
        exit(1);
    }
    gettimeofday(&RTT, NULL);
    int RTTus = (RTT.tv_usec - start.tv_usec);
    fprintf(stderr, "RTT = %d us.\n", RTTus);
    if(strcmp(buffer, "yes") == 0){
        fprintf(stderr, "A file transfer can start.\n");
        sendFile(fd, fileName, toAddr, RTTus);
    }else{
        fprintf(stderr, "Please type ftp before the file name.\n");
    }
    
    return 0;
}
