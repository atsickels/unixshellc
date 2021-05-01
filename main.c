/* Author: Austin Sickels (except provided code) */


/* This program simulates a UNIX shell, written in C
 The main method of the code executes in the main loop which calls relevant functions from itself
 The commands are executed in the exeCmd function and depending on the input, either run in the
 background or the foreground.*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>


#define MAX_LINE 80
#define MAX_ARGS (MAX_LINE/2 + 1)
#define REDIRECT_OUT_OP '>'
#define REDIRECT_IN_OP '<'
#define PIPE_OP '|'
#define BG_OP '&'

/* Holds a single command. */
typedef struct Cmd {
    /* The command as input by the user. */
    char line[MAX_LINE + 1];
    /* The command as null terminated tokens. */
    char tokenLine[MAX_LINE + 1];
    /* Pointers to each argument in tokenLine, non-arguments are NULL. */
    char* args[MAX_ARGS];
    /* Pointers to each symbol in tokenLine, non-symbols are NULL. */
    char* symbols[MAX_ARGS];
    /* The process id of the executing command. */
    pid_t pid;
    
    int jobNum; // Number of the index of a background job
    
    int isActive; // Whether the command has finished processing
    
    int isRunning; // Whether the command is currently processing
    
    int errorCode; // Error code for when a command errors out

} Cmd;

/* The process of the currently executing foreground command, or 0. */
pid_t foregroundPid = 0;

Cmd* foregroundCmd; // The command currently being executed in the foreground


int activeSize = 0; // the size of the active processes array
Cmd* *activeProc; // the active processes array

/* Parses the command string contained in cmd->line.
 * * Assumes all fields in cmd (except cmd->line) are initailized to zero.
 * * On return, all fields of cmd are appropriatly populated. */
void parseCmd(Cmd* cmd) {
    char* token;
    int i=0;
    strcpy(cmd->tokenLine, cmd->line);
    strtok(cmd->tokenLine, "\n");
    token = strtok(cmd->tokenLine, " ");
    while (token != NULL) {
        if (*token == '\n') {
            cmd->args[i] = NULL;
        } else if (*token == REDIRECT_OUT_OP || *token == REDIRECT_IN_OP
                || *token == PIPE_OP || *token == BG_OP) {
            cmd->symbols[i] = token;
            cmd->args[i] = NULL;
        } else {
            cmd->args[i] = token;
        }
        token = strtok(NULL, " ");
        i++;
    }
    cmd->args[i] = NULL;
}

/* Finds the index of the first occurance of symbol in cmd->symbols.
 * * Returns -1 if not found. */
int findSymbol(Cmd* cmd, char symbol) {
    for (int i = 0; i < MAX_ARGS; i++) {
        if (cmd->symbols[i] && *cmd->symbols[i] == symbol) {
            return i;
        }
    }
    return -1;
}

/* Signal handler for SIGTSTP (SIGnal - Terminal SToP),
 * which is caused by the user pressing control+z. */
void sigtstpHandler(int sig_num) {
    /* Reset handler to catch next SIGTSTP. */
    signal(SIGTSTP, sigtstpHandler);
    if (foregroundPid > 0) {
        /* Foward SIGTSTP to the currently running foreground process. */
        kill(foregroundPid, SIGTSTP);
        // Add the stopped process to the list of jobs and make the list bigger (this is described more in detail below)
        activeSize = activeSize + 1;
        activeProc = realloc(activeProc, activeSize*sizeof(Cmd*));
        activeProc[activeSize - 1] = foregroundCmd;
        foregroundCmd->jobNum = activeSize;
        foregroundCmd->isActive = 1;
        foregroundCmd->isRunning = 0;
        /* Theres a bug where if you send a stop signal to one process it seems to stop all processes
         To prevent this I check which processes were running and active before being told to stop
         and tell them to continue, note this is only for the previously running and active jobs,
         any job that was active and not running should stay that way until further notice */
        for (int i = 0; i < activeSize - 1; i++) {
            if (activeProc[i]->isActive && activeProc[i]->isRunning) {
                kill(activeProc[i]->pid, SIGCONT);
            }
        }
    }
}

/* Executes once a child process is finished, depending on how it finished
 the reponse differs
 cmd - the command that just finished
 jobNum - the index of the command in the job list
 status - how the process exited */
void *childDoneSig(Cmd *cmd, int jobNum, int status) {
    if (status == 0) { // command exited normally
        printf("[%d] Done %s", cmd->jobNum, cmd->line);
        cmd->isActive = 0; // no longer active
        cmd->isRunning = 0; // no longer running
    } else { // something went wrong in the execution
        printf("[%d] Exit %d %s", cmd->jobNum, status, cmd->line);
        cmd->isActive = 0; //no longer active
        cmd->isRunning = 0; // no longer running
    }
    return 0;
}

