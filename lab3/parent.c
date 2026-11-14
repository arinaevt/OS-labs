#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdbool.h>

#define SHM_SIZE 4096

static char CHILD_NAME[] = "child";

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

    char shm_name[32];
    char sem_name[32];

    snprintf(shm_name, sizeof(shm_name), "/shm%d", getpid());
    snprintf(sem_name, sizeof(sem_name), "/sem%d", getpid());

    int shm = shm_open(shm_name, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if(shm == -1){
        const char msg[] = "error: failed to open SHM\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }

    if(ftruncate(shm, SHM_SIZE) == -1){
        const char msg[] = "error: failed to resize SHM\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(shm);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }

    char *shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if(shm_buf == MAP_FAILED){
        const char msg[] = "error: failed to map SHM\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }

    sem_t *sem = sem_open(sem_name, O_CREAT | O_RDWR, 0600, 1);
    if(sem == SEM_FAILED){
        const char msg[] = "error: failed to open semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }

    int file = open(argv[1], O_RDONLY);
    if(file < 0){
        const char msg[] = "error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        sem_close(sem);
        munmap(shm_buf, SHM_SIZE);
        sem_unlink(sem_name);
        shm_unlink(shm_name);
        close(shm);
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch(child){
    case -1:{
        const char msg[] = "error: failed to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        munmap(shm_buf, SHM_SIZE);
        close(shm);
        sem_unlink(sem_name);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    } break;
    
    case 0:{
        if(dup2(file, STDIN_FILENO) < 0){
            const char msg[] = "error: dup2 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(EXIT_FAILURE);
        }
        close(file);

        char path[2048];
        snprintf(path, sizeof(path) - 1, "%s/%s", prograth, CHILD_NAME);
        char *args[] = {CHILD_NAME, shm_name, sem_name, NULL};

        execv(path, args);

        const char msg[] = "error: failed to exec child program\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    } break;
    
    default: {
        close(file);
        close(shm);

        const char header[] = "Составные числа из файла:\n";
        write(STDOUT_FILENO, header, sizeof(header) - 1);

        bool running = true;
        while(running){
            sem_wait(sem);

            uint32_t *length = (uint32_t *)shm_buf;
            char *text = shm_buf + sizeof(uint32_t);

            if(*length == UINT32_MAX){
                running = false;
            } else if(*length > 0){
                write(STDOUT_FILENO, text, *length);
                *length = 0;
            }

            sem_post(sem);
        }

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

        sem_close(sem);
        munmap(shm_buf, SHM_SIZE);
        sem_unlink(sem_name);
        shm_unlink(shm_name);
    } break;
    }

    return 0;
}