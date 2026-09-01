#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

/* Task Structure from Phase 1 */
typedef struct {
    int task_id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int start_time;
    int completion_time;
    int waiting_time;
} Task;

/* Performance Metrics Structure for Phase 3 */
typedef struct {
    char policy_name[32];
    double avg_waiting_time;
    double avg_turnaround_time;
    double cpu_utilization;
    int context_switches;
} PolicyMetrics;

/* Singly linked list structures for the Task Queue */
typedef struct TaskNode {
    Task task;
    struct TaskNode* next;
} TaskNode;

typedef struct {
    TaskNode* head;
    TaskNode* tail;
    int size;
} TaskQueue;

/* Updated Function Pointer Signature to accept Metrics Struct */
typedef void (*SchedulerFunc)(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics);

/* Queue Helper Functions */
void queue_init(TaskQueue* q);
bool queue_enqueue(TaskQueue* q, Task task);
bool queue_dequeue(TaskQueue* q, Task* out_task);
void queue_free(TaskQueue* q);
TaskQueue* generate_synthetic_tasks(int count);
int copy_queue_to_array(const TaskQueue* q, Task* array);

/* Scheduling Policies with Metrics Calculations */
void simulate_sjf(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics);
void simulate_priority(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics);
void simulate_round_robin(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics);

/* Helper Functions */
void log_event(FILE* csv, int timestamp, int task_id, const char* policy, const char* event, int cpu_used, int context_switches);
void print_metrics_table(const PolicyMetrics* metrics_arr, int count);
void save_metrics_to_csv(const PolicyMetrics* metrics_arr, int count, const char* filepath);

int main(void) {
    srand((unsigned int)time(NULL));

    int num_tasks = 5;
    TaskQueue* original_queue = generate_synthetic_tasks(num_tasks);
    if (original_queue == NULL) {
        fprintf(stderr, "Failed to generate tasks.\n");
        return EXIT_FAILURE;
    }

    Task* tasks_array = malloc(sizeof(Task) * num_tasks);
    if (tasks_array == NULL) {
        queue_free(original_queue);
        free(original_queue);
        return EXIT_FAILURE;
    }
    copy_queue_to_array(original_queue, tasks_array);

    // Open CSV file for writing simulation trace logs
    FILE* log_file = fopen("simulation_log.csv", "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        free(tasks_array);
        queue_free(original_queue);
        free(original_queue);
        return EXIT_FAILURE;
    }
    fprintf(log_file, "timestamp,task_id,policy_name,event_type,cpu_time_used,context_switches_so_far\n");

    // Initialize metrics tracking array
    PolicyMetrics metrics_results[3];

    SchedulerFunc schedulers[] = { simulate_sjf, simulate_priority, simulate_round_robin };
    int num_policies = sizeof(schedulers) / sizeof(schedulers[0]);

    for (int i = 0; i < num_policies; i++) {
        schedulers[i](tasks_array, num_tasks, log_file, &metrics_results[i]);
    }

    fclose(log_file);

    // Print summary table to CLI
    print_metrics_table(metrics_results, num_policies);

    // Save summary metrics to separate CSV
    save_metrics_to_csv(metrics_results, num_policies, "metrics_summary.csv");

    free(tasks_array);
    queue_free(original_queue);
    free(original_queue);

    printf("\nPhase 3 Complete. Metrics exported to 'metrics_summary.csv'\n");
    return EXIT_SUCCESS;
}

/* Helper to log transitions to CSV */
void log_event(FILE* csv, int timestamp, int task_id, const char* policy, const char* event, int cpu_used, int context_switches) {
    fprintf(csv, "%d,%d,%s,%s,%d,%d\n", timestamp, task_id, policy, event, cpu_used, context_switches);
}

/* Print performance summary table to terminal */
void print_metrics_table(const PolicyMetrics* metrics_arr, int count) {
    printf("\n=========================================================================\n");
    printf("%-15s %-18s %-20s %-15s %-10s\n", "Policy", "Avg Waiting Time", "Avg Turnaround Time", "CPU Util %", "Context SW");
    printf("-------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("%-15s %-18.2f %-20.2f %-15.2f%% %-10d\n",
               metrics_arr[i].policy_name,
               metrics_arr[i].avg_waiting_time,
               metrics_arr[i].avg_turnaround_time,
               metrics_arr[i].cpu_utilization,
               metrics_arr[i].context_switches);
    }
    printf("=========================================================================\n");
}

/* Save summary metrics to CSV */
void save_metrics_to_csv(const PolicyMetrics* metrics_arr, int count, const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Failed to create metrics summary file");
        return;
    }
    fprintf(file, "policy_name,avg_waiting_time,avg_turnaround_time,cpu_utilization,context_switches\n");
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s,%.2f,%.2f,%.2f,%d\n",
                metrics_arr[i].policy_name,
                metrics_arr[i].avg_waiting_time,
                metrics_arr[i].avg_turnaround_time,
                metrics_arr[i].cpu_utilization,
                metrics_arr[i].context_switches);
    }
    fclose(file);
}

