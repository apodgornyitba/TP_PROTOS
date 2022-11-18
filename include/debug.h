#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#define STDOUT_DEBUG 1
#define FILE_DEBUG 2

#ifndef DEBUGGING_FILE
#define DEBUGGING_FILE
    FILE * debugging_file;
#endif

void debug_init(int setting);
void debug_file_close();
void debug(char * etiqueta, int codigo,char * mensaje, int extra);

#endif