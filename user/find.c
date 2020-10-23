#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct dirent {
    ushort inum;
    char name[14];
};

char * my_strcat(char *dest, const char *src) {
    int i,j;
    for (i = 0; dest[i] != '\0'; i++);
    for (j = 0; src[j] != '\0'; j++)
        dest[i+j] = src[j];
    dest[i+j] = '\0';
    return dest;
}

void find(int file_fd, char* path, char *target_name){
    struct dirent e;
    struct stat st;
    int fd;
    while(read(file_fd, &e,sizeof(e)) == sizeof(e)){
        if(e.name[0] != '\0')
        {
            if ((strcmp(e.name, ".") != 0) && (strcmp(e.name, "..") != 0)) {
                char buf[256];
                strcpy(buf, path);
                my_strcat(buf, "/");
                my_strcat(buf, e.name);
                printf("%s\n",buf);
                if ((fd = open(buf, 0)) < 0) {
                    fprintf(2, "find_find: cannot open %s\n", buf);
                    return;
                }
                if (fstat(fd, &st) < 0) {
                    fprintf(2, "find: cannot stat %s\n", buf);
                    close(fd);
                    return;
                } else if (st.type == T_DIR) {
                    find(fd, buf, target_name);
                }
                if (st.type == T_FILE && (strcmp(target_name, e.name) == 0)) {
                    printf("%s\n", buf);
                }
                close(fd);
            }
        }
    }
    close(file_fd);
}


int main(int argc, char *argv[]){
    int fd;
    struct stat st;
    if(argc < 3 ){
        fprintf(2,"invalid input, input example: find . a\n");
        exit(-1);
    }
    if((fd = open(argv[1], 0)) < 0){
        fprintf(2, "find: cannot open %s\n", argv[1]);
        exit(-1);
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", argv[1]);
        close(fd);
        exit(-1);
    }
    if(st.type != T_DIR){
        fprintf(2, "find: %s\n not a directory", argv[1]);
        close(fd);
        exit(-1);
    }

    find(fd, argv[1], argv[2]);
    exit(0);
}