/* Executes the given command
 cmd - the given command
 isBg - whether the command should run in the foreground or background*/
int exeCmd(Cmd* cmd, int isBg) {
    cmd->isRunning = 1; // This command is now running
    cmd->pid = fork();
    if (cmd->pid<0) { // error when creating the child
        return 1;
    } else if (cmd->pid==0) { // we are the child
        int GTIdx = findSymbol(cmd, '>'); // the index of the redirect stdout symbol, or -1
        int LTIdx = findSymbol(cmd, '<'); // the index of the redirect stdin symbol, or -1
        int pipeIdx = findSymbol(cmd, '|'); // the index of the pipe symbol, or -1
        if (GTIdx >= 0) { // redirect stdout to a file
            FILE *file = fopen(cmd->args[GTIdx+1],"a+"); // open a file pointer to be redirected into
            dup2(fileno(file), STDOUT_FILENO); // duplicate the file pointer to stdout
            fclose(file); // close the original file
        } else if (LTIdx >= 0) { // redirect stdin to a program
            FILE *file = fopen(cmd->args[LTIdx+1],"r"); // open the file to be read from
            dup2(fileno(file), STDIN_FILENO); // duplicate the file pointer to stdin
            fclose(file); //close original file
        } else if (pipeIdx >= 0) { // pipe stdout from one to stdin of another
            int firstSize = pipeIdx * sizeof(char*); // size of the first array is the index of the pipe symbol * the size of a char*
            /* size of the second array is the total array-the pipe index+1 to get the first arg after pipe * the size of a char* */
            int secondSize = (sizeof(cmd->args)) - ((pipeIdx + 1) * sizeof(char*));
            char* firstCmd[firstSize + 1]; // init the array
            firstCmd[(firstSize/sizeof(char*))-1] = NULL; // set the last char to null to make sure its null terminated for execvp
            char* secondCmd[secondSize + 1]; // init the array
            secondCmd[(secondSize/sizeof(char*))-1] = NULL; // set the last char to null to make sure its null terminated for execvp
            
            // copy the relevant information from the given command to our new commands
            memcpy(firstCmd, &cmd->args, firstSize);
            memcpy(secondCmd, &cmd->args[pipeIdx + 1], secondSize);

            int fd[2]; // declare our pipes file descriptors
            if (pipe(fd) < 0) { //some error occurred return 1
                return 1;
            }
            pid_t temppid1 = fork(); // fork for the first command to execute
            if (temppid1 < 0) { // error check
                return 1;
            }
            if (temppid1 == 0) { // execute the first command from our pipe as a child process
                dup2(fd[1], STDOUT_FILENO); // redirect stdout to our pipe
                close(fd[0]); // close original fd's
                close(fd[1]);
                if (execvp(firstCmd[0], firstCmd) < 0) { //execute the command
                    cmd->errorCode = errno; // set an error code if it happens
                    exit(errno); // return the error code if it happens
                }
            }
            
            pid_t temppid2 = fork(); // fork for the second command
            if (temppid2 < 0) { //error check
                return 1;
            }
            if (temppid2 == 0) { //execute the second command from our pipe as a child process
                dup2(fd[0], STDIN_FILENO); // redirect stdin
                close(fd[0]); // close original fd's
                close(fd[1]);
                if (execvp(secondCmd[0], secondCmd) < 0) { //execute command
                    cmd->errorCode = errno; // set error code if it happens
                    exit(errno); //return error code
                }
            }
            close(fd[0]); // close "main" processes read end
            close(fd[1]); // close "main" processes write end
            waitpid(temppid1, NULL, 0); // wait for the first child we made
            waitpid(temppid2, NULL, 0); // wait for the second child we made
        }
        if (pipeIdx < 0) { // pipe has its own exec commands so for every other case we want to exec here
            if (execvp(cmd->args[0], cmd->args) < 0) { // same as others
                cmd->errorCode = errno;
                exit(errno);
            }
        }
        exit(0); // return needed in all paths :)
    } else {
        if (!isBg) { // if this isnt a background process
            foregroundPid = cmd->pid; // its the current foreground process
            foregroundCmd = cmd; // and the current foreground cmd
            waitpid(cmd->pid, NULL, WUNTRACED); // we wanna wait for it since its the foreground (wuntraced just in case we later kill it
            foregroundPid = 0; // once its done the foregroundpid is reset to 0
            cmd->isRunning = 0; // and the command is no longer running
            // I dont reset the foregroundCmd because theres no "default command" instead I rely on the pid to determine if
            // there is an eligable cmd in the foreground
        }
        return 0;
    }
}

