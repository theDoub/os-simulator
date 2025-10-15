#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];

static int current_prio = 0;        // priority of the current processing waiting queue
// static int current_slot_usage = 0;  // number of used slot of current processing waiting queue
static int slot_usage[MAX_PRIO];
#endif

/**
 * @brief Check if all scheduling queues are empty.
 *
 * This function checks whether all ready and run queues (and MLQ queues if enabled)
 * are empty, indicating that there are no processes left to schedule.
 *
 * @return 1 if all queues are empty, 0 otherwise.
 */
int queue_empty(void) {
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++) {
        if (!empty(&mlq_ready_queue[prio]))
            return -1;
    }
#endif
    return (empty(&ready_queue) && empty(&run_queue));
}

/**
 * @brief Initialize the scheduler and its queues.
 *
 * This function initializes all scheduling queues and related variables,
 * including mutexes and slot counters for MLQ scheduling if enabled.
 */
void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i;
    for (i = 0; i < MAX_PRIO; i++) {
        mlq_ready_queue[i].size = 0;
// Cấu hình ban đầu: Cấp cao hơn có nhiều slot hơn.
        slot[i] = MAX_PRIO - i;
// Khởi tạo slot_usage ban đầu từ cấu hình.
        slot_usage[i] = slot[i];
    }
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// MLQ SCHED //////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

#ifdef MLQ_SCHED

/**
 * @brief Get the next process from the current MLQ ready queues.
 *
 * This function selects and removes the next process to run from the multi-level
 * queue (MLQ) ready queues, following the MLQ round-robin policy and slot usage.
 *
 * @return Pointer to the selected process, or NULL if no process is available.
 */
struct pcb_t * get_mlq_proc(void) {    
    pthread_mutex_lock(&queue_lock);

    // Kiểm tra tất cả slot_usage có đều = 0, nếu vậy reset lại slot_usage từ mảng slot.
    bool all_zero = true;
    for (int i = 0; i < MAX_PRIO; i++) {
        if (slot_usage[i] > 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        for (int i = 0; i < MAX_PRIO; i++) {
            slot_usage[i] = slot[i];
        }
    }

    struct pcb_t * proc = NULL;
    // Duyệt qua các mức ưu tiên để tìm hàng đợi có slot còn và không rỗng
    for (int pr = 0; pr < MAX_PRIO; pr++) {
        if (slot_usage[pr] > 0 && !empty(&mlq_ready_queue[pr])) {
            proc = dequeue(&mlq_ready_queue[pr]);
            slot_usage[pr]--;
            pthread_mutex_unlock(&queue_lock);
            return proc;
        }
    }
    pthread_mutex_unlock(&queue_lock);
    return NULL;
}

/**
 * @brief Put a process back into its MLQ ready queue.
 *
 * This function enqueues a process into the MLQ ready queue corresponding to its priority.
 *
 * @param proc Pointer to the process to enqueue.
 */
void put_mlq_proc(struct pcb_t * proc) {
    if(proc == NULL) return;
    pthread_mutex_lock(&queue_lock);
        enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

/**
 * @brief Get the next process to run.
 *
 * This function selects and removes the next process to run from the ready or run queues,
 * or from the MLQ ready queues if MLQ scheduling is enabled.
 *
 * @return Pointer to the selected process, or NULL if no process is available.
 */
struct pcb_t * get_proc(void) {
    return get_mlq_proc();
}

/**
 * @brief Put a process back into the scheduler's queue.
 *
 * This function puts a running process back into the appropriate queue after its time slice, and updates bookkeeping lists.
 *
 * @param proc Pointer to the process to put back.
 */
void put_proc(struct pcb_t * proc) {
    if(proc == NULL) return;
    proc->ready_queue = &ready_queue;
    proc->mlq_ready_queue = mlq_ready_queue;
    proc->running_list = &running_list;
    put_mlq_proc(proc);
}

/**
 * @brief Add a new process to the scheduler's queue.
 *
 * This function adds a new process to the appropriate ready queue and updates bookkeeping lists.
 *
 * @param proc Pointer to the process to add.
 */
void add_proc(struct pcb_t * proc) {
    if(proc == NULL) return;
    put_proc(proc);
}
#else
///////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// NORMAL SCHED /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

struct pcb_t * get_proc(void) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&queue_lock);
    if (!empty(&ready_queue))
        proc = dequeue(&ready_queue);
    else if (!empty(&run_queue))
        proc = dequeue(&run_queue);
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t * proc) {
    if(proc == NULL) return;
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
    if(proc == NULL) return;    
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);    
}
#endif

