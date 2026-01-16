#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "scheduler.h"

#define INITIAL_JOB_COUNT 10
#define MAX_IDLE_ALLOWANCE 2
#define NUM_RUNS 5

// --- Helper Functions ---

int compare_arrival(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->arrival_time != p2->arrival_time) return p1->arrival_time - p2->arrival_time;
    return p1->id - p2->id;
}

void reset_processes(Process *dest, Process *src, int count) {
    for (int i = 0; i < count; i++) {
        dest[i] = src[i];
        dest[i].remaining_time = src[i].run_time;
        dest[i].start_time = -1;
        dest[i].finish_time = 0;
        for(int t=0; t<TOTAL_QUANTA; t++) dest[i].history[t] = false;
        dest[i].waiting_time = 0;
        dest[i].turnaround_time = 0;
        dest[i].response_time = 0;
    }
}

// Ensure CPU is never idle for more than 2 consecutive quanta
bool is_workload_valid(Process *procs, int count) {
    int current_time = 0;
    for (int i = 0; i < count; i++) {
        if (procs[i].arrival_time > current_time) {
            if (procs[i].arrival_time - current_time > MAX_IDLE_ALLOWANCE) return false;
            current_time = procs[i].arrival_time;
        }
        current_time += procs[i].run_time;
    }
    return true;
}

// Generate Arrival 0-99, Run 1-10, Prio 1-4
int generate_workload(Process *workload, int seed) {
    srand(seed);
    int count = INITIAL_JOB_COUNT;
    bool valid = false;

    while (!valid) {
        for (int i = 0; i < count; i++) {
            workload[i].id = i + 1;
            workload[i].name = (i < 26) ? ('A' + i) : '?';
            workload[i].arrival_time = rand() % 100;
            workload[i].run_time = (rand() % 10) + 1;
            workload[i].priority = (rand() % 4) + 1;
        }
        qsort(workload, count, sizeof(Process), compare_arrival);
        
        // Verify idle time, increase count if needed
        if (is_workload_valid(workload, count)) valid = true;
        else count++;
    }
    return count;
}

void generate_timeline_string(Process *p, int count, char *buffer) {
    for (int t = 0; t < TOTAL_QUANTA; t++) {
        buffer[t] = '_';
        for (int i = 0; i < count; i++) {
            if (p[i].history[t]) {
                buffer[t] = p[i].name;
                break; 
            }
        }
    }
    buffer[TOTAL_QUANTA] = '\0';
}

// Export separate files for each run: gantt_FCFS_run1.csv
void export_gantt_csv(const char* algo_name, int run_id, const char* time_chart) {
    char filename[64];
    sprintf(filename, "gantt_%s_run%d.csv", algo_name, run_id + 1);
    
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "Job,Start,End\n");
    if (time_chart[0] == '\0') { fclose(f); return; }

    char current = time_chart[0];
    int start = 0;

    for (int t = 1; t <= TOTAL_QUANTA; t++) {
        char next = (t < TOTAL_QUANTA) ? time_chart[t] : 0;
        if (next != current) {
            if (current != ' ' && current != '_' && current != 0) {
                fprintf(f, "%c,%d,%d\n", current, start, t);
            }
            current = next;
            start = t;
        }
    }
    fclose(f);
}

void run_simulation_step(const char* name, int run_id, AlgoFunc func, Process* workload, int count, SimulationStats *stats, bool export_csv) {
    Process p[MAX_JOBS];
    reset_processes(p, workload, count);
    
    // 1. EXECUTE ALGORITHM
    func(p, count);

    // 2. GENERATE CHARTS
    char time_chart[TOTAL_QUANTA + 1];
    generate_timeline_string(p, count, time_chart);

    // 3. CALCULATE STATS
    double sum_tat = 0, sum_wt = 0, sum_rt = 0;
    int completed = 0;

    for (int i = 0; i < count; i++) {
        // Only include processes that actually ran
        if (p[i].finish_time > 0) {
            completed++;
            p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
            p[i].waiting_time = p[i].turnaround_time - p[i].run_time;
            p[i].response_time = p[i].start_time - p[i].arrival_time;

            sum_tat += p[i].turnaround_time;
            sum_wt += p[i].waiting_time;
            sum_rt += p[i].response_time;
        }
    }

    if (completed > 0) {
        stats->total_turnaround += (sum_tat / completed);
        stats->total_waiting += (sum_wt / completed);
        stats->total_response += (sum_rt / completed);
        stats->total_throughput += ((double)completed / TOTAL_QUANTA);
        stats->valid_runs++;
    }

    if (export_csv) {
        export_gantt_csv(name, run_id, time_chart);
    }
}

int main(int argc, char *argv[]) {
    // ARGUMENT PARSING: Check for -csv flag
    bool enable_csv = false;
    if (argc > 1 && strcmp(argv[1], "-csv") == 0) {
        enable_csv = true;
    }

    const char* names[] = {"FCFS", "SJF", "SRT", "RR", "HPF-NP", "HPF-Pre"};
    AlgoFunc funcs[] = {run_FCFS, run_SJF, run_SRT, run_RR, run_HPF_NonPreemptive, run_HPF_Preemptive};
    SimulationStats stats[6] = {0};

    printf("[==========] Scheduling Simulation (5 Runs)\n");
    if (enable_csv) printf("[ INFO     ] CSV Export ENABLED\n");
    else            printf("[ INFO     ] CSV Export DISABLED (Use -csv to enable)\n");
    
    int base_seed = time(NULL);
    printf("[ %-8s ] Base Seed: %d\n", "INFO", base_seed);
    printf("[----------] Starting Tests...\n");

    // Generate 5 sets of workloads. Run each algo 5 times.
    for (int run = 0; run < NUM_RUNS; run++) {
        printf("[ INFO     ] Processing Run %d/%d...\n", run + 1, NUM_RUNS);
        
        Process workload[MAX_JOBS];
        // Unique workload for each run (Seed + Run Index)
        int count = generate_workload(workload, base_seed + run);
        
        for (int i = 0; i < 6; i++) {
            run_simulation_step(names[i], run, funcs[i], workload, count, &stats[i], enable_csv);
        }
    }

    printf("[----------] Finished.\n");
    printf("\n[==========] Final Statistics (Average over 5 runs)\n");
    printf("%-10s %-12s %-12s %-12s %-12s\n", "Algorithm", "Avg TAT", "Avg Wait", "Avg Resp", "Throughput");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < 6; i++) {
        if (stats[i].valid_runs > 0) {
            double div = stats[i].valid_runs;
            printf("%-10s %-12.2f %-12.2f %-12.2f %-12.2f\n", 
                names[i],
                stats[i].total_turnaround / div,
                stats[i].total_waiting / div,
                stats[i].total_response / div,
                (stats[i].total_throughput / div) * 100.0
            );
        } else {
             printf("%-10s [ NO DATA ]\n", names[i]);
        }
    }
    printf("[==========] Done.\n");
    return 0;
}