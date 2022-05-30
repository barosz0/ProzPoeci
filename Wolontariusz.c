#include "Global.h"

#define PRIV_TAG 3

#define IN_WAIT_STATUS 1
#define IN_WORK_STATUS 2

#define SING_UP_FLAG 1
#define FIRST_CHECK_FLAG 2
#define TAKE_ORDER_FLAG 3

pthread_mutex_t queueMut = PTHREAD_MUTEX_INITIALIZER;

int *queue_rank;
int *queue_lamport;
int wolontariusz_stan;
int zleceniodawca = -1;

void sing_up()
{
    struct Msg msg;
    msg.sender = mpi_rank;
    msg.flag = SING_UP_FLAG;

    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg.lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );

    for(int i=0;i<poeci_offset;i++)
    {
        // wysyla wiadomosc do wszystkich wolontariuszy
        MPI_Send(&msg,sizeof(msg),MPI_BYTE,i, GLOBAL_TAG, MPI_COMM_WORLD);
    }
}

void queue_insert(int rank,int lamp)
{
    pthread_mutex_lock( &queueMut );

    int poz = 0;

    for(int i=0;i<poeci_offset;i++)
    {
        // znalazlem wolne miejsce
        if(queue_rank[i]==-1)
        {
            queue_rank[i]=rank;
            queue_lamport[i]=lamp;
            break;
        }

        // znalazlem proces z wiekszym zegarem wiec wskakuje przed niego
        if(queue_lamport[i]>lamp)
        {
            //przesuwam kolejke o jeden w kierunku tylu
            for(int j=poeci_offset-2;j>=i;j--)
            {
                queue_rank[j+1] = queue_rank[j];
                queue_lamport[j+1] = queue_lamport[j];
            }
            queue_rank[i]=rank;
            queue_lamport[i]=lamp;
            break;
        }

        // znalzlem proces z takim samym zegarem ale starszy wiec wskakuje przed niego
        if(queue_rank[i]>rank && queue_lamport[i] == lamp)
        {
            //przesuwam kolejke o jeden w kierunku tylu
            for(int j=poeci_offset-2;j>=i;j--)
            {
                queue_rank[j+1] = queue_rank[j];
                queue_lamport[j+1] = queue_lamport[j];
            }
            queue_rank[i]=rank;
            queue_lamport[i]=lamp;
            break;
        }
    }
    pthread_mutex_unlock( &queueMut );

}

int delete_from_queue(int rank, int lamp)
{
    pthread_mutex_lock( &queueMut );

    int flag = 0;

    if(queue_lamport[0]==lamp && queue_rank[0]==rank)
    {
        flag = 1;
        for(int i=0;i<poeci_offset-1;i++)
        {
            queue_lamport[i] = queue_lamport[i+1];
            queue_rank[i] = queue_rank[i+1];
        }
        queue_lamport[poeci_offset-1] = -1;
        queue_rank[poeci_offset-1] = -1;
    }

    pthread_mutex_unlock( &queueMut );
    return flag;
}

int check_first(int rank, int lamp)
{
    pthread_mutex_lock( &queueMut );
    int flag = 0;

    if(queue_lamport[0]==lamp && queue_rank[0]==rank)
    {
        flag = 1;
    }

    pthread_mutex_unlock( &queueMut );
    return flag;

}

int try_take_order()
{
    printf("Ja %d probuje |%d|%d|%d|\n",mpi_rank,queue_rank[0],queue_rank[1],queue_rank[2]);
    struct Msg msg;
    msg.sender = mpi_rank;
    msg.flag = FIRST_CHECK_FLAG;
    msg.data[0] = queue_lamport[0];

    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg.lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );

    for(int i=0;i<poeci_offset;i++)
    {
        // wysyla wiadomosc do wszystkich wolontariuszy
        if(i!=mpi_rank)
            MPI_Send(&msg,sizeof(msg),MPI_BYTE,i, GLOBAL_TAG, MPI_COMM_WORLD);
    }

    int flag = 1;

    for(int i=0;i<poeci_offset;i++)
    {
        if(i==mpi_rank)
            continue;
        
        MPI_Status status;
        struct Msg rec;
        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, PRIV_TAG, MPI_COMM_WORLD, &status);

        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );

        if(rec.flag !=FIRST_CHECK_FLAG)
            printf("Got wrong Flag\n");
        if(!rec.data[0])
        {
            flag = 0;
        }
    }

    

    if(flag)
    {
        msg.sender = mpi_rank;
        msg.flag = TAKE_ORDER_FLAG;
        msg.data[0] = queue_lamport[0];

        pthread_mutex_lock( &lamportMut );
        lamport_clock+=1;
        msg.lamport = lamport_clock;
        pthread_mutex_unlock( &lamportMut );

        for(int i=0;i<poeci_offset;i++)
        {
            // wysyla wiadomosc do wszystkich wolontariuszy (siebie też)
            
            MPI_Send(&msg,sizeof(msg),MPI_BYTE,i, GLOBAL_TAG, MPI_COMM_WORLD);
        }
        return 1;
    }
    return 0;
}

