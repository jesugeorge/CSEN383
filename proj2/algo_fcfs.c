#include "scheduler.h"

void run_FCFS(Process *p, int count) {
    int time = 0;
    
    // Removed "&& time < TOTAL_QUANTA" from loop
    for (int i = 0; i < count; i++) {

        // CUTOFF: If time is up and this job hasn't started, stop.
        if (time >= TOTAL_QUANTA) break;

        // CPU waits until process arrives
        if (time < p[i].arrival_time) {
            time = p[i].arrival_time;
        }

        // Double check cutoff after idle
        if (time >= TOTAL_QUANTA) break;

        // Set start time once
        if (p[i].start_time == -1) {
            p[i].start_time = time;
        }

        // Run process to completion (Removed "&& time < TOTAL_QUANTA")
        while (p[i].remaining_time > 0) {
            if(time < TOTAL_QUANTA) p[i].history[time] = true; // Visuals only
            p[i].remaining_time--;
            time++;
        }

        p[i].finish_time = time;
    }
}