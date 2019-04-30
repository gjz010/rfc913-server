#pragma once
// Let's base our entire work on this coroutine.
// Coroutine makes our life easier.
// For example, when dealing with protocols we no longer need to deal with state machines, but deal with fiber scheduling,

// A coroutine supports three options：
// 1. start: this starts a coroutine.
// 2. yield: throws out a value to outer coroutine.
// 3. await: transfers control to the coroutine. when control is regained,
//           a value is fetched from the coroutine.

// Allocate 64KB stack memory.
// This is much cheaper than fork or pthread or something.
#include "sys/types.h"
#include "stdint.h"
#include "setjmp.h"
#define STACK_SIZE 65536
// RAX register is always the scratch.
typedef struct {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} context;

typedef struct tag_coroutine{
    struct tag_coroutine* caller;
    context c_context;
    uint8_t* stack;
    int state;
    jmp_buf exception_env;
} coroutine;
#define coroutine_catch (setjmp(current_coroutine->exception_env))
#define coroutine_throw {do{longjmp(current_coroutine->exception_env, 1);}while(0);}
extern coroutine* current_coroutine;
extern coroutine* root_coroutine;

typedef void (*coroutine_func)(void*);

// Set the main process as the outer-most coroutine.
void coroutine_init();
void coroutine_start(coroutine* c, coroutine_func entry, void* arg);
void coroutine_yield();
int coroutine_await();
void coroutine_destroy(coroutine* co);
