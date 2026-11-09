#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

static char CHILD_NAME[] = "child"; /дочерний

int main(int argc, char *argv[]){
    if(argc == 1){
        char msg[1024];
        uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
        write(STDERR_FILENO, msg, len);
        exit(EXIT_FAILURE);
    }

    char prograth[1024];
    {
        ssize_t len = readlink("/proc/self/exe", prograth, sizeof(prograth) - 1);
        if(len == -1){
            const char msg[] = "error: failed to read full program path\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        while(prograth[len] != '/' && len > 0){
            --len;
        }

        prograth[len] = '\0';
    }

    char tmpfile[] = "/tmp/lab3XXXXXX";
    int fd = mkstemp(tmpfile);
    if(fd == -1){
        const char msg[] = "error: failed to create temporary file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const size_t MAP_SIZE = 4096;
    if(ftruncate(fd, MAP_SIZE) == -1){
        const char msg[] = "error: failed to ftruncate\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(fd);
        unlink(tmpfile);
        exit(EXIT_FAILURE);
    }
    
    char *shared = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared == MAP_FAILED){
        const char msg[] = "error: mmap failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(fd);
        unlink(tmpfile);
        exit(EXIT_FAILURE);
    }

    memset(shared, 0, MAP_SIZE);

    int file = open(argv[1], O_RDONLY);
    if(file < 0){
        const char msg[] = "error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        munmap(shared, MAP_SIZE);
        close(fd);
        unlink(tmpfile);
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch(child){
    case -1:{
        const char msg[] = "error: failed to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        munmap(shared, MAP_SIZE);
        close(fd);
        unlink(tmpfile);
        exit(EXIT_FAILURE);
    } break;
    
    case 0:{
        if(dup2(file, STDIN_FILENO) < 0){
            const char msg[] = "error: dup2 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(file);

        char path[2048];
        snprintf(path, sizeof(path) - 1, "%s/%s", prograth, CHILD_NAME);
        char *const args[] = {CHILD_NAME, tmpfile, NULL};

        execv(path, args);

        const char msg[] = "error: failed to exec child program\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    } break;
    
    default: {
        
        close(file);
        close(fd);

        const char header[] = "Составные числа из файла:\n";
        write(STDOUT_FILENO, header, sizeof(header) - 1);



        int status;
        waitpid(child, &status, 0);

        size_t pos = 0;
        while(pos < MAP_SIZE && shared[pos] != '\0'){
            size_t start = pos;
            while(pos < MAP_SIZE && shared[pos] != '\n' && shared[pos] != '\0'){
                pos++;
            }
            if(pos > start){
                write(STDOUT_FILENO, shared + start, pos - start);
                write(STDOUT_FILENO, "\n", 1);
            }
            if(pos < MAP_SIZE && shared[pos] == '\n'){
                pos++;
            }
        }

        if(WIFEXITED(status)){
            int code = WEXITSTATUS(status);
            if(code == 1 ){
                const char msg[] = "Дочерний процесс завершён: найдено отрицательное число\n";
                write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            } else if(code == 2){
                const char msg[] = "Дочерний процесс завершён: найдено простое число\n";
                write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            } else{
                const char msg[] = "Дочерний процесс завершён\n";
                write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            }
        }

        const char msg[] = "Parent process is ended\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);

        munmap(shared, MAP_SIZE);
        unlink(tmpfile);
    } break;
    }

    return 0;

}
