#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include "Poeta.c"
#include <unistd.h>
#include <time.h>


int size,rank;

int poeci_offset;

struct Msg{
    int od;
    int Flag;
};

void wait(int ms)
{
    //printf("Czekam %d ms", ms);
    clock_t t;
    t = clock();
    
    double wait_time =  CLOCKS_PER_SEC * (ms/1000.0);

    while(clock() < (double)t+wait_time);
}

int rand_num(int min, int max)
{
    return rand()%(max - min) + min;
}

int choice(int perc) // taki niby bool
{
    if(perc >= 100)
        return 1;
    int los = rand_num(1,100);
    //printf("los : %d\n",los);

    return los <=perc;
}
    
void poeta_Komunikacja()
{

}

void poeta_main()
{

    printf("Poeta nr %d zaczyna\n",rank-poeci_offset);

    if(choice(10))
    {
        printf("%d zaprasza \n",rank - poeci_offset);
    }

}

void wolonatariusz_main()
{
    printf("Wolontariusz nr %d zaczyna\n",rank);

}


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    //wait(200*rank);

    srand(time(NULL)+rank*20); 

    poeci_offset = 3;

    if(rank < poeci_offset)
        wolonatariusz_main();
    else
        poeta_main();

    MPI_Finalize();
}

