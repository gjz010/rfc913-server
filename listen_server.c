#include "listen_server.h"
#include "coroutine.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "sys/types.h"
#include <netinet/in.h>
#include "errno.h"
#include "assert.h"
#include "unistd.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
typedef struct {
    poll_queue* event_queue;
    file_handle* listen_fh;
    coroutine_func client_handler;
} listen_server_arguments;
void listen_server_entry(void* listen_socket){
    printf("some listen server started.\n");
    listen_server_arguments* args=listen_socket;
    poll_queue* event_queue=args->event_queue;
    file_handle* listen_fh=args->listen_fh;
    coroutine_func client_handler=args->client_handler;
    free(args); // Free the argument.
    socklen_t client_len;
    struct sockaddr_in client_addr;
    int ret;
    for(;;){
        while((ret=accept_async(listen_fh, &client_addr, &client_len))==-1 && errno==EAGAIN);
        if(ret>=0){
            printf("accept\n");
            // Start a client coroutine here.
            file_handle* fh=wrap_fd(ret);
            //int optval=1;
            //struct timeval tv;
            //memset(&tv, 0, sizeof(tv));
            //tv.tv_sec=0;
            //assert()
            register_file(event_queue, fh);
            coroutine* client_co=malloc(sizeof(coroutine));
            coroutine_start(client_co, client_handler, fh);
            coroutine_await(client_co);
            // When back, the client should be hanging on some file descriptor.
            if(fh->destroyed){
                unregister_file(event_queue, fh);
                free(fh);
            }
        }else{
            if(errno==EPROTO || errno==ECONNABORTED) continue; //abort this connection and goto next.
            else{
                assert(0);
            }
        }
    }
    destroy_fh(listen_fh);
}
coroutine* start_listen_server(listen_server_options* options){
    printf("starting server on %s:%d\n", options->listen_addr, options->listen_port);
    int listenfd;
    listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd==-1) goto onerror;
    int optval=1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int))<0) goto cleanup_socket;
    optval=1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int))<0) goto cleanup_socket;
    optval=1;
    if(setsockopt(listenfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(int))<0){
        printf("warning: TCP_DEFER_ACCEPT failed. this may happen when you are using WSL.\n");
    }
    struct sockaddr_in listenaddr;
    listenaddr.sin_family=AF_INET;
    listenaddr.sin_addr.s_addr=inet_addr(options->listen_addr);
    listenaddr.sin_port=htons(options->listen_port);
    if(bind(listenfd, (struct sockaddr*)&listenaddr, sizeof(listenaddr) )<0) goto cleanup_socket;
    if(listen(listenfd, 128)<0) goto cleanup_socket;
    file_handle* listen_fh=wrap_fd(listenfd);
    register_file(options->event_queue, listen_fh);
    coroutine* lc=malloc(sizeof(coroutine));
    listen_server_arguments* args=malloc(sizeof(*args));
    listen_server_arguments tmp={
        .event_queue=options->event_queue,
        .listen_fh=listen_fh,
        .client_handler=options->client_handler
    };
    *args=tmp;
    coroutine_start(lc, &listen_server_entry, args);
    coroutine_await(lc);
    return lc;
cleanup_socket:
    perror("Something is wrong");
    close(listenfd);
onerror:
    return 0;
}


