#include <unistd.h>
#include <string.h>
#include <stdio.h>


int main(int argc, char **argv)
{
    
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);

    return 0;
}