#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <ctype.h>
#include <semaphore.h>

#define SHM_SIZE 4096

static bool is_prime(int n){
    if (n < 2){
        return false;
    } else if(n == 2){
        return true;
    }
    for(int i = 2; i * i <= n; i++){
        if (n % i == 0){
            return false;
        }
    }
    return true;
}

void write_num(sem_t *sem, char *shm_buf, int num){
    char num_buf[16];
    int j = sizeof(num_buf) - 1;

    num_buf[j] = '\n';
    j--;

    if(num == 0){
        num_buf[j] = '0';
        j--;
    } else{
        while(num > 0){
            num_buf[j] = '0' + num % 10;
            num /= 10;
            j--;
        }
    }
    j++;

    size_t len = sizeof(num_buf) - j;

    sem_wait(sem);
    uint32_t *length = (uint32_t *)shm_buf;
    while(*length != 0){
        sem_post(sem);
        usleep(1000);
        sem_wait(sem);
    }
    char *text = shm_buf + sizeof(uint32_t);
    *length = (uint32_t)len;
    memcpy(text, num_buf + j, len);
    sem_post(sem);
}

void end(sem_t *sem, char *shm_buf){
    sem_wait(sem);
    uint32_t *length = (uint32_t *)shm_buf;
    *length = UINT32_MAX;
    sem_post(sem);
}

int main(int argc, char **argv){

    char *shm_name = argv[1];
    char *sem_name = argv[2];

    int shm = shm_open(shm_name, O_RDWR, 0);
    if(shm == -1){
        const char msg[] = "error: failed to open SHM\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }
    
    char *shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if(shm_buf == MAP_FAILED){
        const char msg[] = "error: failed to map SHM\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }
    close(shm);

    sem_t *sem = sem_open(sem_name, O_RDWR);
    if(sem == SEM_FAILED){
        const char msg[] = "error: failed to open semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }

    char buf[4096];
    ssize_t bytes;
    int cur = 0;
    bool was_digit = false;
    bool minus = false;

    bool running = true;
    while(running){
        bytes = read(STDIN_FILENO, buf, sizeof(buf));
        if(bytes < 0){
            const char msg[] = "error: failed to read from stdin\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(EXIT_FAILURE);
        }

        if(bytes == 0){
            running = false;
            break;
        }

        for (ssize_t i = 0; i < bytes; ++i){
            char c = buf[i];
            if(isdigit(c)){
                cur = cur * 10 + (c - '0');
                was_digit = true;
            } else if (c == '-' && !was_digit){
                minus = true;
            } else if(c == '\n'){
                if(was_digit){
                    if(minus){
                        end(sem, shm_buf);
                        munmap(shm_buf, SHM_SIZE);
                        sem_close(sem);
                        _exit(1);
                    }
                    if(is_prime(cur)){
                        end(sem, shm_buf);
                        munmap(shm_buf, SHM_SIZE);
                        sem_close(sem);
                        _exit(2);
                    }

                    write_num(sem, shm_buf, cur);

                    cur = 0;
                    minus = false;
                    was_digit = false;
                }
            }
        }
    
    }

    if(was_digit){
        if(minus){
            end(sem, shm_buf);
            munmap(shm_buf, SHM_SIZE);
            sem_close(sem);
            _exit(1);
        }
        if(is_prime(cur)){
            end(sem, shm_buf);
            munmap(shm_buf, SHM_SIZE);
            sem_close(sem);
            _exit(2);
        }

        write_num(sem, shm_buf, cur);
    }

    end(sem, shm_buf);

    munmap(shm_buf, SHM_SIZE);
    sem_close(sem);
    _exit(0);
}