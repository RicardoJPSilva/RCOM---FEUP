/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <netdb.h>

#define SERVER_PORT 21
#define SERVER_ADDR "193.137.29.15"

struct args{
    char* name;
    char* pass;
    char* ip;
    char* path;
};

struct args args;
int sockfd;
FILE* filefd;

char* getIP(const char* hostName){
    struct hostent *h;

    if ((h = gethostbyname(hostName)) == NULL) {
        herror("unable to find the host ip address");
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    return inet_ntoa(*((struct in_addr *) h->h_addr));
}


int readSocket(){
    char r[1000];

    printf("reading\n");
    FILE * a = fdopen(sockfd,"r");
    do {
        fgets(r,1000, a);
        printf("%s",r);
    } while (r[3] != ' ');

    return atoi(r);
}

void getSocket(const char* ip_address, uint16_t port){
    /*server address handling*/

    struct sockaddr_in server_addr;

    memset((char *) &server_addr,'0',sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("unable to .connect socket");
        exit(-1);
    }

    filefd = fdopen(sockfd,"r");
}

//ftp://user:pass@ftp.up.pt/path
void parceArgs(char* value) {

    char* token = strtok(value,":"); // ftp:

    if (token == NULL || strcmp(token,"ftp") != 0){
        printf("error:protocol must be ftp\n");
        printf("found:%s\n",token);
        exit(1);
    }

    value = &strtok(NULL,"")[2];// pass@ftp.up.pt/path
    args.name = strtok(value,":");
    if(args.name == NULL){
        printf("error:username not found\n");
        exit(1);
    }
    printf("name:%s\n",args.name);

    args.pass = strtok(NULL,"@");
    if(args.pass == NULL) {
        printf("error:password not found\n");
    }
    printf("password:%s\n",args.pass);


    args.ip = getIP(strtok(NULL,"/"));
    args.path = strtok(NULL,"");
    if(args.path == NULL){
        printf("error:no path provided\n");
    }

    printf("path:%s\n",args.path);
}

void login(){
    char buf[1000] = "user ";
    strcat(buf,args.name);
    strcat(buf,"\n");
    char r[1000];

    printf("sending:%s",buf);
    write(sockfd,buf,strlen(buf));

    int response = readSocket();

    if(response / 100 != 3){
        printf("unexpected response\n");
        exit(1);
    }

    strcpy(buf,"pass ");
    strcat(buf,args.pass);
    strcat(buf,"\n");

    printf("sending:%s",buf);
    if(write(sockfd,buf, strlen(buf)) < 0){
        printf("error writting");
        exit(1);
    }

    response = readSocket();

    if(response/ 100 != 2){
        printf("unexpected response");
        exit(1);
    }

}
void downloadFile(){
    char const* filename = &strrchr(args.path,'/')[1];

    FILE* f = fopen(filename,"w");

    if(f == NULL) {
        printf("It wasn't possible to open the file");
        exit(1);
    }

    char buf[1000];
    int bytes;
    while((bytes = read(sockfd, buf, sizeof(buf)))){
        if(bytes < 0){
            printf("Error reading from data socket\n");
            exit(1);
        }
        if((bytes = fwrite(buf, bytes, 1, f)) < 0){
            printf("Error writing data to file\n");
            exit(1);
        }
    }
    printf("download finished\n");

    fclose(f);
}

void download(){
    printf("sending pasv\n");
    write(sockfd,"pasv\n",5);

    char* response = malloc(1000);

    do {
        fgets(response, 1000, filefd);
        printf("%s", response);
    } while (response[3] != ' ');

    if(atoi(response)/100 != 2){
        printf("unexpected response");
        exit(1);
    }

    char ip[20] = "";
    int num[6];

    //parse pasv response
    strtok(response,"(");
    char* temp = strtok(NULL,"(");

    num[0] = atoi(strtok(temp, ","));
    for (int i = 1; i < 6; ++i) {
        num[i] = atoi(strtok(NULL,","));
    }

    sprintf(ip,"%d.%d.%d.%d",num[0],num[1],num[2],num[3]);
    int port = num[4]*256+num[5];
    printf("download at:\nip:%s\nport:%d\n",ip,port);

    char buf[1000] = "retr ";
    strcat(buf,args.path);
    strcat(buf,"\n");
    printf("%s\n",buf);




    write(sockfd,buf, strlen(buf));
    getSocket(ip,port);

    downloadFile();

    free(response);

}

/*
 *  user @user
 *  pass @pass
 *  cwd
 *  pasv
 *  pasv
 *      227 Entering Passive Mode (193,136,28,12,19,91)
 *          ip 193.136.28.12
 *          port 19 * 256 + 91
 *  retr
 *
 *
 */


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("****Missing argument\n");
        exit(1);
    }

    parceArgs(argv[1]);

    getSocket(args.ip,SERVER_PORT);
    readSocket();

    login();

    download();

}




