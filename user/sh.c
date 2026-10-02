// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  fprintf(2, "$ ");
  memset(buf, 0, nbuf);
  // 从终端读取nbuf个字节到buf里面
  gets(buf, nbuf);
  // 如果什么都没读到直接返回-1
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  // 作用是确保 fd 0、1、2 都已打开。
  // 如果拿到的 fd ≥ 3，说明 0、1、2 都被占了，关掉并退出
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // 
  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    // 如果是 'cd '
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      // buf 里是一整行命令，末尾有换行符 '\n'。
      // 去掉末尾换行符
      buf[strlen(buf)-1] = 0;  // chop \n
      // buf+3跳过 'cd '
      // 切换当前工作目录到 /home
      // chdir用于文件切换
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    // fork()一个子进程来执行
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;
  // 在堆区开辟了cmd的一个结构体并设置type=exec
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  // 将redircmd的类型强转成cmd类型
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  // 由于管道让
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  // 跳过buf前面的whitspace含有的字符
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  
  // 拿到buff的第一个字符
  // 这是个值拷贝
  ret = *s;
  // 如果 s 指向 <、|、>、&、;、(、) 其中一个，ret 直接就是那个字符，case 里 s++ 后就返回了
  // 如果 s 是>>，那ret就是+返回
  // 如果s不是符号，那就ret = a
  // 但是了每次执行gettoken，s都会移动到下个token
  // 类似于echo > hello world
  // s会移动到echo后的hello开头
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+'; // 对于>>改成 +
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  // echo hello world
  // 上方s移动到echo后的空格，再清除空格，让s移动到hello开头的h
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

// 查看ps跳过开头的空白符后, 开头是不是toks
// 拿到buf的地址，buf最后一个字符的地址，toks=""
int
peek(char **ps, char *es, char *toks)
{
  char *s;
  // ps = char** = &char*  
  // *ps = *&char* = char*
  // 解引用
  s = *ps;
  // char whitespace[] = " \t\r\n\v";
  // 检查s中开头每个字符看看是不是包含whitspace里的字符（\t在c里是一个字符）
  // 跳过开头的空白，\thelloworld，就跳过掉\t只剩下helloworld
  while(s < es && strchr(whitespace, *s))
    s++;
  // 把最后的空白符位置的后一个位置赋值给ps
  // 由于peek传过来是指针，所以直接改就行
  *ps = s;
  // *s非0，也就是s不是'\0'
  // strchr(toks, *s)在 toks 里查找 *s 这个字符，找到返回非 0，找不到返回 0。
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  // 将指针推到es指针指向s最后的'\0'
  es = s + strlen(s);
  cmd = parseline(&s, es);

  // 查看后续是否还有内容，如果还有内容，就解析错误
  peek(&s, es, "");
  // 如果
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

// ps是buf的第一个字符地址的地址
// es指向buf最后的\0
struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  // 解析是否有管道
  // ps是buf的第一个字符地址的地址
  // es指向buf最后的\0
  cmd = parsepipe(ps, es);
  // 查看buf里面是否有 '&'
  while(peek(ps, es, "&")){
    // 跳过 '&'
    gettoken(ps, es, 0, 0);
    // 以backcmd结尾来实现在后台继续跑
    cmd = backcmd(cmd);
  }
  // 查看buf里面是否有 ';'
  if(peek(ps, es, ";")){
    // 跳过';'
    gettoken(ps, es, 0, 0);
    // 把多条命令串成"列表命令"，依次执行
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;
  // ps是buf的第一个字符地址的地址
  // es指向buf最后的\0
  // 提取一个包含参数的cmd
  cmd = parseexec(ps, es);
  // 如果buf里面有|那就执行以下内容
  if(peek(ps, es, "|")){
    // 吃掉 '|'，ps 指向 'b'
    gettoken(ps, es, 0, 0);
    // 执行管道的cmd
    // 如果是|,那就左右分别
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  // 看看这个buf里面是否含有'<'或者'>'
  // parseredirs 把它包成：REDIR(EXEC(echo hello), 文件=b, 模式=写, fd=1)
  // 如果这个命令是echo > helloworld
  while(peek(ps, es, "<>")){
    // 跳过echo
    tok = gettoken(ps, es, 0, 0);
    // gettoken(ps, es, &q, &eq) != 'a'：文件名应该是普通词（类型 'a'），否则报错
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    // 
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  // 如果是含有<>字符的命令行就强转成redircmd
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

// 最终提取一个execmd
// 提取每个token的第一个和最后一个指针放到
//   char *argv[MAXARGS];
//   char *eargv[MAXARGS];
struct cmd*
parseexec(char **ps, char *es)
{
  // ps是buf的第一个字符地址的地址
  // es指向buf最后的\0
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  
  // 判断ps开头字符是不是"("
  struct cmd *ret;
  if(peek(ps, es, "("))
    return parseblock(ps, es);

  // 创建一个cmd然后设置类型为exec
  ret = execcmd();

  // 强转
  cmd = (struct execcmd*)ret;

  argc = 0;
  // ret是一个类型为exec的cmd
  // ps是buf的第一个字符地址的地址
  // es指向buf最后的\0
  // 看看是不是包含<>的命令 
  // 这里创建了一个cmd指向->execcmd
  ret = parseredirs(ret, ps, es);

  // 看看这个命令是不是包含"|)&;"其中一个
  // 如果不存在就开始提取
  while(!peek(ps, es, "|)&;")){
    // 根本没有词就直接跳转走
    // 经过gettoken
    // 比如提取hello world最后，ps变成指向world的w，q指向hello的h，eq是hello后一位的空格的指针
    if((tok=gettoken(ps, es, &q, &eq)) == 0) // 读到末尾（token 类型 0）就退出
      break;
    // 如果不是普通词，直接panic
    if(tok != 'a')
      panic("syntax");
    // q和eq是这个token的头指针和指向token后一位的指针
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    // [重定向] 命令名 [重定向] 参数 [重定向] 参数 [重定向] ...
    // 可能不止一次重定向所以需要再次检查
    // 新的 REDIR 节点，->cmd 指向旧的 cmd
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    // 把每个token参数结束位置改成 '\0'
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
