#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_JOBS 100
#define TOTAL_QUANTA 100

// --- Formatting Macros ---
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RESET   "\033[0m"

typedef struct {
    int id;             
    char name;          
    int arrival_time;   
    int run_time;       
    int priority;       
    
    // Dynamic State
    int remaining_time; 
    int start_time;     
    int finish_time;    
    
    // NEW: The process tracks its own execution history
    // This removes the need for 'time_chart' in the algo arguments
    bool history[TOTAL_QUANTA];

    // Stats
    int waiting_time;
    int turnaround_time;
    int response_time;
} Process;

typedef struct {
    double total_turnaround;
    double total_waiting;
    double total_response;
    double total_throughput;
    int valid_runs;
} SimulationStats;

// CLEANER SIGNATURE: No more 'char* time_chart'
typedef void (*AlgoFunc)(Process* p, int count);

// --- Algorithm Prototypes ---
void run_FCFS(Process *p, int count);
void run_SJF(Process *p, int count);
void run_SRT(Process *p, int count);
void run_RR(Process *p, int count);
void run_HPF_NonPreemptive(Process *p, int count);
void run_HPF_Preemptive(Process *p, int count);

#endif