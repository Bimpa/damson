/*******************************************************************
DAMSON

Contributors:
    Dave Allerton - compiler, emulator and code generator
    Paul Richmond - debugger, emulator
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "emulator.h"
#include "codegen.h"

void help();

/* --------------------------------------------------------- */
int main(int argc, char *argv[])
{
    bool         DisAssembling;
    bool         Emulating;
    bool 	     CodeGenerating;
    bool         Debugging;
    bool         BoundsChecking;
    bool         ArithmeticChecking;

    unsigned int i;
    time_t       timer;
    
    printf("DAMSON Version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
    printf("(C) Copyright University of Sheffield 2015\n");
    printf("(C) Authors Dave Allerton and Paul Richmond\n");

    timer = time(NULL);
    printf("%s\n", asctime(localtime(&timer)));

    if (argc <= 1)
    {
        printf("No file?\n");
        return EXIT_FAILURE;
    }

    if ((strcmp(argv[1], "--help") == 0))
    {
        help();
        return EXIT_FAILURE;
    }
    
    DisAssembling = false;
    Emulating = true;
    CodeGenerating = false;
    Debugging = false;
    BoundsChecking = false;
    ArithmeticChecking = false;
    ProfileNode = 0;

    for (i=2; i<argc; i+=1)
    {
        if (strcmp(argv[i], "-dis") == 0)
        {
            DisAssembling = true;
            Emulating = false;
        }
        else if (strcmp(argv[i], "-c") == 0)
        {
            CodeGenerating = true;
            Emulating = false;
    }
        else if (strcmp(argv[i], "-debug") == 0)
        {
            Debugging = true;
        }
        else if (strcmp(argv[i], "-bc") == 0)
        {
            BoundsChecking = true;
        }
        else if (strcmp(argv[i], "-ar") == 0)
        {
             ArithmeticChecking = true;
        }
        else if (strcmp(argv[i], "-pr") == 0)
        {
            ProfileNode = atoi(argv[i+1]);
            i += 1;
        }
        else
        {
            help();
            return EXIT_FAILURE;
        }
    }

    if (Compile(argv[1], CodeGenerating, DisAssembling, Debugging, BoundsChecking))
    {
        if (Emulating && Errors == 0)
        {
            Emulate(Debugging, ArithmeticChecking);
        }
    } 
    else 
    {
        printf("Cannot find %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    Shutdown();
    return EXIT_SUCCESS;
}

/* --------------------------------------------------------- */
void help()
{
    printf("options:\n-dis      dis-assemble\n"
                     "-c        code generate\n"
                     "-debug    debugging\n"
		     "-bc       bounds checking\n"
                     "-ar       arithmetic checking\n"
                     "-pr       profile checking\n"
                     "--help    this message\n");
}

