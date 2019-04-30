#pragma once
// coroutine for creating a listen server.
// the server will dispatch the server to the given functions.
#include "poll.h"
#include "coroutine.h"
typedef struct{
    poll_queue* event_queue;
    const char* listen_addr;
    int listen_port;
    coroutine_func client_handler;
} listen_server_options;


void listen_server_entry(void* listen_socket);

coroutine* start_listen_server(listen_server_options* options);
