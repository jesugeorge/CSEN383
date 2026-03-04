#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 512

struct timeval start_time;

void get_timestamp(char *buffer, size_t size) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // Calculate total elapsed seconds
  double elapsed = (tv.tv_sec - start_time.tv_sec) +
                   (tv.tv_usec - start_time.tv_usec) / 1000000.0;

  int minutes = (int)elapsed / 60;
  double remaining_seconds = elapsed - (minutes * 60);

  // Format as M:SS.mmm
  snprintf(buffer, size, "%d:%06.3f", minutes, remaining_seconds);
}

double get_elapsed_seconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // Return the total elapsed seconds as a double
  return (tv.tv_sec - start_time.tv_sec) +
         (tv.tv_usec - start_time.tv_usec) / 1000000.0;
}

// =========================================================================
// PERSON 2: THE TIME & FORMATTING
// =========================================================================
void format_and_print_message(FILE *out_file, char *raw_pipe_buffer) {
  // [PERSON 2 TODO]: Use strtok_r to split `raw_pipe_buffer` by newlines.
  // For each line, prepend the parent's current timestamp (get_timestamp)
  // and output using fprintf() in the format: [ParentTime] [ChildLine]
  // Make sure to fflush(out_file)!
}

// =========================================================================
// PERSON 3: THE GENERATOR NODES (Child 1-4)
// =========================================================================
void run_generator_child(int child_index, int write_fd) {
  // Seed srand() uniquely for this child.
  // Run a while loop checking get_elapsed_seconds() < 30.0.
  // Inside the loop: sleep for 0, 1, or 2 seconds randomly.
  // Use get_timestamp() to format your payload, e.g. "[ChildTime] [CHILD n]
  // Message X\n" and write() it to `write_fd`.

  srand(time(NULL) ^ (getpid() << 16));
    int msg_count = 1;
    char msg_buffer[BUFFER_SIZE];
    char ts_buffer[32];

    while (get_elapsed_seconds() < 30.0) {
        sleep(rand() % 3);

        // Re-check time after sleeping to ensure we don't write at 31+ seconds
        if (get_elapsed_seconds() >= 30.0) break;

        get_timestamp(ts_buffer, sizeof(ts_buffer));
        int len = snprintf(msg_buffer, sizeof(msg_buffer), "%s: Child %d message %d\n",
                           ts_buffer, child_index, msg_count);

        write(write_fd, msg_buffer, len);
        msg_count++;
    }
    close(write_fd); 
}

// =========================================================================
// PERSON 4: THE INTERACTIVE NODE (Child 5)
// =========================================================================
void run_interactive_child(int write_fd) {
  // [PERSON 4 TODO]: Run a while loop checking get_elapsed_seconds() < 30.0.
  // Inside the loop: Use select() on STDIN_FILENO with a timeout (e.g. 2s)
  // so you don't block past the 30-second mark.
  // If select() detects keyboard input:
  // 1. Read input with fgets().
  // 2. Strip trailing \n with strcspn().
  // 3. Format with get_timestamp() and write() to `write_fd`.
}

// =========================================================================
// PERSON 1: CORE ARCHITECTURE & MULTIPLEXER
// =========================================================================
int main() {
  int fds[5][2];
  pid_t pids[5];

  // Initialize start_time for the entire system
  gettimeofday(&start_time, NULL);

  // 1. Create pipes
  for (int i = 0; i < 5; i++) {
    if (pipe(fds[i]) == -1) {
      perror("pipe failed");
      exit(1);
    }
  }

  // 2. Fork child processes
  for (int i = 0; i < 5; i++) {
    pids[i] = fork();

    if (pids[i] < 0) {
      perror("fork failed");
      exit(1);
    }

    if (pids[i] == 0) { // === CHILD PROCESS ===
      // Close all read ends, and others' write ends to prevent leaks.
      for (int k = 0; k < 5; k++) {
        close(fds[k][READ_END]);
        if (k != i)
          close(fds[k][WRITE_END]);
      }

      if (i < 4) {
        run_generator_child(i + 1, fds[i][WRITE_END]);
      } else {
        run_interactive_child(fds[i][WRITE_END]);
      }

      close(fds[i][WRITE_END]);
      exit(0);
    }
  }

  // === PARENT PROCESS ===
  for (int i = 0; i < 5; i++) {
    close(fds[i][WRITE_END]);
  }

  FILE *out_file = fopen("output.txt", "w");
  if (!out_file)
    exit(1);
  setvbuf(out_file, NULL, _IONBF, 0);

  fd_set inputs, inputfds;
  FD_ZERO(&inputs);

  for (int i = 0; i < 5; i++) {
    FD_SET(fds[i][READ_END], &inputs);
  }

  int active_children = 5;
  char buffer[BUFFER_SIZE];

  while (active_children > 0) {
    inputfds = inputs;
    int result = select(FD_SETSIZE, &inputfds, NULL, NULL, NULL);
    if (result == -1)
      break;

    if (result > 0) {
      for (int i = 0; i < 5; i++) {
        if (FD_ISSET(fds[i][READ_END], &inputfds)) {
          int bytes_read = read(fds[i][READ_END], buffer, BUFFER_SIZE - 1);
          if (bytes_read == 0) { // EOF
            FD_CLR(fds[i][READ_END], &inputs);
            close(fds[i][READ_END]);
            active_children--;
          } else if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            format_and_print_message(out_file, buffer);
          }
        }
      }
    }
  }

  fclose(out_file);
  for (int i = 0; i < 5; i++)
    wait(NULL);
  printf("All children terminated. Parent exiting.\n");
  return 0;
}
