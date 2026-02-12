#include "proj4.h"

// ===========================================================================
// Output Helpers
// ===========================================================================

#define DIVIDER                                                                \
  "----------------------------------------------------------------------"
#define HEADER                                                                 \
  "======================================================================"

// ===========================================================================
// Global State
// ===========================================================================

static PageFrame frames[TOTAL_PAGES]; // physical memory
static Process all_procs[NUM_JOBS];   // all generated processes
// (active process count is tracked via active_list.count)

// ===========================================================================
// Name Generation (A-Z, a-z, 0-9, then wrap)
// ===========================================================================

static char gen_name(int index) {
  if (index < 26)
    return 'A' + index;
  if (index < 52)
    return 'a' + (index - 26);
  if (index < 62)
    return '0' + (index - 52);
  // wrap around with uppercase for overflow
  return 'A' + (index % 26);
}

// ===========================================================================
// Memory Map
// ===========================================================================

static void print_memory_map(void) {
  printf("Mem: [");
  for (int i = 0; i < TOTAL_PAGES; i++) {
    if (frames[i].occupied && frames[i].owner != NULL)
      printf("%c", frames[i].owner->name);
    else
      printf(".");
  }
  printf("]\n");
}

// ===========================================================================
// Free Page List Operations
// ===========================================================================

static void init_frames(void) {
  for (int i = 0; i < TOTAL_PAGES; i++) {
    frames[i].occupied = 0;
    frames[i].owner = NULL;
    frames[i].virt_page = -1;
    frames[i].load_time = 0;
    frames[i].last_used = 0;
    frames[i].use_count = 0;
  }
}

static int count_free_pages(void) {
  int count = 0;
  for (int i = 0; i < TOTAL_PAGES; i++) {
    if (!frames[i].occupied)
      count++;
  }
  return count;
}

// Allocate n pages for a process. Returns 1 on success, 0 on failure.
static int alloc_pages(Process *p, int n, int tick) {
  if (count_free_pages() < n)
    return 0;
  int allocated = 0;
  for (int i = 0; i < TOTAL_PAGES && allocated < n; i++) {
    if (!frames[i].occupied) {
      frames[i].occupied = 1;
      frames[i].owner = p;
      frames[i].virt_page = -1; // no virtual page loaded yet
      frames[i].load_time = tick;
      frames[i].last_used = tick;
      frames[i].use_count = 0;
      allocated++;
    }
  }
  return 1;
}

// Free all pages owned by a process
static void free_pages(Process *p) {
  for (int i = 0; i < TOTAL_PAGES; i++) {
    if (frames[i].occupied && frames[i].owner == p) {
      frames[i].occupied = 0;
      frames[i].owner = NULL;
      frames[i].virt_page = -1;
      frames[i].load_time = 0;
      frames[i].last_used = 0;
      frames[i].use_count = 0;
    }
  }
  // Clear process page table
  for (int v = 0; v < MAX_VIRT_PAGES; v++) {
    p->page_table[v] = -1;
  }
}

// Find a free frame owned by process p (allocated but no virt page loaded)
// or any free frame in the system. Returns frame index or -1.
static int find_free_frame_for(Process *p) {
  // First: look for a frame owned by p that has no virtual page loaded
  for (int i = 0; i < TOTAL_PAGES; i++) {
    if (frames[i].occupied && frames[i].owner == p && frames[i].virt_page == -1)
      return i;
  }
  return -1;
}

// ===========================================================================
// Job Queue (sorted linked list by arrival time)
// ===========================================================================

typedef struct {
  JobNode *head;
  int size;
} JobQueue;

static void jobqueue_init(JobQueue *q) {
  q->head = NULL;
  q->size = 0;
}

static void jobqueue_insert(JobQueue *q, Process *p) {
  JobNode *node = (JobNode *)malloc(sizeof(JobNode));
  node->proc = p;
  node->next = NULL;

  if (q->head == NULL || p->arrival_ms < q->head->proc->arrival_ms) {
    node->next = q->head;
    q->head = node;
  } else {
    JobNode *cur = q->head;
    while (cur->next != NULL && cur->next->proc->arrival_ms <= p->arrival_ms) {
      cur = cur->next;
    }
    node->next = cur->next;
    cur->next = node;
  }
  q->size++;
}

static Process *jobqueue_peek(JobQueue *q) {
  if (q->head == NULL)
    return NULL;
  return q->head->proc;
}

static Process *jobqueue_dequeue(JobQueue *q) {
  if (q->head == NULL)
    return NULL;
  JobNode *node = q->head;
  Process *p = node->proc;
  q->head = node->next;
  free(node);
  q->size--;
  return p;
}

