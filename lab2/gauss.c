#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#define MAX_N 2000
#define BUF_SIZE 1024

double A[MAX_N][MAX_N];
double b[MAX_N];
double x[MAX_N];
int n;

void print_str(const char *str){
    int len = 0;
    while(str[len] != '\0'){
        len++;
    }
    write(STDOUT_FILENO, str, len);
}

void print_int(int num){
    char buf[12];
    int i = 11;

    if(num == 0){
        buf[i--] = '0';
    } else{
        bool negative = false;
        if(num < 0){
            negative = true;
            num = -num;
        }
        while(num > 0){
            buf[i--] = '0' + (num % 10);
            num /= 10;
        }
        if(negative){
            buf[i--] = '-';
        }
    }
    write(STDOUT_FILENO, buf + i + 1, sizeof(buf) - i - 1);
}

void print_double(double num){
    if(num < 0){
        write(STDOUT_FILENO, "-", 1);
        num = - num;
    }
    int int_part = (int)num;
    double frac_part = num - int_part;
    print_int(int_part);
    print_str(".");
    for(int i = 0; i < 3; i++){
        frac_part *= 10;
        int d = (int)frac_part;
        char c = '0' + d;
        write(STDOUT_FILENO, &c, 1);
        frac_part -= d;
    }
}



int parse_int(const char *str, int *pos){
    int result = 0;
    int sign = 1;
    while(str[*pos] == ' ' || str[*pos] == '\t'){
        (*pos)++;
    }
    if(str[*pos] == '-'){
        sign = -1;
        (*pos)++;
    }
    while(str[*pos] >= '0' && str[*pos] <= '9'){
        result = result * 10 + (str[*pos] - '0');
        (*pos)++;
    }
    return result * sign;
}

void generate_system(){
    srand(42);
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            if(i == j){
                A[i][j] = n * 10;
            } else{
                A[i][j] = (rand() % 100) / 10;
            }
        }
        b[i] = rand() % 1000;
    }
}

typedef struct {
    int thread_id;
    int count_threads;
    int k;
} ThreadArgs;

void* forward_step(void *arg){
    ThreadArgs *ta = (ThreadArgs*)arg;
    int k = ta->k;
    for(int i = ta->thread_id; i < n; i += ta->count_threads){
        if(i <= k){
            continue;
        }
        double factor = A[i][k] / A[k][k];
        for(int j = k; j < n; j++){
            A[i][j] -= factor * A[k][j];
        }
        b[i] -= factor * b[k];
    }
    return NULL;
}

void parallel_gauss(int count_threads){
    pthread_t* threads = (pthread_t*)malloc(count_threads * sizeof(pthread_t));
    ThreadArgs* args = (ThreadArgs*)malloc(count_threads * sizeof(ThreadArgs));
    for(int k = 0; k < n - 1; k++){
        for(int t = 0; t < count_threads; t++){
            args[t].thread_id = t;
            args[t].count_threads = count_threads;
            args[t].k = k;
            pthread_create(&threads[t], NULL, forward_step, &args[t]);
        }
        for(int t = 0; t < count_threads; t++){
            pthread_join(threads[t], NULL);
        }
    }
    for(int i = n - 1; i >= 0; i--){
        x[i] = b[i];
        for(int j = i + 1; j < n; j++){
            x[i] -= A[i][j] * x[j];
        }
        x[i] /= A[i][i];
    }

    free(threads);
    free(args);
}


void sequential_gauss(){
    for(int k = 0; k < n - 1; k++){
        for(int i = k + 1; i < n; i++){
            double factor = A[i][k] / A[k][k];
            for(int j = k; j < n; j++){
                A[i][j] -= factor * A[k][j];
            }
            b[i] -= factor * b[k];
        }
    }
    for(int i = n - 1; i >= 0; i--){
        x[i] = b[i];
        for(int j = i + 1; j < n; j++){
            x[i] -= A[i][j] * x[j];
        }
        x[i] /= A[i][i];
    }
}

double A_copy[MAX_N][MAX_N];
double b_copy[MAX_N];

void save_original(){
    for(int i = 0; i < n; i++){
        b_copy[i] = b[i];
        for(int j = 0; j < n; j++){
            A_copy[i][j] = A[i][j];
        }
    }
}

void restore_original(){
    for(int i = 0; i < n; i++){
        b[i] = b_copy[i];
        for(int j = 0; j < n; j++){
            A[i][j] = A_copy[i][j];
        }
    }
}

double get_time_ms(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char* argv[]){
    if(argc < 2){
        print_str("Мало аргументов! :(\n");
        return 1;
    }

    int pos = 0;
    int logical_cores = parse_int(argv[1], &pos);

    n = MAX_N;

    int pid = getpid();
    print_str("PID: ");
    print_int(pid);
    print_str("\n\n");

    // input_system();
    generate_system();
    save_original();

    double t_start = get_time_ms();
    sequential_gauss();
    double t_seq = get_time_ms() - t_start;
    print_str("Время выполнения последовательного алгоритма: ");
    print_double(t_seq);
    print_str(" мс\n");

    int thread_counts[8];
    thread_counts[0] = logical_cores / 4;
    if(thread_counts[0] < 1) thread_counts[0] = 1;
    thread_counts[1] = logical_cores / 2;
    if(thread_counts[1] < 1) thread_counts[1] = 1;
    thread_counts[2] = logical_cores - 2;
    if(thread_counts[2] < 1) thread_counts[2] = 1;
    thread_counts[3] = logical_cores - 1;
    if(thread_counts[3] < 1) thread_counts[3] = 1;
    thread_counts[4] = logical_cores;
    thread_counts[5] = logical_cores * 2;
    thread_counts[6] = logical_cores * 16;
    thread_counts[7] = logical_cores * 128;

    print_str("\nРезультаты параллельных алгоритмов:\n");
    for(int i = 0; i < 8; i++){
        int count_threads = thread_counts[i];
        restore_original();
        t_start = get_time_ms();
        parallel_gauss(count_threads);
        double t_par = get_time_ms() - t_start;
        double speed_up = t_seq / t_par;
        double efficiency = speed_up / count_threads;

        print_str("Число потоков: ");
        print_int(count_threads);
        print_str("\nВремя исполнения (мс): ");
        print_double(t_par);
        print_str("\nУскорение: ");
        print_double(speed_up);
        print_str("\nЭффективность: ");
        print_double(efficiency);
        print_str("\n--------------------------------------\n");
    }

    print_str("Решение x:\n");
    for(int i = 0; i < n; i++){
        print_double(x[i]);
        print_str("\n");
    }

    return 0;
}