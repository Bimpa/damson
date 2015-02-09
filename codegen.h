#include "compiler.h"

extern void CodeGenerate(char Filename[],
                         char nodename[], 
                         Instruction *code,    unsigned int codesize, 
                         int *gv,              unsigned int gvsize,
                         int *ev,              unsigned int evsize,
                         ProcedureItem *procs, unsigned int nprocs,
                         unsigned int *labs,   unsigned int nlabs);
