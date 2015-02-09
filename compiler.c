/* DAMSON compiler
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "compiler.h"
#include "emulator.h"
#include "codegen.h"
#include "debug.h"

#define MaxKeyWords   16
#define MaxDirectives 9
#define MaxProcs      13
#define MaxFunctions  9
#define MaxSwitches   100
#define MaxInstBuff   100
#define MaxBreaks     100
#define MaxContinues  100

#define CodeSize      10000
#define MaxDefines    1000
#define MaxProcedures 100
#define MaxPrototypes 100
#define MaxRangeSize  100
#define MaxGlobals    1000
#define MaxExternals  1000
#define MaxGvSize     10000
#define MaxEvSize     800000
#define MaxStrings    500

#define SAVESPACESIZE 3

#define CR            13
#define EOL           10
#define SPACE         ' '
#define COMMA         ','
#define TAB           9

char *KeyWords[MaxKeyWords] =
{ "",   "if", "else", "while",   "do",  "for",   "switch", "break",
  "continue", "case", "default", "int", "float", "void",   "return", "extern" };

char *Directives[MaxDirectives] =
{ "", "timestamp", "node", "monitor", "define", "include", "alias", "log", "snapshot" };

char *Procs[MaxProcs] =
{ "", "sendpkt", "delay", "printf", "exit", "signal", "wait", "tickrate",
      "putbyte", "putword", "readsdram", "writesdram", "syncnodes"};

char *Functions[MaxFunctions] =
{ "", "getclk", "abs", "fabs", "createthread", "deletethread", "getbyte", "getword", "bitset"};

#define c_ERROR            0
#define c_IF               1
#define c_ELSE             2
#define c_WHILE            3
#define c_DO               4
#define c_FOR              5
#define c_SWITCH           6
#define c_BREAK            7
#define c_CONTINUE         8
#define c_CASE             9
#define c_DEFAULT          10
#define c_INT              11
#define c_FLOAT            12
#define c_VOID             13
#define c_RETURN           14
#define c_EXTERN           15

#define c_TIMESTAMP        20
#define c_NODE             21
#define c_MONITOR          22
#define c_DEFINE           23
#define c_INCLUDE          24
#define c_ALIAS            25
#define c_LOG              26
#define c_SNAPSHOT         27

#define c_LBRACKET         30
#define c_RBRACKET         31
#define c_LCURLY           32
#define c_RCURLY           33
#define c_LSQUARE          34
#define c_RSQUARE          35

#define c_EQUAL            40
#define c_NOT_EQUAL        41
#define c_LESS             42
#define c_LESS_OR_EQUAL    43
#define c_GREATER          44
#define c_GREATER_OR_EQUAL 45

#define c_PLUS             50
#define c_MINUS            51
#define c_MULT             52
#define c_DIV              53
#define c_MOD              54
#define c_NEG              55
#define c_NOT              56

#define c_LOGIC_AND        60
#define c_LOGIC_OR         61
#define c_LOGIC_XOR        62
#define c_LSHIFT           63
#define c_RSHIFT           64
#define c_ONESCOMP         65

#define c_PLUS_ASSIGN      70
#define c_MINUS_ASSIGN     71
#define c_MULT_ASSIGN      72
#define c_DIV_ASSIGN       73
#define c_MOD_ASSIGN       74
#define c_LOGIC_AND_ASSIGN 75
#define c_LOGIC_OR_ASSIGN  76
#define c_LOGIC_XOR_ASSIGN 77
#define c_LSHIFT_ASSIGN    78
#define c_RSHIFT_ASSIGN    79

#define c_COMMA            80
#define c_ASSIGN           81
#define c_AND              82
#define c_OR               83

#define c_NUMBER           100
#define c_VAR              101
#define c_STRING           102
#define c_SEMICOLON        103
#define c_COLON            104
#define c_QUERY            105

#define c_NONE             9998
#define c_EOF              9999

typedef struct
{
    unsigned int CaseValue;
    unsigned int CaseAddress;
} SwitchItem;

typedef struct
{
    unsigned int Size;
    unsigned int Stack[StackSize];
} OpStackStruct;

typedef struct
{
    unsigned int Size;
    enum VarType Stack[StackSize];
} ArgStackStruct;

typedef struct
{
    unsigned int Size;
    int Stack[StackSize];
} ConstStackStruct;

typedef struct
{
    char         DefName[MaxStringSize + 1];
    int          DefValue;
    enum VarType DefType;
} DefinetableItem;

typedef struct
{
    char            *nodename;
    unsigned int    startaddress;
    Instruction     *code;
    unsigned int    codesize;
    int             *gv; 
    unsigned int    gvsize;
    NametableItem   *globals;
    unsigned int    nglobals;
    int             *ev;
    unsigned int    evsize;
    NametableItem   *externals;
    unsigned int    nexternals;
    ProcedureItem   *procs;
    unsigned int    nprocs;
    unsigned int    *labs;
    unsigned int    nlabs;
    InterruptVector *intv;
    unsigned int    intvsize;
} Prototype;

typedef struct
{
    unsigned int low;
    unsigned int high;
} RangeType;

typedef struct
{
    unsigned int nItems;
    char         Name[MaxStringSize + 1];
    RangeType    Range[MaxRangeSize + 1];
} RangeListType;

/* GLOBALS */
unsigned int           Labels[MaxLabels + 1 + 50];  /* allow 50 for system calls */
unsigned int           NumberOfLabels;
unsigned int           LineNumber;
bool                   SymbolBackSpacing;
unsigned int           LastSymbol;
char                   LastSymbolString[MaxStringSize + 1];
unsigned int           Errors;
FILE                   *FileStream = NULL;
DefinetableItem        Defines[MaxDefines + 1];
unsigned int           NumberOfDefines;
ProcedureItem          Procedures[MaxProcedures + 1];
unsigned int           NumberOfProcedures;
FILE                   *LinkerFileStream;
FILE                   *MakefileStream;
unsigned int           ssp;
unsigned int           CurrentProcedure;
unsigned int           PC;
unsigned int           NumberOfInterrupts;
InterruptVector        IntVector[MaxNodes + 1];
NametableItem          Globals[MaxGlobals + 1];
unsigned int           NumberOfGlobals;
NametableItem          Externals[MaxExternals + 1];
unsigned int           NumberOfExternals;
unsigned int           NumberOfLogs;
unsigned int           FirstLog;
Instruction            Instructions[CodeSize + 1];
unsigned int           ProgramSize;
unsigned int           Node;
unsigned int           BreakList[MaxBreaks];
unsigned int           BreakLevel;
unsigned int           ContinueList[MaxContinues];
unsigned int           ContinueListFlag[MaxContinues];
unsigned int           ContinueLevel;
unsigned int           gvsize;
int                    GlobalVector[MaxGvSize + 1];
unsigned int           evsize;
int                    ExternalVector[MaxEvSize + 1];
bool                   DisAssembling;
bool                   CodeGenerating;
bool                   BoundsChecking;
char                   LastString[MaxStringSize];
enum TimeStampMode     TimeStamping;
bool                   Monitoring;
unsigned int           NumberOfNodes;
struct tnode           *Links;
bool                   externmode;
char                   *CurrentPrototype;
char                   *Prototypes[MaxPrototypes];
unsigned int           NumberOfPrototypes;
bool                   AliasMode;
unsigned int           Including;
bool                   GlobalDeclarations;
struct LineInfo        LineNumbers[MaxLines+1];
struct LineInfo        *LineNumberList;
unsigned int           NumberOfLines;
unsigned int           FirstLine;
unsigned long long int samplingt1;
unsigned long long int samplingt2;
unsigned long long int samplingt3;
RangeListType          RangeList;
unsigned int           nMakefiles;
char                   MakefileNames[MaxPrototypes + 1] [MaxStringSize];
bool                   LogFiles;

/* PROTOTYPES */
unsigned int NextLabel();
void         SetLabel(unsigned int lab, unsigned int addr);
unsigned int GetLabel(unsigned int lab);
int          Str2Int(char str[]);
void         CopyString(char a[], char b[]);
void         FileBackSpace(char Ch);
char         Rdch();
void         ReadName(char Str[], char Ch);
bool         SameString(char a[], char b[]);
void         ReadCharacter(char str[], char Ch);
void         ReadNumber(char Str[], char Ch);
void         ReadString(char Str[], char tCh);
unsigned int ReadSymbol(char Str[]);
unsigned int NextSymbol(char Str[]);
bool         Integer(char Str[]);
void         AddProcedure(char v[], enum VarType ptype);
unsigned int FindLocal(unsigned int proc, char v[]);
unsigned int FindLocalProcedure(char v[]);
unsigned int FindLocalProcedureNumber(unsigned int l);
unsigned int AddString(char str[]);
void         AddGlobal(char v[], enum VarType t, bool scalar);
unsigned int AllocateConstantGlobalArray(unsigned int dimensions[], enum VarType t, unsigned int d);
unsigned int AllocateGlobalArray(unsigned int a[]);
unsigned int AllocateLocalArray(unsigned int procno, unsigned int a[]);
void         AddLocal(char v[], enum VarType t, bool scalar);
void         AddDefine(char str[], int x, enum VarType t);
unsigned int LookupDefine(char v[]);
unsigned int FindGlobal(char v[]);
unsigned int FindProcedure(char v[]);
unsigned int FindFunction(char v[]);
void         GenCode2(unsigned char OpCode, int Arg);
void         GenCode1(unsigned char OpCode);
void         stackinc(unsigned char OpCode, int Arg);
void         SymbolBackSpace();
void         OpStackInit(OpStackStruct *s);
bool         OpStackEmpty(OpStackStruct *s);
void         OpStackPush(unsigned int Op, OpStackStruct *s);
unsigned int OpStackPop(OpStackStruct *s);
unsigned int OpStackTop(OpStackStruct *s);
void         ArgStackInit(ArgStackStruct *s);
void         ArgStackPush(enum VarType Arg, ArgStackStruct *s);
enum VarType ArgStackPop(ArgStackStruct *s);
void         ConstStackInit(ConstStackStruct *s);
void         ConstStackPush(int x, ConstStackStruct *s);
int          ConstStackPop(ConstStackStruct *s);
void         CheckTypes(enum VarType dest, enum VarType source);
void         GenOpCode(unsigned int Op, ArgStackStruct *s);
unsigned int Precedence(unsigned int Op);
void         ReadAddress();
enum VarType ReadExpression();
void         EvaluateConstant(unsigned int Op, ConstStackStruct *s);
int          ReadConstantExpression(enum VarType *t);
void         ReadBooleanExpression();
void         ReadBlock(unsigned int Op);
unsigned int ReadStatement();
struct tnode *AddLink(struct tnode *p, unsigned int s, unsigned int d);
void         FreeLinks(struct tnode *p);
bool         ReadFile(char FileName[], bool include);
char*        FindLocalName(unsigned int pc, unsigned int p);
char*        FindGlobalName(unsigned int p);
void         DisAssemble();
void         ResetNode();
void         SetFileBaseName(char Filename[], char FileBaseName[]);
void         GetFileName(char infile[], char outfile[], char ext[]);
void         AddExternal(char v[], enum VarType t, bool scalar);
unsigned int FindExternal(char v[]);
unsigned int AllocateConstantExternalArray(unsigned int dimensions[], enum VarType t, unsigned int d);
unsigned int AllocateExternalArray(unsigned int a[]);
void         ReadRange(RangeListType *rlist);
void         ReadAliasStatement(RangeListType *nlist);
void         CheckPrintf(char argstr[], unsigned int n);
unsigned int AliasReadAddress();
int          Str2FixedPoint(char str[]);
void         outword(unsigned int x, FILE *stream);
void         UpdateMakefile(FILE *f, char filename[], char nodename[]);
void         WriteMakefile(FILE *f, char filename[]);
 
/* --------------------------------------------------------- */
int Str2FixedPoint(char str[])
{
    double x = atof(str) * 65536.0;
    
    if (x >= 0)
    {
        return (int) (x + 0.5);
    }
    else
    {
        return (int) (x - 0.5);
    }
}
 
/* --------------------------------------------------------- */
unsigned int NextLabel()
{
    NumberOfLabels += 1;
    if (NumberOfLabels >= MaxLabels)
    {
        Error(101, "Too many labels (%d)", MaxLabels);
    }
    return NumberOfLabels;
}

/* --------------------------------------------------------- */
void SetLabel(unsigned int lab, unsigned int addr)
{
    Labels[lab] = addr;
    GenCode2(s_LAB, lab);
}

/* --------------------------------------------------------- */
unsigned int GetLabel(unsigned int lab)
{
    return Labels[lab];
}

/* --------------------------------------------------------- */
int Str2Int(char str[])
{
    unsigned int p;
    unsigned int base;
    char         ch;
    int          x;
    
    x    = 0;
    p    = 0;
    base = 10;
    
    if (str[0] == '\'')
    {
        if (str[1] == '\\')
        {
            switch (str[2])
            {
            case 'a':
                return '\a';
                break;
            case 'b':
                return '\b';
                break;
            case 'f':
                return '\f';
                break;
            case 'n':
                return '\n';
                break;
            case 'r':
                return '\r';
                break;
            case 't':
                return '\t';
                break;
            case 'v':
                return '\v';
                break;
            case '\\':
                return '\\';
                break;
            case '?':
                return '\?';
                break;
            case '\'':
                return '\'';
                break;
            case '\"':
                return '\"';
                break;
            default:
                Error(1000, "Unrecognised \\ character <%s>\n", str);
            }
        }
        else
        {
            return (int) str[1];
        }
    }

    if (str[p] == '0')
    {
        base = 8;
        p    = p + 1;

        if (str[p] == 'x' || str[p] == 'X')
        {
            base = 16;
            p    = p + 1;
        }
    }

    while (1)
    {
        ch = str[p];
        if (ch == '\0')
        {
            return x;
        }
        if ((ch >= '0' && ch <= '7' && base == 8) || (ch >= '0' && ch <= '9' && base > 8))
        {
            x = x * base + ch - '0';
        }

        else if (ch >= 'a' && ch <= 'f' && base == 16)
        {
            x = x * base + 10 + ch - 'a';
        }
        else if (ch >= 'A' && ch <= 'F' && base == 16)
        {
            x = x * base + 10 + ch - 'A';
        }
        else
        {
            Error(1005, "Bad integer string <%s>\n", str);
        }
        p = p + 1;
    }
}

/* --------------------------------------------------------- */
void Error(unsigned int code, char *fmt, ...)
{
    va_list list;
    char    *p, *r;
    int     e;
    float   f;
    
    va_start(list, fmt);

    if (code < 1000)
    {
        printf("Fatal error %d at line  %d\n", code, LineNumber);
    }
    else if (code < 2000)
    {
        printf("Compilation error %d at line %d\n", code, LineNumber);
        Errors += 1;
    }
    else if (code < 3000)
    {
        printf("CG error %d\n", code);
    }
    else
    {
        printf("Run-time error %d\n", code);
    }
     
    for (p=fmt; *p; ++p)
    {
        if (*p != '%')
        {
            printf("%c", *p);
        } 
        else 
        {
            switch (*++p)
            {
                case 's':
                {
                    r = va_arg(list, char *);
                    printf("%s", r);
                    continue;
                }
 
                case 'i':
                {
                    e = va_arg(list, int);
                    printf("%i", e);
                    continue;
                }
 
                case 'd':
                {
                    e = va_arg(list, int);
                    printf("%d", e);
                    continue;
                }
 
                case 'f':
                {
                    f = va_arg(list, double);
                    printf("%f", f);
                    continue;
                }
 
                default: 
                     printf("%c", *p);
            }
        }
    }
    
    va_end(list);

    if (code < 1000)  /* fatal error */
    {
        Shutdown();
        exit(EXIT_FAILURE);
    }
}

/* --------------------------------------------------------- */
void CopyString(char a[], char b[])
{
    strcpy(b, a);  /* copy string <a> to <b> */
}

/* --------------------------------------------------------- */
void FileBackSpace(char Ch)
{
    if (Ch == EOL)
    {
        LineNumber = LineNumber - 1;
    }
    ungetc(Ch, FileStream);
}

/* --------------------------------------------------------- */
char Rdch()
{
    char Ch;

    do
    {
        Ch = fgetc(FileStream);
    } while (Ch == CR);

    if (Ch == EOL)
    {
        LineNumber += 1;
        if (LineNumber > MaxLines)
        {
            Error(236, "Too many lines in program (%d)\n", LineNumber);
        }
        externmode = false;
        if (Including == 0)
        {
            LineNumbers[LineNumber].codeoffset = PC;
        }
    }

    return Ch;
}

/* --------------------------------------------------------- */
void ReadName(char Str[], char Ch)  /* Ch is an alphabetic character */
{
    unsigned int p;

    p = 0;

    while (((Ch >= 'A') && (Ch <= 'Z')) ||
           ((Ch >= 'a') && (Ch <= 'z')) ||
           ((Ch == '_') && (p > 0)) ||
           ((Ch >= '0') && (Ch <= '9') && (p > 0)))
    {
        Str[p] = Ch;
        p     += 1;
        Ch     = Rdch();
        if (Ch == EOF)
        {
            Error(1010, "EOF encountered reading a name\n");
        }
    }

    Str[p] = 0;
    FileBackSpace(Ch);
}

/* --------------------------------------------------------- */
bool SameString(char a[], char b[])
{
    return strcmp(a, b) == 0;
}

/* --------------------------------------------------------- */
void ReadCharacter(char str[], char Ch)
{
    unsigned int p;
    char         ch;

    p      = 1;
    str[0] = Ch;
    do
    {
        ch = Rdch();
        if (Ch == EOF)
        {
            Error(1015, "EOF encountered reading a character\n");
        }
        str[p] = ch;
        p      = p + 1;
    } while (ch != '\'');

    str[p] = 0;
}

/* --------------------------------------------------------- */
void ReadNumber(char Str[], char Ch)  /* Ch is a numeric character */
{
    unsigned int p;

    p = 0;

    while (((Ch >= '0') && (Ch <= '9')) ||
           ((Ch >= 'A') && (Ch <= 'F')) ||
           ((Ch >= 'a') && (Ch <= 'f')) ||
           (Ch == '.') || (Ch == 'X') || (Ch == 'x') || (Ch == 'E') || (Ch == 'e'))
    {
        Str[p] = Ch;
        p     += 1;
        Ch     = Rdch();
        if (Ch == EOF)
        {
            Error(1020, "EOF encountered reading a number\n");
        }
    }

    Str[p] = 0;
    FileBackSpace(Ch);
}

/* --------------------------------------------------------- */
void ReadString(char Str[], char tCh)   /* read a string until tCh encountered, stripping leading spaces */
{
    unsigned int p;
    char         Ch;
    char         rCh;
    
    p = 0;
    
    if (tCh == '"')
    {
        Ch = Rdch();

    }
    else  /* skip leading spaces and tabs */
    {
        do
        {
            Ch = Rdch();
            if (Ch == EOL)
            {
                Error(1025, "EOL encountered reading a string\n");
            }
            if (Ch == EOF)
            {
                Error(1030, "EOF encountered reading a string\n");
            }
        } while (!(((Ch != SPACE) && (Ch != TAB)) || (Ch == EOF)));
    }
    
    do
    {
        Str[p] = Ch;
        p     += 1;
        Ch     = Rdch();
        rCh    = Ch;
        
        if (Ch == '\\')
        {
            Ch = Rdch();
            switch (Ch)
            {
                case 'a':
                    Ch = '\a';
                    break;
                case 'b':
                    Ch = '\b';
                    break;
                case 'f':
                    Ch = '\f';
                    break;
                case 'n':
                    Ch = '\n';
                    break;
                case 'r':
                    Ch = '\r';
                    break;
                case 't':
                    Ch = '\t';
                    break;
                case 'v':
                    Ch = '\v';
                    break;
                case '\\':
                    Ch = '\\';
                    break;
                case '?':
                    Ch = '\?';
                    break;
                case '\'':
                    Ch = '\'';
                    break;
                case '\"':
                    Ch = '\"';
                    break;
                default: 
                    Error(1035, "Unrecognised \\ character <%c> in a constant\n", Ch);
                    break;
            }
        }
        
    } while (!(rCh == tCh || rCh == EOL || rCh == EOF));

    Str[p] = 0;
}

