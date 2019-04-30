#define _POSIX_C_SOURCE 199309L
#define SOL_TCP 6
#include "netinet/tcp.h"
#include "file.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "assert.h"
#include "sys/socket.h"
#include "stdio.h"
#include "stdarg.h"
#include "coroutine.h"
#include "time.h"
#include "sys/timerfd.h"
#include "poll.h"
#include "sys/sendfile.h"
#include <fcntl.h>
#include <arpa/inet.h>
#define E_WOULDBLOCK EAGAIN
void wait_read(file_handle* fh){
    fh->can_read=0;
    fh->reader_list=current_coroutine;
    coroutine_yield();
}

void wait_write(file_handle* fh){
    fh->can_write=0;
    fh->writer_list=current_coroutine;
    coroutine_yield();
}

void notify_read(file_handle* fh){
    //printf("%d can read now\n", fh);
    fh->can_read=1;
    if(fh->reader_list){
        coroutine* reader=fh->reader_list;
        fh->reader_list=0;
        coroutine_await(reader);
    }
}

void notify_write(file_handle* fh){
    //printf("%d can write now\n", fh);
    fh->can_write=1;
    if(fh->writer_list){
        coroutine* writer=fh->writer_list;
        fh->writer_list=0;
        coroutine_await(writer);
    }
}

// problem: regular file does not benefit from non-blocking IO.
void set_nonblocking(int fd){
    int flags;
    flags=fcntl(fd, F_GETFL, 0);
    if(flags==-1) flags=0;
    assert(fcntl(fd, F_SETFL, flags|O_NONBLOCK)==0);
}
#define BUFFER_SIZE (1024)
file_handle* wrap_fd(int fd){
    set_nonblocking(fd);
    file_handle* fh=malloc(sizeof(file_handle)+BUFFER_SIZE);
    fh->fd=fd;
    fh->can_read=1;
    fh->can_write=1;
    fh->rdhup=0;
    fh->wrhup=0;
    fh->eof=0;
    fh->buffer_size=BUFFER_SIZE;
    fh->write_pos=0;
    fh->read_pos=0;
    fh->error=0;
    fh->destroyed=0;
    return fh;
}

void destroy_fh(file_handle* fh){
    close(fh->fd);
    fh->destroyed=1;
}

ssize_t read_async(file_handle* fh, void* buf, size_t count){
    //printf("read_async started\n");
    //if(fh->rdhup) {errno=EIO;return -1;}
    ssize_t ret;
    while((ret=read(fh->fd, buf, count))<0 && errno==E_WOULDBLOCK){
        // would block, then block.
        wait_read(fh);
        //if(fh->rdhup) {errno=EIO;return -1;}
    }
    //printf("[read_async] %d %d\n", ret, errno);
    return ret;
}

int getchar_async(file_handle* fh){
    char buffer;
    int ret=read_async(fh, &buffer, 1);
    if(ret<=0) return -1;
    else return buffer;
}

ssize_t write_async(file_handle* fh, const void* buf, size_t count){
    //printf("write_async started\n");
    //if(fh->wrhup) {errno=EPIPE;return -1;}
    ssize_t ret;
    while((ret=write(fh->fd, buf, count))<=0 && errno==E_WOULDBLOCK){
        //printf("err: %d %d\n", ret, errno);
        wait_write(fh);
        //if(fh->wrhup) {errno=EPIPE; return -1;}
    }
    //printf("[write_async] %d %d\n", ret, errno);
    return ret;
}