static void jobqueue_clear(JobQueue *q) {
  while (q->head != NULL) {
    JobNode *tmp = q->head;
    q->head = tmp->next;
    free(tmp);
  }
  q->size = 0;
}

// ===========================================================================
// Workload Generation
// ===========================================================================

static int compare_arrival(const void *a, const void *b) {
  const Process *pa = (const Process *)a;
  const Process *pb = (const Process *)b;
  return pa->arrival_ms - pb->arrival_ms;
}

static void generate_workload(void) {
  for (int i = 0; i < NUM_JOBS; i++) {
    all_procs[i].name = gen_name(i);
    all_procs[i].size = PROC_SIZES[rand() % NUM_PROC_SIZES];
    all_procs[i].arrival_ms = rand() % SIM_DURATION_MS;
    all_procs[i].duration_ms =
        PROC_DURATIONS[rand() % NUM_PROC_DURATIONS] * 1000;
    all_procs[i].remaining_ms = all_procs[i].duration_ms;
    all_procs[i].current_page = 0; // always start at page 0
    all_procs[i].active = 0;
    all_procs[i].next = NULL;
    for (int v = 0; v < MAX_VIRT_PAGES; v++) {
      all_procs[i].page_table[v] = -1;
    }
  }
  // Sort by arrival time
  qsort(all_procs, NUM_JOBS, sizeof(Process), compare_arrival);
}

// Build a job queue from the global all_procs array
static void build_job_queue(JobQueue *q) {
  jobqueue_init(q);
  for (int i = 0; i < NUM_JOBS; i++) {
    // Reset process state for a new run
    all_procs[i].remaining_ms = all_procs[i].duration_ms;
    all_procs[i].current_page = 0;
    all_procs[i].active = 0;
    for (int v = 0; v < MAX_VIRT_PAGES; v++) {
      all_procs[i].page_table[v] = -1;
    }
    jobqueue_insert(q, &all_procs[i]);
  }
}

// ===========================================================================
// Locality of Reference
// ===========================================================================

// Given the current virtual page and total number of virtual pages,
// compute the next page reference using the 70/30 locality model.
static int next_page_ref(int current, int num_pages) {
  int r = rand() % 11; // 0..10
  int next;

  if (r < 7) {
    // 70% chance: delta is -1, 0, or +1
    int delta = (rand() % 3) - 1; // -1, 0, +1
    next = current + delta;
    // Wrap around within 0..(num_pages-1)
    if (next < 0)
      next = num_pages - 1;
    if (next >= num_pages)
      next = 0;
  } else {
    // 30% chance: jump to a page j where |delta| >= 2
    // i.e., j != current, current-1, current+1
    int j;
    do {
      j = rand() % num_pages;
    } while (abs(j - current) <= 1 || abs(j - current) >= (num_pages - 1));
    // Handle wrap-around edge: if num_pages is small, fallback
    next = j;
  }

  // Final safety clamp
  if (next < 0)
    next = 0;
  if (next >= num_pages)
    next = num_pages - 1;
  return next;
}

// ===========================================================================
// Page Replacement Algorithms
// ===========================================================================

// ---- FIFO: evict the page with the earliest load_time ----
static int fifo_replace(PageFrame f[], int total, Process *req, int vpage,
                        int tick) {
  int victim = -1;
  int oldest_time = tick + 1;
  for (int i = 0; i < total; i++) {
    if (f[i].occupied && f[i].virt_page >= 0) {
      if (f[i].load_time < oldest_time) {
        oldest_time = f[i].load_time;
        victim = i;
      }
    }
  }
  return victim;
}

// ---- LRU: evict page with oldest last_used timestamp ----
static int lru_replace(PageFrame f[], int total, Process *req, int vpage,
                       int tick) {
  int victim = -1;
  int oldest_used = tick + 1;
  int oldest_load = tick + 1;

  for (int i = 0; i < total; i++) {
    // only consider frames that currently contain a loaded virtual page
    if (f[i].occupied && f[i].virt_page >= 0 && f[i].owner != NULL) {
      if (f[i].last_used < oldest_used) {
        oldest_used = f[i].last_used;
        oldest_load = f[i].load_time;
        victim = i;
      } else if (f[i].last_used == oldest_used) {
        // tie-break: evict the one that has been in memory longer
        if (f[i].load_time < oldest_load) {
          oldest_load = f[i].load_time;
          victim = i;
        }
      }
    }
  }

  return victim; // -1 means "no victim found" (shouldn't happen if memory full)
}

