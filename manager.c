#include "common.h"

static void evacuation_handler_manager(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Manager] SIGUSR1 -> ewakuacja_flag=1\n");
}
static void inventory_handler_manager(int signo)
{
    (void)signo;
    inventory_flag = 1;
    printf("[Manager] SIGUSR2 -> inventory_flag=1\n");
}

void manager(int msqid_cashier[3])
{
    signal(SIGUSR1, evacuation_handler_manager);
    signal(SIGUSR2, inventory_handler_manager);

    struct shared_data *sh = pointer_shared_data;

    int hours_diff = T_k - T_p;     
    int seconds_per_hour = 15;     
    int total_simulation = hours_diff * seconds_per_hour;

    time_t start_time = time(NULL);
    if (start_time == (time_t)(-1)) {
        perror("time");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL) ^ getpid());

    while(!evacuation_flag) {
        time_t now = time(NULL);
        if (now == (time_t)(-1)) {
            perror("time");
            exit(EXIT_FAILURE);
        }
        time_t elapsed = now - start_time;

        if(!inventory_flag) {
            if ((rand() % 10000)==1) {
                printf("[Manager %d] Inwentaryzacja! -> SIGUSR2\n", getpid());
                kill(0, SIGUSR2);
            }
        }

        if ((rand() % 10000000000000)==1) {
            if (sem_wait(&sh->store_open_lock)==-1) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
            sh->store_open=0;
            sem_post(&sh->store_open_lock);

            printf("[Manager %d] Ewakuacja! -> SIGUSR1\n", getpid());
            inventory_flag=0;
            kill(0, SIGUSR1);
            evacuation_flag=1;
            break;
        }

        if(elapsed<total_simulation){

        } else {

            if (sem_wait(&sh->store_open_lock)==-1) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
            sh->store_open=0;
            sem_post(&sh->store_open_lock);


            struct msg_request close_msg;
            memset(&close_msg,0,sizeof(close_msg));
            close_msg.mtype=1;
            close_msg.count=0;
            for (int i=0;i<3;i++){
                msgsnd(msqid_cashier[i], &close_msg, sizeof(close_msg)-sizeof(long),0);
            }
            printf("[Manager] Koniec dniówki => sklep zamknięty.\n");
            break;
        }

    }

    if(evacuation_flag){

    } else if(inventory_flag){
        printf("[Manager] ***INWENTARYZACJA*** Stan końcowy podajników:\n");
        for(int i=0; i<PRODUCTS; i++){
            printf("   Podajnik %d: %d / %d\n",
                   i, sh->shelf[i].count, sh->shelf[i].capacity);
        }
    }
    printf("[Manager %d] manager() exit.\n", getpid());
}
