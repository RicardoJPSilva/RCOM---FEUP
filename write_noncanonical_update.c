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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source


#define FLAG 0x7e
#define A 0x03
#define C 0x03
#define UA 0x07

enum states{
    START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,STOP2
};

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{

    const char *serialPortName = argv[1];

    // Program usage: Uses either COM1 or COM2


    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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

    // Create string to send
    //read the payload
    unsigned char payload[] = {FLAG,A,C,A^C,FLAG};
    for (int i = 0; i < 5 ; ++i) {
        printf("%X",payload[i]);
    }
    printf("\n");

    int bytes = write(fd, payload, sizeof(payload)/sizeof(char));
    printf("%d bytes written\n", bytes);
    
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    int state = START;

    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

        printf(":%s:%d\n", buf, bytes);
        printf(":");
        for (int i = 0; i < bytes; ++i) {
            printf("%X",buf[i]);
        }
        printf(":%d\n",bytes);

        for (int i = 0; i < bytes; i++)
        {
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
                }else if (buf[i] = UA)
                {
                    state = C_RCV;
                }else{
                    state = START;
                }
                break;
            case C_RCV:
                if (buf[i] == A^UA)
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
                    state = STOP2;
                }else{
                    state = START;
                }
                break;
            case STOP2:

                STOP = TRUE;
                break;
            } 
            printf("state:%d\n",state);
        }



    }

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
