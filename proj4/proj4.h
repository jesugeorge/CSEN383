#ifndef PROJ4_H
#define PROJ4_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// Constants
// ===========================================================================

#define TOTAL_PAGES       100   // 100 physical page frames (1 MB each)
#define MAX_VIRT_PAGES    11    // virtual pages per process: 0..10
#define SIM_DURATION_MS   60000 // 1 minute in milliseconds
#define REF_INTERVAL_MS   100   // one page reference every 100 ms
#define TOTAL_TICKS       (SIM_DURATION_MS / REF_INTERVAL_MS) // 600 ticks
#define NUM_JOBS          150
#define NUM_RUNS          5
#define DETAILED_REFS     100   // print detailed records for first 100 refs

// Process sizes (in pages/MB)
static const int PROC_SIZES[]    = {5, 11, 17, 31};
#define NUM_PROC_SIZES    4

// Service durations (in seconds)
static const int PROC_DURATIONS[] = {1, 2, 3, 4, 5};
#define NUM_PROC_DURATIONS 5

// ===========================================================================
// Data Structures
// ===========================================================================

// Represents a process (job)
typedef struct Process {
    char name;                      // single-char identifier A-Z, a-z, 0-9...
    int  size;                      // number of pages in virtual address space
    int  arrival_ms;                // arrival time in ms
    int  duration_ms;               // service duration in ms
    int  remaining_ms;              // time left to run (ms)
    int  current_page;              // current virtual page being referenced
    int  active;                    // 1 if currently in memory, 0 otherwise
    int  page_table[MAX_VIRT_PAGES]; // maps virt page -> phys frame (-1 = not loaded)
    struct Process *next;           // linked list pointer
} Process;

// A physical page frame
typedef struct {
    int      occupied;   // 1 if occupied, 0 if free
    Process *owner;      // which process owns this frame (NULL if free)
    int      virt_page;  // which virtual page of the owner is stored here
    int      load_time;  // tick when page was loaded (for FIFO)
    int      last_used;  // tick when page was last referenced (for LRU)
    int      use_count;  // how many times referenced (for LFU/MFU)
} PageFrame;

// Job queue node (sorted linked list by arrival time)
typedef struct JobNode {
    Process *proc;
    struct JobNode *next;
} JobNode;

// ===========================================================================
// Algorithm enum
// ===========================================================================

typedef enum {
    ALG_FIFO,
    ALG_LRU,
    ALG_LFU,
    ALG_MFU,
    ALG_RANDOM,
    ALG_COUNT
} Algorithm;

static const char *ALG_NAMES[] = {
    "FIFO", "LRU", "LFU", "MFU", "Random"
};

// ===========================================================================
// Function pointer type for page replacement
// ===========================================================================

// Given a requesting process and its needed virtual page,
// return the physical frame index of the victim to evict.
typedef int (*ReplaceFn)(PageFrame frames[], int total_frames,
                         Process *requester, int vpage, int current_tick);

#endif // PROJ4_H
