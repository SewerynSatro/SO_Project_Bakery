#include "common.h"
#include <pthread.h>

struct shared_data *pointer_shared_data = NULL;
volatile sig_atomic_t evacuation_flag = 0;
volatile sig_atomic_t inventory_flag = 0;
FILE *file_main;
void ring_buffer_init(struct ring_buffer *rb, int cap) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->capacity = cap;
    memset(rb->items, 0, sizeof(rb->items));
}
int ring_buffer_push(struct ring_buffer *rb, int value) {
    if (rb->count == rb->capacity) return -1;
    rb->items[rb->tail] = value;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return 0;
}
int ring_buffer_pop(struct ring_buffer *rb, int *out_value) {
    if (rb->count == 0) return -1;
    if (out_value) {
        *out_value = rb->items[rb->head];
    }
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return 0;
}

static void evacuation_handler_main(int signo)
{
    (void)signo;
    evacuation_flag = 1;
    printf("[Main] Otrzymano SIGUSR1 -> ewakuacja_flag=1\n");
    fprintf(file_main, "[Main] Otrzymano SIGUSR1 -> ewakuacja_flag=1\n");
}

void* wait_thread_func(void* arg)
{
    (void)arg;
    int reaped_count = 0;
    int status;
    pid_t wpid;

    while ((wpid = wait(&status)) > 0) {
        reaped_count++;
    }

    if (wpid == -1 && errno == ECHILD) {
        printf("[WaitThread] Nie ma więcej procesów potomnych. Reaped=%d\n", reaped_count);
        fprintf(file_main, "[WaitThread] Nie ma więcej procesów potomnych. Reaped=%d\n", reaped_count);
    } else if (wpid == -1) {
        perror("[WaitThread] wait");
    }
    printf("[WaitThread] Zakończono oczekiwanie na procesy (łącznie %d).\n", reaped_count);
    fprintf(file_main, "[WaitThread] Zakończono oczekiwanie na procesy (łącznie %d).\n", reaped_count);
    return NULL;
}


