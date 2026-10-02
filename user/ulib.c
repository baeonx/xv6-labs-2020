#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// 把t赋值给s, 
char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

// 看p, q除了前缀的公共部分后，第一个不同的字符相差了什么
int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

// 得到字符串s的长度
uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

// 在dst指向的空间中，填满长度为n的c数字
void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

// 看看c里面是否含有s里的切片
char*
strchr(const char *s, char c)
{
  for(; *s; s++) // 遍历，直到遇到 '\0'（*s == 0 时结束）
    if(*s == c)
      return (char*)s;
  return 0;
}

// 从终端读取max个字符到buf
char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

// ls里面用到了
// struct stat {
//   int dev;     // File system's disk device 文件系统所在的磁盘设备号
//   uint ino;    // Inode number inode 编号
//   short type;  // Type of file 文件类型（T_DIR/T_FILE/T_DEVICE）
//   short nlink; // Number of links to file 硬链接数
//   uint64 size; // Size of file in bytes 文件大小（字节）
// };
// stat(buf, &st) 
// buf是dir/a.txt, dir1/dir2/
int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;
  // 打开该buf文件路径的文件
  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

// 0到9的字符变成整个数字
// '9999'变成9999
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

// 将vsrc的数据拼接到vdst到
void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  
  if (src > dst) {
    // 如果src在dst后面
    //  将vsrc后n个的内容赋值给vdst后n个内容
    while(n-- > 0)
      *dst++ = *src++;
  } else {
    // 如果src在dst前面
    dst += n;
    src += n;
    while(n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

void *
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}
