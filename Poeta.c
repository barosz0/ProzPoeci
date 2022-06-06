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

int choice(int perc) // taki niby bool
{
    if(perc >= 100)
        return 1;
    int los = rand_num(1,100);
    //printf("los : %d\n",los);

    return los <=perc;
}

void wait_till(int ms, int status)
{
    clock_t t;
    t = clock();
    
    double wait_time =  CLOCKS_PER_SEC * (ms/1000.0);

    while((clock() < (double)t+wait_time) && (poeta_stan != status));
}

void send_msg(struct Msg* msg, int destination, int tag)
{
    msg->sender = mpi_rank;
    pthread_mutex_lock( &lamportMut );
    lamport_clock+=1;
    msg->lamport = lamport_clock;
    pthread_mutex_unlock( &lamportMut );
    int ret = MPI_Send(msg,sizeof(*msg),MPI_BYTE,destination, tag, MPI_COMM_WORLD);

    //printf("Ja %d wyslalem wiadomosc do %d z tagiem %d z flaga %d efekt %d\n",mpi_rank,destination,tag,msg -> flag,ret);
}

void recive_invite(struct Msg m)
{
    struct Msg odp;
    odp.flag = INV_ODP_FLAG;

    pthread_mutex_lock( &stateMut );
    if(poeta_stan == IN_PARTY_STATUS)
    {
        odp.data[0] = 0;
        printf("[P][%d][%d] Odrzucam zaproszenie od %d - juz imprezuje\n",mpi_rank,lamport_clock,m.sender);
    }
    else
    {
        if(choice(PRAWDOPODOBIENSTWO_PRZYJECIA_ZAPROSZENIA))
        {    
            odp.data[0] = 1;
            printf("[P][%d][%d] Przyjalem zaproszenie od %d\n",mpi_rank,lamport_clock,m.sender);
            //change status na zaproszony
            poeta_stan = IN_PARTY_STATUS;
            party_creator = m.sender;
        }
        else
        {
            odp.data[0] = 0;
            printf("[P][%d][%d] Odrzucam zaproszenie od %d\n",mpi_rank,lamport_clock,m.sender);
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
    printf("[P][%d][%d] Startuje watek komunikacyjny",mpi_rank,lamport_clock);

    while(1+1){

        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, GLOBAL_TAG, MPI_COMM_WORLD, &status);
        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );

        //printf("Ja %d otrzymalem od %d flage %d\n",mpi_rank,rec.sender,rec.flag);
        switch (rec.flag)
        {
        case INV_FLAG:
            //printf("Otrzymana zaproszenie %d\n", mpi_rank);
            printf("[P][%d][%d] Otrzymalem zaproszenie od %d\n",mpi_rank,lamport_clock,rec.sender);
            recive_invite(rec);
        break;

        default:
            //printf("Odebrano wiadomosc od %d z flaga %d",rec.sender,rec.flag);
            break;
        }
    }
}

int send_invite()
{
    printf("[P][%d][%d] Wysylam zaproszenie na libacje\n",mpi_rank,lamport_clock);

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
        MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);
        //printf("ODP od %d %d %d\n",rec.sender,(int)rec.data[0],party_tab[i-poeci_offset]);
        pthread_mutex_lock( &lamportMut );
        lamport_clock= max(lamport_clock,rec.lamport)+1;
        pthread_mutex_unlock( &lamportMut );
        

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

    int count = 0;

    for(int i=0;i<mpi_size-poeci_offset;i++)
    {
        //printf("%d ",party_tab[i]);
        if(party_tab[i])
            count++;
    }
    //printf("count %d\n",count);
    printf("[P][%d][%d] Mam wszystkie odpowiedzi, przyjelo %d poetow\n",mpi_rank,lamport_clock,count);
    
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
    int time = rand_num(time_odpoczynek[0],time_odpoczynek[1]);
    printf("[P][%d][%d] Odpoczywam po libacji przez %d ms\n",mpi_rank,lamport_clock,time);
    wait(time);
    printf("[P][%d][%d] Koniec odpoczynku\n",mpi_rank,lamport_clock);
}