// ---- LFU: stub ----
// ---- LFU: evict page with lowest use_count ----
static int lfu_replace(PageFrame f[], int total, Process *req, int vpage,
                       int tick) {
  int victim = -1;
  int min_use = 2147483647;   // large number (INT_MAX alternative)
  int oldest_time = tick + 1; // for tie-breaking

  for (int i = 0; i < total; i++) {
    if (f[i].occupied && f[i].virt_page >= 0) {

      // Choose frame with smallest use_count
      if (f[i].use_count < min_use) {
        min_use = f[i].use_count;
        oldest_time = f[i].load_time;
        victim = i;
      }
      // Tie-breaker: if same use_count, pick oldest loaded
      else if (f[i].use_count == min_use) {
        if (f[i].load_time < oldest_time) {
          oldest_time = f[i].load_time;
          victim = i;
        }
      }
    }
  }

  return victim;
}

// ---- MFU: stub ----
static int mfu_replace(PageFrame f[], int total, Process *req, int vpage,
                       int tick) {
  int victim = -1;
  int max_use = -1;
  int oldest_time = tick + 1;

  for (int i = 0; i < total; i++) {
    if (f[i].occupied && f[i].virt_page >= 0) {
      if (f[i].use_count > max_use) {
        max_use = f[i].use_count;
        oldest_time = f[i].load_time;
        victim = i;
      } else if (f[i].use_count == max_use) {
        // tie-break: evict the one that has been in memory longer
        if (f[i].load_time < oldest_time) {
          oldest_time = f[i].load_time;
          victim = i;
        }
      }
    }
  }

  return victim;
}

// ---- Random: stub ----
// ---- Random: pick a random occupied page to evict ----
static int random_replace(PageFrame f[], int total, Process *req, int vpage,
                          int tick) {
  int candidates[TOTAL_PAGES];
  int count = 0;

  // Collect all valid occupied frames
  for (int i = 0; i < total; i++) {
    if (f[i].occupied && f[i].virt_page >= 0) {
      candidates[count++] = i;
    }
  }

  // If no candidates found (should not normally happen)
  if (count == 0)
    return -1;

  // Pick one randomly
  int r = rand() % count;
  return candidates[r];
}

static ReplaceFn get_replace_fn(Algorithm alg) {
  switch (alg) {
  case ALG_FIFO:
    return fifo_replace;
  case ALG_LRU:
    return lru_replace;
  case ALG_LFU:
    return lfu_replace;
  case ALG_MFU:
    return mfu_replace;
  case ALG_RANDOM:
    return random_replace;
  default:
    return fifo_replace;
  }
}

// Probe whether an algorithm is implemented by calling it on a single
// occupied test frame.  Stubs return -1; real implementations return 0.
static int is_stub(Algorithm alg) {
  PageFrame test[1];
  test[0].occupied = 1;
  test[0].owner = &all_procs[0];
  test[0].virt_page = 0;
  test[0].load_time = 0;
  test[0].last_used = 0;
  test[0].use_count = 1;

  ReplaceFn fn = get_replace_fn(alg);
  return (fn(test, 1, &all_procs[0], 0, 1) < 0);
}

// ===========================================================================
// Handle a page reference for one process
// Returns: 1 = hit, 0 = miss
// ===========================================================================

static int handle_page_ref(Process *p, int vpage, int tick, ReplaceFn replace,
                           int print_detail, int *ref_counter) {
  int hit = 0;
  int evicted_frame = -1;
  char evict_owner = '.';
  int evict_vpage = -1;

  // Check if page is already in memory
  if (p->page_table[vpage] >= 0) {
    // HIT
    int frame = p->page_table[vpage];
    frames[frame].last_used = tick;
    frames[frame].use_count++;
    hit = 1;
  } else {
    // MISS — need to load this page

    // First try to find a free frame allocated to this process
    int frame = find_free_frame_for(p);

    if (frame >= 0) {
      // Use the free frame
      frames[frame].virt_page = vpage;
      frames[frame].load_time = tick;
      frames[frame].last_used = tick;
      frames[frame].use_count = 1;
      p->page_table[vpage] = frame;
    } else {
      // No free frames for this process — need to evict
      evicted_frame = replace(frames, TOTAL_PAGES, p, vpage, tick);

      if (evicted_frame >= 0) {
        // Record eviction info
        evict_owner = frames[evicted_frame].owner->name;
        evict_vpage = frames[evicted_frame].virt_page;

        // Clear old mapping
        Process *victim_proc = frames[evicted_frame].owner;
        if (victim_proc != NULL && frames[evicted_frame].virt_page >= 0) {
          victim_proc->page_table[frames[evicted_frame].virt_page] = -1;
        }

        // Load new page into this frame
        frames[evicted_frame].owner = p;
        frames[evicted_frame].virt_page = vpage;
        frames[evicted_frame].load_time = tick;
        frames[evicted_frame].last_used = tick;
        frames[evicted_frame].use_count = 1;
        p->page_table[vpage] = evicted_frame;
      }
      // If evicted_frame is -1 (stub returned -1), page fault unresolved
    }
    hit = 0;
  }

  // Print detailed record if requested
  if (print_detail) {
    double ts = (double)(tick * REF_INTERVAL_MS) / 1000.0;
    if (hit) {
      printf("%6.1fs | Proc %-2c | Page %2d | HIT  |              --\n", ts,
             p->name, vpage);
    } else if (evicted_frame >= 0) {
      printf("%6.1fs | Proc %-2c | Page %2d | MISS | Evict %c/pg%-2d\n", ts,
             p->name, vpage, evict_owner, evict_vpage);
    } else {
      printf("%6.1fs | Proc %-2c | Page %2d | MISS | Free frame used\n", ts,
             p->name, vpage);
    }
    (*ref_counter)++;
  }

  return hit;
}

