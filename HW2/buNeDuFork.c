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

/*The function returns the size in blocks of the file given by path 
or -1 if path does not corresponds to an ordinary file.*/
int sizepathfun (char *path);

/* Traverses the tree, starting at the path. Creates a new process for each
directory to get the sizes. It applies the pathfun function to each file that 
it encounters in the traversal. The functionreturns the sum of the positive 
return values of pathfun,or -1 (if it failed to traverse any subdirectory) */
int postOrderApply (char *path, int pathfun (char *path1));

/* This function is implemented as a wrapper of postOrderApply.
   It creates first child of main process to traverse on directories.
   After all child process is finished if flag is z, it parses file and
   generate output for -z flag*/
void postOrderApplyWrapper(char *path,int pathfun (char *path1));

/*Checks current path is a directory or not */
int isDirectory(char *path);
/*Checks current path is a regular file or not */
int isRegularFile(char *path);
/*Check user has a read permission of file*/
int canUserRead(char *path);
/*Lock the file using fcntl*/
void lockFile(FILE *fp);
/*Unlock the file using fcntl*/
void unlockFile(FILE *fp);
/*Counts number of child process created in a program */
int countProcess(int arr[],int n);


int flag = 0; /* -z flag */
FILE *fp; /*Global file */

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

void lockFile(FILE *fp){
	struct flock f;
	 
	f.l_type = F_WRLCK;
	f.l_start = 0;
	f.l_whence = SEEK_SET;
	f.l_len = 0;
	
	fcntl(fileno(fp), F_SETLK, &f);
}
  
void unlockFile(FILE *fp){
	struct flock f;
	
	f.l_type = F_UNLCK;
	f.l_start = 0;
	f.l_whence = SEEK_SET;
	f.l_len = 0;
	 
	fcntl(fileno(fp), F_SETLK, &f);
}

int countProcess(int arr[],int n){
	int i,j,counter = 1; 
    for (i = 1; i < n; i++) {  
    	for (j = 0; j < i; j++) 
        	if (arr[i] == arr[j]) 
                break; 
        if (i == j) 
            ++counter; 
    }
    return counter;
}


void postOrderApplyWrapper(char *path,int pathfun (char *path1)){

	int i = 0,j = 0,status,tempSize,n = 0;
	pid_t pid;
	char line[1024];
	int *pidArr;
	int *sizeArr;
	char (*pathArr)[1024];

	/* Clean the file at the beginning */
	fp = fopen("151044057sizes.txt", "w");
	fclose(fp);
    
	if(!canUserRead(path)){ /* Can not access to file */
    	printf("Cannot access to %s",path);
	}
	else if(isDirectory(path)){/* Checks given path is directory and create new process */
		pid = fork();
		if(pid == 0){ 
			postOrderApply(path,pathfun);
			_exit(EXIT_SUCCESS);
		}
		else if(pid > 0){/* Main process */
			waitpid(pid,&status,0); /* waits to all child process finish */
			
			fp = fopen("151044057sizes.txt", "r");
		    if(fp == NULL){
				perror("File");
				exit(EXIT_FAILURE);
			}

			/*Counts number of line*/
			while(fgets(line, 1024, fp))
				++n;

			pidArr = malloc(n * sizeof(int));
			sizeArr = malloc(n * sizeof(int));
			pathArr = malloc(n * sizeof(*pathArr));

			/* Parse file to arrays */
			fseek(fp, 0, SEEK_SET);
			while(fgets(line, 1024, fp)){
				sscanf(line,"%d %d %[^\n]",&pidArr[i],&sizeArr[i],pathArr[i]);
				++i;
			}
			if(flag == 1){/* -z flag */
				/*Add sub directories size to parent directory */
				for(i=n-1; i >= 0; --i){
					for(j=n-1; j>=0; --j){
						if(i != j && strstr(pathArr[j], pathArr[i]) &&
							sizeArr[i] != -1 && sizeArr[j] != -1){ /* Do not include special file sizes */
							sizeArr[i] += sizeArr[j];
						}
					}
				}
			}
			/* Outputs the result */
			printf("PID\tSIZE\tPATH\n");
			for(i=0; i<n; ++i){
				if(sizeArr[i] == -1)
					printf("%d\t\t%s\n",pidArr[i],pathArr[i]);
				else
					printf("%d\t%d\t%s\n",pidArr[i],sizeArr[i],pathArr[i]);
			}
			printf("%d child processes created. Main process is %d.\n",countProcess(pidArr,n),getpid());

			free(pidArr);
			free(sizeArr);
			free(pathArr);
			fclose(fp);
		}
		else{
			perror("fork");
			exit(EXIT_FAILURE);	
		}
	}
	else if(isRegularFile(path)){/* Checks current file is regular*/
		if((tempSize = pathfun(path)) != -1) 
			printf("%d\t%d\t%s\n",getpid(),tempSize,path);
	}	
	else{ /* Special file */
		printf("%d\tSpecial file %s\n",getpid(),path);
	}	
}

int postOrderApply (char *path, int pathfun (char *path1)){
	
	DIR *dir;
	struct dirent *ent;
	
	int sumSize = 0;
	int tempSize = 0;
	int status;
	char currentPath[512];

	pid_t pid;

	fp = fopen ("151044057sizes.txt", "a");
	if(fp == NULL){
		perror("File");
		exit(EXIT_FAILURE);
	}

	if ((dir = opendir (path)) != NULL) {
	  	while ((ent = readdir (dir)) != NULL) {
	  	 	
     	 	if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){
				sprintf(currentPath,"%s/%s", path, ent->d_name); /* Concatanate current file name and path */
				if (!canUserRead(currentPath)){ /* Check user can read the file */
     	 			lockFile(fp);
    				fprintf(fp,"%d\t-1\tCannot access to %s\n",getpid(),currentPath);
    				unlockFile(fp);
				}
    			else{
    				if(isDirectory(currentPath)){/* Checks current file is directory and create new process*/			
     	 				pid = fork();
     	 				if(pid == 0){/*Child*/
     	 					while ((closedir(dir) == -1) && (errno == EINTR));
     	 					fclose(fp);
     	 					postOrderApply(currentPath,pathfun);
     	 					_exit(EXIT_SUCCESS);
     	 				}
     	 				else if(pid > 0){/*Parent*/
     	 					waitpid(pid,&status,0);
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
						lockFile(fp);
						fprintf (fp,"%d\t-1\tSpecial file %s\n",getpid(), ent->d_name);
						unlockFile(fp);
					}
    			}
			}		
     	}		
	}
	else{
		lockFile(fp);
		fprintf(fp,"%d\t-1\tCannot access to %s\n",getpid(),currentPath);
		unlockFile(fp);
		return -1;
	}   
	/* Lock file to prevent write to the same file at the same time.*/ 
	lockFile(fp); 
    fprintf(fp,"%d\t%d\t%s\n",getpid(), sumSize, path);
	unlockFile(fp); /* Unlock file */

    /*Close directory*/
    while ((closedir(dir) == -1) && (errno == EINTR));
    fclose(fp);
    return sumSize;
} 
		

int main(int argc, char* argv[]){
	
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
