/* DAMSON compiler and code generator
   code generator based on Martin Richard's MC68000 code generator for Tripos
*/
   
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
//#include <err.h>
//#include <sysexits.h>

#include "compiler.h"

#ifdef WIN32
#define EX_SOFTWARE 70
#define EX_OSERR 71
#define errx Error
#else
#include <err.h>
#include <sysexits.h>
#endif

#ifndef O_BINARY       /* needed for Linux */
#define O_BINARY 0
#endif

#define k_lv            020
#define k_loc           001
#define k_lvloc         (k_loc + k_lv)
#define k_glob          002
#define k_lvglob        (k_glob + k_lv)
#define k_lab           003
#define k_lvlab         (k_lab + k_lv)
#define k_numb          040
#define k_reg           050

#define i_AND           0   /* ARM instructions */
#define i_EOR           1
#define i_SUB           2
#define i_RSB           3
#define i_ADD           4
#define i_TST           8
#define i_TEQ           9
#define i_CMP           10
#define i_CMN           11
#define i_ORR           12
#define i_MOV           13
#define i_BIC           14
#define i_MVN           15

#define i_MULT          100  /* pseudo instructions */
#define i_DIV           101
#define i_REM           102
#define i_MULTF         103
#define i_DIVF          104
#define i_NEG           105
#define i_ABS           106
#define i_NOT           107
#define i_FLOAT         108
#define i_INT           109
#define i_LSHIFT        110
#define i_RSHIFT        111
#define i_COMP          112
#define i_RV            113

#define b_EQ            0
#define b_NE            1
#define b_GE            10
#define b_LS            11
#define b_GR            12
#define b_LE            13
#define b_BR            14
#define b_NO            99  /* pseudo branch - no code generated */

#define r_r0            0
#define r_r1            1
#define r_r2            2
#define r_r3            3
#define r_r4            4
#define r_r5            5
#define r_r6            6
#define r_r7            7
#define r_r8            8
#define r_r9            9
#define r_g             10
#define r_p             11
#define r_ip            12
#define r_sp            13
#define r_lr            14
#define r_pc            15

#define allregsused     0x3FF
#define CodeSize        10000
#define MaxConsts       200
#define MaxStrings      5000
#define MaxSwitches     100
#define Maxsstack       1000
#define MaxProcedures   100
#define SAVESPACESIZE   3

typedef struct
{
    unsigned int nLabels;
    unsigned int Loc[MaxLabels + 1];
    unsigned int Label[MaxLabels + 1];
} LabelRefItem;

typedef struct
{
    unsigned int nConsts;
    unsigned int Loc[MaxConsts + 1];
    unsigned int Const[MaxConsts + 1];
} ConstRefItem;


struct lnode
{
    struct lnode *next;
    unsigned int type;
    int          val;
};

typedef struct
{
    unsigned int type;
    int          val;
    unsigned int pos;
} sstackItem;

/* globals */
unsigned int            ssp;
sstackItem              sstack[Maxsstack + 1];
FILE                    *OutputFilestream;
LabelRefItem            ProgLabelRefs;
unsigned int            ProgLabelList[MaxLabels + 1 + 50];
ConstRefItem            ProgConstRefs;
unsigned int            ProgStringList[MaxStrings + 1];
unsigned int            ProgCounter;
unsigned int            ProgSize;
unsigned int            ProgCode[CodeSize + 1];

unsigned int            Op;
unsigned int            pendingop;
unsigned int            arg1;
unsigned int            arg2;
unsigned int            maxssp;
struct lnode            *slave[r_r9 + 1];
struct lnode            *freelist;
struct lnode            dplist[100];
unsigned int            dp;
bool                    incode;
unsigned int            cgdebug;
unsigned int            SYSCALL_Arg;
unsigned int            lastlab = 0;
char                    FileBaseName[MaxStringSize];

unsigned int            NodeNumber;
Instruction             *Instructions;
unsigned int            ProgramSize;
int                     *globalvector;
unsigned int            globalvectorsize;
ProcedureItem           *Procedures;
unsigned int            NumberOfProcedures;
unsigned int            *Labels;
unsigned int            NumberOfLabels;

/* prototypes */
bool         cgdyadic(unsigned int Op, bool swappable);
void         forgetr(unsigned int r);
void         forgetall();
void         cgmonadic(unsigned int Op);
void         OpenOutputFile(char FileName[]);
void         CloseOutputFile();
void         OutputWrch(char Ch);
void         GenRRR(unsigned int Op, unsigned int Rn, unsigned int Rd, unsigned int Rm);
void         GenRRn(unsigned int Op, unsigned int Rn, unsigned int Rd, unsigned int sh, unsigned int n);
void         GenRRRcond(unsigned int Op, unsigned int cond, unsigned int Rn, unsigned int Rd, unsigned int Rm);
void         GenRRncond(unsigned int Op, unsigned int cond, unsigned int Rn, unsigned int Rd, unsigned int sh, unsigned int n);
void         GenLDR(unsigned int r, unsigned int rx, int n);
void         GenSTR(unsigned int r, unsigned int rx, int n);
void         GenBranch(unsigned int cond, unsigned int lab);
void         GenBranchWithLink(unsigned int lab);
void         GenSysCall(unsigned int n, unsigned int r);
void         GenMult(unsigned int Rd, unsigned int Rs, unsigned int Rm);
void         setlab(unsigned int Label);
void         FillinLabels();
void         AddLabelRef(unsigned int label, unsigned int Location);
void         AddConstRef(int val, unsigned int Location);
void         FillinConsts();
void         OutWord(unsigned int w);
void         WriteProgram(char filename[]);
unsigned int powerof2(int x);

void         initstack(unsigned int n);
void         loadt(unsigned int k, int n);
void         storein(unsigned int k, int n);
void         stack(unsigned int n);
void         store(unsigned int a, unsigned int b);
void         storet(unsigned int a);
void         swapargs();
unsigned int regscontaining(unsigned int k, int n);
struct lnode *getblk(struct lnode *a, unsigned int b, int c);
void         freeblk(struct lnode *p);
void         forgetvar(unsigned int k, int n);
void         forgetvars();
unsigned int choosereg(unsigned int a);
unsigned int regsinuse();
void         remem(unsigned int a, unsigned int b, int c);
void         cgpendingop();
unsigned int cgcmp(unsigned int f);
int          condbfn(unsigned int op);
unsigned int compbfn(unsigned int f);
void         initslave();
void         lose1(unsigned int k, int n);
unsigned int rdop();
int          rdn();
unsigned int movetoanyr(unsigned int a);
unsigned int movetor(unsigned int a, unsigned int r);
unsigned int regswithinfo();
void         dboutput(unsigned int lev);
void         wrkn(unsigned int k, int n);
void         cgcondjump(bool b, unsigned int l);
bool         isfree(unsigned int r);
void         freereg(unsigned int r);
unsigned int nextfree();
int          regusedby(unsigned int a);
unsigned int CheckLab();
unsigned int FindLocalProcedureNumber(unsigned int n, unsigned int l);
void         cgapply(unsigned int Op, unsigned int k, bool syscall);
void         cgsave(unsigned int n);
void         cgreturn(unsigned int n);
void         cgstring(unsigned int p);
void         GenAddConstant(unsigned int Rn, unsigned int Rd, int n);
void         GenLoadConstant(unsigned int r, int c);
void         moveinfo(unsigned int s, unsigned int r);
bool         isinslave(unsigned int r, unsigned int k, int n);
void         cgname(char name[]);
unsigned int VarSize(unsigned int node, unsigned int p, unsigned int v);
void         bswitch(unsigned int p, unsigned int q, unsigned int d,
                     int casek[], unsigned int casel[]);
void         stackinc(unsigned char OpCode, int Arg);
void         cgstind();
void         cgrv();
void         GenLDRR(unsigned int r, unsigned int s);
void         GenSTRR(unsigned int Rn, unsigned int Rd, unsigned int Rm);
unsigned int op2size(unsigned int n);
void         cg();
unsigned int cgNextLabel();
unsigned int pack4b(unsigned char a, unsigned char b, unsigned char c, unsigned char d);

/* --------------------------------------------------------- */
void CodeGenerate(char FileBaseName[],
                  char nodename[], 
                  Instruction *code,    unsigned int codesize,
                  int *gv,              unsigned int gvsize,
                  int *ev,              unsigned int evsize,
                  ProcedureItem *procs, unsigned int nprocs,
                  unsigned int *labs,   unsigned int nlabs)
{
    unsigned int i;
    char         OutputFileName[MaxStringSize];

    sprintf(OutputFileName, "%s_%s.o", FileBaseName, nodename);

    Instructions = code;
    ProgramSize = codesize;

    globalvector = gv;
    globalvectorsize = gvsize;

    Procedures = procs;
    NumberOfProcedures = nprocs;

    Labels = labs;
    NumberOfLabels = nlabs;
    
    cg();
    FillinConsts();
    FillinLabels();
    
    for (i=1; i<=nprocs; i+=1)  /* set offset in prototype definition */
    {
        procs[i].Offset = Procedures[i].Offset;
    }
    WriteProgram(OutputFileName);

    printf("Node %s: %d bytes\n", nodename, ProgSize * 4);
}

