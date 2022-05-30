#include "Global.h"


#define INV_FLAG 1
#define INV_ODP_FLAG 2
#define PARTY_INFO_FLAG 3
#define PARTY_END_FLAG 4



#define IN_WAIT_STATUS 1
#define IN_PARTY_STATUS 2

#define ALKOHOL 0
#define ZAGRYCHA 1
#define SEPIENIE 2


int poeta_stan;
int *party_tab;
int party_creator;

int alkohol;
int zagrycha;
int sepienie;



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

void send_msg(struct Msg* msg, int destination, int tag)
{
    msg->sender = mpi_rank;
    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg->lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );
    int ret = MPI_Send(msg,sizeof(*msg),MPI_BYTE,destination, tag, MPI_COMM_WORLD);

    printf("Ja %d wyslalem wiadomosc do %d z tagiem %d z flaga %d efekt %d\n",mpi_rank,destination,tag,msg -> flag,ret);
}

void recive_invite(struct Msg m)
{
    struct Msg odp;
    odp.flag = INV_ODP_FLAG;

    pthread_mutex_lock( &stateMut );
    if(poeta_stan == IN_PARTY_STATUS)
    {
        odp.data[0] = 0;
        printf("Ja %d odrzucilem zaproszenie od %d - juz imprezuje\n",mpi_rank,m.sender);
    }
    else
    {
        if(choice(50))
        {    
            odp.data[0] = 1;
            printf("Ja %d przyjalem zaproszenie od %d\n",mpi_rank,m.sender);
            //change status na zaproszony
            poeta_stan = IN_PARTY_STATUS;
            party_creator = m.sender;
        }
        else
        {
            odp.data[0] = 0;
            printf("Ja %d odrzucilem zaproszenie od %d\n",mpi_rank,m.sender);
        }
    }
    pthread_mutex_unlock( &stateMut );


    //odp.sender = mpi_rank;
    send_msg(&odp,m.sender,PARTY_TAG);
    
}

void *poeta_Komunikacja(void *ptr)
{
    MPI_Status status;
    struct Msg rec;

    while(1+1){

        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, GLOBAL_TAG, MPI_COMM_WORLD, &status);
        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );

        printf("Ja %d otrzymalem od %d flage %d\n",mpi_rank,rec.sender,rec.flag);
        switch (rec.flag)
        {
        case INV_FLAG:
            printf("Otrzymana zaproszenie %d\n", mpi_rank);
            recive_invite(rec);
        break;

        default:
            printf("Odebrano wiadomosc od %d z flaga %d",rec.sender,rec.flag);
            break;
        }
    }
}

int send_invite()
{
    struct Msg inv;
    inv.flag = INV_FLAG;
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i == mpi_rank)
            continue;
        send_msg(&inv, i,GLOBAL_TAG);
    }

    MPI_Status status;
    struct Msg rec;

    party_tab = (int*)malloc(sizeof(int)*(mpi_size-poeci_offset));
    party_tab[mpi_rank-poeci_offset] = 1;

    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank) continue; // nie odbieram od siebie
        printf("Czekam na odpowiedź od %d\n",i);
        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);
        //printf("ODP od %d %d %d\n",rec.sender,(int)rec.data[0],party_tab[i-poeci_offset]);
        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );
        
        printf("Mam odpowiedź od %d\n",i);

        if(rec.flag == INV_ODP_FLAG)
        {
            party_tab[i-poeci_offset]=(int)rec.data[0];
        }
        else
        {
            printf("-----ERROR - Nie odpowiedni tag w odebranej wiadomosci (send_invite)\n");
            party_tab[i-poeci_offset]=0;
        }
    }
    printf("Mam wszystkie zaproszenia\n");

    int count = 0;

    for(int i=0;i<mpi_size-poeci_offset;i++)
    {
        printf("%d ",party_tab[i]);
        if(party_tab[i])
            count++;
    }
    printf("count %d\n",count);
    if(count>3)
        return count;
    else
    {
        // free(party_tab);
        // party_tab = NULL;
        return 0;
    }
        
}

void odpocznij_po_party()
{
    int time = rand_num(300,1000);
    wait(time);
}

