#ifndef GLOBAL_H
#define GLOBAL_H
#include <time.h>
#include <mpi.h>
#include <pthread.h>

#define MAKE_ORDER_FLAG 10
#define GLOBAL_TAG 1
#define PARTY_TAG 2

#define PRAWDOPODOBIENSTWO_PRZYJECIA_ZAPROSZENIA 50 //%
#define PRAWDOPODOBIENSTWO_ZAPROSZENIA 20


pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lamportMut = PTHREAD_MUTEX_INITIALIZER;

int time_odpoczynek[2] = {300,1000};
int time_party[2] = {200,2000};
int time_sprzatanie[2] = {500,3000};
int time_miedzy_obrotami_poeta[2] = {2000,3000};

int mpi_size,mpi_rank;

int poeci_offset;

int lamport_clock;

struct Msg{
    int sender;
    int lamport;
    int flag;
    double data[3];
};//__attribute__((aligned(64)));

int rand_num(int min, int max)
{
    if(min >= max)
        return min;
    return rand()%(max - min) + min;
}

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