/* --------------------------------------------------------- */
void setlab(unsigned int Label)
{
    ProgLabelList[Label] = ProgSize;
}

/* --------------------------------------------------------- */
void AddConstRef(int val, unsigned int location)
{
   unsigned int n = ProgConstRefs.nConsts + 1;
   
   if (ProgConstRefs.nConsts > MaxConsts)
   {
       Error(301, "Too many constants (%d)\n", MaxConsts);
   }
   ProgConstRefs.Const[n] = val;
   ProgConstRefs.Loc[n] = location;
   ProgConstRefs.nConsts = n;
}

/* --------------------------------------------------------- */
void FillinLabels()
{
    unsigned int i;
    for (i=1; i<=ProgLabelRefs.nLabels; i+=1)
    {
        ProgCode[ProgLabelRefs.Loc[i]] += 
          (ProgLabelList[ProgLabelRefs.Label[i]] - ProgLabelRefs.Loc[i] - 2) & 0xFFFFFF;
    }
}

/* --------------------------------------------------------- */
void FillinConsts()
{
    unsigned int i;
    unsigned int p;
    
    for (i=1; i<=ProgConstRefs.nConsts; i+=1)
    {
        p = (ProgSize - ProgConstRefs.Loc[i] - 2) * 4;
        if (p > 4095)
        {
            Error(3001, "FillinConsts: offset %d too large\n", p);
        }
        ProgCode[ProgConstRefs.Loc[i]] += p;
        OutWord(ProgConstRefs.Const[i]);
    }
}

/* --------------------------------------------------------- */

/* ELF constants */

/* Definition of the default string table section ".shstrtab" */

const char defaultStrTable[] = 
{
    /* offset 00 */ '\0',  /* The NULL section */
    /* offset 01 */ '.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', '\0',
    /* offset 11 */ '.', 's', 't', 'r', 't', 'a', 'b', '\0',
    /* offset 19 */ '.', 's', 'y', 'm', 't', 'a', 'b', '\0',
    /* offset 27 */ '.', 'c', 'o', 'm', 'm', 'e', 'n', 't', '\0',
    /* offset 36 */ '.', 'b', 's', 's', '\0',
    /* offset 41 */ '.', 'd', 'a', 't', 'a', '\0',
    /* offset 47 */ '.', 'r', 'e', 'l', '.', 't', 'e', 'x', 't', '\0',
    /* offset 57 */ '.', 't', 'e', 'x', 't', '\0'
};

const char defaultStrTableLen = sizeof(defaultStrTable); /* Length of the "defaultStrTable" string */
const int iSectionsNumber = 9;                           /* The number of sections in the ELF object file */

/* Sections offset */
    const char _shstrtab_offset = 1;
    const char _strtab_offset   = 11;
    const char _symtab_offset   = 19;
    const char _comment_offset  = 27;
    const char _bss_offset      = 36;
    const char _data_offset     = 41;
    const char _rel_text_offset = 47;
    const char _text_offset     = 57;

/* Position of sections within the object file */
    const char _shstrtab        = 1;
    const char _strtab          = 2;
    const char _symtab          = 3;
    const char _text            = 4;
    const char _data            = 5;