/* --------------------------------------------------------- */
unsigned int ReadSymbol(char Str[])
{
    char         Ch, nCh;
    unsigned int i;
    unsigned int d;

    if (SymbolBackSpacing)
    {
        CopyString(LastSymbolString, Str);
        SymbolBackSpacing = false;
        return LastSymbol;
    }

    Str[0] = 0;

    do
    {
        Ch = Rdch();
        if (Ch == EOF)
        {
            return c_EOF;
        }
    } while (Ch == SPACE || Ch == TAB || Ch == EOL);

    switch (Ch)
    {
    case ';':
        LastSymbol = c_SEMICOLON;
        break;

    case ':':
        LastSymbol = c_COLON;
        break;

    case '?':
        LastSymbol = c_QUERY;
        break;

    case '#':
        Ch = Rdch();
        ReadName(Str, Ch);
        for (i = 1; i <= MaxDirectives - 1; i += 1)
        {
            if (SameString(Str, Directives[i]))
            {
                LastSymbol = c_TIMESTAMP + i - 1;
                CopyString(Str, LastSymbolString);
                return LastSymbol;
            }
        }
        Error(1040, "Unknown directive %s\n", Str);
        LastSymbol = c_ERROR;
        break;

    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y':
    case 'Z':
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y':
    case 'z':
        ReadName(Str, Ch);
        d = LookupDefine(Str);
        if (d > 0)
        {
            CopyString(Str, LastSymbolString);
            LastSymbol = c_NUMBER;
            break;
        }

        for (i = 1; i <= MaxKeyWords - 1; i += 1)
        {
            if (SameString(Str, KeyWords[i]))
            {
                CopyString(Str, LastSymbolString);
                LastSymbol = i;
                return LastSymbol;
            }
        }
        CopyString(Str, LastSymbolString);
        LastSymbol = c_VAR;
        break;

    case '\'':
        ReadCharacter(Str, Ch);
        CopyString(Str, LastSymbolString);
        LastSymbol = c_NUMBER;
        break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        ReadNumber(Str, Ch);
        CopyString(Str, LastSymbolString);
        LastSymbol = c_NUMBER;
        break;

    case '"':
        ReadString(Str, '"');
        LastSymbol = c_STRING;
        break;

    case '(':
        LastSymbol = c_LBRACKET;
        break;

    case ')':
        LastSymbol = c_RBRACKET;
        break;

    case '{':
        LastSymbol = c_LCURLY;
        break;

    case '}':
        LastSymbol = c_RCURLY;
        break;

    case '[':
        LastSymbol = c_LSQUARE;
        break;

    case ']':
        LastSymbol = c_RSQUARE;
        break;

    case '=':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_EQUAL;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_ASSIGN;
        }
        break;

    case '!':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_NOT_EQUAL;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_NOT;
        }
        break;

    case '&':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_LOGIC_AND_ASSIGN;
        }
        else if (nCh == '&')
        {
            LastSymbol = c_AND;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_LOGIC_AND;
        }
        break;

    case '|':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_LOGIC_OR_ASSIGN;
        }
        else if (nCh == '|')
        {
            LastSymbol = c_OR;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_LOGIC_OR;
        }
        break;

    case '^':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_LOGIC_XOR_ASSIGN;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_LOGIC_XOR;
        }
        break;

    case '<':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_LESS_OR_EQUAL;
        }
        else if (nCh == '<')
        {
            nCh = Rdch();
            if (nCh == '=')
            {
                LastSymbol = c_LSHIFT_ASSIGN;
            }
            else
            {
                FileBackSpace(nCh);
                LastSymbol = c_LSHIFT;
            }
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_LESS;
        }
        break;

    case '>':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_GREATER_OR_EQUAL;
        }
        else if (nCh == '>')
        {
            nCh = Rdch();
            if (nCh == '=')
            {
                LastSymbol = c_RSHIFT_ASSIGN;
            }
            else
            {
                FileBackSpace(nCh);
                LastSymbol = c_RSHIFT;
            }
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_GREATER;
        }
        break;

    case '~':
        LastSymbol = c_ONESCOMP;
        break;

    case '+':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_PLUS_ASSIGN;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_PLUS;
        }
        break;

    case '-':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_MINUS_ASSIGN;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_MINUS;
        }
        break;

    case '*':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_MULT_ASSIGN;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_MULT;
        }
        break;

    case '/':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_DIV_ASSIGN;
        }
        else if (nCh == '/')
        {
            do
            {
                Ch = Rdch();
            } while (!(Ch == EOL || Ch == EOF));
            if (Ch == EOL)
            {
                LastSymbol = ReadSymbol(Str);
            }
            else
            {
                LastSymbol = c_EOF;
            }
        }
        else if (nCh == '*')
        {
            do
            {
                Ch = nCh;
                nCh = Rdch();
            } while(!(((Ch == '*') && nCh == '/') || Ch == EOF));

            if (Ch == EOF)
            {
                Error(1045, "EOF encountered in a comment %s\n", Str);
            }
            else
            {
                LastSymbol = ReadSymbol(Str);
            }
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_DIV;
        }
        break;

    case '%':
        nCh = Rdch();
        if (nCh == '=')
        {
            LastSymbol = c_MOD_ASSIGN;
        }
        else
        {
            FileBackSpace(nCh);
            LastSymbol = c_MOD;
        }
        break;

    case ',':
        LastSymbol = c_COMMA;
        break;

    default:
        Error(1050, "Unrecognised Keyword %s\n", Str);
        return c_ERROR;
        break;
    }

    return LastSymbol;
}

/* --------------------------------------------------------- */
unsigned int NextSymbol(char Str[])
{
    unsigned int k;

    k          = ReadSymbol(Str);
    LastSymbol = k;
    CopyString(Str, LastSymbolString);
    SymbolBackSpacing = true;
    return k;
}

/* --------------------------------------------------------- */
bool Integer(char Str[])
{
    unsigned int p;
    char         ch;

    p = 0;
    while (1)
    {
        ch = Str[p];
        p  = p + 1;
        if (ch == '\0')
        {
            return true;
        }
        if (ch == '.')
        {
            return false;
        }
    }
}

/* --------------------------------------------------------- */
void AddProcedure(char v[], enum VarType ptype)
{
    unsigned int i;
    unsigned int pnum;
    unsigned int s;
    char         Str[MaxStringSize + 1];
    unsigned int nparams;
    enum VarType t;
    unsigned int dim;
    int          x;
      
    pnum    = 0;
    nparams = 0;

    if (NumberOfProcedures >= MaxProcedures)
    {
        Error(102, "Too many procedures <%s> %d\n", v, MaxProcedures);
    }

    if (NumberOfProcedures > 0)
    {
        for (i = 1; i <= NumberOfProcedures; i += 1)
        {
            if (SameString(v, Procedures[i].Name))
            {
                if (Procedures[i].Versions > 1)
                {
                    Error(1055, "Multiple declaration %s\n", v);
                }
                pnum = i;  /* prototype already declared */
                break;
            }
        }
    }

    if (pnum == 0 && SameString(v, "main"))   /* prototype not needed for main */
    {
        unsigned int l = NextLabel();
        NumberOfProcedures += 1;
        CopyString(v, Procedures[NumberOfProcedures].Name);
        Procedures[NumberOfProcedures].ProcType = ptype;
        Procedures[NumberOfProcedures].nArgs = 0;
        Procedures[NumberOfProcedures].BP = 0;
        Procedures[NumberOfProcedures].nLocals = 0;
        pnum = NumberOfProcedures;
        Instructions[1].Arg = l;
        SetLabel(l, PC);        
    }

    if (pnum == 0)  /* prototype declaration */
    {
        NumberOfProcedures += 1;
        CopyString(v, Procedures[NumberOfProcedures].Name);
        Procedures[NumberOfProcedures].ProcType = ptype;
        Procedures[NumberOfProcedures].nArgs = 0;
        Procedures[NumberOfProcedures].Label = NextLabel();
        Procedures[NumberOfProcedures].BP = 0;
        Procedures[NumberOfProcedures].nLocals = 0;
        Procedures[NumberOfProcedures].Versions = 1;
        Procedures[NumberOfProcedures].Offset = 0;
        Procedures[NumberOfProcedures].CallCount = 0;
        Procedures[NumberOfProcedures].TickCount = 0;

        while (1)
        {
            s = ReadSymbol(Str);
            if (s == c_VOID)
            {
                continue;
            }

            if (s == c_INT || s == c_FLOAT)
            {
                t = (s == c_INT) ? IntType : FloatType;
                nparams += 1;
                Procedures[NumberOfProcedures].Args[nparams].vType   = t;
                Procedures[NumberOfProcedures].Args[nparams].vOffset = nparams;
                s = ReadSymbol(Str);
                if (s == c_VAR)
                {
                    s = ReadSymbol(Str);  /* ignore the arg name in prototype */
                }
                
                if (s == c_LSQUARE)
                {
                    dim = 0;
                 
                    while (1)
                    {
                        dim += 1;
                        if (dim > MaxDimensions)
                        {
                            Error(1060, "Too many dimensions in array %s\n", v);
                        }

                        s = NextSymbol(Str);
                        if (s == c_NUMBER)
                        {
                            unsigned int d = LookupDefine(Str);
                            s = ReadSymbol(Str);
                            if (d > 0)
                            {
                                x = Defines[d].DefValue;
                            }
                            else
                            {
                                x = atoi(Str);
                            }
                            if (x < 1)
                            {
                                Error(1065, "Invalid array index %s\n", Str);
                            }
                            else
                            {
                                Procedures[NumberOfProcedures].Args[nparams].vDimensions[dim] = x;
                            }
                        }
                        else if (dim > 1)
                        {
                            Error(1070, "Numeric value expected in array declaration <%s>\n", Str);
                        }
                        s = ReadSymbol(Str);
                        if (s != c_RSQUARE)
                        {
                           Error(1075, "] missing in array declaration <%s>\n", Str);
                        }
                        s = ReadSymbol(Str);
                        if (s != c_LSQUARE)
                        {
                            break;
                        }
                    }
                    
                    Procedures[NumberOfProcedures].Args[nparams].vDimensions[0] = dim;
                }
            }

            if (s == c_RBRACKET)
            {
                break;
            }
            if (s != c_COMMA)
            {
                Error(1080, ", or ) expected in procedure prototype <%s>\n", Str);
            }
        }
        
        s = ReadSymbol(Str);
        if (s != c_SEMICOLON)
        {
            Error(1085, "; expected in procedure prototype <%s>\n", Str);
        }

        Procedures[NumberOfProcedures].nArgs   = nparams;
        Procedures[NumberOfProcedures].nLocals = nparams;
        Procedures[NumberOfProcedures].BP = nparams;  /* args always 1 word */
    }
    else
    {
        CurrentProcedure = pnum;

        Procedures[pnum].Versions = 2;  /* block further declarations */

        SetLabel(Procedures[pnum].Label, PC);
        if (Procedures[pnum].ProcType != ptype)
        {
            Error(1090, "Procedure type differs from procedure prototype <%s>\n", Procedures[pnum].Name);
        }

        while (1)
        {
            s = ReadSymbol(Str);
            if (s == c_VOID)
            {
                continue;
            }

            if (s == c_INT || s == c_FLOAT)


            {
                nparams = nparams + 1;
                t = (s == c_INT) ? IntType : FloatType;
                if (t != Procedures[pnum].Args[nparams].vType)
                {
                    Error(1095, "Arg type differs from procedure prototype <%s>\n", Str);
                }
                s = ReadSymbol(Str);
                if (s == c_VAR)
                {
                    CopyString(Str, Procedures[pnum].Args[nparams].Name);
                }
                else
                {
                    Error(1100, "Variable name expected <%s>\n", Str);
                }

                s = ReadSymbol(Str);
                if (s == c_LSQUARE)
                {
                    dim = 0;
                    
                    while (1)
                    {
                        dim += 1;
                        if (dim > MaxDimensions)
                        {
                            Error(1105, "Too many dimensions in array <%s>\n", v);
                        }

                        s = NextSymbol(Str);
                        if (s == c_NUMBER)
                        {
                            unsigned int d = LookupDefine(Str);
                            s = ReadSymbol(Str);
                            if (d > 0)
                            {
                                x = Defines[d].DefValue;
                            }
                            else
                            {
                                x = atoi(Str);
                            }
                            if (x < 1)
                            {
                                Error(1110, "Invalid array index <%s>\n", Str);
                            }
                            else
                            {
                                if (Procedures[pnum].Args[nparams].vDimensions[dim] != x)
                                {
                                    Error(1115, "Array index differs from prototype <%s>\n", Str);
                                }
                            }
                        }
                        else if (dim > 1)
                        {
                            Error(1120, "Numeric value expected in array declaration <%s>\n", Str);
                        }
                        s = ReadSymbol(Str);
                        if (s != c_RSQUARE)
                        {
                            Error(1125, "] missing in array declaration <%s>\n", Str);
                        }
                        s = ReadSymbol(Str);
                        if (s != c_LSQUARE)
                        {
                            break;
                        }
                    }
                    
                    if (Procedures[pnum].Args[nparams].vDimensions[0] != dim)
                    {
                        Error(1130, "Array dimension differs from prototype %d", dim);
                    }
                }
            }
            if (s == c_RBRACKET)
            {
                ssp = nparams + SAVESPACESIZE;
                if (nparams != Procedures[pnum].nArgs)
                {
                    Error(1131, "Procedure <%s> has %d arguments (%d defined)\n", Procedures[pnum].Name, nparams, Procedures[pnum].nArgs);
                }
                break;
            }
            if (s != c_COMMA)
            {
                Error(1135, ", or ) expected in procedure <%s>\n", Str);
            }
        }
        
        s = ReadSymbol(Str);
        if (s != c_LCURLY)
        {
            Error(1140, "{ expected in procedure <%s>\n", Str);
        }

        GlobalDeclarations = false;  /* they stop once the first function is defined */
        
        GenCode2(s_ENTRY, pnum);
        ssp = nparams + SAVESPACESIZE; 
        GenCode2(s_SAVE, ssp); 
        
        ReadBlock(s);
        
        //if ((Instructions[PC-1].Op != s_RTRN) ||
        //    (Instructions[PC-1].Arg != pnum))
        //{
            GenCode2(s_RTRN, pnum);  /* *** */
        //}
        CurrentProcedure = 0;
    }
}

