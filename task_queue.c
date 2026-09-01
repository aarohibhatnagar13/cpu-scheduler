#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

/*
 * Structure representing a single Task (Process) in the system.
 * This holds both the configuration of the task and its execution metrics.
 */
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

/*
 * Node for our queue implementation. 
 * Using a self-referential struct to build a singly linked list.
 */
typedef struct TaskNode {
    Task task;
    struct TaskNode* next;
} TaskNode;

/*
 * Queue abstraction holding pointers to head and tail to allow 
 * O(1) insertion (enqueue) and O(1) removal (dequeue).
 */
typedef struct {
    TaskNode* head;
    TaskNode* tail;
    int size;
} TaskQueue;

/* Function Declarations */
void queue_init(TaskQueue* q);
bool queue_enqueue(TaskQueue* q, Task task);
bool queue_dequeue(TaskQueue* q, Task* out_task);
void queue_free(TaskQueue* q);
void queue_print(const TaskQueue* q);
TaskQueue* generate_synthetic_tasks(int count);

int main(void) {
    // Seed the random number generator for reproducibility/randomness
    srand((unsigned int)time(NULL));

    int num_tasks = 5;
    printf("Generating %d synthetic tasks...\n", num_tasks);
    
    TaskQueue* queue = generate_synthetic_tasks(num_tasks);
    if (queue == NULL) {
        fprintf(stderr, "Failed to generate tasks due to memory allocation failure.\n");
        return EXIT_FAILURE;
    }

    printf("\nGenerated Task Queue:\n");
    queue_print(queue);

    printf("\nSimulating dequeuing tasks one by one:\n");
    Task t;
    while (queue_dequeue(queue, &t)) {
        printf("Dequeued Task ID: %d | Arrival: %d | Burst: %d | Priority: %d\n",
               t.task_id, t.arrival_time, t.burst_time, t.priority);
    }

    // Clean up all allocated memory
    queue_free(queue);
    free(queue);

    printf("\nMemory cleaned up successfully. Phase 1 complete.\n");
    return EXIT_SUCCESS;
}

/*
 * Initializes the queue pointers and size.
 */
void queue_init(TaskQueue* q) {
    if (q != NULL) {
        q->head = NULL;
        q->tail = NULL;
        q->size = 0;
    }
}

/*
 * Appends a task to the tail of the queue.
 * Returns true on success, false if memory allocation fails.
 */
bool queue_enqueue(TaskQueue* q, Task task) {
    if (q == NULL) return false;

    TaskNode* new_node = malloc(sizeof(TaskNode));
    if (new_node == NULL) {
        return false; // Out of memory
    }

    new_node->task = task;
    new_node->next = NULL;

    if (q->tail == NULL) {
        // Queue is empty
        q->head = new_node;
        q->tail = new_node;
    } else {
        // Append to tail
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->size++;
    return true;
}

/*
 * Removes a task from the head of the queue.
 * Copies the data into out_task. Returns true if successful, false if empty.
 */
bool queue_dequeue(TaskQueue* q, Task* out_task) {
    if (q == NULL || q->head == NULL || out_task == NULL) {
        return false;
    }

    TaskNode* temp = q->head;
    *out_task = temp->task;

    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL; // Queue is now empty
    }

    free(temp);
    q->size--;
    return true;
}

/*
 * Frees all internal nodes in the queue.
 */
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

/*
 * Prints details of tasks currently in the queue.
 */
void queue_print(const TaskQueue* q) {
    if (q == NULL || q->head == NULL) {
        printf("Queue is empty.\n");
        return;
    }

    printf("%-8s %-12s %-10s %-10s %-8s\n", "ID", "Arrival", "Burst", "Remaining", "Priority");
    printf("------------------------------------------------------------\n");
    TaskNode* current = q->head;
    while (current != NULL) {
        printf("%-8d %-12d %-10d %-10d %-8d\n",
               current->task.task_id,
               current->task.arrival_time,
               current->task.burst_time,
               current->task.remaining_time,
               current->task.priority);
        current = current->next;
    }
}

/*
 * Generates N tasks with randomized parameters.
 * Returns a dynamically allocated TaskQueue pointer.
 */
TaskQueue* generate_synthetic_tasks(int count) {
    TaskQueue* q = malloc(sizeof(TaskQueue));
    if (q == NULL) return NULL;

    queue_init(q);

    for (int i = 0; i < count; i++) {
        Task t;
        t.task_id = i + 1;
        // Restricting values to realistic bounds for demonstration
        t.arrival_time = rand() % 10;          // 0 to 9
        t.burst_time = (rand() % 10) + 1;       // 1 to 10
        t.remaining_time = t.burst_time;       // Initially equal to burst
        t.priority = (rand() % 5) + 1;         // Priority 1 to 5
        t.start_time = -1;                     // -1 indicates not started
        t.completion_time = 0;
        t.waiting_time = 0;

        if (!queue_enqueue(q, t)) {
            // Clean up and return NULL if an allocation fails halfway through
            queue_free(q);
            free(q);
            return NULL;
        }
    }

    return q;
}