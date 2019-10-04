#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include<sys/wait.h> 
#include<unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>

#define GRN "\x1b[32m"
#define RESET "\x1b[0m"
#define MAX_LEN 1024

char *builtins[] = {"cd","help","exit"};
char *utilities[] = {"lsf","pwd","cat","wc","bunedu"};
char *right_utilities[] = {"cat","wc","bunedu"};
char cwd[MAX_LEN];
char exePath[MAX_LEN];

/*Checks commmand is builtin*/
int isBuiltin(char* command);
/*Checks command is utility*/
int isUtility(char* command);
/*Checks command is right utility for pipe command*/
int isRightUtility(char *command);
/*Change directory according to given path */
int gtushell_cd(char* path);
/*print the list of supported commands*/
void gtushell_help();
/*exit the shell */
void gtushell_exit();
/* Seperates  <,>,| from command */
int parseSymbol(char* command, char** parsedCommand, char* targetSymbol);
/*Seperates spaces from commands*/
void parseSpace(char* command, char** parsedCommand);
/*Seperate space from left and right parts of piped and redirection command*/
void parseLeftAndRight(char* parsedSymbol[2], char** parsedLeft, char** parsedRight);
/*Checks command has valid arguments on left and right end of pipe */
int isValidPipeCommand(char** parsed, char** parsedRight);
/*Checks command has valid arguments on left and right end of redirection*/
int isValidRedirectCommand(char** parsed, char** parsedRight,int symbol);
/*Checks utility command has valid arguments */
int isValidUtilityCommand(char** parsed);
/*Parse given command according to command type */
void parseCommand(char* command, char** parsedCommand, char** parsedRight);
/*Execute left and right end of piped command */
void exePipeCommand(char** leftSide, char **rightSide);
/*execute regular command */
void exeNormalCommand(char **args);
/*Execute command and write result to given file */
void writeFile(char *fileName, char** leftArgs);
/*Execute command and read input from given file*/
void readFile(char *fileName, char** leftArgs);
/*Execute redirection command which has < or > */
void exeRedirection(char** leftSide, char **rightSide,int symbol);
/*Catch the sigterm,sigint,sigtstp and exit shell properly */
void handle_signals(int signo);
/*Checks command is empty or consist of fully space */
int isCommandEmpty(char* command);

int isBuiltin(char* command){
    int i;
    for(i=0; i<3; ++i)
        if(strcmp(builtins[i],command) == 0)
            return 1;
    return 0;
}

int isUtility(char* command){
    int i;
    for(i=0; i<5; ++i)
        if(strcmp(utilities[i],command) == 0)
            return 1;
    return 0;
}

int isRightUtility(char *command){
    int i;
    for(i=0; i<3; ++i)
        if(strcmp(right_utilities[i],command) == 0)
            return 1;
    return 0;
}

int gtushell_cd(char* path){
    if(chdir(path) == -1){
        printf("%s: ",path);
        fflush(stdout);
        perror("");
        return 1;
    }
    else{
        getcwd(cwd, sizeof(cwd));
        return 0;
    }
}

void gtushell_help(){
    printf("\n#### WELCOME TO GTUSHELL ####\n" 
            "\n#Utilities\t\t#Builtins\n"
            ">> lsf\t\t\t>> cd [directory]\n>> cat [filename]\t"
            ">> help\n>> wc [filename]\t>> exit\n>> pwd\n>> bunedu [-z] [path]\n\n\n");
    printf("> : redirection right\nexe > filename\n\n");
    printf("< : redirection left\nexe < filename\n\n");
    printf("| : pipe operation\nexe | exe\n");

}

void gtushell_exit(){
    printf("Bye Bye\n");
    exit(0);
}

int parseSymbol(char* command, char** parsedCommand, char* targetSymbol) { 
    int i = 0; 
    while ((parsedCommand[i++] = strsep(&command, targetSymbol)) != NULL);
    if (parsedCommand[1] != NULL){ 
        return 1; 
    } 
    return 0; // Command has not | , > , <
} 
  
void parseSpace(char* command, char** parsedCommand) { 
    int i = 0;
    while ((parsedCommand[i] = strsep(&command, " ")) != NULL){
        if( strlen(parsedCommand[i]) != 0) /* In case of command has multiple space */
           ++i;
    }  
}

