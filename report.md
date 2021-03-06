RFC 913 File transfer server
========

概述
--------

本程序基本实现了[RFC913 Simple File Transfer Protocol](https://tools.ietf.org/html/rfc913)，实现了一个简单的文件传输程序。
编译、运行、样例见README.md。

设计选型
--------

### 基于epoll的事件驱动

为了提高开发效率和运行效率，本程序的总体设计采用了基于epoll的单进程单线程的模式。
全局维护一个基于epoll实现的事件队列。
所有的socket文件描述符（以及之后用到的timerfd）都被设置为O_NONBLOCK的，只有当和它们关联的事件发生，它们才会被读取或写入。
这使得我们可以采用基于事件驱动/异步的方式编写服务器程序。

### 带简单异常处理的用户态协程

一种开发事件驱动的方式是编写复杂的网络协议处理状态机，当事件发生时，状态机被向前推进若干步。然而这样的状态机的维护复杂度会大大提升。
为了有利于逻辑的编写，我们使用了另外一种方式：协程（亦可以是Fiber）。

协程的基本思想来源于线程的上下文切换（对应于C中的setjmp/longjmp对）。使用与进程调度相似的思想，我们可以在协程需要被阻塞时将其挂起（yield），而当阻塞因素消失时再将其继续执行（next）。
假如我们允许协程在一个非阻塞文件描述符上等待，我们就可以在协程中处理文件描述符。

协程支持如下几种操作。

- start：启动一个协程。在这里，我们指定协程启动的入口和参数。
- next：切换到一个协程中执行，直到该协程执行完毕或者放弃执行。
- yield：一个协程可以主动放弃执行权，并回到上次调用next的地方。

每个协程执行需要自己的context，包括寄存器状态和栈状态。
我们这里给每个协程提供一个64KB的栈（实际上具体到文件服务器而言，处理一个客户端需要的栈大小远远小于64KB。可能的优化方式是通过编译器的静态分析，使得协程在没有递归的情况下不需要栈运行），
这个开销远远小于进程和线程的内存开销。
同时，由于协程之间的上下文切换不需要经过内核，协程的上下文切换比起进程和线程亦有效率上的优势。

将协程与之前状态机模式对比的话，不难看出，状态机模型中的“状态”对应的恰巧就是协程的整个context。
然而，虽然协程模式模式比起状态机模式可能有一定的效率损失和内存浪费，但是带来了巨大的开发便利。
例如，假如使用状态机处理网络协议，我们可能需要显式构造状态机和所有的状态转移。
而如果利用协程的话，协程的执行过程就是协议处理过程本身，一些状态被隐藏在了处理代码当中。

为了用协程处理网络协议，首先我们对文件描述符进行封装，加入了wait和notify操作：一个协程可以在一个文件描述符上wait（把自己加到文件描述符的等待队列里后yield），然后由epoll来notify这个文件描述符（执行next操作）。

我们将read和write进行包装，使得它们能够在收到EAGAIN时，能够自动在文件描述符上wait。
随后，我们仿照实现了fread_async和fwrite_async，用来尽可能地读满buffer（同时提供缓冲区以应对interactive的情况）和尽可能地写入整个buffer。

```
main(root coroutine)                 listening socket coroutine
        +                                      +
        |                                      .
        |                                      .
        |epoll_wait()                          .
        |             coroutine_await()        .
        +------------------------------------->+                socket handler coroutine
        .            listenfd EPOLLIN          |accept()
        .                                      +-------------------------+
        .                                      .     coroutine_start()   v
        .                                      .                         |
        .                                      .                         | read_async()
        .                                      +<------------------------+
        +<-------------------------------------+      coroutine_yield()  .
        |epoll_wait()    coroutine_yield()     |                         .
        .                                      .                         .
        .(Blocking)                            .                         .
        .                                      .                         .
        .                                                                .
        |                        sockfd EPOLLIN                          .
        +--------------------------------------------------------------->+
        .                      coroutine_await()                         |read_async() return
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |write_async()
        .                      coroutine_yield()                         |
        +<---------------------------------------------------------------+
        |epoll_wait()                                                    .
        .(Blocking)                                                      .
        |                      sockfd EPOLLOUT                           .
        +--------------------------------------------------------------->+
        .                      coroutine_await()                         |write_async() return
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                |
        .                                                                +

```



在我们处理socket时，我们可能会希望，当一些read/write操作不能成功完成（read得到预料之外的eof，write返回EPIPE），或者其它的一些条件不能得到满足时，不再试图恢复，而是直接释放所有资源，关闭整个连接。
因此，我们同时加入了异常处理机制。协程可以通过coroutine_catch（setjmp）设置异常处理程序，然后通过coroutine_throw（longjmp）来抛出异常。
通过这种方式，我们获得了ex打头的一系列“可能抛出异常的读写函数”。
这进一步简化了我们的代码编写：大多数情况下当socket断开时，我们没有什么挽回的方法；而socket不断开，我们就可以不需要考虑异常处理。

借助于对协程的支持，我们以一点内存开销和（用户态）上下文切换的代价，使得我们能够以近于线性、阻塞的代码风格编写异步、事件驱动的网络协议处理程序。
在非CPU-intense的场合，这种交换是值得的。

### 基于单个TCP连接的传输

不同于FTP，RFC913是一种基于单个TCP连接的文件传输协议。而交互式的数据传输和大块文件的传输有时需要不同的策略。
一个最典型的例子是，大块文件的传输需要开启Nagle算法以减少小数据包的数量，而交互式命令的传输则需要关闭Nagle算法。
我们的解决方案是，默认情况下关闭Nagle算法，而当需要文件传输时再打开它。


协议实现
--------

RFC913是一种简单的文件传输协议。它使用ASCII作为控制流，并且在指定长度的情况下直接二进制传输。

### 协议简介

RFC913的命令全部由四个字母构成，由NULL截止。

- USER user-id：指定登录用的用户名。
- ACCT account：指定登录用的账户（常用于计费用）。本程序没有实现这个命令，而是仅使用用户名和密码登录。
- PASS password：指定登录用的密码。
- TYPE { A | B | C }：指定文件传输的方式。本程序没有实现这个命令，而是强制使用字节为单位的传输（B模式）。
- LIST { F | V } directory-path：列出目录下的文件。
- CDIR new-directory：切换当前工作路径。本程序没有实现分目录鉴权。
- KILL file-spec：删除指定的文件。
- NAME old-file-spec：重命名指定的文件。当成功时，需要使用二级命令TOBE new-file-spec来指定新文件名。由于rename syscall的限制，该命令不能跨文件系统搬运文件。
- DONE：退出系统。
- RETR file-spec：从远端接收一个文件，获得文件的大小。当成功时，使用二级命令SEND来开始传输，或者使用STOP拒绝传输。
- STOR { NEW | OLD | APP } file-spec：向远端发送一个文件。使用时，先通过SIZE告知服务器文件大小，再开始传输。本程序没有实现文件版本机制，NEW子命令在文件存在时会直接失败。

RFC913的全部消息（S->C）由种类标识符打头，由NULL截止。
- "+"表示执行成功。
- "-"表示执行失败。
- "!"表示登录成功。在PASS命令时以此表示成功。
- " "表示远端接收到RETR命令后发送的文件大小。

### 协议实现细节

在RFC913的实现中，我们采取了一些措施来保证协议的安全性和效率。

- 认证机制：PASS命令会等待2-3秒钟，以模拟防止暴力破解和基于认证时长的密码破解。
- 超过1023字符长的命令会直接被丢掉，返回一条错误信息。
- 关闭Linger，避免大量TIME_WAIT连接。
- 通过缩短TCP Keepalive时间来尽早清除已经断开的发呆用户。
- 在数据传输开始时开启Nagle算法，数据传输结束后再关闭Nagle算法。
- 发送文件时使用Sendfile，避免用户态与内核态之间的拷贝。
- 提供chroot选项，将服务器锁在指定路径下并且切换到低权限，即使是服务器进程被攻破，入侵者也不能访问到指定路径之外。
