#ifndef PROJ3_H
#define PROJ3_H

#include <pthread.h>

#define NUM_SELLERS 10
#define MAX_MINUTES 60

// Customer structure (teammate's design)
typedef struct Customer {
    int id;
    int arrival_time;
    int service_time;
    int start_time;
    int finish_time;
    struct Customer *next;
} Customer;

// FIFO Queue for each seller (teammate's design)
typedef struct {
    Customer *front;
    Customer *rear;
    int size;
} Queue;

// Seller Data Structure (teammate's design)
typedef struct {
    int seller_id;
    char seller_type;   // 'H', 'M', 'L'
    Queue *queue;
} SellerArgs;

// Venue structure
typedef struct {
    char seats[10][10][10];
    int seats_sold;
    pthread_mutex_t mutex;
} Venue;

// Barrier structure
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int crossing;
    int limit;
} Barrier;

// Global variables
extern Venue venue;
extern Barrier barrier_start;
extern Barrier barrier_end;
extern SellerArgs sellers[NUM_SELLERS];

#endif
