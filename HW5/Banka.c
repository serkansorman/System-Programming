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
#include <sys/time.h>

#define SERVER_FIFO "/tmp/bank_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/client_fifo.%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 8)
#define NUM_OF_SERVICES 4
#define F_SETPIPE_SZ 1031

int logfd[2],clientCountfd[2],servicesfd[2],mainfd[2],failClientfd[2];
int serviceTime;
long startMS;
pid_t mainPid;

// Waits client in specific time as milisecond
void delay(unsigned long ms);
// Gets current time as milisecond
long getCurrentTime();
// Gets service name according to service number
char* getServiceName(int serviceNo);
// Gets number of clients to be serviced for each service
int* getClientCounters(int numOfClients,int clientCounter[4]);
// Gets data from pipes and writes to Banka.log
void writeLog();
//Write log,send request to rest of clients and kill all bank process
void exit_Bank();
//Starts services to take request and send respond
void startService(int serviceNo);
void errExit(char* message,char* target);
//Handle SIGINT and SIGTERM then exit properly by main process
void handle_signal(int signo);


void delay(unsigned long ms){
    unsigned long end = getCurrentTime() + ms;
    while(end > getCurrentTime());
}

long getCurrentTime() {
    struct timeval te; 
    gettimeofday(&te, NULL); 
    return te.tv_sec*1000 + te.tv_usec/1000;
}

void errExit(char* message,char* target){
    fprintf(stderr, "%s %s: ",message,target);
    perror("");
    exit(EXIT_FAILURE);
}

char* getServiceName(int serviceNo){
    switch(serviceNo){
        case 1:
        return "process1";
        case 2:
        return "process2";
        case 3:
        return "process3";
        default:
        return "process4";
    }
}

int* getClientCounters(int numOfClients,int clientCounter[4]){
    
    int serviceNum;
    for(int i=0; i<4; ++i)
        clientCounter[i] = 0;
    
    //Set each service to number of clients 
    close(servicesfd[1]);
    for(int i=0; i<numOfClients; ++i){
        read(servicesfd[0],&serviceNum,sizeof(int));
        ++clientCounter[serviceNum-1];
    }
    close(servicesfd[0]);

    return clientCounter;
}

void writeLog(){

    int numOfClients = 0;
    char buf[512];
    char buf2[4][20];
    int clientCounter[4];

    /*Clean the log file*/
    FILE *fp = fopen("Banka.log","w");
    fclose(fp);

    fp = fopen("Banka.log","a");
    if(fp == NULL){
        errExit("open","Banka.log");
    }
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    fprintf(fp,"%02d/%02d/%d tarihinde islem basladi. Bankamiz %d saniye hizmet verecek.\n\n",
    local->tm_mday, local->tm_mon + 1, local->tm_year + 1900,serviceTime);
    fprintf(fp,"clientPid\tprocessNo\tPara\tislem bitis zamani\n");
    fprintf(fp,"---------\t---------\t----\t------------------\n");

    /*Get number of client */
    close(clientCountfd[1]);
    read(clientCountfd[0],&numOfClients,sizeof(int));
    close(clientCountfd[0]);
   
    close(logfd[1]);
    FILE *fp2 = fdopen(logfd[0],"r");

    if(fp2 == NULL)
        errExit("open","pipe");
    
    for (int i=0; i<numOfClients; ++i) { 
        fgets(buf,512,fp2);
        if(numOfClients < 4){
            sscanf(buf,"%s %s %s %s",buf2[0],buf2[1],buf2[2],buf2[3]);
            sprintf(buf,"%s%16s%7s%10s msec\n",buf2[0],getServiceName(i+1),buf2[2],buf2[3]);
        }
        fprintf(fp,"%s",buf);
    }
    close(logfd[0]);
    
    fprintf(fp, "\n%d saniye dolmustur %d müsteriye hizmet verdik.\n",serviceTime, numOfClients);
    getClientCounters(numOfClients,clientCounter);

    int clientsPerService;
    for(int i=0; i<4; ++i){
        if(numOfClients < 4){
            if(i<numOfClients)
                clientsPerService = 1;
            else
                clientsPerService = 0;
        }
        else
            clientsPerService = clientCounter[i];
        fprintf(fp, "%s %d musteriye hizmet sundu.\n",getServiceName(i+1),clientsPerService);
    }
    
    fclose(fp2);
    fclose(fp);
}

void exit_Bank() {
    pid_t clientPid;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    int clientFd,bank_closed = 404;
    int serverFd = open(SERVER_FIFO,O_RDONLY | O_NONBLOCK);
    int failClientCount = 0;

    if(serverFd == -1)
        errExit("open",SERVER_FIFO);
    
    // Send bank_closed respond to rest of clients
    while(read (serverFd, &clientPid, sizeof(pid_t)) > 0){

        snprintf(clientFifo,CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) clientPid);
        clientFd = open(clientFifo, O_WRONLY);
        if(clientFd == -1)
            errExit("open",clientFifo);
        
        write(clientFd, &bank_closed,sizeof(int));
        close(clientFd);
    }
    close(serverFd);
    unlink(SERVER_FIFO);


    close(failClientfd[1]);
    read(failClientfd[0],&failClientCount,sizeof(int));
    close(failClientfd[0]);

    // Send bank_closed respond to rest of clients
    close(mainfd[1]);
    while(failClientCount > 0){

        read(mainfd[0],&clientPid,sizeof(pid_t));

        snprintf(clientFifo,CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) clientPid);
        clientFd = open(clientFifo, O_WRONLY);
        if(clientFd == -1)
            errExit("open1",clientFifo);
        
        write(clientFd, &bank_closed,sizeof(int));
        close(clientFd);
        --failClientCount;
    }
    close(mainfd[0]);
    
    writeLog();
    kill(0, SIGKILL); //Kill all bank processes
}