void cancel_party()
{
    printf("[P][%d][%d] Anuluje libacje\n",mpi_rank,lamport_clock);
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

    printf("[P][%d][%d] Zamawiam sprzatanie\n",mpi_rank,lamport_clock);
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
        for(int i=0;i<poeci_offset;i++)
        {

            MPI_Status status;
            struct Msg rec;
            MPI_Recv(&rec,sizeof(rec),MPI_BYTE,i, PARTY_TAG, MPI_COMM_WORLD, &status);

            pthread_mutex_lock( &lamportMut );
            lamport_clock= max(lamport_clock,rec.lamport)+1;
            pthread_mutex_unlock( &lamportMut );


            if(rec.data[0])
            {
                flag=0;
            }
        }

        if(flag){
            //printf("Nie udana proba zamownia sprzatania\n");
            wait(100); // zeby nie sypal wiadomosciami
        }
    }

    MPI_Status status;
    struct Msg rec;
    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    
    printf("[P][%d][%d] Do sprzatania zglosil sie %d zajmie mu to %d ms\n",mpi_rank,lamport_clock,rec.sender,(int)rec.data[0]);


    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,MPI_ANY_SOURCE, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    printf("[P][%d][%d] Otrzymalem informacje o zakonczeniu sprzatania\n",mpi_rank,lamport_clock);
}

