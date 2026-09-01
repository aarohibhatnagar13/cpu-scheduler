#define _GNU_SOURCE // Required on Linux to expose CPU affinity APIs
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#ifdef __linux__
#include <sched.h>
#endif

/* Busy-loop to act as a CPU-bound workload */
void run_cpu_workload(int iterations) {
    volatile unsigned long long counter = 0;
    for (int i = 0; i < iterations; i++) {
        counter++;
    }
}

/* 
 * Reads context switches for a given PID.
 * On Linux, parses /proc/[pid]/status.
 * On non-Linux (macOS), uses POSIX getrusage() for child processes.
 */
void get_context_switches(pid_t pid, long* voluntary, long* nonvoluntary) {
    *voluntary = 0;
    *nonvoluntary = 0;

#ifdef __linux__
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return; // Process might have terminated already
    }

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(line, "voluntary_ctxt_switches: %ld", voluntary);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches: %ld", nonvoluntary);
        }
    }
    fclose(file);
#else
    // macOS/POSIX fallback using getrusage for completed child processes
    (void)pid; // suppress unused variable warning
    struct rusage usage;
    if (getrusage(RUSAGE_CHILDREN, &usage) == 0) {
        *voluntary = usage.ru_nvcsw;
        *nonvoluntary = usage.ru_nivcsw;
    }
#endif
}

int main(void) {
    const int num_children = 3;
    pid_t pids[num_children];
    struct timeval start_time, end_time;

    printf("Starting Phase 4: Real Scheduler Integration\n");

#ifdef __linux__
    printf("Linux System detected. Applying CPU Pinning & Scheduler policies.\n");
    
    // Pin this parent process (and its future children) to CPU Core 0
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("Warning: sched_setaffinity failed");
    } else {
        printf("Successfully pinned execution to CPU Core 0\n");
    }

    // Set real-time scheduler policy (SCHED_FIFO) with different priority configurations
    // Note: This requires 'sudo' privileges on Linux to succeed.
    struct sched_param param;
    param.sched_priority = 50; // Middle range priority for FIFO
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("Notice: sched_setscheduler failed (run as sudo on Linux for real-time priority)");
    } else {
        printf("Successfully configured system scheduler to SCHED_FIFO\n");
    }
#else
    printf("macOS/Darwin detected. Compiling in POSIX portability mode (Skipping Linux-only CPU affinity APIs).\n");
#endif

    FILE* log_file = fopen("real_scheduler_log.csv", "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return EXIT_FAILURE;
    }
    // Matching the Phase 5 Output CSV Contract
    fprintf(log_file, "timestamp,task_id,policy_name,event_type,cpu_time_used,context_switches_so_far\n");

    gettimeofday(&start_time, NULL);

    // Spawn child processes
    for (int i = 0; i < num_children; i++) {
        long vol_before = 0, nonvol_before = 0;
        
        gettimeofday(&end_time, NULL);
        long timestamp = end_time.tv_sec - start_time.tv_sec;

        pids[i] = fork();

        if (pids[i] < 0) {
            perror("Fork failed");
            fclose(log_file);
            return EXIT_FAILURE;
        }

        if (pids[i] == 0) {
            // --- CHILD PROCESS ---
            // Run a heavy computation loop (roughly 1-2 seconds of CPU active state)
            run_cpu_workload(500000000); 
            exit(EXIT_SUCCESS);
        } else {
            // --- PARENT PROCESS ---
            get_context_switches(pids[i], &vol_before, &nonvol_before);
            
            // Log START event
            fprintf(log_file, "%ld,%d,%s,%s,%d,%ld\n", 
                    timestamp, i + 1, "REAL_OS", "START", 0, (vol_before + nonvol_before));
            printf("[REAL OS] Spawned Child %d (PID: %d)\n", i + 1, pids[i]);
        }
    }

    // Reap child processes and measure execution
    for (int i = 0; i < num_children; i++) {
        int status;
        long vol_after = 0, nonvol_after = 0;

        waitpid(pids[i], &status, 0); // Wait for child to complete
        
        gettimeofday(&end_time, NULL);
        long timestamp = end_time.tv_sec - start_time.tv_sec;

        // Obtain context switch statistics
        get_context_switches(pids[i], &vol_after, &nonvol_after);
        long total_switches = vol_after + nonvol_after;

        // Log COMPLETE event
        fprintf(log_file, "%ld,%d,%s,%s,%d,%ld\n", 
                timestamp, i + 1, "REAL_OS", "COMPLETE", 1, total_switches);
        printf("[REAL OS] Child %d (PID: %d) complete. Total context switches observed: %ld\n", 
               i + 1, pids[i], total_switches);
    }

    fclose(log_file);
    printf("\nPhase 4 Complete. Real measurements logged to 'real_scheduler_log.csv'\n");
    return EXIT_SUCCESS;
}