#include "scheduler.h"

void run_FCFS(Process *p, int count) {
    int time = 0;
    int completed = 0;

    for (int i = 0; i < count && time < TOTAL_QUANTA; i++) {

        // CPU waits until process arrives
        if (time < p[i].arrival_time) {
            time = p[i].arrival_time;
        }

        // Set start time once
        if (p[i].start_time == -1) {
            p[i].start_time = time;
        }

        // Run process to completion
        while (p[i].remaining_time > 0 && time < TOTAL_QUANTA) {
            p[i].history[time] = true;
            p[i].remaining_time--;
            time++;
        }

        p[i].finish_time = time;
        completed++;
    }
}

