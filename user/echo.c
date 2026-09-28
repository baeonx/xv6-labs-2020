#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 输入: (3, char a[3] = "hello world") a[0] = "hello", a[1] = " ", a[2] = "world"
// 控制台打印输出 "hello world\n"
int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
}
