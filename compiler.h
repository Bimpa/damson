/* DAMSON compiler header (also used by emulator and code generator)
*/

#ifndef COMPILER
#define COMPILER

#include <stdbool.h>

#define VERSION_MAJOR   5
#define VERSION_MINOR   5

#define bpw             4  /* bytes per 32-bit word */

#define s_LG            1
#define s_LP            2
#define s_LN            3
#define s_LSTR          4
#define s_LL            5
#define s_LLG           6
#define s_LLP           7
#define s_LLL           8
#define s_EQ            9
#define s_NE           10
#define s_LS           11
#define s_GR           12
#define s_LE           13
#define s_GE           14
#define s_JT           15
#define s_JF           16
#define s_JUMP         17
#define s_OR           18
#define s_AND          19
#define s_PLUS         20
#define s_MINUS        21
#define s_MULT         22
#define s_MULTF        23
#define s_DIV          24
#define s_DIVF         25
#define s_REM          26
#define s_NEG          27
#define s_NOT          28
#define s_ABS          29
#define s_SG           30
#define s_SP           31
#define s_SL           32
#define s_SYSCALL      33
#define s_LOGAND       34
#define s_LOGOR        35
#define s_NEQV         36
#define s_LSHIFT       37
#define s_RSHIFT       38
#define s_COMP         39
#define s_SWITCHON     40
#define s_FNAP         41
#define s_RTAP         42
#define s_RTRN         43
#define s_ENTRY        44
#define s_RV           45
#define s_STIND        46
#define s_PUSHTOS      47
#define s_VCOPY        48
#define s_FLOAT        49
#define s_INT          50
#define s_SWAP         51
#define s_DEBUG        52
#define s_LBOUNDSCHECK 53
#define s_GBOUNDSCHECK 54

#define s_STACK        60  /* pseudo instructions */
#define s_QUERY        61
#define s_STORE        62
#define s_SAVE         63
#define s_RES          64
#define s_RSTACK       65
#define s_LAB          66
#define s_DISCARD      67
#define s_NONE         68
#define s_EOF          69

#define MaxStringSize  200
#define MaxPts         5000
#define MaxDimensions  10
#define MaxLocals      100
#define MaxNodes       1500
#define MaxLabels      1000
#define StackSize      10000
#define MaxLinks       10001
#define MaxLines       150000

#define GLOBALBASE     50  /* moved from compiler.c */

enum VarType       { VoidType, IntType, FloatType };
enum TimeStampMode { Off, On };

typedef struct
{
    unsigned char Op;
    int           Arg;
} Instruction;

typedef struct
{
    char          Name[MaxStringSize + 1];
    enum VarType  vType;
    unsigned int  vOffset;
    unsigned int  vDimensions[MaxDimensions+1];
} NametableItem;

typedef struct
{
    char          Name[MaxStringSize + 1];  /* name of the procedure */
    unsigned int  ProcType;                 /* type of the procedure */
    unsigned int  nArgs;                    /* number of parameters of the procedure */
    unsigned int  Label;                    /* start address of the procedure */
    unsigned int  BP;                       /* base pointer */
    unsigned      nLocals;                  /* number of args + local variables */
    NametableItem Args [MaxLocals];         /* list args + local variables */
    unsigned int  Versions;                 /* 0=prototype, 1=implementation, >1=error */
    unsigned int  Offset;                   /* Arm offset - need for ELF builder */
    unsigned int  CallCount;                /* profiler calls */
    unsigned int  TickCount;                /* profiler ticks */
} ProcedureItem;

typedef struct
{
    unsigned int  iNumber;
    unsigned int  iVector;
} InterruptVector;

struct tnode
{
    unsigned int node;
    unsigned int destinations[MaxLinks + 1];
    unsigned int ndest;
    struct tnode *left;
    struct tnode *right;
};

struct LineInfo
{
    unsigned int    codeoffset;
    struct NodeInfo *pnode;
};

extern enum TimeStampMode TimeStamping;
extern bool               Monitoring;
extern unsigned int       NumberOfNodes;
extern struct tnode       *Links;
extern unsigned int       Errors;
extern unsigned int       NumberOfLines;
extern struct LineInfo    *LineNumberList;
extern char               FileBaseName[MaxStringSize];
extern bool               LogFiles;

extern bool Compile(char Filename[], bool CodeGenerating, bool DisAssembling, bool Debugging, bool BoundsChecking);
extern void Shutdown();
extern void Error(unsigned int code, char *fmt, ...);

#endif
