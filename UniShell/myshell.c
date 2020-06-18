
#include <stdio.h>
#include <linux/limits.h>
#include <wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "LineParser.h"

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0


typedef struct process {
    cmdLine *cmd;                         /* the parsed command line*/
    pid_t pid;                          /* the process id that is running the command*/
    int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;              /* next process in chain */
} process;

typedef struct pair {

    char* var;
    char* val;
    struct pair *next;

} pair;

/*Program flags*/
char debug = 0;

/*The processes list, holds ref to head at 0*/
process **this_process_list = NULL;

/*The variables list, holds ref to head at 0*/
pair **var_list = NULL;

/*Check the flags of the program*/
void checkMode(int argc, char *arguments[]) {

    /*Check all our arguments*/
    int i;
    for (i = 1; i < argc; i++) {

        if (arguments[i][1] == 'd') {

            fprintf(stderr, "Debug mode\n");
            debug = 1;

        }
    }
}

void del_link(process *proc) {

    process *temp = this_process_list[0];

    /*If the link we need to delete is the head*/
    if (temp == proc) {

        this_process_list[0] = this_process_list[0]->next;

        free(temp->cmd);
        free(temp);

    } else {

        /*Find the link in our list*/
        while (temp->next != proc)
            temp = temp->next;

        /*Now temp -> next is the link we want to delete*/
        process *to_del = temp->next;
        temp->next = temp->next->next;

        free(to_del->cmd);
        free(to_del);

    }
}

/*Delete all the processes and their information*/
void freeProcessList(process *process_list) {

    /*Check that we have anything to delete*/
    if(process_list != NULL) {

        /*Delete all data associated with this process*/
        freeCmdLines(process_list->cmd);
        if (process_list->next != NULL)
            freeProcessList(process_list->next);
        free(process_list);

    }

}

void freeVarsList(pair* current){

    /*Check that we have anything to delete*/
    if(current != NULL) {

        free(current -> var);
        free(current -> val);

        if (current->next != NULL)
            freeVarsList(current->next);

        free(current);

    }


}

/*Change the status of a given process*/
void updateProcessStatus(process *process_list, int pid, int status) {

    /*Find the process in the list and update it*/
    while (process_list != NULL) {

        if (process_list->pid == pid) {

            process_list->status = status;
            break;

        } else {

            process_list = process_list->next;

        }

    }
}

/*Iterate through the processes and update their status*/
void updateProcessList(process **process_list) {

    process *temp = process_list[0];
    while (temp != NULL) {

        int status;

        if (temp->status != TERMINATED) {

            /*Check the status of the process*/
            pid_t return_pid = waitpid(temp->pid, &status,WNOHANG | WUNTRACED | WCONTINUED);


            /*A signal has been recieved*/
            if(return_pid > 0) {

                if(WSTOPSIG(status) > 0 && WSTOPSIG(status) < 32){

                    /*Process is suspended*/
                    temp->status = SUSPENDED;

                }else if(WIFCONTINUED(status)){

                    /*Process is continued*/
                    temp->status = RUNNING;

                }else{
                    temp->status = TERMINATED;
                }

            }

        }

        /*Next process*/
        temp = temp->next;
    }



}

/*Add a process to the process list*/
void addProcess(process **process_list, cmdLine *cmd, pid_t pid) {

    /*If the list head is empty create a new one*/
    if (this_process_list[0] == NULL) {

        /*Create the head*/
        this_process_list[0] = malloc(sizeof(process));
        this_process_list[0]->cmd = cmd;
        this_process_list[0]->pid = pid;
        this_process_list[0]->status = RUNNING;
        this_process_list[0]->next = NULL;

    } else {

        /*Get to the end of the list*/
        process *temp = this_process_list[0];
        while (temp->next != NULL)
            temp = temp->next;

        /*Create a new process in the list and assign values to it*/
        temp->next = malloc(sizeof(process));
        temp->next->cmd = cmd;
        temp->next->pid = pid;
        temp->next->status = RUNNING;
        temp->next->next = NULL;


    }
}

/*Helper function to print commands*/
void print_cmd(char *const *arguments) {

    int i = 0;
    while (arguments[i] != NULL) {
        printf("%s ", arguments[i]);
        i++;
    }
}

