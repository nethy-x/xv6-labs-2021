#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int child_fd[2];
    int parent_fd[2];

    pipe(child_fd);
    pipe(parent_fd);

    int pid = fork();
    if (pid == 0) {
        // I am child
        char buf[1];
        read(parent_fd[0],buf,1);
        printf("%d: received ping\n",getpid());
        write(child_fd[1],"y",sizeof(char));
    } else if (pid > 0) {
        // I am parent
        write(parent_fd[1],"x",sizeof(char));
        char buf[1];
        read(child_fd[0],buf,1);
        printf("%d: received ping\n",getpid());
    } else {
        // Fork err
        fprintf(2,"error: fork failed");
    }

    exit();
} 