void cancel_party()
{
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank)
            continue;

        if(party_tab[i-poeci_offset]){
            struct Msg m;
            m.flag = PARTY_END_FLAG;
            send_msg(&m,i,PARTY_TAG);
        }
    }
}

void sprzatanie()
{

    printf("Zamawiam wolontariusza\n");
    struct Msg msg;
    msg.sender = mpi_rank;
    msg.flag = MAKE_ORDER_FLAG;

    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg.lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );

    int flag = 1;

    while(flag)
    {   
        for(int i=0;i<poeci_offset;i++)
        {
            // wysyla wiadomosc do wszystkich wolontariuszy
            MPI_Send(&msg,sizeof(msg),MPI_BYTE,i, GLOBAL_TAG, MPI_COMM_WORLD);
        }
        printf("Wyslane podania\n");
        for(int i=0;i<poeci_offset;i++)
        {

            MPI_Status status;
            struct Msg rec;
            MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);

            pthread_mutex_lock( &lamportMut );
            lamport_clock= max(lamport_clock,rec.lamport)+1;
            pthread_mutex_unlock( &lamportMut );

            printf("Otrzymana odpowiedz od %d z flaga %d\n",rec.sender,rec.data[0]);

            if(rec.data[0])
            {
                flag=0;
            }
        }

        if(flag){
            printf("Nie udana proba zamownia sprzatania\n");
            wait(100); // zeby nie sypal wiadomosciami
        }
    }

    MPI_Status status;
    struct Msg rec;
    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, PARTY_TAG, MPI_COMM_WORLD, &status);

    printf("Posprzatac ma %d zajmie mu to %d\n",rec.sender,rec.data[0]);


    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, PARTY_TAG, MPI_COMM_WORLD, &status);

    printf("Zostalo posprzatane\n");
}

