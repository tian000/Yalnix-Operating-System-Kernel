#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

main(int argc, char **argv)
{
    char buf[TERMINAL_MAX_LINE];
    char *cmd_argv[TERMINAL_MAX_LINE];  /* An overkill for expediency */
    char *prompt = "yalnix> ";
    char separators[4];
    int termno;

    separators[0] = ' ';
    separators[1] = '\t';
    separators[2] = '\n';
    separators[3] = '\0';

    if (argc < 2) {
	TtyPrintf(TTY_CONSOLE, "usage: shell termno\n");
	Exit(1);
    }
    termno = atoi(argv[1]);

    if ((termno < 0) || (termno >= NUM_TERMINALS)) {
	TtyPrintf(TTY_CONSOLE, "shell: invalid terminal number %d\n", termno);
	Exit(1);
    }

    TtyPrintf(termno, "Starting shell....\n");

    while (1)
    {
	int n, pid, pid2;
	int status;
	int j;

	TtyPrintf(termno, prompt);

	n = TtyRead(termno, buf, sizeof(buf));
	if (n <= 0 || n >= sizeof(buf))		/* line too big */
	    continue;
	buf[n] = '\0';

	if (!(cmd_argv[0] = strtok(buf, separators)))
	    continue;
	if (strcmp(cmd_argv[0], "exit") == 0) {
	    TtyPrintf(termno, "Exitting shell....\n");
	    Exit(0);
	}

	j = 1;
	while ((cmd_argv[j++] = strtok(NULL, separators)) != NULL)
	    ;

	pid = Fork();

	if (pid < 0) {
	    TtyPrintf(termno, "Cannot Fork process\n");
	    continue;
	}

	if (pid == 0) {
	    Exec(cmd_argv[0], cmd_argv);
	    TtyPrintf(termno, "Could not Exec `%s'\n", cmd_argv[0]);
	    Exit(1);
	}

	pid2 = Wait(&status);
	if (pid2 < 0) {
	    TtyPrintf(termno, "Wait returned error!\n");
	    continue;
	}
	if (pid2 == pid) {
	    TtyPrintf(termno, "Pid %d exited with status %d\n",
		pid2, status);
        
	}
	else {
	    TtyPrintf(termno, "Mystery pid %d exited with status %d\n",
		pid2, status);
	}
    }
}
