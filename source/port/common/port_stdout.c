#include <stdio.h>

int __io_putchar(int ch)
{
    fprintf(stdout,"%c",ch);
    fflush(stdout);
    
    return 1;
}
