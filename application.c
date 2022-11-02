//
// Created by ricardo on 21-10-2022.
//
#include "data-link.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>


#define TRANSMITTER 0
#define RECEIVER 1

#define DATA 1
#define START 2
#define END 3
#define FILE_LENGTH 0
#define FILE_NAME 1

#define PACKED_SIZE (BUF_SIZE-9)

char* fileName;
size_t fileSize;
size_t sequenceCounter = 0;
size_t byteCounter = 0;

int min(int n1, int n2) {

    if(n1<n2) {
        return n1;
    } else if(n2>n1) {
        return n2;
    } else {
        return 0;
    }
}

int llopen(const char* portName,int mode,char* name,size_t size){
    int fd = openPort(portName);
    if(fd >= 0) {
        if (mode == RECEIVER ) {
            int result1 = awaitConnection(fd);

            struct array buf = Read(fd);
            if(buf.content[0] != START)return -1;
            size_t mysize;
            int i = 1;
            while(i < buf.size) {
                if(buf.content[i] == FILE_LENGTH){
                     i++;
                     mysize = buf.content[i];
                     i++;
                    memcpy(&fileSize,&buf.content[i],mysize);
                    printf("%luFILE_LENGTH:%lu\n",mysize,fileSize);
                    i += mysize;
                }else if (buf.content[i] == FILE_NAME){
                    i++;
                    mysize = buf.content[i];
                    i++;
                    fileName = malloc(mysize);
                    memcpy(fileName,&buf.content[i],mysize);
                    printf("FILE_NAME:%s\n",fileName);
                    i +=mysize;
                }else{
                    break;
                }
            }
            if (result1 == 0)return fd;
            else return -1;
        } else {
            int result1 = connect(fd);
            int name_len = min(strlen(name)+1,BUF_SIZE - 10 - sizeof(size_t));
            unsigned char* buf = malloc((5+name_len) * sizeof(unsigned char) + sizeof(size_t));
            buf[0] = START;
            buf[1] = FILE_LENGTH;
            buf[2] = sizeof(size_t);
            //number of chars it takes to store a size_t (with round up)

            memcpy(&buf[3],&size, sizeof(size_t));
            buf[3+ sizeof(size_t)] = FILE_NAME;
            buf[4 + sizeof(size_t)] = name_len;
            memcpy(&buf[5 + sizeof(size_t)],name,name_len);
            buf[4 + sizeof(size_t)+name_len] =  '\0';
            int result = Write(fd,(struct array){buf,(5+name_len)* sizeof(unsigned char) + sizeof(size_t)});

            if (result1 == 0 && result == 0)return fd;
            else return -1;
        }
    }
    return fd;
}

int llwrite(int fd,unsigned char * buffer, int length){
    unsigned char* frame = malloc(length+4);

    frame[0] = DATA;

    frame[1] = sequenceCounter%255;
    sequenceCounter++;

    byteCounter += length;
    frame[2] =  length / (BUF_SIZE-4);
    frame[3] = length % (BUF_SIZE-4);

    memcpy(&frame[4],buffer,length);

    if(Write(fd,(struct array){frame,length+4}) == 0)return 1;
    else return 0;
}

size_t llread(int fd,char** buffer) {
    struct array a = Read(fd);
    if(a.content[0] == DATA){
        *buffer = malloc(a.size-4);
        memcpy(*buffer,&a.content[4],a.size-4);
        return a.content[3];
    }else if(a.content[0] == END){
        free(a.content);
        return 0;
    }else{
        return -1;
    }
}

int llclose(int fd,int mode,char* name,size_t size) {

    if(mode == TRANSMITTER) {
        int name_len = min(strlen(name)+1,BUF_SIZE - 10 - sizeof(size_t));
        unsigned char* buf = malloc((5+name_len) * sizeof(unsigned char) + sizeof(size_t));
        buf[0] = END;
        buf[1] = FILE_LENGTH;
        buf[2] = sizeof(size_t);
        //number of chars it takes to store a size_t (with round up)

        memcpy(&buf[3],&size, sizeof(size_t));
        buf[3+ sizeof(size_t)] = FILE_NAME;
        buf[4 + sizeof(size_t)] = name_len;
        memcpy(&buf[5 + sizeof(size_t)],name,name_len);
        buf[4 + sizeof(size_t)+name_len] =  '\0';
        int result = Write(fd,(struct array){buf,(5+name_len)* sizeof(unsigned char) + sizeof(size_t)});

        if(result == 0)return fd;
        else return -1;
    }
    closePort(fd);
}

int main(int argc,char* argv[]){
    const char *serialPortName = argv[1];
    int fd;
    FILE* myFile;
    if(argc == 2){
        fd = llopen(serialPortName, RECEIVER,"",0);

        myFile = fopen("b.txt","w");
        if(fd < 0) exit(-1);
        char* buffer;
        int length = llread(fd,  &buffer);
        while (length > 0){
            for (int i = 0; i < length; ++i) {
                fputc(buffer[i],myFile);
            }
            char* temp = buffer;
            free(temp);
            length = llread(fd,&buffer);
        }
    }else if(argc == 3){
        myFile = fopen(argv[2],"r");
        if(myFile == NULL){
            printf("Wasn't able to open file");
            exit(1);
        }

        fileName = "b.txt";
        fseek(myFile,0,SEEK_END);
        fileSize = ftell(myFile);
        fseek(myFile,0,SEEK_SET);

        fd = llopen(serialPortName, TRANSMITTER,argv[2],fileSize);
        if(fd < 0) exit(-1);
        char* payload = malloc(PACKED_SIZE);
        while(fgets(payload,PACKED_SIZE,myFile) != NULL){
            if(llwrite(fd,(unsigned char*)payload, strlen(payload)) < 0)exit(1);
        }
        if(llclose(fd,TRANSMITTER,(char*)fileName,fileSize) < 0)exit(1);
        free(payload);
    }else{
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }
    fclose(myFile);





}
