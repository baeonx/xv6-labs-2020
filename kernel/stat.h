#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device 文件系统所在的磁盘设备号
  uint ino;    // Inode number inode 编号
  short type;  // Type of file 文件类型（T_DIR/T_FILE/T_DEVICE）
  short nlink; // Number of links to file 硬链接数
  uint64 size; // Size of file in bytes 文件大小（字节）
};
