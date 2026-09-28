#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  
  if(argc >= 2){
    fprintf(2, "Usage: pingpong...\n");
    exit(1);
  }

  char fwrite[] = "ping";
  char fread[5];
  char zwrite[] = "pong";
  char zread[5];
  int f[2];
  int z[2];
  pipe(f);
  pipe(z);
  // 要先创建管道再fork,这样才能两个进程共享管道
  int pid = fork();
  if(pid == 0)
  {
    // 子进程关闭f[1]的写端口
    close(f[1]);
    read(f[0], zread, 5);
    // 子进程读完，关闭读端口
    close(f[0]);
    fprintf(1, "%d: received %s\n", getpid(), zread);
    // 关闭z[0]读端口
    close(z[0]);
    write(z[1], zwrite, 5);
    // 写完关闭z[1]的写端口
    close(z[1]);
    exit(0);
  }
  else
  {
    // 关闭f[0]读端口
    close(f[0]);
    write(f[1], fwrite, 5);
    close(f[1]);

    close(z[1]);
    read(z[0], fread, 5);
    close(z[0]);
    // 父进程等任意一个子进程结束，不关心它返回什么状态码，回收掉就行
    wait(0);
    // int status;
    // wait(&status);   // 等待子进程结束，并把退出码写入 status
    fprintf(1, "%d: received %s\n", getpid(), fread);
    exit(0);
  }
}