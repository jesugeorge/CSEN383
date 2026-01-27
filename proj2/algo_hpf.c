#include "scheduler.h"

#define NUM_QUEUES 4
#define AGING_LIMIT 5

typedef struct {
    Process *q[MAX_JOBS];
    int front, rear; // rear acts as 'count' in shifting implementation
} Queue;

// FIX: Simple check to ensure we don't go out of bounds
static void enqueue(Queue *q, Process *p) {
    if (q->rear < MAX_JOBS) {
        q->q[q->rear++] = p;
    } else {
        // With shifting dequeue, this is unlikely unless we have >100 active jobs
        fprintf(stderr, "Queue overflow\n");
        exit(1);
    }
}

// FIX: Shifting Dequeue
// Moves all elements down so 'rear' doesn't grow forever.
// Keeps the array contiguous [0..rear], so your aging loop works safely.
static Process *dequeue(Queue *q) {
    if (q->front == q->rear) return NULL; // Empty

    Process *p = q->q[0]; // Always take from 0

    // Shift everyone left
    for (int i = 0; i < q->rear - 1; i++) {
        q->q[i] = q->q[i + 1];
    }
    
    q->rear--; // Decrease count
    // q->front stays 0
    return p;
}

static bool is_empty(Queue *q) { return q->rear == 0; }

void run_HPF_Preemptive(Process *p, int count) {
    Queue queues[NUM_QUEUES] = {0};
    int time = 0, finished = 0;
    int wait_age[MAX_JOBS] = {0};

    // Initialize processes
    for (int i = 0; i < count; i++) {
        p[i].remaining_time = p[i].run_time;
        p[i].start_time = -1;
        // Safety: Ensure priority is 1-4
        if(p[i].priority < 1) p[i].priority = 1;
        if(p[i].priority > 4) p[i].priority = 4;
        
        for (int t = 0; t < TOTAL_QUANTA; t++)
            p[i].history[t] = false;
    }

    // FIX: Removed "time < TOTAL_QUANTA" (Run to completion)
    while (finished < count) {

        // Add arriving processes
        for (int i = 0; i < count; i++) {
            if (p[i].arrival_time == time) {
                // FIX: Priority 1-4 maps to Index 0-3
                enqueue(&queues[p[i].priority - 1], &p[i]);
            }
        }

        Process *current = NULL;
        int active_q = -1;

        // Pick highest priority ready process
        for (int pr = 0; pr < NUM_QUEUES; pr++) {
            if (!is_empty(&queues[pr])) {
                // Peek first (don't remove yet until we know it's valid)
                current = queues[pr].q[0]; 
                
                // FIX: Start Cutoff Rule
                // If it's new and time >= 100, we must drop it.
                if (current->start_time == -1 && time >= TOTAL_QUANTA) {
                    dequeue(&queues[pr]); // Remove and discard
                    current = NULL; // Force loop to continue looking or idle
                    
                    // Note: In strict logic, we might need to check the next job immediately.
                    // But simpler to just break and let the next tick handle it if busy.
                    // Check if we exhausted this queue?
                    if (is_empty(&queues[pr])) continue;
                    else {
                        // If there are more jobs, we should check them now?
                        // For simplicity, we just decrement pr to retry this queue
                        pr--; 
                        continue;
                    }
                }
                
                // If valid, actually remove it
                current = dequeue(&queues[pr]);
                active_q = pr;
                break;
            }
        }

        if (current) {
            if (current->start_time == -1) {
                current->start_time = time;
                current->response_time = time - current->arrival_time;
            }

            if (time < TOTAL_QUANTA)
                current->history[time] = true;

            current->remaining_time--;

            // Preempt or finish
            if (current->remaining_time == 0) {
                current->finish_time = time + 1;
                finished++;
            } else {
                // FIX: Re-enqueue to same priority index
                enqueue(&queues[current->priority - 1], current);
            }
        } else {
            // Idle: If we are past 100 and idle, we are done
            if (time >= TOTAL_QUANTA) break;
        }

        // Aging (Your logic preserved, works with Shifting Queue)
        for (int pr = 1; pr < NUM_QUEUES; pr++) {
            // Since front is always 0 in shifting queue, iterate 0 to rear
            for (int i = 0; i < queues[pr].rear; i++) {
                Process *proc = queues[pr].q[i];
                if (!proc) continue;

                int idx = proc->id - 1; 
                wait_age[idx]++;
                if (wait_age[idx] >= AGING_LIMIT) {
                    // Reduce Priority (e.g. 2 -> 1)
                    if (proc->priority > 1) proc->priority--;
                    
                    // Move to higher priority queue
                    enqueue(&queues[pr - 1], proc);
                    wait_age[idx] = 0;
                    
                    // Remove from current queue by marking NULL? 
                    // No, shifting queue doesn't support holes easily.
                    // We must manually shift this one out or rebuild.
                    // SIMPLE FIX: Just mark NULL here and filter in dequeue?
                    // OR: Since this is specific, let's just accept the NULL slot logic
                    // you had, but updated for the new enqueue.
                    // Actually, 'enqueue' adds to rear. We can just shift the array here manually.
                    for(int k=i; k < queues[pr].rear - 1; k++) {
                        queues[pr].q[k] = queues[pr].q[k+1];
                    }
                    queues[pr].rear--;
                    i--; // Decrement i so we don't skip the next one
                }
            }
        }

        time++;
    }

    // Compute stats
    for (int i = 0; i < count; i++) {
        if(p[i].finish_time > 0) {
            p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
            p[i].waiting_time = p[i].turnaround_time - p[i].run_time;
            p[i].response_time = p[i].start_time - p[i].arrival_time;
        }
    }
}

