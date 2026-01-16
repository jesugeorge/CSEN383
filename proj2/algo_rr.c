#include "scheduler.h"

#define QUANTUM 1

typedef struct { int items[100]; int head, tail; } Queue;
void init_q(Queue* q) { q->head = 0; q->tail = 0; }
int pop(Queue* q) { return q->items[q->head++]; }
void push(Queue* q, int v) { q->items[q->tail++] = v; }
bool empty(Queue* q) { return q->head == q->tail; }

// NO time_chart argument needed!
void run_RR(Process *p, int count) {
    int time = 0;
    int completed = 0;
    Queue q; init_q(&q);
    bool in_queue[MAX_JOBS] = {false};

    // Load initial
    for(int i=0; i<count; i++) {
        if(p[i].arrival_time == 0) { push(&q, i); in_queue[i] = true; }
    }

    while (time < TOTAL_QUANTA && completed < count) {
        if (!empty(&q)) {
            int idx = pop(&q);
            
            // Start Constraint
            if (p[idx].start_time == -1) {
                if (time >= 99) continue;
                p[idx].start_time = time;
            }

            // --- UPDATE PROCESS STATE ---
            p[idx].remaining_time--;
            p[idx].history[time] = true; // Mark that "I ran at this time"
            // -----------------------------
            
            time++;

            // Check Arrivals
            for(int i=0; i<count; i++) {
                if(p[i].arrival_time == time && !in_queue[i]) {
                    push(&q, i); in_queue[i] = true;
                }
            }

            if (p[idx].remaining_time > 0) push(&q, idx);
            else { p[idx].finish_time = time; completed++; }
        } else {
            time++;
            for(int i=0; i<count; i++) {
                if(p[i].arrival_time == time && !in_queue[i]) {
                    push(&q, i); in_queue[i] = true;
                }
            }
        }
    }
}