void startService(int serviceNo){

    int money,numOfClients,failClientCount;
    int serverFd, clientFd;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    pid_t clientPid,tempPid;
    char *buf;

    serverFd = open(SERVER_FIFO,O_RDONLY | O_NONBLOCK);
    if(serverFd == -1)
        exit(EXIT_FAILURE); 
    fcntl(serverFd, F_SETPIPE_SZ, 1024 * 1024); /* Set max fifo size */


    while(1){

        // Gets request from client  
        if(read (serverFd, &clientPid, sizeof(pid_t)) != sizeof(pid_t))
            continue; // Keep read request

        write(mainfd[1], &clientPid,sizeof(pid_t));
        //Update number of unsuccesfull client
        read(failClientfd[0],&failClientCount,sizeof(int));
        ++failClientCount;
        write(failClientfd[1], &failClientCount,sizeof(int));
        

        delay(1500); // waits each client 1500 ms
        
        // Open unique respond fifo to send money to client
        snprintf(clientFifo,CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) clientPid);
        clientFd = open(clientFifo, O_WRONLY);
        if(clientFd == -1)
            errExit("open",clientFifo);
        
        //Get random money for client
        srand(clock() * clientPid);   
        money = (rand() % 100) + 1; 

        //Update number of client
        read(clientCountfd[0],&numOfClients,sizeof(int));
        ++numOfClients;
        write(clientCountfd[1], &numOfClients,sizeof(int));

        //Update client counter list for each service
        write(servicesfd[1], &serviceNo,sizeof(int));

        //pass log info to main bank process via pipe
        buf = malloc(512);
        sprintf(buf,"%d%16s%7d%10ld msec\n",clientPid,getServiceName(serviceNo),money,getCurrentTime() - startMS);
        write(logfd[1],buf,strlen(buf));
        free(buf);

        //Send money to current client as a respond
        if(write(clientFd, &money,sizeof(int)) != sizeof(int))
            fprintf(stderr, "Error writing respond to %s\n",clientFifo);      
        close(clientFd);

        //Update number of unsuccesfull client
        read(failClientfd[0],&failClientCount,sizeof(int));
        --failClientCount;
        write(failClientfd[1], &failClientCount,sizeof(int));

        read(mainfd[0],&tempPid,sizeof(pid_t));

    }
}

void handle_signal(int signo){
    // if signal is catched by main process, write log and exit properly
    if(mainPid == getpid()){
        if(signo == SIGINT){
            printf("Bank is terminated by SIGINT\n");
            exit_Bank();
        }
        else if(signo == SIGTERM){
            printf("Bank is terminated by SIGTERM\n");
            exit_Bank();
        }
    }
}

int main(int argc, char **argv){

    int numOfClients = 0;

    if (argc != 2 || ((serviceTime = atoi (argv[1]))) <= 0) {
       fprintf(stderr, "Usage: %s [serviceTime]\n", argv[0]); 
       exit(1);
    }

    mainPid = getpid(); // Get main pid
    startMS = getCurrentTime(); // Get start time of bank
    
    signal(SIGALRM, exit_Bank);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    alarm(serviceTime);         //Bank is run until reachs the service time

    if(mkfifo(SERVER_FIFO, 0666) == -1)
        errExit("mkfifo",SERVER_FIFO);
    
    // Open pipe to pass log info to main bank process
    if(pipe(logfd) == -1)
        errExit("pipe","logfd");
    // Open pipe to calculate total number of clients that serviced
    if(pipe(clientCountfd) == -1)
        errExit("pipe","clientCountfd");
    // Open pipe to calculate number of clients for each services
    if(pipe(servicesfd) == -1)
        errExit("pipe","servicesfd");
    // Open pipe to pass unsuccesfull client pid to main process
    if(pipe(mainfd) == -1)
        errExit("pipe","mainfd");
    // Open pipe to calculate number of unseccesfull clients
    if(pipe(failClientfd) == -1)
        errExit("pipe","failClientfd");
      
    //Set client counter to 0 at the beginning
    write(clientCountfd[1],&numOfClients,sizeof(int));

    write(failClientfd[1],&numOfClients,sizeof(int));

    for(int i=1; i<=NUM_OF_SERVICES; ++i){
        pid_t pid = fork();
        if(pid == 0){
            startService(i);       
        }
        else if(pid > 0){
            /* Pass the parent to wait all services*/
        }
        else{
            errExit("fork","");
        }
    }
    
    /*Wait all services */
    while(wait(NULL) != -1);

    return 0; 
} 