void party_host()
{
    // synchronizacja startowa aka informacja ze libacja sie odbedzie
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank)
            continue;

        if(party_tab[i-poeci_offset]){
            struct Msg m;
            m.flag = PARTY_INFO_FLAG;
            send_msg(&m,i,PARTY_TAG);
        }
    }


    MPI_Status status;
    struct Msg rec;

    int wartosci[mpi_size-poeci_offset][3];

    // Zebeanie liczb od procesow
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank){
            wartosci[mpi_rank-poeci_offset][0] = alkohol;
            wartosci[mpi_rank-poeci_offset][0] = zagrycha;
            wartosci[mpi_rank-poeci_offset][0] = sepienie;
            continue;
        }
        if(party_tab[i-poeci_offset])
        {
            printf("Czekam na wiadomosc od %d\n",i);
            MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);
            pthread_mutex_lock( &lamportMut );
            lamport_clock= max(lamport_clock,rec.lamport)+1;
            pthread_mutex_unlock( &lamportMut );

            if(rec.flag == PARTY_INFO_FLAG)
            {
                wartosci[i-poeci_offset][0]=(int)rec.data[0];
                wartosci[i-poeci_offset][1]=(int)rec.data[1];
                wartosci[i-poeci_offset][2]=(int)rec.data[2];

            }
            else
            {
                printf("-----ERROR - Nie odpowiedni tag w odebranej wiadomosci (PARTY_zbieranie info)\n");
                party_tab[i-poeci_offset]=0;
            }
        }

    }

    // szukam ofiar

    double min_stosunek = 0;
    int ofiary[2] = {-1, -1};

    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(party_tab[i-poeci_offset])
        {
            double stosunek;
            if(wartosci[i-poeci_offset][2]==0)
                stosunek= 1;
            else // ((alkohol + zagrycha) /sepienie
                stosunek = (wartosci[i-poeci_offset][0] +wartosci[i-poeci_offset][1])/(double)wartosci[i-poeci_offset][2];

            // wybierane sa osoby z najmniejszym stosunkiem, drugorzegnie brany pod uwage jest pid
            if(ofiary[0]==-1)
            {
                min_stosunek = stosunek;
                ofiary[0]=i;
            }
            else if(ofiary[1]==-1)
            {
                if (stosunek < min_stosunek)
                {
                    min_stosunek = stosunek;
                    ofiary[1] = ofiary[0];
                    ofiary[0] = i;
                }
                else
                {
                    ofiary[1] = i;
                }
            }
            else
            {
                if (stosunek < min_stosunek)
                {
                    min_stosunek = stosunek;
                    ofiary[1] = ofiary[0];
                    ofiary[0] = i;
                }
            }
        }

    }

    // wybieram osobe do alkocholu
    int stosunekA;
    int stosunekB;

    // ustawiam tak aby mlodszy proces byl procesem A
    if(ofiary[0] > ofiary[1])
    {
        int pom = ofiary[1];
        ofiary[1] = ofiary[0];
        ofiary[0] = pom;
    }

    if(wartosci[ofiary[0]-poeci_offset][2]==0)
        stosunekA = 1;
    else
        stosunekA = wartosci[ofiary[0]-poeci_offset][0]/wartosci[ofiary[0]-poeci_offset][2];
    
    if(wartosci[ofiary[1]-poeci_offset][2]==0)
        stosunekB = 1;
    else
        stosunekB = wartosci[ofiary[1]-poeci_offset][0]/wartosci[ofiary[1]-poeci_offset][2];


    printf("Ofiary to %d %d\n", ofiary[0],ofiary[1]);
    if(stosunekB < stosunekA)
    {
        int pom = ofiary[1];
        ofiary[1] = ofiary[0];
        ofiary[0] = pom;
    }

    // wysylanie informacji o alkocholu
    if(ofiary[0]== mpi_rank)
    {
        alkohol++;
    }
    else
    {
        struct Msg m;
        m.flag = PARTY_INFO_FLAG;
        m.sender = mpi_rank;
        m.data[0] = ALKOHOL;
        send_msg(&m,ofiary[0],PARTY_TAG);
    }

    // wysylanie informacji o zagrycha
    if(ofiary[1]== mpi_rank)
    {
        zagrycha++;
    }
    else
    {
        struct Msg m;
        m.flag = PARTY_INFO_FLAG;
        m.sender = mpi_rank;
        m.data[0] = ZAGRYCHA;
        send_msg(&m,ofiary[1],PARTY_TAG);

    }

    // wysylanie informacji o sepieniu
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank || i == ofiary[0] ||i == ofiary[1])
            continue;

        if(party_tab[i-poeci_offset]){
            struct Msg m;
            m.flag = PARTY_INFO_FLAG;
            m.data[0] = SEPIENIE;
            send_msg(&m,i,PARTY_TAG);
        }

    }

    int party_time = rand_num(200,2000);

    // wysylanie czasu libacji
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank)
            continue;

        if(party_tab[i-poeci_offset]){
            struct Msg m;
            m.flag = PARTY_INFO_FLAG;
            m.data[0] = party_time;
            send_msg(&m,i,PARTY_TAG);
        }
    }

    // libacja
    printf("Trwa libacja %d przez %d\n",mpi_rank,party_time);
    wait(party_time);

    // oczekuje az procesy zakoncza libacje
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank)
            continue;

        if(party_tab[i-poeci_offset]){
            MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);
            pthread_mutex_lock( &lamportMut );
            lamport_clock= max(lamport_clock,rec.lamport)+1;
            pthread_mutex_unlock( &lamportMut );

            if(rec.flag!= PARTY_END_FLAG)
                printf("----- ERROR - Nieodpowiednia flaga party - end (%d) \n", rec.flag);
        }
    }
    printf("Otrzymalem potwierdzenie zakonczenia libacji od procesow\n");
    
    //zamowienie wolontariusza
    //sprzatanie();

    // wysylanie informacji o zakonczeniu imprezy
    for(int i = poeci_offset;i<mpi_size; i++)
    {
        if(i==mpi_rank)
            continue;

        if(party_tab[i-poeci_offset]){
            struct Msg m;
            m.flag = PARTY_END_FLAG;
            send_msg(&m,i,PARTY_TAG);
        }
    }

}

