#include "sys/socket.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string>
#include "sys/sendfile.h"
int read_command(FILE* f, char* buffer, int n){
    int i=0;
    while(i<n){
        char c=fgetc(f);
        assert(c>=0);
        buffer[i]=c;
        if(c==0){
            return i;
        }
        i++;
    }
    //drain up a command
    while(fgetc(f)!=0);
    return i;
}
int main(int argc, char** argv){
    int sockfd;
    struct sockaddr_in servaddr;
    if(argc!=3){
        printf("usage: %s [host] [port]\n", argv[0]);
        return 1;
    }
    sockfd=socket(AF_INET, SOCK_STREAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    int ret=connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(ret<0){
        perror("connect failed");
        return 2;
    }
    FILE* f=fdopen(sockfd, "r");
    std::string command_buffer;
    int state=0; //0 for command, 1 for retr size received, 2 for ready to send
    int is_storing=0;
    int is_shutdown=0;
    while(1){
        command_buffer.clear();
        state=0;
        char ch;
        unsigned long long size=0;
        while((ch=fgetc(f))!=0){
            if(state==1){
                size=size*10+ch-'0';
            }else
            if(ch==' ' && state==0) state=1;
            else if(is_storing){
                if(ch=='+') state=2;
                is_storing=0;
            }else if(state==0) state=3;
            putchar(ch);
        }
        putchar('\n');
        if(is_shutdown==1){
            shutdown(sockfd, SHUT_RDWR);
            return 0;
        }
        if(state==1){
input_file_name:
            printf("Please input file name: ");
            fflush(stdout);
            command_buffer.clear();
            while((ch=getchar())!='\n'){
                command_buffer.push_back(ch);
            }
            if(command_buffer.size()==0){
                printf("STOP\n");
                write(sockfd, "STOP\0", 5);
                //receive next instruction.
                continue;
            }
            int fd=open(command_buffer.c_str(), O_WRONLY|O_CREAT, 00700);
            if(fd<0){
                printf("Open file failed: %s\n", strerror(errno));
                goto input_file_name;
            }
            printf("SEND\n");
            write(sockfd, "SEND\0", 5);
            char buffer[1024];
            uint64_t total_buffer=size/1024;
            uint64_t partial=size%1024;
            while(total_buffer--){
                fread(buffer, 1, 1024, f);
                write(fd, buffer, 1024);
            }
            fread(buffer, 1, partial, f);
            write(fd, buffer, partial);
            //printf("Received %ld bytes\n", size);
            close(fd);
            /*
            struct stat statbuf;
            if(fstat(fd, &statbuf)!=0){
                printf("Get file size failed: %s\n", strerror(errno));
                close(fd);
                goto input_file_name;
            }
            printf("SIZE %ld\n", statbuf.st_size);
            char buffer[1024];
            sprintf(buffer, "SIZE %ld%c", statbuf.st_size, (char)0);
            */
        }else if(state==2){
input_file_name_stor:
            printf("Please input file name: ");
            fflush(stdout);
            command_buffer.clear();
            while((ch=getchar())!='\n'){
                command_buffer.push_back(ch);
            }
            int fd=open(command_buffer.c_str(), O_RDONLY, 00700);
            if(fd<0){
                printf("Open file failed: %s\n", strerror(errno));
                goto input_file_name_stor;
            }
            struct stat statbuf;
            if(fstat(fd, &statbuf)!=0){
                printf("Get file size failed: %s\n", strerror(errno));
                close(fd);
                goto input_file_name;
            }
            printf("SIZE %ld\n", statbuf.st_size);
            char buffer[1024];
            sprintf(buffer, "SIZE %ld", statbuf.st_size);
            write(sockfd, buffer, strlen(buffer));
            buffer[0]=0;
            write(sockfd, buffer, 1);
            // read next char.
            int flag=0;
            while((ch=fgetc(f))!=0){
                if(flag==0 && ch!='+') flag=2;
                if(flag==0) flag=1;
                putchar(ch);
            }
            putchar('\n');
            if(flag==1){ //ready to receive
                //printf("sendfile started\n");
                sendfile(sockfd, fd, NULL, statbuf.st_size);
                close(fd);
                continue;
            }else{
                close(fd);
                
            }
        }else{

        }
scan:
        command_buffer.clear();
        printf("sftp> ");
        fflush(stdout);
        //parse locally then send
        while((ch=getchar())!='\n'){
            command_buffer.push_back(ch);
        }
        if(command_buffer.size()>=4 && !strncasecmp(command_buffer.c_str(), "stor", 4)){
            //printf("stor\n");
            is_storing=1;
        }
        if(command_buffer.size()>=4 && !strncasecmp(command_buffer.c_str(), "done", 4)){
            is_shutdown=1;
        }
        write(sockfd, command_buffer.c_str(), command_buffer.size());
        ch=0;
        write(sockfd, &ch, 1);
    }
}