int isValidPipeCommand(char** args, char** parsedRight){

    if(!isUtility(args[0])){
        printf("%s: invalid left pipe command\n",args[0]);
        printf("Usage: from [wc, bunedu, lsf, cat or pwd] to [wc, cat or bunedu]\n");
        return 0;
    }
    else if(!isRightUtility(parsedRight[0])){
        printf("%s: invalid right pipe command\n",parsedRight[0]);
        printf("Usage: from [wc, bunedu, lsf, cat or pwd] to [wc, cat or bunedu]\n");
        return 0;
    }
    return 1; 
}

int isValidRedirectCommand(char** args, char** parsedRight,int symbol){

    if(!isUtility(args[0])){
        printf("%s: invalid command\n",args[0]);
        printf("Usage: utilities to/from file\n");
        return 0;
    }
    return 1;    
}

int isValidUtilityCommand(char** args){

    if(isBuiltin(args[0])){

        if(strcmp(args[0],"cd") == 0)
            gtushell_cd(args[1]);
        else if(strcmp(args[0],"help") == 0)
            gtushell_help();
        else
            gtushell_exit();

        return 0;
    }
    else if(isUtility(args[0]))
        return 1;
    else{
        printf("%s: command not found\n",args[0]);
        return 0;
    }

}

void parseLeftAndRight(char* parsedSymbol[2], char** parsedLeft, char** parsedRight){
    parseSpace(parsedSymbol[0], parsedLeft);    /*Parse space left */
    parseSpace(parsedSymbol[1], parsedRight);   /*Parse space right */
}


void parseCommand(char* command, char** parsedCommand, char** parsedRight) { 
  
    char* parsedSymbol[2];   /* Parse given pipe or redirection command to left and right part */
    int hasPipe = 0;         /* Flag for checking command has | */
    int hasRedirectRight = 0;  /* Flag for checking command has > */
    int hasRedirectLeft = 0; /* Flag for checking command has < */

    /* Parse piped command */
    hasPipe = parseSymbol(command, parsedSymbol, "|");
    if (hasPipe) { 
        parseLeftAndRight(parsedSymbol,parsedCommand,parsedRight);
        if(isValidPipeCommand(parsedCommand,parsedRight)){
            exePipeCommand(parsedCommand, parsedRight);         /* exe | exe */
            return ;
        }
        return ;

    }/* Parse redirection > command */
    hasRedirectRight = parseSymbol(command, parsedSymbol, ">");
    if(hasRedirectRight){
        parseLeftAndRight(parsedSymbol,parsedCommand,parsedRight);
        if(isValidRedirectCommand(parsedCommand,parsedRight,1)){
            exeRedirection(parsedCommand, parsedRight,1);       /* exe > file */
            return ;
        }
        return ;

    }/* Parse redirection < command */
    hasRedirectLeft = parseSymbol(command, parsedSymbol, "<");
    if(hasRedirectLeft){
        parseLeftAndRight(parsedSymbol,parsedCommand,parsedRight);
        if(isValidRedirectCommand(parsedCommand,parsedRight,2)){
            exeRedirection(parsedCommand, parsedRight,2);       /* exe < file */
            return ;
        }
        return ;
    }
     
    /*Parse normal command */
    parseSpace(command, parsedCommand);
    if(isValidUtilityCommand(parsedCommand))
        exeNormalCommand(parsedCommand);
    return ;   
}


void exePipeCommand(char** leftSide, char **rightSide){

    int newfd[2];
    char tempStr[MAX_LEN];

    if(pipe(newfd) == -1){
        perror("pipe");
        exit(1);
    }
    pid_t pid = fork();
    if(pid == 0){

        /*Replace stdout with pipe */
        close(newfd[0]); 
        dup2(newfd[1], STDOUT_FILENO); 
        close(newfd[1]); 
        
        sprintf(tempStr,"%s/%s", exePath, leftSide[0]); 
        execvp(tempStr,leftSide);
   
        _exit(0);
    }
    else if(pid > 0){
        wait(NULL);
        pid_t pid = fork();
        if(pid == 0){ 

            /*Replace stdin with pipe */
            close(newfd[1]); 
            dup2(newfd[0], STDIN_FILENO); 
            close(newfd[0]); 
            
            sprintf(tempStr,"%s/%s", exePath, rightSide[0]);  // Set command exe path
            execvp(tempStr,rightSide);
            _exit(0);
        }
        else if(pid > 0){
            wait(NULL); 
        }
        else{
            perror("fork");
            exit(1);
        }   
    }
    else{
        perror("fork");
        exit(1);
    }
}


