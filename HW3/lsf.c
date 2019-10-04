#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>


void printFileInfo(char *fileName){

    struct stat fileStat;
    if(lstat(fileName,&fileStat) < 0 || S_ISDIR(fileStat.st_mode))    
        return ;

    printf( (S_ISREG(fileStat.st_mode)) ? "R" : "S");
    printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
    printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
    printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
    printf(" ");

    printf("%5ld ",(long)fileStat.st_size);
    printf("%s\n",fileName);

}


void traverseDirectory (){
    
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (".")) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0){
                printFileInfo(ent->d_name);
            }       
        }      
    }
    else{
        fprintf(stderr,"Cannot open directory");
        perror (" ");  
    }      
    /*Close directory*/
    while ((closedir(dir) == -1) && (errno == EINTR));
} 

int main(int argc, char **argv)
{
    
    traverseDirectory();
    return 0;
}