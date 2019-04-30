.text
.globl context_switch
context_switch:
        #movq 8(%rsp), %rax;
        popq 0(%rdi);
        movq %rsp, 8(%rdi);
        movq %rbx, 16(%rdi);
        movq %rcx, 24(%rdi);
        movq %rdx, 32(%rdi);
        movq %rsi, 40(%rdi);
        movq %rdi, 48(%rdi);
        movq %rbp, 56(%rdi);
        movq %r8, 64(%rdi);
        movq %r9, 72(%rdi);
        movq %r10, 80(%rdi);
        movq %r11, 88(%rdi);
        movq %r12, 96(%rdi);
        movq %r13, 104(%rdi);
        movq %r14, 112(%rdi);
        movq %r15, 120(%rdi);
        #movq 8(%rsp), %rax;
        movq 8(%rsi), %rsp;
        movq 16(%rsi), %rbx;
        movq 24(%rsi), %rcx;
        movq 32(%rsi), %rdx;
        movq 48(%rsi), %rdi;
        movq 56(%rsi), %rbp;
        movq 64(%rsi), %r8;
        movq 72(%rsi), %r9;
        movq 80(%rsi), %r10;
        movq 88(%rsi), %r11;
        movq 96(%rsi), %r12;
        movq 104(%rsi), %r13;
        movq 112(%rsi), %r14;
        movq 120(%rsi), %r15;
        pushq 0(%rsi);
        movq 40(%rsi), %rsi;
        ret;
