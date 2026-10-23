/* SPDX-License-Identifier: MIT */
/* terminal shelltool|cmdtool [label]: avoid interactive input line limits. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char label[301];
    char program[80];
    char *child = "/helios/tmp/xview-fixes/very-long-login-command-directory-to-exercise-buffer-bounds/xvchild";
    char *command[7];
    int n = 0;
    if (argv[0][0] == '-') {
        FILE *log = fopen("/helios/tmp/terminal-child.log", "a");
        if (!log) return 1;
        fprintf(log,"PASS login child argv[0] length %u\n",(unsigned)strlen(argv[0]));
        fclose(log);
        puts("PASS long login command started");
        fflush(stdout);
        sleep(40);
        return 0;
    }
    if (argc < 2 || argc > 3) return 2;
    if (strcmp(argv[1],"shelltool") && strcmp(argv[1],"cmdtool")) return 2;
    strcpy(program,"/helios/bin/x11/");
    strcat(program,argv[1]);
    command[n++] = program;
    if (argc == 3) {
        memset(label,'L',sizeof(label)-1); label[sizeof(label)-1] = 0;
        command[n++] = "-Wl"; command[n++] = label;
    }
    command[n++] = "-L"; command[n++] = child; command[n] = NULL;
    execv(command[0],command);
    perror("terminal regression launch");
    return 1;
}
