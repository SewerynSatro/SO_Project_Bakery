#include "common.h"
FILE *file_customer;
static void evacuation_handler_customer(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Customer %d] SIGUSR1 -> ewakuacja\n", getpid());
    fprintf(file_customer, "[Customer %d] SIGUSR1 -> ewakuacja\n", getpid());
}

void customer_routine(void)
{
    signal(SIGUSR1, evacuation_handler_customer);
    signal(SIGUSR2, SIG_IGN);

    file_customer = fopen("customers.log", "a");
    if(!file_customer){
        perror("fopen");
        exit(1);
    }

    while(1){
        if(evacuation_flag){
            printf("[Klient %d] Ewakuacja - rezygnuję.\n", getpid());
            fprintf(file_customer, "[Klient %d] Ewakuacja - rezygnuję.\n", getpid());
            return;
        }
        if(sem_wait(&pointer_shared_data->store_capacity_sem)==-1){
            if(errno==EINTR){
                if(evacuation_flag){
                    printf("[Klient %d] Ewakuacja podczas czekania.\n", getpid());
                    fprintf(file_customer, "[Klient %d] Ewakuacja podczas czekania.\n", getpid());
                    return;
                }
                continue;
            }
            perror("sem_wait store_capacity_sem");
            return;
        }
        break;
    }

    if(sem_wait(&pointer_shared_data->store_open_lock)==-1){
        if(errno==EINTR && evacuation_flag){
            printf("[Klient %d] Ewakuacja - wychodzę.\n", getpid());
            fprintf(file_customer, "[Klient %d] Ewakuacja - wychodzę.\n", getpid());
            sem_post(&pointer_shared_data->store_capacity_sem);
            return;
        }
        perror("sem_wait store_open_lock");
        return;
    }
    int is_open = pointer_shared_data->store_open;
    sem_post(&pointer_shared_data->store_open_lock);

    if(!is_open || evacuation_flag){
        printf("[Klient %d] Sklep zamknięty/ewakuacja - odchodzę.\n", getpid());
        fprintf(file_customer, "[Klient %d] Sklep zamknięty/ewakuacja - odchodzę.\n", getpid());
        sem_post(&pointer_shared_data->store_capacity_sem);
        return;
    }

    printf("[Klient %d] Wchodzę do sklepu.\n", getpid());
    fprintf(file_customer, "[Klient %d] Wchodzę do sklepu.\n", getpid());

    srand(time(NULL) ^ (getpid()));
    int types_count = 2 + rand() % (PRODUCTS/4 -1);
    int chosen_types[PRODUCTS];
    int wanted[PRODUCTS];

    int pool[PRODUCTS];
    for(int i=0;i<PRODUCTS;i++){
        pool[i]=i;
    }
    for(int i=0;i<PRODUCTS;i++){
        int swap_idx = i + rand()%(PRODUCTS-i);
        int tmp=pool[i];
        pool[i]=pool[swap_idx];
        pool[swap_idx]=tmp;
    }
    for(int i=0;i<types_count;i++){
        chosen_types[i]=pool[i];
    }
    for(int i=0;i<types_count;i++){
        int prod_idx = chosen_types[i];
        int cap = pointer_shared_data->shelf[prod_idx].capacity;
        int want = 1 + rand() % (cap/2 > 0 ? cap : 1);
        wanted[i] = want;
    }

    // printf("[Klient %d] Lista zakupów (%d typów):\n", getpid(), types_count);
    // for(int i=0;i<types_count;i++){
    //     printf("   - Product %d: %d szt.\n", chosen_types[i], wanted[i]);
    // }

    if(sem_wait(&pointer_shared_data->shelf_lock)==-1){
        if(errno==EINTR && evacuation_flag){
            printf("[Klient %d] Ewakuacja podczas shelf_lock\n", getpid());
            fprintf(file_customer, "[Klient %d] Ewakuacja podczas shelf_lock\n", getpid());
            sem_post(&pointer_shared_data->store_capacity_sem);
            return;
        }
        perror("sem_wait shelf_lock");
        sem_post(&pointer_shared_data->store_capacity_sem);
        return;
    }

    int got[PRODUCTS]; 
    int had[PRODUCTS]; 
    memset(got,0,sizeof(got));
    memset(had,0,sizeof(had));

    int total_bought=0;
    for(int i=0;i<types_count;i++){
        int pid = chosen_types[i];
        had[i] = pointer_shared_data->shelf[pid].count;
        int want = wanted[i];

        if(had[i]>0){
            int to_take = (want <= had[i])? want: had[i];
            int pop_count=0;
            while(pop_count<to_take){
                if(ring_buffer_pop(&pointer_shared_data->shelf[pid], NULL)==0){
                    pop_count++;
                } else break;
            }
            got[i]=pop_count;
            total_bought+=pop_count;
        }
    }
    sem_post(&pointer_shared_data->shelf_lock);

    printf("[Klient %d] Szczegóły zakupów:\n", getpid());
    fprintf(file_customer, "[Klient %d] Szczegóły zakupów:\n", getpid());
    for(int i=0;i<types_count;i++){
        if(had[i]==0){
            printf("   - Produkt %d: Chciałem=%d, Mam=0 (podajnik pusty)\n", chosen_types[i], wanted[i]);
            fprintf(file_customer, "   - Produkt %d: Chciałem=%d, Mam=0 (podajnik pusty)\n", chosen_types[i], wanted[i]);
        } else {
            printf("   - Produkt %d: Chciałem=%d, Mam=%d\n", chosen_types[i], wanted[i], got[i]);
            fprintf(file_customer, "   - Produkt %d: Chciałem=%d, Mam=%d\n", chosen_types[i], wanted[i], got[i]);
        }
    }
    if(total_bought==0){
        printf("[Klient %d] Nie było produktów, których chciałem. Wychodzę.\n", getpid());
        fprintf(file_customer, "[Klient %d] Nie było produktów, których chciałem. Wychodzę.\n", getpid());
        sem_post(&pointer_shared_data->store_capacity_sem);
        return;
    }

    printf("[Klient %d] Mam łącznie %d szt. produktów.\n", getpid(), total_bought);
    fprintf(file_customer, "[Klient %d] Mam łącznie %d szt. produktów.\n", getpid(), total_bought);

    if(evacuation_flag){
        printf("[Klient %d] Ewakuacja - uciekam!\n", getpid());
        fprintf(file_customer, "[Klient %d] Ewakuacja - uciekam!\n", getpid());
        sem_post(&pointer_shared_data->store_capacity_sem);
        return;
    }

    if(sem_wait(&pointer_shared_data->open_cashiers_sem) ==-1){
        perror("[Client %d] sem_wait open_cashiers_sem");
    }
    int openC = pointer_shared_data->open_cashiers; 
    
    int chosen_cashier = 0;
    if(openC == 1) {
        chosen_cashier = 0;
    }
    else if(openC == 2) {
        chosen_cashier = rand() % 2; 
    }
    else {
        chosen_cashier = rand() % 3;
    }
    sem_post(&pointer_shared_data->open_cashiers_sem);
    
    int reply_qid = msgget(IPC_PRIVATE, IPC_CREAT|0666);
    if(reply_qid<0){
        perror("[Klient] msgget (reply_qid)");
        sem_post(&pointer_shared_data->store_capacity_sem);
        return;
    }

    struct msg_request req;
    memset(&req,0,sizeof(req));
    req.mtype=1;
    req.client_pid=getpid();
    req.reply_queue_id=reply_qid;
    req.count=types_count;

    for(int i=0;i<types_count;i++){
        req.product_id[i] = chosen_types[i];
        req.quantity[i]   = got[i];
    }

    key_t keyForCashier;
    if(chosen_cashier==0) keyForCashier=ftok("/tmp",101);
    else if(chosen_cashier==1) keyForCashier=ftok("/tmp",102);
    else keyForCashier=ftok("/tmp",103);

    int msqid_c = msgget(keyForCashier,0666);
    if(msqid_c<0){
        perror("[Klient] msgget cashier");
        sem_post(&pointer_shared_data->store_capacity_sem);
        msgctl(reply_qid, IPC_RMID, NULL);
        return;
    }

    msgsnd(msqid_c, &req, sizeof(req)-sizeof(long),0);
    printf("[Klient %d] Wysłałem prośbę o paragon do kasy %d\n", getpid(), chosen_cashier);
    fprintf(file_customer, "[Klient %d] Wysłałem prośbę o paragon do kasy %d\n", getpid(), chosen_cashier);

    struct msg_reply rep;
    if(msgrcv(reply_qid, &rep, sizeof(rep)-sizeof(long), getpid(), 0)==-1){
        perror("[Klient] msgrcv reply");
        sem_post(&pointer_shared_data->store_capacity_sem);
        msgctl(reply_qid, IPC_RMID, NULL);
        return;
    }

    printf("[Klient %d] Odebrałem paragon z kasy %d: %d szt., %.2f zł.\n", getpid(), chosen_cashier, rep.total_items, rep.total_price);
    fprintf(file_customer, "[Klient %d] Odebrałem paragon z kasy %d: %d szt., %.2f zł.\n", getpid(), chosen_cashier, rep.total_items, rep.total_price);

    msgctl(reply_qid, IPC_RMID, NULL);

    sem_post(&pointer_shared_data->store_capacity_sem);
    printf("[Klient %d] Zakończyłem zakupy i wychodzę.\n", getpid());
    fprintf(file_customer, "[Klient %d] Zakończyłem zakupy i wychodzę.\n", getpid());
    fclose(file_customer);
}
