rfc913-server
============
C implementation of [RFC913](https://tools.ietf.org/html/rfc913) server, with a simple C++ client.


Build
========
```
make all
```


Usage
========

```
$ ./file_server --help # for help
usage: ./file_server [--host bind-host=127.0.0.1] [--port bind-port=40021]
         [--workdir working_directory=./] [--chroot] [--daemon]
--host bind-host         Set the listening host. default to 127.0.0.1
--port bind-port         Set the listening port. default to 40021
--workdir working_directory      Set the working directory for users. default to cwd
--chroot         Use chroot and nobody group to protect your server. Requires root.
--daemon         Run in background.
```

for example

```
# ./file_server --host 127.0.0.1 --port 115 --workdir ./data --chroot
```

Client usage
========

Simple wrapper for RFC913 client.

- Automatically add NULL to your command, so that you can work as if you were using telnet.
- When calling RETR or STOR, it allows you to choose a filename.

Example
========
```
$ ./client 127.0.0.1 40021
+MIT-XX SFTP Service made by gjz010 with love!
sftp> USER gjz010
+gjz010 ok, send password
sftp> PASS sftpserver
! gjz010 logged in. Have fun...
sftp> STOR NEW file_server
+File does not exist, will create new file
Please input file name: ./file_server
SIZE 34248
+ok, waiting for file
+Saved file_server
sftp> list V ./
+./:
File name       Type    Size(Bytes)
.       Directory       4096
..      Directory       4096
file_server     RegularFile     34248

sftp> RETR file_server
 34248
Please input file name: ./my_file_server
SEND
sftp> DONE
+MIT-XX closing connection. Goodbye and have a nice dream!
```
