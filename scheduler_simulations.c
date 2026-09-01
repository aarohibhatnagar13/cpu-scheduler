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

/* Function Pointer Signature for Scheduling Policies */
typedef void (*SchedulerFunc)(const Task* input_tasks, int count, FILE* log_file);

/* Queue Helper Functions */
void queue_init(TaskQueue* q);
bool queue_enqueue(TaskQueue* q, Task task);
bool queue_dequeue(TaskQueue* q, Task* out_task);
void queue_free(TaskQueue* q);
TaskQueue* generate_synthetic_tasks(int count);
int copy_queue_to_array(const TaskQueue* q, Task* array);

/* Scheduling Policies */
void simulate_sjf(const Task* input_tasks, int count, FILE* log_file);
void simulate_priority(const Task* input_tasks, int count, FILE* log_file);
void simulate_round_robin(const Task* input_tasks, int count, FILE* log_file);

/* Logger Helper */
void log_event(FILE* csv, int timestamp, int task_id, const char* policy, const char* event, int cpu_used, int context_switches);

int main(void) {
    srand((unsigned int)time(NULL));

    int num_tasks = 5;
    TaskQueue* original_queue = generate_synthetic_tasks(num_tasks);
    if (original_queue == NULL) {
        fprintf(stderr, "Failed to generate tasks.\n");
        return EXIT_FAILURE;
    }

    // Convert the queue to a contiguous array for easier index manipulation during simulation
    Task* tasks_array = malloc(sizeof(Task) * num_tasks);
    if (tasks_array == NULL) {
        queue_free(original_queue);
        free(original_queue);
        return EXIT_FAILURE;
    }
    copy_queue_to_array(original_queue, tasks_array);

    // Open CSV file for writing simulation logs
    FILE* log_file = fopen("simulation_log.csv", "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        free(tasks_array);
        queue_free(original_queue);
        free(original_queue);
        return EXIT_FAILURE;
    }

    // Write CSV Header
    fprintf(log_file, "timestamp,task_id,policy_name,event_type,cpu_time_used,context_switches_so_far\n");

    // Define array of function pointers to execute our policies dynamically
    SchedulerFunc schedulers[] = { simulate_sjf, simulate_priority, simulate_round_robin };
    const char* policy_names[] = { "SJF", "Priority", "Round Robin" };
    int num_policies = sizeof(schedulers) / sizeof(schedulers[0]);

    for (int i = 0; i < num_policies; i++) {
        printf("\nRunning simulation for: %s...\n", policy_names[i]);
        schedulers[i](tasks_array, num_tasks, log_file);
    }

    fclose(log_file);
    free(tasks_array);
    queue_free(original_queue);
    free(original_queue);

    printf("\nPhase 2 Complete. Simulation logged to 'simulation_log.csv'\n");
    return EXIT_SUCCESS;
}

/* Helper to log transitions to CSV */
void log_event(FILE* csv, int timestamp, int task_id, const char* policy, const char* event, int cpu_used, int context_switches) {
    fprintf(csv, "%d,%d,%s,%s,%d,%d\n", timestamp, task_id, policy, event, cpu_used, context_switches);
    printf("[TIME %d] Task %d (%s) -> %s (slice: %d, CS: %d)\n", 
           timestamp, task_id, policy, event, cpu_used, context_switches);
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
 * SIMULATION POLICIES
 * ============================================================================ */

/* 
 * 1. Shortest Job First (SJF) - Non-preemptive 
 */
void simulate_sjf(const Task* input_tasks, int count, FILE* log_file) {
    // Clone input array to keep execution isolated
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int completed = 0;
    int current_time = 0;
    int context_switches = 0;
    int last_running_task_id = -1;

    while (completed < count) {
        int best_index = -1;
        int min_burst = 1e9;

        // Find arrived task with the shortest burst time
        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= current_time && tasks[i].remaining_time > 0) {
                if (tasks[i].burst_time < min_burst) {
                    min_burst = tasks[i].burst_time;
                    best_index = i;
                }
            }
        }

        // If no task has arrived, advance time to the next closest arrival
        if (best_index == -1) {
            int next_arrival = 1e9;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > current_time) {
                    if (tasks[i].arrival_time < next_arrival) {
                        next_arrival = tasks[i].arrival_time;
                    }
                }
            }
            current_time = (next_arrival == 1e9) ? current_time + 1 : next_arrival;
            continue;
        }

        Task* t = &tasks[best_index];
        
        // Context switch detection
        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        t->start_time = current_time;
        log_event(log_file, current_time, t->task_id, "SJF", "START", 0, context_switches);

        // Run non-preemptively to completion
        current_time += t->burst_time;
        t->remaining_time = 0;
        t->completion_time = current_time;
        t->waiting_time = t->completion_time - t->arrival_time - t->burst_time;

        log_event(log_file, current_time, t->task_id, "SJF", "COMPLETE", t->burst_time, context_switches);

        last_running_task_id = t->task_id;
        completed++;
    }

    free(tasks);
}

