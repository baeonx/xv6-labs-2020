// Simple grep.  Only supports ^ . * $ operators.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[1024];
int match(char*, char*);

void
grep(char *pattern, int fd)
{
  int n, m;
  char *p, *q;

  m = 0;
  // read(fd, buf+m, sizeof(buf)-m-1)
  // fd是文件描述符，buf是传入的位置指针，n是读取的最多字节数（一般指buf里面的剩余空间）
  while((n = read(fd, buf+m, sizeof(buf)-m-1)) > 0){
    m += n;
    buf[m] = '\0';
    p = buf;
    // strchr(p, '\n')
    // p里面有'\n'就返回该'\n'所在位置（指针表示）,否则返回0
    // 要进这个循环得是识别到text的多出一行的数据了，例如helllo world\n hello读取多于一行
    while((q = strchr(p, '\n')) != 0){
      // 将'\n'变成0
      // 为什么必须把 \n 变成 0
      // 因为 match 和它调用的 matchhere 是按 C 字符串工作的，靠 '\0' 判断字符串结束。
      // 如果不改 *q，那么 p 指向的字符串是：
      // text
      // "hello world\nfoo bar\n"
      // '\n'变成0， q的字符串变成hello world\0结束
      *q = 0;
      if(match(pattern, p)){
        *q = '\n'; // 匹配成功，恢复 '\n'
        // 往fd=1, 写入hello world\n
        write(1, p, q+1 - p);
      }
      p = q+1;// 跳过\n到下一行
    }

    // 去掉扫过的上一行并移动buf
    // 
    // hello world\nello
    // 把ello 移动到 hello world\n变成
    // elloo world\n 
    if(m > 0){
      m -= p - buf;
      memmove(buf, p, m);
    }
  }
}

// 
int
main(int argc, char *argv[])
{
  int fd, i;
  char *pattern;

  // 如果命令行参数的总个数小于等于1， fprintf(2), 2是错误打印出:"usage: grep pattern [file ...]\n"
  // exit(1) 表示以非 0 状态码退出，通常代表程序出错/异常结束（0 表示成功）。
  //举例：
  // grep参数不够，报错退出。
  if(argc <= 1){
    fprintf(2, "usage: grep pattern [file ...]\n");
    exit(1);
  }

  // argv[1]个参数是pattern
  // 举例：
  // grep hello，在标准输入里搜索 "hello"。
  // argv[1]就是hello
  pattern = argv[1];

  // 如果如果命令行参数的总个数小于等于2说明pattern只有一个参数
  // 举例：
  // grep hello file.txt，在 file.txt 里搜索 "hello"。
  // 上述的参数有三个就不能直接执行
  if(argc <= 2){
    // 传入pattern, fd
    // fd = 0说明是终端输入
    grep(pattern, 0);
    exit(0);
  }

  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf("grep: cannot open %s\n", argv[i]);
      exit(1);
    }
    grep(pattern, fd);
    close(fd);
  }
  exit(0);
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  // 如果是 re='^hello', text=hello world
  if(re[0] == '^')
    // re+1变成 hello，text匹配
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  // hello, text
  // *text!='\0'说明text不为空
  // re[0]==*text 如果re的首部等于text的首部就递归
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

