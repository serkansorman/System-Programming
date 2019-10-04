#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include<sys/wait.h> 
#include<unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include<signal.h> 

#define F_SETPIPE_SZ 1031
#define F_GETPIPE_SZ 1032

/*The function returns the size in blocks of the file given by path 
or -1 if path does not corresponds to an ordinary file.*/
int sizepathfun (char *path);

/* Traverses the tree, starting at the path. Creates a new process for each
directory to get the sizes. It applies the pathfun function to each file that 
it encounters in the traversal. The function returns the sum of the positive 
return values of pathfun,or -1 (if it failed to traverse any subdirectory) */
int postOrderApply (char *path, int pathfun (char *path1));

/* This function is implemented as a wrapper of postOrderApply.
   It creates first child of main process to traverse on directories.
   After all child process write results to fifo and all childs are 
   finished main process will read the FIFO, prints result as standart output*/
void postOrderApplyWrapper(char *path,int pathfun (char *path1));

/*Checks current path is a directory or not */
int isDirectory(char *path);
/*Checks current path is a regular file or not */
int isRegularFile(char *path);
/*Check user has a read permission of file*/
int canUserRead(char *path);
/* Handle Ctrl+C */
void sigint_handler();
/* Handle Ctrl+Z */
void sigstop_handler();

int flag = 0; /* -z flag */
int fifo; /* fd of fifo */
int fd2[2]; /*Global pipe to count process */
char *outputFifo = "/tmp/151044057sizes";


void sigint_handler(){
	unlink(outputFifo); /* unlink fifo */
	printf("Ctrl+C is handled\n");
	kill(0, SIGKILL);
}

void sigstop_handler(){
	unlink(outputFifo); /* unlink fifo */
	printf("Ctrl+Z is handled\n");
	kill(0, SIGKILL);
}

int sizepathfun (char *path){
	struct stat st;
    if (stat(path, &st) != 0){
    	fprintf(stderr,"Can not determine size of %s\n",path);
    	return -1;
	}
	return st.st_size;
}

int isDirectory(char *path) {
	struct stat statbuf;
	if (lstat(path, &statbuf) == -1){
		return 0;
	}
	else 
		return S_ISDIR(statbuf.st_mode);
}


int isRegularFile(char *path) {
	struct stat statbuf;
	if (lstat(path, &statbuf) == -1)
		return 0;
	else 
		return S_ISREG(statbuf.st_mode);
}

int canUserRead(char *path){
    struct stat ret;
    if (lstat(path, &ret) == -1) {
        return 0;
    }
    return (ret.st_mode & S_IRUSR);     
}


void postOrderApplyWrapper(char *path,int pathfun (char *path1)){

	int tempSize,numOfChild = 0;
	pid_t pid;
	char line[1024];

	if(!canUserRead(path)){ /* Can not access to file */
    	printf("Cannot access to %s\n",path);
	}
	else if(isDirectory(path)){/* Checks given path is directory and create new process */

		if (mkfifo(outputFifo, 0666) == -1) {
			perror("mkfifo");
			exit(EXIT_FAILURE);
		}

    	fifo = open(outputFifo, O_RDWR | O_NONBLOCK);
    	fcntl(fifo, F_SETPIPE_SZ, 1024 * 1024); /* Set max pipe size */
    	//printf("%ld\n",(long)fcntl(fifo, F_GETPIPE_SZ));

		if(pipe(fd2) == -1){// open pipe for process counter
			perror("pipe");
    		exit(EXIT_FAILURE);
		} 
		pid = fork();
		if(pid == 0){
			++numOfChild; /*First child */
			write(fd2[1],&numOfChild, sizeof(int)); /* Write num of child to fifo */
			postOrderApply(path,pathfun);
			_exit(EXIT_SUCCESS);
		}
		else if(pid > 0){/* Main process */
			while(wait(NULL) != -1); /* Waits child processes */

			close(fd2[1]);
			read(fd2[0], &numOfChild, sizeof(numOfChild)); /* Read number of childs from pipe */
			close(fd2[0]);
			
			printf("PID\tSIZE\tPATH\n");
			FILE *fp = fdopen(fifo,"r");		/* Read FIFO */
			while(fgets(line, 1024, fp))
				printf("%s",line );

			close(fifo);
			fclose(fp);
			unlink(outputFifo); /* unlink fifo */

			printf("%d child processes created. Main process is %d.\n",numOfChild,getpid());

		}
		else{
			perror("fork");
			exit(EXIT_FAILURE);	
		}
	}
	else if(isRegularFile(path)){/* Checks current file is regular*/
		if((tempSize = pathfun(path)) != -1) 
			printf("%d\t%d\t%s\n",getpid(),tempSize / 1024,path);
	}	
	else{ /* Special file */
		printf("%d\tSpecial file %s\n",getpid(),path);
	}		
}

