#include "LineParser.h"
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>


#define BUFFER_SIZE 2048
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0


#define STDIN 0
#define STDOUT 1


typedef struct process {
    cmdLine *cmd;
    pid_t pid;
    int status;
    time_t time;
    struct process *next;

} process;


process *global_process_list;


char *getStatusString(int status) {

    if (status == TERMINATED)
        return "Terminated";

    else if (status == RUNNING)
        return "Running";

    else
        return "Suspended";
}


char *getNameOfProcess(int pid) {

    process *curr_process = global_process_list;

    while (curr_process != NULL && curr_process->pid != pid) {
        curr_process = curr_process->next;
    }

    if (curr_process == NULL)
        return NULL;

    else
        return curr_process->cmd->arguments[0];

}


process *list_append(process *process_list, cmdLine *cmd, pid_t pid) {

    if (process_list == NULL) {

        process *new_process = malloc(sizeof(process));
        new_process->cmd = cmd;
        new_process->pid = pid;
        new_process->time = time(0);
        new_process->status = RUNNING;
        new_process->next = NULL;
        return new_process;
    } else
        process_list->next = list_append(process_list->next, cmd, pid);

    return process_list;
}


void addProcess(process **process_list, cmdLine *cmd, pid_t pid) {

    *process_list = list_append(*process_list, cmd, pid);

}


void printProcess(process *process) {

    printf("%d\t\t%s\t%s\t\t%ld\n", process->pid, process->cmd->arguments[0], getStatusString(process->status),
           process->time);

}


void list_print(process *process_list) {

    process *curr_process = process_list;

    while (curr_process != NULL) {
        printProcess(curr_process);
        curr_process = curr_process->next;
    }
}


void freeProcessList(process *process_list) {

    process *curr_process = process_list;

    if (curr_process != NULL) {

        freeProcessList(curr_process->next);
        freeCmdLines(curr_process->cmd);
        free(curr_process->cmd);
        free(curr_process->next);
        free(curr_process);
    }

}


void updateProcessStatus(process *process_list, int pid, int status) {

    int new_status = RUNNING;

    if (WIFEXITED(status) || WIFSIGNALED(status))
        new_status = TERMINATED;

    else if (WIFSTOPPED(status))
        new_status = SUSPENDED;

    else if (WIFCONTINUED(status))
        new_status = RUNNING;

    process_list->status = new_status;

}