ssize_t accept_async(file_handle* fh, struct sockaddr* addr, socklen_t* size){
    ssize_t ret;
    while((ret=accept(fh->fd, addr, size))==-1 && errno==E_WOULDBLOCK){
        wait_read(fh);
    }
    return ret;
}
size_t fwrite_async(file_handle* fh, const void* buf, size_t count){
    int counter=0;
    int ret;
    while(counter<count){
        if((ret=write_async(fh, ((char*)buf)+counter, count-counter))!=-1){
            counter+=ret;
        }else{
            fh->wrhup=1;
            return counter;
        }
    }
    //printf("%d %d\n", counter, count);
    return counter;
}
// Buffered version.
size_t fread_async(file_handle* fh, void* buf, size_t count){
    //printf("[fread_async] %d\n", count);
    if(fh->error) return 0;
    if(fh->eof) return 0;
    size_t read_count=0;
    // step 1: try to fill buf with buffer.
    int content_size=fh->write_pos-fh->read_pos;
    if(count<=content_size){
        memcpy(buf, fh->buffer+fh->read_pos, count);
        fh->read_pos+=count;
        if(fh->write_pos==fh->read_pos) fh->write_pos=fh->read_pos=0;
        return count;
    }else{
        memcpy(buf, fh->buffer+fh->read_pos, content_size);
        read_count+=content_size;
        fh->write_pos=fh->read_pos=0;
    }
    // Now fh->write_pos=fh->read_pos=0.
    int remaining=count-read_count;
    int entire_buffers=remaining/fh->buffer_size;
    int read_through_bytes=entire_buffers*fh->buffer_size;
    // for entire-buffer case: read through and save a copy.
    while(read_through_bytes>0){
        ssize_t ret=read_async(fh, ((size_t)buf)+read_count, read_through_bytes);
        if(ret>0){
            read_through_bytes-=ret;
            read_count+=ret;
        }else{
            // Something fishy is going on.
            if(ret==0){
                fh->eof=1;
                return read_count;
            }
            switch(errno){
                case EIO:{
                    fh->error=1;
                    return read_count;
                }
                default:{
                    return read_count;
                }
            }
        }
    }
    size_t residue_start=((size_t)buf)+read_count;
    int residue=remaining-read_through_bytes;
//    printf("residue: %d %d\n", residue, read_count);
    if(residue>0){
        int received_bytes=0;
        while(received_bytes<residue){
            ssize_t ret=read_async(fh, fh->buffer, fh->buffer_size);
//            printf("ret: %d\n", ret);
            if(ret>0){
                if(received_bytes+ret<=residue){
                    //copy everything in buffer to target.
                    memcpy(residue_start+received_bytes, fh->buffer, ret);
                    received_bytes+=ret;
                    read_count+=ret;
                }else{
                    int used_part=residue-received_bytes;
                    memcpy(residue_start+received_bytes, fh->buffer, used_part);
                    received_bytes+=used_part;
                    read_count+=used_part;
                    fh->read_pos=used_part;
                    fh->write_pos=ret;
                }
            }else{
                if(ret==0){ // this means that you still want more bytes, but no more for you.
                    fh->eof=1;
                    fh->read_pos=fh->write_pos=0;
                    return read_count;
                }
                switch(errno){
                    case EIO:{ // err: last read.
                        fh->error=1;
                        fh->read_pos=fh->write_pos=0;
                        return read_count;
                    }
                    default:{ // some other reason: maintain state.
                        fh->read_pos=fh->write_pos=0;
                        return read_count;
                    }
                }
            }
        }
    }
    return read_count;
}


int fhprintf_async(file_handle* fh, const char* format, ...){
    char buffer[1024];
    int ret;
    va_list args;
    va_start(args, format);
    ret=vsnprintf(buffer, 1024, format, args);
    va_end(args);
    if(ret<0 || ret>=1024) return 0;
    return fwrite_async(fh, buffer, ret);
}

void ex_fread_async(file_handle* fh, void* buf, size_t count){
    size_t read_size=fread_async(fh, buf, count);
    //printf("%d\n", read_size);
    if(read_size!=count) coroutine_throw;
}

void ex_fwrite_async(file_handle* fh, const void* buf, size_t count){
    size_t write_size=fwrite_async(fh, buf, count);
    if(write_size!=count) coroutine_throw;
}

void ex_fhprintf_async(file_handle* fh, const char* format, ...){
    char buffer[1024];
    int ret;
    va_list args;
    va_start(args, format);
    ret=vsnprintf(buffer, 1024, format, args);
    va_end(args);
    if(ret<0 || ret>=1024) coroutine_throw;
    ex_fwrite_async(fh, buffer, ret);
}


char ex_getchar_async(file_handle* fh){
    char buf;
    ex_fread_async(fh, &buf, 1);
    return buf;
}

// Creates a timerfd, read_async the timerfd and come back.
void sleep_async(long milliseconds){
    int timer_fd=timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK); //non-blocking timer.
    struct itimerspec timer_spec;
    memset(&timer_spec, 0, sizeof(struct itimerspec));
    timer_spec.it_value.tv_sec=milliseconds/1000;
    timer_spec.it_value.tv_nsec=((long)(milliseconds%1000))*1000*1000;
    timerfd_settime(timer_fd, 0, &timer_spec, 0);
    file_handle* timer_fh=wrap_fd(timer_fd);
    register_file(global_queue, timer_fh); //This requires that we have one and only one event queue.
    char buf[64];
    read_async(timer_fh, buf, 64); //sleep here.
    // when the coroutine finally wakes up, destroy the timer_fd.
    destroy_fh(timer_fh);
}

// disable nagle's algorithm, for interactive purposes.
void disable_nagle(file_handle* fh){
    int one=1;
    setsockopt(fh->fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));

}
// enable nagle's algorithm for data transfer purposes.
// disable tcp_nodelay.
void enable_nagle(file_handle* fh){
    int zero=0;
    setsockopt(fh->fd, SOL_TCP, TCP_NODELAY, &zero, sizeof(zero));
}

void ex_sendfile_async(file_handle* fh, int in_fd, size_t count){
    while(count>0){
        int ret;
        while((ret=sendfile(fh->fd, in_fd, NULL, count))<=0 && errno==E_WOULDBLOCK){
            wait_write(fh);
        }
        if(ret<=0){
            coroutine_throw; // something is wrong.
        }else{
            count-=ret;
        }
    }
}
