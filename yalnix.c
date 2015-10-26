/**********************************************************
  yalnix_wj.c
  yalnix

  Created by Wenzhe Jiang & Jun Liu on 3/21/15.
  Copyright (c) 2015 Wenzhe Jiang &Jun Liu. All rights reserved.
**********************************************************/
#include "kernel.h"

/*variables definitions*/
phys_frame *free_frames_head;
int free_frame_cnt=0;

unsigned long next_PT_vaddr=VMEM_1_LIMIT-PAGESIZE;
int half_full = 0;

interruptHandler interruptVector[TRAP_VECTOR_SIZE];
struct pte *pt_r1;
struct pte idle_pt_r0[PAGE_TABLE_LEN];

pcb *currentProc;
pcb *readyQ_head=NULL, *readyQ_end=NULL;
pcb *waitQ_head=NULL, *waitQ_end=NULL;
pcb *delayQ_head;
pcb *idleProc;

terminal yalnix_term[NUM_TERMINALS];
char kernel_stack_buff[PAGESIZE*KERNEL_STACK_PAGES];

unsigned int next_pid = 0;
void *kernel_brk;
int vm_enabled = 0;

/**********************************************************/

void KernelStart(ExceptionStackFrame *frame,
                 unsigned int pmem_size,
                 void *orig_brk,
                 char** cmd_args)
{
    unsigned int i;
    unsigned long addr;

    pt_r1=(pte*)malloc(PAGE_TABLE_SIZE);
    kernel_brk = orig_brk;
    delayQ_head=(pcb*)malloc(sizeof(pcb));

/**********************************************************/

    /* Initialize interrupt vector table and write the start physical addr to REG_VECTOR_BASE */
    interruptVector[TRAP_KERNEL] = &trap_kernel_handler;
    interruptVector[TRAP_CLOCK] = &trap_clock_handler;
    interruptVector[TRAP_ILLEGAL] = &trap_illegal_handler;
    interruptVector[TRAP_MEMORY] = &trap_memory_handler;
    interruptVector[TRAP_MATH] = &trap_math_handler;
    interruptVector[TRAP_TTY_RECEIVE] = &trap_tty_receive_handler;
    interruptVector[TRAP_TTY_TRANSMIT] = &trap_tty_transmit_handler;
    for (i=7; i<TRAP_VECTOR_SIZE; i++) {
        interruptVector[i] = NULL;
    }
    WriteRegister(REG_VECTOR_BASE, (RCS421RegVal)(interruptVector));

/**********************************************************/

    /* initialize the terminal */

    for(i=0;i<NUM_TERMINALS;i++){
        yalnix_term[i].n_buf_char=0;
        yalnix_term[i].write_buf=NULL;
        yalnix_term[i].readQ_head=NULL;
        yalnix_term[i].readQ_end=NULL;
        yalnix_term[i].writeQ_head=NULL;
        yalnix_term[i].writeQ_end=NULL;
        yalnix_term[i].writingProc=NULL;
    }

/**********************************************************/

    /* initialize the free phys pages structure */
    free_frames_head = (phys_frame*)malloc(sizeof(phys_frame));
    phys_frame *tmp = free_frames_head;
    for (i=PMEM_BASE; i<PMEM_BASE+pmem_size; i += PAGESIZE){
        tmp->next = (phys_frame*)malloc(sizeof(phys_frame));
        tmp = tmp->next;
        tmp->phys_frame_num = free_frame_cnt;
        free_frame_cnt++;
    }
    tmp = free_frames_head;
    phys_frame* t;
    while (tmp->next!=NULL) {
        if (tmp->next->phys_frame_num>=(KERNEL_STACK_BASE>>PAGESHIFT) && tmp->next->phys_frame_num<((unsigned long)kernel_brk>>PAGESHIFT)) {
            t = tmp->next;
            tmp->next = tmp->next->next;
            free_frame_cnt--;
            free(t);
        }
        else tmp = tmp->next;
    }

/**********************************************************/

    /* initialize r1 page table */
    WriteRegister(REG_PTR1,(RCS421RegVal)(pt_r1));
    for (addr = VMEM_1_BASE; addr<(unsigned long)(&_etext); addr+=PAGESIZE) {
        i = (addr-VMEM_1_BASE)>>PAGESHIFT;
        pt_r1[i].pfn = addr>>PAGESHIFT;
        pt_r1[i].valid = 1;
        pt_r1[i].kprot = PROT_READ|PROT_EXEC;
        pt_r1[i].uprot = PROT_NONE;
    }
    for (; addr<(unsigned long)kernel_brk; addr += PAGESIZE) {
        i = (addr-VMEM_1_BASE)>>PAGESHIFT;
        pt_r1[i].pfn = addr>>PAGESHIFT;
        pt_r1[i].valid = 1;
        pt_r1[i].kprot = PROT_READ|PROT_WRITE;
        pt_r1[i].uprot = PROT_NONE;
    }

/**********************************************************/

    /* build and initialize page table r0 */
    WriteRegister(REG_PTR0,(RCS421RegVal)(idle_pt_r0));
    for (addr = KERNEL_STACK_BASE; addr<KERNEL_STACK_LIMIT; addr+=PAGESIZE) {
        i = addr>>PAGESHIFT;
        idle_pt_r0[i].pfn = addr>>PAGESHIFT;
        idle_pt_r0[i].valid = 1;
        idle_pt_r0[i].kprot = PROT_READ|PROT_WRITE;
        idle_pt_r0[i].uprot = PROT_NONE;
    }

/**********************************************************/

    /* enable virtual memory */
    WriteRegister(REG_VM_ENABLE,1);
    vm_enabled = 1;

/**********************************************************/

    /* create idle process */
    idleProc = (pcb*)malloc(sizeof(pcb));
    idleProc->pid = next_pid++;
    idleProc->pt_r0 = idle_pt_r0;
    idleProc->ctx=(SavedContext*)malloc(sizeof(SavedContext));
    idleProc->n_child=0;
    idleProc->delay_clock=0;
    idleProc->brk=MEM_INVALID_PAGES;
    idleProc->statusQ=NULL;
    idleProc->parent=NULL;
    idleProc->next=NULL;

/**********************************************************/

    /*set currentProc as idle*/
    currentProc=idleProc;
    LoadProgram("idle",cmd_args,frame);

/**********************************************************/

    /* create init process */
    pcb *initProc = (pcb*)malloc(sizeof(pcb));
    initProc->pid = next_pid++;
    allocPageTable(initProc);

    initProc->ctx=(SavedContext*)malloc(sizeof(SavedContext));
    initProc->n_child=0;
    initProc->delay_clock=0;
    initProc->brk=MEM_INVALID_PAGES;
    initProc->statusQ=NULL;
    initProc->parent=NULL;
    initProc->next=NULL;

/**********************************************************/

    for (addr=KERNEL_STACK_BASE; addr<KERNEL_STACK_LIMIT; addr+=PAGESIZE) {
        i = addr>>PAGESHIFT;
        initProc->pt_r0[i].pfn = getFreePage();
        initProc->pt_r0[i].valid = 1;
        initProc->pt_r0[i].kprot = PROT_READ|PROT_WRITE;
        initProc->pt_r0[i].uprot = PROT_NONE;
    }
    ContextSwitch(init_sf,currentProc->ctx,currentProc,initProc);//XXX

    if(currentProc->pid==0)
        LoadProgram("idle",cmd_args, frame);
    else if(currentProc->pid==1) {
        if (cmd_args==NULL || cmd_args[0]==NULL) LoadProgram("init",cmd_args,frame);
        else LoadProgram(cmd_args[0],cmd_args, frame);
    }
}