/* 
 * 2. Priority Scheduling - Non-preemptive (Lower integer priority value = higher priority)
 */
void simulate_priority(const Task* input_tasks, int count, FILE* log_file) {
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int completed = 0;
    int current_time = 0;
    int context_switches = 0;
    int last_running_task_id = -1;

    while (completed < count) {
        int best_index = -1;
        int highest_priority = 1e9;

        // Find arrived task with the highest priority (lowest numerical value)
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
                    if (tasks[i].arrival_time < next_arrival) {
                        next_arrival = tasks[i].arrival_time;
                    }
                }
            }
            current_time = (next_arrival == 1e9) ? current_time + 1 : next_arrival;
            continue;
        }

        Task* t = &tasks[best_index];

        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        t->start_time = current_time;
        log_event(log_file, current_time, t->task_id, "PRIORITY", "START", 0, context_switches);

        current_time += t->burst_time;
        t->remaining_time = 0;
        t->completion_time = current_time;
        t->waiting_time = t->completion_time - t->arrival_time - t->burst_time;

        log_event(log_file, current_time, t->task_id, "PRIORITY", "COMPLETE", t->burst_time, context_switches);

        last_running_task_id = t->task_id;
        completed++;
    }

    free(tasks);
}

/* 
 * 3. Round Robin (RR) - Preemptive (Quantum = 2)
 */
void simulate_round_robin(const Task* input_tasks, int count, FILE* log_file) {
    Task* tasks = malloc(sizeof(Task) * count);
    memcpy(tasks, input_tasks, sizeof(Task) * count);

    int quantum = 2;
    int current_time = 0;
    int completed = 0;
    int context_switches = 0;
    int last_running_task_id = -1;

    // Simple queue array containing indices of ready tasks
    int* ready_queue = malloc(sizeof(int) * count * 100); // oversized buffer for simplicity
    int head = 0, tail = 0;

    bool* in_queue = calloc(count, sizeof(bool));

    // Check initially arrived tasks at time 0
    for (int i = 0; i < count; i++) {
        if (tasks[i].arrival_time <= current_time) {
            ready_queue[tail++] = i;
            in_queue[i] = true;
        }
    }

    while (completed < count) {
        if (head == tail) {
            // Ready queue is empty, find the next arrival time to jump to
            int next_arrival = 1e9;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > current_time) {
                    if (tasks[i].arrival_time < next_arrival) {
                        next_arrival = tasks[i].arrival_time;
                    }
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

        int current_idx = ready_queue[head++];
        Task* t = &tasks[current_idx];

        if (last_running_task_id != -1 && last_running_task_id != t->task_id) {
            context_switches++;
        }

        // Determine slice to execute
        int execution_slice = (t->remaining_time < quantum) ? t->remaining_time : quantum;

        if (t->remaining_time == t->burst_time) {
            t->start_time = current_time;
        }

        log_event(log_file, current_time, t->task_id, "RR", "START", 0, context_switches);

        // Execute slice
        current_time += execution_slice;
        t->remaining_time -= execution_slice;
        last_running_task_id = t->task_id;

        // Check for any new tasks that arrived during our execution slice
        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= current_time && !in_queue[i] && tasks[i].remaining_time > 0) {
                ready_queue[tail++] = i;
                in_queue[i] = true;
            }
        }

        if (t->remaining_time > 0) {
            log_event(log_file, current_time, t->task_id, "RR", "PREEMPT", execution_slice, context_switches);
            ready_queue[tail++] = current_idx; // Re-enqueue current task
        } else {
            t->completion_time = current_time;
            t->waiting_time = t->completion_time - t->arrival_time - t->burst_time;
            log_event(log_file, current_time, t->task_id, "RR", "COMPLETE", execution_slice, context_switches);
            completed++;
        }
    }

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

    // Hardcode some seeds for stable output validation
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