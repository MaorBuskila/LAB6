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



#define BUFFER_SIZE 2048
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0


#define STDIN 0
#define STDOUT 1


typedef struct process{
    cmdLine* cmd;
    pid_t pid;
    int status;
    struct process *next;

} process;

/* Function dec */
char* getStatus(int status);
char* getNameOfProcess(int pid);
void printProcess(process* process);
void printList(process* process_list);
void printProcessList(process** process_list);
void displayPrompt();
void printDebug(char *buffer, int pid, int debug);


process *addToList(process* process_list, cmdLine* cmd, pid_t pid);
void addProcess(process** process_list, cmdLine* cmd, pid_t pid);
void freeProcessList(process* process_list);
void updateProcessStatus(process* process_list, int pid, int status);
void updateProcessList(process **process_list);
void updateProcessList(process **process_list);
void delete_process(process* process);
int deleteTerminatedProcesses(process** process_list);
int execSpecialCommand(cmdLine* command, int debug);
void execute(cmdLine* pCmdLine, int debug);
void pipeline(int debug, cmdLine *command);




process* global_process_list;


int main(int argc, char const *argv[]) {

    int debug = 0;
    int i;

    global_process_list = NULL;

    for (i = 1; i < argc; i++){

        if (strcmp("-d", argv[i]) == 0)
            debug = 1;
    }
    char buf[BUFFER_SIZE];

    while(1){

        displayPrompt();
        fgets(buf,BUFFER_SIZE,stdin);
        cmdLine* line = parseCmdLines(buf);
        execute(line, debug);
        fprintf(stdout, "%c",'\n');

    }

    return 0;
}

/* ----- Printing ----- */
void printDebug(char *buffer, int pid, int debug) {
    if (debug == 1) {
        if (pid != -1)
            fprintf(stderr, "(%s: %d)\n", buffer, pid);
        else
            fprintf(stderr, "%s\n", buffer);
    }
}
char* getStatus(int status){

    if(status == TERMINATED)
        return "Terminated";

    else if(status == RUNNING)
        return "Running";

    else
        return "Suspended";
}


char* getNameOfProcess(int pid){

    process* curr = global_process_list;

    while(curr != NULL && curr->pid != pid){
        curr = curr->next;
    }

    if(curr == NULL)
        return NULL;

    else
        return curr->cmd->arguments[0];

}

void printProcess(process* process){

    printf("%d\t\t%s\t%s\t\t", process->pid, process->cmd->arguments[0], getStatus(process->status));

}

void printList(process* process_list){

    process* curr = process_list;

    while(curr != NULL){
        printProcess(curr);
        curr = curr->next;
    }
}
void printProcessList(process** process_list){

    updateProcessList(process_list);

    printf("PID\t\tCommand\t\tSTATUS\n");

    printList(*process_list);

    deleteTerminatedProcesses(process_list);

}
void displayPrompt(){

    char path_name[PATH_MAX];
    getcwd(path_name,PATH_MAX);
    fprintf(stdout, "%s>",path_name);
}

/* ------- List manage -------------- */
process *addToList(process* process_list, cmdLine* cmd, pid_t pid){

    if(process_list == NULL){

        process* new_process = malloc(sizeof(process));
        new_process->cmd = cmd;
        new_process->pid = pid;
        new_process->status = RUNNING;
        new_process->next = NULL;
        return new_process;
    }

    else
        process_list->next = addToList(process_list->next,cmd, pid);

    return process_list;
}


void addProcess(process** process_list, cmdLine* cmd, pid_t pid){

    *process_list = addToList(*process_list, cmd, pid);

}



void freeProcessList(process* process_list){

    process* curr = process_list;

    if(curr != NULL){

        freeProcessList(curr->next);
        freeCmdLines(curr->cmd);
        free(curr->cmd);
        free(curr->next);
        free(curr);
    }

}


void updateProcessStatus(process* process_list, int pid, int status){

    int new_status = RUNNING;


    if(WIFSTOPPED(status))
        new_status = SUSPENDED;

    else if(WIFEXITED(status) || WIFSIGNALED(status))
        new_status = TERMINATED;

    else if(WIFCONTINUED(status))
        new_status = RUNNING;

    process_list->status = new_status;

}


void updateProcessList(process **process_list){

    process* curr = *process_list;

    while(curr != NULL){

        int status;
        pid_t pid = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if(pid != 0)   /*pid argument changed state*/
            updateProcessStatus(curr, curr->pid ,status);

        curr=curr->next;
    }
}



