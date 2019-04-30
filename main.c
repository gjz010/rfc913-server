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

void disable_sigpipe(){
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
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


int main(){
    coroutine_init(); //initialize root coroutine
    disable_sigpipe(); //disable sigpipe
    printf("server started\n");
    poll_queue* eventqueue=poll_init();
    listen_server_options cmd_server={
        .event_queue=eventqueue,
        .listen_addr="127.0.0.1",
        .listen_port=40021,
        .client_handler=&sftp_conn
    };
    coroutine* cmd_server_co=start_listen_server(&cmd_server);
    while(poll_once(eventqueue));
    return 0;
}

