#include "poll.h"
#include "stdlib.h"
#include "sys/epoll.h"
#include "assert.h"
poll_queue* global_queue;
poll_queue* poll_init(){
    int epoll_fd=epoll_create1(0);
    assert(epoll_fd>=0);
    poll_queue* ret=malloc(sizeof(poll_queue)+sizeof(struct epoll_event)*EVENT_BUF_SIZE);
    ret->epoll_fd=epoll_fd;
    ret->counter=0;
    global_queue=ret;
    return ret;
}

int poll_once(poll_queue* q){
    if(q->counter==0) return 0;
    ssize_t nfds=epoll_wait(q->epoll_fd, q->events, EVENT_BUF_SIZE, -1);
    assert(nfds!=-1);
    if(nfds==0) return 0;
    else{
        for(int i=0; i<nfds; i++){
            file_handle* fh=q->events[i].data.ptr;
            if((q->events[i].events | EPOLLIN) || (q->events[i].events | EPOLLRDHUP)) {
                //printf("[poll] poll_in or poll_rdhup\n");
                notify_read(fh);
                if(!fh->destroyed){
                    if(q->events[i].events | EPOLLOUT) notify_write(fh);
                }else{
                    unregister_file(q, fh);
                    free(fh);
                }
            }else{
                if(q->events[i].events|EPOLLOUT) notify_write(fh);
                if(fh->destroyed){
                    unregister_file(q, fh);
                    free(fh);
                }
            }
            // Error handling not considered here: throw them to the coroutine.
        }
        
        return 1;
    }
}



void register_file(poll_queue* q, file_handle* file){
    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
    ev.data.ptr=file;
    q->counter++;
    assert(epoll_ctl(q->epoll_fd, EPOLL_CTL_ADD, file->fd, &ev)!=-1);
}

void unregister_file(poll_queue* q, file_handle* file){
    q->counter--;
    //if(q->counter==1) exit(0);
    epoll_ctl(q->epoll_fd, EPOLL_CTL_DEL, file->fd, 0);
}
