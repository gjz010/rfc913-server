#pragma once
// wrapper of epoll.
// when there are file descriptors in the epoll, notify the hooked file descriptor.
#include "file.h"
#include "sys/epoll.h"
#define EVENT_BUF_SIZE 128
typedef struct {
    int epoll_fd;
    int counter;
    struct epoll_event events[];
} poll_queue;

poll_queue* poll_init();
poll_queue* poll_destroy();

void register_file(poll_queue* q, file_handle* file);
void unregister_file(poll_queue* q, file_handle* file);

// calls epoll_wait once and fetch all events.
int poll_once(poll_queue* q);

extern poll_queue* global_queue;
