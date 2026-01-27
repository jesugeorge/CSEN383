#include "scheduler.h"

#define NUM_QUEUES 4
#define AGING_LIMIT 5

typedef struct {
  Process *q[MAX_JOBS];
  int front, rear;
} Queue;

static void enqueue(Queue *q, Process *p) { q->q[q->rear++] = p; }

static Process *dequeue(Queue *q) {
  if (q->front == q->rear)
    return NULL;
  return q->q[q->front++];
}

static bool is_empty(Queue *q) { return q->front == q->rear; }

void run_HPF_Preemptive(Process *p, int count) {
  Queue queues[NUM_QUEUES] = {0};
  int time = 0, finished = 0;
  int wait_age[MAX_JOBS] = {0};

  for (int i = 0; i < count; i++) {
    p[i].remaining_time = p[i].run_time;
    p[i].start_time = -1;
    for (int t = 0; t < TOTAL_QUANTA; t++)
      p[i].history[t] = false;
  }

  while (finished < count) {

    // arrivals
    for (int i = 0; i < count; i++) {
      if (p[i].arrival_time == time) {
        enqueue(&queues[p[i].priority], &p[i]);
      }
    }

    Process *current = NULL;

    // pick highest priority ready process
    for (int pr = 0; pr < NUM_QUEUES; pr++) {
      if (!is_empty(&queues[pr])) {
        current = dequeue(&queues[pr]);
        break;
      }
    }

    if (current) {
      if (current->start_time == -1)
        current->start_time = time;

      current->history[time] = true;
      current->remaining_time--;

      // RR quantum = 1 → preempt immediately
      if (current->remaining_time == 0) {
        current->finish_time = time + 1;
        finished++;
      } else {
        enqueue(&queues[current->priority], current);
      }
    }

    // aging
    for (int pr = 1; pr < NUM_QUEUES; pr++) {
      for (int i = queues[pr].front; i < queues[pr].rear; i++) {
        Process *proc = queues[pr].q[i];
        wait_age[proc->id]++;
        if (wait_age[proc->id] >= AGING_LIMIT) {
          proc->priority--;
          enqueue(&queues[pr - 1], proc);
          wait_age[proc->id] = 0;
          queues[pr].q[i] = NULL;
        }
      }
    }

    time++;
  }

  // stats
  for (int i = 0; i < count; i++) {
    p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
    p[i].waiting_time = p[i].turnaround_time - p[i].run_time;
    p[i].response_time = p[i].start_time - p[i].arrival_time;
  }
}

#include <limits.h>

void run_HPF_NonPreemptive(Process *p, int count)
{
    int time = 0;
    int completed = 0;
    Process *current = NULL;

    // Aging counters: how many consecutive quanta waited at current priority level
    int wait_at_level[MAX_JOBS] = {0};

    /* ---- Initialize dynamic fields ---- */
    for (int i = 0; i < count; i++) {
        p[i].remaining_time = p[i].run_time;
        p[i].start_time = -1;
        p[i].finish_time = 0;
        p[i].waiting_time = 0;
        p[i].turnaround_time = 0;
        p[i].response_time = 0;

        for (int t = 0; t < TOTAL_QUANTA; t++) {
            p[i].history[t] = false;
        }
    }

    /* ---- Main simulation loop ---- */
    while (completed < count && time < TOTAL_QUANTA) {
        /* ---- Aging step: increment wait counters for ready-but-not-running jobs ---- */
        for (int i = 0; i < count; i++) {
            if (p[i].arrival_time <= time && p[i].remaining_time > 0) {
                if (current != &p[i]) {
                    wait_at_level[i]++;

                    if (wait_at_level[i] >= 5) {
                        if (p[i].priority > 0) p[i].priority--; // bump up
                        wait_at_level[i] = 0;                  // reset at new level
                    }
                } else {
                    // running, not waiting
                    wait_at_level[i] = 0;
                }
            }
        }

        /* Select next job if CPU is idle */
        if (current == NULL) {
            int best = -1;
            int best_priority = INT_MAX;
            int best_arrival = INT_MAX;
            int best_id = INT_MAX;

            for (int i = 0; i < count; i++) {
                if (p[i].arrival_time <= time &&
                    p[i].remaining_time > 0) {

                    if (p[i].priority < best_priority ||
                       (p[i].priority == best_priority &&
                        p[i].arrival_time < best_arrival) ||
                       (p[i].priority == best_priority &&
                        p[i].arrival_time == best_arrival &&
                        p[i].id < best_id)) {

                        best = i;
                        best_priority = p[i].priority;
                        best_arrival = p[i].arrival_time;
                        best_id = p[i].id;
                    }
                }
            }

            if (best != -1) {
                current = &p[best];

                if (current->start_time == -1) {
                    current->start_time = time;
                    current->response_time =
                        time - current->arrival_time;
                }
            }
        }

        /* Execute one quantum */
        if (current != NULL) {
            if (time < TOTAL_QUANTA) {
                current->history[time] = true;
            }

            current->remaining_time--;

            if (current->remaining_time == 0) {
                current->finish_time = time + 1;
                completed++;
                current = NULL;
            }
        }

        time++;
    }
}