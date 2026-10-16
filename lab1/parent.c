#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

static char SERVER_PROGRAM_NAME[] = "child";

int main(int argc, char *argv[]){
    if(argc == 1){
        char msg[1024];
        uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
        write(STDERR_FILENO, msg, len);
        exit(EXIT_SUCCESS);
    }

    char prograth[1024];
    {
        ssize_t len = readlink("/proc/self/exe", prograth, sizeof(prograth) - 1);
        if(len == -1){
            const char msg[] = "error: failed to read full programm path\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        while(prograth[len] != '/' && len > 0){
            --len;
        }

        prograth[len] = '\0';
    }

    int pipe1[2];
    if(pipe(pipe1) == -1){
        const char msg[] = "error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int file = open(argv[1], O_RDONLY);
    if(file < 0){
        const char msg[] = "error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(pipe1[0]);
        close(pipe1[1]);
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch(child){
    case -1:{
        const char msg[] = "error: faied to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        close(pipe1[0]);
        close(pipe1[1]);
        exit(EXIT_FAILURE);
    } break;
    
    case 0:{
        close(pipe1[0]);

        if(dup2(file, STDIN_FILENO) < 0){
            const char msg[] = "error: dup2 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(file);

        if(dup2(pipe1[1], STDOUT_FILENO) < 0){
            const char msg[] = "error: dup2 stdout failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(pipe1[1]);


        char path[1024];
        snprintf(path, sizeof(path) - 1, "%s/%s", prograth, SERVER_PROGRAM_NAME);
        char *const args[] = {SERVER_PROGRAM_NAME, argv[1], NULL};

        int32_t status = execv(path, args);
        if(status == -1){
            const char msg[] = "error: failed to exec child program\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
    } break;
    
    default: {
        
        close(pipe1[1]);
        close(file);

        const char header[] = "Составные числа из файла:\n";
        write(STDOUT_FILENO, header, sizeof(header) - 1);

        char buf[4096];
        ssize_t bytes;

        while((bytes = read(pipe1[0], buf, sizeof(buf))) > 0){
            write(STDOUT_FILENO, buf, bytes);
        }

        if(bytes < 0){
            const char msg[] = "error: failed to read from pipe\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
        }

        close(pipe1[0]);

        int status;
        waitpid(child, &status, 0);

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
    } break;
    }

    return 0;

}