int postOrderApply (char *path, int pathfun (char *path1)){
	
	DIR *dir;
	struct dirent *ent;
	
	int sumSize = 0;
	int tempSize = 0,numOfChild = 0;
	char currentPath[512],*tempStr = NULL;

	pid_t pid;
	int fd[2]; /* Pipe for passing size to parent */

	
	if ((dir = opendir (path)) != NULL) {
	  	while ((ent = readdir (dir)) != NULL) {
	  	 	
     	 	if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){
				sprintf(currentPath,"%s/%s", path, ent->d_name); /* Concatanate current file name and path */
				if (!canUserRead(currentPath)){ /* Check user can read the file */
     	 			tempStr = malloc(1024);
    				sprintf(tempStr,"%d\t-1\tCannot access to %s\n",getpid(),currentPath);
					write(fifo, tempStr, strlen(tempStr));
					free(tempStr);   				
				}
    			else{
    				if(isDirectory(currentPath)){/* Checks current file is directory and create new process*/
    					if(flag == 1){
    					    if(pipe(fd) == -1){ /* open pipe for pass sub dir size to parent dir */
    							perror("pipe");
    							exit(EXIT_FAILURE);
    						}	
    					}
     	 				pid = fork();
     	 				if(pid == 0){/*Child*/

     	 					/*Updates number of child process on pipe*/
							read(fd2[0], &numOfChild, sizeof(numOfChild));
							++numOfChild;
							write(fd2[1], &numOfChild, sizeof(numOfChild));
				
     	 					while ((closedir(dir) == -1) && (errno == EINTR));
     	 					tempSize = postOrderApply(currentPath,pathfun);

     	 					/* -z flag, sub dir sizes written to pipe by child process*/
     	 					if(flag == 1){
	     	 					close(fd[0]);
								write(fd[1], &tempSize, sizeof(tempSize));
								close(fd[1]);
							}

     	 					_exit(EXIT_SUCCESS);
     	 				}
     	 				else if(pid > 0){/*Parent*/

     	 					/* -z flag, sub dir sizes read by parent dir and add to total size*/
     	 					if(flag == 1){ 
     	 						close(fd[1]);
     	 						read(fd[0], &tempSize, sizeof(tempSize));
     	 						close(fd[0]);
     	 						sumSize += tempSize;
     	 					}
     	 				}
     	 				else{
     	 					perror("fork");
     	 					exit(EXIT_FAILURE);
     	 				}	
					}	
					else if(isRegularFile(currentPath)){/* Checks current file is regular, then add size*/
						if((tempSize = pathfun(currentPath)) != -1) /*Checks size can obtained succesfully */
							sumSize += tempSize;	
					}	
					else{ /* Special file */
						
						tempStr = malloc(1024);
						sprintf (tempStr,"%d\t-1\tSpecial_file_%s\n",getpid(), ent->d_name);
						write(fifo, tempStr, strlen(tempStr));
						free(tempStr);
						
					}
    			}
    			while(wait(NULL) != -1); /* Waits child processes */
			}		
     	}		
	}
	else{
		tempStr = malloc(1024);
		sprintf(tempStr,"%d\t-1\tCannot access to %s\n",getpid(),currentPath);
		write(fifo, tempStr, strlen(tempStr));
		free(tempStr);		
		return -1;
	}   
	
	tempStr = malloc(1024);
	sprintf(tempStr,"%d\t%d\t%s\n",getpid(), sumSize / 1024, path);
	write(fifo, tempStr,strlen(tempStr));
	free(tempStr);
        
    /*Close directory and fd of fifo*/
    while ((closedir(dir) == -1) && (errno == EINTR));
    close(fifo);
    return sumSize;
} 
		

int main(int argc, char* argv[]){
	signal(SIGINT,sigint_handler); 		/* Ctrl+C */
	signal(SIGTSTP,sigstop_handler);	/* Ctrl+Z */

	if(argc != 2 && argc != 3){ /* number of argument must be 2 or 3 */
		fprintf(stderr,"Usage: %s rootpath OR %s -z rootpath\n",argv[0],argv[0]);
		return 1;
	}
	else if(argc == 2){
		postOrderApplyWrapper(argv[1],sizepathfun);/*Second argument is a root path */
	}
	else{ /* argc == 3 */
		if(strcmp(argv[1],"-z") != 0){ /* Check -z flag */
			fprintf(stderr,"Invalid option %s\nUsage: %s -z rootpath\n",argv[1],argv[0]);
			return 1;
		} 
		flag = 1; /* -z flag accepted*/
		postOrderApplyWrapper(argv[2],sizepathfun); /*Third argument is a root path */
	}
	return 0; 
}