/* --------------------------------------------------------- */
unsigned int FindLocal(unsigned int proc, char v[])
{
    unsigned int i;

    for (i = 1; i <= Procedures[proc].nLocals; i += 1)
    {
        if (SameString(v, Procedures[proc].Args[i].Name))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindLocalProcedure(char v[])
{
    unsigned int i;

    for (i = 1; i <= NumberOfProcedures; i += 1)
    {
        if (SameString(v, Procedures[i].Name))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindLocalProcedureNumber(unsigned int l)
{
    unsigned int i;

    for (i = 1; i <= NumberOfProcedures; i += 1)
    {
        if (Procedures[i].Label == l)
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int AddString(char str[])
{
    unsigned int s;
    unsigned int b;

    strcpy(LastString, str);  /* remember last string */    
    gvsize += 1;
    if (gvsize >= MaxGvSize)
    {
        Error(103, "Too many globals <%d>\n", gvsize);
    }
    b = gvsize;
    s = strlen(str) + 1;  /* assume 4 bytes per word */
    
    memcpy(&GlobalVector[gvsize], str, s);
    gvsize += s / 4 + 1;
    return b;
}       
            
/* --------------------------------------------------------- */
void AddGlobal(char v[], enum VarType t, bool scalar)
{
    unsigned int s;
    char         Str[MaxStringSize + 1];
    unsigned int dim;
    int          x;
    enum VarType vt;
    
    if (FindGlobal(v) != 0)
    {
        Error(1145, "Double declaration <%s>\n", v);
        return;
    }

    if (FindExternal(v) != 0)
    {
        Error(1146, "External name clash <%s>\n", v);
        return;
    }

    NumberOfGlobals += 1;
    if (NumberOfGlobals >= MaxGlobals)
    {
        Error(104, "Too many global variables (%d) %s\n", MaxGlobals, v);
    }

    CopyString(v, Globals[NumberOfGlobals].Name);

    dim = 0;
    x = 0;
    
    if (scalar)
    {
        s = NextSymbol(Str);
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            x = ReadConstantExpression(&vt);
        }
        
        gvsize += 1;
        if (gvsize >= MaxGvSize)
        {
            Error(103, "Too many globals <%d>\n", gvsize);
        }
        GlobalVector[gvsize] = x;

        Globals[NumberOfGlobals].vOffset = gvsize;
        Globals[NumberOfGlobals].vType   = t;
    }
    else
    {
        while (1)
        {
            s = ReadSymbol(Str);
            if (s != c_LSQUARE)
            {
               Error(1150, "[ missing in array declaration <%s>\n", Str);
            }

            dim += 1;
            if (dim > MaxDimensions)
            {
                Error(1115, "Too many dimensions in array <%s>\n", v);
            }

            s = ReadSymbol(Str);
            if (s == c_NUMBER)
            {
                unsigned int d = LookupDefine(Str);
                if (d > 0)
                {
                    x = Defines[d].DefValue;
                }
                else
                {
                    x = atoi(Str);
                }
                if (x < 1)
                {
                    Error(1160, "Invalid array index <%s>\n", Str);
                }
                else
                {
                    Globals[NumberOfGlobals].vDimensions[dim] = x;
                }
            }
            else
            {
                Error(1165, "Numeric value expected in array declaration <%s>\n", Str);
            }
            s = ReadSymbol(Str);
            if (s != c_RSQUARE)
            {
                Error(1170, "] missing in array declaration <%s>\n", Str);
            }
            s = NextSymbol(Str);
            if (s != c_LSQUARE)
            {
                break;
            }
        }
    }

    Globals[NumberOfGlobals].vDimensions[0] = dim;
    if (dim > 0)
    {
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            Globals[NumberOfGlobals].vOffset =
                AllocateConstantGlobalArray(Globals[NumberOfGlobals].vDimensions, t, 1);
        }
        else
        {
            Globals[NumberOfGlobals].vOffset =
                AllocateGlobalArray(Globals[NumberOfGlobals].vDimensions);
        }
        Globals[NumberOfGlobals].vType = t;
    }
}       
            

/* --------------------------------------------------------- */
void AddExternal(char v[], enum VarType t, bool scalar)
{
    unsigned int s;
    char         Str[MaxStringSize + 1];
    unsigned int dim;
    int          x;
    enum VarType vt;
    
    if (FindExternal(v) != 0)
    {
        Error(1175, "Double declaration <%s>\n", v);
        return;
    }

    if (FindGlobal(v) != 0)
    {
        Error(1176, "Global name clash <%s>\n", v);
        return;
    }

    NumberOfExternals += 1;
    if (NumberOfExternals >= MaxExternals)
    {
        Error(105, "Too many external variables (%d) %s\n", MaxExternals, v);
    }

    CopyString(v, Externals[NumberOfExternals].Name);

    dim = 0;
    x = 0;
    
    if (scalar)
    {
        s = NextSymbol(Str);
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            x = ReadConstantExpression(&vt);
        }
        
        evsize += 1;
        if (evsize >= MaxEvSize)
        {
            Error(125, "Too many externals <%d>\n", evsize);
        }
        ExternalVector[evsize] = x;

        Externals[NumberOfExternals].vOffset = evsize;
        Externals[NumberOfExternals].vType   = t;
    }
    else
    {
        while (1)
        {
            s = ReadSymbol(Str);
            if (s != c_LSQUARE)
            {
               Error(1180, "[ missing in array declaration <%s>\n", Str);
            }

            dim += 1;
            if (dim > MaxDimensions)
            {
                Error(1185, "Too many dimensions in array <%s>\n", v);
            }

            s = ReadSymbol(Str);
            if (s == c_NUMBER)
            {
                unsigned int d = LookupDefine(Str);
                if (d > 0)
                {
                    x = Defines[d].DefValue;
                }
                else
                {
                    x = atoi(Str);
                }
                if (x < 1)
                {
                    Error(1190, "Invalid array index <%s>\n", Str);
                }
                else
                {
                    Externals[NumberOfExternals].vDimensions[dim] = x;
                }
            }
            else
            {
                Error(1195, "Numeric value expected in array declaration <%s>\n", Str);
            }
            s = ReadSymbol(Str);
            if (s != c_RSQUARE)
            {
                Error(1200, "] missing in array declaration <%s>\n", Str);
            }
            s = NextSymbol(Str);
            if (s != c_LSQUARE)
            {
                break;
            }
        }
    }

    Externals[NumberOfExternals].vDimensions[0] = dim;
    if (dim > 0)
    {
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            Externals[NumberOfExternals].vOffset =
                AllocateConstantExternalArray(Externals[NumberOfExternals].vDimensions, t, 1);
        }
        else
        {
            Externals[NumberOfExternals].vOffset =
                AllocateExternalArray(Externals[NumberOfExternals].vDimensions);
        }
        Externals[NumberOfExternals].vType = t;
    }
}       
            
/* --------------------------------------------------------- */
unsigned int AllocateConstantExternalArray(unsigned int dimensions[], enum VarType t, unsigned int d)
{
    unsigned int b;
    unsigned int i;
    unsigned int Op;
    char         Str[MaxStringSize + 1];
    int          x;
    bool         negsign;
    
    b = evsize + 1;
    
    if (d == dimensions[0])
    {
        Op = ReadSymbol(Str);
        if (Op != c_LCURLY)
        {
            Error(1205, "{ missing in constant array declaration <%s>\n", Str);
        }
        for (i=1; i<= dimensions[d]; i+=1)
        {
            unsigned int dn;
            
            negsign = false;
            Op = ReadSymbol(Str);
            if (Op == c_MINUS)
            {
                negsign = true;
                Op = ReadSymbol(Str);
            }
            if (Op != c_NUMBER)
            {
                Error(1210, "Number expected in constant array declaration <%s>\n", Str);
            }
            dn = LookupDefine(Str);
            if (dn > 0)
            {
                x = Defines[dn].DefValue;
            }
            else
            {
                if (t == IntType)
                {
                     x = Str2Int(Str);
                }
                else
                {
                    x = Str2FixedPoint(Str);
                }
                if (negsign)
                {
                    x = -x;
                }
            }
            evsize += 1;
            if (evsize >= MaxEvSize)
            {
                Error(125, "Too many externals <%d>\n", evsize);
            }
            ExternalVector[evsize] = x;
            
            Op = ReadSymbol(Str);
            if (i == dimensions[d])
            {
                if (Op != c_RCURLY)
                {
                    Error(1215, "} missing in constant array declaration <%s>\n", Str);
                }
            }
            else if (Op != c_COMMA)
            {
                Error(1220, ", missing in constant array declaration <%s>\n", Str);
            }
        }
    }
    else
    {
        Op = ReadSymbol(Str);
        if (Op != c_LCURLY)
        {
            Error(1225, "{ missing in constant array declaration <%s>\n", Str);
        }
        for (i=1; i<=dimensions[d]; i+=1)
        {
            AllocateConstantExternalArray(dimensions, t, d+1);
            Op = ReadSymbol(Str);
            if (i == dimensions[d])
            {
                if (Op != c_RCURLY)
                {
                    Error(1230, "} missing in constant array declaration <%s>\n", Str);
                }
            }
            else if (Op != c_COMMA)
            {
                Error(1235, ", missing in constant array declaration <%s>\n", Str);
            }
        }
    }
                
    return b;
}

/* --------------------------------------------------------- */
unsigned int AllocateExternalArray(unsigned int a[])
{
    unsigned int b;
    unsigned int i;
    unsigned int n;

    n = 1;
    for (i=1; i<=a[0]; i+=1)
    {
        n = n * a[i];
    }

    b = evsize + 1;
    for (i=1; i<=n; i+=1)
    {
        evsize += 1;
        if (evsize >= MaxEvSize)
        {
            Error(125, "Too many externals <%d>\n", evsize);
        }
        ExternalVector[evsize] = 0;
    }

    return b;
}

/* --------------------------------------------------------- */
unsigned int AllocateConstantGlobalArray(unsigned int dimensions[], enum VarType t, unsigned int d)
{
    unsigned int b;
    unsigned int i;
    unsigned int Op;
    char         Str[MaxStringSize + 1];
    int          x;
    bool         negsign;
    
    b = gvsize + 1;
    
    if (d == dimensions[0])
    {
        Op = ReadSymbol(Str);
        if (Op != c_LCURLY)
        {
            Error(1240, "{ missing in constant array declaration <%s>\n", Str);
        }
        for (i=1; i<= dimensions[d]; i+=1)
        {
            unsigned int dn;
            
            negsign = false;
            Op = ReadSymbol(Str);
            if (Op == c_MINUS)
            {
                negsign = true;
                Op = ReadSymbol(Str);
            }
            if (Op != c_NUMBER)
            {
                Error(1245, "Number expected in constant array declaration <%s>\n", Str);
            }
            dn = LookupDefine(Str);
            if (dn > 0)
            {
                x = Defines[dn].DefValue;
            }
            else
            {
                if (t == IntType)
                {
                     x = Str2Int(Str);
                }
                else
                {
                    x = Str2FixedPoint(Str);
                }
                if (negsign)
                {
                    x = -x;
                }
            }
            gvsize += 1;
            if (gvsize >= MaxGvSize)
            {
                Error(103, "Too many globals <%d>\n", gvsize);
            }
            GlobalVector[gvsize] = x;
            
            Op = ReadSymbol(Str);
            if (i == dimensions[d])
            {
                if (Op != c_RCURLY)
                {
                    Error(1250, "} missing in constant array declaration <%s>\n", Str);
                }
            }
            else if (Op != c_COMMA)
            {
                Error(1255, ", missing in constant array declaration <%s>\n", Str);
            }
        }
    }
    else
    {
        Op = ReadSymbol(Str);
        if (Op != c_LCURLY)
        {
            Error(1260, "{ missing in constant array declaration <%s>\n", Str);
        }
        for (i=1; i<=dimensions[d]; i+=1)
        {
            AllocateConstantGlobalArray(dimensions, t, d+1);
            Op = ReadSymbol(Str);
            if (i == dimensions[d])
            {
                if (Op != c_RCURLY)
                {
                    Error(1265, "} missing in constant array declaration <%s>\n", Str);
                }
            }
            else if (Op != c_COMMA)
            {
                Error(1270, ", missing in constant array declaration <%s>\n", Str);
            }
        }
    }
                
    return b;
}

/* --------------------------------------------------------- */
unsigned int AllocateGlobalArray(unsigned int a[])
{
    unsigned int b;
    unsigned int i;
    unsigned int n;

    n = 1;
    for (i=1; i<=a[0]; i+=1)
    {
        n = n * a[i];
    }

    b = gvsize + 1;
    for (i=1; i<=n; i+=1)
    {
        gvsize += 1;
        if (gvsize >= MaxGvSize)
        {
            Error(103, "Too many globals <%d>\n", gvsize);
        }
        GlobalVector[gvsize] = 0;
    }

    return b;
}

/* --------------------------------------------------------- */
unsigned int AllocateLocalArray(unsigned int procno, unsigned int a[])
{
    unsigned int b;
    unsigned int i;
    unsigned int n;

    n = 1;
    for (i=1; i<=a[0]; i+=1)
    {
        n = n * a[i];
    }

    b = Procedures[procno].BP + 1;
    Procedures[procno].BP += n;

    return b;
}

/* --------------------------------------------------------- */
void AddLocal(char v[], enum VarType t, bool scalar)
{
    unsigned int i;
    unsigned int s;
    char         Str[MaxStringSize + 1];
    unsigned int n;
    unsigned int dim;
    int          x = 0;
    unsigned int prod;
    unsigned int asize;
    enum VarType r;
    
    if (FindGlobal(v) != 0)
    {
        Error(1276, "Global name clash <%s>\n", v);
    }
    
    if (FindExternal(v) != 0)
    {
        Error(1277, "External name clash <%s>\n", v);
    }
    
    n = Procedures[CurrentProcedure].nLocals;
    if (n > 0)
    {
        for (i = 1; i <= n; i += 1)
        {
            if (SameString(v, Procedures[CurrentProcedure].Args[i].Name))
            {
                Error(1275, "Multiple declaration <%s>\n", v);
                return;
            }
        }
    }

    n = n + 1;
    if (n >= MaxLocals)
    {
        Error(106, "Too many local variables (%d) %s\n", MaxLocals, v);
    }

    Procedures[CurrentProcedure].nLocals = n;
    CopyString(v, Procedures[CurrentProcedure].Args[n].Name);

    dim = 0;

    if (scalar)
    {
        ssp += 1;
        GenCode1(s_QUERY);
        GenCode1(s_STORE);
        s = NextSymbol(Str);
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            r = ReadExpression();
            if (r == IntType && t == FloatType)
            {
                GenCode1(s_FLOAT);              
            }
            else if (r == FloatType && t == IntType)
            {
                GenCode1(s_INT);
            }
            GenCode2(s_SP, Procedures[CurrentProcedure].BP + 1);
        }
        
        Procedures[CurrentProcedure].BP += 1;
        Procedures[CurrentProcedure].Args[n].vOffset = Procedures[CurrentProcedure].BP;
        Procedures[CurrentProcedure].Args[n].vType   = t;
    }
    else
    {
        asize = 1;
        while (1)
        {
            s = ReadSymbol(Str);
            if (s != c_LSQUARE)
            {
               Error(1280, "[ missing in array declaration <%s>\n", Str);
            }

            dim += 1;
            if (dim > MaxDimensions)
            {
                Error(1285, "Too many dimensions in array <%s>\n", v);
            }

            s = ReadSymbol(Str);
            if (s == c_NUMBER)
            {
                unsigned int d = LookupDefine(Str);

                if (d > 0)
                {
                    x = Defines[d].DefValue;
                }
                else
                {
                    x = atoi(Str);
                }
                if (x < 1)
                {
                    Error(1290, "Invalid array index <%s>\n", Str);
                }
                else
                {
                    Procedures[CurrentProcedure].Args[n].vDimensions[dim] = x;
                    asize *= x;
                }
            }
            else
            {
                Error(1295, "Numeric value expected in array declaration <%s>\n", Str);
            }
            s = ReadSymbol(Str);
            if (s != c_RSQUARE)
            {
                Error(1300, "] missing in array declaration <%s>\n", Str);
            }
            s = NextSymbol(Str);
            if (s != c_LSQUARE)
            {
                ssp += asize;
                GenCode2(s_STACK, ssp);
                GenCode1(s_STORE);
                break;
            }
        }
    }

    Procedures[CurrentProcedure].Args[n].vDimensions[0] = dim;
    if (dim > 0)
    {
        if (s == c_ASSIGN)
        {
            s = ReadSymbol(Str);
            x = AllocateConstantGlobalArray(Procedures[CurrentProcedure].Args[n].vDimensions, t, 1);
            Procedures[CurrentProcedure].Args[n].vOffset =
                AllocateLocalArray(CurrentProcedure, Procedures[CurrentProcedure].Args[n].vDimensions);
            
            prod = bpw;
            for (i=1; i<=dim; i+=1)
            {
                prod = prod * Procedures[CurrentProcedure].Args[n].vDimensions[i];
            }
            GenCode2(s_LLP, Procedures[CurrentProcedure].Args[n].vOffset);
            GenCode2(s_LLG, x);
            GenCode2(s_VCOPY, prod);
        }
        else
        {
            Procedures[CurrentProcedure].Args[n].vOffset =
                AllocateLocalArray(CurrentProcedure, Procedures[CurrentProcedure].Args[n].vDimensions);
        }
        Procedures[CurrentProcedure].Args[n].vType   = t;
    }
}

/* --------------------------------------------------------- */
void AddDefine(char str[], int x, enum VarType t)
{
    if (LookupDefine(str) > 0)
    {
        Error(1305, "Double define declaration <%s>\n", str);
    }

    NumberOfDefines += 1;
    if (NumberOfDefines >= MaxDefines)
    {
        Error(107, "Too many define statements (%d) %s\n", MaxDefines, str);
    }

    CopyString(str, Defines[NumberOfDefines].DefName);
    Defines[NumberOfDefines].DefValue = x;
    Defines[NumberOfDefines].DefType = t;
    //printf("AddDefine: %s=%d %08x\n", str, x, x);
}

/* --------------------------------------------------------- */
unsigned int LookupDefine(char v[])
{
    unsigned int i;

    for (i = 1; i <= NumberOfDefines; i += 1)
    {
        if (SameString(v, Defines[i].DefName))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindGlobal(char v[])
{
    unsigned int i;

    for (i = GLOBALBASE; i <= NumberOfGlobals; i += 1)
    {
        if (SameString(v, Globals[i].Name))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindExternal(char v[])
{
    unsigned int i;

    for (i = 1; i <= NumberOfExternals; i += 1)
    {
        if (SameString(v, Externals[i].Name))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindProcedure(char v[])
{
    unsigned int i;

    for (i = 1; i <= MaxProcs - 1; i += 1)
    {
        if (SameString(v, Procs[i]))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
unsigned int FindFunction(char v[])
{
    unsigned int i;

    for (i = 1; i <= MaxFunctions - 1; i += 1)
    {
        if (SameString(v, Functions[i]))
        {
            return i;
        }
    }
    return 0;
}

/* --------------------------------------------------------- */
void GenCode2(unsigned char OpCode, int Arg)
{
    if (!CodeGenerating)  /* ignore pseudo instruction if not code generating */
    {
        switch(OpCode)
        {
            case s_STACK:
            case s_QUERY:
            case s_STORE:
            case s_SAVE:
            case s_RSTACK:
            case s_LAB:
                return;
        }
    }
    
    if (PC > 2)
    {
        if (Instructions[PC-1].Op == s_LN && Instructions[PC-2].Op == s_LN)  /* worth optimising + and * for constants */
        {
            int arg1 = Instructions[PC-1].Arg;
            int arg2 = Instructions[PC-2].Arg;
            switch (OpCode)
            {
                case s_PLUS:
                    Instructions[PC-2].Arg = arg1 + arg2;
                    PC -= 1;
                    ProgramSize -= 1;
                    return;
                case s_MULT:
                    Instructions[PC-2].Arg = arg1 * arg2;
                    PC -= 1;
                    ProgramSize -= 1;
                    return;
                default:
                    break;
            }
        }
    }
    
    Instructions[PC].Op  = OpCode;
    Instructions[PC].Arg = Arg;
    PC                     += 1;
    ProgramSize            += 1;
    if (ProgramSize >= CodeSize)
    {
        Error(108, "Program too large %d\n", CodeSize);
    }
    stackinc(OpCode, Arg);
}

/* --------------------------------------------------------- */
void GenCode1(unsigned char OpCode)
{
    GenCode2(OpCode, 0);
}

/* --------------------------------------------------------- */
void stackinc(unsigned char OpCode, int Arg)
{
    switch (OpCode)
    {
        case s_LG:
        case s_LP:
        case s_LN:
        case s_LSTR:
        case s_LL:
        case s_LLG:
        case s_LLP:
        case s_LLL:
            ssp += 1;
            return;
            
        case s_EQ:
        case s_NE:
        case s_LS:
        case s_GR:
        case s_LE:
        case s_GE:
        case s_JT:
        case s_JF:
        case s_OR:
        case s_AND:
        case s_PLUS:
        case s_MINUS:
        case s_MULT:
        case s_MULTF:
        case s_DIV:
        case s_DIVF:
        case s_REM:
        case s_SG:
        case s_SP:
        case s_SL:
        case s_LOGAND:
        case s_LOGOR:
        case s_NEQV:
        case s_LSHIFT:
        case s_RSHIFT:
        case s_SWITCHON:
        case s_RES:
            ssp -= 1;
            return;
        
        case s_STIND:
            ssp -= 2;
            return;
        
        default:
            return;
    }
}

/* --------------------------------------------------------- */
void SymbolBackSpace()
{
    SymbolBackSpacing = true;
}

/* --------------------------------------------------------- */
void OpStackInit(OpStackStruct *s)
{
    s->Size = 0;
}

/* --------------------------------------------------------- */
bool OpStackEmpty(OpStackStruct *s)
{
    return s->Size == 0;
}

/* --------------------------------------------------------- */
void OpStackPush(unsigned int Op, OpStackStruct *s)
{
    if (s->Size >= StackSize)
    {
        Error(109, "Op stack overflow (%d)\n", StackSize);
    }
    else
    {
        s->Size          += 1;
        s->Stack[s->Size] = Op;
    }
}

/* --------------------------------------------------------- */
unsigned int OpStackPop(OpStackStruct *s)
{
    if (s->Size <= 0)
    {
        Error(110, "Op stack underflow\n");
        return 0;  /* can't happen */
    }
    else
    {
        s->Size -= 1;
        return s->Stack[s->Size + 1];
    }
}

/* --------------------------------------------------------- */
unsigned int OpStackTop(OpStackStruct *s)
{
    return s->Stack[s->Size];
}

/* --------------------------------------------------------- */
void ArgStackInit(ArgStackStruct *s)
{
    s->Size = 0;
}

/* --------------------------------------------------------- */
void ArgStackPush(enum VarType Arg, ArgStackStruct *s)
{
    if (s->Size >= StackSize)
    {
        Error(111, "Arg stack overflow (%d)\n", StackSize);
    }
    else
    {
        s->Size          += 1;
        s->Stack[s->Size] = Arg;
    }
}

/* --------------------------------------------------------- */
enum VarType ArgStackPop(ArgStackStruct *s)
{
    if (s->Size <= 0)
    {
        Error(112, "Arg stack underflow\n");
        return 0;  /* can't happen */
    }
    else
    {
        s->Size -= 1;
        return s->Stack[s->Size + 1];
    }
}

/* --------------------------------------------------------- */
void ConstStackInit(ConstStackStruct *s)
{
    s->Size = 0;
}

/* --------------------------------------------------------- */
void ConstStackPush(int x, ConstStackStruct *s)
{
    if (s->Size >= StackSize)
    {
        Error(113, "Const stack overflow (%d)\n", StackSize);
    }
    else
    {
        s->Size          += 1;
        s->Stack[s->Size] = x;
    }
}

/* --------------------------------------------------------- */
int ConstStackPop(ConstStackStruct *s)
{
    if (s->Size <= 0)
    {
        Error(114, "Const stack underflow\n");
        return 0;  /* can't happen */
    }
    else
    {
        s->Size -= 1;
        return s->Stack[s->Size + 1];
    }
}

/* --------------------------------------------------------- */
void CheckTypes(enum VarType dest, enum VarType source)
{
    if ((dest == source) || (source == VoidType) || (dest == VoidType))
    {
        return;
    }
    if (dest == IntType)
    {
        GenCode1(s_INT);
    }
    else
    {
        GenCode1(s_FLOAT);
    }
}

/* --------------------------------------------------------- */
void GenOpCode(unsigned int Op, ArgStackStruct *s)
{
    enum VarType t1 = VoidType;
    enum VarType t2 = VoidType;
    bool         IntegerMultiply = false;
    bool         IntegerDivide   = false;
    
    if (CurrentPrototype == NULL)
    {
        Error(115, "No node defined\n");
    }

    switch (Op)
    {
    case c_PLUS:
        t1 = ArgStackPop(s);
        t2 = ArgStackPop(s);
        if (t1 != t2)
        {
            if (t2 == IntType)
            {
                GenCode1(s_SWAP);
                GenCode1(s_FLOAT);
            }
            else
            {
                GenCode1(s_FLOAT);
                t1 = FloatType;
            }
        }
        ArgStackPush(t1, s);
        break;

    case c_MINUS:
    case c_LESS:
    case c_LESS_OR_EQUAL:
    case c_GREATER:
    case c_GREATER_OR_EQUAL:
        t1 = ArgStackPop(s);
        t2 = ArgStackPop(s);
        if (t1 != t2)
        {
            if (t2 == IntType)
            {
                GenCode1(s_SWAP);
                GenCode1(s_FLOAT);
                GenCode1(s_SWAP);
            }
            else
            {
                GenCode1(s_FLOAT);
                t1 = FloatType;
            }
        }
        if (Op == c_MINUS)
        {
            ArgStackPush(t1, s);
        }
        else
        {
            ArgStackPush(IntType, s);
        }
        break;

    case c_MULT:
        t1 = ArgStackPop(s);
        t2 = ArgStackPop(s);
        if (t1 != t2)
        {
            IntegerMultiply = true;
            t1 = FloatType;
        }
        else
        {
            IntegerMultiply = (t1 == IntType);
        }
        ArgStackPush(t1, s);
        break;

    case c_DIV:
        t1 = ArgStackPop(s);
        t2 = ArgStackPop(s);
        if (t1 != t2)
        {
            if (t2 == IntType)
            {
                GenCode1(s_SWAP);
                GenCode1(s_FLOAT);
                GenCode1(s_SWAP);
            }
            else
            {
                IntegerDivide = true;
                t1 = FloatType;
            }
        }
        else
        {
            IntegerDivide = (t1 == IntType);
        }
        ArgStackPush(t1, s);
        break;

    case c_MOD:
    case c_AND:
    case c_OR:
    case c_LOGIC_AND:
    case c_LOGIC_OR:
    case c_LOGIC_XOR:
    case c_LSHIFT:
    case c_RSHIFT:
    case c_EQUAL:
    case c_NOT_EQUAL:
        t1 = ArgStackPop(s);
        t2 = ArgStackPop(s);
        if ((t1 != IntType) || (t2 != IntType))
        {
            Error(1310, "Non-integer arguments in expression\n");
        }
        ArgStackPush(IntType, s);
        break;
    }

    switch (Op)
    {
    case c_PLUS:
        GenCode1(s_PLUS);
        break;

    case c_MINUS:
        GenCode1(s_MINUS);
        break;

    case c_MULT:
        if (IntegerMultiply)
        {
            GenCode1(s_MULT);
        }
        else
        {
            GenCode1(s_MULTF);
        }
        break;

    case c_DIV:
        if (IntegerDivide)
        {
            GenCode1(s_DIV);
        }
        else
        {
            GenCode1(s_DIVF);
        }
        break;

    case c_MOD:
        GenCode1(s_REM);
        break;

    case c_AND:
        GenCode1(s_AND);
        break;

    case c_OR:
        GenCode1(s_OR);
        break;

    case c_NEG:
        GenCode1(s_NEG);
        break;

    case c_NOT:
        GenCode1(s_NOT);
        break;
        
    case c_INT:
        GenCode1(s_INT);
        break;
        
    case c_FLOAT:
        GenCode1(s_FLOAT);
        break;

    case c_LOGIC_AND:
        GenCode1(s_LOGAND);
        break;

    case c_LOGIC_OR:
        GenCode1(s_LOGOR);
        break;

    case c_LOGIC_XOR:
        GenCode1(s_NEQV);
        break;

    case c_LSHIFT:
        GenCode1(s_LSHIFT);
        break;

    case c_RSHIFT:
        GenCode1(s_RSHIFT);
        break;

    case c_ONESCOMP:
        GenCode1(s_COMP);
        break;

    case c_EQUAL:
        GenCode1(s_EQ);
        break;

    case c_NOT_EQUAL:
        GenCode1(s_NE);
        break;

    case c_LESS:
        GenCode1(s_LS);
        break;

    case c_GREATER:
        GenCode1(s_GR);
        break;

    case c_LESS_OR_EQUAL:
        GenCode1(s_LE);
        break;

    case c_GREATER_OR_EQUAL:
        GenCode1(s_GE);
        break;

    default:
        Error(1315, "GenOpCode: unknown op code %d\n", Op);
        break;
    }
}

/* --------------------------------------------------------- */
unsigned int Precedence(unsigned int Op)
{
    switch (Op)
    {
    case c_QUERY:
        return 1;
        break;

    case c_OR:
        return 2;
        break;

    case c_AND:
        return 3;
        break;

    case c_LOGIC_OR:
        return 4;
        break;

    case c_LOGIC_XOR:
        return 5;
        break;

    case c_LOGIC_AND:
        return 6;
        break;

    case c_EQUAL:
    case c_NOT_EQUAL:
        return 7;
        break;

    case c_LESS:
    case c_LESS_OR_EQUAL:
    case c_GREATER:
    case c_GREATER_OR_EQUAL:
        return 8;
        break;

    case c_LSHIFT:
    case c_RSHIFT:
        return 9;
        break;

    case c_PLUS:
    case c_MINUS:
        return 10;
        break;

    case c_MULT:
    case c_DIV:
    case c_MOD:
        return 11;
        break;

    case c_NEG:
    case c_NOT:
    case c_INT:
    case c_FLOAT:
        return 12;
        break;

    default:
        return 0;
        break;
    }
}

/* --------------------------------------------------------- */
void ReadAddress()
{
    unsigned int Op;
    unsigned int p;
    char         Str[MaxStringSize + 1];
    char         tStr[MaxStringSize + 1];
    unsigned int s;
    unsigned int i;
    unsigned int prod;
    unsigned int nbrackets;
    unsigned int adjust[MaxDimensions + 1];
    
    Op = ReadSymbol(Str);
    
    if (Op == c_VAR)
    {
        if (CurrentProcedure > 0)
        {
            p = FindLocal(CurrentProcedure, Str);
            if (p > 0)
            {
                if (Procedures[CurrentProcedure].Args[p].vDimensions[0] == 0)
                {
                    GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p].vOffset);
                    return;
                }
                else
                {
                    if (p <= Procedures[CurrentProcedure].nArgs)
                    {
                        GenCode2(s_LP, Procedures[CurrentProcedure].Args[p].vOffset);
                    }
                    else
                    {
                        GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p].vOffset);
                    }
                    if (NextSymbol(tStr) == c_LSQUARE)
                    {
                        nbrackets = 0;
                        while (1)
                        {
                            nbrackets += 1;
                            if (nbrackets > Procedures[CurrentProcedure].Args[p].vDimensions[0])
                            {
                                Error(1320, "Too many dimensions in array <%s>\n", tStr);
                            }
                            s = ReadSymbol(tStr);
                            ReadExpression();
                            s = ReadSymbol(tStr);
                            if (s != c_RSQUARE)
                            {
                                Error(1325, "] missing in array <%s>\n", tStr);
                            }
                            if (NextSymbol(tStr) != c_LSQUARE)
                            {
                                while(nbrackets < Procedures[CurrentProcedure].Args[p].vDimensions[0])
                                {
                                    adjust[nbrackets] = PC;
                                    GenCode2(s_LN, 0);   /* to be filled in later */
                                    GenCode1(s_MULT);
                                    GenCode1(s_PLUS);
                                    GenCode2(s_LN, 0);
                                    nbrackets += 1;
                                }
                                GenCode2(s_LN, bpw);
                                GenCode1(s_MULT);
                                GenCode1(s_PLUS);
                                break;
                            }
                            adjust[nbrackets] = PC;
                            GenCode2(s_LN, 0);   /* to be filled in later */
                            GenCode1(s_MULT);
                            GenCode1(s_PLUS);
                        }
                        prod = bpw;
                        for (i=1; nbrackets>0; i+=1)
                        {
                            if (i > 1)
                            {
                                Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                            }
                            prod = prod * Procedures[CurrentProcedure].Args[p].vDimensions[nbrackets];
                            nbrackets -= 1;
                        }
                    }
                    return;
                }
            }
        }
        p = FindGlobal(Str);
        if (p > 0)
        {
            if (Globals[p].vDimensions[0] == 0)
            {
                 GenCode2(s_LLG, Globals[p].vOffset);
            }
            else
            {
                GenCode2(s_LLG, Globals[p].vOffset);
                if (NextSymbol(tStr) == c_LSQUARE)
                {
                    nbrackets = 0;
                    while (1)
                    {
                        nbrackets += 1;
                        if (nbrackets > Globals[p].vDimensions[0])
                        {
                            Error(1330, "Too many dimensions in global array <%s>\n", tStr);
                        }
                        s = ReadSymbol(tStr);
                        ReadExpression();
                        s = ReadSymbol(tStr);
                        if (s != c_RSQUARE)
                        {
                            Error(1335, "] missing in global array <%s>\n", tStr);
                        }
                        if (NextSymbol(tStr) != c_LSQUARE)
                        {
                            while(nbrackets < Globals[p].vDimensions[0])
                            {
                                adjust[nbrackets] = PC;
                                GenCode2(s_LN, 0);   /* to be filled in later */
                                GenCode1(s_MULT);
                                GenCode1(s_PLUS);
                                GenCode2(s_LN, 0);
                                nbrackets += 1;
                            }
                            GenCode2(s_LN, bpw);
                            GenCode1(s_MULT);
                            GenCode1(s_PLUS);
                            break;
                        }
                        adjust[nbrackets] = PC;
                        GenCode2(s_LN, 0);   /* to be filled in later */
                        GenCode1(s_MULT);
                        GenCode1(s_PLUS);
                    }
                        
                    prod = bpw;
                    for (i=1; nbrackets>0; i+=1)
                    {
                        if (i > 1)
                        {
                            Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                        }
                        prod = prod * Globals[p].vDimensions[nbrackets];
                        nbrackets -= 1;
                    }
                }
            }
        }
        else
        {
            p = FindExternal(Str);
            if (p > 0)
            {
                if (Externals[p].vDimensions[0] == 0)
                {
                     GenCode2(s_LN, Externals[p].vOffset * bpw);
                }
                else
                {
                    GenCode2(s_LN, Externals[p].vOffset * bpw);
                    if (NextSymbol(tStr) == c_LSQUARE)
                    {
                        nbrackets = 0;
                        while (1)
                        {
                            nbrackets += 1;
                            if (nbrackets > Externals[p].vDimensions[0])
                            {
                                Error(1340, "Too many dimensions in external array <%s>\n", tStr);
                            }
                            s = ReadSymbol(tStr);
                            ReadExpression();
                            s = ReadSymbol(tStr);
                            if (s != c_RSQUARE)
                            {
                                Error(1345, "] missing in external array <%s>\n", tStr);
                            }
                            if (NextSymbol(tStr) != c_LSQUARE)
                            {
                                while(nbrackets < Externals[p].vDimensions[0])
                                {
                                    adjust[nbrackets] = PC;
                                    GenCode2(s_LN, 0);   /* to be filled in later */
                                    GenCode1(s_MULT);
                                    GenCode1(s_PLUS);
                                    GenCode2(s_LN, 0);
                                    nbrackets += 1;
                                }
                                GenCode2(s_LN, bpw);
                                GenCode1(s_MULT);
                                GenCode1(s_PLUS);
                                break;
                            }
                            adjust[nbrackets] = PC;
                            GenCode2(s_LN, 0);   /* to be filled in later */
                            GenCode1(s_MULT);
                            GenCode1(s_PLUS);
                        }
                            
                        prod = bpw;
                        for (i=1; nbrackets>0; i+=1)
                        {
                            if (i > 1)
                            {
                                Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                            }
                            prod = prod * Externals[p].vDimensions[nbrackets];
                            nbrackets -= 1;
                        }
                    }
                }
            }
            else
            {
                Error(1350, "Unknown variable or function <%s>\n", Str);
            }
        }
    }
    else
    {
        Error(1355, "Address must be a variable <%s>\n", Str);
    }
}

/* --------------------------------------------------------- */
unsigned int AliasReadAddress()
{
    unsigned int Op;
    unsigned int p;
    char         Str[MaxStringSize + 1];
    char         tStr[MaxStringSize + 1];
    unsigned int s;
    unsigned int i;
    unsigned int prod;
    unsigned int nbrackets;
    unsigned int n;
    int          indices[MaxDimensions + 1];
    unsigned int products[MaxDimensions + 1];
    unsigned int address;
    enum VarType vt;
    
    Op = ReadSymbol(Str);
    
    if (Op != c_VAR)
    {
        Error(1355, "Address must be a variable <%s>\n", Str);
    }

    p = FindGlobal(Str);
    if (p == 0)
    {
        Error(1350, "Unknown gloabl variable <%s>\n", Str);
    }
    
    address = Globals[p].vOffset;
    nbrackets = 0;

    if (Globals[p].vDimensions[0] == 0)
    {
        return address;
    }

    if (NextSymbol(tStr) == c_LSQUARE)
    {
        while (1)
        {
            nbrackets += 1;
            if (nbrackets > Globals[p].vDimensions[0])
            {
                Error(1330, "Too many dimensions in global array <%s>\n", tStr);
            }
            s = ReadSymbol(tStr);
            indices[nbrackets] = ReadConstantExpression(&vt);
            s = ReadSymbol(tStr);
            if (s != c_RSQUARE)
            {
                Error(1335, "] missing in global array <%s>\n", tStr);
            }
            if (NextSymbol(tStr) != c_LSQUARE)
            {
                nbrackets = Globals[p].vDimensions[0];
                break;
            }
        }
            
        prod = 1;
        n = nbrackets;
        for (i=1; n>0; i+=1)
        {
            if (i > 1)
            {
                products[n] = prod;
            }
            prod = prod * Globals[p].vDimensions[n];
            n -= 1;
        }
    }
    else
    {
        Error(1335, "[ missing in global array <%s>\n", tStr);
    }
    
    prod = 0;
    for (i=1; i<nbrackets; i+=1)
    {
        prod += indices[i] * products[i];
    }
    
    return address + prod + indices[nbrackets];
}

/* --------------------------------------------------------- */
enum VarType ReadExpression()
{
    unsigned int   Op;
    unsigned int   LastOp;
    unsigned int   p;
    char           Str[MaxStringSize + 1];
    char           tStr[MaxStringSize + 1];
    unsigned int   s;
    unsigned int   nArgs;
    unsigned int   L1, L2;
    bool           uselocalprocedure;
    OpStackStruct  OpStack;
    ArgStackStruct ArgStack;
    unsigned int   i;
    unsigned int   prod;
    enum VarType   t;
    unsigned int   nbrackets;
    unsigned int   adjust[MaxDimensions + 1];

    OpStackInit(&OpStack);
    ArgStackInit(&ArgStack);
    Op = c_NONE;

    while (1)
    {
        LastOp = Op;
        Op     = ReadSymbol(Str);

        if (Op == c_MINUS)
        {
            if (!(LastOp == c_VAR || LastOp == c_NUMBER || LastOp == c_RBRACKET))
            {
                Op = c_NEG;
            }
        }
        else if (Op == c_LOGIC_AND)
        {
            if (!(LastOp == c_VAR || LastOp == c_NUMBER || LastOp == c_RBRACKET))
            {
                ReadAddress();
                return IntType;
            }
        }
        else if (Op == c_LBRACKET)
        {
            s = NextSymbol(tStr);
            if (s == c_INT)
            {
                s = ReadSymbol(tStr);
                if (NextSymbol(tStr) != c_RBRACKET)
                {
                    Error(1360, ") missing in (int) <%s>\n", tStr);
                }
                else
                {
                    s = ReadSymbol(tStr);
                    t = ReadExpression();
                    if (t != IntType)
                    {
                        GenCode1(s_INT);
                    }
                    return IntType;
                }
            }
            else if (s == c_FLOAT)
            {
                s = ReadSymbol(tStr);
                if (NextSymbol(tStr) != c_RBRACKET)
                {
                    Error(1365, ") missing in (float) <%s>\n", tStr);
                }
                else
                {
                    s = ReadSymbol(tStr);
                    t = ReadExpression();
                    if (t != FloatType)
                    {
                        GenCode1(s_FLOAT);
                    }
                    return FloatType;
                }
            }
        }

        switch (Op)
        {
        case c_LBRACKET:
            OpStackPush(Op, &OpStack);
            break;

        case c_RBRACKET:
            while (1)
            {
                if (OpStackEmpty(&OpStack))
                {
                    SymbolBackSpace();
                    return ArgStackPop(&ArgStack);
                }
                else if (OpStackTop(&OpStack) == c_LBRACKET)
                {
                    break;
                }
                else
                {
                    GenOpCode(OpStackPop(&OpStack), &ArgStack);
                }
            }
            OpStackPop(&OpStack);
            break;

        case c_VAR:
            if (CurrentProcedure > 0)
            {
                p = FindLocal(CurrentProcedure, Str);
                if (p > 0)
                {
                    if (Procedures[CurrentProcedure].Args[p].vDimensions[0] == 0)
                    {
                        GenCode2(s_LP, Procedures[CurrentProcedure].Args[p].vOffset);
                        ArgStackPush(Procedures[CurrentProcedure].Args[p].vType, &ArgStack);
                        break;
                    }
                    else
                    {
                        if (p <= Procedures[CurrentProcedure].nArgs)
                        {
                            GenCode2(s_LP, Procedures[CurrentProcedure].Args[p].vOffset);
                        }
                        else
                        {
                            GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p].vOffset);
                        }

                        if (NextSymbol(tStr) == c_LSQUARE)
                        {
                            nbrackets = 0;
                            while (1)
                            {
                                nbrackets += 1;
                                if (nbrackets > Procedures[CurrentProcedure].Args[p].vDimensions[0])
                                {
                                    Error(1370, "Too many dimensions in array <%s>\n", tStr);
                                }
                                s = ReadSymbol(tStr);
                                t = ReadExpression();
                                s = ReadSymbol(tStr);
                                if (s != c_RSQUARE)
                                {
                                    Error(1375, "] missing in array <%s>\n", tStr);
                                }
                                if (NextSymbol(tStr) != c_LSQUARE)
                                {
                                    GenCode2(s_LN, bpw);
                                    GenCode1(s_MULT);
                                    GenCode1(s_PLUS);
                                    break;
                                }
                                adjust[nbrackets] = PC;
                                GenCode2(s_LN, 0);   /* to be filled in later */
                                GenCode1(s_MULT);
                                GenCode1(s_PLUS);
                            }
                            prod = bpw;
                            for (i=1; nbrackets>0; i+=1)
                            {
                                if (i > 1)
                                {
                                    Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                                }
                                prod = prod * Procedures[CurrentProcedure].Args[p].vDimensions[nbrackets];
                                nbrackets -= 1;
                            }
                            if (BoundsChecking)
                            {
                                if (p <= Procedures[CurrentProcedure].nArgs)
                                {
                                    GenCode2(s_LP, Procedures[CurrentProcedure].Args[p].vOffset);
                                }
                                else
                                {
                                    GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p].vOffset);
                                }
                                prod = 1;
                                for (i=1; i<=Procedures[CurrentProcedure].Args[p].vDimensions[0]; i+=1)
                                {
                                    prod *= Procedures[CurrentProcedure].Args[p].vDimensions[i];
                                }
                                GenCode2(s_LN, prod * bpw);
                                GenCode2(s_LBOUNDSCHECK, p);
                            }
                            GenCode1(s_RV);
                            ArgStackPush(Procedures[CurrentProcedure].Args[p].vType, &ArgStack);
                        }
                        else
                        {
                            ArgStackPush(Procedures[CurrentProcedure].Args[p].vType, &ArgStack);
                        }
                        break;
                    }
                }
            }

            p = FindGlobal(Str);
            if (p > 0)
            {
                if (Globals[p].vDimensions[0] == 0)
                {
                     GenCode2(s_LG, Globals[p].vOffset);
                     ArgStackPush(Globals[p].vType, &ArgStack);
                     break;
                }
                else
                {
                    GenCode2(s_LLG, Globals[p].vOffset);
                    if (NextSymbol(tStr) == c_LSQUARE)
                    {
                        nbrackets = 0;
                        while (1)
                        {
                            nbrackets += 1;
                            if (nbrackets > Globals[p].vDimensions[0])
                            {
                                Error(1380, "Too many dimensions in global array <%s>\n", tStr);
                            }
                            s = ReadSymbol(tStr);
                            t = ReadExpression();
                            s = ReadSymbol(tStr);
                            if (s != c_RSQUARE)
                            {
                                Error(1385, "] missing in array <%s>\n", tStr);
                            }
                            if (NextSymbol(tStr) != c_LSQUARE)
                            {
                                GenCode2(s_LN, bpw);
                                GenCode1(s_MULT);
                                GenCode1(s_PLUS);
                                break;
                            }
                            adjust[nbrackets] = PC;
                            GenCode2(s_LN, 0);   /* to be filled in later */
                            GenCode1(s_MULT);
                            GenCode1(s_PLUS);
                        }
                        
                        prod = bpw;
                        for (i=1; nbrackets>0; i+=1)
                        {
                            if (i > 1)
                            {
                                Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                            }
                            prod = prod * Globals[p].vDimensions[nbrackets];
                            nbrackets -= 1;
                        }
                        if (BoundsChecking)
                        {
                            GenCode2(s_LLG, Globals[p].vOffset);
                            prod = 1;
                            for (i=1; i<=Globals[p].vDimensions[0]; i+=1)
                            {
                                prod *= Globals[p].vDimensions[i];
                            }
                            GenCode2(s_LN, prod * bpw);
                            GenCode2(s_GBOUNDSCHECK, p);
                        }
                        GenCode1(s_RV);
                    }
                    ArgStackPush(Globals[p].vType, &ArgStack);
                    break;
                }
            }
            else
            {
                uselocalprocedure = false;
                p                 = FindLocalProcedure(Str);
                if (p > 0)
                {
                    uselocalprocedure = true;
                }
                if (p == 0)
                {
                    p = FindFunction(Str);
                    if (p == 2 || p == 3)  /* abs or fabs? */
                    {
                        t = ReadExpression();  /* change abs(exp) to <exp> s_ABS */
                        GenCode1(s_ABS);
                        return t;
                    }
                    if (p == 8)  /* bitset? */
                    {
                        t = ReadExpression();  /* change abs(exp) to <exp> s_ABS */
                        return VoidType;
                    }
                }
                if (p > 0)
                {
                    s = ReadSymbol(tStr);
                    if (s == c_LBRACKET)
                    {
                        int k = ssp;
                        
                        ssp = ssp + SAVESPACESIZE;
                        nArgs = 0;
                        GenCode2(s_STACK, ssp);
                        while (1)
                        {
                            if (NextSymbol(tStr) == c_RBRACKET)
                            {
                                s = ReadSymbol(tStr);
                                break;
                            }
                            else
                            {
                                SymbolBackSpace();
                            }
                            nArgs += 1;
                            t = ReadExpression();
                            s = ReadSymbol(tStr);
                            if (s == c_RBRACKET)
                            {
                                break;
                            }
                            else if (s != c_COMMA)
                            {
                                Error(1390, "Unknown arg <%s>\n", tStr);
                            }
                        }
                        if (uselocalprocedure)
                        {
                            if (nArgs != Procedures[p].nArgs)
                            {
                                Error(1395, "Procedure <%s> has %d arguments (%d defined)\n", Str, nArgs, Procedures[p].nArgs);
                            }
                            GenCode2(s_LN, Procedures[p].Label);
                            GenCode2(s_FNAP, k);
                            ArgStackPush(Procedures[p].ProcType, &ArgStack);
                        }
                        else
                        {
                            GenCode2(s_LN, nArgs);
                            GenCode2(s_LN, p + 100);
                            GenCode2(s_SYSCALL, k);
                            
                            t = VoidType;
                            switch (p)
                            {
                                case 1: /* getclk */
                                    t = IntType;
                                    break;
                                case 2: /* abs */
                                    t = IntType;
                                    break;
                                case 3: /* fabs */
                                    t = FloatType;
                                    break;
                                case 4: /* createthread */
                                    t = IntType;
                                    break;
                                case 5: /* deletethread */
                                    t = IntType;
                                    break;
                                case 6: /* getbyte */ 
                                    t = IntType;
                                    break;
                                case 7: /* getword */
                                    t = IntType;
                                    break;
                                default:
                                    Error(1400, "Unknown system function %d\n", p+100);
                                    break;
                            }
                            ArgStackPush(t, &ArgStack);
                        }
                        ssp = k + 1;
                    }
                    else
                    {
                        //Error(1405, "( expected in function <%s>\n", Str);
                        //if (Procedures[p].Versions < 2)  /* entry addr must be defined */
                        //{
                        //    Error(1410, "Procedure not defined <%s>\n", Str);
                        //}
                        GenCode2(s_LLL, Procedures[p].Label);
                        ArgStackPush(IntType, &ArgStack);
                        SymbolBackSpace();
                    }
                }
                else
                {
                    Error(1415, "Unknown variable or function <%s>\n", Str);
                }
            }

            break;

        case c_NUMBER:
            p = LookupDefine(Str);

            if (p > 0)
            {
                GenCode2(s_LN, Defines[p].DefValue);
                ArgStackPush(Defines[p].DefType, &ArgStack);
            }
            else if (Integer(Str))
            {
                GenCode2(s_LN, Str2Int(Str));
                ArgStackPush(IntType, &ArgStack);
            }
            else
            {
                GenCode2(s_LN, Str2FixedPoint(Str));
                ArgStackPush(FloatType, &ArgStack);
            }
            break;

        case c_STRING:
            p = AddString(Str);
            GenCode2(s_LSTR, p);
            ArgStackPush(IntType, &ArgStack);
            break;

        case c_PLUS:
        case c_MINUS:
        case c_MULT:
        case c_DIV:
            
        case c_MOD:
        case c_NEG:
        case c_NOT:
        case c_AND:
        case c_OR:
        case c_LOGIC_AND:
        case c_LOGIC_OR:
        case c_LOGIC_XOR:
        case c_LSHIFT:
        case c_RSHIFT:
        case c_ONESCOMP:

        case c_EQUAL:
        case c_NOT_EQUAL:
        case c_LESS:
        case c_LESS_OR_EQUAL:
        case c_GREATER:
        case c_GREATER_OR_EQUAL:
        
            while (1)
            {
                if (OpStackEmpty(&OpStack) || (Precedence(OpStackTop(&OpStack)) < Precedence(Op)))
                {
                    break;
                }
                else
                {
                    GenOpCode(OpStackPop(&OpStack), &ArgStack);
                }
            }
            OpStackPush(Op, &OpStack);
            break;

            
        case c_QUERY:
            L1 = NextLabel();
            L2 = NextLabel();
            GenCode2(s_JF, L1);
            s = ssp;
            t = ReadExpression();
            GenCode2(s_RES, L2);
            ssp = s;
            GenCode2(s_STACK, ssp);
            Op = ReadSymbol(Str);
            if (Op != c_COLON)
            {
                Error(1420, ": expected in conditional expression <%s>\n", Str);
            }
            SetLabel(L1, PC);
            t = ReadExpression();
            GenCode2(s_RES, L2);
            
            Op = ReadSymbol(Str);
            if (Op != c_SEMICOLON && Op != c_COMMA)
            {
                Error(1425, "; expected in conditional expression <%s>\n", Str);
            }
            SetLabel(L2, PC);
            GenCode2(s_RSTACK, s);
            /* falls into default: */

        default:
            while (!OpStackEmpty(&OpStack))
            {
                GenOpCode(OpStackPop(&OpStack), &ArgStack);
            }
            SymbolBackSpace();
            return ArgStackPop(&ArgStack);
        }
    }
}

/* --------------------------------------------------------- */
void EvaluateConstant(unsigned int Op, ConstStackStruct *s)
{
    int x1;
    int x2;
    
    switch (Op)
    {
    case c_PLUS:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x1 + x2, s);
        break;

    case c_MINUS:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x2 - x1, s);
        break;

    case c_MULT:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x1 * x2, s);
        break;

    case c_DIV:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x2 / x1, s);
        break;

    case c_MOD:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x2 % x1, s);
        break;

    case c_NEG:
        x1 = ConstStackPop(s);
        ConstStackPush(-x1, s);
        break;

    case c_LOGIC_AND:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x1 & x2, s);
        break;

    case c_LOGIC_OR:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x1 | x2, s);
        break;

    case c_LOGIC_XOR:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x1 ^ x2, s);
        break;

    case c_LSHIFT:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x2 << x1, s);
        break;

    case c_RSHIFT:
        x1 = ConstStackPop(s);
        x2 = ConstStackPop(s);
        ConstStackPush(x2 >> x1, s);
        break;

    default:
        Error(1430, "EvaluateConstant: unknown Op %d\n", Op);
        break;
    }
}