void delete_process(process* process){

    freeCmdLines(process->cmd);
    process->cmd = NULL;
    process->next = NULL;
    free(process);
    process = NULL;
}


int deleteTerminatedProcesses(process** process_list){ /*deleting one process only for each call of the function*/

    process* curr_process = *process_list;
    process* prev_process;

    /*case of deleting the head*/
    if(curr_process != NULL && curr_process->status == TERMINATED){
        *process_list = curr_process->next;
        delete_process(curr_process);
        return 1;
    }

    /*iterate to the next terminated process*/
    while (curr_process != NULL && curr_process->status != TERMINATED){
        prev_process = curr_process;
        curr_process=curr_process->next;
    }

    /*none terminated found*/
    if(curr_process == NULL)
        return 0;

    else{
        prev_process->next = curr_process->next;
        delete_process(curr_process);
        return 1;
    }
}


int execSpecialCommand(cmdLine* command, int debug){

    int special = 0;

    if(strcmp(command->arguments[0],"cd") == 0){

        special = 1;

        int val = chdir("..");
        freeCmdLines(command);

        if(val < 0){
            perror("ERROR on cd command");

            if(debug)
                fprintf(stderr, "%s\n","ERROR on cd command");

        }
    }

    else if(strcmp(command->arguments[0],"nap") == 0){

        special = 1;

        int nap_time = atoi(command->arguments[1]);
        int nap_pid = atoi(command->arguments[2]);

        int suspend_fork = fork();
        int kill_status;
        freeCmdLines(command);

        if (suspend_fork == 0){
            kill_status = kill(nap_pid, SIGTSTP);


            if (kill_status == -1)
                perror("kill SIGTSTP failed");

            else{

                printf("%d handling SIGTSTP:\n",nap_pid);

                sleep(nap_time);
                kill_status = kill(nap_pid, SIGCONT);

                if (kill_status == -1)
                    perror("kill SIGCONT failed");

                else
                    printf("%d handling SIGCONT\n",nap_pid);
            }
            _exit(1);

        }

    }



    else if(strcmp(command->arguments[0],"showprocs") == 0){

        special = 1;
        printProcessList(&global_process_list);
        freeCmdLines(command);

    }


    else if(strcmp(command->arguments[0],"stop") == 0){

        special = 1;

        int stop_pid = atoi(command->arguments[1]);
        freeCmdLines(command);

        if(kill(stop_pid,SIGINT) == -1)    /*terminated*/
            perror("kill SIGINT failed");

        else{
            printf("%s", getNameOfProcess(stop_pid) + 2);
            printf("%s handling SIGINT\n","");
        }

    }


    return special;
}




void execute(cmdLine* pCmdLine, int debug){

    if(execSpecialCommand(pCmdLine, debug) == 0){

        if(strcmp(pCmdLine->arguments[0],"quit") == 0){
            freeProcessList(global_process_list);
            freeCmdLines(pCmdLine);
            exit(EXIT_SUCCESS);
        }

        pid_t pid = fork();
        int val = 0;

        if (pid == 0){

            if (pCmdLine->inputRedirect) {
                close(STDIN);
                fopen(pCmdLine->inputRedirect, "r");

            }

            if (pCmdLine->outputRedirect) {
                close(STDOUT);
                fopen(pCmdLine->outputRedirect, "w+");
            }



            val = execvp(pCmdLine->arguments[0],pCmdLine->arguments);
        }

        if(pid != -1)  //child success
            addProcess(&global_process_list, pCmdLine, pid);

        if(debug){
            fprintf(stderr, "%s","PID: ");
            fprintf(stderr, "%d\n",pid);
            fprintf(stderr, "%s","Executing command: ");
            fprintf(stderr, "%s\n",pCmdLine->arguments[0]);

        }

        if(pCmdLine->blocking)
            waitpid(pid, NULL, 0);


        if(val < 0){
            perror("Could not execute the command");
            _exit(EXIT_FAILURE);
        }
    }

}

void pipeline(int debug, cmdLine *command) {

    int status;
    pid_t c_process1, c_process2;
    int p[2];

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
        execvp(command->arguments[0], command->arguments);

        perror("Error");
        exit(EXIT_FAILURE);

    } else { //parent process
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
            execvp(command->arguments[0], command->arguments);

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
}