// ===========================================================================
// Collect active processes into an array for iteration
// ===========================================================================

#define MAX_ACTIVE 150

typedef struct {
  Process *procs[MAX_ACTIVE];
  int count;
} ActiveList;

static ActiveList active_list;

static void active_list_add(Process *p) {
  if (active_list.count < MAX_ACTIVE) {
    active_list.procs[active_list.count++] = p;
  }
}

static void active_list_remove(int idx) {
  if (idx < 0 || idx >= active_list.count)
    return;
  active_list.procs[idx] = active_list.procs[active_list.count - 1];
  active_list.count--;
}

// ===========================================================================
// Run one simulation
// ===========================================================================

typedef struct {
  int hits;
  int misses;
  int swapped_in;
} RunStats;

static RunStats run_simulation(Algorithm alg, int run_number,
                               int print_details) {
  RunStats stats = {0, 0, 0};
  ReplaceFn replace = get_replace_fn(alg);
  int ref_counter = 0; // for detailed printing

  // Initialize memory
  init_frames();
  active_list.count = 0;

  // Build job queue for this run
  JobQueue jq;
  build_job_queue(&jq);

  if (print_details) {
    printf("\nRun %d (detailed trace — first %d page references)\n", run_number,
           DETAILED_REFS);
    printf("%s\n", DIVIDER);
    printf("%6s | %-6s | %4s | %-4s | %-16s\n", "Time", "Proc", "Page", "Hit?",
           "Eviction");
    printf("%s\n", DIVIDER);
  }

  // Main simulation loop: each tick is 100ms
  for (int tick = 0; tick < TOTAL_TICKS; tick++) {
    int current_ms = tick * REF_INTERVAL_MS;
    double current_sec = (double)current_ms / 1000.0;

    // 1) Check for new arrivals — admit if >= 4 free pages
    while (jobqueue_peek(&jq) != NULL &&
           jobqueue_peek(&jq)->arrival_ms <= current_ms) {

      if (count_free_pages() >= 4) {
        Process *p = jobqueue_dequeue(&jq);

        // Allocate pages for this process
        int needed = p->size;
        if (needed > count_free_pages()) {
          // Not enough free pages even though >= 4 free.
          // Can't admit. Re-insert? No — spec says wait.
          // We break and try again next tick.
          // Push back: we can't easily push back with our queue,
          // so we just break here and the process stays at head.
          break;
        }

        alloc_pages(p, needed, tick);
        p->active = 1;
        p->current_page = 0;

        // Load page 0 into one of the allocated frames
        int f0 = find_free_frame_for(p);
        if (f0 >= 0) {
          frames[f0].virt_page = 0;
          frames[f0].load_time = tick;
          frames[f0].last_used = tick;
          frames[f0].use_count = 1;
          p->page_table[0] = f0;
        }

        active_list_add(p);
        stats.swapped_in++;

        if (print_details) {
          printf("%6.1fs | >>> Proc %c ENTER  | %2d pages | %ds duration\n",
                 current_sec, p->name, p->size, p->duration_ms / 1000);
          print_memory_map();
        }
      } else {
        // Not enough free pages, wait
        break;
      }
    }

    // 2) For each active process: generate page reference
    for (int i = 0; i < active_list.count; i++) {
      Process *p = active_list.procs[i];
      if (!p->active)
        continue;

      // Generate next page reference (skip first tick — already on page 0)
      if (p->remaining_ms < p->duration_ms) {
        // Not the first tick for this process
        int num_vpages = (p->size < MAX_VIRT_PAGES) ? p->size : MAX_VIRT_PAGES;
        p->current_page = next_page_ref(p->current_page, num_vpages);
      }

      int should_print = print_details && (ref_counter < DETAILED_REFS);
      int h = handle_page_ref(p, p->current_page, tick, replace, should_print,
                              &ref_counter);
      if (h)
        stats.hits++;
      else
        stats.misses++;

      // Decrement remaining time
      p->remaining_ms -= REF_INTERVAL_MS;
    }

    // 3) Check for completed processes
    for (int i = active_list.count - 1; i >= 0; i--) {
      Process *p = active_list.procs[i];
      if (p->active && p->remaining_ms <= 0) {
        p->active = 0;

        if (print_details) {
          printf("%6.1fs | <<< Proc %c EXIT   | %2d pages | %ds duration\n",
                 current_sec, p->name, p->size, p->duration_ms / 1000);
        }

        free_pages(p);

        if (print_details) {
          print_memory_map();
        }

        active_list_remove(i);
      }
    }
  }

  jobqueue_clear(&jq);

  if (print_details) {
    int total_refs = stats.hits + stats.misses;
    double hit_ratio = (total_refs > 0) ? (double)stats.hits / total_refs : 0.0;
    printf("%s\n", DIVIDER);
    printf(
        "Run %d | Hits: %-5d | Misses: %-5d | Hit%%: %.2f%% | Swapped-In: %d\n",
        run_number, stats.hits, stats.misses, hit_ratio * 100.0,
        stats.swapped_in);
  }

  return stats;
}

