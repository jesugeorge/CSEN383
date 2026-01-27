#include "scheduler.h"

#define NUM_QUEUES 4
#define AGING_LIMIT 5

typedef struct {
    Process *q[MAX_JOBS];
    int front, rear;
} Queue;

static void enqueue(Queue *q, Process *p) {
    if (q->rear < MAX_JOBS) { // prevent overflow
        q->q[q->rear++] = p;
    } else {
        fprintf(stderr, "Queue overflow\n");
        exit(1);
    }
}

static Process *dequeue(Queue *q) {
    while (q->front < q->rear) {
        Process *p = q->q[q->front++];
        if (p != NULL) return p; // skip NULLs
    }
    return NULL;
}

static bool is_empty(Queue *q) { return q->front == q->rear; }

void run_HPF_Preemptive(Process *p, int count) {
    Queue queues[NUM_QUEUES] = {0};
    int time = 0, finished = 0;
    int wait_age[MAX_JOBS] = {0};

    // Initialize processes
    for (int i = 0; i < count; i++) {
        p[i].remaining_time = p[i].run_time;
        p[i].start_time = -1;
        for (int t = 0; t < TOTAL_QUANTA; t++)
            p[i].history[t] = false;
    }

    while (finished < count && time < TOTAL_QUANTA) {

        // Add arriving processes to their priority queues
        for (int i = 0; i < count; i++) {
            if (p[i].arrival_time == time) {
                enqueue(&queues[p[i].priority], &p[i]);
            }
        }

        Process *current = NULL;

        // Pick highest priority ready process
        for (int pr = 0; pr < NUM_QUEUES; pr++) {
            if (!is_empty(&queues[pr])) {
                current = dequeue(&queues[pr]);
                break;
            }
        }

        if (current) {
            if (current->start_time == -1)
                current->start_time = time;

            if (time < TOTAL_QUANTA)
                current->history[time] = true;

            current->remaining_time--;

            // Preempt or finish
            if (current->remaining_time == 0) {
                current->finish_time = time + 1;
                finished++;
            } else {
                enqueue(&queues[current->priority], current);
            }
        }

        // Aging
        for (int pr = 1; pr < NUM_QUEUES; pr++) {
            for (int i = queues[pr].front; i < queues[pr].rear; i++) {
                Process *proc = queues[pr].q[i];
                if (!proc) continue; // skip NULL slots

                int idx = proc->id - 1; // 0-based
                wait_age[idx]++;
                if (wait_age[idx] >= AGING_LIMIT) {
                    proc->priority--;
                    if (proc->priority < 0) proc->priority = 0; // clamp
                    enqueue(&queues[pr - 1], proc);
                    wait_age[idx] = 0;
                    queues[pr].q[i] = NULL;
                }
            }
        }

        time++;
    }

    // Compute stats
    for (int i = 0; i < count; i++) {
        p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
        p[i].waiting_time = p[i].turnaround_time - p[i].run_time;
        p[i].response_time = p[i].start_time - p[i].arrival_time;
    }
}

void run_HPF_NonPreemptive(Process *processes, int process_count) {
    // Empty stub
}
