#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>

#define SERVER_FIFO "/tmp/bank_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/client_fifo.%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 8)
#define BANK_CLOSED 404

void errExit(char* message,char* target){
    fprintf(stderr, "%s %s: ",message,target);
    perror("");
    exit(EXIT_FAILURE);
}

void handle_signal(int signo){

    if(signo == SIGINT){
        printf("Client is terminated by SIGINT\n");
        kill(0, SIGKILL); //Kill client processes
    }
    else if(signo == SIGTERM){
        printf("Client is terminated by SIGTERM\n");
        kill(0, SIGKILL); //Kill client processes
    }
}


int main(int argc, char **argv){ 

  char clientFifo[CLIENT_FIFO_NAME_LEN];
  int serverFd, clientFd,money = 0;
  pid_t clientPid;
  int numOfClients;

  if (argc != 2 || ((numOfClients = atoi (argv[1]))) <= 0) {
       fprintf(stderr, "Usage: %s [numOfClients]\n", argv[0]); 
       exit(1);
  }

  //Handle signals
  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);


  serverFd = open(SERVER_FIFO, O_RDWR);
  if(serverFd == -1)
    errExit("open",SERVER_FIFO);
  

  for(int i=0; i<numOfClients; ++i){
      pid_t pid = fork();
      if(pid == 0){
        // Each client open respond fifo to take money from bank
        clientPid = getpid();
        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) clientPid);
        if(mkfifo(clientFifo, 0666) == -1)
          errExit("mkfifo",clientFifo);
        
        //Write clientPid to server fifo as request
        if(write(serverFd, &clientPid, sizeof(pid_t)) != sizeof(pid_t))
          fprintf(stderr, "Error writing request to %s\n",SERVER_FIFO );
        close(serverFd);

        // Read respond from bank
        clientFd = open(clientFifo, O_RDONLY);
        if(clientFd == -1)
          errExit("open",clientFifo);
        read(clientFd, &money, sizeof(int));
          
        if(money == BANK_CLOSED){
          printf("Musteri %d parasını alamadi :(\n",clientPid);
          unlink(clientFifo);
        }
        else{
          printf("Musteri %d %d lira aldi :)\n",clientPid,money);
          unlink(clientFifo);     
        }
        close(clientFd);
        _exit(0);
      }
      else if(pid > 0){
        /* Pass the parent to wait all clients finished */   
      }
      else{
          perror("fork");
          exit(1);
      }
  }

  /*Wait all clients */
  while(wait(NULL) != -1);
    
  return 0; 
} 
