#include "common.h"
FILE *file_manager;
static void evacuation_handler_manager(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Manager] SIGUSR1 -> ewakuacja_flag=1\n");
    fprintf(file_manager, "[Manager] SIGUSR1 -> ewakuacja_flag=1\n");
}
static void inventory_handler_manager(int signo)
{
    (void)signo;
    inventory_flag = 1;
    printf("[Manager] SIGUSR2 -> inventory_flag=1\n");
    fprintf(file_manager, "[Manager] SIGUSR2 -> inventory_flag=1\n");
}
static void open_or_close_cashiers(void)
{
    sem_wait(&pointer_shared_data->open_cashiers_sem);
    int val;
    if (sem_getvalue(&pointer_shared_data->store_capacity_sem, &val) == -1) {
        perror("[Manager] sem_getvalue store_capacity_sem");
        return;
    }
    int current_in_store = STORE_CAPACITY - val;
    int K = (STORE_CAPACITY / 3);
    int cashier_open = 1;
    if (current_in_store >= K){
        cashier_open = 2;
    }
    if (current_in_store >= 2 * K){
        cashier_open = 3;
    }

    pointer_shared_data->open_cashiers = cashier_open;

    for (int i = 1; i < 3; i++) {
        int valSem = 0;
        sem_getvalue(&pointer_shared_data->cashier_active[i], &valSem);

        if ( (i+1) <= cashier_open && valSem == 0 ) {
            sem_post(&pointer_shared_data->cashier_active[i]);
            printf("[Manager] Otwarto kasę nr %d.\n", i);
            fprintf(file_manager, "[Manager] Otwarto kasę nr %d.\n", i);
        }
        else if ( (i+1) > cashier_open && valSem > 0 ) {
            if (sem_wait(&pointer_shared_data->cashier_active[i]) == 0) {
                printf("[Manager] Zamknięto kasę nr %d.\n", i);
                fprintf(file_manager, "[Manager] Zamknięto kasę nr %d.\n", i);
            }
        }
    }
    sem_post(&pointer_shared_data->open_cashiers_sem);
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

    char filename[64];
    snprintf(filename, sizeof(filename), "manager-%lld.log", (long long)start_time);
    file_manager = fopen(filename, "a");
    if(!file_manager){
        perror("fopen");
        exit(1);
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
            if ((rand() % 1000)==1) {
                printf("[Manager %d] Inwentaryzacja! -> SIGUSR2\n", getpid());
                fprintf(file_manager, "[Manager %d] Inwentaryzacja! -> SIGUSR2\n", getpid());
                kill(0, SIGUSR2);
            }
        }

        if ((rand() % 100000000000)==1) {
            if (sem_wait(&sh->store_open_lock)==-1) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
            sh->store_open=0;
            sem_post(&sh->store_open_lock);

            printf("[Manager %d] Ewakuacja! -> SIGUSR1\n", getpid());
            fprintf(file_manager, "[Manager %d] Ewakuacja! -> SIGUSR1\n", getpid());
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
            fprintf(file_manager, "[Manager] Koniec dniówki => sklep zamknięty.\n");
            break;
        }

        open_or_close_cashiers();

    }

    if(evacuation_flag){

    } else if(inventory_flag){
        printf(VIOLET "[Manager] ***INWENTARYZACJA*** Stan końcowy podajników:" RESET "\n");
        fprintf(file_manager, "[Manager] ***INWENTARYZACJA*** Stan końcowy podajników:\n");
        for(int i=0; i<PRODUCTS; i++){
            printf(LIGHTVIOLET "   [Manager %d] Podajnik %d: %d / %d" RESET "\n", getpid(), i, sh->shelf[i].count, sh->shelf[i].capacity);
            fprintf(file_manager, "   [Manager %d] Podajnik %d: %d / %d\n", getpid(), i, sh->shelf[i].count, sh->shelf[i].capacity);
        }
    }
    printf("[Manager %d] manager() exit.\n", getpid());
    fprintf(file_manager, "[Manager %d] manager() exit.\n", getpid());
    fclose(file_manager);
}

