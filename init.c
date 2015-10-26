#include <stdio.h>
#include <stdlib.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

#define MAX_ARGC	32

int
StartTerminal(int i)
{
    char *cmd_argv[MAX_ARGC];
    char numbuf[128];	/* big enough for %d */
    int pid;

    if (i == TTY_CONSOLE)
	cmd_argv[0] = "console";
    else
	cmd_argv[0] = "shell";
    sprintf(numbuf, "%d", i);
    cmd_argv[1] = numbuf;
    cmd_argv[2] = NULL;

    TracePrintf(0, "Pid %d calling Fork\n", GetPid());
    pid = Fork();
    TracePrintf(0, "Pid %d got %d from Fork\n", GetPid(), pid);


    if (pid < 0) {
	TtyPrintf(TTY_CONSOLE,
	    "Cannot Fork control program for terminal %d.\n", i);
	return (ERROR);
    }

    if (pid == 0) {
	Exec(cmd_argv[0], cmd_argv);
	TtyPrintf(TTY_CONSOLE,
	    "Cannot Exec control program for terminal %d.\n", i);
	Exit(1);
    }

    TtyPrintf(TTY_CONSOLE, "Started pid %d on terminal %d\n", pid, i);
    return (pid);
}

int
main(int argc, char **argv)
{
    int pids[NUM_TERMINALS];
    int i;
    int status;
    int pid;

    for (i = 0; i < NUM_TERMINALS; i++) {
	pids[i] = StartTerminal(i);
	if ((i == TTY_CONSOLE) && (pids[TTY_CONSOLE] < 0)) {
	    TtyPrintf(TTY_CONSOLE, "Cannot start Console monitor!\n");
	    Exit(1);
	}
    }

    while (1) {
	pid = Wait(&status);
	if (pid == pids[TTY_CONSOLE]) {
	    TtyPrintf(TTY_CONSOLE, "Halting Yalnix\n");
	    /*
	     *  Halt should normally be a privileged instruction (and
	     *  thus not usable from user mode), but the hardware
	     *  has been set up to allow it for this project so that
	     *  we can shut down Yalnix simply here.
	     */
	    Halt();
	}
	for (i = 1; i < NUM_TERMINALS; i++) {
	    if (pid == pids[i]) break;
	}
	if (i < NUM_TERMINALS) {
	    TtyPrintf(TTY_CONSOLE, "Pid %d exited on terminal %d.\n", pid, i);
	    pids[i] = StartTerminal(i);
	}
	else {
	    TtyPrintf(TTY_CONSOLE, "Mystery pid %d returned from Wait!\n", pid);
	}
    }
}