/*Print all our processes*/
void printProcessList(process **process_list) {

    updateProcessList(this_process_list);

    printf("PID       Command	\tSTATUS\n\n");

    /*Get the head*/
    process *temp = this_process_list[0];

    while (temp != NULL) {

        if (temp->status == TERMINATED) {

            printf("%d     ", temp->pid);
            print_cmd(temp->cmd->arguments);
            printf("	         TERMINATED\n");

            process* terminated = temp;

            temp = temp->next;

            /*Now delete it*/
            del_link(terminated);

        } else if (temp->status == RUNNING) {

            printf("%d     ", temp->pid);
            print_cmd(temp->cmd->arguments);
            printf("          RUNNING\n");

            temp = temp->next;

        } else {

            printf("%d     ", temp->pid);
            print_cmd(temp->cmd->arguments);
            printf("        SUSPENDED\n");

            temp = temp->next;

        }

    }
}

/*Handle suspend/wait/kill*/
void handle_swk(cmdLine* pCmdLine){

    /*The process we will work with*/
    pid_t pid = atoi(pCmdLine -> arguments[1]);

    /*Different scenarios:*/
    if((strcmp(pCmdLine->arguments[0], "kill") == 0)){

        printf("Handling SIGINT\n");

        /*Terminate the given process*/
        kill(pid, SIGINT);

        freeCmdLines(pCmdLine);

    } else if (strcmp(pCmdLine->arguments[0], "suspend") == 0){

        printf("Handling SIGSTOP\n");

        /*Suspend the given process*/
        kill(pid, SIGSTOP);

        freeCmdLines(pCmdLine);

    }else if((strcmp(pCmdLine->arguments[0], "wake") == 0)){

        printf("Handling SIGCONT\n");

        /*Continue the given process*/
        kill(pid, SIGCONT);

        freeCmdLines(pCmdLine);

    }


}

/*Handle cd / procs */
void handle_special(cmdLine* pCmdLine){

    if( strcmp(pCmdLine->arguments[0], "cd") == 0 ){

        /*Wrong cd format*/
        if (pCmdLine->argCount != 2) {

            fprintf(stderr, "Wrong cd format");

        }else {

            if(strcmp(pCmdLine->arguments[1] , "~") == 0) {

                /*Go to $HOME*/
                chdir(getenv("HOME"));

            }else{

                /*Change working directory*/
                chdir(pCmdLine->arguments[1]);

            }

        }

    } else if (strcmp(pCmdLine->arguments[0], "procs") == 0){

        /*If our command is procs*/
        printProcessList(this_process_list);

    }

    freeCmdLines(pCmdLine);

}

/*Handle io redirection*/
void handle_io(cmdLine* pCmdLine){

    /*Check the output path -> STDOUT or file?*/
    if (pCmdLine->outputRedirect != NULL) {

        int fd = open(pCmdLine->outputRedirect, O_RDWR | O_CREAT);
        dup2(fd, 1);
        close(fd);

    }

    /*Check the input path -> STDIN or file?*/
    if (pCmdLine->inputRedirect != NULL) {

        int fd = open(pCmdLine->inputRedirect, O_RDWR);

        /*No file exists in directory -> Exit*/
        if(fd == -1){

            perror("No file with matching filename exists in directory");
            exit(1);

        }
        dup2(fd, 0);
        close(fd);

    }

}

void set_var(char* new_var, char* new_val){

    /*Our list is empty, create a new head*/
    if(var_list[0] == NULL){

        /*No vars at all */
        var_list[0] = malloc(sizeof(pair));
        var_list[0] -> val = malloc(sizeof(new_val));
        strcpy(var_list[0] -> val , new_val);
        var_list[0] -> var = malloc(sizeof(new_var));
        strcpy(var_list[0] -> var, new_var);
        var_list[0] -> next = NULL;

    }else{

        /*Iterate through all the variables, if we have a match change the val, else create a new link*/
        char found = 0;
        pair* temp = var_list[0];

        if(strcmp(temp->var, new_var) == 0){

            /*Found the variable*/
            strcpy(temp->val, new_val);
            found = 1;

        } else {

            while (temp->next != NULL) {

                if (strcmp(temp->var, new_var) == 0) {

                    /*Found the variable*/
                    strcpy(temp->val, new_val);
                    found = 1;
                    break;
                }

            }

        }

        if(found == 0){

            /*Var doesnt exist in our list, create a new link*/
            temp -> next = malloc(sizeof(pair));
            temp -> next -> var = malloc(sizeof(new_var));
            strcpy(temp -> next -> var , new_var);
            temp -> next -> val = malloc(sizeof(new_val));
            strcpy(temp -> next -> val, new_val);
            temp -> next -> next = NULL;

        }

    }


}

