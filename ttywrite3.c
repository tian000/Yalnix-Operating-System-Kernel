#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

char line[TERMINAL_MAX_LINE];

int
main()
{
    int i;
    int pid;

    if (pid == Fork()) {
	    //printf("pid=%d\n",pid);
	for (i = 0; i < 10; i++) {
	    //printf("parent cycle %d\n",i);
	    sprintf(line, "Parent line %d\n", i);
	    TtyWrite(0, line, strlen(line));
	}
    }
    else {
	for (i = 0; i < 10; i++) {
	    sprintf(line, "Child line %d\n", i);
	    TtyWrite(0, line, strlen(line));
	}
    }

    Exit(0);
}
