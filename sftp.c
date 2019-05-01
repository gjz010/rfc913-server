#define _BSD_SOURCE
#include <sys/sysmacros.h>
#include <stdlib.h>
#include <time.h>
#include "poll.h"
#include "file.h"
#include "coroutine.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "stdio.h"
#include "dirent.h"
#include "errno.h"
#include <sys/stat.h>
#include "unistd.h"
#include <fcntl.h>
#include <string.h>
#include "netinet/in.h"
#include "netinet/tcp.h"
inline void ex_putchar_async(file_handle* fh, char ch){
    ex_fwrite_async(fh, &ch, 1);
}
inline int ex_readline_async(file_handle* fh, char* buf, int max_size){
    int i=0;
    char chr;
    for(i=0;i<max_size;i++){
        buf[i]=ex_getchar_async(fh);
        if(buf[i]==0) return i;
    }
    //The command is too long for the buffer, so we drop the rest of the command.
    while(ex_getchar_async(fh)==0);
    return max_size;
}
int check_username_and_password(const char* username, const char* password){
    return (!strcmp(username, "gjz010")) && (!strcmp(password, "sftpserver"));
}
DIR *opendirat(int dir, const char *dirname) {
  // Open directory.
  int fd = openat(dir, dirname, O_RDONLY | O_NONBLOCK | O_DIRECTORY);
  if (fd == -1)
    return NULL;

  // Create directory handle.
  DIR *result = fdopendir(fd);
  if (result == NULL)
    close(fd);
  return result;
}
const char* resolve_type(uint64_t mode){
    switch(mode & S_IFMT){
        case S_IFBLK: return "BlockDev";
        case S_IFCHR: return "CharacterDev";
        case S_IFDIR: return "Directory";
        case S_IFIFO: return "NamedFIFO";
        //case S_IFLINK: return "SymLink";
        case S_IFREG: return "RegularFile";
        case S_IFSOCK: return "Socket";
        default: return "<unknown>";
    }
}

