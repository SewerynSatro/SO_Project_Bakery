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
#define MAX_GENERATED_CLIENTS 50
#define PRODUCTS 13
#define PRODUCTS_CAPACITY 10
#define T_p 8
#define T_k 16


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
