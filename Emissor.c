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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source


#define FLAG 0x7e
#define A 0x03
#define SET 0x03
#define UA 0x07
#define ESC 0X7d
#define XOR_ESC 0X20
#define DISC 0X0B
#define RR 0x05
#define REJ 0x01

#define numTransmissions 4
#define TIMEOUT 3

#define FALSE 0
#define TRUE 1

char C = 0x00;


struct termios oldtio;

#define BUF_SIZE 256

volatile int STOP = FALSE;


enum states{
    START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,END
};
int state = START;

struct array{
    unsigned char *content;
    size_t size;
};

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

struct array destuff(struct array buf){

    int destuffedSize = 0;
    if(buf.size < 1)return buf;

    for (int i = 0; i < buf.size; ++i) {
        if(buf.content[i] == ESC){
            i++;
            buf.content[destuffedSize] = XOR_ESC^buf.content[i];
        }
        destuffedSize++;
    }

    struct array a = {malloc(destuffedSize*sizeof(unsigned char)), destuffedSize};
    memcpy(a.content,buf.content,destuffedSize);
    return a;

}

//So funciona com tramas de supervição e não numeradas
void updateState(struct array buf,unsigned char a,unsigned char c){
    for (int i = 0; i < buf.size; i++){
        switch (state){
            case START:
                if (buf.content[i] == FLAG)
                {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (buf.content[i] == a) {
                    state = A_RCV;
                }else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf.content[i] == FLAG)
                {
                    state = FLAG_RCV;
                }else if (buf.content[i] == c)
                {
                    state = C_RCV;
                }else{
                    state = START;
                }
                break;
            case C_RCV:
                if (buf.content[i] == (a^c))
                {
                    state = BCC_OK;
                }else if(buf.content[i] == FLAG)
                {
                    state = FLAG_RCV;
                }else{
                    state = START;
                }
                break;
            case BCC_OK:
                if (buf.content[i] == FLAG)
                {
                    state = END;
                }else{
                    state = START;
                }
            case END:
                STOP = TRUE;
                break;
            default:
                state = START;
        }
        printf("state:%d\n",state);
    }
}

struct array receptor(int fd){

    unsigned char* buf = (unsigned char*) malloc((BUF_SIZE + 1) * sizeof(unsigned char )); // +1: Save space for the final '\0' char
    // Returns after 5 chars have been input
    size_t bytes = read(fd, buf, BUF_SIZE);
    buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

    //printing the hexadecimal code received
    for (int i = 0; i < bytes; ++i) {
        printf("%02X",buf[i]);
    }
    if(bytes > 0){
        printf(":%lu\n",bytes);
        buf = realloc(buf,bytes);
    }


    //interpreting the received bytes with a state machine

    struct array a = {buf, bytes};

    return a;
}

void emissor(int fd, struct array payload){

    // printing the hexadecimal value of the payload
    for (int i = 0; i < payload.size; ++i) {
        printf("%02X",payload.content[i]);
    }
    printf("\n");

    // writing the output
    size_t bytes = write(fd, payload.content, payload.size);
    printf("%lu bytes written\n", bytes);
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


    while (alarmCount < numTransmissions && STOP == FALSE){
        //receiving and processing the data
        struct array bytes = receptor(fd);
        state = START;
        updateState(bytes,a,c);
        free(bytes.content);

        end = time(NULL);
        if(((double)(end-start))>= TIMEOUT){
            alarmCount++;
            emissor(fd, payload);
            start=end;
        }
    }

    if(alarmCount == numTransmissions)return -1;
    else return 0;
}

void connect(int fd){
    //build the set payload
    unsigned char a[] = {FLAG, A, SET, A ^ SET, FLAG};
    struct array payload = {a,5};
    if(message(fd,payload,A,UA) == 0) printf("Connection established");
    else printf("Connection failed");
}

void desconect(int fd){
    unsigned char a[] = {FLAG,A,DISC,A^DISC,FLAG};
    struct array payload = {a,5};

    if(message(fd,payload,A,UA) == 0)printf("Connection terminated");
    else printf("unable to terminate connection");

    unsigned char b[] = {FLAG,A,UA,A^UA,FLAG};
    struct array ua = {b,5};
    emissor(fd,ua);
}

void processData(int fd,struct array data,char c){
    int xor = data.content[0];
    for (int j = 1; j < data.size-1; ++j) {
        xor ^= data.content[j];
    }
    if(xor == data.content[data.size-1]){
        unsigned char a[] = {FLAG,A,c+RR,A^(c+RR),FLAG};

        emissor(fd,(struct array){a,6});
    }
    else{
        unsigned char a[] = {FLAG,A,c+REJ,A^(c+REJ)};
        emissor(fd,(struct array){a,6});
    }
}

void readData(int fd){
    struct array buf = receptor(fd);
    state = START;
    unsigned char c;
    unsigned char data[BUF_SIZE];
    int dataSize = 0;

    for (int i = 0; i < buf.size; i++){
        switch (state){
            case START:
                if (buf.content[i] == FLAG)
                {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (buf.content[i] == A) {
                    state = A_RCV;
                }else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf.content[i] == FLAG){
                    state = FLAG_RCV;
                }else if (buf.content[i] == 0x45){
                    c = 0x40;
                    state = C_RCV;
                }else if(buf.content[i] == 0x05){
                    c = 0x00;
                    state = C_RCV;
                }else{
                    state = START;
                }
                break;
            case C_RCV:
                if (buf.content[i] == (0x45^c))
                {
                    state = BCC_OK;
                }else if(buf.content[i] == FLAG)
                {
                    state = FLAG_RCV;
                }else{
                    state = START;
                }
                break;
            case BCC_OK:
                if (buf.content[i] == FLAG)
                {
                    processData(fd,(struct array){data,dataSize},c);
                    state = END;
                }else{
                    if(buf.content[i] == ESC){
                        i++;
                        data[dataSize] = XOR_ESC^buf.content[i];
                    }else{
                        data[dataSize] = buf.content[i];
                    }
                    dataSize++;
                }
            case END:
                STOP = TRUE;
                break;
            default:
                state = START;
        }
        printf("state:%d\n",state);
    }

}

void sendData(int fd, struct array data){
    data = stuff(data);

    const unsigned char header[] = {FLAG, A, C, A ^ C};

    if(C == 0x00)C = 0x45;
    else C = 0x05;

    unsigned char BCC2 = data.content[0];
    for (int i = 1; i < data.size; ++i) {
        BCC2 = BCC2 ^ data.content[i];
    }

    const unsigned char footer[] = {BCC2, FLAG};

    unsigned char* payload = malloc((4+data.size+2)*sizeof(unsigned char));
    struct array a = {payload,(4+data.size+2)};

    memcpy(payload,   header, (4* sizeof(unsigned char)));
    memcpy(payload+4, data.content,   data.size* sizeof(unsigned char));
    memcpy(payload+data.size+4,footer,2* sizeof(unsigned char));

    message(fd,a,A,C == 0x00 ? 0x05 :0x85);

    free(payload);
    free(data.content);
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
    connect(fd);
        closePort(fd);
    }

}
