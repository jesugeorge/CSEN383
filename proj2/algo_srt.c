#include "scheduler.h"

static int pick_next_srt(Process *p, int n, int now) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (p[i].remaining_time <= 0) continue;
        if (p[i].arrival_time > now) continue;

        // FIX: Cutoff Rule inside selection
        if (p[i].start_time == -1 && now >= TOTAL_QUANTA) continue;

        if (best == -1) {
            best = i;
        } else if (p[i].remaining_time < p[best].remaining_time) {
            best = i;
        } else if (p[i].remaining_time == p[best].remaining_time) {
            if (p[i].arrival_time < p[best].arrival_time)
                best = i;
            else if (p[i].arrival_time == p[best].arrival_time && p[i].id < p[best].id)
                best = i;
        }
    }
    return best;
}

void run_SRT(Process *processes, int process_count) {
    int t = 0;
    int completed = 0;

    // FIX: Changed "for t < TOTAL" to "while completed < count"
    while (completed < process_count) {
        int idx = pick_next_srt(processes, process_count, t);
        
        if (idx == -1) { 
            if (t >= TOTAL_QUANTA) break; // Stop if idle past 100
            t++;
            continue; 
        }

        Process *cur = &processes[idx];

        if (cur->start_time < 0)
            cur->start_time = t;

        if (t < TOTAL_QUANTA) cur->history[t] = true; // Safe visual update
        cur->remaining_time--;

        if (cur->remaining_time == 0) {
            cur->finish_time = t + 1; // Logic depends on increment time
            completed++;
        }
        t++; // Increment time
    }
}