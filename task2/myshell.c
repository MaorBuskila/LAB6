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
#include <fcntl.h>



#define BUFFER_SIZE 2048
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0


#define STDIN 0
#define STDOUT 1

#define READ_FLAGS (O_RDONLY)
#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
#define APPEND_FLAGS (O_WRONLY | O_CREAT | O_APPEND)
#define READ_MODES (S_IRUSR | S_IRGRP | S_IROTH)
#define CREATE_MODES (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)


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



void pipeCommands(cmdLine* input_command, int debug);



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
                int fd_input = open(pCmdLine->inputRedirect, READ_FLAGS, READ_MODES);

                if(fd_input == -1){
                    perror("Failed to open the file given as input...");
                    return;
                }

                if(dup2(fd_input, STDIN_FILENO) == -1){
                    perror("Failed to redirect standard input...");
                    return;
                }

                if(close(fd_input) == -1){
                    perror("Failed to close the input file...");
                    return;
                }

            }

            if (pCmdLine->outputRedirect) {
                int fd_output = open(pCmdLine->outputRedirect,APPEND_FLAGS, CREATE_MODES);

                if(fd_output == -1){
                    perror("Failed to create or append to the file given as input...");
                    return;
                }

                if(dup2(fd_output, STDOUT_FILENO) == -1){
                    perror("Failed to redirect standard error...");
                    return;
                }
                if(close(fd_output) == -1){
                    perror("Failed to close the output file...");
                    return;
                }
            }

            if(pCmdLine->next){
                pipeCommands(pCmdLine,debug);
                return;
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

/*handling single piped command*/
void pipeCommands(cmdLine* input_command,int debug){
    pid_t child1_pid , child2_pid;
    int fileDescriptors [2]; //file descriptor for 2 childs
    if (pipe(fileDescriptors)==-1){
        perror("pipe didn't succeed");
        exit(EXIT_FAILURE);
    }
    /*creating the first child process*/
    child1_pid = fork();
    if (child1_pid==-1){
        int error_code = errno;
        perror(strerror(error_code));
        exit(EXIT_FAILURE);
    }
    /*the first child will be enter*/
    if (!child1_pid){
        close(STDOUT);
        dup2(fileDescriptors[1],STDOUT);
        close(fileDescriptors[1]);
        printDebug("child1>going to execute cmd: ...", -1, debug);
        if (!(execvp(input_command->arguments[0] ,input_command->arguments))){
            perror("Cant exec");
            _exit(EXIT_FAILURE);
        }

    }

        /*creating the second child process*/
    else {
        close(fileDescriptors[1]); // closing the write -fileDescriptor
        //forking child 2
        child2_pid = fork();
        if (child2_pid==-1){
            int error_code = errno;
            perror(strerror(error_code));
            exit(EXIT_FAILURE);
        }
        /*the second child will be enter */
        if (!child2_pid)
        {
            close(STDIN);
            dup2(fileDescriptors[0],STDIN);
            close(fileDescriptors[0]);
            execvp(input_command->next->arguments[0] ,input_command->next->arguments);
            int error_code = errno;
            perror(strerror(error_code));
            _exit(EXIT_FAILURE);
        }
        else {
            close(fileDescriptors[0]);
            waitpid (child1_pid , NULL , 0);
            waitpid (child2_pid , NULL , 0);
        }
    }
}
