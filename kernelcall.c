#include "kernel.h"

int kernel_Fork(void)
{
    int child_pid;
    unsigned long i;
    ProcessControlBlock *temp;
    ProcessControlBlock *childProc;

    childProc=(ProcessControlBlock*)malloc(sizeof(ProcessControlBlock));
    childProc->ctx=(SavedContext*)malloc(sizeof(SavedContext));
    allocPageTable(childProc);

/************************************************************/
/* check mem */

    if(used_pgn_r0()>free_frame_cnt){
        TracePrintf(0,"kernel_fork ERROR: not enough phys mem for creat Region0.\n");
        free(childProc->ctx);
        free(childProc->pt_r0);
        free(childProc);
        return ERROR;
    }

/************************************************************/
/* initialize the child pcb */

    childProc->pid=next_pid++;
    childProc->parent=currentProc;
    childProc->next=NULL;
    childProc->brk=currentProc->brk;
    childProc->n_child=0;
    childProc->delay_clock=0;
    childProc->statusQ=NULL;

    child_pid=childProc->pid;
    currentProc->n_child++;
    temp=currentProc;


    ContextSwitch(fork_sf,temp->ctx,temp,childProc);

    if(currentProc->pid==temp->pid){
        return child_pid;
    }
    else{
        return 0;
    }
}

int kernel_Exec(char *filename, char **argvec, ExceptionStackFrame *frame){
    int status;
    if(filename==NULL)
        return ERROR;

    status=LoadProgram(filename,argvec,frame);

    if(status==-1)
        return ERROR;
    if(status==-2){
        kernel_Exit(ERROR);
    }
    return 0;

}

void kernel_Exit(int status)
{
    ProcessControlBlock *tempProc;

    if(currentProc->pid==0||currentProc->pid==1)
        Halt();


    /*make al the children orphans */
    delete_child();

    if(currentProc->parent==NULL){
        ContextSwitch(exit_sf,currentProc->ctx,currentProc,next_ready_queue());
        return;
    }

    /* add status to the Q of parent */
    add_status(status);

    tempProc=next_wait_queue();

    if(tempProc==NULL){
        fflush(stdout);
        ContextSwitch(exit_sf,currentProc->ctx,currentProc,next_ready_queue());
    }
    else{
        fflush(stdout);
        ContextSwitch(exit_sf,currentProc->ctx,currentProc,tempProc);
    }
}

int kernel_Wait(int *status_ptr)
{
    int return_pid;
    StatusQueue *temp_status;
    if(currentProc->n_child==0)
        return ERROR;

    if(currentProc->statusQ==NULL){
        ContextSwitch(wait_sf,currentProc->ctx,currentProc,next_ready_queue());
    }

/************************************************************/
/* free the status in FIFO */

    return_pid=currentProc->statusQ->pid;

    *(status_ptr)=currentProc->statusQ->status;
    temp_status=currentProc->statusQ;
    currentProc->statusQ=currentProc->statusQ->next;

    free(temp_status);
    return return_pid;
}

int kernel_Getpid(void)
{
    return currentProc->pid;
}

int kernel_Brk(void *addr)
{
    if(addr==NULL)
        return ERROR;
    if((unsigned long)addr+PAGESIZE>user_stack_bot())
        return ERROR;
    unsigned long i, pn_addr, pn_brk;
    pn_addr=UP_TO_PAGE((unsigned long)addr)>>PAGESHIFT;
    pn_brk=UP_TO_PAGE((unsigned long)currentProc->brk)>>PAGESHIFT;
    if(pn_addr>=pn_brk){
        if(pn_addr-pn_brk>free_frame_cnt)
            return ERROR;
        for(i=MEM_INVALID_PAGES;i<pn_addr;i++){
            if(currentProc->pt_r0[i].valid==0){
                currentProc->pt_r0[i].valid=1;
                currentProc->pt_r0[i].uprot=PROT_READ|PROT_WRITE;
                currentProc->pt_r0[i].kprot=PROT_READ|PROT_WRITE;
                currentProc->pt_r0[i].pfn=getFreePage();
            }
        }
    }
    else{
        for(i=pn_brk;i>pn_addr;i--){
            if(currentProc->pt_r0[i].valid==1){
                removeUsedPage((currentProc->pt_r0)+i);
                currentProc->pt_r0[i].valid=0;
            }
        }
    }

    currentProc->brk=(unsigned long)addr;
    return 0;
}

int kernel_Delay(int clock_ticks)
{
    int i;

    if(clock_ticks<0)
        return ERROR;
    currentProc->delay_clock=clock_ticks;
    if(clock_ticks>0){
        ContextSwitch(delay_sf,currentProc->ctx,currentProc,next_ready_queue());
    }

    return 0;
}

int kernel_Ttyread(int tty_id, void *buf, int len)
{
    int return_len=0;
    if(len<0||buf==NULL)
        return ERROR;


    if(yalnix_term[tty_id].n_buf_char==0){

        add_read_queue(tty_id,currentProc);
        ContextSwitch(tty_sf,currentProc->ctx,currentProc,next_ready_queue());
    }

/************************************************************/
/* copy chars to buf */

    if(yalnix_term[tty_id].n_buf_char<=len){
        return_len=yalnix_term[tty_id].n_buf_char;
        memcpy(buf,yalnix_term[tty_id].read_buf,len);
        yalnix_term[tty_id].n_buf_char=0;
    }
    else{
        memcpy(buf,yalnix_term[tty_id].read_buf,len);
        yalnix_term[tty_id].n_buf_char-=len;
        memcpy(yalnix_term[tty_id].read_buf,(yalnix_term[tty_id].read_buf)+len,yalnix_term[tty_id].n_buf_char);

        return_len=len;
        if(yalnix_term[tty_id].readQ_head!=NULL)
            ContextSwitch(switch_sf,currentProc->ctx,currentProc,next_read_queue(tty_id));
    }
    return return_len;
}

int kernel_Ttywrite(int tty_id, void *buf, int len)
{
    if(buf==NULL||len<0||len>TERMINAL_MAX_LINE)
        return ERROR;

/************************************************************/
/* check if writing is busy */
    if(yalnix_term[tty_id].writingProc!=NULL){
        add_write_queue(tty_id,currentProc);
        ContextSwitch(tty_sf,currentProc->ctx,currentProc,next_ready_queue());
    }

    yalnix_term[tty_id].write_buf=buf;
    TtyTransmit(tty_id,yalnix_term[tty_id].write_buf,len);

    yalnix_term[tty_id].writingProc=currentProc;
    ContextSwitch(tty_sf,currentProc->ctx,currentProc,next_ready_queue());
    yalnix_term[tty_id].writingProc=NULL;
    if(yalnix_term[tty_id].writeQ_head!=NULL)
        ContextSwitch(switch_sf,currentProc->ctx,currentProc,next_write_queue(tty_id));

    return len;
}