/* Copies queue nodes into a contiguous array and returns the count */
int copy_queue_to_array(const TaskQueue* q, Task* array) {
    int idx = 0;
    TaskNode* current = q->head;
    while (current != NULL) {
        array[idx++] = current->task;
        current = current->next;
    }
    return idx;
}

/* ============================================================================
 * SIMULATION POLICIES WITH INTEGRATED METRICS CALCULATIONS
 * ============================================================================ */

void calculate_and_store_metrics(const Task* tasks, int count, int total_span, int total_busy_time, int context_switches, const char* name, PolicyMetrics* out) {
    int total_waiting_time = 0;
    int total_turnaround_time = 0;

    for (int i = 0; i < count; i++) {
        int turnaround_time = tasks[i].completion_time - tasks[i].arrival_time;
        total_turnaround_time += turnaround_time;
        total_waiting_time += (turnaround_time - tasks[i].burst_time);
    }

    strncpy(out->policy_name, name, sizeof(out->policy_name) - 1);
    out->avg_waiting_time = (double)total_waiting_time / count;
    out->avg_turnaround_time = (double)total_turnaround_time / count;
    out->cpu_utilization = (total_span > 0) ? ((double)total_busy_time / total_span) * 100.0 : 0.0;
    out->context_switches = context_switches;
}

/* 1. SJF */
void simulate_sjf(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics) {
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int completed = 0;
    int current_time = 0;
    int context_switches = 0;
    int last_running_task_id = -1;
    int total_busy_time = 0;
    int start_span_time = -1;

    while (completed < count) {
        int best_index = -1;
        int min_burst = 1e9;

        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= current_time && tasks[i].remaining_time > 0) {
                if (tasks[i].burst_time < min_burst) {
                    min_burst = tasks[i].burst_time;
                    best_index = i;
                }
            }
        }

        if (best_index == -1) {
            int next_arrival = 1e9;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > current_time) {
                    if (tasks[i].arrival_time < next_arrival) next_arrival = tasks[i].arrival_time;
                }
            }
            current_time = (next_arrival == 1e9) ? current_time + 1 : next_arrival;
            continue;
        }

        if (start_span_time == -1) {
            start_span_time = current_time;
        }

        Task* t = &tasks[best_index];
        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        t->start_time = current_time;
        log_event(log_file, current_time, t->task_id, "SJF", "START", 0, context_switches);

        current_time += t->burst_time;
        total_busy_time += t->burst_time;
        t->remaining_time = 0;
        t->completion_time = current_time;

        log_event(log_file, current_time, t->task_id, "SJF", "COMPLETE", t->burst_time, context_switches);

        last_running_task_id = t->task_id;
        completed++;
    }

    calculate_and_store_metrics(tasks, count, current_time - start_span_time, total_busy_time, context_switches, "SJF", out_metrics);
    free(tasks);
}

/* 2. Priority */
void simulate_priority(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics) {
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int completed = 0;
    int current_time = 0;
    int context_switches = 0;
    int last_running_task_id = -1;
    int total_busy_time = 0;
    int start_span_time = -1;

    while (completed < count) {
        int best_index = -1;
        int highest_priority = 1e9;

        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= current_time && tasks[i].remaining_time > 0) {
                if (tasks[i].priority < highest_priority) {
                    highest_priority = tasks[i].priority;
                    best_index = i;
                }
            }
        }

        if (best_index == -1) {
            int next_arrival = 1e9;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > current_time) {
                    if (tasks[i].arrival_time < next_arrival) next_arrival = tasks[i].arrival_time;
                }
            }
            current_time = (next_arrival == 1e9) ? current_time + 1 : next_arrival;
            continue;
        }

        if (start_span_time == -1) {
            start_span_time = current_time;
        }

        Task* t = &tasks[best_index];
        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        t->start_time = current_time;
        log_event(log_file, current_time, t->task_id, "PRIORITY", "START", 0, context_switches);

        current_time += t->burst_time;
        total_busy_time += t->burst_time;
        t->remaining_time = 0;
        t->completion_time = current_time;

        log_event(log_file, current_time, t->task_id, "PRIORITY", "COMPLETE", t->burst_time, context_switches);

        last_running_task_id = t->task_id;
        completed++;
    }

    calculate_and_store_metrics(tasks, count, current_time - start_span_time, total_busy_time, context_switches, "PRIORITY", out_metrics);
    free(tasks);
}

