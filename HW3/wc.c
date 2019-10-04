#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include<sys/wait.h> 
#include<unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#define MAX_SIZE 20000


int main(int argc, char **argv)
{
    char filename[1024];
	char line[1024];
    char text[MAX_SIZE];
    int numOfLine = 0;
    struct stat fileStat;
    int i = 0;
  

    if(argc == 1){ 
        read(STDIN_FILENO,text,sizeof(text));
        while(text[i] != '\0'){
            if(text[i] == '\n')
                ++numOfLine;
            ++i;
        } 
    }
    else{

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
            ++numOfLine;
    }


	

    printf("%d\n",numOfLine);
		

    return 0;
}