// ===========================================================================
// Main
// ===========================================================================

int main(void) {
  srand((unsigned int)time(NULL));

  // Generate the workload once (arrival times, sizes, durations)
  generate_workload();

  printf("\n%s\n", HEADER);
  printf("  PAGE REPLACEMENT ALGORITHMS SIMULATOR\n");
  printf("  Jobs: %d | Physical Pages: %d | Duration: %d ms\n", NUM_JOBS,
         TOTAL_PAGES, SIM_DURATION_MS);
  printf("%s\n", HEADER);

  // For each algorithm
  for (int alg = 0; alg < ALG_COUNT; alg++) {
    if (is_stub((Algorithm)alg)) {
      printf("\n%s\n", HEADER);
      printf("  [%s] — NOT YET IMPLEMENTED (stub)\n", ALG_NAMES[alg]);
      printf("%s\n", HEADER);
      continue;
    }

    printf("\n%s\n", HEADER);
    printf("  [%s] Page Replacement\n", ALG_NAMES[alg]);
    printf("%s\n", HEADER);

    int total_hits = 0;
    int total_misses = 0;
    int total_swapped = 0;

    for (int run = 1; run <= NUM_RUNS; run++) {
      // Print detailed output only for the first run
      int print_detail = (run == 1);
      RunStats rs = run_simulation((Algorithm)alg, run, print_detail);
      total_hits += rs.hits;
      total_misses += rs.misses;
      total_swapped += rs.swapped_in;

      if (!print_detail) {
        int total_refs = rs.hits + rs.misses;
        double hr = (total_refs > 0) ? (double)rs.hits / total_refs : 0.0;
        printf("Run %d | Hits: %-5d | Misses: %-5d | Hit%%: %.2f%% | "
               "Swapped-In: %d\n",
               run, rs.hits, rs.misses, hr * 100.0, rs.swapped_in);
      }
    }

    // Averages
    double avg_hit_ratio = 0.0;
    int avg_refs = total_hits + total_misses;
    if (avg_refs > 0) {
      avg_hit_ratio = (double)total_hits / (double)avg_refs;
    }
    double avg_miss_ratio = 1.0 - avg_hit_ratio;
    double avg_swapped = (double)total_swapped / NUM_RUNS;

    printf("\n%s\n", DIVIDER);
    printf("%s — Average over %d runs\n", ALG_NAMES[alg], NUM_RUNS);
    printf("%s\n", DIVIDER);
    printf("Avg Hit Ratio:         %8.2f%%\n", avg_hit_ratio * 100.0);
    printf("Avg Miss Ratio:        %8.2f%%\n", avg_miss_ratio * 100.0);
    printf("Avg Processes Swapped: %8.1f\n", avg_swapped);
    printf("%s\n", DIVIDER);
  }

  printf("\n%s\n", HEADER);
  printf("  SIMULATION COMPLETE\n");
  printf("%s\n\n", HEADER);

  return 0;
}