/* --------------------------------------------------------- */
void WriteProgram(char filename[])
{
    int          fd;
    Elf          *pElf;
    Elf32_Ehdr   *pEhdr;
    Elf32_Shdr   *pShdr;
    Elf_Scn      *pScn;
    Elf_Data     *pData;

    char         strtab[MaxProcedures * 20];  /* assume 20 characters per name worst case */
    unsigned int strsize;
    unsigned int p;
    unsigned int moffset[MaxProcedures];
    unsigned int i;
    

    /* Create the ELF header */
    if (elf_version(EV_CURRENT) == EV_NONE)   /* It must appear before "elf_begin()" */
    {
        errx(EX_SOFTWARE, "ELF library initialization failed: %s", elf_errmsg(-1));
    }
    if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0777)) < 0)
    {
        errx(EX_OSERR, "open \"%s\" failed", filename);
    }
    if ((pElf = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL)  /* 3rd argument is ignored for "ELF_C_WRITE" */
    {
        errx(EX_SOFTWARE, "elf_begin() failed: %s.", elf_errmsg(-1));
    }

    if ((pEhdr = elf32_newehdr(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_newehdr() failed: %s", elf_errmsg(-1));
    }
    pEhdr->e_ident[EI_CLASS] = ELFCLASS32;  /* Defined by ARM architecture */
    pEhdr->e_ident[EI_DATA] = ELFDATA2LSB;  /* Defined by ARM architecture */
    pEhdr->e_machine = EM_ARM;              /* ARM architecture */
    pEhdr->e_type = ET_REL;                 /* Relocatable file (object file) */
    pEhdr->e_shstrndx = _shstrtab;          /* Point to the shstrtab section */
    pEhdr->e_flags = 0x5000000;             /* !! ST - set EABI version 5 */

    /* Create the section "default section header string table (.shstrtab)" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }

    pData->d_align = 1;
    pData->d_buf = (void *) defaultStrTable;
    pData->d_type = ELF_T_BYTE;
    pData->d_size = defaultStrTableLen;

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _shstrtab_offset;  /* Point to the name of the section */
    pShdr->sh_type = SHT_STRTAB;
    pShdr->sh_flags = 0;
    /* End of section "String table" */

    /* Create the section ".strtab" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }

    strtab[0] = '\0';
    strsize = 1;
    p = 0;

    while (1)
    {
        strtab[strsize] = filename[p];
        if (strtab[strsize] == '\0')
        {
            break;
        }
        strsize += 1;
        p += 1;
    }
    p = 0;
    strsize += 1;
    moffset[0] = strsize;  /* offset of "_mainprog" string */
    while (1)
    {
        strtab[strsize] = "_mainprog"[p];
        if (strtab[strsize] == '\0')
        {
            break;
        }
        strsize += 1;
        p += 1;
    }       

    for (i=1; i<=NumberOfProcedures; i+=1)
    {
        p = 0;
        strsize += 1;
        moffset[i] = strsize;  /* offset of procedure name string */
        while (1)
        {
            strtab[strsize] = Procedures[i].Name[p];
            if (strtab[strsize] == '\0')
            {
                break;
            }
            strsize += 1;
            p += 1;
        }       
    }

    pData->d_align = 1;
    pData->d_buf = (void *) strtab;
    pData->d_type = ELF_T_BYTE;
    pData->d_size = strsize + 1;

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _strtab_offset;
    pShdr->sh_type = SHT_STRTAB;
    pShdr->sh_flags = 0;
    /* End of section ".strtab" */

    /* Create the section ".symtab" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }

    Elf32_Sym x[5 + MaxProcedures];

    /* Definition of the undefined section (this must be the first item by the definition of TIS ELF) */
    x[0].st_name = 0;
    x[0].st_value = 0;
    x[0].st_size = 0;
    x[0].st_info = 0;
    x[0].st_other = 0;
    x[0].st_shndx = SHN_UNDEF;

    /* Definition of the name of the source file (this must be the second item by the definition in TIS ELF) */
    x[1].st_name = 1;
    x[1].st_value = 0;
    x[1].st_size = 0;
    x[1].st_info = ELF32_ST_INFO(STB_LOCAL, STT_FILE); /* This is the value that st_info must have (because of TIS ELF) */
    x[1].st_other = 0;
    x[1].st_shndx = SHN_ABS;  /* The section where the symbol is */

    /* Definition of the ".text" section as a section in the ".symtab" section */
    x[2].st_name = 0;
    x[2].st_value = 0;
    x[2].st_size = 0;
    x[2].st_info = ELF32_ST_INFO(STB_LOCAL, STT_SECTION);
    x[2].st_other = 0;
    x[2].st_shndx = _text;  /* The section where the symbol is */

    /* Definition of the ".data" section as a section in the ".symtab" section */
    x[3].st_name = 0;
    x[3].st_value = 0;
    x[3].st_size = 0;
    x[3].st_info = ELF32_ST_INFO(STB_LOCAL, STT_SECTION);
    x[3].st_other = 0;
    x[3].st_shndx = _data;  /* The section where the symbol is */

    /* Definition of the "_mainprog" symbol */
    x[4].st_name = moffset[0];  /* Offset in the "strtab" section where the name starts */
    x[4].st_value = 0;
    x[4].st_size = 0;
    x[4].st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
    x[4].st_other = 0;
    x[4].st_shndx = _text;  /* The section where the symbol is */

    for (i=1; i<=NumberOfProcedures; i+=1)
    {
        x[i+4].st_name = moffset[i];  /* Offset in the "strtab" section where the name starts */
        x[i+4].st_value = Procedures[i].Offset;
        x[i+4].st_size = 0;
        x[i+4].st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
        x[i+4].st_other = 0;
        x[i+4].st_shndx = _text;  /* The section where the symbol is */
    }
    
    pData->d_align = 4;
    pData->d_buf = (void *) x;
    pData->d_type = ELF_T_BYTE;
    pData->d_size = sizeof(Elf32_Sym) * (NumberOfProcedures + 5);

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _symtab_offset;  /* Point to the name of the section */
    pShdr->sh_type = SHT_SYMTAB;
    pShdr->sh_flags = 0;
    pShdr->sh_link = _strtab;  /* point to the section .strtab (the section that contain the strings) */
    pShdr->sh_info = ELF32_ST_INFO(STB_LOCAL, 4);  /* the 2nd arg is because of TIS ELF (One greater than the symbol table index of the last local symbol (binding STB_LOCAL)) */
    /* End of section ".symtab" */

    /* Create the section ".text" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }

    pData->d_align = 4;
    pData->d_off = 0;
    pData->d_buf = (void *) ProgCode;
    pData->d_type = ELF_T_BYTE;
    pData->d_size = ProgSize * 4;

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _text_offset;
    pShdr->sh_type = SHT_PROGBITS;
    pShdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    /* End of section ".text" */

    /* Create the section ".data" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    pData->d_align = 4;

    pData->d_buf = (void *) NULL;
    pData->d_type = ELF_T_BYTE;
    pData->d_size = 0;
    pData->d_version = EV_CURRENT;

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _data_offset;
    pShdr->sh_type = SHT_PROGBITS;
    pShdr->sh_flags = SHF_ALLOC | SHF_WRITE;
    /* End of section ".data" */

    /* Create the section ".rel.text" */
    if ((pScn = elf_newscn(pElf)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }
    if ((pData = elf_newdata(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf_newdata() failed: %s.", elf_errmsg(-1));
    }

    pData->d_align = 4;

    pData->d_buf = (void *) NULL;
    pData->d_type = 0; /* The type of each relocation is specified in the relocation itself */
    pData->d_size = 0;

    if ((pShdr = elf32_getshdr(pScn)) == NULL)
    {
        errx(EX_SOFTWARE, "elf32_etshdr() failed: %s.", elf_errmsg(-1));
    }

    pShdr->sh_name = _rel_text_offset;
    pShdr->sh_type = SHT_REL;
    pShdr->sh_flags = 0;
    pShdr->sh_link = _symtab;
    pShdr->sh_info = _text;
    /* End of section ".rel.text" */

    /* Update the sections internally */
    if (elf_update(pElf, ELF_C_NULL) < 0)
    {
        errx(EX_SOFTWARE, "elf_update(NULL) failed: %s.", elf_errmsg(-1));
    }
    /* Write the object file */
    if (elf_update(pElf, ELF_C_WRITE) < 0)
    {
        errx(EX_SOFTWARE, "elf_update() failed: %s.", elf_errmsg(-1));
    }
    /* Close all handles */
    elf_end(pElf);
    close(fd);
}

/* --------------------------------------------------------- */
void OutWord(unsigned int w)
{
    ProgCode[ProgSize] = w;
    ProgSize += 1;
    if (ProgSize >= CodeSize)
    {
        Error(302, "Program too large (%d)\n", ProgSize);
    }
}

/* --------------------------------------------------------- */
unsigned int op2size(unsigned int n)
{
    unsigned int s = 1;
    
    while (1)
    {
        while (((n & 3) == 0) && ((n & 0xFFFFFF00) != 0))
            n = n >> 2;
        
        if ((n & 0xFFFFFF00) == 0)
            return s;
        else
        {
            s += 1;
            n = n >> 8;
        }
    }
}

/* --------------------------------------------------------- */
void GenLoadConstant(unsigned int r, int x)
{
    unsigned int op1, op2;
    unsigned int r1  = 0;
    unsigned int n = (unsigned int) x;
    int          s1 = op2size(n);
    int          s2 = op2size(~n);
    unsigned int sh = 0;
    
    if (s1 <= s2)
    {
        op1 = i_MOV;
        op2 = i_ORR;
    }
    else
    {
        n = ~n;
        op1 = i_MVN;
        op2 = i_BIC;
    }
        
    while (1)
    {
        while (((n & 3) == 0) && ((n & 0xFFFFFF00) != 0))
        {
            n = n >> 2;
            sh += 1;
        }
        GenRRn(op1, r1, r, (16 - sh) % 16, n & 0xFF);
        if ((n & 0xFFFFFF00) == 0)
            return;
        n = n >> 8;
        sh = sh + 4;
        op1 = op2;  /* for ORR and BIC use r in dest field */
        r1 = r;
    }
}

/* --------------------------------------------------------- */
void GenAddConstant(unsigned int Rn, unsigned int Rd, int c)
{
    unsigned int op;
    unsigned int n;
    unsigned int sh = 0;
    
    if (c >= 0)
    {
        n = c;
        op = i_ADD;
    }
    else
    {
        n = -c;
        op = i_SUB;
    }
    
    while (1)
    {
        while (((n & 3) == 0) && ((n & 0xFFFFFF00) != 0))
        {
            n = n >> 2;
            sh += 1;
        }
        
        GenRRn(op, Rn, Rd, (16 - sh) % 16, n & 0xFF);
        if ((n & 0xFFFFFF00) == 0)
            return;
        n = n >> 8;
        sh += 4;
        Rn = Rd;
    }
}

/* --------------------------------------------------------- */
void AddLabelRef(unsigned int label, unsigned int location)
{
    unsigned int n = ProgLabelRefs.nLabels + 1;
    
    if (ProgLabelRefs.nLabels >= MaxLabels)
    {
        Error(303, "Too many labels (%d)\n", MaxLabels);
    }
    ProgLabelRefs.Loc[n] = location;
    ProgLabelRefs.Label[n] = label;
    ProgLabelRefs.nLabels = n;
}

/* --------------------------------------------------------- */
void GenBranch(unsigned int cond, unsigned int lab)
{
    if (incode)
    {
        AddLabelRef(lab, ProgSize);
        OutWord((cond << 28) | (0xA << 24));
        if (cond == b_BR)
        {
            incode = false;
        }
    } 
}

/* --------------------------------------------------------- */
void GenBranchWithLink(unsigned int lab)
{
    AddLabelRef(lab, ProgSize);
    OutWord((14 << 28) | (0xB << 24)); 
}

/* --------------------------------------------------------- */
void GenSysCall(unsigned int n, unsigned int r)
{
    switch (n)
    {
        case 18:  /* putbyte */
            OutWord(0xe7c12000);    /* strb r2,[r0,r1] */
            break;

        case 19:  /* putword */
            OutWord(0xe7802101);    /* str r2,[r0,r1, lsl #2] */
            break;
 
        case 36:  /* getbyte */
            OutWord(0xe7d10000);    /* ldrb r0,[r1,r0] */
            break;

        case 37:  /* getword */
            OutWord(0xe7900101);    /* mov r1,r1,asl #2 */
            break;

        default:
            GenLDR(r, r_g, n * 4);                       /* load global n */
            OutWord((14 << 28) | (0x12fff3 << 4) | r);   /* BLX r */ 
            break;
    }
}

/* --------------------------------------------------------- */
void GenRRR(unsigned int Op, unsigned int Rn, unsigned int Rd, unsigned int Rm)  /* rd = rn OP rm */
{
    if (incode)
    {
        OutWord((14 << 28) | (Op << 21) | (Rn << 16) | (Rd << 12) | Rm);
    }
}

/* --------------------------------------------------------- */
void GenRRn(unsigned int Op, unsigned int Rn, unsigned int Rd, unsigned int sh, unsigned int n)
{
    if (incode)
    {
        OutWord((14 << 28) | (1 << 25) | (Op << 21) | (Rn << 16) | (Rd << 12) | (sh << 8) | n);
    }
}

/* --------------------------------------------------------- */
void GenRRRcond(unsigned int Op, unsigned int cond, unsigned int Rn, unsigned int Rd, unsigned int Rm)  /* rd = rn OP rm */
{
    if (incode)
    {
        OutWord((cond << 28) | (Op << 21) | (Rn << 16) | (Rd << 12) | Rm);
    }
}

/* --------------------------------------------------------- */
void GenRRncond(unsigned int Op, unsigned int cond, unsigned int Rn, unsigned int Rd, unsigned int sh, unsigned int n)
{
    if (incode)
    {
        OutWord((cond << 28) | (1 << 25) | (Op << 21) | (Rn << 16) | (Rd << 12) | (sh << 8) | n);
    }
}

/* --------------------------------------------------------- */
void GenMult(unsigned int Rd, unsigned int Rs, unsigned int Rm)  /* rd = rs * rm */
{
    if (incode)
    {
        OutWord((14 << 28) | (Rd << 16) | (Rs << 8) | (9 << 4) | Rm);
    }
}

/* --------------------------------------------------------- */
void GenLDR(unsigned int r, unsigned int rx, int n)
{
    if (incode)
    {
        OutWord((14 << 28) | (0x59 << 20) | (rx << 16) | (r << 12) | n);
    }
}

/* --------------------------------------------------------- */
void GenLDRR(unsigned int r, unsigned int s)
{
    if (incode)
    {
        OutWord((14 << 28) | (0x79 << 20) | (r << 16) | (r << 12) | s);
    }
}

/* --------------------------------------------------------- */
void GenSTR(unsigned int r, unsigned int rx, int n)
{
    if (incode)
    {
        OutWord((14 << 28) | (0x58 << 20) | (rx << 16) | (r << 12) | n);
    }
}

/* --------------------------------------------------------- */
void GenSTRR(unsigned int Rn, unsigned int Rd, unsigned int Rm)
{
    if (incode)
    {
        OutWord((14 << 28) | (0x78 << 20) | (Rn << 16) | (Rd << 12) | Rm);
    }
}

/* --------------------------------------------------------- */
void initstack(unsigned int n)
{
    arg2 = 0;
    arg1 = 1;
    ssp = n;
    pendingop = s_NONE;
    sstack[arg2].type = k_loc;
    sstack[arg2].val = ssp - 2;
    sstack[arg2].pos = ssp - 2;
    sstack[arg1].type = k_loc;
    sstack[arg1].val = ssp - 1;
    sstack[arg1].pos = ssp - 1;
    if (ssp >= maxssp)
    {
        maxssp = ssp;
    }
}

/* --------------------------------------------------------- */
void stack(unsigned int n)
{
    if (n >= ssp + 4)
    {
        store(0, ssp - 1);
        initstack(n);
        return;
    }
    while (n > ssp)
    {
        loadt(k_loc, ssp);
    }
    
    while (n != ssp)
    {
        if (arg2 == 0)
        {
            if (n == ssp - 1)
            {
                ssp = n;
                sstack[arg1].type = sstack[arg2].type;
                sstack[arg1].val  = sstack[arg2].val;
                sstack[arg1].pos  = ssp - 1;
                sstack[arg2].type = k_loc;
                sstack[arg2].val  = ssp - 2;
                sstack[arg2].pos  = ssp - 2;
            }
            else
            {
                initstack(n);
            }
            return;
        }
        arg1 -= 1;
        arg2 -= 1;
        ssp -= 1;
    }
}

/* --------------------------------------------------------- */
void store(unsigned int a, unsigned int b)
{
    unsigned int p;
    unsigned int s;
    
    for (p=0; p<=arg1; p+=1)
    {
        s = sstack[p].pos;
        if (s > b)
        {
            break;
        }
        if (s >= a && sstack[p].type >= k_reg)
        {
            storet(p);
        }
    }
    for (p=0; p<=arg1; p+=1)
    {
        s = sstack[p].pos;
        if (s > b)
        {
            return;
        }
        if (s >= a)
        {
            storet(p);
        }
    }
}

/* --------------------------------------------------------- */
int regusedby(unsigned int a)
{
    unsigned int k = sstack[a].type;
    
    if (k != k_reg)
    {
        return -1;
    }
    return sstack[a].val;
}

/* --------------------------------------------------------- */
bool isfree(unsigned int r)
{
    unsigned int t;
    
    for (t=0; t<=arg1; t+=1)
    {
        if (regusedby(t) == r)
        {
            return false;
        }
    }
    return true;
}

/* --------------------------------------------------------- */
void freereg(unsigned int r)
{
    unsigned int t;
    
    for (t=0; t<=arg1; t+=1)
    {
        if (regusedby(t) == r)
        {
            storet(t);
            break;
        }
    }
}

/* --------------------------------------------------------- */
unsigned int nextfree()
{
    unsigned int r;
    
    r = choosereg(~(regswithinfo() | regsinuse()));
    return r;
}

/* --------------------------------------------------------- */
void storet(unsigned int a)
{
    unsigned int k = sstack[a].type;
    int          n = sstack[a].val;
    unsigned int s = sstack[a].pos;
    unsigned int r;
    
    if (!(k == k_loc && s == n))
    {
        r = movetoanyr(a);
        GenSTR(r, r_p, 4 * s);
        sstack[a].type = k_loc;
        sstack[a].val = s;
        remem(r, k_loc, n);
    }
}

/* --------------------------------------------------------- */
void loadt(unsigned int k, int n)
{
    cgpendingop();
    if (ssp >= Maxsstack - 3)
    {
        Error(304, "Simulated stack overflow (%d)\n", ssp);
    }
    else
    {
        arg1 += 1;
        arg2 += 1;
        sstack[arg1].type = k;
        sstack[arg1].val = n;
        sstack[arg1].pos = ssp;
        ssp += 1;
        if (maxssp < ssp)
        {
            maxssp = ssp;
        }
    }
}

/* --------------------------------------------------------- */
void lose1(unsigned int k, int n)
{
    ssp -= 1;
    if (arg2 == 0)
    {
        sstack[arg2].type = k_loc;
        sstack[arg2].val  = ssp - 2;
        sstack[arg2].pos  = ssp - 2;
    }
    else
    {
        arg1 -= 1;
        arg2 -= 1;
    }
    sstack[arg1].type = k;
    sstack[arg1].val = n;
    sstack[arg1].pos = ssp - 1;
}

 /* --------------------------------------------------------- */
unsigned int choosereg(unsigned int regs)
{
    unsigned int r;
    
    if (cgdebug > 5)
    {
        printf("CHOOSEREG(%4x)\n", regs);
    }
    for (r=r_r0; r<=r_r9; r+=1)
    {
        if (((regs >> r) & 1) == 1)
        {
            return r;
        }
    }
    return r_r0;
}
    
/* --------------------------------------------------------- */
void initslave()
{
    unsigned int r;
    
    for (r=r_r0; r<=r_r9; r+=1)
    {
        slave[r] = NULL;
    }
}

/* --------------------------------------------------------- */
void forgetr(unsigned int r)
{
    struct lnode *a;

    a = (struct lnode *) &slave[r];
    while (a->next != NULL)
    {
        a = a->next;
    }
    a->next = freelist;
    freelist = slave[r];
    slave[r] = NULL;
}

/* --------------------------------------------------------- */
void forgetall()
{
    unsigned int r;
    
    for (r=r_r0; r<=r_r9; r+=1)
    {
        forgetr(r);
    }
}

/* --------------------------------------------------------- */
void remem(unsigned int r, unsigned int k, int n)
{
    if (k < k_reg)
    {
        slave[r] = getblk(slave[r], k, n);
    }
}

/* --------------------------------------------------------- */
void moveinfo(unsigned int s, unsigned int r)
{
    struct lnode *p;
    
    p = slave[s];
    forgetr(r);
    while (p != NULL)
    {
        remem(r, p->type, p->val);
        p = p->next;
    } 
}

/* --------------------------------------------------------- */
void forgetvar(unsigned int k, int n)
{
    struct lnode *a;
    struct lnode *p;
    unsigned int r;
    
    for (r=r_r0; r<= r_r9; r+=1)
    {
        
        a = (struct lnode *) &slave[r];
        while (1)
        {
            p = a->next;
            if (p == NULL)
            {
                break;
            }
            if (p->type == k && p->val == n)
            {
                a->next = p->next;
                freeblk(p);
            }
            else
            {
                a = p;
            }
        }
    }
}

/* --------------------------------------------------------- */
void forgetvars()
{
    struct lnode *a;
    struct lnode *p;
    unsigned int r;
    
    for (r=r_r0; r<= r_r9; r+=1)
    {
        
        a = (struct lnode *) &slave[r];
        while (1)
        {
            p = a->next;
            if (p == NULL)
            {
                break;
            }
            if (p->type < k_numb)
            {
                a->next = p->next;
                freeblk(p);
            }
            else
            {
                a = p;
            }
        }
    }
}

/* --------------------------------------------------------- */
unsigned int regscontaining(unsigned int k, int n)
{
    unsigned int regset = 0;
    unsigned int r;
    
    if (k == k_reg)
    {
        return 1 << n;
    }
    
    for (r=r_r0; r<=r_r9; r+=1)
    {
        if (isinslave(r, k, n))
        {
            regset = regset | (1 << r);
        }
    }
    return regset;
}

/* --------------------------------------------------------- */
bool isinslave(unsigned int r, unsigned int k, int n)
{
    struct lnode *p;
    
    p = slave[r];
    
    while (p != NULL)
    {
        if (p->type == k && p->val == n)
        {
            return true;
        }
        p = p->next;
    }
    return false;
}

/* --------------------------------------------------------- */
unsigned int regsinuse()
{
    unsigned int regset = 0;
    unsigned int t;
    
    for (t=0; t<=arg1; t+=1)
    {
        if (sstack[t].type == k_reg)
        {
            regset = regset | (1 << sstack[t].val);
        }
    }
    return regset;
} 
 
/* --------------------------------------------------------- */
unsigned int regswithinfo()
{
    unsigned int regset = 0;
    unsigned int r;
    
    for (r=r_r0; r<=r_r9; r+=1)
    {
        if (slave[r] != NULL)
        {
            regset = regset | (1 << r);
        }
    }
    return regset;
}

/* --------------------------------------------------------- */
struct lnode *getblk(struct lnode *a, unsigned int b, int c)
{
    struct lnode *p = freelist;
    
    if (p == NULL)
    {
        dp += 1;
        p = &dplist[dp];
    }
    else
    {
        freelist = p->next;
    }
    p->next = a;
    p->type = b;
    p->val = c;
    return p;
}

/* --------------------------------------------------------- */
void freeblk(struct lnode *p)
{
    p->next = freelist;
    freelist = p;
}

/* --------------------------------------------------------- */
void swapargs()
{
    unsigned int k  = sstack[arg1].type;
    int          n  = sstack[arg1].val;
    
    sstack[arg1].type = sstack[arg2].type;
    sstack[arg1].val  = sstack[arg2].val;
    sstack[arg2].type = k;
    sstack[arg2].val  = n;
}

/* --------------------------------------------------------- */
unsigned int movetoanyr(unsigned int a)
{
    unsigned int k;
    int          n;
    unsigned int usedregs;
    unsigned int poss;
    unsigned int t;
    
    while (1)
    {
        usedregs = regsinuse();
        k = sstack[a].type;
        n = sstack[a].val;
        
        if (k == k_reg)  /* already on the stack? */
        {
            return n;
        }
    
        poss = regscontaining(k, n);  /* in the slave ? */
        if (poss != 0)
        {
            return choosereg(poss);

        }
    
        poss = ~(usedregs | regswithinfo());
        if (poss != 0)
        {
            return movetor(a, choosereg(poss));
        }

        poss = ~usedregs;
        if (poss != 0)
        {
            return movetor(a, choosereg(poss));
        }
        
        /* all regs in use - so free the oldest */
        for (t=0; t<= arg1; t+=1)
        {
            if (regusedby(t) >= 0)
            {
                storet(t);
                break;
            }
        }
    }
}

/* --------------------------------------------------------- */
unsigned int movetor(unsigned int a, unsigned int r)
{
    unsigned int k;
    int          n;
    unsigned int s;
    int          poss;
    int          p;
    
    k = sstack[a].type;
    n = sstack[a].val;
    
    if (k == k_reg && n == r)
    {
        return r;
    }
    
    if (regusedby(a) != r)
    {
        freereg(r);
        k = sstack[a].type;
        n = sstack[a].val;
    }
    
    poss = regscontaining(k, n);
    
    if (poss != 0)
    {
        s = choosereg(poss);
        if (r != s)
        {
            GenRRR(i_MOV, 0, r, s);
            moveinfo(s, r);
        }
        goto ret;
    }
    
    switch (k)
    {
        case k_loc:
            if (n > 1023)
            {
                Error(3002, "k_loc offset too large %d\n", n * 4);
            }
            GenLDR(r, r_p, n * 4);
            break;
        case k_glob:
            if (n > 1023)
            {
                Error(3003, "k_glob offset too large %d\n", n * 4);
            }
            GenLDR(r, r_g, n * 4);
            break;
        case k_numb:
            GenLoadConstant(r, n);
            break;
        case k_lab:
            /* *** to be written */
            break;
        case k_lvloc:
            if (n > 1023)
            {
                Error(3004, "k_lvloc offset too large %d\n", n * 4);
            }
            GenAddConstant(r_p, r, n * 4);
            break;
        case k_lvglob:
            if (n > 1023)
            {
                Error(3005, "k_lvglob offset too large %d\n", n * 4);
            }
            GenAddConstant(r_g, r, n * 4);
            break;
        case k_lvlab:
            /* only applies to strings - the label will be forwards (i.e. +ve offset from PC*/
            p = (ProgLabelList[n] - ProgSize - 2) * 4;
printf("k_lvlab: n=%d lab=%d p=%d Progsize=%d\n", n, ProgLabelList[n], p, ProgSize); 
            if (op2size(p) == 1)
            {
                GenAddConstant(r_pc, r, p);
            }
            else
            {
                GenRRR(i_MOV, 0, r, r_pc);
                GenAddConstant(r, r, p);
            }
            break;                  
    }

ret:
    forgetr(r);
    remem(r, k, n);
    sstack[a].type = k_reg;
    sstack[a].val = r;
    return r;
}

/* --------------------------------------------------------- */
unsigned int powerof2(int x)  /* return shift value 2,3,4,5... for 4,8,16,32... */
{
    unsigned int p;
    unsigned int q;
    
    x = abs(x);
    q = 4;
    for (p=2; p<31; p+=1)
    {
        if (q == x)
        {
            return p;
        }
        q = q + q;
    }
    return 0;
}

/* --------------------------------------------------------- */
void OpenOutputFile(char FileName[])
{
    OutputFilestream = fopen(FileName, "w+b");
    if (OutputFilestream == NULL) 
    {
        return;
    }
}

/* --------------------------------------------------------- */
void CloseOutputFile()
{
  fclose(OutputFilestream);
}

/* --------------------------------------------------------- */
void OutputWrch(char Ch)
{
    fputc(Ch, OutputFilestream);
}

/* --------------------------------------------------------- */
unsigned int cgNextLabel()
{
    NumberOfLabels += 1;
    if (NumberOfLabels >= MaxLabels)
    {
        Error(305, "Too many labels (%d)", MaxLabels);
    }
    return NumberOfLabels;
}

/* --------------------------------------------------------- */
void bswitch(unsigned int p, unsigned int q, unsigned int d,
             int casek[], unsigned int casel[])
{
    unsigned int m;
    unsigned int t;
    unsigned int i;
    unsigned int r;
    
    if ((q - p) > 6)
    {
        m = cgNextLabel();
        t = (p + q) / 2;
        loadt(k_numb, casek[t]);
        GenBranch(cgcmp(b_GE), m);
        r = movetoanyr(arg1);  /* find which reg was used and forget it */
        forgetr(r);
        stack(ssp-1);
        bswitch(p, t-1, d, casek, casel);
        GenBranch(b_BR, d);
        setlab(m);
        forgetall();
        incode = true;
        GenBranch(b_EQ, casel[t]);
        bswitch(t+1, q, d, casek, casel); 
    }
    else
    {
        for (i=p; i<=q; i+=1)
        {
            loadt(k_numb, casek[i]);
            GenBranch(cgcmp(b_EQ), casel[i]);
            r = movetoanyr(arg1);  /* find which reg was used and forget it */
            forgetr(r);
            stack(ssp-1);
        }
    }
}

/* --------------------------------------------------------- */
void storein(unsigned int k, int n)
{
    unsigned int r;
    
    cgpendingop();
    
    r = movetoanyr(arg1);
    switch (k)
    {
        default: 
            Error(3006, "storein: unrecognised type (%d)\n", k);
            break;
        case k_loc: 
            GenSTR(r, r_p, n * 4);
            break;
        case k_glob:
            GenSTR(r, r_g, n * 4);
            break;
        //case k_lab: 
        //    s = choosereg(~reginuse());
        //    genrl(mf_la,s,n);
        //    genrnr(mf_sw,r,0,s);
        //    break;
    }
    forgetvar(k, n);
    remem(r, k, n);
    stack(ssp-1);
}

/* --------------------------------------------------------- */
unsigned int rdop()
{
    ProgCounter += 1;
    if (ProgCounter > ProgramSize)
    {
        return s_EOF;
    }
    else
    {
        return Instructions[ProgCounter].Op;
    }
}

/* --------------------------------------------------------- */
int rdn()
{
    return Instructions[ProgCounter].Arg;
}

/* --------------------------------------------------------- */
void dboutput(unsigned int lev)
{
    unsigned int r;
    unsigned int t;
    struct lnode *p;
    
    if (lev > 2)
    {
        printf("\nSLAVE: ");
        for (r=r_r0; r<= r_r9; r+=1)
        {
            p = slave[r];
            if (p == NULL) 
            {
                continue;
            }
            printf(" R%d=", r);
            while (p != NULL)
            {
                wrkn(p->type, p->val);
                p = p->next;
            }
        }
    }
 
    if (lev > 1)
    {
        printf("\nSTACK: ");
        for (t=0; t<=arg1; t+=1)
        {
            wrkn(sstack[t].type, sstack[t].val);
        }
    }
 
    printf("\nIPC=%d OP=%d/%d SSP=%d ARG1=%d ARG2=%d PC=%x\n", 
           ProgCounter, Op, pendingop, ssp, arg1, arg2, ProgSize * 4);
}
 
/* --------------------------------------------------------- */
void wrkn(unsigned int k, int n)
{
    char *s;
    
    switch (k)
    {
        default:          s = "?";
                          break;
        case k_numb:      s = "N%d";
                          break;
        case k_loc:       s = "P%d";
                          break;
        case k_glob:      s = "G%d";
                          break;
        case k_lab:       s = "L%d";
                          break;
        case k_lvloc:     s = "@P%d";
                          break;
        case k_lvglob:    s = "@G%d";
                          break;
        case k_lvlab:     s = "@L%d";
                          break;
        case k_reg:       s = "R%d";
                          break;
    }
    printf(s, n);
    printf(" ");
}

/* --------------------------------------------------------- */
unsigned int CheckLab()
{
    unsigned int i;
    unsigned int lab = 0;
        
    for (i=1; i<=NumberOfLabels; i+=1)
    {
        if (ProgCounter == Labels[i])
        {
            lab = i;
            break;
        }
    }
    return lab;
}

/* --------------------------------------------------------- */

void cg()
{
    unsigned int i;
    unsigned int p;
    unsigned int l;
    unsigned int m;
    unsigned int nextop;
    unsigned int r;
    
    ProgCounter = 0;
    ProgSize = 0;
    ProgLabelRefs.nLabels = 0;
    ProgConstRefs.nConsts = 0;
    incode = true;

    GenBranch(b_BR, 0);     /* add JMP main at start */
    
    initslave();
    initstack(3);
    cgdebug = 0;  /* was 10 - 0 to switch off */
    Op = rdop();
    
    while (1)
    {
        if (cgdebug > 6)
        {
            dboutput(3);
        }
        
        switch (Op)
        {
            case s_EOF:
                return;
                
            case s_LP:
                loadt(k_loc, rdn() + 2);
                break;
            
            case s_LG:
                loadt(k_glob, rdn());
                break;

            case s_LL:
                loadt(k_lab, rdn());
                break;

            case s_LN:
                loadt(k_numb, rdn());
                break;

            case s_LLP:
                loadt(k_lvloc, rdn() + 2);
                break;

            case s_LSTR:
            case s_LLG:
                loadt(k_lvglob, rdn());
                break;

            case s_LLL:
                loadt(k_lvlab, rdn());
                break;

            case s_SP:
                storein(k_loc, rdn() + 2);
                break;

            case s_SG:
                storein(k_glob, rdn());
                break;
            
            case s_SL:
                storein(k_lab, rdn());
                break;

            case s_STIND:
                cgstind();
                break;

            case s_RV:
                cgrv();
                break;

            case s_MULT: case s_DIV: case s_REM:
            case s_MULTF: case s_DIVF:
            case s_PLUS: case s_MINUS:
            case s_EQ: case s_NE:
            case s_LS: case s_GR: case s_LE: case s_GE:
            case s_LSHIFT: case s_RSHIFT:
            case s_LOGAND: case s_LOGOR: case s_NEQV:
            case s_OR: case s_AND:
            case s_NOT: case s_NEG: case s_ABS: case s_COMP:
            case s_FLOAT: case s_INT:
                cgpendingop();
                pendingop = Op;
                break;
                
            case s_JT:
            case s_JF:
                l = rdn();
                nextop = rdop();
                if (nextop == s_JUMP)
                {
                    cgcondjump(Op == s_JF, rdn());
                    goto jump;
                }
                cgcondjump(Op == s_JT, l);
                Op = nextop;
                continue;
                
            case s_RES:
                cgpendingop();
                store(0, ssp-2);
                movetor(arg1, r_r0);
                stack(ssp-1);
                
            case s_JUMP:
                cgpendingop();
                store(0, ssp-1);
                l = rdn();
            
            jump:
                while (1)
                {
                    Op = rdop();
                    if (Op != s_STACK)
                    {
                        break;
                    }
                    stack(rdn());
                }
                
                if (Op != s_LAB)
                {
                    GenBranch(b_BR, l);
                    incode = false;
                    continue;
                }
                m = rdn();
                if (l != m)
                {
                    GenBranch(b_BR, l);
                    incode = false;
                }
                goto lab;
                
            case s_LAB:
                cgpendingop();
                store(0, ssp - 1);
                m = rdn();
            lab:
                setlab(m);
                lastlab = m;
                incode = true;
                forgetall();
                break;
                    
            case s_RSTACK:
                initstack(rdn());
                loadt(k_reg, r_r0);
                break;
            
            case s_SWITCHON:
            {
                unsigned int n;
                unsigned int d;
                int          a;
                unsigned int l;
                unsigned int j;
                int          casek[MaxSwitches];
                unsigned int casel[MaxSwitches];
                
                n = rdn();
                rdop();
                d = rdn();
                
                for (i=1; i<=n; i+=1)
                {
                    rdop();
                    a = rdn();
                    rdop();
                    l = rdn();
                    j = i - 1;
                    while (j != 0)
                    {
                        if (a > casek[j])
                        {
                            break;
                        }
                        casek[j+1] = casek[j];
                        casel[j+1] = casel[j];
                        j -= 1;
                    }
                    casek[j+1] = a;
                    casel[j+1] = l;
                }
                
                cgpendingop();
                store(0, ssp-2);
                movetor(arg1, r_r0);
                bswitch(1, n, d, casek, casel);
                GenBranch(b_BR, d);
                stack(ssp - 1);
                break;
            }

            case s_RTRN:
                cgreturn(rdn());
                incode = false;
                break;

            case s_ENTRY:
                p = rdn();
                forgetall();
                incode = true;
                cgname(Procedures[p].Name);
                setlab(lastlab);  /* reset entry label *after* the entry name */
                Procedures[p].Offset = ProgSize * 4;  /* needed for ELF generator */
                break;
                
            case s_SAVE:
                cgsave(rdn());
                break;
                
            case s_FNAP:
            case s_RTAP:
                cgapply(Op, rdn(), false);
                break;

            case s_SYSCALL:
                p = sstack[arg1].val;  /* remove syscall no. from stack */
                stack(ssp-1);
                if (p < 100)
                {
                    sstack[arg1].val = p + 10;        /* 11-29 */
                    cgapply(s_RTAP, rdn(), true);
                }
                else
                {
                    sstack[arg1].val = p - 100 + 30;  /* 31-49 */
                    cgapply(s_FNAP, rdn(), true);
                }
                break;
               
            case s_PUSHTOS:
                cgpendingop();   /* only used for += etc operators */
                r = movetoanyr(arg1);
                GenRRR(i_MOV, 0, r_lr, r);  /* use link reg to remember addr */
                sstack[arg1].val = r_lr;
                loadt(k_reg, r);
                break;

            case s_VCOPY:
                loadt(k_numb, rdn());
                movetor(arg1, r_r2);
                stack(ssp-1);
                movetor(arg1, r_r1);
                stack(ssp-1);
                movetor(arg1, r_r0);
                stack(ssp-1);
                for (r=r_r3; r<=r_r9; r+=1)
                {
                    freereg(r);
                }
                GenSysCall(4, r_r3);
                forgetall();
                break;

            case s_SWAP:
                cgpendingop();
                swapargs();
                break;

            case s_STACK:
                cgpendingop();
                stack(rdn());
                break;

            case s_QUERY:
                cgpendingop();
                stack(ssp + 1);
                break;

            case s_STORE:
                cgpendingop();
                store(0, ssp-1);
                break;
            
            case s_DISCARD:  /* ignored by compiler */
                break;

            default:
                Error(3007, "Unknown icode (%d)\n", Op);
                break;
        }
        Op = rdop();
    }
}

/* --------------------------------------------------------- */
void cgcondjump(bool b, unsigned int l)
{
    int bfn = condbfn(pendingop);
    
    if (bfn < 0)
    {
        cgpendingop();
        loadt(k_numb, 0);
        bfn = b_NE;
    }
    pendingop = s_NONE;
    store(0, ssp-3);
    if (!b)
    {
        bfn = compbfn(bfn);
    }
    bfn = cgcmp(bfn);
    if (bfn != b_NO)
    {
        GenBranch(bfn, l);
    }
    stack(ssp-2);    
}

/* --------------------------------------------------------- */
int condbfn(unsigned int op)
{
    switch (op)
    {
        case s_EQ:
            return b_EQ;
        case s_NE:
            return b_NE;
        case s_LS:
            return b_LS;
        case s_GR:
            return b_GR;
        case s_LE:
            return b_LE;
        case s_GE:
            return b_GE;
        default:
            return -1;
    }
}

/* --------------------------------------------------------- */
unsigned int compbfn(unsigned int f)
{
    switch (f)
    {
        case b_EQ:
            return b_NE;
            break;
        case b_NE:
            return b_EQ;
            break;
        case b_LS:
            return b_GE;
            break;
        case b_GR:
            return b_LE;
            break;
        case b_LE:
            return b_GR;
            break;
        case b_GE:
            return b_LS;
            break;
        default:
            return f;
    }
}

/* --------------------------------------------------------- */
void cgpendingop()
{
    unsigned int f = 0;
    unsigned int r = 0;
    unsigned int pndop = pendingop;
    
    pendingop = s_NONE;
    
    switch (pndop)
    {
        default:
            Error(3008, "Unrecognised pendingop (%d)\n", pndop);
            
        case s_NONE:
            return;
            
        case s_EQ:
        case s_NE:
        case s_LS:
        case s_GR:
        case s_LE:
        case s_GE:
            f = cgcmp(condbfn(pndop));  /* was f = cgcmp(compbfn(condbfn(pndop))); */
            r = nextfree();
            GenRRncond(i_MOV, f, 0, r, 0, 1);
            GenRRncond(i_MOV, compbfn(f), 0, r, 0, 0);
            forgetr(r);
            lose1(k_reg, r);
            return;

        case s_OR:
            cgdyadic(i_ORR, true);
            break;

        case s_AND:
            cgdyadic(i_AND, true);
            break;

        case s_PLUS:
            cgdyadic(i_ADD, true);
            break;

        case s_MINUS:
            cgdyadic(i_SUB, false);
            break;

        case s_MULT:
            cgdyadic(i_MULT, true);
            break;

        case s_DIV:
        case s_REM:
        case s_MULTF:
        case s_DIVF:
                                  f = 1;  /* DIV/REM = syscall 1 */
            if (pndop == s_MULTF) f = 2;  /* MULTF   = syscall 2 */
            if (pndop == s_DIVF)  f = 3;  /* DIVF    = syscall 3 */
            if (pndop == s_DIV || pndop == s_REM)
            {
                movetor(arg2, r_r1);
                movetor(arg1, r_r0);
            }
            else
            {
                movetor(arg2, r_r0);
                movetor(arg1, r_r1);
            }
            for (r=r_r2; r<=r_r9; r+=1)
            {
                freereg(r);
            }
            GenSysCall(f, r_r2);
            forgetall();
            if (pndop == s_REM)
            {
                lose1(k_reg, r_r1);
            }
            else
            {
                lose1(k_reg, r_r0);
            }
            break;

        case s_NEG:
            cgmonadic(i_NEG);
            return;

        case s_ABS:
            cgmonadic(i_ABS);
            return;

        case s_NOT:
            cgmonadic(i_NOT);
            return;

        case s_COMP:
            cgmonadic(i_COMP);
            return;
            
        case s_FLOAT:
            cgmonadic(i_FLOAT);
            return;

        case s_INT:
            cgmonadic(i_INT);
            return;

        case s_LOGAND:
            cgdyadic(i_AND, true);
            break;

        case s_LOGOR:
            cgdyadic(i_ORR, true);
            break;

        case s_NEQV:
            cgdyadic(i_EOR, true);
            break;

        case s_LSHIFT:
            cgdyadic(i_LSHIFT, false);
            break;

        case s_RSHIFT:
            cgdyadic(i_RSHIFT, false);
            break;
    }
}

/* --------------------------------------------------------- */
bool cgdyadic(unsigned int Op, bool swapable)
{
    unsigned int r;
    unsigned int s = 0;
    unsigned int t;
    unsigned int k1, k2;
    int          n1, n2;
    int          n = 0;
    unsigned int sh;
    bool         swapped = false;

    if (swapable && (sstack[arg2].type == k_numb))
    {
        swapargs();
        swapped = true;
    }

    if (Op == i_SUB && (sstack[arg2].type == k_numb))
    {
        swapargs();
        swapped = true;
        Op = i_RSB;
    }
        
    k1 = sstack[arg1].type;
    k2 = sstack[arg2].type;
    n1 = sstack[arg1].val;
    n2 = sstack[arg2].val;
    
    if (k1 == k_numb && k2 == k_numb)
    {
        switch(Op)
        {
            case i_ADD:  
                n = n1 + n2;
                break;
            case i_RSB: 
                n = n1 - n2;
                break;
            case i_MULT:  
                n = n1 * n2;
                break;
            case i_MULTF:  
                n = (int) ((long long int) n1 * (long long int) n2) / (long long int) 65536;
                break;
            case i_DIV:  
                n = n2 / n1;
                break;
            case i_DIVF:  
                if (n1 < 0)
                {
                    n1 = -n1;
                    n2 = -n2;
                }
                n = (int) (((long long int) n2 * (long long int) 65536) / (long long int) n1);
                break;
            case i_REM:  
                n = n2 % n1;
                break;
            case i_ORR:
                n = n1 | n2;
                break;
            case i_AND:
                n = n1 & n2;
                break;
            case i_EOR:
                n = n1 ^ n2;
                break;
        case i_LSHIFT:
                n = n2 << n1;
                break;
        case i_RSHIFT:
                n = n2 >> n1;
                break;
        default: 
                printf("CG error: unknown dyadic Op%d\n", Op);
                break;
        }
        lose1(k_numb, n);
        return swapped;
    }

    if (k1 == k_numb)
    {
        r = movetoanyr(arg2);
        if (Op != i_CMP)
        {
            s = nextfree();
            forgetr(s);
        }
        
        if (Op == i_LSHIFT || Op == i_RSHIFT)
        {
            if (n1 != 0)
            {
                sh = (Op == i_LSHIFT) ? 0 : 4; 
                GenRRR(i_MOV, 0, s, (n1 << 7) | (sh << 4) | r); 
                lose1(k_reg, s);
            }
            else
            {
                stack(ssp-1);
            }
            pendingop = s_NONE;
            return swapped;
        }
        
        if (Op == i_MULT)
        {
            sh = 1;
            switch (n1)
            {
                case 0:
                    GenRRn(i_MOV, 0, r, 0, 0);
                    forgetr(r);
                    remem(r, k_numb, 0);
                    break;
                case 1:
                    break;
                case 2:
                    GenRRR(i_ADD, r, r, r);
                    forgetr(r);
                    break;
                case -1:
                    GenRRn(i_RSB, r, r, 0, 1);
                    forgetr(r);
                    break;
                case -2:
                    GenRRR(i_ADD, r, r, r);
                    GenRRn(i_RSB, r, r, 0, 1);
                    forgetr(r);
                    break;
                default:
                    sh = powerof2(n1);
                    if (sh > 0)
                    {
                        GenRRR(i_MOV, 0, r, (sh << 7) | r); 
                        if (n1 < 0)
                        {
                            GenRRn(i_RSB, r, r, 0, 1);
                        }
                        forgetr(r);
                    }
            }
            if (sh > 0)
            {
                lose1(k_reg, r);
                pendingop = s_NONE;
                return swapped;
            }
        
            s = movetoanyr(arg1);
            t = nextfree();
            forgetr(t);
            GenMult(t, r, s);
            lose1(k_reg, t);
            pendingop = s_NONE;
            return swapped;
        }
        
        if (Op == i_CMP && n1 < 0)
        {
            Op = i_CMN;
            n1 = -n1;
            sstack[arg1].val = n1;
        }
        
        if (op2size(n1) == 1)
        {
            sh = 0;
            while (((n1 & 3) == 0) && ((n1 & 0xFFFFFF00) != 0))
            {
                n1 = n1 >> 2;
                sh += 1;
            }
            if (Op == i_CMP || Op == i_CMN)
            {
                GenRRn(Op, r, 0, (16 - sh) % 16, n1 | (1 << 20));
            }
            else
            {
                GenRRn(Op, r, s, (16 - sh) % 16, n1);
                lose1(k_reg, s);
            }
            return swapped;
        }
        else
        {

            t = movetoanyr(arg1);
            if (Op == i_CMP || Op == i_CMN)
            {
                GenRRR(Op, r, 0, t | (1 << 20));
            }
            else
            {
                GenRRR(Op, r, s, t);
                lose1(k_reg, s);
            }
            return swapped;
        }
    }
    
    /* at this point, neither argument is a constant so use regs */

    r = movetoanyr(arg2);
    s = movetoanyr(arg1);
    if (Op != i_CMP)  /* t not used in comparison */
    {
        t = nextfree();
        forgetr(t);
    }
    
    if (Op == i_CMP)
    {
        GenRRR(Op, r, 0, s | (1 << 20));
    }
    else if ((Op == i_LSHIFT) || (Op == i_RSHIFT))
    {
        sh = (Op == i_LSHIFT) ? 1 : 5;
        GenRRR(i_MOV, 0, t, (s << 8) | (sh << 4) | r); 
    }
    else if (Op == i_MULT)
    {
        GenMult(t, r, s);
    }
    else
    {
        GenRRR(Op, r, t, s);
    }
    if (Op != i_CMP)
    {
        lose1(k_reg, t);
    }
    return swapped;
}

/* --------------------------------------------------------- */
void cgmonadic(unsigned int Op)
{
    unsigned int r;
    unsigned int k;
    int n;
    
    k = sstack[arg1].type;
    n = sstack[arg1].val;
    if (k == k_numb)
    {
        switch(Op)
        {
            case i_NEG:  
                n = -n;
                break;
            case i_NOT:
                if (n == 0)
                {
                    n = 1;
                }
                else
                {
                    n = 0;
                }
                break;
            case i_ABS:  
                n = abs(n);
                break;
            case i_COMP:  
                n = ~n;
                break;
            case i_FLOAT:
                n = n << 16;
                break;
            case i_INT:
                n = n >> 16;
                break;
            default: 
                Error(3009, "Unknown monadic constant Op (%d)\n", Op);
                break;
        }
        sstack[arg1].val = n;
        return;
    }
    
    r = movetoanyr(arg1);
    switch(Op)
    {
        case i_NEG:
            GenRRn(i_RSB, r, r, 0, 0);  
            break;
        case i_COMP:
            GenRRR(i_MVN, 0, r, r);
            break;
        case i_NOT:
            GenRRn(i_CMP, r | 16, 0, 0, 0);  /* also set bit 20 */
            GenRRncond(i_MOV, b_NE, 0, r, 0, 0);
            GenRRncond(i_MOV, b_EQ, 0, r, 0, 1);  
            break;
        case i_ABS:
            GenRRn(i_CMP, r | 16, 0, 0, 0);  /* also set bit 20 */
            GenRRncond(i_RSB, b_LS, r, r, 0, 0);
            break;
        case i_FLOAT:
            GenRRR(i_MOV, 0, r, (16 << 7) | r);
            break;
        case i_INT:
            GenRRR(i_MOV, 0, r, (16 << 7) | (4 << 4) | r);
            break;
        case i_RV:
            GenLDR(r, r, 0);
            break;
        default: 
            Error(3010, "Unknown monadic op (%d)\n", Op);
            break;
    }
    if (!(Op == i_FLOAT || Op == i_INT))
    {
        forgetr(r);
        sstack[arg1].type = k_reg;
        sstack[arg1].val = r;
    }
}

/* --------------------------------------------------------- */
unsigned int cgcmp(unsigned int f)
{
    unsigned int k1, k2;
    int          n1, n2;
    bool         swapped;
    bool         jumping;
    
    k1 = sstack[arg1].type;
    k2 = sstack[arg2].type;
    n1 = sstack[arg1].val;
    n2 = sstack[arg2].val;
    jumping = false;
    
    if (k1 == k_numb && k2 == k_numb)
    {
        switch(f)
        {
            case b_EQ:
                jumping = n1 == n2;
                break;
            case b_NE:
                jumping = n1 != n2;
                break;
            case b_GE:
                jumping = n2 >= n1;
                break;
            case b_LS:
                jumping = n2 < n1;
                break;
            case b_GR:
                jumping = n2 > n1;
                break;
            case b_LE:
                jumping = n2 <= n1;
                break;
            default:
                printf("CG error: unknown constant branch condition %d\n", f);
                break;            
        }
        if (jumping)
        {
            return b_BR;
        }
        else
        {
            return b_NO;  /* i.e. suppress the jump */
        }
    }
    
    swapped = cgdyadic(i_CMP, true);
    if (!swapped) 
    {
        return f;
    }
    else
    {
        switch(f)
        {
            case b_LS:
                return b_GR;
            case b_GR:
                return b_LS;
            case b_LE:
                return b_GE;
            case b_GE:
                return b_LE;
            default:
                return f;
        }
    }
}

/* --------------------------------------------------------- */
void cgrv()
{

    unsigned int r;
    unsigned int s;
    unsigned int k = sstack[arg1].type;
    int n          = sstack[arg1].val;
    
    if (pendingop == s_MINUS && k == k_numb)
    {
        pendingop = s_PLUS;
        n = -n;
    }
    if (pendingop == s_PLUS && k == k_numb)
    {
        r = movetoanyr(arg2);
        GenLDR(r, r, n);
        lose1(k_reg, r);

        pendingop = s_NONE;
    }
    else if (pendingop == s_PLUS)
    {
        s = movetoanyr(arg1);
        r = movetoanyr(arg2);
        GenLDRR(r, s);
        lose1(k_reg, r);
        pendingop = s_NONE;
    }
    else
    {
        cgpendingop();
        r = movetoanyr(arg1);
        GenLDR(r, r, 0);
    }
    forgetr(r);
}

/* --------------------------------------------------------- */
void cgstind()
{
    unsigned int r1;
    unsigned int r2;
    unsigned int r3;
    unsigned int k;
    int          n;
    unsigned int arg3 = arg2-1;

    
    k = sstack[arg1].type;
    n = sstack[arg1].val;

    if (pendingop == s_MINUS && k == k_numb)
    {
        pendingop = s_PLUS;
        n = -n;
    }
    if (pendingop == s_PLUS && k == k_numb)
    {
        r2 = movetoanyr(arg2);
        r1 = movetoanyr(arg3);
        stack(ssp-3);
        GenSTR(r1, r2, n);
        pendingop = s_NONE;
        forgetvars();
    }
    else if (pendingop == s_PLUS)
    {
        r1 = movetoanyr(arg1);
        r2 = movetoanyr(arg2);
        r3 = movetoanyr(arg3);
        stack(ssp-3);
        GenSTRR(r2, r3, r1);
        pendingop = s_NONE;
        forgetvars();
    }
    else
    {
        cgpendingop();
        r1 = movetoanyr(arg1);
        r2 = movetoanyr(arg2);
        stack(ssp-2);
        GenSTR(r2, r1, 0);
        forgetvars();
    }
}

/* --------------------------------------------------------- */
void cgsave(unsigned int n)
{
    unsigned int r;
    unsigned int s;
    for (r=r_r0; r<=r_r3; r+=1)
    {
        s = 3+r-r_r0;
        if (s >= n)
        {
            break;
        }
        remem(r, k_loc, s);
    }

    initstack(n);
    
    OutWord(0xE8A4C800);              /* STM r_r4,{r_p,r_lr,r_pc}        inc r4    */
    OutWord(0XE884000F);              /* STM r_r4,{r_r0,r_r1,r_r2,r_r3}  no inc r4 */
    GenRRn(i_SUB, r_r4, r_p, 0, 12);  /* SUB r_p,r_r4,#12                          */
}

/* --------------------------------------------------------- */
void cgapply(unsigned int Op, unsigned int k, bool syscall)
{
    unsigned int sa1 = k+3;
    unsigned int sa4 = k+6;
    int          r;
    int          s;
    unsigned int t;
    unsigned int l;
    
    cgpendingop();
    
    /* store args 5,6.... */
    store(sa4+1, ssp-2);
    
    /* now deal with non-args */
    for (t=0; t<=arg2; t+=1)
    {
        if (sstack[t].pos >= k)
        {
            break;
        }
        if (sstack[t].type == k_reg)
        {
            storet(t);
        }
    }
    
    /* move args 1-4 to arg regs */
    for (t=arg2; t>=0; t-=1)
    {
        s = sstack[t].pos;
        r = s-k-3;
        if (s < sa1)
        {
            break;
        }
        if ((s <= sa4) && isfree(r))
        {
            movetor(t, r);
        }
    }
    for (t=arg2; t>=0; t-=1)
    {
        s = sstack[t].pos;
        r = s-k-3;
        if (s < sa1)
        {
            break;
        }
        if (s <= sa4)
        {
            movetor(t, r);
        }
    }
    
    /* deal with args not in SS */
    for (s=sa1; s<=sa4; s+=1)
    {
        r = s-k-3;
        if (s >= sstack[0].pos)
        {
            break;
        }
        if (regusedby(arg1) == r)
        {
            movetor(arg1, r_r9);
        }
        loadt(k_loc, s);
        movetor(arg1, r);
        stack(ssp - 1);
    }


    GenAddConstant(r_p, r_r4, 4 * k);  /* ADD r_r4,r_p,#4*k  */
            
    l = sstack[arg1].val;
    if (syscall)
    {
        GenSysCall(l, r_r5);
    }
    else
    {
        GenBranchWithLink(l);
    }
    forgetall();
    stack(k);
    
    if (Op == s_FNAP)
    {
        loadt(k_reg, r_r0);
    }
}

/* --------------------------------------------------------- */
void cgreturn(unsigned int n)
{
    cgpendingop();
    if (Procedures[n].ProcType != VoidType)
    {
        movetor(arg1, r_r0);
        stack(ssp - 1);
    }
    
    OutWord(0xE89B8800);   /* LDM r_p,{r_p,r_pc}  no inc */
    
    initstack(ssp);
}

/* --------------------------------------------------------- */
void cgstring(unsigned int n)  /* n is the string number */
{
    unsigned int lab = 0;
    
    lab = ProgStringList[n];
    loadt(k_lvlab, lab);
}

/* --------------------------------------------------------- */
void cgname(char name[])
{
    unsigned int  w;
    unsigned int  i;
    unsigned int  j;
    char          str[8];
    
    strncpy(str, name, 8);
    
    if (strcmp(name, "main") == 0)
    {
        str[4] = '_';  /* change to main_ to avoid linker clash */
        str[5] = '\0';
    }
    for (i=0; i<=7; i+=1)
    {
        if (str[i] == '\0')
        {
            for (j=i+1; j<=7; j+=1)
            {
                str[j] = '\0';
            }
            break;
        }
    }
    w = pack4b(str[0], str[1], str[2], str[3]);
    OutWord(w);
    w = pack4b(str[4], str[5], str[6], str[7]);
    OutWord(w);
}

/* --------------------------------------------------------- */
unsigned int pack4b(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    return (d << 24) | (c << 16) | (b << 8) | a;
}