void *wolontariusz_Komunikacja(void *ptr)
{
    //printf("----------------Start komunikacja wolon\n");

    MPI_Status status;
    struct Msg rec;
    
    while(1+1){

        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, GLOBAL_TAG, MPI_COMM_WORLD, &status);
        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );

        printf("Ja %d otrzymalem %d od %d\n",mpi_rank,rec.flag,rec.sender);
        switch (rec.flag)
        {
        case SING_UP_FLAG:
            queue_insert(rec.sender,rec.lamport);
            break;
        case FIRST_CHECK_FLAG: // odsyla informacje czy podany proces moze wejsc do sekcji
            struct Msg m;
            m.data[0] = check_first(rec.sender,rec.data[0]);
            m.sender = mpi_rank;
            m.flag = FIRST_CHECK_FLAG;
            pthread_mutex_lock( &lamportMut );
            lamport_clock+=1;
            m.lamport = lamport_clock;
            pthread_mutex_unlock( &lamportMut );
            MPI_Send(&m,sizeof(m),MPI_BYTE,rec.sender, PRIV_TAG, MPI_COMM_WORLD);
            break;
        case MAKE_ORDER_FLAG: // poeta prosi o sprzatanie
            printf("Ja %d otrzymalem prosbe o sprzatanie od %d\n",mpi_rank,rec.sender);
            pthread_mutex_lock( &queueMut );
            pthread_mutex_lock( &stateMut );
            if(queue_rank[0]==mpi_rank && wolontariusz_stan==IN_WAIT_STATUS)
            {
                pthread_mutex_unlock( &stateMut );
                pthread_mutex_unlock( &queueMut );

                zleceniodawca = rec.sender;

                pthread_mutex_lock( &stateMut );
                wolontariusz_stan = IN_WORK_STATUS;
                pthread_mutex_unlock( &stateMut );


                // if(try_take_order())
                // {
                //     pthread_mutex_lock( &stateMut );
                //     wolontariusz_stan = IN_WORK_STATUS;
                //     pthread_mutex_unlock( &stateMut );

                //     struct Msg m;
                //     m.data[0] = 1;
                //     m.sender = mpi_rank;
                //     m.flag = MAKE_ORDER_FLAG;
                //     pthread_mutex_lock( &lamportMut );
                //     lamport_clock+=1;
                //     m.lamport = lamport_clock;
                //     pthread_mutex_unlock( &lamportMut );
                //     MPI_Send(&m,sizeof(m),MPI_BYTE,rec.sender, PARTY_TAG, MPI_COMM_WORLD);
                //     zleceniodawca = rec.sender;
                // }
                // else
                // {
                //     struct Msg m;
                //     m.data[0] = 0;
                //     m.sender = mpi_rank;
                //     m.flag = MAKE_ORDER_FLAG;
                //     pthread_mutex_lock( &lamportMut );
                //     lamport_clock+=1;
                //     m.lamport = lamport_clock;
                //     pthread_mutex_unlock( &lamportMut );
                //     MPI_Send(&m,sizeof(m),MPI_BYTE,rec.sender, PARTY_TAG, MPI_COMM_WORLD);

                // }
            }
            else
            {
                pthread_mutex_unlock( &stateMut );
                pthread_mutex_unlock( &queueMut );

                struct Msg m;
                m.data[0] = 0;
                m.sender = mpi_rank;
                m.flag = MAKE_ORDER_FLAG;
                pthread_mutex_lock( &lamportMut );
                lamport_clock+=1;
                m.lamport = lamport_clock;
                pthread_mutex_unlock( &lamportMut );
                MPI_Send(&m,sizeof(m),MPI_BYTE,rec.sender, PARTY_TAG, MPI_COMM_WORLD);

            }
            printf("Koniec----------------------------------------------------------|\n");
            break;
        case TAKE_ORDER_FLAG:
            delete_from_queue(rec.sender,rec.data[0]);
            break;
        default:
            printf("Odebrano wiadomosc od %d z flaga %d",rec.sender,rec.flag);
            break;
        }
    }
}

void sprzataj()
{
    int czas = rand_num(500,3000);

    struct Msg msg;

    msg.data[0]=czas;
    msg.sender = mpi_rank;
    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg.lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );
    msg.flag = TAKE_ORDER_FLAG;

    MPI_Send(&msg,sizeof(msg),MPI_BYTE,zleceniodawca, PARTY_TAG, MPI_COMM_WORLD);
    printf("Wyslano do %d\n",zleceniodawca);

    wait(czas);

    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg.lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );

    MPI_Send(&msg,sizeof(msg),MPI_BYTE,zleceniodawca, PARTY_TAG, MPI_COMM_WORLD);
}

void *wolontariusz_main(void *ptr)
{
    sing_up();
    while(2+2)
    {
        pthread_mutex_lock( &stateMut );
        if(wolontariusz_stan == IN_WORK_STATUS)
        {
            pthread_mutex_unlock( &stateMut );
            
            if(try_take_order())
            { 

                struct Msg m;
                m.data[0] = 1;
                m.sender = mpi_rank;
                m.flag = MAKE_ORDER_FLAG;
                pthread_mutex_lock( &lamportMut );
                lamport_clock+=1;
                m.lamport = lamport_clock;
                pthread_mutex_unlock( &lamportMut );
                MPI_Send(&m,sizeof(m),MPI_BYTE,zleceniodawca, PARTY_TAG, MPI_COMM_WORLD);

                sprzataj();
                sing_up();
                
            }
            else
            {
                struct Msg m;
                m.data[0] = 0;
                m.sender = mpi_rank;
                m.flag = MAKE_ORDER_FLAG;
                pthread_mutex_lock( &lamportMut );
                lamport_clock+=1;
                m.lamport = lamport_clock;
                pthread_mutex_unlock( &lamportMut );
                MPI_Send(&m,sizeof(m),MPI_BYTE,zleceniodawca, PARTY_TAG, MPI_COMM_WORLD);

            }



            pthread_mutex_lock( &stateMut );
            wolontariusz_stan = IN_WAIT_STATUS;
            pthread_mutex_unlock( &stateMut );
            zleceniodawca = -1;
        }
        else
        {
            pthread_mutex_unlock( &stateMut );
        }
        
    }

}