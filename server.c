/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: chenda37
 *
 * Created on September 21, 2021, 7:23 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

/*
 * 
 */
bool numberChecker(char *string){
    for(int i = 0; i < strlen(string); i++){
        if(isdigit(string[i]) == 0){
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    srand(time(0));
    int portNum;
    if(argc !=2){
        return 0;
    }else{
        if(numberChecker(argv[1]) == true){
            portNum = atoi(argv[1]);
        }else{
            return 0;
        }
    }
    
    struct sockaddr_in addr;
    int fd;
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        exit(1);
    }
    
    addr.sin_family = AF_INET;    
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(portNum);
    int bd;
    if((bd = bind(fd, (struct sockaddr*)&addr, sizeof(addr))) == -1){
        addr.sin_port = htons(0);                                           //let os decide port num 
        fprintf(stderr, "Connection to port %d failed. Now connected to %d\n", portNum, ntohs(addr.sin_port));
        if((bd = bind(fd, (struct sockaddr*)&addr, sizeof(addr))) == -1){
            exit(1);
        }
    }
    char a[100] = "Server receiving on port ";
    char pmessage[100];
    sprintf(pmessage, "%d\n", ntohs(addr.sin_port));
    fprintf(stderr, strcat(a, pmessage));
    
    char buffer[2000] = "";
    char message[500] = "";
    struct sockaddr_storage addrStorage;
    socklen_t addrStorageSize = sizeof(addrStorage);
    //listen for messages from the client
    //break out of this loop when file transfer is initiated
    while(true){
        if(recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addrStorage, &addrStorageSize) == -1){
            exit(1);
        }else{
            if(strcmp(buffer, "ftp") == 0){
                strcat(message, "yes");             
                if(sendto(fd, message, sizeof(message), 0, (struct sockaddr*)&addrStorage, addrStorageSize) == -1){
                    exit(1);
                }else{
                    strcpy(message, "");
                    strcpy(buffer,"");  
                    break;
                }
            }else{
                strcat(message, "no");
                if(sendto(fd, message, sizeof(message), 0, (struct sockaddr*)&addrStorage, addrStorageSize) == -1){
                    exit(1);
                }
            }  
        }
        strcpy(message, "");
        strcpy(buffer,"");   
    }
    
    unsigned int total_frag, frag_num, size;
    char name[200];
    char filedata[1000];
    FILE *fp = NULL;
    bool firstPacket = true;
    
    //parsing packets 
    //first packet format is different from the rest of the packets
    while(true){
        if(recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addrStorage, &addrStorageSize) == -1){
            exit(1);  
        }else{      
            //parsing of first packet
            if(firstPacket == true){
                int colonCount = 0; 
                int categoryLength = 0;
                int category_got = 0;
                char *category = NULL;            
                for(int i = 0; i < 2000; i++){
                    if(buffer[i] != ':'){
                        if(buffer[i+1] != ':'){
                            categoryLength++;
                        }else{                  //when we hit :
                            categoryLength++;
                            category = malloc(sizeof(char)* (categoryLength + 1));
                            for(int j = 0; j < categoryLength; j++){
                                category[j] = buffer[i-categoryLength + 1 + j];
                            }
                            category[categoryLength]= '\0';
                            if(category_got == 0){
                                total_frag = atoi(category);
                            }else if(category_got == 1){
                                frag_num = atoi(category);
                            }else if(category_got == 2){
                                size = atoi(category);
                            }else if(category_got == 3){
                                strcpy(name, category);
                            }
                            category_got++;
                            categoryLength = 0;
                        }
                    }else{
                        colonCount++;
                    }
                    if(colonCount == 4){
                        memcpy(filedata, &buffer[i+1], size);
                        break;
                    }
                }
                //simulate packet drops
                if(rand()%50 != 1){
                    strcat(message,"Acknowledged");
                    if(sendto(fd, message, sizeof(message), 0, (struct sockaddr*)&addrStorage, addrStorageSize) == -1){
                        exit(1);
                    }
                    strcpy(message, "");

                    fp = fopen(name, "w");            
                    if(fp == NULL){
                        fprintf(stderr, "Error creating file\n");
                        exit(1);
                    }  

                    fwrite(filedata, sizeof(char), size, fp);

                    firstPacket = false;
                }else{
                    fprintf(stderr, "Packet dropped. Num = %d\n", frag_num);
                }
            //parsing of other packets
            }else{
                int colonCount = 0; 
                int categoryLength = 0;
                char *category = NULL;            
                for(int i = 0; i < 2000; i++){
                    if(buffer[i] != ':'){
                        if(buffer[i+1] != ':'){
                            categoryLength++;
                        }else{                  //when we hit :
                            categoryLength++;
                            category = malloc(sizeof(char)* (categoryLength + 1));
                            for(int j = 0; j < categoryLength; j++){
                                category[j] = buffer[i-categoryLength + 1 + j];
                            }
                            category[categoryLength]= '\0';
                           
                            if(colonCount == 1){
                                frag_num = atoi(category);
                            }else if(colonCount == 2){
                                size = atoi(category);
                            }
                            categoryLength = 0;
                        }
                    }else{
                        colonCount++;
                    }
                    if(colonCount == 4){
                        memcpy(filedata, &buffer[i+1], size);
                        break;
                    }
                }
                //simulate packet drops
                if(rand()%50 != 1){
                    strcat(message, "Acknowledged");
                    if(sendto(fd, message, sizeof(message), 0, (struct sockaddr*)&addrStorage, addrStorageSize) == -1){
                        exit(1);
                    }              
                    strcpy(message, "");

                    fwrite(filedata, sizeof(char), size, fp);
                }else{                   
                    fprintf(stderr, "Packet dropped. Num = %d\n", frag_num);  
                }
            }
            if(frag_num == total_frag){
                break;
            }
        }
        
    }
    
    return 0;
}


