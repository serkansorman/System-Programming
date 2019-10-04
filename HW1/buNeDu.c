#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>

/*The function returns the size in blocks of the file given by path 
or -1 if path does not corresponds to an ordinary file.*/
int sizepathfun (char *path);

/* Traverses the tree, starting at the path. It applies the pathfun 
function to each file that it encounters in the traversal. The function
returns the sum of the positive return values of pathfun,
or -1 (if it failed to traverse any subdirectory) */
int depthFirstApply (char *path, int pathfun (char *path1));

/* This function is implemented as a wrapper of depthFirstApply.
   if given path is not directory, checks file acces permissions and types
   otherwise call the depthFirstApply to traverse the tree, starting at the root path */
void depthFirstApplyWrapper(char *path,int pathfun (char *path1));

int flag = 0; /* -z flag */

int sizepathfun (char *path){
	
	struct stat st;
    if (stat(path, &st) != 0){
    	fprintf(stderr,"Can not determine size of %s\n",path);
    	return -1;
	}
	
	return st.st_size;
}


void depthFirstApplyWrapper(char *path,int pathfun (char *path1)){
	
	int temp = 0;
	struct stat statbuf;
	lstat(path, &statbuf);
	
	if(access(path, R_OK) != 0){ /* Can not access to file */
    	fprintf(stderr,"Cannot access to %s",path);
    	perror (" ");
	}
	else if(S_ISDIR(statbuf.st_mode)){/* Checks given path is directory */
		if(depthFirstApply(path,pathfun) != -1)
			printf("%s \n", path);
	}
	else if(S_ISREG(statbuf.st_mode)){/* Checks current file is regular*/
		temp = pathfun(path);
		if(temp != -1) /*Checks size can obtained succesfully */
			printf("%d	%s\n", temp / 1024,path); /* Convert to KB */
	}	
	else{ /* Special file */
		printf ("Special file %s\n",path);
	}	
}


int depthFirstApply (char *path, int pathfun (char *path1)){
	
	DIR *dir;
    struct stat statbuf; 
	struct dirent *ent;
	
	int sumSize = 0;
	int temp = 0;
	char *currentPath = NULL;

	if ((dir = opendir (path)) != NULL) {
	  	while ((ent = readdir (dir)) != NULL) {
	  	 	
     	 	if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){

     	 		currentPath = malloc(sizeof(path)+sizeof(ent->d_name)+sizeof(char));/* path/fileName */
				sprintf(currentPath,"%s/%s", path, ent->d_name); /* Concatanate current file name and path */
     	 		lstat(currentPath, &statbuf);

				if (access(currentPath, R_OK) != 0){ /* Can not access to file */
    				fprintf(stderr,"Cannot access to %s",currentPath);
    				perror (" ");
				}
    			else{
    				if(S_ISDIR(statbuf.st_mode)){/* Checks current file is directory */
     	 				/* -z flag */
	     	 			if(flag == 1){
	     	 				/* Traverses in current directory recursively and includes sub directories size*/
	     	 				temp = depthFirstApply(currentPath,pathfun);
	     					if(temp != -1){/*Checks size can obtained succesfully */
	     						sumSize += temp;
	     						printf ("%s \n", currentPath);
	     					} 	 
						}	
	     				else{
	     					depthFirstApply(currentPath,pathfun); /* Not includes sub directories size */
	     					printf ("%s \n", currentPath);
						}
							
					}	
					else if(S_ISREG(statbuf.st_mode)){/* Checks current file is regular, then add size*/
						temp = pathfun(currentPath);
						if(temp != -1) /*Checks size can obtained succesfully */
							sumSize += temp;	
					}	
					else{ /* Special file */
			 			printf ("Special file %s\n", ent->d_name);
					}
    			}
    			free(currentPath);
			}		
     	}		
	}
	else {
	  fprintf(stderr,"Cannot access to %s",path);
	  perror (" ");	  
	  return -1;
	}   

    printf("%d	", sumSize / 1024);
    /*Close directory*/
    while ((closedir(dir) == -1) && (errno == EINTR));
    return sumSize;
} 
		

int main(int argc, char* argv[]){
	
	if(argc != 2 && argc != 3){ /* number of argument must be 2 or 3 */
		fprintf(stderr,"Usage: %s rootpath OR %s -z rootpath\n",argv[0],argv[0]);
		return 1;
	}
	else if(argc == 2){
		depthFirstApplyWrapper(argv[1],sizepathfun);/*Second argument is a root path */
	}
	else{ /* argc == 3 */
		if(strcmp(argv[1],"-z") != 0){ /* Check -z flag */
			fprintf(stderr,"Invalid option %s\nUsage: %s -z rootpath\n",argv[1],argv[0]);
			return 1;
		} 
		flag = 1; /* -z flag accepted*/
		depthFirstApplyWrapper(argv[2],sizepathfun); /*Third argument is a root path */
	}
	
	return 0; 
}
