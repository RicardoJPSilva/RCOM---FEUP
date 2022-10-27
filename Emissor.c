// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include "emissor.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

struct linkLayer{
    char port[20];
    int bandRate;
    unsigned int sequenceNumber;
    unsigned int timeout;
    unsigned int numTransmissions;
};

#define TIMEOUT 3
#define numTransmissions 4
#define BUF_SIZE 256

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

struct array{
    unsigned char *content;
    size_t size;
};

enum states{
    START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,END,BCC2_OK
};
struct frame{
    int sending;
    size_t size;
    unsigned char* payload;
    unsigned char control;
};

struct frame myFrame= {0,0,0,0};
int state = START;

void frameAtts(struct frame f) {

    if(f.sending == 1) {
        printf("|Emissor");
        for (int i = 7; i < f.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    } else {
        printf("|Receptor");
        for (int i = 8; i < f.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    }
    printf("|New Frame");
    for (int i = 9; i < f.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

    printf("|Size: %zu",f.size);
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

        printf("|Emissor");
        for (int i = 7; i < payload.size*2; ++i) {
            printf("-");
        }
        printf("|\n");

    }else{
        printf("|Receptor");
        for (int i = 8; i < payload.size*2; ++i) {
            printf("-");
        }
        printf("|\n");
    }
    printf("|New Frame");
    for (int i = 9; i < payload.size*2; ++i) {
        printf("-");
    }
    printf("|\n");

    printf("|Size:%lu",payload.size);
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
    printf("|");
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
//So funciona com tramas de supervição e não numeradas
int updateState(struct array buf,int* myState,unsigned char c){
    for (int i = 0; i < buf.size; i++){
        switch (*myState){
            case START:
                if (buf.content[i] == FLAG)
                {
                    *myState = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (buf.content[i] == A) {
                    *myState = A_RCV;
                }else{
                    *myState = START;
                }
                break;
            case A_RCV:
                if (buf.content[i] == FLAG)
                {
                    *myState = FLAG_RCV;
                }else if(c == buf.content[i]){
                    *myState = C_RCV;
                }else{
                    *myState = START;
                }
                break;
            case C_RCV:
                if(buf.content[i] == FLAG)
                {
                    *myState = FLAG_RCV;
                }else if(buf.content[i] == (c ^ A)){
                    *myState = BCC_OK;
                }else{
                    *myState = START;
                }
                break;
            case BCC_OK:
                if (buf.content[i] == FLAG)
                {
                    *myState = END;
                }else{
                    *myState = START;
                }
            case END:
                STOP = TRUE;
                break;
            default:
                i--;
                *myState = START;
        }
    }
    return c;
}

struct array getResponse(unsigned char c,int correct) {
    unsigned char a[] = {FLAG,A,0x00,0x00,FLAG};
    if(correct == 1) {
        switch (c) {
            case DISC:
                a[2] = DISC;
                connected == 2;
                break;
            case SET:
                a[2] = UA;
                C == I0;
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
                return (struct array) {NULL, -1};
        }
    }else{
        if (c != C){
            a[2] = c == I1? RR0:RR1;
            a[3] = c == I1? RR0:RR1;
        }else if(c == I0 || c == I1){
            a[2] = REJ1;
        }else if(c == I1){
            a[2] = REJ0;
        }else{
            return (struct array) {NULL, -1};
        }
    }
    a[3] = a[2]^A;
    return (struct array) {a, 5};
}

int awaitConnection(int fd){
    unsigned char* buf = malloc(BUF_SIZE* sizeof(unsigned char));
    int i = 0;
    unsigned char c = 0xFF;//garbage value
    int myState = START;
    //doesn't send UA
    while (connected == 0){
        if(state == START)i = 0;
        int bytes = read(fd,(void*)&buf[i],1);
        if (bytes > 0) {
            printf("%02X\n",buf[i]);
            updateState((struct array) {&buf[i], 1}, &myState,SET);
        }
        if(state == END && c == SET)break;
        i = (i+1)%BUF_SIZE;
    }
    free(buf);
    return 0;
}

struct array receptor(int fd){

    unsigned char* buf = (unsigned char*) malloc((BUF_SIZE + 1) * sizeof(unsigned char )); // +1: Save space for the final '\0' char
    // Returns after 5 chars have been input
    size_t bytes = read(fd, buf, BUF_SIZE);
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
        perror(serialPortName);
        exit(-1);
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

int message(int fd,struct array payload,unsigned char a,unsigned char c ){
    emissor(fd, payload);
    int alarmCount = 0;

    //register time
    long int start = time(NULL);
    long int end;
    int myState = START;

    while (alarmCount < numTransmissions && myState != END){
        //receiving and processing the data
        struct array bytes = receptor(fd);
        updateState(bytes, &myState,c);
        free(bytes.content);
        end = time(NULL);

        if(((double)(end-start))>= TIMEOUT && myState != END){
            printf("No response sending frame again\n");
            alarmCount++;
            emissor(fd, payload);
            start = time(NULL);
        }
        if(myState == END){
            state = START;
            printf("Unexpected response sending frame again\n");
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
    if(message(fd,payload,A,UA) == 0){
        printf("Connection established\n");
        return 0;
    }
    else{
        printf("Connection failed\n");
        return 1;
    }
}

int disconnect(int fd){
    unsigned char a[] = {FLAG,A,DISC,A^DISC,FLAG};
    struct array payload = {a,5};

    unsigned char b[] = {FLAG,A,UA,A^UA,FLAG};
    struct array ua = {b,5};
    emissor(fd,ua);

    if(message(fd,payload,A,UA) == 0){
        printf("Connection terminated\n");
        return 0;
    }
    else {
        printf("unable to terminate connection\n");
        return 1;
    }

}

struct array processData(int fd, unsigned char *buf, int* i) {
    unsigned char* data = malloc(BUF_SIZE * sizeof(unsigned char));
    int dataSize = 0;
    unsigned char BCC2 = 0; //zero is the neutral element of XOR
    while(state != END && (*i < BUF_SIZE)){
        (*i)++;
        read(fd, (void*)&buf[*i], 1);
        if(buf[*i] == FLAG_RCV){
            if(BCC2 == 0){
                state == END;
                return  (struct array){data,dataSize};
            }else{
                state = START;
                free(data);
                return (struct array){NULL,-1};
            }
        }
        if(buf[*i] == ESC){
            (*i)++;
            read(fd, (void*)&buf[*i], 1);
            data[dataSize] = XOR_ESC ^ buf[*i];
        }else{
            data[dataSize] = buf[*i];
            BCC2 ^= data[dataSize];
        }


    }
    if(dataSize > 0){
        data = realloc(data,dataSize);
        return (struct array){data,dataSize};
    }
    return (struct array){NULL,-1};
}

struct array getData(int fd){
    unsigned char c = 0xFF; //garbage value
    unsigned char* buf = malloc(BUF_SIZE * sizeof(unsigned char));
    int i = 0;
    struct array data;
    struct array response;

    while(connected == 1 || connected == 2) {
        state = START;
        while (state != END && i < BUF_SIZE) {
            if(state == START)i = 0;
            read(fd,&buf[i],1);
            switch (state) {
                case START:
                    state = buf[i] == FLAG ? FLAG_RCV : START;
                    break;
                case FLAG_RCV:
                    state = buf[i] == A ? A_RCV : START;
                    break;
                case A_RCV:
                    state = buf[i] == FLAG ? FLAG_RCV : C_RCV;
                    c = buf[i];
                    break;
                case C_RCV:
                    if (buf[i] == (c ^ A)) {
                        state = BCC_OK;
                        if(c == I1 || c == I0) data = processData(fd, buf, &i);
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
            i++;
            printf("state:%d\n", state);
        }
        struct frame myFrame     = {0,data.size,data.content,c};
        if(response.size > 0)emissor(fd,response);
        if((c == I1 || c == I0) && i < BUF_SIZE)break;//prevents frames bigger than BUF_SIZE
        if(c != DISC)connected = 1;
        if(c == UA && connected == 2)connected = -1;
    }

    //need review
    if(connected == 1 && data.size > 0){
        free(buf);
        for (int j = 0; j < data.size; ++j) {
            printf("%02x",data.content[j]);
        }
        printf("\n");

        return data;
    }
    if(connected == -1){
        free(buf);
        free(data.content);
        connected = -1;
    }
    return (struct array){NULL,-1};
}

int sendData(int fd, struct array data){
    const unsigned char header[] = {FLAG, A, C  , A ^ C};
    const unsigned char footer[] = { FLAG};

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


    unsigned char* payload = malloc((4+data.size+2)*sizeof(unsigned char));
    struct array a = {payload,(4+data.size+2)};

    memcpy(payload,   header, (4* sizeof(unsigned char)));
    memcpy(payload + 4, data.content,   data.size * sizeof(unsigned char));
    memcpy(payload + 4 + data.size+1 ,footer,sizeof(unsigned char));

    int r = message(fd,a,A, C == I0?RR1:RR0);

    free(temp.content);
    free(payload);
    free(data.content);

    if(C == I0)C = I1;
    else C = I0;

    return r;
}

int main(int argc, char *argv[]) {

    const char *serialPortName = argv[1];

    // Program usage: Uses either COM1 or COM2

    if (argc != 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }else{
        int fd = openPort(serialPortName);
        if(connect(fd) != 0)return 1;
        unsigned char* a = malloc(6* sizeof(unsigned char ));

        a[0] = 0x01;
        a[1] = 0x02;
        a[2] = 0x03;
        a[3] = 0x04;
        a[4] = 0x05;
        a[5] = 0x06;
        sendData(fd,(struct array){a,6});
        disconnect(fd);
        closePort(fd);
    }
}
