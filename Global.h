#ifndef GLOBAL_H
#define GLOBAL_H
#include <time.h>
#include <mpi.h>
#include <pthread.h>

#define MAKE_ORDER_FLAG 10
#define GLOBAL_TAG 1
#define PARTY_TAG 2


pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lamportMut = PTHREAD_MUTEX_INITIALIZER;

int mpi_size,mpi_rank;

int poeci_offset;

int lamport_clock;

struct Msg{
    int sender;
    int lamport;
    int flag;
    double data[3];
};

int max(int a, int b)
{
    if(a>b)
        return a;
    return b;
}

void wait(int ms)
{


    //printf("Czekam %d ms", ms);
    clock_t t;
    t = clock();
    
    double wait_time =  CLOCKS_PER_SEC * (ms/1000.0);

    while(clock() < (double)t+wait_time);
}

#endif