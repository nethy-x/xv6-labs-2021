
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void find_prime(int fd_read ){
    int p;
    read(fd_read, &p, sizeof(int));
    printf("prime %d\n",p);
    int n;
    int fds[2];
    pipe(fds);
    int new_call = 0;
    while(read(fd_read, &n, sizeof(n)) == sizeof(int)){
        new_call = 1;
        if(n % p != 0){
            write(fds[1], &n, sizeof(int));
        }
    }
    if(new_call){
        int ret = fork();
        if(ret == 0){
            close(fds[1]);
            find_prime(fds[0]);
        }else if(ret < 0){
            fprintf(2,"find_prime() fork() error\n");
        }
    }
    close(fds[1]);
    wait(&p);
}




int main(int argc, char *argv[]){
    close(0);
    int fds[2];
    int i;
    if( pipe(fds) < 0){
        fprintf(2,"pipe() error\n");
    }

    int ret = fork();
    if(ret == 0){
        //child
        close(fds[1]);
        find_prime(fds[0]);
    }else if(ret > 0){
        //parent
        for(i = 2; i < 36; i++){
            write(fds[1], &i, sizeof(int));
        }
        close(fds[1]);
        wait(&ret);
    }else{
        fprintf(2, "fork() error\n");
        exit(1);
    }
    exit(0);
}