void party_host()
{
    
    printf("[P][%d][%d] Wysylam potwierdzenie ze impreza sie odbedzie\n",mpi_rank,lamport_clock);
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

    printf("[P][%d][%d] Czekam na widomosci o procesow z informacja\n",mpi_rank,lamport_clock);

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
    
    printf("[P][%d][%d] Odebralem dane od procesow, teraz szukam ofiar\n",mpi_rank,lamport_clock);
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
    double stosunekA;
    double stosunekB;

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
        stosunekA = wartosci[ofiary[0]-poeci_offset][0]/(double)wartosci[ofiary[0]-poeci_offset][2];
    
    if(wartosci[ofiary[1]-poeci_offset][2]==0)
        stosunekB = 1;
    else
        stosunekB = wartosci[ofiary[1]-poeci_offset][0]/(double)wartosci[ofiary[1]-poeci_offset][2];


    //printf("Ofiary to %d %d\n", ofiary[0],ofiary[1]);

    
    
    
    if(stosunekB < stosunekA)
    {
        int pom = ofiary[1];
        ofiary[1] = ofiary[0];
        ofiary[0] = pom;
    }

    printf("[P][%d][%d] ofiary to %d i %d\n",mpi_rank,lamport_clock, ofiary[0], ofiary[1]);
    printf("[P][%d][%d] alkochol przynosi %d\n",mpi_rank,lamport_clock,ofiary[0]);
    printf("[P][%d][%d] zagryche przynosi %d\n",mpi_rank,lamport_clock,ofiary[1]);


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

    int party_time = rand_num(time_party[0],time_party[1]);

    printf("[P][%d][%d] Losuje czas libacji %d\n",mpi_rank,lamport_clock,party_time);

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
    printf("[P][%d][%d] Trwa libacja przez %d\n",mpi_rank,lamport_clock,party_time);
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
    printf("[P][%d][%d] Otrzymalem od wszystkich procesow informacje o zakonczeniu libacji\n",mpi_rank,lamport_clock);
    
    //zamowienie wolontariusza
    sprzatanie();

    printf("[P][%d][%d] Wysylam informacje o zakonczeniu sprzatania \n",mpi_rank,lamport_clock);

    // wysylanie informacji o zakonczeniu sprzatania i imprezy
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
        printf("[P][%d][%d] Otrzymalem potwierdzenie odbycia sie imprezy\n",mpi_rank,lamport_clock);

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
    printf("[P][%d][%d] Wysylam moje dane to organizatora |%d|%d|%d|\n",
                            mpi_rank,lamport_clock,alkohol,zagrycha,sepienie);

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
        char* funkcja = "NULL";
        if(rec.data[0]==ALKOHOL)
            {
                alkohol++;
                funkcja = "Alkochol";
            }
        else if(rec.data[0] == ZAGRYCHA)
            {
                zagrycha++;
                funkcja = "Zagrycha";
                
            }
        else if(rec.data[0] == SEPIENIE)
            {
                sepienie++;
                funkcja = "Nic";
            }
        else
            printf("-----ERROR - Odebrano nieodpowiedni typ wykonywanej funkcji \n");

        printf("[P][%d][%d] odebralem funkcje: %s moje dane:|%d|%d|%d|\n",
                            mpi_rank,lamport_clock,funkcja,alkohol,zagrycha,sepienie);
        
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
    printf("[P][%d][%d] Odebralem czas trwania libacji %d ms\n",mpi_rank,lamport_clock, party_time);

    // libacja
    printf("[P][%d][%d] Trwa libacja przez %d\n",mpi_rank,lamport_clock,party_time);
    wait(party_time);
    
    struct Msg msg;
    
    
    msg.flag = PARTY_END_FLAG; // 16;//
    msg.data[0] = 1;
    send_msg(&msg,party_creator,PARTY_TAG);


    printf("[P][%d][%d] Czekam na informacje o posprzataniu pokoju\n",mpi_rank,lamport_clock);
    // czekam na iformacje ze pokoj posprzatany
    MPI_Recv(&rec,sizeof(rec),MPI_BYTE,party_creator, PARTY_TAG, MPI_COMM_WORLD, &status);
    pthread_mutex_lock( &lamportMut );
    lamport_clock= max(lamport_clock,rec.lamport)+1;
    pthread_mutex_unlock( &lamportMut );

    if(rec.flag == PARTY_END_FLAG)
    {
        printf("[P][%d][%d] Odebralem informacje o posprzataniu pokoju\n",mpi_rank,lamport_clock);
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

    printf("[P][%d][%d] Startuje watek glowny\n",mpi_rank,lamport_clock);

    while(2+2){

        int wait_time = rand_num(time_miedzy_obrotami_poeta[0],time_miedzy_obrotami_poeta[1]);
        //printf("Czekam %d\n",wait_time);
        wait_till(wait_time,IN_PARTY_STATUS);

        if(choice(PRAWDOPODOBIENSTWO_ZAPROSZENIA)) //(mpi_rank == 4)//
        {
            
            pthread_mutex_lock( &stateMut );
            if(poeta_stan == IN_WAIT_STATUS)
            {
                poeta_stan = IN_PARTY_STATUS;
                pthread_mutex_unlock( &stateMut );

                if(send_invite())
                {
                    printf("[P][%d][%d] Zaproszenie przyjelo wystarczajaco gosci\n",mpi_rank,lamport_clock);
                    party_host();
                    odpocznij_po_party();
                }
                else
                {
                    printf("[P][%d][%d] Nie wystarczajaco gosci aby zaczac impreze\n",mpi_rank,lamport_clock);
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
            printf("[P][%d][%d] Koncze libacje\n",mpi_rank,lamport_clock);
            if(odb)
            {
                odpocznij_po_party();
            }
            party_creator = -1;
            pthread_mutex_lock( &stateMut );
            poeta_stan = IN_WAIT_STATUS;
            pthread_mutex_unlock( &stateMut );
        }
        printf("[P][%d][%d] Koncze obrot -- DEBUGHELP\n",mpi_rank,lamport_clock);
        // wait(2000); 
    }
}

//mpirun -np 10 --oversubscribe xterm -e gdb -ex run a.out
