#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define _WRONG_INIT 'D'
#define STDIN 1  // file descriptor for standard input

int main()
{
    printf("%d",strlen(_WRONG_INIT));
    return 0;
} 