/* 3. Round Robin */
void simulate_round_robin(const Task* input_tasks, int count, FILE* log_file, PolicyMetrics* out_metrics) {
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int quantum = 2;
    int current_time = 0;
    int completed = 0;
    int context_switches = 0;
    int last_running_task_id = -1;
    int total_busy_time = 0;
    int start_span_time = -1;

    int* ready_queue = malloc(sizeof(int) * count * 100);
    int head = 0, tail = 0;
    bool* in_queue = calloc(count, sizeof(bool));

    for (int i = 0; i < count; i++) {
        if (tasks[i].arrival_time <= current_time) {
            ready_queue[tail++] = i;
            in_queue[i] = true;
        }
    }

    while (completed < count) {
        if (head == tail) {
            int next_arrival = 1e9;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > current_time) {
                    if (tasks[i].arrival_time < next_arrival) next_arrival = tasks[i].arrival_time;
                }
            }
            if (next_arrival != 1e9) {
                current_time = next_arrival;
                for (int i = 0; i < count; i++) {
                    if (tasks[i].arrival_time <= current_time && !in_queue[i] && tasks[i].remaining_time > 0) {
                        ready_queue[tail++] = i;
                        in_queue[i] = true;
                    }
                }
            } else {
                current_time++;
            }
            continue;
        }

        if (start_span_time == -1) {
            start_span_time = current_time;
        }

        int current_idx = ready_queue[head++];
        Task* t = &tasks[current_idx];

        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        int execution_slice = (t->remaining_time < quantum) ? t->remaining_time : quantum;
        if (t->remaining_time == t->burst_time) {
            t->start_time = current_time;
        }

        log_event(log_file, current_time, t->task_id, "RR", "START", 0, context_switches);

        current_time += execution_slice;
        total_busy_time += execution_slice;
        t->remaining_time -= execution_slice;
        last_running_task_id = t->task_id;

        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= current_time && !in_queue[i] && tasks[i].remaining_time > 0) {
                ready_queue[tail++] = i;
                in_queue[i] = true;
            }
        }

        if (t->remaining_time > 0) {
            log_event(log_file, current_time, t->task_id, "RR", "PREEMPT", execution_slice, context_switches);
            ready_queue[tail++] = current_idx;
        } else {
            t->completion_time = current_time;
            log_event(log_file, current_time, t->task_id, "RR", "COMPLETE", execution_slice, context_switches);
            completed++;
        }
    }

    calculate_and_store_metrics(tasks, count, current_time - start_span_time, total_busy_time, context_switches, "RR", out_metrics);

    free(in_queue);
    free(ready_queue);
    free(tasks);
}

/* ============================================================================
 * QUEUE OPERATIONS & GENERATOR (reused from Phase 1)
 * ============================================================================ */

void queue_init(TaskQueue* q) {
    if (q != NULL) {
        q->head = NULL;
        q->tail = NULL;
        q->size = 0;
    }
}

bool queue_enqueue(TaskQueue* q, Task task) {
    if (q == NULL) return false;
    TaskNode* new_node = malloc(sizeof(TaskNode));
    if (new_node == NULL) return false;
    new_node->task = task;
    new_node->next = NULL;
    if (q->tail == NULL) {
        q->head = new_node;
        q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->size++;
    return true;
}

bool queue_dequeue(TaskQueue* q, Task* out_task) {
    if (q == NULL || q->head == NULL || out_task == NULL) return false;
    TaskNode* temp = q->head;
    *out_task = temp->task;
    q->head = q->head->next;
    if (q->head == NULL) q->tail = NULL;
    free(temp);
    q->size--;
    return true;
}

void queue_free(TaskQueue* q) {
    if (q == NULL) return;
    TaskNode* current = q->head;
    while (current != NULL) {
        TaskNode* next = current->next;
        free(current);
        current = next;
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

TaskQueue* generate_synthetic_tasks(int count) {
    TaskQueue* q = malloc(sizeof(TaskQueue));
    if (q == NULL) return NULL;
    queue_init(q);

    int arrival_seeds[] = {0, 1, 4, 1, 8};
    int burst_seeds[]   = {2, 5, 10, 10, 4};
    int priority_seeds[] = {3, 2, 5, 3, 3};

    for (int i = 0; i < count; i++) {
        Task t;
        t.task_id = i + 1;
        t.arrival_time = (i < 5) ? arrival_seeds[i] : rand() % 10;
        t.burst_time   = (i < 5) ? burst_seeds[i]   : (rand() % 10) + 1;
        t.remaining_time = t.burst_time;
        t.priority     = (i < 5) ? priority_seeds[i] : (rand() % 5) + 1;
        t.start_time   = -1;
        t.completion_time = 0;
        t.waiting_time = 0;

        if (!queue_enqueue(q, t)) {
            queue_free(q);
            free(q);
            return NULL;
        }
    }
    return q;
}