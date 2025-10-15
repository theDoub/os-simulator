# OSSim Sierra  

## -- SCHEDULER --  

### Changing Functions  
The following functions in the scheduler were implemented:  
- `src/queue.c: enqueue(), dequeue()`  
- `src/sched.c: get_mlq_proc(), put_proc(), add_proc(), get_proc()`

### How to Run and Expected output
1. Compile:
- `make all` 
2. Run:
- `./os sched`,   this expect is `output/sched.output`
- `./os sched_0`, this expect is `output/sched_0.output`
- `./os sched_1`, this expect is `output/sched_1.output`


## -- MEMORY MANAGEMENT --  

### Changing Functions

### `src/mm-memphy.c`
- `MEMPHY_dump()` – Displays non-zero content of memory for debugging
  
### `src/mm.c`
- `vmap_page_range()` – Maps virtual pages to physical frames in PGD
- `vm_map_ram()` – High-level allocation and mapping into RAM
- `init_mm()` – Initializes memory management structures and default VMA


### `src/mm-vm.c`
- `__mm_swap_page()` – Handles swapping a page to swap space
- `get_vm_area_node_at_brk()` – Allocates memory from the program break and updates VMA
- `validate_overlap_vm_area()` – Checks for overlap in new memory allocation with existing VMAs
- `inc_vma_limit()` – Extends VMA boundary and maps the additional region to RAM

### `src/libmem.c`
- `__alloc()` – Implements region allocation with free list or heap growth
- `__free()` – Frees a virtual memory region
- `liballoc()` – System call interface for memory allocation
- `libfree()` – System call interface for memory freeing
- `pg_getpage()` – Handles page fault, frame allocation, swapping, and updates page table
- `pg_getval()` – Reads a value from a virtual address using page table
- `pg_setval()` – Writes a value to a virtual address
- `find_victim_page()` – FIFO-based page replacement implementation
- `get_free_vmrg_area()` – Locates a suitable memory region inside VMA
- `pte_set_swap()` – Sets swap type and offset in page table entry
- `print_pgtbl()` – Dumps page table entries (used for debugging)

### How to Run  
1. Ensure you have a C compiler (e.g., GCC) installed on your system.  
2. Navigate to the project directory:  
  ```bash  
  cd d:/school/HK242/OPERATING_SYSTEM/os-assignment-hk242/ossim_sierra  
  ```  
3. Compile the project:  
  ```bash  
  make all  
  ```  
4. Run the executable:  
  ```bash  
  ./os os_0_mlq_paging
  ./os os_1_mlq_paging_small_1K
  ./os os_1_mlq_paging_small_4K
  ./os os_1_mlq_paging
  ./os os_1_singleCPU_mlq_paging
  ./os os_1_singleCPU_mlq
  ```  

### Expected Output  
- The memory manager allows processes to operate in an isolated and efficient virtual memory space
- Example output:  
  ```  
  Time slot   4
	CPU 2: Put process  1 to run queue
	CPU 2: Dispatched process  1
  ===== PHYSICAL MEMORY AFTER ALLOCATION =====
  PID=1 - Region=4 - Address=00000200 - Size=300 byte
  print_pgtbl: 0 - 1024
  00000000: 80000001
  00000004: 80000000
  00000008: 80000003
  00000012: 80000002
  Page Number: 0 -> Frame Number: 1
  Page Number: 1 -> Frame Number: 0
  Page Number: 2 -> Frame Number: 3
  Page Number: 3 -> Frame Number: 2
  ================================================================
	Loaded a process at input/proc/m1s, PID: 3 PRIO: 15
  ```  
- The program will terminate when all processes are completed.  


### -- SYSCALL --  

#### Changing Functions
The following syscall was implemented:  
- `src/sys_killall.c: __sys_killall(struct pcb_t *caller, struct sc_regs *regs)`

#### Purpose
- Custom system call to **terminate all processes in the ready queue with a matching path name**.
- Name is read from the caller's memory region (via `libread()`), and all matches are removed from the MLQ queues.

#### How It Works
- The syscall reads a null-terminated process name from memory (`regs->a1`).
- Iterates through `mlq_ready_queue[MAX_PRIO]`.
- Matching `proc->path` is:
  - Removed from the queue
  - Marked with `priority = -1`

#### How to Run
1. Compile:
   ```bash
   make all
   ```
2. Run syscall scenario:
   ```bash
   ./os os_sc
   ```

#### Expected Output
- Terminal shows:
  ```
  The procname retrieved from memregionid 9 is "sc3"
  ```
- Matching processes like `sc3` will not be dispatched.
- Example:
  ```
  Time slot   0
  ld_routine
  Time slot   1
  	Loaded a process at input/proc/sc3, PID: 1 PRIO: 15
  	CPU 0: Dispatched process  1
  Time slot   2
  	CPU 0: Processed  1 has finished
  	CPU 0 stopped
  ```

#### Notes
- Only affects processes in ready queues
- Requires `MLQ_SCHED` to be enabled
- Does not affect running/completed processes
