#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  // dir1/a.txt 提取a.txt,p最终到 '/'
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  // p移动到a.txt的a
  p++;

  // Return blank-padded name.
  // 如果这个名字太大直接返回
  if(strlen(p) >= DIRSIZ)
    return p;
  // 拼接，将p这个指针后面的东西拼接到buf后面，类似于提取到buf
  // a.txt提取到buf
  memmove(buf, p, strlen(p));
  // 在buf+strlen(p)指向的是a.txt后一位
  // 填满空格，使得满足buf里面的长度是DIRSIZ
  // 返回buf
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  // 把这个fd所在的文件存到st里面
  // struct stat {
  //   int dev;     // File system's disk device 文件系统所在的磁盘设备号
  //   uint ino;    // Inode number inode 编号
  //   short type;  // Type of file 文件类型（T_DIR/T_FILE/T_DEVICE）
  //   short nlink; // Number of links to file 硬链接数
  //   uint64 size; // Size of file in bytes 文件大小（字节）
  // };
  // 读取文件的元信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  // 如果是文件直接走到这里
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;
  // 如果是目录就走到这
  // strlen(path)	当前路径长度	比如 "dir" 是 3
  // 1	加一个 /	拼接时要加的分隔符
  // DIRSIZ	加文件名的最大长度	DIRSIZ = 14，名字最多 14 字符
  // 1	加字符串结束符 \0	C 字符串必须以 \0 结尾
  // sizeof buf	超过缓冲区大小	buf 装不下就报错
  case T_DIR:
    // 文件地址 当前目录path + '/' + 文件名长度 + '\0' > size of buf
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // fd是要打开的文件的描述符，将文件读取到内存放到de当中
    // de 会被下一次 read 覆盖
    // read读取文件里的内容
    // 文件夹的内容是当中是一串连续的 struct dirent
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // inode节点
      if(de.inum == 0)
        continue;
      // 把de.name的名字嫁接到p后面，形成dir/
      // memmove(void *vdst, const void *vsrc, int n)的n长度可能超过vsrc出现越界读，需人为设定
      // de.name本身就DIRSIZ长度
      memmove(p, de.name, DIRSIZ);
      // 最后赋值个0
      p[DIRSIZ] = 0;
      // 读取该文件的元信息到st
      // 看看这个文件是不是真的可读
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;
  // 只有ls的情况,argc == 1,直接not open
  if(argc < 2){
    ls(".");
    exit(0);
  }
  // ls * 展开成 ls a.txt b.txt c.txt
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