void printVarsList(){

    printf("Symbols table:\n");
    pair* temp = var_list[0];
    while(temp != NULL){

        printf("%s : %s\n", temp -> var, temp -> val);
        temp = temp -> next;
    }

}

/*Handle set / vars*/
void handle_vars(cmdLine* pCmdLine){


    if( strcmp(pCmdLine->arguments[0], "set") == 0 ){

        set_var(pCmdLine -> arguments[1] , pCmdLine -> arguments [2]);
        freeCmdLines((pCmdLine));
    } else if (strcmp(pCmdLine->arguments[0], "vars") == 0){

        printVarsList();
        freeCmdLines(pCmdLine);

    }

}

/*Get the value of a variable*/
char* get_val(char* var){

    pair* temp = var_list[0];
    while (temp != NULL){

        if(strcmp(temp -> var, var) == 0)
            return temp->val;

        temp = temp->next;
    }

    /*No match was found*/
    return NULL;

}

/*Replace all the vars with the #$# sign before them*/
void replace_vars(cmdLine* pCmdLine){

    int i;
    for(i = 0;i < pCmdLine -> argCount ;i++){

        if(pCmdLine->arguments[i][0] == '$'){

            char* replaced = get_val(pCmdLine -> arguments[i] + 1);

            /*No match was found, exit*/
            if(replaced == NULL){

                fprintf(stderr, "No match for var :%s was found!\n", pCmdLine -> arguments[i] + 1);
                break;

            }

            replaceCmdArg(pCmdLine, i, replaced);

        }
    }

}

/*Exec regular commands without pipes*/
void exec_reg(cmdLine* pCmdLine){

    int ret;

    /*Fork current process*/
    pid_t pid;
    pid = fork();

    /*Add the process to the list*/
    addProcess(this_process_list, pCmdLine, pid);

    /*Child process*/
    if (pid == 0) {

        /*Input/Output redirection*/
        handle_io(pCmdLine);

        /*If debug mode -> Print PID, commnad*/
        if (debug)
            fprintf(stderr, "PID: %d Command:%s\n", getpid(), pCmdLine->arguments[0]);

        /*No pipe ->Leave current process and execute*/
        ret = execvp(pCmdLine->arguments[0], pCmdLine->arguments);

        /*Failed to execute -> Exit abnormally*/
        if (ret == -1) {

            perror("Error");
            exit(1);

        }

    } else {

        /*Parent process*/
        /*Failed to fork -> exit abnormally*/
        if (pid == -1) {

            perror("Error");
            _exit(1);

        }

        /*Wait for child to finish executing if it is blocking*/
        if (pCmdLine->blocking == 1) {

            int status;
            waitpid(pid, &status, 0);

            updateProcessStatus(this_process_list[0], pid, TERMINATED);

        }

    }

}