int party()
{
    MPI_Status status;
    struct Msg rec;
    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,party_creator, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );


    if(rec.flag == PARTY_INFO_FLAG)
    {
        printf("Ja %d otrzymalem potwierdzenie\n",mpi_rank);

    }
    else if(rec.flag == PARTY_END_FLAG)
    {
        return 0;
    }
    else
    {
        printf("-----ERROR - Nieodpowiedni tag w odebranej wiadomosci (PARTY_START)\n");
    }


    {
    struct Msg msg;

    msg.data[0] = alkohol;
    msg.data[1] = zagrycha;
    msg.data[2] = sepienie;

    msg.flag = PARTY_INFO_FLAG;

    send_msg(&msg,party_creator,PARTY_TAG);
    }



    // odbieranie funkcji pelnionej podczas imprezy

    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,party_creator, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    if(rec.flag == PARTY_INFO_FLAG)
    {
        if(rec.data[0]==ALKOHOL)
            alkohol++;
        else if(rec.data[0] == ZAGRYCHA)
            zagrycha++;
        else if(rec.data[0] == SEPIENIE)
            sepienie++;
        else
            printf("-----ERROR - Odebrano nieodpowiedni typ wykonywanej funkcji \n");
    }
    else
    {
        printf("-----ERROR - Nieodpowiedni tag w odebranej wiadomosci (PARTY_odbieranie_funkcji)\n");
    }


    // odbieranie czasu trwania imprezy

    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,party_creator, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    int party_time = 0;

    if(rec.flag == PARTY_INFO_FLAG)
    {
        party_time = rec.data[0];
    }
    else
    {
        printf("-----ERROR - Nieodpowiedni tag w odebranej wiadomosci (PARTY_odbieranie_czasu)\n");
    }

    // libacja
    printf("Trwa libacja %d przez %d\n",mpi_rank,party_time);
    wait(party_time);
    
    struct Msg msg;
    
    
    msg.flag = PARTY_END_FLAG; // 16;//
    msg.data[0] = 1;
    send_msg(&msg,party_creator,PARTY_TAG);


    printf("Ja %d czekam na iformacje ze pokoj posprzatany\n",mpi_rank);
    // czekam na iformacje ze pokoj posprzatany
    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,party_creator, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    if(rec.flag == PARTY_END_FLAG)
    {
        return 1;
    }
    else
    {
        printf("-----ERROR - Nieodpowiedni tag w odebranej wiadomosci (PARTY_Koniec)\n");
    }
    
    return 0;

}

void *poeta_main(void *ptr)
{

    printf("Poeta nr %d zaczyna\n",mpi_rank);

    while(2+2){
        if(mpi_rank == 4)//choice(30)
        {
            printf("%d zaprasza \n",mpi_rank - poeci_offset);
            pthread_mutex_lock( &stateMut );
            if(poeta_stan == IN_WAIT_STATUS)
            {
                poeta_stan = IN_PARTY_STATUS;
                pthread_mutex_unlock( &stateMut );

                if(send_invite())
                {
                    printf("Mozna zaczynac impreze. \n");
                    party_host();
                    //odpocznij_po_party();
                }
                else
                {
                    printf("Nie wystarczajaco gosci\n");
                    cancel_party();
                    
                }

                pthread_mutex_lock( &stateMut );
                poeta_stan = IN_WAIT_STATUS;
                pthread_mutex_unlock( &stateMut );
            }
            else
            {
                pthread_mutex_unlock( &stateMut );
            }

            if(party_tab != NULL)
            {
                free(party_tab);
                party_tab = NULL;   
            }

        }

        if(poeta_stan == IN_PARTY_STATUS) // mutex nie potrzebny bo z tego stanu i tak nie zmienia sie
        {
            int odb = party();
            printf("Ja %d koncze party\n",mpi_rank);
            //odpocznij_po_party();
            party_creator = -1;
            pthread_mutex_lock( &stateMut );
            poeta_stan = IN_WAIT_STATUS;
            pthread_mutex_unlock( &stateMut );
        }
        printf("Koniec obrotu %d moj stan to: %d\n", mpi_rank, poeta_stan);
        wait(2000);
    }
}

//mpirun -np 10 --oversubscribe xterm -e gdb -ex run -ex y a.out
