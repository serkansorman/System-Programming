#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include<sys/wait.h> 
#include<unistd.h>
#include <sys/types.h>



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


int flag = 0; /* -z flag */


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

	int tempSize;

	if(!canUserRead(path)){ /* Can not access to file */
    	fprintf(stderr,"Cannot access to %s",path);
    	perror (" ");
	}
	else if(isDirectory(path)){/* Checks given path is directory and create new process */
		postOrderApply(path,pathfun);	
	}
	else if(isRegularFile(path)){/* Checks current file is regular*/
		if((tempSize = pathfun(path)) != -1) 
			printf("%d\t%s\n",tempSize,path);
	}	
	else{ /* Special file */
		printf("\tSpecial file %s\n",path);
	}		
}

int postOrderApply (char *path, int pathfun (char *path1)){
	
	DIR *dir;
	struct dirent *ent;
	
	int sumSize = 0;
	int tempSize = 0;
	char *currentPath;


	if ((dir = opendir (path)) != NULL) {
	  	while ((ent = readdir (dir)) != NULL) {
	  	 	
     	 	if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){
				currentPath = malloc(sizeof(path)+sizeof(ent->d_name)+sizeof(char));/* path/fileName */
				sprintf(currentPath,"%s/%s", path, ent->d_name); /* Concatanate current file name and path */
				if (!canUserRead(currentPath)){ /* Check user can read the file */
    				printf("Cannot access to %s\n",currentPath);		
				}
    			else{
    				if(isDirectory(currentPath)){/* Checks current file is directory and create new process*/
    					
	 	 				if(flag == 1){
	     	 				/* Traverses in current directory recursively and includes sub directories size*/
	     	 				tempSize = postOrderApply(currentPath,pathfun);
	     					if(tempSize != -1)/*Checks size can obtained succesfully */
	     						sumSize += tempSize; 	 
						}	
	     				else
	     					postOrderApply(currentPath,pathfun); /* Not includes sub directories size */	
					}	
					else if(isRegularFile(currentPath)){/* Checks current file is regular, then add size*/
						if((tempSize = pathfun(currentPath)) != -1) /*Checks size can obtained succesfully */
							sumSize += tempSize;	
					}	
					else{ /* Special file */
						printf ("\tSpecial file %s\n", ent->d_name);
					}
    			}
    			free(currentPath);
			}		
     	}		
	}
	else{
	  printf("Cannot access to %s\n",path); 
	  return -1;
	}   


	printf("%d\t%s\n",sumSize, path);
        
    /*Close directory */
    while ((closedir(dir) == -1) && (errno == EINTR));
    return sumSize;
} 
		

int main(int argc, char* argv[]){


	char path[1024];
	
	if(argc > 3){ /* number of argument must be <= 3 */
		fprintf(stderr,"Usage: %s rootpath OR %s -z rootpath\n",argv[0],argv[0]);
		return 1;
	}
	if(argc == 1){
		fgets(path,sizeof(path),stdin);
        if(path[strlen(path) - 1] == '\n')
            path[strlen(path) - 1] = '\0'; 
		postOrderApplyWrapper(path,sizepathfun);
	}
	else if(argc == 2){
		if(strcmp(argv[1],"-z") == 0){ /* bunedu -z */
		
			/*Read path from pipe */
			fgets(path,sizeof(path),stdin);
        	if(path[strlen(path) - 1] == '\n')
            	path[strlen(path) - 1] = '\0'; 
			flag = 1; /* -z flag accepted*/
			postOrderApplyWrapper(path,sizepathfun);
		}
		else
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