void sftp_conn(void* pfh){
    file_handle* fh=pfh;
    volatile DIR* cwd=opendir("./");
    volatile int helper_fd=-1;
    char command_buffer[1024];
    char data_buffer[8192];
    if(coroutine_catch){ // exception handler.
        //printf("ouch!\n");
        goto shutdown;
    }else{
        struct linger lg;
        lg.l_onoff=1;
        lg.l_linger=0;
        if(setsockopt(fh->fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg))==-1){
            shutdown(fh->fd, SHUT_RDWR);
            coroutine_throw;
        }
        int keepalive_enabled = 1;
        int keepalive_time = 180;
        int keepalive_count = 3;
        int keepalive_interval = 10;
        int ssoret;
        ssoret=setsockopt(fh->fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive_enabled, sizeof(int));
        if(ssoret==-1) coroutine_throw;
        ssoret=setsockopt(fh->fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(int));
        if(ssoret==-1) coroutine_throw;
        ssoret=setsockopt(fh->fd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(int));
        if(ssoret==-1) coroutine_throw;
        ssoret=setsockopt(fh->fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval, sizeof(int));
        if(ssoret==-1) coroutine_throw;
        disable_nagle(fh);

        ex_fhprintf_async(fh, "+MIT-XX SFTP Service made by gjz010 with love!");
        ex_putchar_async(fh, 0);
        // initialize the state machine
        char username[1024];
        int auth_state=0; //0 for not-authorized, 1 for waiting for password and 2 for logged in.
        int rename_state=0;
        // and start the scanner.
        while(1){
            int len=ex_readline_async(fh, command_buffer, 1024);
            if(len==1024){
                ex_fhprintf_async(fh, "-Your command is too long, longer than 1023 bytes.");
                ex_putchar_async(fh, 0);
                continue;
            }
            if(len<4 || (len>4 && command_buffer[4]!=' ')){
                ex_fhprintf_async(fh, "-No such command.");
                ex_putchar_async(fh, 0);
                continue;
            }
            // we've got the command, then parse it.
            if(!strncasecmp(command_buffer, "DONE", 4)){
                ex_fhprintf_async(fh, "+MIT-XX closing connection. Goodbye and have a nice dream!");
                ex_putchar_async(fh, 0);
                break; //goto shutdown.
            }else
            if(!strncasecmp(command_buffer, "USER", 4)){
                if(auth_state==0){
                    //This is safe.
                    strcpy(username, command_buffer+5);
                    auth_state=1;
                    ex_fhprintf_async(fh, "+%s ok, send password", username);
                    ex_putchar_async(fh, 0);
                }else if(auth_state==1){
                    ex_fhprintf_async(fh, "-Please send your password.");
                    ex_putchar_async(fh, 0);
                }else{
                    ex_fhprintf_async(fh, "-You have already logged in as %s.", username);
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "ACCT", 4)){
                ex_fhprintf_async(fh, "-ACCT not implemented on this server.");
                ex_putchar_async(fh, 0);
            }else
            if(!strncasecmp(command_buffer, "PASS", 4)){
                // sleep a second here to prevent brute-force attack.
                if(auth_state==2){
                    ex_fhprintf_async(fh, "-You have already logged in as %s.", username);
                    ex_putchar_async(fh, 0);
                }else{
                    sleep_async(2000);
                    if(check_username_and_password(username, command_buffer+5)){
                        auth_state=2;
                        ex_fhprintf_async(fh, "! %s logged in. Have fun...", username);
                        ex_putchar_async(fh, 0);
                    }else{
                        ex_fhprintf_async(fh, "-Wrong password, try again");
                        ex_putchar_async(fh, 0);
                        auth_state=0;
                    }
                }
            }else
            if(auth_state!=2){
                ex_fhprintf_async(fh, "-Please login first.");
                ex_putchar_async(fh, 0);
            }else
            if(!strncasecmp(command_buffer, "TYPE", 4)){
                ex_fhprintf_async(fh, "-Type Binary is mandatory on the server.");
                ex_putchar_async(fh, 0);
            }else
            if(!strncasecmp(command_buffer, "LIST", 4)){
                if(len>=8 && (command_buffer[5]=='f' || command_buffer[5]=='F' || command_buffer[5]=='v' || command_buffer[5]=='V') && command_buffer[6]==' '){
                    //printf("list\n");
                    DIR* new_dir=opendirat(dirfd(cwd), command_buffer+7);
                    if(new_dir==0){
                        int last_errno=errno;
                        ex_fhprintf_async(fh, "-Open directory %s error(%d): %s", command_buffer+7, last_errno, strerror(last_errno));
                        ex_putchar_async(fh, 0);
                    }else{
                        //printf("found\n");
                        int verbose=(command_buffer[5]=='v' || command_buffer[5]=='V');
                        struct dirent* elem;
                        ex_fhprintf_async(fh, "+%s:\r\n", command_buffer+7);
                        if(verbose){
                            ex_fhprintf_async(fh, "File name\tType\tSize(Bytes)\r\n");
                        }
                        while((elem=readdir(new_dir))!=NULL){
                            //printf("%s\n", elem->d_name);
                            if(!verbose){
                                ex_fhprintf_async(fh, "%s\r\n", elem->d_name);
                            }else{
                                struct stat statbuf;
                                if(fstatat(dirfd(new_dir), elem->d_name, &statbuf, 0)==0){
                                    ex_fhprintf_async(fh, "%s\t%s\t%ld\r\n", elem->d_name, resolve_type(statbuf.st_mode), statbuf.st_size);
                                }else{
                                    ex_fhprintf_async(fh, "%s\t<unknown>\t<unknown>\r\n", elem->d_name);
                                }
                            }
                        }
                        ex_putchar_async(fh, 0);
                    }
                    closedir(new_dir);
                    
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: LIST { F | V } directory-path");
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "CDIR", 4)){
                if(len>=6){
                    DIR* new_dir=opendirat(dirfd(cwd), command_buffer+5);
                    if(new_dir==0){
                        int last_errno=errno;
                        ex_fhprintf_async(fh, "-Can't connect to directory because: (%d) %s", last_errno, strerror(last_errno));
                        ex_putchar_async(fh, 0);
                    }else{
                        closedir(cwd);
                        cwd=new_dir;
                        ex_fhprintf_async(fh, "!Changed working dir to %s", command_buffer+5);
                        ex_putchar_async(fh, 0);
                    }
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: CDIR new-directory");
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "KILL", 4)){
                if(len>=6){
                    int ret=unlinkat(dirfd(cwd), command_buffer+5, 0);
                    if(ret<0 && errno==EISDIR){
                        ret=unlinkat(dirfd(cwd), command_buffer+5, AT_REMOVEDIR);
                    }
                    if(ret<0){
                        int last_errno=errno;
                        ex_fhprintf_async(fh, "-Not deleted because: (%d) %s", last_errno, strerror(last_errno));
                        ex_putchar_async(fh, 0);
                    }else{
                        ex_fhprintf_async(fh, "+%s deleted", command_buffer+5);
                        ex_putchar_async(fh, 0);
                    }
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: KILL file-spec");
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "NAME", 4)){
                // place TOBE in this loop.
                // the sad thing is that splitting the command into two breaks atomicity.
                if(len>=6){
                    if(faccessat(dirfd(cwd), command_buffer+5, W_OK|R_OK, 0)==0){
                        strcpy(data_buffer, command_buffer+5);
                        ex_fhprintf_async(fh, "+File exists");
                        ex_putchar_async(fh, 0);
                        while(1){
                            len=ex_readline_async(fh, command_buffer, 1024);
                            if(len==1024){
                                ex_fhprintf_async(fh, "-Your command is too long, longer than 1023 bytes.");
                                ex_putchar_async(fh, 0);
                                continue;
                            }
                            if(len>=6 && !strncasecmp(command_buffer, "TOBE", 4)){
                                int ret=renameat(dirfd(cwd), data_buffer, dirfd(cwd), command_buffer+5);
                                if(ret<0){
                                    int last_errno=errno;
                                    ex_fhprintf_async(fh, "-File wasn't renamed because (%d) %s", last_errno, strerror(last_errno));
                                    ex_putchar_async(fh, 0);
                                }else{
                                    ex_fhprintf_async(fh, "+%s renamed to %s", data_buffer, command_buffer+5);
                                    ex_putchar_async(fh, 0);
                                }
                            }else{
                                ex_fhprintf_async(fh, "-File wasn't renamed because the command is cancelled");
                                ex_putchar_async(fh, 0);
                            }
                            break;
                        }
                    }else{
                        ex_fhprintf_async(fh, "-Can't find %s", command_buffer+5);
                        ex_putchar_async(fh, 0);
                    }
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: NAME old-file-spec");
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "DONE", 4)){
                ex_fhprintf_async(fh, "+MIT-XX closing connection. Goodbye and have a nice dream!");
                ex_putchar_async(fh, 0);
                break; //goto shutdown.
            }else
            if(!strncasecmp(command_buffer, "RETR", 4)){
                if(len>=6){
                    helper_fd=openat(dirfd(cwd), command_buffer+5, O_RDONLY);
                    if(helper_fd>=0){
                        struct stat statbuf;
                        if(fstat(helper_fd, &statbuf)!=0){
                            int last_errno=errno;
                            ex_fhprintf_async(fh, "-Retriving file stat failed because (%d) %s", last_errno, strerror(last_errno));
                            ex_putchar_async(fh, 0);
                            close(helper_fd);
                            helper_fd=-1;
                            continue;
                        }
                        if((statbuf.st_mode & S_IFMT)!=S_IFREG){
                            ex_fhprintf_async(fh, "-You can only retrive regular file.");
                            ex_putchar_async(fh, 0);
                            close(helper_fd);
                            helper_fd=-1;
                            continue;
                        }
                        ex_fhprintf_async(fh, " %ld", statbuf.st_size);
                        ex_putchar_async(fh, 0);
                        while(1){
                            len=ex_readline_async(fh, command_buffer, 1024);
                            if(len==1024){
                                ex_fhprintf_async(fh, "-Your command is too long, longer than 1023 bytes.");
                                ex_putchar_async(fh, 0);
                                continue;
                            }
                            if(len!=4 || (strncasecmp(command_buffer, "SEND", 4) && strncasecmp(command_buffer, "STOP", 4))){
                                ex_fhprintf_async(fh, "-Please try again: use SEND to start sending and use STOP to cancel.");
                                ex_putchar_async(fh, 0);
                            }else{
                                if(!strncasecmp(command_buffer, "SEND", 4)){
                                    enable_nagle(fh);
                                    //Magic!
                                    ex_sendfile_async(fh, helper_fd, statbuf.st_size);
                                    disable_nagle(fh);
                                }else{ //STOP
                                    ex_fhprintf_async(fh, "+ok, RETR aborted");
                                    ex_putchar_async(fh, 0);
                                }
                                close(helper_fd);
                                helper_fd=-1;
                                break;
                            }
                        }
                    }else{
                        helper_fd=-1;
                        int last_errno=errno;
                        ex_fhprintf_async(fh, "-Open file failed because (%d) %s", last_errno, strerror(last_errno));
                        ex_putchar_async(fh, 0);
                    }
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: RETR file-spec");
                    ex_putchar_async(fh, 0);
                }
            }else
            if(!strncasecmp(command_buffer, "STOR", 4)){
                if(len>=10 && !(strncasecmp(command_buffer+5, "NEW", 3) && strncasecmp(command_buffer+5, "OLD", 3) && strncasecmp(command_buffer+5, "APP", 3) )){
                    char name_buffer[1024]; // a bit too wasteful?
                    int option=0;
                    int open_errno=0;
                    uint64_t original_size=0;
                    strcpy(name_buffer, command_buffer+9);
                    //printf("store\n");
                    if (!strncasecmp(command_buffer+5, "NEW", 3)){
                        option=0;
                        helper_fd=openat(dirfd(cwd), command_buffer+9, O_WRONLY|O_EXCL|O_CREAT, 00644);
                        //printf("opened %d %d\n", helper_fd, errno);
                        if(helper_fd<0){
                            helper_fd=-1;
                            if(errno==EEXIST){
                                ex_fhprintf_async(fh, "-File exists, but system doesn't support generations");
                                ex_putchar_async(fh, 0);
                                continue; //Continue?
                            }else open_errno=errno;
                        }else{
                            //printf("opened\n");
                            ex_fhprintf_async(fh, "+File does not exist, will create new file");
                            ex_putchar_async(fh, 0);
                        }
                    }else
                    if (!strncasecmp(command_buffer+5, "OLD", 3)){
                        option=1;
                        int exist=(faccessat(dirfd(cwd), command_buffer+9, W_OK, 0)==0);
                        helper_fd=openat(dirfd(cwd), command_buffer+9, O_WRONLY|O_CREAT,00644);
                        if(helper_fd<0){    //OLD should always return a '+'
                            helper_fd=-1;
                            open_errno=errno;
                        }else{
                            if(exist){
                                ex_fhprintf_async(fh, "+Will write over old file");
                            }else{
                                ex_fhprintf_async(fh, "+Will create new file");
                            }
                            ex_putchar_async(fh, 0);
                        }
                    }else{
                        option=2;
                        int exist=(faccessat(dirfd(cwd), command_buffer+9, W_OK, 0)==0);
                        helper_fd=openat(dirfd(cwd), command_buffer+9, O_WRONLY|O_CREAT,00644);
                        if(helper_fd<0){    //APP should always return a '+'
                            helper_fd=-1;
                            open_errno=errno;
                        }else{
                            if(exist){
                                ex_fhprintf_async(fh, "+Will append to file");
                            }else{
                                ex_fhprintf_async(fh, "+Will create new file");
                            }
                            ex_putchar_async(fh, 0);
                            struct stat statbuf;
                            if(fstat(helper_fd, &statbuf)==0){
                                original_size=statbuf.st_size;
                            }else{
                                // failed to get the stat, then silently close the fd.
                                open_errno=errno;
                                close(helper_fd);
                                helper_fd=-1;
                            }
                        }
                    }
                    // wait for size
                    while(1){
                        len=ex_readline_async(fh, command_buffer, 1024);
                        if(len==1024){
                            ex_fhprintf_async(fh, "-Your command is too long, longer than 1023 bytes.");
                            ex_putchar_async(fh, 0);
                            continue;
                        }
                        if(len<6 || (strncasecmp(command_buffer, "SIZE", 4) ) ){
                            ex_fhprintf_async(fh, "-Please try again: use SIZE <number-of-bytes-in-file> to specify size.");
                            ex_putchar_async(fh, 0);
                        }else{
                            uint64_t size=0;
                            for(int i=5; i<len; i++){
                                if(command_buffer[i]>='0' && command_buffer[i]<='9'){
                                    size=size*10+(command_buffer[i]-'0');
                                }else{
                                    ex_fhprintf_async(fh, "-Please try again: use SIZE <number-of-bytes-in-file> to specify size.");
                                    ex_putchar_async(fh, 0);
                                    continue;
                                }
                            }
                            if(helper_fd==-1){
                                //failed at open.
                                ex_fhprintf_async(fh, "-Not enough room, don't send it (open failed: %d %s)", open_errno, strerror(open_errno));
                                ex_putchar_async(fh, 0);
                            }else{
                                if(ftruncate(helper_fd, size+original_size)==0){
                                    //use the slower-than-turtle read-copy procedure.
                                    lseek(helper_fd, original_size, SEEK_SET);
                                    ex_fhprintf_async(fh, "+ok, waiting for file");
                                    ex_putchar_async(fh, 0);
                                    enable_nagle(fh);
                                    uint64_t received_size=0;
                                    uint64_t entire_buffers=size/8192;
                                    uint64_t remaining=size%8192;
                                    for(int i=0;i<entire_buffers;i++){
                                        ex_fread_async(fh, data_buffer, 8192);
                                        write(helper_fd, data_buffer, 8192);
                                        received_size+=8192;
                                        //printf("progress: %ld/%ld\n", received_size, size);
                                    }
                                    ex_fread_async(fh, data_buffer, remaining);
                                    write(helper_fd, data_buffer, remaining);
                                    received_size+=remaining;
                                    //printf("progress: %ld/%ld\n", received_size, size);
                                    disable_nagle(fh);
                                    close(helper_fd);
                                    helper_fd=-1;
                                    ex_fhprintf_async(fh, "+Saved %s", name_buffer);
                                    ex_putchar_async(fh, 0);
                                }else{
                                    close(helper_fd);
                                    helper_fd=-1;
                                    ex_fhprintf_async(fh, "-Not enough room, don't send it (open failed: %d %s)", open_errno, strerror(open_errno));
                                    ex_putchar_async(fh, 0);
                                }
                            }
                            break;
                        }
                    }
                }else{
                    ex_fhprintf_async(fh, "-Invalid parameters. usage: STOR { NEW | OLD | APP } file-spec");
                    ex_putchar_async(fh, 0);
                }
            }else{
                ex_fhprintf_async(fh, "-No such command.");
                ex_putchar_async(fh, 0);
            }
        }
    }
shutdown:
    if(helper_fd!=-1) close(helper_fd);
    closedir(cwd);
    destroy_fh(fh);

}
