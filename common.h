#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>

#define STORE_CAPACITY 20
#define MAX_GENERATED_CLIENTS 1000
#define PRODUCTS 13
#define PRODUCTS_CAPACITY 50
#define T_p 8
#define T_k 10

#define RESET "\x1B[0m"
#define RED "\x1B[31m"
#define LIGHTRED "\x1B[91m"
#define GREEN "\x1B[32m"
#define LIGHTGREEN "\x1B[92m"
#define VIOLET  "\x1B[35m"
#define LIGHTVIOLET "\x1B[95m"
#define BLUE "\x1B[34m"
#define LIGHTBLUE "\x1B[94m"
#define WHITE "\x1B[37m"


struct ring_buffer {
    int head;        
    int tail;        
    int count;       
    int capacity;    
    int items[120]; 
};

void ring_buffer_init(struct ring_buffer *rb, int cap);
int  ring_buffer_push(struct ring_buffer *rb, int value);
int  ring_buffer_pop (struct ring_buffer *rb, int *out_value);

struct shared_data {
    sem_t store_open_lock;
    int store_open;

    sem_t store_capacity_sem;
    sem_t shelf_lock;

    int open_cashiers;

    sem_t cashier_active[3];
    sem_t open_cashiers_sem;

    float product_price[PRODUCTS];
    struct ring_buffer shelf[PRODUCTS];
    int baker_loaded[PRODUCTS];
};

struct msg_request {
    long mtype;
    pid_t client_pid;
    int reply_queue_id;
    int count;
    int product_id[PRODUCTS];
    int quantity[PRODUCTS];
};

struct msg_reply {
    long mtype;
    float total_price;
    int total_items;
};

extern struct shared_data *pointer_shared_data;
extern volatile sig_atomic_t evacuation_flag;
extern volatile sig_atomic_t inventory_flag;

void manager(int msqid_cashier[3]);
void baker_routine(void);
void cashier_routine(int cashier_id, int msqid_cashier);
void customer_routine(void);

#endif // COMMON_H
