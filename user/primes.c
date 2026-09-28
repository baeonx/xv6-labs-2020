#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int end = 35;
int start = 2;

void generate(int p[])
{
    for(int i = start; i <= end; i ++)
    {
        write(p[1], &i, 4);
    }
    close(p[1]);
}

void primes(int p_left[])
{
    int first;
    // 关闭不用的写端
    close(p_left[1]);
    // 从 fd 当前的位置读最多 n 字节，读完把位置往后移。
    int num = read(p_left[0], &first, 4);
    if(num > 0)
    {
        fprintf(1, "prime %d\n", first);
        int t;
        int p_right[2];
        pipe(p_right);
        if(fork() > 0)
        {
            close(p_right[0]);
            while(read(p_left[0], &t, 4) > 0)
            {
                if(t % first != 0)
                {
                    write(p_right[1], &t, 4);
                }
            }
            close(p_right[1]);
            wait(0);
            exit(0);
        }
        else{
            primes(p_right);
        }
    }
    exit(0);
}


int
main(int argc, char *argv[])
{
    int p[2];
    pipe(p);   
    if(fork() == 0){
        // 子进程：负责读
        primes(p);        // 把 p 传进去
        exit(0);
    } else {
        // 父进程：负责写
        close(p[0]);      // 关掉不用的读端
        generate(p);      // 把 p 传进去
        wait(0);
        exit(0);
    }
}