/* --------------------------------------------------------- */
int ReadConstantExpression(enum VarType *t)
{
    unsigned int     Op;
    unsigned int     LastOp;
    char             Str[MaxStringSize + 1];
    OpStackStruct    OpStack;
    ConstStackStruct ConstStack;
    enum VarType     vt = IntType;
    unsigned int     d;

    OpStackInit(&OpStack);
    ConstStackInit(&ConstStack);
    Op = c_NONE;

    while (1)
    {
        LastOp = Op;
        Op     = ReadSymbol(Str);

        if (Op == c_MINUS)
        {
            if (!(LastOp == c_NUMBER || LastOp == c_RBRACKET))
            {
                Op = c_NEG;
            }
        }

        switch (Op)
        {
        case c_LBRACKET:
            OpStackPush(Op, &OpStack);
            break;

        case c_RBRACKET:
            while (1)
            {
                if (OpStackEmpty(&OpStack))
                {
                    SymbolBackSpace();
                    *t = vt;
                    return ConstStackPop(&ConstStack);
                }
                else if (OpStackTop(&OpStack) == c_LBRACKET)
                {
                    break;
                }
                else
                {
                    EvaluateConstant(OpStackPop(&OpStack), &ConstStack);
                }
            }
            OpStackPop(&OpStack);
            break;

        case c_NUMBER:
            d = LookupDefine(Str);
            if (d > 0)
            {
                ConstStackPush(Defines[d].DefValue, &ConstStack);
            }
            else if (Integer(Str))
            {
                ConstStackPush(Str2Int(Str), &ConstStack);
            }
            else
            {
                vt = FloatType;
                ConstStackPush(Str2FixedPoint(Str), &ConstStack);
            }
            break;

        case c_VAR:
        case c_STRING:
            Error(1435, "Constant expected in a directive <%s>\n", Str);
            break;
            
        case c_PLUS:
        case c_MINUS:
        case c_MULT:
        case c_DIV:
            
        case c_MOD:
        case c_NEG:
        case c_LOGIC_AND:
        case c_LOGIC_OR:
        case c_LOGIC_XOR:
        case c_LSHIFT:
        case c_RSHIFT:

            while (1)
            {
                if (OpStackEmpty(&OpStack) || (Precedence(OpStackTop(&OpStack)) < Precedence(Op)))
                {
                    break;
                }
                else
                {
                    EvaluateConstant(OpStackPop(&OpStack), &ConstStack);
                }
            }
            OpStackPush(Op, &OpStack);
            break;

        default:
            while (!OpStackEmpty(&OpStack))
            {
                EvaluateConstant(OpStackPop(&OpStack), &ConstStack);
            }
            if (Op != c_EOF)
            {
                SymbolBackSpace();
            }
            *t = vt;
            return ConstStackPop(&ConstStack);
        }
    }
}

