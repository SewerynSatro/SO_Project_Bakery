#include "common.h"

static void evacuation_handler_cashier(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Cashier %d] SIGUSR1 -> ewakuacja\n", getpid());
}
static void inventory_handler_cashier(int signo)
{
    (void)signo;
    inventory_flag = 1;
    printf("[Cashier %d] SIGUSR2 -> inwentaryzacja\n", getpid());
}

static void handle_request_and_reply(int cashier_id, struct msg_request *req, int sold[PRODUCTS])
{
    float sum_price=0.0f;
    int total_items=0;

    for(int i=0; i<req->count; i++){
        int pid = req->product_id[i];
        int qty = req->quantity[i];
        sum_price += pointer_shared_data->product_price[pid]*qty;
        sold[pid]+=qty;
        total_items+=qty;
    }

    struct msg_reply rep;
    rep.mtype = req->client_pid;
    rep.total_price = sum_price;
    rep.total_items = total_items;

    msgsnd(req->reply_queue_id, &rep, sizeof(rep)-sizeof(long),0);

    printf("[Kasa %d] Obsługa klienta %d: %d szt. => %.2f\n", cashier_id, req->client_pid, total_items, sum_price);
}

void cashier_routine(int cashier_id, int msqid_cashier)
{
    signal(SIGUSR1, evacuation_handler_cashier);
    signal(SIGUSR2, inventory_handler_cashier);

    printf("[Kasa %d, PID=%d] Start.\n", cashier_id, getpid());

    int sold[PRODUCTS];
    memset(sold,0,sizeof(sold));

    while(1){
        if(evacuation_flag){
            printf("[Kasa %d] Ewakuacja\n", cashier_id);
            break;
        }

        if(sem_wait(&pointer_shared_data->store_open_lock)==-1){
            if(errno==EINTR && evacuation_flag){
                printf("[Kasa %d] Ewakuacja\n", cashier_id);
                break;
            }
            perror("sem_wait store_open_lock");
        }
        int is_open = pointer_shared_data->store_open;
        sem_post(&pointer_shared_data->store_open_lock);

        if(is_open){
            struct msg_request req;
            ssize_t rcv_size = msgrcv(msqid_cashier, &req, sizeof(req)-sizeof(long),
                                      0, 0);
            if(rcv_size==-1){
                if(errno==EINTR && evacuation_flag){
                    printf("[Kasa %d] Ewakuacja\n", cashier_id);
                    break;
                }
                continue;
            }
            handle_request_and_reply(cashier_id, &req, sold);
        } else {
            struct msg_request req;
            ssize_t rcv_size = msgrcv(msqid_cashier, &req, sizeof(req)-sizeof(long),
                                      0, IPC_NOWAIT);
            if(rcv_size==-1){
                if(errno==ENOMSG){
                    printf("[Kasa %d] Kolejka pusta i sklep zamknięty\n", cashier_id);
                    break;
                }
                if(errno==EINTR && evacuation_flag){
                    printf("[Kasa %d] Ewakuacja\n", cashier_id);
                    break;
                }
                continue;
            }
            if(req.count==0){
                printf("[Kasa %d] Odebrano wiadomość on managera że zamykamy\n", cashier_id);
                break;
            }
            handle_request_and_reply(cashier_id, &req, sold);
        }
    }

    int total_sold=0;
    for(int i=0; i<PRODUCTS; i++){
        total_sold += sold[i];
    }

    if(evacuation_flag){
    } else if(inventory_flag){
        printf("[Kasa %d, PID=%d] ***INWENTARYZACJA*** Sprzedano łącznie %d szt.\n",
               cashier_id, getpid(), total_sold);
        for(int i=0;i<PRODUCTS;i++){
            printf("   Product %d: %d szt.\n", i, sold[i]);
        }
    }
    printf("[Kasa %d, PID=%d] Koniec.\n", cashier_id, getpid());
}
