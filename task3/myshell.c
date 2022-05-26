
#include "pipeHelper.h"
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


typedef struct process {
    cmdLine *cmd;
    pid_t pid;
    int status;
    struct process *next;

} process;

/* Function dec */

void displayPrompt();

void printDebug(char *buffer, int pid, int debug);

int execSpecialCommand(cmdLine *command, int debug);

void execute(cmdLine *command, int debug, int counter);

int cmdCounter(cmdLine *command, int debug);

process *global_process_list;


int main(int argc, char const *argv[]) {

    int debug = 0;
    int i;
    int counter = 1;

    global_process_list = NULL;

    for (i = 1; i < argc; i++) {

        if (strcmp("-d", argv[i]) == 0)
            debug = 1;
    }
    char buf[BUFFER_SIZE];

    while (1) {

        displayPrompt();
        fgets(buf, BUFFER_SIZE, stdin);
        cmdLine *line = parseCmdLines(buf);
        counter = cmdCounter(line, debug);
//        fprintf(stdout, "%d\n", counter);
        execute(line, debug, counter);
        fprintf(stdout, "%c", '\n');

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


void displayPrompt() {

    char path_name[PATH_MAX];
    getcwd(path_name, PATH_MAX);
    fprintf(stdout, "%s>", path_name);
}

int execSpecialCommand(cmdLine *command, int debug) {
    int special = 0;
    if (strcmp(command->arguments[0], "cd") == 0) {

        special = 1;

        int val = chdir(command->arguments[1]);
        freeCmdLines(command);

        if (val < 0) {
            perror("ERROR on cd command");

            if (debug)
                fprintf(stderr, "%s\n", "ERROR on cd command");

        }

    }
    return special;
}


void execute(cmdLine *command, int debug, int counter) {
    pid_t wait_pid;
    pid_t pid;
    int **pipes;
    if (execSpecialCommand(command, debug) == 0) {
        int waitpid_status;
        if (strcmp(command->arguments[0], "quit") == 0) {
            freeCmdLines(command);
            exit(EXIT_SUCCESS);
        }
        if (counter > 1) { // if we have few commands, need to create pipe
            pipes = createPipes(counter - 1);
            cmdLine *cur_command = command;

            while (cur_command != NULL) {

                if ((pid = fork()) == -1) {
                    perror("cant fork");
                    exit(1);
                } else if (pid == 0) {
                    /*child*/

                    if (command->inputRedirect) {
                        int fd_input = open(command->inputRedirect, READ_FLAGS, READ_MODES);

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

                    if (command->outputRedirect) {
                        int fd_output = open(command->outputRedirect,APPEND_FLAGS, CREATE_MODES);

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

                    //check if there is left command
                    if (leftPipe(pipes, cur_command) != NULL) {
                        dup2(pipes[cur_command->idx - 1][0],0);/*replace the read end to our file */
                        close(pipes[cur_command->idx - 1][0]); /*Close the file descriptor that was duplicated. */
                    }

                    //check if there is right command
                    if (rightPipe(pipes, cur_command) != NULL) {
                        dup2(pipes[cur_command->idx][1], 1); /*replace the write-end to our file */
                        close(pipes[cur_command->idx][1]); /*Close the file descriptor that was duplicated. */

                    }

                    execvp(cur_command->arguments[0], cur_command->arguments); //execvp only file name

                    exit(1);
                } else {/*parent code*/
                    waitpid(pid, &waitpid_status, 0);//
                    //wait(NULL);

                    if (rightPipe(pipes, cur_command) != NULL) {
                        close(pipes[cur_command->idx][1]);
                    }

                    //check if it is the first command - if not close read channel
                    if (leftPipe(pipes, cur_command) != NULL) {
                        close(pipes[cur_command->idx - 1][0]);
                    }

                    //check if it is the last command
                    if (rightPipe(pipes, cur_command) == NULL) {
                        releasePipes(pipes, counter - 1);

                    }
                    cur_command = cur_command->next;
                }
            }
        }
/*    set follow-fork-mode child    */
/*    set detach-on-fork off        */
/*    ls | tee | tail -n 2     */
        else {/*the old shell */
            pid = fork();
            if (pid == 0) {
                if (command->inputRedirect) {
                    int fd_input = open(command->inputRedirect, READ_FLAGS, READ_MODES);

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

                if (command->outputRedirect) {
                    int fd_output = open(command->outputRedirect,APPEND_FLAGS, CREATE_MODES);

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
                execvp(command->arguments[0], command->arguments); //execvp only file name
                _exit(0);//if execvp in executed succeed then we will not return to this line in the code, so if failed we will get here anyway
            }

            wait_pid = 0;
            if (command->blocking != 0) {
                waitpid(pid, &waitpid_status, 0);// equivalent to wait(&ProgramId)


            }

        }

    }

}

int cmdCounter(cmdLine *command, int debug) {
    int counter = 1;
    cmdLine *cur_command = command;
    while (cur_command->next) {
        cur_command = cur_command->next;
        counter++;
    }
    return counter;;
}