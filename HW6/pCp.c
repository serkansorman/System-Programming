#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>	
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <libgen.h>

#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)

struct file{
    char name[255];
    int fd[2];		// fd[0] read, fd[1] write
};

static pthread_mutex_t bufferLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t stdoutLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t full = PTHREAD_COND_INITIALIZER;

struct file *buffer;
int fileTypeCounters[3];
int buffSize;
int bufferIndex = 0;
int done = 0;
int endFileCounter = 0;
int readenFileCounter = 0;
int isCopiedSucces = 1;

//Copy infd file content to outfd file
int copyfile(int infd,int outfd);
/*Traverse directories recursively and open file descriptor 
both read and write end set them to buffer*/
void openFiles (char *sourcePath, char *destPath);
//Read fds from buffer then operate copy operation
void *consumer(void *args);
//Takes source and destination path and call openFiles function
void *producer(void *params);
void handle_sigint();


int copyfile(int infd,int outfd){

	char buf[4096];
	int size,bytes = 0;
	
	while((size = read(infd,buf,sizeof(buf))) > 0){
	  if(done){
	  	isCopiedSucces = 0;
	  	break;
	  }
      bytes += write(outfd,buf,size);
	}

	return bytes;
}

void openFiles (char *sourcePath, char *destPath){
	
	DIR *dir;
    struct stat statbuf; 
	struct dirent *ent;
	
	int fds[2],error;
	char *currentSourcePath = NULL;
	char *currentDestPath = NULL;

	if ((dir = opendir (sourcePath)) != NULL) {
	  	while ((ent = readdir (dir)) != NULL && !done) {
	  	 	
     	 	if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){

     	 		currentSourcePath = malloc(sizeof(sourcePath)+sizeof(ent->d_name)+sizeof(char));/* path/fileName */
				sprintf(currentSourcePath,"%s/%s", sourcePath, ent->d_name); /* Concatanate current file name and path */

				currentDestPath = malloc(sizeof(destPath)+sizeof(ent->d_name)+sizeof(char));/* path/fileName */
				sprintf(currentDestPath,"%s/%s", destPath, ent->d_name); /* Concatanate current file name and path */

     	 		lstat(currentSourcePath, &statbuf);

				if(S_ISDIR(statbuf.st_mode)){/* Checks current file is directory */
 	 				
 	 				++fileTypeCounters[0];
     	 			mkdir(currentDestPath, 0700);
 	 				openFiles(currentSourcePath,currentDestPath);
 					
				}	
				else if(S_ISREG(statbuf.st_mode) || S_ISFIFO(statbuf.st_mode)){/* Regular and FIFOS */
					
					if(S_ISFIFO(statbuf.st_mode)){
						
						fds[0] = open(currentSourcePath,READ_FLAGS | O_NONBLOCK);
						if(fds[0] != -1)
							++fileTypeCounters[2];
					}
					else{// Regular file 
						
						fds[0] = open(currentSourcePath,READ_FLAGS);
						if(fds[0] != -1)
							++fileTypeCounters[1];	
					}

					//Open file to read
					if(fds[0] == -1){
						// Critical section on stdout
						if ((error = pthread_mutex_lock(&stdoutLock)))
							fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));

						printf("Can not open %s for reading\n",currentSourcePath);

						if ((error = pthread_mutex_unlock(&stdoutLock)))
							fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));

						free(currentSourcePath);
    					free(currentDestPath);
						continue;
					}
					//Open file to write
					fds[1] = open(currentDestPath, WRITE_FLAGS,statbuf.st_mode);
					if(fds[1] == -1){
						// Critical section on stdout
						if ((error = pthread_mutex_lock(&stdoutLock)))
							fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));

						printf("Can not open %s for writing\n",currentDestPath);
						
						if ((error = pthread_mutex_unlock(&stdoutLock)))
							fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));

						free(currentSourcePath);
    					free(currentDestPath);
						continue;
					}

					// Prevents exceeding the per-process limit on the number of open file descriptors
					if(fds[0] >= 1000 || fds[1] >= 1000){
						printf("Number of open file descriptors is exceed\n");
						free(currentSourcePath);
    					free(currentDestPath);
    					done = 1;
						break;	
					}

					////////////////// Critical section /////////////////////////////////// 
					if ((error = pthread_mutex_lock(&bufferLock)))
						fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));

					while(bufferIndex == buffSize) // if buffer is full, wait producer thread
						pthread_cond_wait(&empty, &bufferLock);

					//Pass file to buffer	
					strcpy(buffer[bufferIndex].name,currentSourcePath);
					buffer[bufferIndex].fd[0] = fds[0];
					buffer[bufferIndex].fd[1] = fds[1];

					++bufferIndex;

					pthread_cond_broadcast(&full);

					if ((error = pthread_mutex_unlock(&bufferLock)))
						fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));
					////////////////// Critical section /////////////////////////////////// 
				}	
    			free(currentSourcePath);
    			free(currentDestPath);
			}		
     	}		
	}
	else {
	  fprintf(stderr,"Cannot access to %s",sourcePath);
	  perror (" ");	  
	}   
    closedir(dir);
} 