void updateProcessList(process **process_list) {

    process *curr_process = *process_list;

    while (curr_process != NULL) {

        int status;
        pid_t pid = waitpid(curr_process->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (pid != 0)   /*pid argument changed state*/
            updateProcessStatus(curr_process, curr_process->pid, status);

        curr_process = curr_process->next;
    }
}


void delete_process(process *toDelete) {

    freeCmdLines(toDelete->cmd);
    toDelete->cmd = NULL;
    toDelete->next = NULL;
    free(toDelete);
    toDelete = NULL;
}


int deleteTerminatedProcesses(process **process_list) { /*deleting one process only for each call of the function*/

    process *curr_process = *process_list;
    process *prev_process;

    /*case of deleting the head*/
    if (curr_process != NULL && curr_process->status == TERMINATED) {
        *process_list = curr_process->next;
        delete_process(curr_process);
        return 1;
    }

    /*iterate to the next terminated process*/
    while (curr_process != NULL && curr_process->status != TERMINATED) {
        prev_process = curr_process;
        curr_process = curr_process->next;
    }

    /*none terminated found*/
    if (curr_process == NULL)
        return 0;

    else {
        prev_process->next = curr_process->next;
        delete_process(curr_process);
        return 1;
    }
}


void printProcessList(process **process_list) {

    updateProcessList(process_list);

    printf("PID\t\tCommand\t\tSTATUS\t\tTIME\n");

    list_print(*process_list);

    while (deleteTerminatedProcesses(process_list)) {};

}


void printCWD() {

    char path_name[PATH_MAX];
    getcwd(path_name, PATH_MAX);
    fprintf(stdout, "%s>", path_name);
}


int specialCommand(cmdLine *command, int debug) {

    int special = 0;

    if (strcmp(command->arguments[0], "cd") == 0) {

        special = 1;

        int val = chdir("..");
        freeCmdLines(command);

        if (val < 0) {
            perror("ERROR on cd command");

            if (debug)
                fprintf(stderr, "%s\n", "ERROR on cd command");

        }
    } else if (strcmp(command->arguments[0], "nap") == 0) {

        special = 1;

        int nap_time = atoi(command->arguments[1]);
        int nap_pid = atoi(command->arguments[2]);

        int suspend_fork = fork();
        int kill_status;
        freeCmdLines(command);

        if (suspend_fork == 0) {
            kill_status = kill(nap_pid, SIGTSTP);


            if (kill_status == -1)
                perror("kill SIGTSTP failed");

            else {

                printf("%d handling SIGTSTP:\n", nap_pid);

                sleep(nap_time);
                kill_status = kill(nap_pid, SIGCONT);

                if (kill_status == -1)
                    perror("kill SIGCONT failed");

                else
                    printf("%d handling SIGCONT\n", nap_pid);
            }
            _exit(1);

        }

    } else if (strcmp(command->arguments[0], "showprocs") == 0) {

        special = 1;
        printProcessList(&global_process_list);
        freeCmdLines(command);

    } else if (strcmp(command->arguments[0], "stop") == 0) {

        special = 1;

        int stop_pid = atoi(command->arguments[1]);
        freeCmdLines(command);

        if (kill(stop_pid, SIGINT) == -1)    /*terminated*/
            perror("kill SIGINT failed");

        else {
            printf("%s", getNameOfProcess(stop_pid) + 2);
            printf("%s handling SIGINT\n", "");
        }

    }


    return special;
}


void execute(cmdLine *pCmdLine, int debug) {

    if (specialCommand(pCmdLine, debug) == 0) {

        if (strcmp(pCmdLine->arguments[0], "quit") == 0) {
            freeProcessList(global_process_list);
            freeCmdLines(pCmdLine);
            exit(EXIT_SUCCESS);
        }

        pid_t pid = fork();
        int val = 0;

        if (pid == 0) {
            val = execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        }

        if (pid != -1) { //child success
            addProcess(&global_process_list, pCmdLine, pid);
        }


        if (debug) {
            fprintf(stderr, "%s", "PID: ");
            fprintf(stderr, "%d\n", pid);
            fprintf(stderr, "%s", "Executing command: ");
            fprintf(stderr, "%s\n", pCmdLine->arguments[0]);

        }

        if (pCmdLine->blocking)
            waitpid(pid, NULL, 0);


        if (val < 0) {
            perror("Could not execute the command");
            _exit(EXIT_FAILURE);
        }
    }

}

void printDebug(char *buffer, int pid, int debug) {
    if (debug == 1) {
        if (pid != -1)
            fprintf(stderr, "(%s: %d)\n", buffer, pid);
        else
            fprintf(stderr, "%s\n", buffer);
    }
}

int main(int argc, char const *argv[]) {

    int debug = 0;
    int i, status;
    pid_t c_process1, c_process2;
    int p[2];
    char *ls[3] = {"ls", "-l", 0};
    char *tail[4] = {"tail", "-n", "2", 0};

    for (i = 1; i < argc; i++) {

        if (strcmp("-d", argv[i]) == 0)
            debug = 1;
    }
    if (pipe(p) == -1) {
        perror("pipe didn't succeed");
        exit(EXIT_FAILURE);
    }
    printDebug("parent_process>forking…)", -1, debug);
    c_process1 = fork();
    if (c_process1 == -1) {
        perror("fork1 didn't succeed");
        exit(EXIT_FAILURE);
    }
    printDebug("parent_process>created process with id: ", c_process1, debug);
    if (!c_process1) { // child process
        printDebug("child1>redirecting stdout to the write end of the pipe...", -1, debug);
        close(STDOUT);
        dup(p[1]);
        close(p[1]);
        printDebug("child1>going to execute cmd: ...", -1, debug);
        execvp(ls[0], ls);

        perror("Error");
        exit(EXIT_FAILURE);

    }

    else { //parent process
        printDebug("parent_process>closing the write end of the pipe...", -1, debug);
        close(p[1]);

        printDebug("parent_process>forking…)", -1, debug);
        c_process2 = fork();
        if (c_process2 == -1) {
            perror("fork2 didn't succeed");
            exit(EXIT_FAILURE);
        }
        printDebug("parent_process>created process with id: ", getpid(), debug);
        if (!c_process2) { // child process
            printDebug("child2>redirecting stdin to the read end of the pipe...", -1, debug);
            close(STDIN);
            dup(p[0]);
            close(p[0]);
            printDebug("child2>going to execute cmd: ...", -1, debug);
            execvp(tail[0], tail);

            perror("Error");
            exit(EXIT_FAILURE);
        } else {

            printDebug("parent_process>closing the read end of the pipe...", -1, debug);
            close(p[0]);
            printDebug("parent_process>waiting for child processes 1 to terminate...", -1, debug);
            waitpid(c_process1, &status, 0);  // Parent process waits here for child to terminate.
            printDebug("parent_process>waiting for child processes 2 to terminate...", -1, debug);
            waitpid(c_process2, &status, 0);  // Parent process waits here for child to terminate.
        }
    }


    return 0;
}