/* --------------------------------------------------------- */
void ReadBooleanExpression()
{
    unsigned int Op;
    char         Str[MaxStringSize + 1];
    
    Op = NextSymbol(Str);
    if (Op != c_LBRACKET)
    {
        Error(1440, "( expected in boolean expression <%s>\n", Str);
    }

    Op = ReadSymbol(Str);
    ReadExpression();
    Op = ReadSymbol(Str);
    if (Op != c_RBRACKET)
    {
        Error(1445, ") missing in boolean expression <%s>\n", Str);
    }
}

/* --------------------------------------------------------- */
void ReadBlock(unsigned int Op)
{
    char          Str[MaxStringSize + 1];
    unsigned int  oldssp = ssp;
    bool          Declarations = true;
    
    while (NextSymbol(Str) != c_RCURLY)
    {
        Op = ReadStatement();
        if (!Declarations && (Op == c_INT || Op == c_FLOAT || Op == c_VOID))
        {
            Error(1815, "Unexpected declaration of %s\n", (Op == c_INT) ? "INT" : "FLOAT");
        }
        if (Declarations && Op != c_INT && Op != c_FLOAT)
        {
            Declarations = false;
        }
        
        if (Op == c_ERROR)
        {
            break;
        }
        if (Op == c_EOF)
        {
            Error(500, "Missing }\n");
        }
    }
    
    ssp = oldssp;
    Op = ReadSymbol(Str);  /* skip } */
}

/* --------------------------------------------------------- */
void ReadAliasStatement(RangeListType *nlist)
{
    unsigned int  Op;
    unsigned int  p1;
    unsigned int  p2;
    char          Str[MaxStringSize + 1];
    char          tStr[MaxStringSize + 1];
    int           x1;
    unsigned int  oldgvsize;
    unsigned int  oldevsize;
    RangeListType InterruptRangeList;
    unsigned int  i;
    unsigned int  j;
    unsigned int  k;
    unsigned int  l;
    bool          logging;
    unsigned int  chn;
    enum VarType  vt;

    for (i=1; i<=nlist->nItems; i+=1)
    {
        for (j=nlist->Range[i].low; j<=nlist->Range[i].high; j+=1)
        {
            if (j == ProfileNode)
            {
                OpenProfile(j, FileBaseName);
            }
        }
    }
    
    while (1)
    {
        Op = NextSymbol(Str);
        if (Op == c_LOG || Op == c_SNAPSHOT)
        {
            chn = 0;
            logging = (Op == c_LOG);
            
            Op = ReadSymbol(Str);  /* skip directive */
            
            Op = ReadSymbol(tStr);  /* channel name */
            if (Op != c_STRING)
            {
                Error(123, "invalid channel name %s\n", tStr);
            }

            Op = ReadSymbol(Str);  /* window start time */
            if (Op != c_NUMBER)
            {
                Error(1705, "Number expected in WINDOW directive <%s>\n", Str);
            }
            else
            {
                samplingt1 = TimeToTicks(atof(Str));
            }
            
            Op = ReadSymbol(Str);  /* window stop time */
            if (Op != c_NUMBER)
            {
                Error(1710, "Number expected in WINDOW directive <%s>\n", Str);
            }
            else
            {
                samplingt2 = TimeToTicks(atof(Str));
            }
            
            Op = ReadSymbol(Str);  /* window interval time */
            if (Op != c_NUMBER)
            {
                Error(1715, "Number expected in WINDOW directive <%s>\n", Str);
            }
            else
            {
                samplingt3 = TimeToTicks(atof(Str));
            }

            Op = ReadSymbol(Str);  /* log format */
            if (Op != c_STRING)
            {
                Error(123, "format string expected in log directive (%s)\n", Str);
            }

            for (i=1; i<=nlist->nItems; i+=1)
            {
                for (j=nlist->Range[i].low; j<=nlist->Range[i].high; j+=1)
                {
                    NumberOfLogs += 1;
                    chn = OpenLog(j, FileBaseName, nlist->Name, tStr, Str, samplingt1, samplingt2, samplingt3, logging);
					if (NumberOfLogs == 1)
					{
					    FirstLog = chn;
					}
                }
            }
            
            while (1)
            {
                x1 = AliasReadAddress();
                for (i=1; i<=nlist->nItems; i+=1)
                {
                    for (j=nlist->Range[i].low; j<=nlist->Range[i].high; j+=1)
                    {
                        for (k=1; k<=chn; k+=1)
                        {
                            AddLog(k, x1);
                        }
                    }
                }
                
                Op = NextSymbol(tStr);
                if (Op != c_VAR)
                {
                    break;
                }
            }
            continue;
        }

        if (Op != c_VAR)
        {
            return;
        }
        
        Op = ReadSymbol(Str);
        Op = ReadSymbol(tStr);
        
        if (Op == c_ASSIGN)
        {
            p1 = FindGlobal(Str);
            if (p1 > 0)
            {
                if (Globals[p1].vDimensions[0] == 0)
                {
                    x1 = ReadConstantExpression(&vt);
                    Op = ReadSymbol(Str);
                    if (Op != c_SEMICOLON)
                    {
                        Error(1450, "; missing\n");
                    }

                    p2 = Globals[p1].vOffset;
                    GlobalVector[p2] = x1;
                }
                else
                {
                    oldgvsize = gvsize;
                    gvsize = Globals[p1].vOffset - 1;
                    AllocateConstantGlobalArray(Globals[p1].vDimensions, Globals[p1].vType, 1);
                    gvsize = oldgvsize;
                    Op = ReadSymbol(Str);
                    if (Op != c_SEMICOLON)
                    {
                        Error(1455, "; missing\n");
                    }
                }
            }
            else
            {
                p1 = FindExternal(Str);
                if (p1 > 0)
                {
                    if (Externals[p1].vDimensions[0] == 0)
                    {
                        x1 = ReadConstantExpression(&vt);
                        Op = ReadSymbol(Str);
                        if (Op != c_SEMICOLON)
                        {
                            Error(1460, "; missing\n");
                        }

                        p2 = Externals[p1].vOffset;
                        ExternalVector[p2] = x1;
                    }
                    else
                    {
                        oldevsize = evsize;
                        evsize = Externals[p1].vOffset - 1;
                        AllocateConstantExternalArray(Externals[p1].vDimensions, Externals[p1].vType, 1);
                        evsize = oldevsize;
                        Op = ReadSymbol(Str);
                        if (Op != c_SEMICOLON)
                        {
                            Error(1465, "; missing\n");
                        }
                    }
                }
                else
                {
                    Error(1470, "Unknown variable %s in alias statement\n", Str);
                }
            }
        }
        else if (Op == c_COLON)
        {
            p1 = FindLocalProcedure(Str);
            if (p1 == 0)
            {
                Error(1475, "Unknown function in alias statement\n", Str);
            }
            p2 = Labels[p1];
            ReadRange(&InterruptRangeList);
            
            for (i=1; i<=InterruptRangeList.nItems; i+=1)
            {
                for (j=InterruptRangeList.Range[i].low; j<=InterruptRangeList.Range[i].high; j+=1)
                {
                    for (k=1; k<=nlist->nItems; k+=1)
                    {
                        for (l=nlist->Range[k].low; l<=nlist->Range[k].high; l+=1)
                        {
                            Links = AddLink(Links, j, l);
                        }
                    }
                    NumberOfInterrupts += 1;
                    IntVector[NumberOfInterrupts].iNumber = j;
                    IntVector[NumberOfInterrupts].iVector = p2;
                }
            }
        }
        else
        {
            Error(1480, "Unknown delimiter in alias statement %s%s\n", Str, tStr);
        }
    }
}

