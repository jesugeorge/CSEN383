#include "proj3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global variables
Venue venue;
Barrier barrier_start;
Barrier barrier_end;
SellerArgs sellers[NUM_SELLERS];

// ============================================================================
// Queue Functions
// ============================================================================

Queue *create_queue() {
  Queue *q = malloc(sizeof(Queue));
  q->front = q->rear = NULL;
  q->size = 0;
  return q;
}

void enqueue(Queue *q, Customer *c) {
  c->next = NULL;
  if (q->rear == NULL) {
    q->front = q->rear = c;
  } else {
    q->rear->next = c;
    q->rear = c;
  }
  q->size++;
}

Customer *dequeue(Queue *q) {
  if (q->front == NULL)
    return NULL;
  Customer *c = q->front;
  q->front = c->next;
  if (q->front == NULL)
    q->rear = NULL;
  q->size--;
  return c;
}

// ============================================================================
// Barrier Functions
// ============================================================================

void barrier_init(Barrier *b, int n) {
  b->limit = n;
  b->count = 0;
  b->crossing = 0;
  pthread_mutex_init(&b->mutex, NULL);
  pthread_cond_init(&b->cond, NULL);
}

void barrier_wait(Barrier *b) {
  pthread_mutex_lock(&b->mutex);
  int gen = b->crossing;
  b->count++;
  if (b->count >= b->limit) {
    b->crossing++;
    b->count = 0;
    pthread_cond_broadcast(&b->cond);
  } else {
    while (gen == b->crossing) {
      pthread_cond_wait(&b->cond, &b->mutex);
    }
  }
  pthread_mutex_unlock(&b->mutex);
}

// ============================================================================
// Venue Functions
// ============================================================================

void venue_init() {
  venue.seats_sold = 0;
  pthread_mutex_init(&venue.mutex, NULL);
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      strcpy(venue.seats[i][j], "-");
    }
  }
}

int sell_seat(char seller_type, int customer_id, int seller_id) {
  pthread_mutex_lock(&venue.mutex);

  if (venue.seats_sold >= 100) {
    pthread_mutex_unlock(&venue.mutex);
    return 0;
  }

  int row = -1, col = -1;

  if (seller_type == 'H') {
    // Front to back
    for (int i = 0; i < 10 && row == -1; i++) {
      for (int j = 0; j < 10 && row == -1; j++) {
        if (strcmp(venue.seats[i][j], "-") == 0) {
          row = i;
          col = j;
        }
      }
    }
  } else if (seller_type == 'M') {
    // Middle outward
    int order[] = {4, 5, 3, 6, 2, 7, 1, 8, 0, 9};
    for (int k = 0; k < 10 && row == -1; k++) {
      for (int j = 0; j < 10 && row == -1; j++) {
        if (strcmp(venue.seats[order[k]][j], "-") == 0) {
          row = order[k];
          col = j;
        }
      }
    }
  } else {
    // Back to front
    for (int i = 9; i >= 0 && row == -1; i--) {
      for (int j = 0; j < 10 && row == -1; j++) {
        if (strcmp(venue.seats[i][j], "-") == 0) {
          row = i;
          col = j;
        }
      }
    }
  }

  if (row != -1) {
    char seat_label[10];
    sprintf(seat_label, "%c%d:%02d", seller_type, seller_id + 1, customer_id);
    strcpy(venue.seats[row][col], seat_label);
    venue.seats_sold++;
  }

  pthread_mutex_unlock(&venue.mutex);
  return (row != -1);
}

// TODO: Implement print_chart() function
// Should print the 10x10 seating chart in a formatted way
void print_chart() {}

void log_msg(const char *msg) {
  pthread_mutex_lock(&venue.mutex);
  printf("%s\n", msg);
  pthread_mutex_unlock(&venue.mutex);
}

// ============================================================================
// Seller Functions
// ============================================================================

// Service Time generator
int get_service_time(char type) {
  if (type == 'H')
    return (rand() % 2) + 1; // 1–2
  if (type == 'M')
    return (rand() % 3) + 2; // 2–4
  return (rand() % 4) + 4;   // 4–7
}

// Creating buyers list for seller
void create_buyers_for_seller(Queue *q, char seller_type, int N) {
  for (int i = 0; i < N; i++) {
    Customer *c = malloc(sizeof(Customer));
    c->id = i + 1;
    c->arrival_time = rand() % MAX_MINUTES;
    c->service_time = get_service_time(seller_type);
    c->start_time = -1;
    c->finish_time = -1;
    c->next = NULL;

    enqueue(q, c);
  }
}

