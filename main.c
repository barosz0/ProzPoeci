
#include <stdlib.h>
#include <stdio.h>
#include "Poeta.c"
#include "Wolontariusz.c"
#include <unistd.h>
#include "Global.h"
#include <pthread.h>

pthread_t Kom, Main;


void wolonatariusz_main()
{
    printf("Wolontariusz nr %d zaczyna\n",mpi_rank);

}

void poeta_Init()
{
    
    poeta_stan = IN_WAIT_STATUS;
    alkohol = 0;
    zagrycha = 0;
    sepienie = 0;

    party_tab = NULL;

    pthread_create(&Kom, NULL, poeta_Komunikacja, 0);
    //pthread_create(&PoetaMain, NULL, poeta_main, 0);
    
    poeta_main(NULL);
}

void wolontariusz_Init()
{
    queue_rank = (int*)malloc(sizeof(int)*(poeci_offset));
    queue_lamport = (int*)malloc(sizeof(int)*(poeci_offset));

    for(int i=0;i<poeci_offset;i++)
    {
        queue_rank[i] = -1;
        queue_lamport[i] = -1;
    }

    wolontariusz_stan = IN_WAIT_STATUS;
    pthread_create(&Kom, NULL, wolontariusz_Komunikacja, 0);



    wolontariusz_main(NULL);
}


int main(int argc, char **argv)
{
    int provided;
    MPI_Init_thread(&argc, &argv,MPI_THREAD_MULTIPLE, &provided);

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    //wait(200*rank);

    srand(123214+mpi_rank*20); //time(NULL)

    poeci_offset = 3;
    lamport_clock = 0;

    if(mpi_rank < poeci_offset)
        wolontariusz_Init();
        //wolontariusz_main(NULL);
    else
        poeta_Init();

    //void **retval;

    // pthread_join(PoetaMain,retval);
    //pthread_join(PoetaKom,retval);

    while(1);

    MPI_Finalize();
}

//mpicc main.c -pthread
//mpirun -np 10 --oversubscribe a.out

