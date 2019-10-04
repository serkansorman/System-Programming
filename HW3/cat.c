#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include<sys/wait.h> 
#include<unistd.h>
#include <sys/file.h>
#include <sys/stat.h>


int main(int argc, char **argv)
{
    char filename[1024];
	char line[1024];
    struct stat fileStat;


    if(argc == 1){ 
        fgets(filename,sizeof(filename),stdin);
        if(filename[strlen(filename) - 1] == '\n')
            filename[strlen(filename) - 1] = '\0';
    }
    else
    	strcpy(filename,argv[1]);

    if(lstat(filename,&fileStat) < 0){
        printf("%s: Cannot open file \n",filename); 
        exit(1); 
    }


    if(S_ISDIR(fileStat.st_mode)){
        printf("%s: Is a directory\n",filename);
        exit(0);
    }

    FILE *fp = fopen(filename,"r");
	if (fp == NULL) { 
        printf("%s: Cannot open file \n",filename);  
        exit(1); 
    } 


	while(fgets(line, 1024, fp))
		printf("%s",line );
	printf("\n");

    return 0;
}