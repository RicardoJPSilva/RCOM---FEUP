// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <wait.h>

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

#define RETRANSMITION_COUNT 4

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;
int fd;

struct termios oldtio;

#define BUF_SIZE 256

volatile int STOP = FALSE;


enum states{
    START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,END
};
int state = START;
//include the flags
unsigned char* stuff(const unsigned char *buf, int size){
    unsigned char *stuffed = malloc(2*(size-2));
    stuffed[0] = FLAG;
    int j = 1;
    for (int i = 1; i < size-1; ++i) {
        if(buf[i] == FLAG){
            stuffed[j] = ESC;
            stuffed[++j] = FLAG^XOR_ESC;
        }else if(buf[i] == ESC){
            stuffed[j] = ESC;
            stuffed[++j] = ESC^XOR_ESC;
        }else{
            stuffed[j]=buf[i];
        }
        j++;
    }
    stuffed[j] = FLAG;

    return stuffed;
}

void receptor(){

    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    // Returns after 5 chars have been input
    int bytes = read(fd, buf, BUF_SIZE);
    buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

    //printing the hexadecimal code received
    for (int i = 0; i < bytes; ++i) {
        printf("%02X",buf[i]);
    }
    if(bytes > 0)printf(":%d\n",bytes);

    //interpreting the received bytes with a state machine
    for (int i = 0; i < bytes; i++){
        switch (state){
            case START:
                if (buf[i] == FLAG)
                {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (buf[i] == A){
                    state = A_RCV;
                }else{
                    state = START;
                }
                break;
            case A_RCV:
                if (buf[i] == FLAG)
                {
                    state = FLAG_RCV;
                }else if (buf[i] == UA)
                {
                    state = C_RCV;
                }else{
                    state = START;
                }
                break;
            case C_RCV:
                if (buf[i] == (A^UA))
                {
                    state = BCC_OK;
                }else if(buf[i] == FLAG)
                {
                    state = FLAG_RCV;
                }else{
                    state = START;
                }
                break;
            case BCC_OK:
                if (buf[i] == FLAG)
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

void emissor(){

    alarmEnabled = FALSE;
    alarmCount++;


    // Create string to send
    // read the payload
    unsigned char payload[] = {FLAG, A, SET, A ^ SET, FLAG};

    // printing the hexadecimal value of the payload
    for (int i = 0; i < sizeof(payload)/sizeof(char); ++i) {
        printf("%02X",payload[i]);
    }
    printf("\n");

    // writing the output
    int bytes = write(fd, payload, sizeof(payload)/sizeof(char));
    printf("%d bytes written\n", bytes);
}

int openPort(const char *serialPortName){
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-SET.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);

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

void closePort(){
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}

void connect(){

    emissor();
    long int start = time(NULL);
    long int end;

    while (alarmCount < RETRANSMITION_COUNT && STOP == FALSE){
        receptor();
        end = time(NULL);
        if(((double)(end-start))>= 3){
            emissor();
            start=end;
        }
    }

    if(alarmCount == RETRANSMITION_COUNT)printf("Connection failed");
    else printf("Connection established");

    // Wait until all bytes have been written to the serial port
    sleep(1);
}

int main(int argc, char *argv[]) {

    const char *serialPortName = argv[1];

    // Program usage: Uses either COM1 or COM2

    if (argc < 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    openPort(serialPortName);
    connect();
    closePort();

}