void *seller_thread(void *arg) {
  SellerArgs *s = (SellerArgs *)arg;
  Customer *current = NULL;
  int service_timer = 0;

  for (int minute = 0; minute < MAX_MINUTES; minute++) {
    barrier_wait(&barrier_start);

    // Try to serve new customer
    if (current == NULL && s->queue->front != NULL) {
      Customer *c = s->queue->front;
      if (c->arrival_time <= minute) {
        if (sell_seat(s->seller_type, c->id, s->seller_id)) {
          current = dequeue(s->queue);
          current->start_time = minute;
          service_timer = current->service_time;

          // TODO: Track statistics here (served count, response time)
          // Response time = current minute - arrival time

          char msg[100];
          sprintf(msg, "0:%02d Customer %c%d:%02d assigned seat.", minute,
                  s->seller_type, s->seller_id + 1, current->id);
          log_msg(msg);
          print_chart();
        } else {
          Customer *rejected = dequeue(s->queue);

          // TODO: Track turned away count here

          char msg[100];
          sprintf(msg, "0:%02d Customer %c%d:%02d turned away (Sold Out).",
                  minute, s->seller_type, s->seller_id + 1, rejected->id);
          log_msg(msg);
          free(rejected);
        }
      }
    }

    // Service current customer
    if (current != NULL) {
      service_timer--;
      if (service_timer == 0) {
        current->finish_time = minute;

        // TODO: Track turnaround time here
        // Turnaround time = current minute - arrival time

        char msg[100];
        sprintf(msg, "0:%02d Customer %c%d:%02d leaves.", minute,
                s->seller_type, s->seller_id + 1, current->id);
        log_msg(msg);
        print_chart();
        free(current);
        current = NULL;
      }
    }

    barrier_wait(&barrier_end);
  }

  return NULL;
}

// Initialize sellers
void initialize_sellers(SellerArgs sellers[], int N) {
  int i;

  // H1
  sellers[0].seller_id = 0;
  sellers[0].seller_type = 'H';
  sellers[0].queue = create_queue();
  create_buyers_for_seller(sellers[0].queue, 'H', N);

  // M1–M3
  for (i = 1; i <= 3; i++) {
    sellers[i].seller_id = i;
    sellers[i].seller_type = 'M';
    sellers[i].queue = create_queue();
    create_buyers_for_seller(sellers[i].queue, 'M', N);
  }

  // L1–L6
  for (i = 4; i < 10; i++) {
    sellers[i].seller_id = i;
    sellers[i].seller_type = 'L';
    sellers[i].queue = create_queue();
    create_buyers_for_seller(sellers[i].queue, 'L', N);
  }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <N>\n", argv[0]);
    exit(1);
  }

  srand(time(NULL));
  int N = atoi(argv[1]);

  // Initialize venue and barriers
  venue_init();
  barrier_init(&barrier_start, NUM_SELLERS);
  barrier_init(&barrier_end, NUM_SELLERS);

  // Initialize sellers
  initialize_sellers(sellers, N);

  // Create threads
  pthread_t threads[NUM_SELLERS];
  for (int i = 0; i < NUM_SELLERS; i++) {
    pthread_create(&threads[i], NULL, seller_thread, &sellers[i]);
  }

  // Wait for threads
  for (int i = 0; i < NUM_SELLERS; i++) {
    pthread_join(threads[i], NULL);
  }

  // TODO: Print final report here
  // - Total seats sold (from venue.seats_sold)
  // - Total customers turned away (sum from all sellers)
  // - Average response time (total response time / customers served)
  // - Average turnaround time (total turnaround time / customers served)
  // - Throughput (customers served / 60 minutes)
  int served = 0, turned = 0, resp = 0, turn = 0;

  for (int i = 0; i < NUM_SELLERS; i++) {
    served += sellers[i].served;
    turned += sellers[i].turned_away;
    resp += sellers[i].total_response;
    turn += sellers[i].total_turnaround;
  }

  printf("\n--- Final Report ---\n");
  printf("Seats sold: %d\n", venue.seats_sold);
  printf("Turned away: %d\n", turned);
  printf("Avg response time: %.2f\n", (float)resp / served);
  printf("Avg turnaround time: %.2f\n", (float)turn / served);
  printf("Throughput: %.2f customers/min\n", served / 60.0);

  printf("\nSimulation Complete.\n");

  return 0;
}
