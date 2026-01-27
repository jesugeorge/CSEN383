#include "scheduler.h"

#define QUANTUM 1

// FIX: tail acts as 'count'. 'head' is unused (always 0).
typedef struct { int items[100]; int head, tail; } Queue;
void init_q(Queue* q) { q->head = 0; q->tail = 0; }

void push(Queue* q, int v) { 
    if(q->tail < 100) q->items[q->tail++] = v; 
}

// FIX: Shift elements left on pop to prevent overflow
int pop(Queue* q) {
    if(q->tail == 0) return -1;
    int v = q->items[0];
    for(int i=0; i < q->tail - 1; i++) {
        q->items[i] = q->items[i+1];
    }
    q->tail--;
    return v;
}

bool empty(Queue* q) { return q->tail == 0; }

void run_RR(Process *p, int count) {
    int time = 0;
    int completed = 0;
    Queue q; init_q(&q);
    bool in_queue[MAX_JOBS] = {false};

    // Load initial
    for(int i=0; i<count; i++) {
        if(p[i].arrival_time == 0) { push(&q, i); in_queue[i] = true; }
    }

    // FIX: Removed "time < TOTAL_QUANTA"
    while (completed < count) {
        if (!empty(&q)) {
            // Peek to check Start Cutoff
            int idx = q.items[0];
            
            // CUTOFF: If new job and time >= 100, drop it completely
            if (p[idx].start_time == -1 && time >= TOTAL_QUANTA) {
                pop(&q); // Remove
                if (empty(&q) && time >= TOTAL_QUANTA) break; // Stop if done
                continue; 
            }
            
            idx = pop(&q); // Valid pop
            
            if (p[idx].start_time == -1) {
                p[idx].start_time = time;
            }

            p[idx].remaining_time--;
            if (time < TOTAL_QUANTA) p[idx].history[time] = true; 
            
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
            // Idle
            if(time >= TOTAL_QUANTA) break; // Stop if idle past 100
            time++;
            for(int i=0; i<count; i++) {
                if(p[i].arrival_time == time && !in_queue[i]) {
                    push(&q, i); in_queue[i] = true;
                }
            }
        }
    }
}