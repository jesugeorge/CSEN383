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
        // Reset history using the constant
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
            workload[i].arrival_time = rand() % 100; // 0-99
            workload[i].run_time = (rand() % 10) + 1;
            workload[i].priority = (rand() % 4) + 1; // 1-4
        }
        qsort(workload, count, sizeof(Process), compare_arrival);
        
        if (is_workload_valid(workload, count)) valid = true;
        else count++;
    }
    return count;
}

// Generates timeline only up to TOTAL_QUANTA (100) to respect array bounds
int generate_timeline_string(Process *p, int count, char *buffer) {
    int max_finish = 0;
    for (int i = 0; i < count; i++) {
        if (p[i].finish_time > max_finish) max_finish = p[i].finish_time;
    }
    
    // Fill buffer up to 100
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
    return max_finish;
}

void export_gantt_csv(const char* algo_name, int run_id, const char* time_chart) {
    char filename[64];
    sprintf(filename, "gantt_%s_run%d.csv", algo_name, run_id + 1);
    FILE *f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "Job,Start,End\n");

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

// Verbose output compliant with Source 39
// UPDATED: Now takes 'original_workload' to calculate stats based on INITIAL priority
void print_run_details(Process *p, Process *original_workload, int count, const char *algo_name, int run_id, const char *timeline, int actual_end_time) {
    printf("Run #%d: %s\n", run_id + 1, algo_name);
    printf("Timeline: %s\n", timeline);
    printf("------------------------------------------------------------------------\n");
    printf("%-5s %-9s %-9s %-9s %-9s %-9s %-9s\n", "Name", "Arrival", "Burst", "Prio", "TAT", "Wait", "Resp");
    
    double total_tat = 0, total_wait = 0, total_resp = 0;
    int executed = 0;
    
    // HPF Stats Buckets
    double q_tat[5]={0}, q_wait[5]={0}, q_resp[5]={0};
    int q_count[5]={0};

    // Use max_finish_time for throughput calc
    double actual_duration = (actual_end_time > TOTAL_QUANTA) ? (double)actual_end_time : (double)TOTAL_QUANTA;

    for(int i=0; i<count; i++) {
        // IMPORTANT: Use ORIGINAL priority for grouping (ignoring aging changes)
        int initial_prio = original_workload[i].priority;

        printf("%-5c %-9d %-9d %-9d ", p[i].name, p[i].arrival_time, p[i].run_time, initial_prio);
        
        if (p[i].finish_time > 0) {
            printf("%-9d %-9d %-9d\n", p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
            
            total_tat += p[i].turnaround_time;
            total_wait += p[i].waiting_time;
            total_resp += p[i].response_time;
            executed++;
            
            // Collect HPF stats based on INITIAL priority
            if(initial_prio >= 1 && initial_prio <= 4) {
                q_tat[initial_prio] += p[i].turnaround_time;
                q_wait[initial_prio] += p[i].waiting_time;
                q_resp[initial_prio] += p[i].response_time;
                q_count[initial_prio]++;
            }
        } else {
             printf("[DROPPED]\n"); 
        }
    }
    
    if (executed > 0) {
        printf("------------------------------------------------------------------------\n");
        printf("RUN SUMMARY: Avg TAT: %.2f | Avg Wait: %.2f | Avg Resp: %.2f | Throughput: %.2f\n",
            total_tat/executed, total_wait/executed, total_resp/executed, (double)executed/actual_duration); 
    }
    
    if (strncmp(algo_name, "HPF", 3) == 0) {
        printf("HPF QUEUE STATS (Based on Initial Priority):\n");
        for(int q=1; q<=4; q++) {
             if(q_count[q] > 0) {
                 printf("  [P%d] Count: %d | Avg TAT: %.2f | Avg Wait: %.2f | Throughput: %.2f\n", 
                     q, q_count[q], q_tat[q]/q_count[q], q_wait[q]/q_count[q], (double)q_count[q]/actual_duration); 
             } else {
                 printf("  [P%d] Count: 0  | [STARVATION DETECTED]\n", q); 
             }
        }
    }
    printf("\n");
}

void run_simulation_step(const char* name, int run_id, AlgoFunc func, Process* workload, int count, SimulationStats *stats, bool export_csv) {
    Process p[MAX_JOBS];
    reset_processes(p, workload, count);
    
    func(p, count); // Algorithm runs

    char time_chart[TOTAL_QUANTA + 1];
    int actual_end_time = generate_timeline_string(p, count, time_chart);
    if (actual_end_time < 100) actual_end_time = 100;

    double sum_tat = 0, sum_wt = 0, sum_rt = 0;
    int completed = 0;

    for (int i = 0; i < count; i++) {
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

    double actual_duration = (double)actual_end_time;

    if (completed > 0) {
        stats->total_turnaround += (sum_tat / completed);
        stats->total_waiting += (sum_wt / completed);
        stats->total_response += (sum_rt / completed);
        stats->total_throughput += ((double)completed / actual_duration);
        stats->valid_runs++;
    }

    // Pass 'workload' (original data) for correct stats grouping
    print_run_details(p, workload, count, name, run_id, time_chart, actual_end_time);

    if (export_csv) {
        export_gantt_csv(name, run_id, time_chart);
    }
}

int main(int argc, char *argv[]) {
    bool enable_csv = false;
    if (argc > 1 && strcmp(argv[1], "-csv") == 0) {
        enable_csv = true;
    }

    const char* names[] = {"FCFS", "SJF", "SRT", "RR", "HPF-NP", "HPF-Pre"};
    AlgoFunc funcs[] = {run_FCFS, run_SJF, run_SRT, run_RR, run_HPF_NonPreemptive, run_HPF_Preemptive};
    SimulationStats stats[6] = {0};

    int base_seed = time(NULL);
    printf("BASE SEED: %d\n\n", base_seed);

    for (int run = 0; run < NUM_RUNS; run++) {
        Process workload[MAX_JOBS];
        int count = generate_workload(workload, base_seed + run);
        for (int i = 0; i < 6; i++) {
            run_simulation_step(names[i], run, funcs[i], workload, count, &stats[i], enable_csv);
        }
    }

    printf("\n[==========] Final Statistics (Average over 5 runs) [==========]\n");
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
    return 0;
}