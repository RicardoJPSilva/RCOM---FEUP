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

#define PACKED_SIZE (BUF_SIZE-12)

char* fileName;
size_t fileSize;
size_t sequenceCounter = 0;
size_t byteCounter = 0;
int isConnected;

size_t min(size_t n1, size_t n2) {
    if(n1<n2) {
        return n1;
    } else if(n2>n1) {
        return n2;
    } else {
        return 0;
    }
}

int llopen(const char* portName,int mode,const char* name,size_t size){
    int fd = openPort(portName);
    if(fd >= 0) {
        if (mode == RECEIVER ) {
            printf("awaiting data-link Connection\n");
            int result1 = awaitConnection(fd);
            printf("data-link connection established\n");
            printf("---------------------------------\n");
            printf("Awaiting Application connection\n");

            struct array buf = Read(fd);
            if(buf.content[0] != START)return -1;
            size_t mySize;
            size_t i = 1;
            while(i < buf.size) {
                if(buf.content[i] == FILE_LENGTH){
                     i++;
                    mySize = buf.content[i];
                     i++;
                    memcpy(&fileSize, &buf.content[i], mySize);
                    printf("FILE_LENGTH:%lu\n",fileSize);
                    i += mySize;
                }else if (buf.content[i] == FILE_NAME){
                    i++;
                    mySize = buf.content[i];
                    i++;
                    fileName = malloc(mySize);
                    memcpy(fileName, &buf.content[i], mySize);
                    printf("FILE_NAME:%s\n",fileName);
                    i +=mySize;
                }else{
                    break;
                }
            }
            if (result1 == 0){
                printf("Aplication connection set up\n");
                printf("-----------------------------\nn");
                isConnected = 1;
                return fd;
            }else {
                printf("Connection failed\n");
                printf("-----------------\n");
                return -1;
            }
        } else {
            printf("Setting up data-link connection\n");
            int result1 = connect(fd);

            if(result1 == 0 ) {
                printf("data link connection set up\n");
                printf("---------------------------\n");
                printf("Setting up application connection\n");
                int name_len = min(strlen(name) + 1, BUF_SIZE - 10 - sizeof(size_t));
                unsigned char *buf = malloc((5 + name_len) * sizeof(unsigned char) + sizeof(size_t));
                buf[0] = START;
                buf[1] = FILE_LENGTH;
                buf[2] = sizeof(size_t);
                //number of chars it takes to store a size_t (with round up)

                memcpy(&buf[3], &size, sizeof(size_t));
                buf[3 + sizeof(size_t)] = FILE_NAME;
                buf[4 + sizeof(size_t)] = name_len;
                memcpy(&buf[5 + sizeof(size_t)], name, name_len);
                buf[4 + sizeof(size_t) + name_len] = '\0';
                printf("Setting up aplication link\n");
                int result = Write(fd, (struct array) {buf, (5 + name_len) * sizeof(unsigned char) + sizeof(size_t)});
                if(result == 0){
                    printf("Application link set up\n");
                    printf("-----------------------\n");
                    isConnected = 1;
                    return fd;
                }
            }
            printf("Connection failed\n");
            printf("-----------------\n");
            return -1;
        }
    }
    return fd;
}

int llwrite(int fd,const unsigned char * buffer, int length){
    unsigned char* frame = malloc(length+4);

    frame[0] = DATA;

    frame[1] = sequenceCounter%255;
    sequenceCounter++;

    byteCounter += length;
    frame[2] =  length / (BUF_SIZE-4);
    frame[3] = length % (BUF_SIZE-4);

    memcpy(&frame[4],buffer,length);
    printf("Writing info into:\n");
    if(Write(fd,(struct array){frame,length+4}) == 0){
        printf("info Writen successfully\n");
        printf("-------------------------\n");
        return 1;
    }
    printf("Error writing data\n");
    printf("------------------\n");
    return 0;
}

size_t llread(int fd,char** buffer) {
    printf("reading data:\n");
    struct array a = Read(fd);
    if(a.content[0] == DATA){
        *buffer = malloc(a.size-4);
        memcpy(*buffer,&a.content[4],a.size-4);
        int length = a.content[3];
        free(a.content);
        printf("received data\n");
        printf("---------------------------------------\n");
        return length;
    }else if(a.content[0] == END){
        isConnected = 0;
        printf("Aplication link closed\n");
        printf("---------------------------------------\n");
    }
    free(a.content);
    return -1;

}

int llclose(int fd,int mode,const char* name,size_t size) {

    if(mode == RECEIVER){
        while (connectionStatus() != -1) {
            struct array a = Read(fd);
            if(size>0)free(a.content);
        }
    }else{

        size_t name_len = min(strlen(name)+1,BUF_SIZE - 10 - sizeof(size_t));
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
        printf("Closing Connection\n");
        int result = Write(fd,(struct array){buf,(5+name_len)* sizeof(unsigned char) + sizeof(size_t)});

        disconnect(fd);
        closePort(fd);
        if(result == 0){
            printf("application connection closed successfully\n");
            return fd;
        }
        printf("Failed to terminate connection");
        return -1;
    }
    closePort(fd);
    return fd;
}

int main(int argc,char* argv[]){
    const char *serialPortName = argv[1];
    int fd;
    FILE* myFile;
    if(argc == 2){
        fd = llopen(serialPortName, RECEIVER,"",0);
        if(fd < 0) exit(-1);

        myFile = fopen("b.txt","w");
        if(myFile == NULL){
            printf("Wasn't able to open file");
            exit(1);
        }

        char* buffer;
        size_t length = llread(fd,  &buffer);
        while (isConnected == 1){
            for (int i = 0; i < length; ++i) {
                fputc(buffer[i],myFile);
            }
            char* temp = buffer;
            free(temp);
            length = llread(fd,&buffer);
        }
        fclose(myFile);
        llclose(fd,RECEIVER,fileName,fileSize);
    }else if(argc == 3){
        myFile = fopen(argv[2],"r");
        if(myFile == NULL){
            printf("Wasn't able to open file");
            exit(1);
        }

        FILE* test = fopen("test.txt","w");
        if(test == NULL){
            printf("Wasn't able to open file");
            exit(1);
        }

        //get the size of the file
        fseek(myFile,0,SEEK_END);
        fileSize = ftell(myFile);
        fseek(myFile,0,SEEK_SET);

        //opening the connection
        fd = llopen(serialPortName, TRANSMITTER,argv[2],fileSize);
        if(fd < 0) exit(-1);

        unsigned char* payload = malloc(PACKED_SIZE);
        int i;

        while(!feof(myFile)){
            for (i = 0; i < PACKED_SIZE && !feof(myFile); ++i) {
                payload[i] = fgetc(myFile);
                putc(payload[i],test);
            }
            if(feof(myFile))i--;
            printf("\n");
            if(llwrite(fd,payload, i) < 0)exit(1);
        }

        if(llclose(fd,TRANSMITTER,argv[2],fileSize) < 0)exit(1);

        free(payload);
        fclose(myFile);
        fclose(test);

    }else{
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }
}