/* --------------------------------------------------------- */
unsigned int ReadStatement()
{
    unsigned int    Op;
    unsigned int    p1;
    char            Str[MaxStringSize + 1];
    char            tStr[MaxStringSize + 1];
    int             x1 = 0;
    unsigned int    L1, L2;
    unsigned int    nArgs;
    unsigned int    s = 0;
    SwitchItem      CaseList[MaxSwitches];
    unsigned int    nCases;
    unsigned int    i;
    unsigned int    j;
    unsigned int    k;
    unsigned int    DefaultLabel;
    unsigned int    SwitchLabel;
    bool            neglabel;
    unsigned int    prod;
    enum VarType    t;
    unsigned int    nbrackets;
    unsigned int    adjust[MaxDimensions + 1];
    struct NodeInfo *aliasnode;
    enum VarType    vt;
	
    Op = ReadSymbol(Str);

    switch (Op)
    {
    case c_EXTERN:
        externmode = true;
        return c_EXTERN;
        break;
        
    case c_LCURLY:
        ReadBlock(Op);
        return c_LCURLY;
        break;
        
    case c_VAR:
    {
        Instruction  ebuff[MaxInstBuff];
        unsigned int ebuffp = 0;
        unsigned int pc1;
            
        pc1 = 0;
        s = ReadSymbol(tStr);
        if (s == c_LBRACKET)
        {
            int  k = ssp;
            char str1[MaxStringSize];
            
            str1[0] = '\0';
            
            ssp = ssp + SAVESPACESIZE;
            nArgs = 0;
            GenCode2(s_STACK, ssp);
            while (1)
            {
                if (NextSymbol(tStr) == c_RBRACKET)
                {
                    Op = ReadSymbol(tStr);
                    break;
                }
                nArgs += 1;
                t = ReadExpression();
                if (nArgs == 1)
                {
                    strcpy(str1, LastString);  /* may be needed to check printf */
                }
                
                s = ReadSymbol(tStr);
                if (s == c_RBRACKET)
                {
                    break;
                }
                else if (s != c_COMMA)
                {
                    Error(1485, "Unknown arg <%s>\n", tStr);
                }
            }
            p1 = FindLocalProcedure(Str);
            if (p1 > 0)
            {
                GenCode2(s_LN, Procedures[p1].Label);
                GenCode2(s_RTAP, k);
                if (Procedures[p1].ProcType != VoidType)  /* NB. Strictly, this should be a warning */
                {
                    GenCode2(s_DISCARD, 0);  /* pop function result off the stack */
                }
            }
            else
            {
                p1 = FindProcedure(Str);
                if (p1 > 0)
                {
                    GenCode2(s_LN, nArgs);
                    GenCode2(s_LN, p1);
                    GenCode2(s_SYSCALL, k);
                    if (p1 == 3)  /* it's printf */
                    {
                        CheckPrintf(str1, nArgs);
                    }
                }
            }
            ssp = k;
            
            if (p1 > 0)
            {
                s = ReadSymbol(Str);
                if (s != c_SEMICOLON)
                {
                    Error(1490, "Semi-colon expected after procedure <%s>\n", Str);
                }
            }
            else
            {
                Error(1495, "Unknown procedure <%s>\n", Str);
            }
            break;
        }

        if (s == c_LSQUARE)
        {
            SymbolBackSpace();
            p1 = FindGlobal(Str);
            if (p1 > 0)
            {
                pc1 = PC;  /* remember loading of array address */
                GenCode2(s_LLG, Globals[p1].vOffset);
                nbrackets = 0;
                while (1)
                {
                    nbrackets += 1;
                    if (nbrackets > Globals[p1].vDimensions[0])
                    {
                        Error(1500, "Too many dimensions in global array <%s>\n", tStr);
                    }
                    s = ReadSymbol(tStr);
                    t = ReadExpression();
                    s = ReadSymbol(tStr);
                    if (s != c_RSQUARE)
                    {
                        Error(1505, "] missing in array <%s>\n", tStr);
                    }
                    if (NextSymbol(tStr) != c_LSQUARE)
                    {
                        GenCode2(s_LN, bpw);
                        GenCode1(s_MULT);
                        GenCode1(s_PLUS);
                        break;
                    }
                    adjust[nbrackets] = PC;
                    GenCode2(s_LN, 0);   /* to be filled in later */
                    GenCode1(s_MULT);
                    GenCode1(s_PLUS);
                }
                        
                prod = bpw;
                for (i=1; nbrackets>0; i+=1)
                {
                    if (i > 1)
                    {
                        Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                    }
                    prod = prod * Globals[p1].vDimensions[nbrackets];
                    nbrackets -= 1;
                }
            }
            else
            {
                p1 = FindLocal(CurrentProcedure, Str);
                if (p1 > 0)
                {
                    pc1 = PC;  /* remember loading of array address */
                    if (p1 <= Procedures[CurrentProcedure].nArgs)
                    {
                        GenCode2(s_LP, Procedures[CurrentProcedure].Args[p1].vOffset);
                    }
                    else
                    {
                        GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p1].vOffset);
                    }
                    nbrackets = 0;
                    while (1)
                    {
                        nbrackets += 1;
                        if (nbrackets > Procedures[CurrentProcedure].Args[p1].vDimensions[0])
                        {
                            Error(1510, "Too many dimensions in array <%s>\n", tStr);
                        }
                        s = ReadSymbol(tStr);
                        t = ReadExpression();
                        s = ReadSymbol(tStr);
                        if (s != c_RSQUARE)
                        {
                            Error(1515, "] missing in array <%s>\n", tStr);
                        }
                        if (NextSymbol(tStr) != c_LSQUARE)
                        {
                            GenCode2(s_LN, bpw);
                            GenCode1(s_MULT);
                            GenCode1(s_PLUS);
                            break;
                        }
                        adjust[nbrackets] = PC;
                        GenCode2(s_LN, 0);   /* to be filled in later */
                        GenCode1(s_MULT);
                        GenCode1(s_PLUS);
                    }
                    prod = bpw;
                    for (i=1; nbrackets>0; i+=1)
                    {
                        if (i > 1)
                        {
                            Instructions[adjust[nbrackets]].Arg = prod;  /* fill in coeff */
                        }
                        prod = prod * Procedures[CurrentProcedure].Args[p1].vDimensions[nbrackets];
                        nbrackets -= 1;
                    }
                }
            }
            s = ReadSymbol(tStr);
        }

        if (s == c_ASSIGN)
        {
            if (pc1 > 0)
            {
                ebuffp = 0;  /* copy array address evaluation to a local buffer */
                if (pc1 != PC)
                {
                    unsigned int p;
                    for (p=pc1; p<PC; p+=1)
                    {
                        ebuff[ebuffp].Op = Instructions[p].Op;
                        ebuff[ebuffp].Arg = Instructions[p].Arg;
                        ebuffp += 1;
                        if (ebuffp >= MaxInstBuff)
                        {
                            Error(116, "Temporary instruction buffer overflow (%d)\n", MaxInstBuff);
                        }
                        ProgramSize -= 1;
                    }
                }
                PC = pc1;  /* overwrite array address evaluation code */
            }
            
            t = ReadExpression();
            if (CurrentProcedure > 0)
            {
                p1 = FindLocal(CurrentProcedure, Str);
                if (p1 > 0)
                {
                    if (Procedures[CurrentProcedure].Args[p1].vDimensions[0] == 0)
                    {
                        CheckTypes(Procedures[CurrentProcedure].Args[p1].vType, t);
                        GenCode2(s_SP, Procedures[CurrentProcedure].Args[p1].vOffset);
                    }
                    else
                    {
                        CheckTypes(Procedures[CurrentProcedure].Args[p1].vType, t);
                        if (ebuffp > 0)
                        {
                            unsigned int p;
                            for (p=0; p<ebuffp; p+=1)
                            {
                                GenCode2(ebuff[p].Op, ebuff[p].Arg);
                            }
                        }
                        if (BoundsChecking)
                        {
                            if (p1 <= Procedures[CurrentProcedure].nArgs)
                            {
                                GenCode2(s_LP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            else
                            {
                                GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            prod = 1;
                            for (i=1; i<=Procedures[CurrentProcedure].Args[p1].vDimensions[0]; i+=1)
                            {
                                prod *= Procedures[CurrentProcedure].Args[p1].vDimensions[i];
                            }
                            GenCode2(s_LN, prod * bpw);
                            GenCode2(s_LBOUNDSCHECK, p1);
                        }
                        GenCode1(s_STIND);
                    }
                    s = ReadSymbol(Str);
                    if (s != c_SEMICOLON && s != c_RBRACKET)
                    {
                        Error(1520, "; or ) expected after expression <%s>\n", Str);
                    }
                    return c_VAR;
                }
            }

            p1 = FindGlobal(Str);
            if (p1 > 0)
            {
                if (Globals[p1].vDimensions[0] == 0)
                {
                    CheckTypes(Globals[p1].vType, t);
                    GenCode2(s_SG, Globals[p1].vOffset);
                }
                else
                {
                    CheckTypes(Globals[p1].vType, t);
                    if (ebuffp > 0)
                    {
                        unsigned int p;
                        for (p=0; p<ebuffp; p+=1)
                        {
                            GenCode2(ebuff[p].Op, ebuff[p].Arg);
                        }
                    }
                    if (BoundsChecking)
                    {
                        GenCode2(s_LLG, Globals[p1].vOffset);
                        prod = 1;
                        for (i=1; i<=Globals[p1].vDimensions[0]; i+=1)
                        {
                            prod *= Globals[p1].vDimensions[i];
                        }
                        GenCode2(s_LN, prod * bpw);
                        GenCode2(s_GBOUNDSCHECK, p1);
                    }
                    GenCode1(s_STIND);
                }
                s = ReadSymbol(Str);
                if (s != c_SEMICOLON && s != c_RBRACKET)
                {
                    Error(1525, "; or ) expected after expression <%s>\n", Str);
                }
                return c_VAR;
            }
            else
            {
                Error(1530, "Unknown variable <%s>\n", Str);
            }
        }
        else if (s >= c_PLUS_ASSIGN && s <= c_RSHIFT_ASSIGN)
        {
            if (CurrentProcedure > 0)
            {
                p1 = FindLocal(CurrentProcedure, Str);
                if (p1 > 0)
                {
                    if (Procedures[CurrentProcedure].Args[p1].vDimensions[0] == 0)
                    {
                        GenCode2(s_LP, Procedures[CurrentProcedure].Args[p1].vOffset);
                    }
                    else
                    {
                        GenCode1(s_PUSHTOS);
                        if (BoundsChecking)
                        {
                            if (p1 <= Procedures[CurrentProcedure].nArgs)
                            {
                                GenCode2(s_LP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            else
                            {
                                GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            prod = 1;
                            for (i=1; i<=Procedures[CurrentProcedure].Args[p1].vDimensions[0]; i+=1)
                            {
                                prod *= Procedures[CurrentProcedure].Args[p1].vDimensions[i];
                            }
                            GenCode2(s_LN, prod * bpw);
                            GenCode2(s_LBOUNDSCHECK, p1);
                        }
                        GenCode1(s_RV);
                    }
                    t = ReadExpression();
                    if (t != Procedures[CurrentProcedure].Args[p1].vType)
                    {
                        if (t == IntType)
                        {
                            GenCode1(s_FLOAT);
                            t = FloatType;
                        }
                        else
                        {
                            GenCode1(s_INT);
                            t = IntType;
                        }
                    }
                    
                    switch (s)
                    {
                    case c_PLUS_ASSIGN:
                        GenCode1(s_PLUS);
                        break;
                    case c_MINUS_ASSIGN:
                        GenCode1(s_MINUS);
                        break;
                    case c_MULT_ASSIGN:
                        GenCode1((t == IntType) ? s_MULT : s_MULTF);
                        break;
                    case c_DIV_ASSIGN:
                        GenCode1((t == IntType) ? s_DIV : s_DIVF);
                        break;
                    case c_MOD_ASSIGN:
                        GenCode1(s_REM);
                        break;
                    case c_LOGIC_AND_ASSIGN:
                        GenCode1(s_LOGAND);
                        break;
                    case c_LOGIC_OR_ASSIGN:
                        GenCode1(s_LOGOR);
                        break;
                    case c_LOGIC_XOR_ASSIGN:
                        GenCode1(s_NEQV);
                        break;
                    case c_LSHIFT_ASSIGN:
                        GenCode1(s_LSHIFT);
                        break;
                    case c_RSHIFT_ASSIGN:
                        GenCode1(s_RSHIFT);
                        break;
                    }
                    if (Procedures[CurrentProcedure].Args[p1].vDimensions[0] == 0)
                    {
                        CheckTypes(Procedures[CurrentProcedure].Args[p1].vType, t);
                        GenCode2(s_SP, Procedures[CurrentProcedure].Args[p1].vOffset);
                    }
                    else
                    {
                        CheckTypes(Procedures[CurrentProcedure].Args[p1].vType, t);
                        GenCode1(s_SWAP);
                        if (BoundsChecking)
                        {
                            if (p1 <= Procedures[CurrentProcedure].nArgs)
                            {
                                GenCode2(s_LP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            else
                            {
                                GenCode2(s_LLP, Procedures[CurrentProcedure].Args[p1].vOffset);
                            }
                            prod = 1;
                            for (i=1; i<=Procedures[CurrentProcedure].Args[p1].vDimensions[0]; i+=1)
                            {
                                prod *= Procedures[CurrentProcedure].Args[p1].vDimensions[i];
                            }
                            GenCode2(s_LN, prod * bpw);
                            GenCode2(s_LBOUNDSCHECK, p1);
                        }
                        GenCode1(s_STIND);
                    }
                    s = ReadSymbol(Str);
                    if (s != c_SEMICOLON && s != c_RBRACKET)
                    {
                        Error(1535, "; or ) expected after expression <%s>\n", Str);
                    }
                    return c_VAR;
                }
            }

            p1 = FindGlobal(Str);
            if (p1 > 0)
            {
                if (Globals[p1].vDimensions[0] == 0)
                {
                    GenCode2(s_LG, Globals[p1].vOffset);
                }
                else
                {
                    GenCode1(s_PUSHTOS);
                    if (BoundsChecking)
                    {
                        GenCode2(s_LLG, Globals[p1].vOffset);
                        prod = 1;
                        for (i=1; i<=Globals[p1].vDimensions[0]; i+=1)
                        {
                            prod *= Globals[p1].vDimensions[i];
                        }
                        GenCode2(s_LN, prod * bpw);
                        GenCode2(s_GBOUNDSCHECK, p1);
                    }
                    GenCode1(s_RV);
                }

                t = ReadExpression();
                if (t != Globals[p1].vType)
                {
                    if (t == IntType)
                    {
                        GenCode1(s_FLOAT);
                        t = FloatType;
                    }
                    else
                    {
                        GenCode1(s_INT);
                        t = IntType;
                    }
                }

                switch (s)
                {
                case c_PLUS_ASSIGN:
                    GenCode1(s_PLUS);
                    break;
                case c_MINUS_ASSIGN:
                    GenCode1(s_MINUS);
                    break;
                case c_MULT_ASSIGN:
                    GenCode1((t == IntType) ? s_MULT : s_MULTF);
                    break;
                case c_DIV_ASSIGN:
                    GenCode1((t == IntType) ? s_DIV : s_DIVF);
                    break;
                case c_MOD_ASSIGN:
                    GenCode1(s_REM);
                    break;
                case c_LOGIC_AND_ASSIGN:
                    GenCode1(s_LOGAND);
                    break;
                case c_LOGIC_OR_ASSIGN:
                    GenCode1(s_LOGOR);
                    break;
                case c_LOGIC_XOR_ASSIGN:
                    GenCode1(s_NEQV);
                    break;
                case c_LSHIFT_ASSIGN:
                    GenCode1(s_LSHIFT);
                    break;
                case c_RSHIFT_ASSIGN:
                    GenCode1(s_RSHIFT);
                    break;
                }
                
                if (Globals[p1].vDimensions[0] == 0)
                {
                    CheckTypes(Globals[p1].vType, t);
                    GenCode2(s_SG, Globals[p1].vOffset);
                }
                else
                {
                    CheckTypes(Globals[p1].vType, t);
                    GenCode1(s_SWAP);
                    if (BoundsChecking)
                    {
                        GenCode2(s_LLG, Globals[p1].vOffset);
                        prod = 1;
                        for (i=1; i<=Globals[p1].vDimensions[0]; i+=1)
                        {
                            prod *= Globals[p1].vDimensions[i];
                        }
                        GenCode2(s_LN, prod * bpw);
                        GenCode2(s_GBOUNDSCHECK, p1);
                    }
                    GenCode1(s_STIND);
                }
                s = ReadSymbol(Str);
                if (s != c_SEMICOLON && s != c_RBRACKET)
                {
                    Error(1540, "; or ) expected after expression <%s>\n", Str);
                }
                return c_VAR;
            }
            else
            {
                Error(1545, "Unknown variable <%s>\n", Str);
            }
        }

        else
        {
            Error(1550, "= expected <%s>\n", Str);
        }
        break;
    }

    case c_INT:
        while (1)
        {
            if (ReadSymbol(Str) == c_VAR)
            {
                Op = NextSymbol(tStr);
                if (Op == c_LBRACKET)
                {
                    Op = ReadSymbol(tStr);
                    AddProcedure(Str, IntType);
                    break;
                }
                else if (Op == c_LSQUARE)
                {
                    if (CurrentProcedure > 0)
                    {
                        AddLocal(Str, IntType, false);
                    }
                    else if (externmode && GlobalDeclarations)
                    {
                        AddExternal(Str, IntType, false);
                    }
                    else if (GlobalDeclarations)
                    {
                        AddGlobal(Str, IntType, false);
                    }
                    else
                    {
                        Error(1815, "Unexpected declaration of INT\n");
                    }
                }
                else
                {
                    if (CurrentProcedure > 0)
                    {
                        AddLocal(Str, IntType, true);
                    }
                    else if (externmode && GlobalDeclarations)
                    {
                        AddExternal(Str, IntType, true);
                    }
                    else if (GlobalDeclarations)
                    {
                        AddGlobal(Str, IntType, true);
                    }
                    else
                    {
                        Error(1815, "Unexpected declaration of INT\n");
                    }
                }
            }
            else
            {
                Error(1555, "Variable name or procedure expected in int declaration <%s>\n", Str);
            }

            s = ReadSymbol(Str);
            if (s == c_SEMICOLON)
            {
                return c_INT;
            }
            if (s != c_COMMA)
            {
                Error(1560, "Unexpected symbol in int declaration <%s>\n", Str);
            }
        }
        break;

    case c_FLOAT:
        while (1)
        {
            if (ReadSymbol(Str) == c_VAR)
            {
                Op = NextSymbol(tStr);
                if (Op == c_LBRACKET)
                {
                    Op = ReadSymbol(tStr);
                    AddProcedure(Str, FloatType);
                    break;
                }
                else if (Op == c_LSQUARE)
                {
                    if (CurrentProcedure > 0)
                    {
                        AddLocal(Str, FloatType, false);
                    }
                    else if (externmode && GlobalDeclarations)
                    {
                        AddExternal(Str, FloatType, false);
                    }
                    else if (GlobalDeclarations)
                    {
                        AddGlobal(Str, FloatType, false);
                    }
                    else
                    {
                        Error(1815, "Unexpected declaration of FLOAT\n");
                    }
                }
                else
                {
                    if (CurrentProcedure > 0)
                    {
                        AddLocal(Str, FloatType, true);
                    }
                    else if (externmode && GlobalDeclarations)
                    {
                        AddExternal(Str, FloatType, true);
                    }
                    else if (GlobalDeclarations)
                    {
                        AddGlobal(Str, FloatType, true);
                    }
                    else
                    {
                        Error(1815, "Unexpected declaration of FLOAT\n");
                    }
                }
            }
            else
            {
                Error(1565, "Variable name or procedure expected in float declaration <%s>\n", Str);
            }
            s = ReadSymbol(Str);
            if (s == c_SEMICOLON)
            {
                return c_FLOAT;
            }
            if (s != c_COMMA)
            {
                Error(1570, "Unexpected symbol in float declaration <%s>\n", Str);
            }
        }
        break;

    case c_VOID:
        if (ReadSymbol(Str) == c_VAR)
        {
            if (NextSymbol(tStr) == c_LBRACKET)
            {
                Op = ReadSymbol(tStr);
                AddProcedure(Str, VoidType);
                break;
            }
        }
        else
        {
            Error(1575, "Procedure expected in void declaration <%s>\n", Str);
        }
        printf("ReadSatement: Op=%d\n", Op);
        Op = c_NONE;
        break;

    case c_IF:
        ReadBooleanExpression();
        L1 = NextLabel();
        GenCode2(s_JF, L1);
            
        Op = NextSymbol(Str);
        if (Op == c_LCURLY)
        {
            Op = ReadSymbol(Str);  /* skip { */
            ReadBlock(Op);
        }
        else
        {
            Op = ReadStatement();
        }
                
        Op = NextSymbol(Str);

        if (Op != c_ELSE)  /* no ELSE part needed */
        {
            SetLabel(L1, PC);
        }
        else
        {
            L2 = NextLabel();
            GenCode2(s_JUMP, L2);
            SetLabel(L1, PC);
            Op = ReadSymbol(Str);  /* skip else */
            Op = NextSymbol(Str);
            if (Op == c_LCURLY)
            {
                Op = ReadSymbol(Str);   /* skip { */
                ReadBlock(Op);
            }
            else
            {
                Op = ReadStatement();
            }
                    
            SetLabel(L2, PC);
        }
        return c_IF;
        break;

    case c_RETURN:
        if (CurrentProcedure == 0)
        {
            Error(1580, "RETURN not within a procedure\n");
        }

        if (Procedures[CurrentProcedure].ProcType != VoidType)
        {
            Op = NextSymbol(Str);
            if (Op == c_SEMICOLON)
            {
                Error(1585, "No result returned in function <%s>\n", Procedures[CurrentProcedure].Name);
            }
            else
            {
                t = ReadExpression();
                CheckTypes(Procedures[CurrentProcedure].ProcType, t);
            }
            Op = ReadSymbol(Str);
            if (Op != c_SEMICOLON)
            {
                Error(1590, "; missing in RETURN statement\n");
            }
            GenCode2(s_RTRN, CurrentProcedure);
        }
        else
        {
            Op = ReadSymbol(Str);
            if (Op != c_SEMICOLON)
            {
                Error(1595, "; missing in RETURN statement\n");
            }
            GenCode2(s_RTRN, CurrentProcedure);  /* add return if omitted from void function */
        }

        break;

    case c_BREAK:
        if (BreakLevel == 0)
        {
            Error(1600, "Unexpected BREAK\n");
        }
        GenCode2(s_JUMP, BreakList[BreakLevel]);  /* jump to exit label */
        Op = ReadSymbol(Str);
        if (Op != c_SEMICOLON)
        {
            Error(1605, "; expected after break statement\n");
        }
        return c_BREAK;
        break;

    case c_CONTINUE:
        if (ContinueLevel == 0)
        {
            Error(1610, "Unexpected CONTINUE\n");
        }
        GenCode2(s_JUMP, ContinueList[ContinueLevel]);  /* jump to continue label */
        ContinueListFlag[ContinueLevel] = true;
        Op = ReadSymbol(Str);
        if (Op != c_SEMICOLON)
        {
            Error(1615, "; expected after continue statement\n");
        }
        return c_CONTINUE;

    case c_SWITCH:
    {
        Instruction  ebuff[MaxInstBuff];
        unsigned int ebuffp = 0;
        unsigned int pc1 = 0;
        
        BreakLevel            = BreakLevel + 1;
        BreakList[BreakLevel] = NextLabel();

        nCases       = 0;
        DefaultLabel = 0;  /* may be omitted */

        Op = ReadSymbol(Str);
        if (Op != c_LBRACKET || Op == c_ERROR || Op == c_EOF)
        {
            Error(1620, "( expected in SWITCH statement - found <%s>\n", Str);
        }

        pc1 = PC;  /* remember where switch expression starts */
        t = ReadExpression();
        ebuffp = 0;  /* copy switch code to a local buffer */
        if (pc1 != PC)
        {
            unsigned int p;
            for (p=pc1; p<PC; p+=1)
            {
                ebuff[ebuffp].Op = Instructions[p].Op;
                ebuff[ebuffp].Arg = Instructions[p].Arg;
                ebuffp += 1;
                if (ebuffp >= MaxInstBuff)
                {
                    Error(117, "Temporary instruction buffer overflow (%d)\n", MaxInstBuff);
                }
                ProgramSize -= 1;
            }
        }
        PC = pc1;  /* reset to overwrite code */

        SwitchLabel = NextLabel();
        GenCode2(s_JUMP, SwitchLabel);  /* JUMP to switch list decode */

        Op = ReadSymbol(Str);
        if (Op != c_RBRACKET || Op == c_ERROR || Op == c_EOF)
        {
            Error(1625, ") expected in SWITCH statement found <%s>\n", Str);
        }

        Op = ReadSymbol(Str);
        if (Op != c_LCURLY)
        {
            Error(1630, "{ expected in SWITCH Statement - found <%s>\n", Str);
        }

        while (1)
        {
            Op = NextSymbol(Str);
            while (Op == c_CASE || Op == c_DEFAULT)
            {
                Op = ReadSymbol(Str);
                if (Op == c_DEFAULT)
                {
                    if (DefaultLabel == 0)
                    {
                        DefaultLabel = NextLabel();
                        SetLabel(DefaultLabel, PC);
                    }
                    else
                    {
                        Error(1635, "Multiple default label in a SWITCH statement\n");
                    }
                }
                else
                {
                    unsigned int d;
                    
                    neglabel = false;
                    Op = ReadSymbol(Str);
                    if (Op == c_MINUS)
                    {
                        neglabel = true;
                        Op = ReadSymbol(Str);
                    }
                    if (Op != c_NUMBER)
                    {
                        Error(1640, "CASE label must be a number <%s>\n", Str);
                    }

                    d = LookupDefine(Str);
                    if (d > 0)
                    {
                        x1 = Defines[d].DefValue;
                    }
                    else
                    {
                        if (!Integer(Str))
                        {
                            Error(1645, "CASE label must be an integer <%s>\n", Str);
                        }

                        x1 = Str2Int(Str);
                        if (neglabel)
                        {
                            x1 = -x1;
                        }
                    }
                    
                    for (i = 1; i <= nCases; i = i + 1)
                    {
                        if (CaseList[i].CaseValue == x1)
                        {
                            Error(1650, "Repeated CASE label <%s>\n", Str);
                        }
                    }
                    nCases                       = nCases + 1;
                    CaseList[nCases].CaseValue   = x1;
                    CaseList[nCases].CaseAddress = NextLabel();
                    SetLabel(CaseList[nCases].CaseAddress, PC);
                }

                Op = ReadSymbol(Str);
                if (Op != c_COLON)
                {
                    Error(1655, ": expected in CASE label - found <%s>\n", Str);
                }
                Op = NextSymbol(Str);
            }

            Op = NextSymbol(Str);
            while (Op != c_RCURLY && Op != c_CASE && Op != c_DEFAULT)
            {
                Op = ReadStatement();
                Op = NextSymbol(Str);
            }

            if (Op == c_ERROR || Op == c_EOF)
            {
                Error(1660, "} or CASE expected in WHILE Statement - found <%s>\n", Str);
            }

            if (Op == c_RCURLY)
            {
                Op = ReadSymbol(Str);
                GenCode2(s_JUMP, BreakList[BreakLevel]);  /* jump to exit label */
                break;
            }
        }

        SetLabel(SwitchLabel, PC);
        if (ebuffp > 0)
        {
            unsigned int p;
            for (p=0; p<ebuffp; p+=1)
            {
                GenCode2(ebuff[p].Op, ebuff[p].Arg);
            }
        }
        
        GenCode2(s_SWITCHON, nCases);
        GenCode2(0, DefaultLabel);
        
        for (i = 1; i <= nCases; i = i + 1)
        {
            GenCode2(0, CaseList[i].CaseValue);
            GenCode2(0, CaseList[i].CaseAddress);
        }

        SetLabel(BreakList[BreakLevel], PC);
        BreakLevel = BreakLevel - 1;
        return c_SWITCH;
    }

    case c_WHILE:

        ContinueLevel = ContinueLevel + 1;
        BreakLevel    = BreakLevel + 1;

        L1 = NextLabel();
        ContinueList[ContinueLevel] = L1;
        ContinueListFlag[ContinueLevel] = false;
        SetLabel(L1, PC);

        ReadBooleanExpression();

        L2 = NextLabel();
        BreakList[BreakLevel] = L2;
        GenCode2(s_JF, L2);
        
        Op = NextSymbol(Str);
        if (Op == c_LCURLY)
        {
            Op = ReadSymbol(Str);  /* skip { */
            ReadBlock(Op);
        }
        else
        {
            Op = ReadStatement();
        }

        GenCode2(s_JUMP, L1);
        SetLabel(L2, PC);

        BreakLevel = BreakLevel - 1;
        ContinueLevel = ContinueLevel - 1;
        return c_WHILE;

    case c_DO:
        BreakLevel                  = BreakLevel + 1;
        BreakList[BreakLevel]       = NextLabel();
        ContinueLevel               = ContinueLevel + 1;
        ContinueList[ContinueLevel] = NextLabel();
        ContinueListFlag[ContinueLevel] = false;

        L1 = NextLabel();
        SetLabel(L1, PC);
        
        Op = NextSymbol(Str);
        if (Op == c_LCURLY)
        {
            Op = ReadSymbol(Str);
            ReadBlock(Op);
        }
        else
        {
            Op = ReadStatement();
        }

        Op = ReadSymbol(Str);
        if (Op != c_WHILE)
        {
            Error(1665, "WHILE expected in DO_WHILE Statement - found <%s>\n", Str);
        }

        if (ContinueListFlag[ContinueLevel])
        {
            SetLabel(ContinueList[ContinueLevel], PC);
        }

        ReadBooleanExpression();
        GenCode2(s_JT, L1);

        Op = ReadSymbol(Str);
        if (Op != c_SEMICOLON)
        {
            Error(1670, "Semi-colon expected in DO_WHILE Statement - found <%s>\n", Str);
        }


        SetLabel(BreakList[BreakLevel], PC);
        BreakLevel = BreakLevel - 1;
        ContinueLevel = ContinueLevel - 1;
        return c_DO;

    case c_FOR:
    {
        Instruction  ebuff[MaxInstBuff];
        unsigned int ebuffp = 0;
        unsigned int pc1 = 0;
        
        BreakLevel                  = BreakLevel + 1;
        BreakList[BreakLevel]       = NextLabel();
        ContinueLevel               = ContinueLevel + 1;
        ContinueList[ContinueLevel] = NextLabel();
        ContinueListFlag[ContinueLevel] = false;

        Op = ReadSymbol(Str);
        if (Op != c_LBRACKET || Op == c_ERROR || Op == c_EOF)
        {
            Error(1675, "( expected in FOR loop - found <%s>\n", Str);
        }

        if (NextSymbol(Str) != c_SEMICOLON)
        {
            Op = ReadStatement();   /* exp-1 */
        }
        else
        {
            Op = ReadSymbol(Str);   /* omit exp-1 */
        }

        L1 = NextLabel();
        SetLabel(L1, PC);
        if (NextSymbol(Str) != c_SEMICOLON)
        {
            t = ReadExpression();   /* exp-2 */

            Op = ReadSymbol(Str);
            if (Op != c_SEMICOLON || Op == c_ERROR || Op == c_EOF)
            {
                Error(1680, "; expected in FOR loop - found <%s>\n", Str);
            }

            GenCode2(s_JF, BreakList[BreakLevel]);
        }
        else
        {
            Op = ReadSymbol(Str);  /* omit exp-2 */
        }

        pc1 = PC; /* remember where exp3 starts */
        if (NextSymbol(Str) != c_RBRACKET)
        {
            Op = ReadStatement();   /* exp-3 */
        }
        else
        {
            Op = ReadSymbol(Str);
        }

        ebuffp = 0;  /* copy exp3 to a local buffer, then place after statement code */
        if (pc1 != PC)
        {
            unsigned int p;
            for (p=pc1; p<PC; p+=1)
            {
                ebuff[ebuffp].Op = Instructions[p].Op;
                ebuff[ebuffp].Arg = Instructions[p].Arg;
                ebuffp += 1;
                if (ebuffp >= MaxInstBuff)
                {
                    Error(118, "Temporary instruction buffer overflow (%d)\n", MaxInstBuff);
                }
                ProgramSize -= 1;
            }
        }
        
        PC = pc1;  /* overwrite exp3 code */
                 
        Op = NextSymbol(Str);
        if (Op == c_LCURLY)
        {
            Op = ReadSymbol(Str);  /* skip { */
            ReadBlock(Op);
        }
        else
        {
            Op = ReadStatement();
        }

        if (ContinueListFlag[ContinueLevel])
        {
            SetLabel(ContinueList[ContinueLevel], PC);
        }
        if (ebuffp > 0)
        {
            unsigned int p;
            for (p=0; p<ebuffp; p+=1)
            {
                GenCode2(ebuff[p].Op, ebuff[p].Arg);
            }
        }
        GenCode2(s_JUMP, L1);

        SetLabel(BreakList[BreakLevel], PC);
        BreakLevel = BreakLevel - 1;
        ContinueLevel = ContinueLevel - 1;
        return c_FOR;
    }

    case c_DEFINE:
        Op = ReadSymbol(Str);
        if (Op != c_VAR)
        {
            Error(1685, "Invalid DEFINE statement <%s>\n", Str);
        }
        x1 = ReadConstantExpression(&vt);
        AddDefine(Str, x1, vt);
        return c_DEFINE;

    case c_EOF:
        if (Including > 0)
        {
            break;
        }    /* falls into C_ALIAS or c_NODE */

    case c_ALIAS:
    case c_NODE:
        if (Errors > 0)
        {
            return c_EOF;
        }
        
        if (Op != c_EOF)
        {
            s = ReadSymbol(Str);
            if (s != c_VAR)
            {
                Error(1690, "Node name expected\n");
            }
        }

        if (CurrentPrototype != NULL && Errors == 0)
        {
            struct NodeInfo *b;

            if (CodeGenerating)
            {
                CodeGenerate(FileBaseName, 
                             CurrentPrototype, 
                             Instructions, ProgramSize, 
                             GlobalVector, gvsize,
                             ExternalVector, evsize,
                             Procedures, NumberOfProcedures,
                             Labels, NumberOfLabels);
                UpdateMakefile(MakefileStream, FileBaseName, CurrentPrototype);
            }
            
            b = CreatePrototype(CurrentPrototype, 
                                Instructions, ProgramSize, 
                                GlobalVector, gvsize,
                                Globals, NumberOfGlobals, 
                                ExternalVector, evsize,
                                Externals, NumberOfExternals,
                                Procedures, NumberOfProcedures, 
                                Labels, NumberOfLabels);

            for (i=FirstLine; i<LineNumber; i+=1)
            {
                LineNumbers[i].pnode = b;
            }
            FirstLine = LineNumber + 1;
            
            if (DisAssembling)
            {
                DisAssemble();
            }
            free(CurrentPrototype);
            CurrentPrototype = NULL;
            ResetNode();
        }
        
        if (Op == c_NODE && CurrentPrototype == NULL)
        {
            s = strlen(Str) + 1;
            CurrentPrototype = malloc(s);
            if (CurrentPrototype == NULL)
            {
                Error(119, "Unable to allocate prototype node %s\n", Str);
            }
            memcpy(CurrentPrototype, Str, s);
        }

        if (AliasMode)
        {
            for (i=1; i<=RangeList.nItems; i+=1)
            {
                for (j=RangeList.Range[i].low; j<=RangeList.Range[i].high; j+=1)
                {
                    CreateNode(j,              RangeList.Name, 
                               GlobalVector,   gvsize, 
                               ExternalVector, evsize,
                               IntVector,      NumberOfInterrupts);
                    
                    NumberOfNodes += 1;

                    if (CodeGenerating)
                    {
                        outword(j, LinkerFileStream);
                        k = 0;
                        sprintf(tStr, "%s_%s.aplx", FileBaseName, RangeList.Name);
                        while (1)
                        {
                            putc(tStr[k], LinkerFileStream);
                            if (tStr[k] == '\0')
                            {
                                break;
                            }
                            k += 1;
                        }
                        while ((k % 4) != 3)
                        {
                            putc('\0', LinkerFileStream);
                            k += 1;
                        }
                        outword(gvsize - 50, LinkerFileStream);
                        for (k=50; k<=gvsize; k+=1)
                        {
                            outword(GlobalVector[k], LinkerFileStream);
                        }
                        outword(evsize, LinkerFileStream);
                        for (k=1; k<=evsize; k+=1)
                        {
                            outword(ExternalVector[k], LinkerFileStream);
                        }

                        outword(NumberOfInterrupts, LinkerFileStream);
                        for (k=1; k<=NumberOfInterrupts; k+=1)
                        {
                            struct NodeInfo *pn = FindNamedNode(RangeList.Name);

                            p1 = pn->Instructions[IntVector[k].iVector + 1].Arg;
                            outword(pn->Procedures[p1].Offset, LinkerFileStream);
                            outword(IntVector[k].iNumber, LinkerFileStream);
                        }

                        outword(NumberOfLogs, LinkerFileStream);
                        if (NumberOfLogs > 0)
                        {
                            for (k=1; k<=NumberOfLogs; k+=1)
                            {
                                struct LogInfo *p = GetLog(k + FirstLog - 1);
                                unsigned int   m;
                            
                                outword(p->logmode ? 1 : 0, LinkerFileStream);
                                outword((unsigned int) ((float) p->startwindow * 1000000.0 / (float) CLOCK_FREQUENCY), LinkerFileStream);
                                outword((unsigned int) ((float) p->stopwindow * 1000000.0 / (float) CLOCK_FREQUENCY), LinkerFileStream);
                                outword((unsigned int) ((float) p->sampleinterval * 1000000.0 / (float) CLOCK_FREQUENCY), LinkerFileStream);

                                outword(p->noffsets, LinkerFileStream);
                                if (p->noffsets > 0)
                                {
                                    for (m=1; m<=p->noffsets; m+=1)
                                    {
                                        outword(p->offsets[m], LinkerFileStream);
                                    }
                                }
								
                                m = 0;
                                while (1)
                                {
                                    putc(p->format[m], LinkerFileStream);
                                    if (p->format[m] == '\0')
                                    {
                                        break;
                                    }
                                    m += 1;
                                }
                                while ((m % 4) != 3)
                                {
                                    putc('\0', LinkerFileStream);
                                    m += 1;
                                }

                                m = 0;
                                strcpy(tStr, p->name);
                                while (1)
                                {
                                    putc(tStr[m], LinkerFileStream);
                                    if (tStr[m] == '\0')
                                    {
                                        break;
                                    }
                                    m += 1;
                                }
                                while ((m % 4) != 3)
                                {
                                    putc('\0', LinkerFileStream);
                                    m += 1;
                                }
                            }
                        }
                    }
                }
            }

            AliasMode = false;
            ResetNode();
        }
        
        if (Op == c_ALIAS)
        {
            ReadRange(&RangeList);
            strcpy(RangeList.Name, Str);
            AliasMode = true;
            
            aliasnode = FindNamedNode(Str);
            if (aliasnode == NULL)
            {
                Error(124, "Cannot find parent node %s\n", Str);
            }
            s = sizeof(int) * (aliasnode->GlobalVectorSize + 1);
            memcpy(GlobalVector, aliasnode->G, s);
            gvsize = aliasnode->GlobalVectorSize;
            
            s = sizeof(int) * (aliasnode->ExternalVectorSize + 1);
            memcpy(ExternalVector, aliasnode->E, s);
            evsize = aliasnode->ExternalVectorSize;
            
            s = sizeof(NametableItem) * (aliasnode->NumberOfGlobals + 1);
            memcpy(Globals, aliasnode->Globals, s);
            NumberOfGlobals = aliasnode->NumberOfGlobals;
            
            s = sizeof(NametableItem) * (aliasnode->NumberOfExternals + 1);
            memcpy(Externals, aliasnode->Externals, s);
            NumberOfExternals = aliasnode->NumberOfExternals;
            
            s = sizeof(ProcedureItem) * (aliasnode->NumberOfProcedures + 1);
            memcpy(Procedures, aliasnode->Procedures, s);
            NumberOfProcedures = aliasnode->NumberOfProcedures;
            
            s = sizeof(int) * (aliasnode->NumberOfLabels + 1);
            memcpy(Labels, aliasnode->Labels, s);
            NumberOfLabels = aliasnode->NumberOfLabels;
            
            samplingt1 = 0;
            samplingt2 = 0;
            samplingt3 = 0;
            ReadAliasStatement(&RangeList);
        }
        break;

    case c_TIMESTAMP:
        ReadString(Str, EOL);
        if (SameString(Str, "off"))
        {
            TimeStamping = Off;
        }
        else if (SameString(Str, "on"))
        {
            TimeStamping = On;
        }
        else
        {
            Error(1700, "Unrecognised timestamp mode <%s>\n", Str);
            TimeStamping = Off;
        }
        break;
        
    case c_MONITOR:
        ReadString(Str, EOL);
        if (SameString(Str, "off"))
        {
            Monitoring = false;
        }
        else if (SameString(Str, "on"))
        {
            Monitoring = true;
        }
        else
        {
            Error(1765, "Unknown monitor mode <%s>\n", Str);
            Monitoring = false;
        }
        break;

    case c_INCLUDE:
        if (ReadSymbol(Str) == c_STRING)
        {   unsigned int OldLineNumber = LineNumber;
            LineNumber = 1;
            //printf("include: %s\n", Str);
            ReadFile(Str, true);
            LineNumber = OldLineNumber;
        }
        else
        {
            Error(1770, "String expected in INCLUDE directive <%s>\n", Str);
        }
        break;

    case c_ELSE:
        break;

    case c_SEMICOLON:  /* empty statement */
        break;
        
    default:
        Error(1175, "Unrecognised Keyword (%d)\n", Op);
        return c_ERROR;
        break;
    }
    return Op;
}

/* --------------------------------------------------------- */
void ReadRange(RangeListType *rlist)
{
    unsigned int Op;
    unsigned int x1;
    unsigned int x2;
    char         Str[MaxStringSize + 1];
    
    rlist->nItems = 0;
    
    Op = NextSymbol(Str);
    if (Op != c_NUMBER)
    {
        Error(1780, "Number expected in range list <%s>\n", Str);
    }

    while (Op == c_NUMBER)
    {
        ReadSymbol(Str);
        x1 = atoi(Str);
        Op = NextSymbol(Str);
        if (Op == c_MINUS)
        {
            ReadSymbol(Str);
            Op = NextSymbol(Str);
            if (Op == c_NUMBER)
            {
                x2 = atoi(Str);
                if (x2 < x1)
                {
                    Error(1785, "Number too small in range list<%s>\n", Str);
                }
                rlist->nItems += 1;
                if (rlist->nItems > MaxRangeSize)
                {
                    Error(1790, "Too many items in range list (%d)\n", MaxRangeSize);
                }
                rlist->Range[rlist->nItems].low = x1;
                rlist->Range[rlist->nItems].high = x2;
                ReadSymbol(Str);
                Op = NextSymbol(Str);
            }
            else
            {
                Error(1795, "Number expected in range list <%s>\n", Str);
            }
            if (Op == c_COMMA)
            {
                ReadSymbol(Str);
                Op = NextSymbol(Str);
            }
        }
        else if (Op == c_COMMA)
        {
            rlist->nItems += 1;
            if (rlist->nItems > MaxRangeSize)
            {
                Error(1800, "Too many items in range list (%d)\n", MaxRangeSize);
            }
            rlist->Range[rlist->nItems].low = x1;
            rlist->Range[rlist->nItems].high = x1;
            ReadSymbol(Str);
            Op = NextSymbol(Str);
        }
        else
        {
            if (x1 >= 0)
            {
                rlist->nItems += 1;
                if (rlist->nItems > MaxRangeSize)
                {
                    Error(1805, "Too many items in range list (%d)\n", MaxRangeSize);
                }
                rlist->Range[rlist->nItems].low = x1;
                rlist->Range[rlist->nItems].high = x1;
            }
            break;
        }
    }
}

/* --------------------------------------------------------- */
bool ReadFile(char Filename[], bool include)
{
    unsigned int t;
    FILE         *OldFileStream;

    if (include)
    {
        Including += 1;
    }
    OldFileStream = FileStream;
    
    FileStream = fopen(Filename, "r");
    if (FileStream == NULL)
    {
        return false;
    }
    else
    {
        do
        {
            t = ReadStatement();
        } while (t != c_EOF);
    }

    fclose(FileStream);
    FileStream = OldFileStream;
    if (include)
    {
        Including -= 1;
    }

    return true;
}

/* --------------------------------------------------------- */
bool Compile(char FileName[], bool codegen, bool dis, bool debug, bool bcheck)
{
    unsigned int i;
    char         LinkerFileName[MaxStringSize];
    bool         result;
    unsigned     s;
    
    Errors             = 0;
    TimeStamping       = Off;
    Node               = 0;
    NumberOfNodes      = 0;
    Monitoring         = false;
    Links              = NULL;
    LineNumber         = 1;
    FirstLine          = 1;
    NumberOfDefines    = 0;  /* global to a module */
    NumberOfPrototypes = 0;
    CurrentPrototype   = NULL;
    AliasMode          = false;
    Including          = 0;
    NumberOfLogs       = 0;
	LogFiles           = !(codegen | dis);
	
    for (i=1; i<=MaxLines; i+=1)
    {
        LineNumbers[i].pnode = NULL;
    }

    CodeGenerating = codegen;
    DisAssembling = dis;
    BoundsChecking = bcheck;

    SetFileBaseName(FileName, FileBaseName);
    
    if (CodeGenerating)
    {
        GetFileName(FileName, LinkerFileName, "ldr");
        LinkerFileStream = fopen(LinkerFileName, "wb");
        if (LinkerFileStream == NULL) 
        {
            Error(120, "Can't open linker file %s\n", LinkerFileName);
        } 
        MakefileStream = fopen("dmake", "wb");
        if (MakefileStream == NULL) 
        {
            Error(120, "Can't open Makefile\n");
        }
        nMakefiles = 0;
    }
    
    ResetNode();
    
    GenCode2(s_JUMP, 0);
    result = ReadFile(FileName, false);

    s = sizeof(struct LineInfo) * (LineNumber + 1);
    LineNumberList = malloc(s);
    if (LineNumberList == NULL)
    {
        Error(1820, "Unable to allocate memory for LineNumberList: %d lines\n", LineNumber);
    }
    memcpy(LineNumberList, LineNumbers, s);
    NumberOfLines = LineNumber;

    if (CodeGenerating)
    {
        outword(0, LinkerFileStream);
        fclose(LinkerFileStream);
        WriteMakefile(MakefileStream, FileName);
        fclose(MakefileStream);
    }
    return result;
}

/* --------------------------------------------------------- */
void ResetNode()
{    
    unsigned int i;
    
    SymbolBackSpacing  = false;
    BreakLevel         = 0;
    ContinueLevel      = 0;
    CurrentProcedure   = 0;
    PC                 = 1;
    ProgramSize        = 0;
    NumberOfGlobals    = GLOBALBASE;
    gvsize             = GLOBALBASE;
    NumberOfProcedures = 0;
    NumberOfLabels     = 0;
    NumberOfInterrupts = 0;
    NumberOfExternals  = 0;
    evsize             = 0;
    externmode         = false;
    GlobalDeclarations = true;
    NumberOfLogs       = 0;

    for (i=1; i<=MaxProcedures; i+=1)
    {
        Procedures[i].nLocals = 0;
    }
}

/* --------------------------------------------------------- */
char* FindLocalName(unsigned int pc, unsigned int p)
{
    unsigned int i;
    unsigned int pnum;

    while (pc > 0)
    {
        if (Instructions[pc].Op == s_ENTRY)
        {
            break;
        }
        pc = pc - 1;
    }

    pnum = Instructions[pc].Arg;

    for (i=1; i<=Procedures[pnum].nLocals; i+=1)
    {
        if (Procedures[pnum].Args[i].vOffset == p)
        {
            return (char *) Procedures[pnum].Args[i].Name;
        }
    }
    return NULL;
}

/* --------------------------------------------------------- */
char* FindGlobalName(unsigned int p)
{
    unsigned int i;

    for (i=1; i<=NumberOfGlobals; i+=1)
    {
        if (Globals[i].vOffset == p)
        {
            return (char *) Globals[i].Name;
        }
    }
    return NULL;
}

/* --------------------------------------------------------- */
void reformstring(char str[], char cstr[])
{
    unsigned int i;
    unsigned int j;
    char ch;
    
    i = 0;
    j = 0;
    while (1)
    {
        ch = cstr[j];
        switch (ch)
        {
            case '\a':
                str[i] = '\\';
                ch = 'a';
                i += 1;
                break;
            case '\b':
                str[i] = '\\';
                ch = 'b';
                i += 1;
                break;
            case '\f':
                str[i] = '\\';
                ch = 'f';
                i += 1;
                break;
            case '\n':
                str[i] = '\\';
                ch = 'n';
                i += 1;
                break;
            case '\r':
                str[i] = '\\';
                ch = 'r';
                i += 1;
                break;
            case '\t':
                str[i] = '\\';
                ch = 't';
                i += 1;
                break;
            case '\v':
                str[i] = '\\';
                ch = 'v';
                i += 1;
                break;
            case '\?':
                str[i] = '\\';
                ch = '?';
                i += 1;
                break;
            case '\'':
                str[i] = '\\';
                ch = '\'';
                i += 1;
                break;
            case '\"':
                str[i] = '\\';
                ch = '\"';
                i += 1;
                break;
            default:
                break;
        }
        
        str[i] = ch;
        i += 1;
        j += 1;
        if (ch == '\0')
        {
            return;
        }
    }
}

/* --------------------------------------------------------- */
void DisAssemble()
{
    unsigned int i;
    unsigned int k;
    unsigned int Op;
    int          Arg;
    unsigned int lab;
    char         str[100];
    unsigned int line;
    
    printf("Node: %s\n", CurrentPrototype);
    for (i=1; i<=NumberOfGlobals; i+=1)
    {
        printf("G%d %s %d %d\n", i, Globals[i].Name, Globals[i].vOffset, GlobalVector[i]);
    }
    for (i=1; i<=NumberOfExternals; i+=1)
    {
        printf("M%d %s %d %d\n", i, Externals[i].Name, Externals[i].vOffset, ExternalVector[i]);
    }

    printf("gv:\n");
    for (i=0; i<=gvsize; i+=1)
    {
        printf("0x%08x, ", GlobalVector[i]);
    if (i%10 == 9)
    {
        printf("\n");
    }
    }
    printf("\n");
    
    line = 0;
    for (i=1; i<=ProgramSize; i+=1)
    {
        for (k=0; k<LineNumber; k+=1)
        {
            if ((LineNumbers[k].codeoffset <= i) && (line < k))
            {
                line = k;
            }
        }
        printf("%6d(%d): ", i, line);

        lab = 0;
        for (k=1; k<=NumberOfLabels; k+=1)
        {
            if (i == Labels[k])
            {
                lab = k;
                break;
            }
        }
        if (lab > 0)
        {
            printf("L%d:", k);
        }
        printf("\t");

        Op  = Instructions[i].Op;
        Arg = Instructions[i].Arg;
        
    switch (Op)
        {
        case s_LG:
            printf("LG%d\t%s", Arg, FindGlobalName(Arg));
            break;

        case s_LP:
            printf("LP%d\t%s", Arg, FindLocalName(i, Arg));
            break;

        case s_LSTR:
            reformstring(str, (char *) &GlobalVector[Arg]);
            printf("LSTR\t%d \"%s\"", Arg, str);
            break;

        case s_SG:
            printf("SG%d\t%s", Arg, FindGlobalName(Arg));
            break;

        case s_SP:
            printf("SP%d\t%s", Arg, FindLocalName(i, Arg));
            break;

        case s_LN:
            printf("LN\t%d", Arg);
            break;

        case s_RV:
            printf("RV");
            break;

        case s_STIND:
            printf("STIND");
            break;

        case s_LLP:
            printf("LLP%d\t&%s", Arg, FindLocalName(i, Arg));
            break;

        case s_LLG:
            printf("LLG%d\t&%s", Arg, FindGlobalName(Arg));
            break;

        case s_LLL:
            printf("LLL%d", Arg);
            break;

        case s_EQ:
            printf("CMP ==");
            break;

        case s_NE:
            printf("CMP !=");
            break;

        case s_LS:
            printf("CMP <");
            break;

        case s_GR:
            printf("CMP >");
            break;

        case s_LE:
            printf("CMP <=");
            break;

        case s_GE:
            printf("CMP >=");
            break;

        case s_JT:
            printf("JT\tL%d", Arg);
            break;

        case s_JF:
            printf("JF\tL%d", Arg);
            break;

        case s_JUMP:
            printf("JUMP\tL%d", Arg);
            break;

        case s_SWITCHON:
            printf("SWITCH\t%d", Arg);
            i += 1;
            printf(" L%d\n", Instructions[i].Arg);
            for (k=1; k<=Arg; k+=1)
            {
                i += 1;
                printf("\t\t%d", Instructions[i].Arg);
                i += 1;
                printf(" L%d\n", Instructions[i].Arg);
            } 
            break;

        case s_OR:
            printf("OR");
            break;

        case s_AND:
            printf("AND");
            break;

        case s_PLUS:
            printf("PLUS");
            break;

        case s_MINUS:
            printf("MINUS");
            break;

        case s_MULT:
            printf("MULT");
            break;

        case s_DIV:
            printf("DIV");
            break;

        case s_MULTF:
            printf("MULTF");
            break;

        case s_DIVF:
            printf("DIVF");
            break;

        case s_REM:
            printf("REM");
            break;

        case s_NEG:
            printf("NEG");
            break;

        case s_ABS:
            printf("ABS");
            break;

        case s_NOT:
            printf("NOT");
            break;

        case s_FLOAT:
            printf("FLOAT");
            break;

        case s_INT:
            printf("INT");
            break;

        case s_SWAP:
            printf("SWAP");
            break;

        case s_LBOUNDSCHECK:
            printf("LBOUNDSCHECK");
            break;

        case s_GBOUNDSCHECK:
            printf("GBOUNDSCHECK");
            break;

        case s_LOGAND:
            printf("LOGAND");
            break;
        
        case s_LOGOR:
            printf("LOGOR");
            break;

        case s_NEQV:
            printf("LOGXOR");
            break;

        case s_LSHIFT:
            printf("LSHIFT");
            break;

        case s_RSHIFT:
            printf("RSHIFT");
            break;

        case s_COMP:
            printf("COMP");
            break;

        case s_RTRN:
            printf("RETURN\t%s", Procedures[Arg].Name);
            break;

        case s_ENTRY:
            printf("ENTRY\t%s", Procedures[Arg].Name);
            break;

        case s_FNAP:
            printf("FNAP\t%d ", Arg);
            break;

        case s_RTAP:
            printf("RTAP\t%d ", Arg);
            break;

        case s_SYSCALL:
            printf("SYSCALL\t%d ", Arg);
            Arg = Instructions[i-1].Arg;
            if (Arg < 100)
            {
                printf("%s", Procs[Arg]);
            }
            else
            {
                printf("%s", Functions[Arg - 100]);
            }
            break;

        case s_PUSHTOS:
            printf("PUSHTOS");
            break;

        case s_VCOPY:
            printf("VCOPY\t%d ", Arg);
            break;

        case s_STACK:
            printf("STACK\t%d ", Arg);
            break;

        case s_SAVE:
            printf("SAVE\t%d ", Arg);
            break;

        case s_RES:
            printf("RES\tL%d ", Arg);
            break;

        case s_RSTACK:
            printf("RSTACK\t%d ", Arg);
            break;

        case s_DISCARD:
            printf("DISCARD\t%d ", Arg);
            break;

        case s_LAB:
            printf("LAB\t%d ", Arg);
            break;

        case s_QUERY:
            printf("QUERY");
            break;

        case s_STORE:
            printf("STORE");
            break;

        default:
            Error(1810, "Unknown icode (%d)\n", Op);
            break;
        }

        if (Op != s_SWITCHON)
        {
            printf("\n");
        }
    }
}

/* --------------------------------------------------------- */
struct tnode *AddLink(struct tnode *p, unsigned int s, unsigned int d)
{
    if (p == NULL)
    {
        p = (struct tnode *) malloc(sizeof(struct tnode));
        if (p == NULL)
        {
            Error(121, "Addlink: out of memory\n");
        }
        p->node                   = s;
        p->ndest                  = 1;
        p->destinations[p->ndest] = d;
        p->left                   = NULL;
        p->right                  = NULL;
    }
    else if (s < p->node)
    {
        p->left = AddLink(p->left, s, d);
    }
    else if (s > p->node)
    {
        p->right = AddLink(p->right, s, d);
    }
    else
    {
        p->ndest += 1;
        if (p->ndest > MaxLinks)
        {
            Error(122, "Addlink: Too many connections specified (%d)\n", MaxLinks);
        }
        p->destinations[p->ndest] = d;
    }
    return p;
}

/* --------------------------------------------------------- */
void FreeLinks(struct tnode *p)
{
    if (p != NULL)
    {
        if (p->left != NULL)
        {
            FreeLinks(p->left);
        }
        if (p->right != NULL)
        {
            FreeLinks(p->right);
        }
        free(p);
    }
}

/* --------------------------------------------------------- */
void Shutdown()
{
    FreeLinks(Links);
}

/* --------------------------------------------------------- */
void SetFileBaseName(char FileName[], char FileBaseName[])     /* form base name less the '.d' extension */
{
    unsigned int i;
    unsigned int j;
    char         InputFileExt[3];
    
    for (i=0; i<MaxStringSize; i+=1)
    {
        FileBaseName[i] = FileName[i];
        if (FileBaseName[i] == '.')
        {
            /* get file extension */
            for (j=0; j<3; j++)
            {
                InputFileExt[j] = FileName[i+1+j];
                if (FileName[i+1+j] == '\0')
                    break;
            }

            /* check file extension */
            if (strcmp(InputFileExt, "d") != 0)
            {
                printf("Unknown file extension *.%s\n", InputFileExt);
                exit (1);
            }

            FileBaseName[i] = '\0';
            break;
        }
    }
}

/* --------------------------------------------------------- */
void GetFileName(char infile[], char outfile[], char ext[])
{
    unsigned int i;
    unsigned int j;
      
    for (i=0;;i++) 
    {
        outfile[i] = infile[i];
        if (infile[i] == '.') 
        {
            for (j=0;;j++) 
            {
                outfile[i+j+1] = ext[j];
                if (ext[j] == 0) 
                {
                    return;
                }
            }
            return;
        }
    }   
    Error(123, "Invalid filename %s\n", infile);
}

/* --------------------------------------------------------- */
void CheckPrintf(char argstr[], unsigned int nargs)
{
    unsigned int i = 0;
    unsigned int n = 0;
    
    while (1)
    {
        if (argstr[i] == '\0')
        {
            if (nargs - 1 > n)
            {
                printf("Warning: printf has too many arguments (%d)\n", nargs - 1);
            }
            if (nargs - 1 < n)
            {
                printf("Warning: printf has too few arguments (%d)\n", nargs - 1);
            }
            return;
        }
        if (argstr[i] == '%')
        {
            if (argstr[i+1] != '%')
            {
                n += 1;
            }
        }
        i += 1;
    }
}

/* --------------------------------------------------------- */
void outword(unsigned int x, FILE *stream)
{
    fputc((x >> 24) & 0xff, stream);
    fputc((x >> 16) & 0xff, stream);
    fputc((x >>  8) & 0xff, stream);
    fputc(x         & 0xff, stream);
}

/* --------------------------------------------------------- */
void UpdateMakefile(FILE *f, char filename[], char nodename[])
{
    char str[MaxStringSize];

    sprintf(str, "%s_%s", filename, nodename);
    nMakefiles += 1;
    strcpy(MakefileNames[nMakefiles], str);
}

/* --------------------------------------------------------- */
void WriteMakefile(FILE *f, char filename[])
{
    unsigned int i;

    fprintf(f, "# Makefile for %s\n\nLIB_DIR = ../damsonlib/lib\n\n", filename);
    fprintf(f, "LD := arm-none-linux-gnueabi-ld\n");
    fprintf(f, "OC := arm-none-linux-gnueabi-objcopy\n");
    fprintf(f, "OD := arm-none-linux-gnueabi-objdump\n");
    fprintf(f, "RM := /bin/rm -f\nCAT := /bin/cat\n\n");
    fprintf(f, "all : ");
    for (i=1; i<=nMakefiles; i+=1)
    {
        fprintf(f, "%s ", MakefileNames[i]);
    }
    fprintf(f, "\n\n");

    for (i=1; i<=nMakefiles; i+=1)
    {
        char fstr [MaxStringSize];
        char ostr [MaxStringSize];

        sprintf(fstr, "FILE%d", i);
        sprintf(ostr, "OBJS%d", i);

        fprintf(f, "%s = %s\n", fstr, MakefileNames[i]);
        fprintf(f, "%s = $(%s).o $(LIB_DIR)/damsonlib.o\n\n", ostr, fstr);
        fprintf(f, "$(%s): $(%s) example.lnk\n", fstr, ostr);
        fprintf(f, "\t$(LD) -T example.lnk $(%s)\n", ostr);
        fprintf(f, "\t$(OC) --set-section-flags APLX=alloc,code,readonly a.out $(%s).elf\n", fstr);
        fprintf(f, "\t$(OC) -O binary -j APLX    $(%s).elf APLX.bin\n", fstr);
        fprintf(f, "\t$(OC) -O binary -j RO_DATA $(%s).elf RO_DATA.bin\n", fstr);
        fprintf(f, "\t$(OC) -O binary -j RW_DATA $(%s).elf RW_DATA.bin\n", fstr);
        fprintf(f, "\t$(OD) -Dt $(%s).elf > $(%s).lst\n", fstr, fstr);
        fprintf(f, "\t${CAT} APLX.bin RO_DATA.bin RW_DATA.bin > $(%s).aplx\n", fstr);
        fprintf(f, "\t${RM} APLX.bin RO_DATA.bin RW_DATA.bin a.out\n\n");
    }
    fprintf(f, "clean:\n\t${RM} ");
    for (i=1; i<=nMakefiles; i+=1)
    {
        fprintf(f, "%s.o ", MakefileNames[i]);
    }
    fprintf(f, "*.aplx *.elf *.lst\n\t${RM} APLX.bin RO_DATA.bin RW_DATA.bin a.out\n");
}

