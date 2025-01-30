#include "common.h"
FILE *file_baker;
static void evacuation_handler_baker(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Baker %d] SIGUSR1 -> ewakuacja\n", getpid());
    fprintf(file_baker, "[Baker %d] SIGUSR1 -> ewakuacja\n", getpid());
}
static void inventory_handler_baker(int signo)
{
    (void)signo;
    inventory_flag = 1;
    printf("[Baker %d] SIGUSR2 -> inwentaryzacja\n", getpid());
    fprintf(file_baker, "[Baker %d] SIGUSR2 -> inwentaryzacja\n", getpid());
}

void baker_routine(void)
{
    signal(SIGUSR1, evacuation_handler_baker);
    signal(SIGUSR2, inventory_handler_baker);

    time_t now = time(NULL);
    char filename[64];
    snprintf(filename, sizeof(filename), "baker-%lld.log", (long long)now);
    file_baker = fopen(filename, "w");
    if(!file_baker){
        perror("fopen");
        exit(1);
    }

    struct shared_data *sh = pointer_shared_data;
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while(1) {
        if(evacuation_flag){
            printf("[Baker %d] Ewakuacja - kończę.\n", getpid());
            fprintf(file_baker, "[Baker %d] Ewakuacja - kończę.\n", getpid());
            break;
        }

        if(sem_wait(&sh->store_open_lock)==-1){
            if(errno==EINTR && evacuation_flag){
                printf("[Baker %d] Ewakuacja - przerwano sem_wait.\n", getpid());
                fprintf(file_baker, "[Baker %d] Ewakuacja - przerwano sem_wait.\n", getpid());
                break;
            }
            perror("sem_wait store_open_lock");
            break;
        }
        int is_open = sh->store_open;
        sem_post(&sh->store_open_lock);

        if(!is_open){
            printf("[Baker %d] Sklep zamknięty - kończę.\n", getpid());
            fprintf(file_baker, "[Baker %d] Sklep zamknięty - kończę.\n", getpid());
            break;
        }

        //sleep(1 + rand()%2);

        if(sem_wait(&sh->shelf_lock)==-1){
            if(errno==EINTR && evacuation_flag){
                printf("[Baker %d] Ewakuacja - przerwano shelf_lock.\n", getpid());
                fprintf(file_baker, "[Baker %d] Ewakuacja - przerwano shelf_lock.\n", getpid());
                break;
            }
            perror("sem_wait shelf_lock");
            break;
        }

        int i = rand() % PRODUCTS;
        int free_space = sh->shelf[i].capacity - sh->shelf[i].count;
        if(free_space>0){
            int inserted=0;
            for(int k=0; k<free_space; k++){
                if(ring_buffer_push(&sh->shelf[i], 1)==0){
                    inserted++;
                } else break;
            }
            if(inserted>0){
                sh->baker_loaded[i]+=inserted;
                printf("[Baker %d] Uzupełniono podajnik %d (+%d szt., %d/%d)\n", getpid(), i, inserted, sh->shelf[i].count, sh->shelf[i].capacity);
                fprintf(file_baker, "[Baker %d] Uzupełniono podajnik %d (+%d szt., %d/%d)\n", getpid(), i, inserted, sh->shelf[i].count, sh->shelf[i].capacity);
            }
        }

        sem_post(&sh->shelf_lock);
    }

    if(evacuation_flag){

    } else if(inventory_flag){
        printf(BLUE "[Baker %d] ***INWENTARYZACJA*** Raport wyprodukowanych:" RESET "\n", getpid());
        fprintf(file_baker, "[Baker %d] ***INWENTARYZACJA*** Raport wyprodukowanych:\n", getpid());
        for(int i=0; i<PRODUCTS; i++){
            printf(LIGHTBLUE "   [Baker %d] Podajnik %d: %d szt." RESET "\n",getpid(), i, sh->baker_loaded[i]);
            fprintf(file_baker, "   [Baker %d] Podajnik %d: %d szt.\n",getpid(), i, sh->baker_loaded[i]);
        }
    }
    printf("[Baker] baker_routine() koniec.\n");
    fprintf(file_baker, "[Baker] baker_routine() koniec.\n");
    fclose(file_baker);
    _exit(0);
}
