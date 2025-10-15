#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Check if a queue is empty.
 *
 * This function checks whether the given queue is empty.
 *
 * @param q Pointer to the queue to check.
 * @return 1 if the queue is empty or NULL, 0 otherwise.
 */
int empty(struct queue_t* q) {
  if (q == NULL) return 1;
  return (q->size == 0);
}

/**
 * @brief Add a process to the end of a queue.
 *
 * This function enqueues a process at the end of the given queue.
 * If the queue is full or parameters are invalid, the function does nothing.
 *
 * @param q Pointer to the queue.
 * @param proc Pointer to the process to add.
 */
void enqueue(struct queue_t* q, struct pcb_t* proc) {
  if (q == NULL || proc == NULL) return;
  if (q->size >= MAX_QUEUE_SIZE) {
    fprintf(stderr, "Queue is full, cannot enqueue process %d\n", proc->pid);
    fflush(stderr);
    return;
  }

  if (q->size < 0) q->size = 0;

  q->proc[q->size] = proc;
  q->size++;
}

/**
 * @brief Remove and return the process with the highest priority from a queue.
 *
 * This function finds and removes the process with the highest priority
 * (lowest priority value) from the queue. If MLQ_SCHED is defined, it uses
 * the dynamic priority field. The queue is updated accordingly.
 *
 * @param q Pointer to the queue.
 * @return Pointer to the removed process, or NULL if the queue is empty.
 */
struct pcb_t* dequeue(struct queue_t* q) {
  /* TODO: return a pcb whose prioprity is the highest
   * in the queue [q] and remember to remove it from q
   * */
  if (q == NULL || q->size <= 0) return NULL;

  // Validate the queue size // FB: do we really need this check?
  if (q->size > MAX_QUEUE_SIZE) {
    q->size = MAX_QUEUE_SIZE;
  }

  // Find the process with highest priority (lowest number)
  int highest_priority_idx = 0;
  for (int i = 1; i < q->size; i++) {
    if (q->proc[i] == NULL) {
      continue;
    }
    if (q->proc[highest_priority_idx] == NULL) {
      highest_priority_idx = i;
      continue;
    }
#ifdef MLQ_SCHED
    if (q->proc[i]->prio < q->proc[highest_priority_idx]->prio) {
      highest_priority_idx = i;
    }
#else
    if (q->proc[i]->priority < q->proc[highest_priority_idx]->priority) {
      highest_priority_idx = i;
    }
#endif
  }

  // Safety check
  if (q->proc[highest_priority_idx] == NULL) {
    return NULL;
  }

  // Save the highest priority process
  struct pcb_t* highest_proc = q->proc[highest_priority_idx];

  // Remove the process from the queue and shift other processes
  for (int i = highest_priority_idx; i < q->size - 1; i++) {
    q->proc[i] = q->proc[i + 1];
  }

  q->size--;
  return highest_proc;
}
