#include "scheduler.h"

void run_SJF(Process *processes, int process_count) {
    int time = 0;
    int completed = 0;

    // Initialize runtime fields (Kept original loop)
    for (int i = 0; i < process_count; i++) {
        processes[i].remaining_time = processes[i].run_time;
        processes[i].start_time = -1;
        processes[i].finish_time = 0;
        for (int t = 0; t < TOTAL_QUANTA; t++) processes[i].history[t] = false;
    }

    // FIX: Removed "time < TOTAL_QUANTA"
    while (completed < process_count) {

        int idx = -1;
        int shortest = 999999;

        for (int i = 0; i < process_count; i++) {
            if (processes[i].remaining_time > 0 && processes[i].arrival_time <= time) {
                
                // FIX: Cutoff Rule
                if (processes[i].start_time == -1 && time >= TOTAL_QUANTA) continue;

                if (processes[i].remaining_time < shortest) {
                    shortest = processes[i].remaining_time;
                    idx = i;
                }
            }
        }

        // No job ready
        if (idx == -1) {
            if (time >= TOTAL_QUANTA) break; // Stop if idle past 100
            time++;
            continue;
        }

        if (processes[idx].start_time == -1) {
            processes[idx].start_time = time;
        }

        // Run job to completion
        while (processes[idx].remaining_time > 0) {
            if (time < TOTAL_QUANTA) {
                processes[idx].history[time] = true;
            }
            processes[idx].remaining_time--;
            time++;
        }

        processes[idx].finish_time = time;
        completed++;
    }
}