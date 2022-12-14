#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include "data-link.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source





#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define SET 0x03
#define UA 0x07
#define ESC 0X7d
#define XOR_ESC 0X20
#define DISC 0X0B
#define RR0 0x05
#define RR1 0x85
#define REJ0 0x01
#define REJ1 0x81
#define I0 0x00
#define I1 0x40

unsigned char C = I0;

unsigned char control = 0x00;

int connected = 0;

volatile int STOP = FALSE;
struct termios oldtio;

enum states{
    START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,END
};
struct frame{
    int sending;
    size_t size;
    unsigned char* payload;
    unsigned char control;
};

struct frame myFrame= {0,0,0,0};
int state = START;

int connectionStatus(){
    return connected;
}

void frameAtts(struct frame f) {

    if(f.sending == 1) {
        printf("\t|Emissor");
        for (int i = 7; i < f.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    } else {
        printf("\t|Receptor");
        for (int i = 8; i < f.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    }
    printf("\t|New Frame");
    for (int i = 9; i < f.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

    printf("\t|Size: %zu",f.size);
    for (int i = 9; i < f.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

    printf("|\n");
    printf("|");
    for (int i = 0; i < f.size; ++i) {
        printf("%02X",f.payload[i]);
    }
    printf("|\n");
    printf("|\n");

    printf("|Control: %u",f.control);
    for (int i = 9; i < f.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

}

void printFrame(struct array payload, int sending){
    if(sending == 1){

        printf("\t|Emissor");
        for (int i = 7; i < payload.size*2; ++i) {
            printf("-");
        }
        printf("|\n");

    }else{
        printf("\t|Receptor");
        for (int i = 8; i < payload.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    }
    printf("\t|New Frame");
    for (int i = 9; i < payload.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

    printf("\t|Size:%lu",payload.size);
    size_t n = payload.size;
    int count = 0;
    do{
        n /= 10;
        count++;
    } while (n != 0);

    for (int i = 5+count; i < payload.size*2; ++i) {
        printf("-");
    }
    printf("|\n");


    printf("\t|Control:%02X",payload.content[2]);
    for (int i = 10; i < payload.size*2; ++i) {
        printf("-");
    }
    printf("|\n");


    printf("\t|");
    for (int i = 0; i < payload.size; ++i) {
        printf("%02X",payload.content[i]);
    }
    printf("|\n");
    if(sending == 1)printf("\n");

}

struct array stuff(struct array buf){
    unsigned char *stuffed = malloc(2*(buf.size));

    int stuffedSize = 0;
    for (int i = 0; i < buf.size; ++i) {
        if(buf.content[i] == FLAG){
            stuffed[stuffedSize] = ESC;
            stuffed[++stuffedSize] = FLAG^XOR_ESC;
        }else if(buf.content[i] == ESC){
            stuffed[stuffedSize] = ESC;
            stuffed[++stuffedSize] = ESC^XOR_ESC;
        }else{
            stuffed[stuffedSize]=buf.content[i];
        }
        stuffedSize++;
    }

    stuffed = realloc(stuffed,stuffedSize);

    struct array a = {stuffed,stuffedSize};

    return a;
}
//So funciona com tramas de supervi????o e n??o numeradas
void updateState(unsigned char byte,int* myState){
    switch (*myState){
        case START:
            if (byte == FLAG)
            {
                *myState = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if (byte == A) {
                *myState = A_RCV;
            }else{
                *myState = START;
            }
            break;
        case A_RCV:
            if (byte == FLAG)
            {
                *myState = FLAG_RCV;
            }else{
                control = byte;
                *myState = C_RCV;
            }
            break;
        case C_RCV:
            if(byte == FLAG)
            {
                *myState = FLAG_RCV;
            }else if(byte == (control ^ A)){
                *myState = BCC_OK;
            }else{
                *myState = START;
            }
            break;
        case BCC_OK:
            if (byte == FLAG)
            {
                *myState = END;
            }else{
                *myState = START;
            }
        case END:
            STOP = TRUE;
            break;
        default:
            *myState = START;
        }
    printf("State:%d\n",*myState);
}

struct array getResponse(unsigned char c,int correct) {

    unsigned char* a = malloc(5);
    a[0] = FLAG;
    a[1] = A;
    a[4] = FLAG;
    if(correct == 1) {
        switch (c) {
            case DISC:
                a[2] = DISC;
                connected = 2;
                break;
            case SET:
                a[2] = UA;
                break;
            case I0:
                a[2] = RR1;
                C = I1;
                break;
            case I1:
                a[2] = RR0;
                C = I0;
                break;
            default:
                free(a);
                return (struct array) {NULL, 0};
        }
    }
    else{
        if (c != C){
            a[2] = c == I1? RR0:RR1;
            a[3] = c == I1? RR0:RR1;
        }else if(c == I0){
            a[2] = REJ1;
        }else if(c == I1){
            a[2] = REJ0;
        }else{
            free(a);
            return (struct array) {NULL, 0};
        }
    }
    a[3] = a[2]^A;
    return (struct array) {a, 5};
}

struct array receptor(int fd){

    unsigned char* buf = (unsigned char*) malloc((BUF_SIZE + 1) * sizeof(unsigned char )); // +1: Save space for the final '\0' char
    // Returns after 5 chars have been input
    long bytes = read(fd, buf, 1);
    buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
    struct array a = {buf, bytes};
    return a;
}

void emissor(int fd, struct array payload){
    printFrame(payload, 1);
    write(fd, payload.content, payload.size);
}

int openPort(const char *serialPortName){
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-SET.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        return -1;
    }


    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 3; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    return fd;
}

void closePort(int fd){
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}

int message(int fd,struct array payload, unsigned char c ){
    emissor(fd, payload);
    int alarmCount = 0;


    long int start = time(NULL);
    int myState = START;

    //repeats the loop until it has received the frame or exceeded the number of retransmissions
    while (alarmCount < numTransmissions && (myState != END || control != c)){
        //receiving and processing the data
        int i = 0;
        unsigned char* response = malloc(BUF_SIZE);

        //reading the port and storing it
        while(myState != END && ((double)(time(NULL)-start) < TIMEOUT)) {
            int bytes = read(fd,&response[i],1);
            if(bytes <= 0){
                i = 0;
                myState = START;
                continue;
            }
            printf("\t%02X",response[i]);
            updateState(response[i], &myState);
            if(myState == START){
                i = 0;
            }
            i = (i+1)%BUF_SIZE;
        }
        printFrame((struct array){response,i},0);
        free(response);

        if(myState != END){
            printf("\tNo response sending frame again\n");
            alarmCount++;
            emissor(fd, payload);
            start = time(NULL);
        }else if(control != c){
            state = START;
            printf("\tUnexpected response sending frame again\n");
            alarmCount++;
            emissor(fd, payload);
            start = time(NULL);
        }
    }

    if(myState != END)return 1;
    else return 0;
}

int connect(int fd){
    //build the set payload
    C = 0x00;
    unsigned char a[] = {FLAG, A, SET, A ^ SET, FLAG};
    struct array payload = {a,5};
    if(message(fd,payload,UA) == 0){
        printf("\tConnection established\n");
        return 0;
    }
    else{
        printf("\tConnection failed\n");
        return 1;
    }
}

int disconnect(int fd){
    unsigned char a[] = {FLAG,A,DISC,A^DISC,FLAG};
    struct array payload = {a,5};

    unsigned char b[] = {FLAG,A,UA,A^UA,FLAG};
    struct array ua = {b,5};

    if(message(fd,payload,DISC) == 0){
        emissor(fd,ua);
        printf("\tConnection terminated\n");
        return 0;
    }else {
        printf("\tunable to terminate connection\n");
        return 1;
    }
}

int awaitConnection(int fd){
    unsigned char* buf = malloc(BUF_SIZE* sizeof(unsigned char));
    int i = 0;//garbage value
    int myState = START;
    while (connected == 0){
        if(myState == START)i = 0;
        long bytes = read(fd,(void*)&buf[i],1);
        if (bytes > 0) {
            printf("\t%02X",buf[i]);
            updateState(buf[i], &myState);
        }
        if(myState == END){
            printFrame((struct array){&buf[i-4],5},0);
            connected = 1;
            break;
        }
        i = (i+1)%BUF_SIZE;
    }
    free(buf);
    struct array response = getResponse(SET,1);
    emissor(fd,response);
    free(response.content);

    return 0;
}

struct array processData(int fd, unsigned char *buf, int* i) {
    unsigned char* data = malloc((BUF_SIZE-5));
    int dataSize = 0;
    unsigned char BCC2 = 0; //zero is the neutral element of XOR
    while(state != END && (dataSize < BUF_SIZE-5)){

        (*i)++;
        if(read(fd, (void*)&buf[*i], 1) < 1){
            (*i)--;
            continue;
        }


        if(buf[*i] == FLAG){
            if(BCC2 == 0){
                state = END;
                printf("\n\tBCC2:%02X\n",BCC2);
                return  (struct array){data,dataSize};
            }else{
                state = START;
                printf("\n\tBCC2:%02X\n",BCC2);
                free(data);
                return (struct array){NULL,0};
            }
        }
        if(buf[*i] == ESC){
            (*i)++;
            read(fd, (void*)&buf[*i], 1);
            data[dataSize] = XOR_ESC ^ buf[*i];
        }else{
            data[dataSize] = buf[*i];
        }
        printf("%02X",data[dataSize]);
        BCC2 ^= data[dataSize];
        dataSize++;
    }
    if(dataSize > 0){
        data = realloc(data,dataSize);
        return (struct array){data,dataSize};
    }else{
        free(data);
        return (struct array){NULL,0};
    }
}

struct array Read(int fd){
    unsigned char c = 0xFF; //garbage value
    unsigned char* buf = malloc(BUF_SIZE * sizeof(unsigned char));
    int i = 0;
    struct array data = {NULL,0};
    struct array response = {NULL,0};

    while(connected == 1 || connected == 2) {
        state = START;
        while (state != END && i <= BUF_SIZE) {
            if(state == START)i = 0;
            if(read(fd,&buf[i],1) < 1)continue;

            switch (state) {
                case START:
                    state = buf[i] == FLAG ? FLAG_RCV : START;
                    break;
                case FLAG_RCV:
                    if(buf[i] == A)state = A_RCV;
                    else if(buf[i] != FLAG) state = START;
                    break;
                case A_RCV:
                    state = buf[i] == FLAG ? FLAG_RCV : C_RCV;
                    c = buf[i];
                    break;
                case C_RCV:
                    if (buf[i] == (c ^ A)) {
                        state = BCC_OK;
                        if(c == I1 || c == I0) {
                            printf("\t");
                            data = processData(fd, buf, &i);
                            response = data.size > 0? getResponse(c,1):getResponse(c,0);
                        }
                    } else if (buf[i] == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        response = getResponse(c,0);
                        state = START;
                    }
                    break;
                case BCC_OK:
                    if (buf[i] == FLAG) {
                        state = END;
                        response = getResponse(c,1);// changes the global value of C if necessary
                    }else {
                        state = START;
                    }
                case END:
                    STOP = TRUE;
                    break;
                default:
                    state = START;
            }
            printf("\t%02Xstate:%d\n",buf[i],state);
            i++;
        }
        if(i > 0)printFrame((struct array){buf,i},0);

        //if the frame is bigger than the buf size discard the frame
        /*
        if(i > BUF_SIZE){
            printf("frame too big discarding frame\n");
            if(response.size > 0){
                response.size = 0;
                unsigned char* temp = response.content;
                free(temp);
            }
            if(data.size > 0){
                data.size = 0;
                unsigned char* temp = data.content;
                free(temp);
            }
            state = START;
            continue;
        }
         */

        if(response.size > 0 && state == END){
            emissor(fd,response);
            response.size = 0;
            free(response.content);
        }
        if((c == I1 || c == I0) && state == END)break;
        if(connected == 2)connected = c == UA?-1:1;
    }

    //need review
    if(connected == 1 && data.size > 0){
        free(buf);
        return data;
    }
    if(connected == -1){
        free(buf);
        if(data.size > 0)free(data.content);
    }
    return (struct array){NULL,0};
}

int Write(int fd, struct array data){
    const unsigned char header[] = {FLAG, A, C  , A ^ C};
    const unsigned char footer[] = { FLAG};
    if(data.size > BUF_SIZE-5)return -2;
    unsigned char BCC2 = 0;
    for (int i = 0; i < data.size; ++i) {
        BCC2 = BCC2 ^ data.content[i];
    }

    //add BCC2 to the data and stuffing it
    data.content = realloc(data.content,data.size+1);
    data.size++;
    data.content[data.size-1] = BCC2;

    struct array temp = data;
    data = stuff(data);


    unsigned char* payload = malloc((4+data.size+1)*sizeof(unsigned char));
    struct array a = {payload,(4+data.size+1)};

    memcpy(payload,   header, (4* sizeof(unsigned char)));
    memcpy(payload + 4, data.content,   data.size * sizeof(unsigned char));
    memcpy(payload + 4 + data.size ,footer,sizeof(unsigned char));

    int r = message(fd,a, C == I0?RR1:RR0);

    free(temp.content);
    free(payload);
    free(data.content);

    if(C == I0)C = I1;
    else C = I0;

    return r;
}