int SetKernelBrk(void *addr)
{
    if (vm_enabled==0) {
        if ((unsigned long)addr>VMEM_1_LIMIT) return -1;
        kernel_brk = addr;
    }
    else {
        unsigned long a, idx;
        if ((unsigned long)addr-UP_TO_PAGE(kernel_brk)>PAGESIZE*free_frame_cnt) return -1;
        for (a = UP_TO_PAGE(kernel_brk)-1; a<(unsigned long)addr; a+=PAGESIZE) {
            idx = (a-VMEM_1_BASE)>>PAGESHIFT;
            if(pt_r1[idx].valid==0){
                pt_r1[idx].pfn = getFreePage();
                pt_r1[idx].valid = 1;
                pt_r1[idx].kprot = PROT_READ|PROT_WRITE;
                pt_r1[idx].uprot = PROT_NONE;
            }
        }
    }
    return 0;
}


void trap_kernel_handler(ExceptionStackFrame *frame){
    switch(frame->code){
        case YALNIX_FORK:
            frame->regs[0]=kernel_Fork();
            break;
        case YALNIX_EXEC:
            kernel_Exec((char*)(frame->regs[1]),(char**)(frame->regs[2]),frame);
            break;
        case YALNIX_EXIT:
            kernel_Exit((int)(frame->regs[1]));
            break;
        case YALNIX_WAIT:
            frame->regs[0]=kernel_Wait((int*)(frame->regs[1]));
            break;
        case YALNIX_GETPID:
            frame->regs[0]=kernel_Getpid();
            break;
        case YALNIX_BRK:
            frame->regs[0]=kernel_Brk((void*)(frame->regs[1]));
            break;
        case YALNIX_DELAY:
            frame->regs[0]=kernel_Delay((int)(frame->regs[1]));
            break;
        case YALNIX_TTY_READ:
            frame->regs[0]=kernel_Ttyread((int)(frame->regs[1]),(void*)(frame->regs[2]),(int)(frame->regs[3]));
            break;
        case YALNIX_TTY_WRITE:
            frame->regs[0]=kernel_Ttywrite((int)(frame->regs[1]),(void*)(frame->regs[2]),(int)(frame->regs[3]));
            break;
        default:
            break;
    }
}

