#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#include "util.h"

#include <fstream>

using namespace std;


#define LINE_SIZE 4096

typedef struct {
    char D1[LINE_SIZE];
    char D2[LINE_SIZE];
    int bitr;
} row;

row* global_db;
long long int size;
int is_finished = 0;     //程序是否结束
long long int throughput = 0;   //  系统最大并行度
long long int active0 = 0;
long long int active1 = 0;
int peroid = 0;

int *sec_throughput;
long long int timestamp = 0;


void load_db(long long int s) {
    size = s;
    global_db = (row*) malloc(size * sizeof(row));

    long long int i = 0;
    while (i < size) {
        global_db[i].bitr = 1;
        i++;
    }
}

void unit_write0(long long int index1) {
    int k = 0;
    int filed;
    filed = rand();
    while (k++ < 1024) {
        memcpy(global_db[index1].D1 + 4 * k, &filed, 4);
    }
    global_db[index1].bitr = 1;
}

void unit_write1(long long int index1) {
    int k = 0;
    int filed;
    filed = rand();
    while (k++ < 1024) {
        memcpy(global_db[index1].D2 + 4 * k, &filed, 4);
    }
    global_db[index1].bitr = 2;
}


void work0() {
    int i = 0;
    while (i++ < 3) {
        long long int index1 = rand() % (size);   //int value1 = rand();
        unit_write0(index1);
    }
    ++sec_throughput[timestamp];
}

void work1() {
    int i = 0;
    while (i++ < 3) {
        long long int index1 = rand() % (size);   //int value1 = rand();
        unit_write1(index1);
    }
    ++sec_throughput[timestamp];
}

void *transaction(void *info) {
    while (is_finished == 0) {
        int p = peroid;
        if (p % 2 == 0) {
            __sync_fetch_and_add(&active0, 1);
            work0();
            __sync_fetch_and_sub(&active0, 1);
        } else {
            __sync_fetch_and_add(&active1, 1);
            work1();
            __sync_fetch_and_sub(&active1, 1);
        }
    }

}

void *timer(void *info) {
    while (is_finished == 0) {
        sleep(1);
        //printf("%d\n",sec_throughput[timestamp]);
        ++timestamp;
    }
}

void checkpointer(int num) {
    while (num--) {
        int p = peroid;
        long long int i;
        if (p % 2 == 1) {
            while (active0 > 0);
            ofstream ckp_fd("dump.dat", ios::binary);
            i = 0;
            while (i < size) {
                if (global_db[i].bitr == 2)    // write to online  顺带着刷磁盘的过程中执行了
                {
                    memcpy(global_db[i].D1, global_db[i].D2, LINE_SIZE);
                    global_db[i].bitr = 0;
                }
                ckp_fd.write(global_db[i].D1, LINE_SIZE);
                i++;
            }
        } else {
            while (active1 > 0);
            i = 0;
            ofstream ckp_fd("dump.dat", ios::binary);
            while (i < size) {
                if (global_db[i].bitr == 1) {
                    memcpy(global_db[i].D2 , global_db[i].D1, LINE_SIZE);
                    global_db[i].bitr = 0;
                }
                ckp_fd.write(global_db[i].D2, LINE_SIZE);
                i++;
            }
        }
        sleep(1);    //和calc保持一致
        peroid++;
    }
    is_finished = 1;
}

int main(int argc, char const *argv[]) {
    srand((unsigned) time(NULL));
    load_db(atoi(argv[1]));
    sec_throughput = (int *) malloc(3600 * sizeof(int));  // assume running 1000s
    for (int j = 0; j < 1000; ++j) {
        sec_throughput[j] = 0;
    }
    throughput = atoi(argv[2]);
    for (int i = 0; i < throughput; ++i) {
        pthread_t pid_t;
        pthread_create(&pid_t, NULL, transaction, NULL);
    }
    pthread_t time_thread;
    pthread_create(&time_thread, NULL, timer, NULL);
    checkpointer(5);
    long long int sum = 0;
    for (int i = timestamp / 4; i < timestamp / 4 * 3; ++i) {
        sum += sec_throughput[i];
    }
    printf("PB,%d,%lld,%f\n", atoi(argv[1]), throughput, (float) sum / timestamp * 2);
    return 0;
}
