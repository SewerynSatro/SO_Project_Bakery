#include "common.h"
FILE *file_cashier;
static void evacuation_handler_cashier(int signo)
{
    (void)signo;
    inventory_flag = 0;
    evacuation_flag = 1;
    printf("[Cashier %d] SIGUSR1 -> ewakuacja\n", getpid());
    fprintf(file_cashier, "[Cashier %d] SIGUSR1 -> ewakuacja\n", getpid());
}

static void inventory_handler_cashier(int signo)
{
    (void)signo;
    inventory_flag = 1;
    printf("[Cashier %d] SIGUSR2 -> inwentaryzacja\n", getpid());
    fprintf(file_cashier, "[Cashier %d] SIGUSR2 -> inwentaryzacja\n", getpid());
}

static void handle_request_and_reply(int cashier_id, const struct msg_request *req, int sold[PRODUCTS])
{
    float sum_price=0.0f;
    int total_items=0;

    for(int i=0; i<req->count; i++){
        int pid = req->product_id[i];
        int qty = req->quantity[i];
        sum_price += pointer_shared_data->product_price[pid] * qty;
        sold[pid] += qty;
        total_items += qty;
    }

    struct msg_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.mtype       = req->client_pid;
    rep.total_price = sum_price;
    rep.total_items = total_items;

    if (msgsnd(req->reply_queue_id, &rep, sizeof(rep) - sizeof(long), 0) == -1) {
        perror("[Cashier] msgsnd reply");
    } else {
        printf("[Cashier %d] Obsłużono klienta %d: %d szt. => %.2f\n", cashier_id, req->client_pid, total_items, sum_price);
        fprintf(file_cashier, "[Cashier %d] Obsłużono klienta %d: %d szt. => %.2f\n", cashier_id, req->client_pid, total_items, sum_price);
    }
}

void cashier_routine(int cashier_id, int msqid_cashier)
{
    signal(SIGUSR1, evacuation_handler_cashier);
    signal(SIGUSR2, inventory_handler_cashier);

    file_cashier = fopen("cashiers.log", "a");
    if(!file_cashier){
        perror("fopen");
        exit(1);
    }

    printf("[Cashier %d] Start pracy (PID=%d)\n", cashier_id, getpid());
    fprintf(file_cashier, "[Cashier %d] Start pracy (PID=%d)\n", cashier_id, getpid());
    int sold[PRODUCTS];
    memset(sold, 0, sizeof(sold));

    while (!evacuation_flag) {
        if (sem_wait(&pointer_shared_data->store_open_lock) == -1) {
            if (errno == EINTR && evacuation_flag) {
                printf("[Cashier %d] Ewakuacja - przerywam store_open_lock.\n", cashier_id);
                fprintf(file_cashier, "[Cashier %d] Ewakuacja - przerywam store_open_lock.\n", cashier_id);
                break;
            }
            perror("[Cashier] sem_wait(store_open_lock)");
            break;
        }
        int is_open = pointer_shared_data->store_open;
        sem_post(&pointer_shared_data->store_open_lock);

        if (!is_open) {
            printf("[Cashier %d] Sklep zamknięty => sprawdzam kolejkę nowait.\n", cashier_id);
            fprintf(file_cashier, "[Cashier %d] Sklep zamknięty => sprawdzam kolejkę nowait.\n", cashier_id);

            struct msg_request req;
            ssize_t rcv_size = msgrcv(msqid_cashier, &req, sizeof(req) - sizeof(long), 0, IPC_NOWAIT);
            if (rcv_size == -1) {
                if (errno == ENOMSG) {
                    printf("[Cashier %d] Kolejka pusta i sklep zamknięty => kończę.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Kolejka pusta i sklep zamknięty => kończę.\n", cashier_id);
                    break;
                }
                if (errno == EINTR && evacuation_flag) {
                    printf("[Cashier %d] Ewakuacja podczas msgrcv.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Ewakuacja podczas msgrcv.\n", cashier_id);
                    break;
                }
                perror("[Cashier] msgrcv nowait (sklep zamknięty)");
                continue;
            }
            if (req.count == 0) {
                printf("[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                fprintf(file_cashier, "[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                break;
            }
            handle_request_and_reply(cashier_id, &req, sold);
            continue;
        }

        int semVal = 0;
        if (sem_getvalue(&pointer_shared_data->cashier_active[cashier_id], &semVal) == -1) {
            perror("[Cashier] sem_getvalue cashier_active");
            break;
        }

        if (semVal > 0) {
            struct msg_request req;
            ssize_t r = msgrcv(msqid_cashier, &req, sizeof(req) - sizeof(long), 0, 0);
            if (r == -1) {
                if (errno == EINTR && evacuation_flag) {
                    printf("[Cashier %d] Ewakuacja - przerwano msgrcv.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Ewakuacja - przerwano msgrcv.\n", cashier_id);
                    break;
                }
                if (errno == EIDRM) {
                    printf("[Cashier %d] Kolejka usunięta => koniec.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Kolejka usunięta => koniec.\n", cashier_id);
                    break;
                }
                perror("[Cashier] msgrcv (blokujące)");
                continue;
            }
            if (req.count == 0) {
                printf("[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                fprintf(file_cashier, "[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                break;
            }

            handle_request_and_reply(cashier_id, &req, sold);

        } else {
            struct msg_request req;
            ssize_t r = msgrcv(msqid_cashier, &req, sizeof(req) - sizeof(long), 0, IPC_NOWAIT);
            if (r == -1) {
                if (errno == ENOMSG) {
                    continue;
                }
                if (errno == EINTR && evacuation_flag) {
                    printf("[Cashier %d] Ewakuacja - przerwano msgrcv.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Ewakuacja - przerwano msgrcv.\n", cashier_id);
                    break;
                }
                if (errno == EIDRM) {
                    printf("[Cashier %d] Kolejka skasowana => koniec.\n", cashier_id);
                    fprintf(file_cashier, "[Cashier %d] Kolejka skasowana => koniec.\n", cashier_id);
                    break;
                }
                perror("[Cashier] msgrcv (nowait, kasa zamknięta)");
                continue;
            }

            if (req.count == 0) {
                printf("[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                fprintf(file_cashier, "[Cashier %d] Otrzymano sygnał zamknięcia (count=0)\n", cashier_id);
                break;
            }
            handle_request_and_reply(cashier_id, &req, sold);
        }
    } 

    int total_sold = 0;
    for (int i = 0; i < PRODUCTS; i++) {
        total_sold += sold[i];
    }

    if (!evacuation_flag && inventory_flag) {
        printf(GREEN "[Kasa %d, PID=%d] ***INWENTARYZACJA*** Sprzedano łącznie %d szt.\n" RESET, cashier_id, getpid(), total_sold);
        fprintf(file_cashier, "[Kasa %d, PID=%d] ***INWENTARYZACJA*** Sprzedano łącznie %d szt.\n", cashier_id, getpid(), total_sold);
        for(int i=0; i<PRODUCTS; i++){
            if(sold[i] != 0) {
                printf(LIGHTGREEN "   [Kasa %d, PID=%d] Produkt %d: %d szt.\n" RESET,cashier_id, getpid(), i, sold[i]);
                fprintf(file_cashier, "   [Kasa %d, PID=%d] Produkt %d: %d szt.\n" ,cashier_id, getpid(), i, sold[i]);
            }
        }
    }

    printf("[Cashier %d, PID=%d] Koniec.\n", cashier_id, getpid());
    fprintf(file_cashier, "[Cashier %d, PID=%d] Koniec.\n", cashier_id, getpid());
    fclose(file_cashier);
}