void trap_clock_handler(ExceptionStackFrame *frame){
    update_delay_queue();
    ContextSwitch(switch_sf,currentProc->ctx,currentProc,next_ready_queue());//XXX
}

void trap_illegal_handler(ExceptionStackFrame *frame)
{
    switch(frame->code){
        case TRAP_ILLEGAL_ILLOPC:
            printf("Illegal opcode\n");
            break;
        case TRAP_ILLEGAL_ILLOPN:
            printf("Illegal operand\n");
            break;
        case TRAP_ILLEGAL_ILLADR:
            printf("Illegal addressing mode\n");
            break;
        case TRAP_ILLEGAL_ILLTRP:
            printf("Illegal software trap\n");
            break;
        case TRAP_ILLEGAL_PRVOPC:
            printf("Privileged opcode\n");
            break;
        case TRAP_ILLEGAL_PRVREG:
            printf("Illegal register\n");
            break;
        case TRAP_ILLEGAL_COPROC:
            printf("Coprocessor error\n");
            break;
        case TRAP_ILLEGAL_BADSTK:
            printf("Bad stack\n");
            break;
        case TRAP_ILLEGAL_KERNELI:
            printf("Linux kernel sent SIGILL\n");
            break;
        case TRAP_ILLEGAL_USERI:
            printf("Received SIGILL from user\n");
            break;
        case TRAP_ILLEGAL_ADRALN:
            printf("Invalid address alignment\n");
            break;
        case TRAP_ILLEGAL_ADRERR:
            printf("Non-existant physical address\n");
            break;
        case TRAP_ILLEGAL_OBJERR:
            printf("Object-specific HW error\n");
            break;
        case TRAP_ILLEGAL_KERNELB:
            printf("Linux kernel sent SIGBUS\n");
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}

void trap_memory_handler(ExceptionStackFrame *frame)
{
    unsigned long addr=(unsigned long)(frame->addr);
    unsigned long userstackbottom=user_stack_bot();
    unsigned long i;

    unsigned long addr_vpn=addr>>PAGESHIFT;
    unsigned long usbot_vpn=userstackbottom>>PAGESHIFT;
    unsigned long brk_vpn=UP_TO_PAGE(currentProc->brk)>>PAGESHIFT;
    unsigned long down_addr_vpn=DOWN_TO_PAGE(addr)>>PAGESHIFT;
    if(addr_vpn<=usbot_vpn && addr_vpn>brk_vpn
            && (usbot_vpn-down_addr_vpn)<free_frame_cnt)
    {
        for(i=addr>>PAGESHIFT;i<=userstackbottom>>PAGESHIFT;i++){
            if((currentProc->pt_r0)[i].valid){
                Halt();
            }
            (currentProc->pt_r0)[i].valid=1;
            (currentProc->pt_r0)[i].kprot=PROT_READ|PROT_WRITE;
            (currentProc->pt_r0)[i].uprot=PROT_READ|PROT_WRITE;
            (currentProc->pt_r0)[i].pfn=getFreePage();
        }
    }
    else{
        kernel_Exit(ERROR);
    }
}

void trap_math_handler(ExceptionStackFrame *frame)
{
    switch(frame->code){
        case TRAP_MATH_INTDIV:
            printf("Integer divide by zero \n");
            break;
        case TRAP_MATH_INTOVF:
            printf("Integer overflow \n");
            break;
        case TRAP_MATH_FLTDIV:
            printf("Floating divide by zero \n");
            break;
        case TRAP_MATH_FLTOVF:
            printf("Floating overflow \n");
            break;
        case TRAP_MATH_FLTUND:
            printf("Floating underflow \n");
            break;
        case TRAP_MATH_FLTRES:
            printf("Floating inexact result\n");
            break;
        case TRAP_MATH_FLTINV:
            printf("Invalid floating operation \n");
            break;
        case TRAP_MATH_FLTSUB:
            printf("FP subscript out of range \n");
            break;
        case TRAP_MATH_KERNEL:
            printf("Linux kernel sent SIGFPE \n");
            break;
        case TRAP_MATH_USER:
            printf("Received SIGFPE from user \n");
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}

void trap_tty_receive_handler(ExceptionStackFrame *frame)
{
    int tty_id=(frame->code);
    int n_received;

    n_received=TtyReceive(tty_id,yalnix_term[tty_id].read_buf+yalnix_term[tty_id].n_buf_char,TERMINAL_MAX_LINE);
    yalnix_term[tty_id].n_buf_char+=n_received;

    if(yalnix_term[tty_id].readQ_head!=NULL){

        ContextSwitch(switch_sf,currentProc->ctx,currentProc,next_read_queue(tty_id));
    }
}

void trap_tty_transmit_handler(ExceptionStackFrame *frame)
{
    int tty_id=(frame->code);
    ContextSwitch(switch_sf,currentProc->ctx,currentProc,yalnix_term[tty_id].writingProc);
}

