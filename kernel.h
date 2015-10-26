/**************************************************************
  kernel.h
  yalnix

  Created by Wenzhe Jiang & Jun Liu on 3/21/15.
  Copyright (c) 2015 Wenzhe Jiang & Jun Liu. All rights reserved.
**************************************************************/


#ifndef yalnix_kernel_h
#define yalnix_kernel_h

#include <stdio.h>

#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


/**************************************************************
data structures
**************************************************************/

typedef struct pte pte;

/*the status queue for waiting*/
typedef struct status_fifo_q
{
    int pid;
    int status;
    struct status_fifo_q *next;
}StatusQueue;

/*PCB data structure */
typedef struct pcb {
    unsigned int pid;
    struct pte *pt_r0;
    SavedContext *ctx;
    int n_child;
    int delay_clock;
    unsigned long brk;
    StatusQueue *statusQ;
    struct pcb *parent;
    struct pcb *next;
} pcb;

typedef pcb ProcessControlBlock;

/*free page linked list data structure*/
typedef struct pf {
    unsigned int phys_frame_num;
    struct pf *next;
} phys_frame;

typedef struct terminal
{
    int n_buf_char;
    char read_buf[256];
    char *write_buf;
    pcb *readQ_head;
    pcb *readQ_end;
    pcb *writingProc;
    pcb *writeQ_head;
    pcb *writeQ_end;
} terminal;

/**************************************************************
global variables
**************************************************************/

/*head of free phys frames*/
extern phys_frame *free_frames_head;

/* data structure for marking allocated page table */
extern unsigned long next_PT_vaddr;
extern int half_full;

/*number of free phys pages*/
extern int free_frame_cnt;

typedef void (*interruptHandler)(ExceptionStackFrame *frame);
extern interruptHandler interruptVector[TRAP_VECTOR_SIZE];

/*page table for r1*/
extern pte *pt_r1;

/*page table for ro for idle*/
extern pte idle_pt_r0[PAGE_TABLE_LEN];

extern pcb *currentProc;

extern pcb *readyQ_head, *readyQ_end;

extern pcb *waitQ_head, *waitQ_end;

extern pcb *delayQ_head;

extern pcb *idleProc;

extern terminal yalnix_term[NUM_TERMINALS];

extern char kernel_stack_buff[PAGESIZE*KERNEL_STACK_PAGES];

extern unsigned int next_pid;

extern void *kernel_brk;

extern int vm_enabled;


/*reap handler function*/
void trap_kernel_handler(ExceptionStackFrame *frame);

void trap_clock_handler(ExceptionStackFrame *frame);

void trap_illegal_handler(ExceptionStackFrame *frame);

void trap_memory_handler(ExceptionStackFrame *frame);

void trap_math_handler(ExceptionStackFrame *frame);

void trap_tty_receive_handler(ExceptionStackFrame *frame);

void trap_tty_transmit_handler(ExceptionStackFrame *frame);


/*utilities function*/
int LoadProgram(char *name, char **args, ExceptionStackFrame *frame);

int used_pgn_r0(void);

unsigned long getFreePage(void);

void removeUsedPage(pte *p);

void allocPageTable(pcb* p);

unsigned long user_stack_bot(void);

void add_ready_queue(ProcessControlBlock *p);

void add_wait_queue(ProcessControlBlock *p);

void add_read_queue(int tty_id, ProcessControlBlock* p);

void add_write_queue(int tty_id, ProcessControlBlock* p);

ProcessControlBlock *next_ready_queue(void);

ProcessControlBlock *next_read_queue(int tty_id);

ProcessControlBlock *next_write_queue(int tty_id);

ProcessControlBlock *next_wait_queue(void);

void update_delay_queue(void);

RCS421RegVal vaddr2paddr(unsigned long vaddr);


/*context switch functions*/
SavedContext *switch_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *init_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *delay_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *fork_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *exit_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *wait_sf(SavedContext *ctpx, void *p1, void *p2);

SavedContext *tty_sf(SavedContext *ctpx, void *p1, void *p2);

/*kernel call*/
extern int kernel_Fork(void);

extern int kernel_Exec(char *filename, char **argvec, ExceptionStackFrame *frame);

extern void kernel_Exit(int status);

extern int kernel_Wait(int *status_ptr);

extern int kernel_Getpid(void);

extern int kernel_Brk(void *addr);

extern int kernel_Delay(int clock_ticks);

extern int kernel_Ttyread(int tty_id, void *buf, int len);

extern int kernel_Ttywrite(int tty_id, void *buf, int len);

#endif