void *consumer(void *args){

	int error,fds[2];
	char name[255];
	int *bytes = malloc(sizeof(int));
	*bytes = 0;

	while(!done || bufferIndex != 0 ){

		////////////////// Critical section /////////////////////////////////// 
		if ((error = pthread_mutex_lock(&bufferLock)))
			fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));

		while(!done && bufferIndex == 0) // if buffer is empty, wait consumer threads
			pthread_cond_wait(&full, &bufferLock);

		--bufferIndex;
		if(bufferIndex < 0){
			bufferIndex = 0;
			if ((error = pthread_mutex_unlock(&bufferLock)))
				fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));
			continue;
		}
		// Read data from buffer
		strcpy(name,buffer[bufferIndex].name);
		fds[0] = buffer[bufferIndex].fd[0];
		fds[1] = buffer[bufferIndex].fd[1];

		++readenFileCounter;
		
		pthread_cond_broadcast(&empty);

		if ((error = pthread_mutex_unlock(&bufferLock)))
			fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));
		////////////////// Critical section /////////////////////////////////// 

		*bytes += copyfile(fds[0],fds[1]);
		close(fds[0]);
		close(fds[1]);

		++endFileCounter;

		//Critical section on stdout
		if ((error = pthread_mutex_lock(&stdoutLock)))
			fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));

		if(isCopiedSucces)
			printf("%s is copied succesfully\n",name);
		else
			printf("%s is copied but not completely, because of interupt\n",name);
								
		if ((error = pthread_mutex_unlock(&stdoutLock)))
			fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));
		
	}
	return bytes;
}

void *producer(void *params){

	char **args = (char **) params;
	char dirName[1024];
	char editedDest[1024];

	//First create a root directory to copy directory
	strcpy(dirName,basename(args[3]));
	sprintf(editedDest,"%s/%s", args[4], dirName);
	mkdir(editedDest,0700);

	openFiles(args[3],editedDest);

	return NULL;
}

void handle_sigint(){
	printf("SIGINT is handled\n");
	done = 1;
}

int main (int argc, char *argv[]) {

	pthread_t producer_thread, *consumer_threads;
	int numOfConsumers,totalBytes = 0;
	void *bytes;
	DIR *dir1,*dir2;
	struct timeval tv1,tv2; 

	if (argc != 5 || ((numOfConsumers = atoi (argv[1]))) <= 0 || ((buffSize = atoi (argv[2]))) <= 0) {
		fprintf(stderr, "Usage: %s [numOfConsumers] [buffSize] [sourcePath] [destPath]\n", argv[0]);
		return 1;
	}
	else if((dir1 = opendir (argv[3])) == NULL){
		fprintf(stderr, "Can not open %s\n",argv[3]);
		return 1;
	}
	else if((dir2 = opendir (argv[4])) == NULL){
		fprintf(stderr, "Can not open %s\n",argv[4] );
		return 1;
	}
	closedir(dir1);
	closedir(dir2);
	
	signal(SIGINT,handle_sigint);

	for(int i=0; i<3; ++i)
		fileTypeCounters[i] = 0;

	buffer = malloc(buffSize * sizeof(struct file));
	consumer_threads = malloc(numOfConsumers * sizeof(pthread_t));

    gettimeofday(&tv1, NULL); 

	if(pthread_create(&producer_thread, NULL, producer, argv)){
		fprintf(stderr, "Error on thread creation, program is terminated.\n");
		exit(1);
	}
	for(int i = 0; i<numOfConsumers; ++i){
		if(pthread_create(&consumer_threads[i], NULL, consumer, NULL)){
			fprintf(stderr, "Error on thread creation, program is terminated.\n");
			exit(1);
		}
	}

	if(pthread_join(producer_thread, NULL)){
		fprintf(stderr, "Error on join thread, program is terminated.\n");
		exit(1);
	}
	// Set done flag to 1 after producer is end and all file is copied
	while(endFileCounter != readenFileCounter);
	done = 1;
	pthread_cond_broadcast(&full);

	for(int i = 0; i<numOfConsumers; ++i){
		if(pthread_join(consumer_threads[i], &bytes)){
			fprintf(stderr, "Error on join thread, program is terminated.\n");
			exit(1);
		}
		totalBytes += *(int*)bytes;
		free(bytes);
	}
	
	gettimeofday(&tv2, NULL);
	
	printf("\nNumber of bytes copied: %d\n", totalBytes);
	printf("Number of directories copied: %d\n", fileTypeCounters[0]);
	printf("Number of regular files copied: %d\n", fileTypeCounters[1]);
	printf("Number of FIFO's copied: %d\n", fileTypeCounters[2]);
	printf("Total time: %.0lf microseconds\n",((double) (1000000 * (tv2.tv_sec - tv1.tv_sec)) + (double) (tv2.tv_usec - tv1.tv_usec)));

	free(buffer);
	free(consumer_threads);
	return 0;
}