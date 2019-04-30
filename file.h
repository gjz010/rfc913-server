#pragma once
// Coroutine wrapper for a file descriptor.
// Allows coroutines to yield(wait) on a given file descriptor, either read or write.
// In this way we can write asynchronous program in the synchronous way.
#include "coroutine.h"
#include "sys/socket.h"
typedef struct tag_waiting_list{
    coroutine* value;
    struct tag_waiting_list* next;
} waiting_list;

typedef struct {
    int fd;
    int can_read;
    int can_write;
    int rdhup;
    int wrhup;
    int eof;
    int error;
    int destroyed;
// We use list of size 1.
// One element is enough, since a file is only controlled by one coroutine.
    coroutine* reader_list;
    coroutine* writer_list;
// buffer.
    int buffer_size;
    int write_pos;
    int read_pos;
    uint8_t buffer[0];
} file_handle;

file_handle* wrap_fd(int fd);
void destroy_fh(file_handle* fh);

// Asynchronized syscalls.
// They behave just like syscalls: they do read/write, they raise error, and so on.
// The only thing is that they are non-blocking for process/thread, and blocking for coroutine.
ssize_t read_async(file_handle* fh, void* buf, size_t count);
ssize_t write_async(file_handle* fh, const void* buf, size_t count);
ssize_t accept_async(file_handle* fh, struct sockaddr* addr, socklen_t* size);
void sendfile_async(file_handle* out_fh, file_handle* in_fh, off_t* offset, size_t count);
size_t fread_async(file_handle* fh, void* buf, size_t count);
size_t fwrite_async(file_handle* fh, const void* buf, size_t count);
int fhprintf_async(file_handle* fh, const char* format, ...);
int getchar_async(file_handle* fh);

// Exception-support version.
// When unable to finish the operation (e.g. reaching EOF/EPIPE), these functions will throw an exception (coroutine_throw).
// Before you use those functions you have to coroutine_catch(setjmp) first.
void ex_fread_async(file_handle* fh, void* buf, size_t count);
void ex_fwrite_async(file_handle* fh, const void* buf, size_t count);
void ex_fhprintf_async(file_handle* fh, const char* format, ...);
void ex_sendfile_async(file_handle* fh, int in_fd, size_t count);
char ex_getchar_async(file_handle* fh);
void sleep_async(long milliseconds);

// disable nagle's algorithm, for interactive purposes.
void disable_nagle(file_handle* fh);
// enable nagle's algorithm for data transfer purposes.
void enable_nagle(file_handle* fh);


// Wait & Notify..
void wait_read(file_handle* fh);
void wait_write(file_handle* fh);
void notify_read(file_handle* fh);
void notify_write(file_handle* fh);