void exeNormalCommand(char **args){

    char tempStr[MAX_LEN];
    pid_t pid = fork();
    if(pid == 0){
        sprintf(tempStr,"%s/%s", exePath, args[0]); // Set command exe path
        execvp(tempStr,args);       
        
        _exit(0);
    }
    else if(pid > 0){
        wait(NULL); 
    }
    else{
        perror("fork");
        exit(1);
    }
}

void writeFile(char *fileName, char** leftArgs){
    char tempStr[MAX_LEN];
    int fd = open(fileName,O_CREAT | O_WRONLY ,0640);
    pid_t pid = fork();
    if(pid == 0){
        dup2(fd, STDOUT_FILENO);
        sprintf(tempStr,"%s/%s", exePath, leftArgs[0]); // Set command exe path
        execv(tempStr,leftArgs);
        _exit(0);
    }
    else if(pid > 0){
        wait(NULL);
        close(fd);  
    }
    else{
        perror("fork");
        exit(1);
    }

}

void readFile(char *fileName, char** leftArgs){

    int fd = open(fileName, O_RDONLY);
    char tempStr[MAX_LEN];

    if(fd == -1){
        printf("%s: Can not open file\n",fileName);
        return;
    }
    pid_t pid = fork();
    if(pid == 0){
        dup2(fd, STDIN_FILENO);

        sprintf(tempStr,"%s/%s", exePath, leftArgs[0]); // Set command exe path
        execv(tempStr,leftArgs);
    }
    else if(pid > 0){
        wait(NULL);
        close(fd);  
    }
    else{
        perror("fork");
        exit(1);
    }
}

void exeRedirection(char** leftSide, char **rightSide,int symbol){
    if(symbol == 1){ /* exe > filename */
        writeFile(rightSide[0],leftSide);
    }
    else{ /* exe < filename */
        readFile(rightSide[0],leftSide);
    }
}

void handle_signals(int signo){
    
    if(signo == SIGTERM){
       printf("gtushell is terminated by SIGTERM\n");
       exit(0); 
    }
    else if(signo == SIGTSTP){
        printf("gtushell is terminated by SIGTSTP\n");
        exit(0);
    }
    else if(signo == SIGINT){
        printf("gtushell is terminated by SIGINT\n");
        exit(0);
    }
    
}

int isCommandEmpty(char* command){

    /* remove new line from command */
    if(command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = '\0';                           

    if(strlen(command) == 0)/*empty command */
        return 1;
    for(int i=0; i<strlen(command); ++i) /* Only has space */
        if(command[i] != ' ')
            return 0;
    return 1;
}


int main(int argc, char **argv)
{
    char command[MAX_LEN];
    char  *parsedCommand[MAX_LEN]; 
    char  *parsedCommandSymbol[MAX_LEN];
    char commandHistory[MAX_LEN][MAX_LEN];
    char tempStr[MAX_LEN];
    char *username = getenv("USER");
    int numOfCommand=0,nthCommand = 0;

    signal(SIGTERM,handle_signals);
    signal(SIGTSTP,handle_signals);
    signal(SIGINT,handle_signals);

    getcwd(exePath, sizeof(exePath));
    strcpy(cwd, exePath);
    
    while (1) { 
       
        printf(GRN"%s@gtushell: "RESET,username);
        chdir(cwd); /*Update current directory */
        
        /*Take command and check is empty */
        fgets(command,sizeof(command),stdin);
        
        if(isCommandEmpty(command))
            continue;

        /*Check command start with ! */
        if(command[0] == '!'){
            strncpy(tempStr, command+1, strlen(command));
            nthCommand = atoi(tempStr); /* Parse to integer */
            if(nthCommand >= 1 && nthCommand <= numOfCommand)
                strcpy(command,commandHistory[numOfCommand-nthCommand]); /*Get command from history */
        }

        strcpy(commandHistory[numOfCommand++],command); /*save command to history */
        parseCommand(command,parsedCommand, parsedCommandSymbol); /*Parse command to arguments */
              
    } 
    return 0;
}