int main(void)
{
    signal(SIGUSR1, evacuation_handler_main);
    signal(SIGUSR2, SIG_IGN);

    time_t now = time(NULL);
    char filename[64];
    snprintf(filename, sizeof(filename), "main-%lld.log", (long long)now);
    file_main = fopen(filename, "a");
    if(!file_main){
        perror("fopen");
        exit(1);
    }

     if (STORE_CAPACITY < 1) {
        printf("BŁĄD: STORE_CAPACITY (%d) musi być >= 1\n", STORE_CAPACITY);
        fprintf(file_main, "BŁĄD: STORE_CAPACITY (%d) musi być >= 1\n", STORE_CAPACITY);
        exit(EXIT_FAILURE);
    }
    if (MAX_GENERATED_CLIENTS < 1) {
        printf("BŁĄD: MAX_GENERATED_CLIENTS (%d) musi być >= 1\n", MAX_GENERATED_CLIENTS);
        fprintf(file_main, "BŁĄD: MAX_GENERATED_CLIENTS (%d) musi być >= 1\n", MAX_GENERATED_CLIENTS);
        exit(EXIT_FAILURE);
    }
    if (PRODUCTS <= 12) {
        printf("BŁĄD: PRODUCTS (%d) musi być > 12\n", PRODUCTS);
        fprintf(file_main,"BŁĄD: PRODUCTS (%d) musi być > 12\n", PRODUCTS);
        exit(EXIT_FAILURE);
    }
    if (PRODUCTS_CAPACITY > 100 || PRODUCTS_CAPACITY < 1) {
        printf("BŁĄD: PRODUCTS_CAPACITY (%d) musi być <= 100 i >= 1\n", PRODUCTS_CAPACITY);
        fprintf(file_main, "BŁĄD: PRODUCTS_CAPACITY (%d) musi być <= 100 i >= 1\n", PRODUCTS_CAPACITY);
        exit(EXIT_FAILURE);
    }
    if ((T_k - T_p) < 1) {
        printf("BŁĄD: T_k (%d) - T_p (%d) musi być >= 1\n", T_k, T_p);
        fprintf(file_main, "BŁĄD: T_k (%d) - T_p (%d) musi być >= 1\n", T_k, T_p);
        exit(EXIT_FAILURE);
    }



    pointer_shared_data = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (pointer_shared_data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&pointer_shared_data->store_open_lock, 1, 1) == -1) {
        perror("sem_init store_open_lock");
        exit(EXIT_FAILURE);
    }
    pointer_shared_data->store_open = 1;

    if (sem_init(&pointer_shared_data->store_capacity_sem, 1, STORE_CAPACITY) == -1) {
        perror("sem_init store_capacity_sem");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&pointer_shared_data->shelf_lock, 1, 1) == -1) {
        perror("sem_init shelf_lock");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&pointer_shared_data->cashier_active[0], 1, 1) == -1) {
        perror("sem_init cashier_active[0]");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&pointer_shared_data->open_cashiers_sem, 1, 1) == -1) {
        perror("sem_init open_cashiers_sem");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&pointer_shared_data->cashier_active[1], 1, 0) == -1) {
        perror("sem_init cashier_active[1]");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&pointer_shared_data->cashier_active[2], 1, 0) == -1) {
        perror("sem_init cashier_active[2]");
        exit(EXIT_FAILURE);
    }


    for (int i = 0; i < PRODUCTS; i++) {
        pointer_shared_data->baker_loaded[i] = 0;
    }



    srand(time(NULL) ^ getpid());

    int base_capacity = PRODUCTS_CAPACITY;
    for (int i = 0; i < PRODUCTS; i++) {
        float rand_price = 1.0f + (float)(rand() % 1000)/100.0f;
        pointer_shared_data->product_price[i] = rand_price;

        float factor = 0.8f + 0.4f*(rand()/(float)RAND_MAX);
        int capacity = (int)(base_capacity*factor + 0.5f);
        if (capacity < 1) capacity = 1;

        ring_buffer_init(&pointer_shared_data->shelf[i], capacity);
        for(int k=0; k<capacity; k++){
            ring_buffer_push(&pointer_shared_data->shelf[i], 1);
        }
        pointer_shared_data->baker_loaded[i] += capacity;
        printf("[Main] Produkt %d: cena=%.2f, podajnik=%d (full)\n",i, rand_price, capacity);
        fprintf(file_main, "[Main] Produkt %d: cena=%.2f, podajnik=%d (full)\n",i, rand_price, capacity);
    }

    pid_t pid_baker = fork();
    if (pid_baker < 0) {
        perror("fork baker");
        exit(EXIT_FAILURE);
    } else if (pid_baker == 0) {
        baker_routine();
        _exit(0);
    }

    key_t keyA = ftok("/tmp", 101);
    key_t keyB = ftok("/tmp", 102);
    key_t keyC = ftok("/tmp", 103);
    if (keyA == -1 || keyB == -1 || keyC == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    int msqid_cashier[3];
    msqid_cashier[0] = msgget(keyA, IPC_CREAT|0666);
    msqid_cashier[1] = msgget(keyB, IPC_CREAT|0666);
    msqid_cashier[2] = msgget(keyC, IPC_CREAT|0666);
    if (msqid_cashier[0]<0 || msqid_cashier[1]<0 || msqid_cashier[2]<0) {
        perror("msgget cashier");
        exit(EXIT_FAILURE);
    }

    for (int i=0; i<3; i++){
        pid_t pid_casher = fork();
        if (pid_casher < 0) {
            perror("fork cashier");
            exit(EXIT_FAILURE);
        } else if (pid_casher == 0) {
            cashier_routine(i, msqid_cashier[i]);
            _exit(0);
        }
    }

    pid_t pid_manager = fork();
    if (pid_manager < 0) {
        perror("fork manager");
        exit(EXIT_FAILURE);
    } else if (pid_manager == 0) {
        manager(msqid_cashier);
        _exit(0);
    }

    int generated_count = 0;
    while (1) {
        if (generated_count >= MAX_GENERATED_CLIENTS) {
            printf("[Main] Osiągnięto limit klientów (%d) - kończę.\n", MAX_GENERATED_CLIENTS);
            fprintf(file_main, "[Main] Osiągnięto limit klientów (%d) - kończę.\n", MAX_GENERATED_CLIENTS);
            break;
        }
        if (evacuation_flag) {
            printf("[Main] Ewakuacja - przerywam tworzenie klientów.\n");
            fprintf(file_main, "[Main] Ewakuacja - przerywam tworzenie klientów.\n");
            break;
        }

        if (sem_wait(&pointer_shared_data->store_open_lock) == -1) {
            if (errno == EINTR && evacuation_flag) {
                printf("[Main] Ewakuacja - przerwano store_open_lock.\n");
                fprintf(file_main, "[Main] Ewakuacja - przerwano store_open_lock.\n");
                break;
            }
            perror("sem_wait store_open_lock");
            break;
        }
        int is_open = pointer_shared_data->store_open;
        sem_post(&pointer_shared_data->store_open_lock);

        if(!is_open){
            printf("[Main] Sklep zamknięty -> kończę tworzenie klientów.\n");
            fprintf(file_main, "[Main] Sklep zamknięty -> kończę tworzenie klientów.\n");
            break;
        }

        pid_t pid_cust = fork();
        if(pid_cust < 0){
            perror("fork customer");
            break;
        } else if (pid_cust == 0){
            customer_routine();
            _exit(0);
        }

        generated_count++;
        printf("[Main] Stworzono klienta nr %d (PID=%d)\n", generated_count, pid_cust);
        fprintf(file_main, "[Main] Stworzono klienta nr %d (PID=%d)\n", generated_count, pid_cust);
    }

    printf("[Main] Zakończono pętlę tworzenia klientów.\n");
    fprintf(file_main, "[Main] Zakończono pętlę tworzenia klientów.\n");
    pthread_t wait_thread;
    if (pthread_create(&wait_thread, NULL, wait_thread_func, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(wait_thread, NULL) != 0) {
        perror("pthread_join");
    }
    
    for(int i=0; i<3; i++){
        msgctl(msqid_cashier[i], IPC_RMID, NULL);
    }

    sem_destroy(&pointer_shared_data->store_open_lock);
    sem_destroy(&pointer_shared_data->store_capacity_sem);
    sem_destroy(&pointer_shared_data->shelf_lock);
    sem_destroy(&pointer_shared_data->cashier_active[0]);
    sem_destroy(&pointer_shared_data->cashier_active[1]);
    sem_destroy(&pointer_shared_data->cashier_active[2]);

    munmap(pointer_shared_data, sizeof(struct shared_data));

    printf("[Main] Koniec programu.\n");
    fprintf(file_main, "[Main] Koniec programu.\n");
    fclose(file_main);
    return 0;
}