/*Exec commands containing pipes*/
void exec_pipe(cmdLine* pCmdLine){

    int ret;

    /*We will fork twice*/
    pid_t pid1,pid2;
    int status1, status2;

    if(debug)
        fprintf(stderr, "parent_process >forking\n");

    /*The pipe itself*/
    int pipefd[2];

    pipe(pipefd);

    /*Fork current process*/
    pid1 = fork();

    /*Add the process to the list*/
    addProcess(this_process_list, pCmdLine, pid1);

    /*Child process*/
    if (pid1 == 0) {

        if(debug)
            fprintf(stderr, "parent_process>created process with id:%d\n",getpid());

        /*Close STDOUT*/
        close(STDOUT_FILENO);

        /*Duplicate write end*/
        dup(pipefd[1]);

        /*Check the input path -> STDIN or file?*/
        if (pCmdLine->inputRedirect != NULL) {

            int fd = open(pCmdLine->inputRedirect, O_RDWR);

            /*No file exists in directory -> Exit*/
            if(fd == -1){

                perror("No file with matching filename exists in directory");
                exit(1);

            }
            dup2(fd, 0);
            close(fd);

        }


        if(debug)
            fprintf(stderr, "child1>redirecting stdout to the write end of the pipe\n");

        if(debug)
            fprintf(stderr, "child1>going to execute cmd: %s\n", pCmdLine ->arguments[0]);

        /*Execute WRITING part*/
        ret  = execvp(pCmdLine ->arguments[0],pCmdLine ->arguments);

        /*Failed to execute -> Exit abnormally*/
        if (ret == -1) {

            perror("Error");
            exit(1);

        }

    } else {

        /*Parent process*/
        /*Failed to fork -> exit abnormally*/
        if (pid1 == -1) {

            perror("Error");
            _exit(1);

        }

        if(debug)
            fprintf(stderr, "parent_process>waiting for child processes to terminate\n");

        /*Close write end*/
        close(pipefd[1]);

        if(debug)
            fprintf(stderr, "parent_process>closing the write end of the pipe\n");

        /*Proceed to fork again*/
        if(debug)
            fprintf(stderr, "parent_process>forking\n");

        /*Fork current process*/
        pid2 = fork();

        /*Child process*/
        if (pid2 == 0) {

            if(debug)
                fprintf(stderr, "parent_process>created process with id:%d\n",getpid());

            /*Close STDIN*/
            close(STDIN_FILENO);

            /*Duplicate read end*/
            dup(pipefd[0]);

            /*Check the output path -> STDOUT or file?*/
            if (pCmdLine->next->outputRedirect != NULL) {

                int fd = open(pCmdLine->next->outputRedirect, O_RDWR | O_CREAT);
                dup2(fd, 1);
                close(fd);

            }

            if(debug)
                fprintf(stderr, "child2>redirecting stdin to the write end of the pipe\n");

            if(debug)
                fprintf(stderr, "child2>going to execute cmd: %s\n",pCmdLine->next->arguments[0]);

            ret  = execvp(pCmdLine->next->arguments[0],pCmdLine->next->arguments);

            /*Failed to execute -> Exit abnormally*/
            if (ret == -1) {

                perror("Error");
                exit(1);

            }

        } else {

            /*Parent process*/
            /*Failed to fork -> exit abnormally*/
            if (pid2 == -1) {

                perror("Error");
                _exit(1);

            }

            /*Close read end*/
            close(pipefd[0]);

            /*Wait for child to finish writing*/
            waitpid(pid1, &status1, 0);
            /*Wait for children*/
            waitpid(pid2, &status2, 0);

            updateProcessStatus(this_process_list[0], pid1, TERMINATED);

            if(debug)
                fprintf(stderr, "parent_process>closing the read end of the pipe\n");

            if(debug)
                fprintf(stderr, "parent_process>waiting for child processes to terminate\n");


        }

    }

    if(debug)
        fprintf(stderr, "parent_process>exiting\n");

}

void execute(cmdLine *pCmdLine) {

    /*Replace all the variable references*/
    replace_vars(pCmdLine);

    /*Check if we are dealing with a cd or procs command*/
    if (strcmp(pCmdLine->arguments[0], "cd") == 0 || strcmp(pCmdLine->arguments[0], "procs") == 0) {

        handle_special(pCmdLine);

    } else if(strcmp(pCmdLine->arguments[0], "suspend") == 0 || strcmp(pCmdLine->arguments[0], "kill") == 0 || strcmp(pCmdLine->arguments[0], "wake") == 0){

        handle_swk(pCmdLine);

    }else if(strcmp(pCmdLine->arguments[0], "set") == 0 || strcmp(pCmdLine->arguments[0], "vars") == 0){

        handle_vars(pCmdLine);

    }else if(pCmdLine -> next == NULL) {

        exec_reg(pCmdLine);

    }else {

        exec_pipe(pCmdLine);

    }

}

void deal_command(char *input) {

    if (strcmp(input, "quit\n") == 0) {

        /*Delete our list and exit*/
        freeProcessList(this_process_list[0]);
        free(this_process_list);
        freeVarsList(var_list[0]);
        free(var_list);
        exit(0);

    } else {

        /*Execute line*/
        execute(parseCmdLines(input));

    }
}

int main(int argc, char *argv[]) {

    /*Update our program flags*/
    checkMode(argc, argv);

    /*Create an empty process list*/
    this_process_list = malloc(sizeof(process));
    this_process_list[0] = NULL;

    /*Create an empty variables list*/
    var_list = malloc(sizeof(pair));
    var_list[0] = NULL;

    while (1) {

        /*Catch signals #LINE_PARSER**/
        get_signal();

        /*Get the name of the current directory*/
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);

        /*Print our path*/
        printf("~%s :", cwd);

        /*Get a line from the user*/
        char user_line[2048];
        fgets(user_line, sizeof(user_line), stdin);

        /*Execute the line*/
        deal_command(user_line);

    }

    return 0;
}
