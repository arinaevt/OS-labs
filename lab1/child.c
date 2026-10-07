#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include <stdlib.h>
#include <unistd.h>

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

int main(int argc, char **argv){
    char buf[4096];
    ssize_t bytes;

    int cur = 0;
    bool was_digit = false;
    bool minus = false;

    while (bytes = read(STDIN_FILENO, buf, sizeof(buf))){
        if(bytes < 0){
            const char msg[] = "error: failed to read from stdin\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        for (uint32_t i = 0; i < bytes; ++i){
            char c = buf[i];
            if(isdigit(c)){
                cur = cur * 10 + (c - '0');
                was_digit = true;
            } else if (c == '-' && !was_digit){
                minus = true;
            } else if(c == '\n'){
                if(was_digit){
                    int num;
                    if(minus){
                        num = -cur;
                    } else{
                        num = cur;
                    }

                    if(num < 0){
                        const char msg[] = "There is negative number!\n";
                        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
                        exit(1);
                    }
                    if(is_prime(num)){
                        const char msg[] = "There is prime number!\n";
                        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
                        exit(2);
                    }

                    char num_buf[16];
                    int j = sizeof(num_buf) - 1;

                    num_buf[j] = '\n';
                    j--;

                    if(num == 0){
                        num_buf[j] = '0';
                        j--;
                    } else{
                        while (num > 0){
                            num_buf[j] = '0' + (num % 10);
                            num /= 10;
                            j--;
                        }
                    }
                    if(minus){
                        num_buf[j] = '-';
                        j--;
                    }
                    j++;
                    write(STDOUT_FILENO, num_buf + j, sizeof(num_buf) - j);
                }
                cur = 0;
                minus = false;
                was_digit = false;
            }
        }
    }

    if(was_digit){
        int num;
        if(minus){
            num = -cur;
        } else{
            num = cur;
        }

        if(num < 0){
            const char msg[] = "There is negative number!\n";
            write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            exit(1);
        }
        if(is_prime(num)){
            const char msg[] = "There is prime number!\n";
            write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            exit(2);
        }

        char num_buf[16];
        int j = sizeof(num_buf) - 1;
        num_buf[j] = '\n';
        j--;

        if(num == 0){
            num_buf[j] = '0';
            j--;
        } else{
            while (num > 0){
                num_buf[j] = '0' + (num % 10);
                num /= 10;
                j--;
            }
        }
        if(minus){
            num_buf[j] = '-';
            j--;
        }
        j++;
        write(STDOUT_FILENO, num_buf + j, sizeof(num_buf) - j);
    }

    exit(0);
}