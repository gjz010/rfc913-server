#include "coroutine.h"
#include <assert.h>
#include <stdlib.h>
coroutine* current_coroutine;
coroutine* root_coroutine;
int context_switch(context* cfrom, context* cto);

void coroutine_entry(coroutine_func f, void* arg){
    f(arg);
    current_coroutine->state=1; //tag finished
    coroutine_yield(); //as a normal yield.
}
void coroutine_start(coroutine* c, coroutine_func entry, void* arg){
    c->caller=0;
    c->stack=malloc(STACK_SIZE);
    c->state=0;
    memset(&c->c_context, 0, sizeof(context));
    c->c_context.rsp=c->stack+STACK_SIZE;
    // return rip does not matter.
    c->c_context.rdi=entry;
    c->c_context.rsi=arg;
    //*(uint64_t)(c->c_context.rsp+8)=entry; //the entry.
    //*(uint64_t)(c->c_context.rsp+16)=arg; //the arg.
    c->c_context.rbp=c->stack+STACK_SIZE;
    c->c_context.rip=&coroutine_entry;
    // everything is ready now, and only requires a trigger.
    //printf("%d %d\n", c, c->stack);
}


void coroutine_yield(){
    assert(current_coroutine!=root_coroutine); // root coroutine should not be yielded.
    context* current_context=&current_coroutine->c_context;
    context* parent_context=&current_coroutine->caller->c_context;
    coroutine* parent=current_coroutine->caller;
    current_coroutine->caller=0;
    current_coroutine=parent;
    context_switch(current_context, parent_context);
}


int coroutine_await(coroutine* target){
    assert(target->caller==0);
    assert(target->state==0);
    context* current_context=&current_coroutine->c_context;
    target->caller=current_coroutine;
    current_coroutine=target;
    context* new_context=&current_coroutine->c_context;
    context_switch(current_context, new_context);
    // After some hard work we gain control again.
    // now we just free the coroutine.
    if(target->state==0){
        return 0;
    }else{
        coroutine_destroy(target);
        return 1;
    }
}

void coroutine_init(){
    // In fact nothing is done.
    // root_coroutine is the place to store registers.
    root_coroutine=(coroutine*)malloc(sizeof(coroutine));
    current_coroutine=root_coroutine;
}

void coroutine_destroy(coroutine* co){
    assert(co->state==1);
    //printf("%d %d\n", co, co->stack);
    free(co->stack);
    //printf("stack\n");
    free(co);
}
