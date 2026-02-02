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
// Statistics
// ============================================================================

typedef struct {
  long served;                 // customers who were assigned a seat / started service
  long finished;               // customers who completed service (left)
  long turned_away;            // customers rejected due to sold out
  long total_response_time;    // sum(start_time - arrival_time)
  long total_turnaround_time;  // sum(finish_time - arrival_time)
} TypeStats;

static TypeStats stats_H = {0};
static TypeStats stats_M = {0};
static TypeStats stats_L = {0};

static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static TypeStats *get_stats(char type) {
  if (type == 'H') return &stats_H;
  if (type == 'M') return &stats_M;
  return &stats_L;
}

// Fixed composition in your initialize_sellers()
#define NUM_H 1
#define NUM_M 3
#define NUM_L 6

static void print_type_report(const char *name, char type, int num_sellers_of_type) {
  TypeStats *ts = get_stats(type);

  double avg_resp = (ts->served > 0)
                      ? ((double)ts->total_response_time / (double)ts->served)
                      : 0.0;

  // Avg turnaround should be computed over FINISHED customers only
  double avg_tat = (ts->finished > 0)
                     ? ((double)ts->total_turnaround_time / (double)ts->finished)
                     : 0.0;

  // Throughput (customers per minute)
  // A) Assigned throughput: customers who were assigned a seat / started service
  double tp_assigned_type = (double)ts->served / (double)MAX_MINUTES;
  double tp_assigned_per_seller = tp_assigned_type / (double)num_sellers_of_type;

  // B) Finished throughput: customers who completed service (left)
  double tp_finished_type = (double)ts->finished / (double)MAX_MINUTES;
  double tp_finished_per_seller = tp_finished_type / (double)num_sellers_of_type;

  printf("\n[%s Sellers]\n", name);
  printf("  Served (assigned): %ld\n", ts->served);
  printf("  Finished (leaves): %ld\n", ts->finished);
  printf("  Turned Away: %ld\n", ts->turned_away);

  printf("  Avg Response Time (min/customer): %.2f\n", avg_resp);
  printf("  Avg Turnaround Time (min/customer, finished only): %.2f\n", avg_tat);

  printf("  Throughput Assigned (cust/min, type total): %.4f\n", tp_assigned_type);
  printf("  Throughput Assigned per seller (cust/min/seller, avg): %.4f\n", tp_assigned_per_seller);

  printf("  Throughput Finished (cust/min, type total): %.4f\n", tp_finished_type);
  printf("  Throughput Finished per seller (cust/min/seller, avg): %.4f\n", tp_finished_per_seller);
}



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

          // Stats: served count + response time
          long resp = (long)minute - (long)current->arrival_time;

          pthread_mutex_lock(&stats_mutex);
          TypeStats *ts = get_stats(s->seller_type);
          ts->served++;
          ts->total_response_time += resp;
          pthread_mutex_unlock(&stats_mutex);

          char msg[100];
          sprintf(msg, "0:%02d Customer %c%d:%02d assigned seat.", minute,
                  s->seller_type, s->seller_id + 1, current->id);
          log_msg(msg);
          print_chart();
        } else {
          Customer *rejected = dequeue(s->queue);

          // Stats: turned away count
          pthread_mutex_lock(&stats_mutex);
          TypeStats *ts = get_stats(s->seller_type);
          ts->turned_away++;
          pthread_mutex_unlock(&stats_mutex);

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

        // Stats: turnaround time
        long tat = (long)current->finish_time - (long)current->arrival_time;

        pthread_mutex_lock(&stats_mutex);
        TypeStats *ts = get_stats(s->seller_type);
        ts->finished++;
        ts->total_turnaround_time += tat;
        pthread_mutex_unlock(&stats_mutex);

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

  // Final report
  // - Total seats sold (from venue.seats_sold)
  // - Total customers who completed service (left)
  // - Total customers turned away (sum from all sellers)
  // - Average response time (total response time / customers served)
  // - Average turnaround time (total turnaround time / customers finished)
  // - Throughput Assigned (customers served / 60 minutes)
  // - Throughput Finished (customers finished / 60 minutes)

  // Final report (per seller type)
  printf("\n==================== Final Report ====================\n");
  printf("Total Seats Sold: %d\n", venue.seats_sold);

  // Per type stats (H/M/L)
  print_type_report("High", 'H', NUM_H);
  print_type_report("Medium", 'M', NUM_M);
  print_type_report("Low", 'L', NUM_L);

  // Overall totals (all types combined)
  long total_served = stats_H.served + stats_M.served + stats_L.served;
  long total_finished = stats_H.finished + stats_M.finished + stats_L.finished;
  long total_turned_away = stats_H.turned_away + stats_M.turned_away + stats_L.turned_away;

  long total_resp = stats_H.total_response_time + stats_M.total_response_time + stats_L.total_response_time;
  long total_tat = stats_H.total_turnaround_time + stats_M.total_turnaround_time + stats_L.total_turnaround_time;

  double overall_avg_resp = (total_served > 0) ? ((double)total_resp / (double)total_served) : 0.0;
  double overall_avg_tat = (total_finished > 0) ? ((double)total_tat / (double)total_finished) : 0.0;

  double overall_tp_assigned = (double)total_served / (double)MAX_MINUTES;
  double overall_tp_finished = (double)total_finished / (double)MAX_MINUTES;

  printf("\n[Overall]\n");
  printf("  Served (assigned): %ld\n", total_served);
  printf("  Finished (leaves): %ld\n", total_finished);
  printf("  Turned Away: %ld\n", total_turned_away);
  printf("  Avg Response Time (min/customer): %.2f\n", overall_avg_resp);
  printf("  Avg Turnaround Time (min/customer, finished only): %.2f\n", overall_avg_tat);
  printf("  Throughput Assigned (cust/min, total): %.4f\n", overall_tp_assigned);
  printf("  Throughput Finished (cust/min, total): %.4f\n", overall_tp_finished);

  printf("======================================================\n");

  printf("\nSimulation Complete.\n");

  return 0;
}
