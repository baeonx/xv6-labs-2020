#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  
  static char buf[DIRSIZ+1];
  char *p;
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
  buf[strlen(p)] = '\0';
  return buf;
}

void find(char* path, char* file)
{
    
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
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
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    
    if(st.type == T_DIR){
        // 文件地址 当前目录path + '/' + 文件名长度 + '\0' > size of buf
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            exit(1);
        }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';
        // fd是要打开的文件的描述符，将文件读取到内存放到de当中
        // de 会被下一次 read 覆盖
        // read读取fd文件里的内容(区别于fstat()读取文件的元信息)
        // 文件夹当中是一串连续的 struct dirent
        struct stat st0;
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
        // inode节点
        // 目录里有些槽位是空的（文件被删了），inum == 0 
        // 跳过空目录项
        if(de.inum == 0)
            continue;
        char *m = ".";
        char *f = "..";
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        // 跳过自己目录和父目录
        if(strcmp(de.name, m) == 0 || strcmp(de.name, f) == 0) continue;
        // 读取该文件的元信息到st0
        // 看看这个文件是不是真的可读
        if(stat(buf, &st0) < 0){
            printf("ls: cannot stat %s\n", buf);
            continue;
        }
        if(st0.type == T_FILE)
        {
            if(strcmp(fmtname(buf), file) == 0)
                printf("%s\n", buf);
        }
        else{
            find(buf, file);
        }
        }
    }
    close(fd);
    return;
}


int
main(int argc, char *argv[])
{
  // 只有ls的情况,argc == 1,直接not open
  if(argc != 3){
    fprintf(2, "usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}