#include <limits.h>

void run_HPF_NonPreemptive(Process *p, int count)
{
    int time = 0;
    int completed = 0;
    Process *current = NULL;
    int wait_at_level[MAX_JOBS] = {0};

    for (int i = 0; i < count; i++) {
        p[i].remaining_time = p[i].run_time;
        p[i].start_time = -1;
        p[i].finish_time = 0;
        p[i].waiting_time = 0;
        p[i].turnaround_time = 0;
        p[i].response_time = 0;
        for (int t = 0; t < TOTAL_QUANTA; t++) p[i].history[t] = false;
    }

    // FIX: Removed "time < TOTAL_QUANTA"
    while (completed < count) {
        
        // Aging Logic
        for (int i = 0; i < count; i++) {
            if (p[i].arrival_time <= time && p[i].remaining_time > 0) {
                if (current != &p[i]) {
                    wait_at_level[i]++;
                    if (wait_at_level[i] >= 5) {
                        if (p[i].priority > 1) p[i].priority--; // FIX: 1 is min
                        wait_at_level[i] = 0;
                    }
                } else {
                    wait_at_level[i] = 0;
                }
            }
        }

        if (current == NULL) {
            int best = -1;
            int best_priority = INT_MAX;
            int best_arrival = INT_MAX;
            int best_id = INT_MAX;

            for (int i = 0; i < count; i++) {
                if (p[i].arrival_time <= time && p[i].remaining_time > 0) {
                    
                    // FIX: Start Cutoff Rule
                    if (p[i].start_time == -1 && time >= TOTAL_QUANTA) continue;

                    if (p[i].priority < best_priority ||
                       (p[i].priority == best_priority && p[i].arrival_time < best_arrival) ||
                       (p[i].priority == best_priority && p[i].arrival_time == best_arrival && p[i].id < best_id)) {

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
                    current->response_time = time - current->arrival_time;
                }
            } else {
                // If idle and past 100, quit
                if (time >= TOTAL_QUANTA) break;
            }
        }

        if (current != NULL) {
            if (time < TOTAL_QUANTA) current->history[time] = true;
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