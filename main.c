#include "signal.h"
#include "assert.h"
#include "sys/socket.h"
#include "sys/types.h"
#include <stdio.h>
#include "errno.h"
#include "poll.h"
#include "file.h"
#include "coroutine.h"
#include "listen_server.h"
#include "string.h"
#include "getopt.h"
void disable_sigpipe(){
    signal(SIGPIPE, SIG_IGN);
    //signal(SIGHUP, SIG_IGN);
}
void sftp_conn(void* pfh);

void client(void* pfh){
    file_handle* fh=pfh;
    //printf("client: %d\n", pfh);
    struct linger lg;
    lg.l_onoff=1;
    lg.l_linger=0;
    if(setsockopt(fh->fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg))==-1){
        shutdown(fh->fd, SHUT_RDWR);
        goto shutdown;
    }
    char* buf=malloc(1024);
    int ret;
    while((ret=read_async(fh, buf, 1024))>0){
        //printf("write %d\n", ret);
        //sleep(5);
        fwrite_async(fh, buf, ret);
        if(fh->wrhup){
            //printf("write pipe broken. stop writing...\n");
            break;
        };
    }
shutdown:
    //printf("closedown %d\n", errno);
    //shutdown(fh->fd, SHUT_RDWR);
    free(buf);
    destroy_fh(pfh);
}


int main(int argc, char** argv){
    char* host="127.0.0.1";
    int port=40021;
    int need_chroot=0;
    int need_daemon=0;
    while(1){
        int current_option=optind?optind:1;
        int option_index=0;
        static struct option long_options[]={
            {"host", required_argument, 0, 0},
            {"port", required_argument, 0, 0},
            {"workdir", required_argument, 0, 0},
            {"chroot", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"daemon", no_argument, 0, 0}
        };
        int c=getopt_long(argc, argv, "", long_options, &option_index);
        if(c==-1) break;
        switch(option_index){
            case 0: host=optarg;break;
            case 1: port=atoi(optarg);break;
            case 2: if(chdir(optarg)!=0){printf("chdir to %s failed!\n", optarg);return 1;}break;
            case 3: need_chroot=1;break;
            case 4: {
                printf("usage: %s [--host bind-host=%s] [--port bind-port=%d] \n\t [--workdir working_directory=./] [--chroot] [--daemon]\n", argv[0], host, port);
                printf("--host bind-host \t Set the listening host. default to %s\n", host);
                printf("--port bind-port \t Set the listening port. default to %d\n", port);
                printf("--workdir working_directory \t Set the working directory for users. default to cwd\n");
                printf("--chroot \t Use chroot and nobody group to protect your server. Requires root.\n");
                printf("--daemon \t Run in background.\n");
                return 1;
            }
            case 5: need_daemon=1;break;
        }
        
    }
    if(need_chroot){
        if(chroot("./")!=0){
            printf("chroot() to working path failed! Maybe you need some root...\n");
            return 2;
        }
        setuid(65534);
        setgid(65534);
    }
    if(need_daemon){
        if(fork()!=0) return 0;
        else{
            close(1);
            close(2);
        }
    }
    coroutine_init(); //initialize root coroutine
    disable_sigpipe(); //disable sigpipe
    printf("server started\n");
    poll_queue* eventqueue=poll_init();
    listen_server_options cmd_server={
        .event_queue=eventqueue,
        .listen_addr=host,
        .listen_port=port,
        .client_handler=&sftp_conn
    };
    coroutine* cmd_server_co=start_listen_server(&cmd_server);
    while(poll_once(eventqueue));
    return 0;
}