/* A little extra code for background processes
 cmd - the command to execute */
int execBg(Cmd* cmd) {
    if (exeCmd(cmd, 1)) { // exeCmd returns something other than 0 on error, so return that error code
        return cmd->errorCode;
    } else { // returned 0 so we're good
        activeSize = activeSize + 1; // make the job list bigger
        activeProc = realloc(activeProc, activeSize*sizeof(Cmd*)); // realloc the job list with our new size
        activeProc[activeSize - 1] = cmd; // our command goes at the last position in our new list (-1 because we start at 0)
        cmd->jobNum = activeSize; // index of the jobs position in the list is its job number (+1 because of the assignment spec)
        cmd->isActive = 1; // any background process is active until it finishes
        printf("[%d] %d\n", activeSize, cmd->pid);
    }
    return 0;
}

/* Check to see if any background processes have finished this iteration of the loop */
void checkChildProc() {
    int status; // the return status of the child
    int childpid = waitpid(-1, &status, WNOHANG); // 0  if nothing yet, > 0 if a child did finish, WNOHANG so that the background process doesnt hold us hostage
    if (childpid > 0) { // if it was > 0 a child returned
        for (int i = 0; i < activeSize; i++) { // check through the job list to find our child
            if (activeProc[i]->pid == childpid) { // job list at i matches the returning child
                if (WIFSIGNALED(status)) { // if there was a signal that made the child return (i.e. kill) print a special message
                    printf("[%d] Terminated %s", activeProc[i]->jobNum, activeProc[i]->line);
                } else { // no signal everything executed fine
                    childDoneSig(activeProc[i], activeProc[i]->jobNum, WEXITSTATUS(status));
                }
            }
        }
    }
}



int main(void) {
    /* Listen for control+z (suspend process). */
    signal(SIGTSTP, sigtstpHandler);
    activeProc = malloc(activeSize * sizeof(Cmd*));
    char* procState;
    while (1) {
        printf("352> ");
        fflush(stdout);
        Cmd *cmd = (Cmd*) calloc(1, sizeof(Cmd));
        fgets(cmd->line, MAX_LINE, stdin);
        parseCmd(cmd);
        if (!cmd->args[0]) {
            free(cmd);
        } else if (strcmp(cmd->args[0], "exit") == 0) {
            free(cmd);
            free(activeProc);
            exit(0);
        } else if (strcmp(cmd->args[0], "kill") == 0) { // our input is to terminate a process
            int procToKill = atoi(cmd->args[1]); // convert the input to an int to compare pid to
            for (int i = 0; i < activeSize; i++) { // search the job list for the job we want to kill
                if (activeProc[i]->pid == procToKill) { // if we find it
                    if (activeProc[i]->isActive == 0) { // and it is not active
                        printf("Process already completed. Unable to terminate.\n");
                    } else { // it is active
                        activeProc[i]->isActive = 0; // not active anymore
                        kill(procToKill, SIGTERM); // send an unexpected signal to terminate early
                    }
                }
            }
        } else if (strcmp(cmd->args[0], "jobs") == 0) { // command is to list active jobs
            for (int i = 0; i < activeSize; i++) { // search through the job list
                if (activeProc[i]->isActive) { // if a job is active we're going to print it
                    if (activeProc[i]->isRunning) { // depending on if the job is currently running or stopped we want to display that info
                        procState = "Running";
                    } else {
                        procState = "Stopped";
                    }
                    printf("[%d]  %s          %s", activeProc[i]->jobNum, procState, activeProc[i]->line);
                }
            }
        } else if (strcmp(cmd->args[0], "bg") == 0) { // command is to send a stopped job back to the background
            int procToStart = atoi(cmd->args[1]); // convert input to int for pid
            kill(activeProc[procToStart - 1]->pid, SIGCONT); // tell the job to continue
            activeProc[procToStart - 1]->isRunning = 1; // make sure we tell things like jobs the status of this job
        } else {
            if (findSymbol(cmd, BG_OP) != -1) { // command to be executed in background
                if (execBg(cmd)) {
                    return WEXITSTATUS(cmd->errorCode);
                }
            } else { // command can run in foreground
                if (exeCmd(cmd, 0)) {
                    return 1;
                }
            }
            
        }
        checkChildProc(); // Checks to see if any child has exited, and handle it
    }
    return 0;
}
