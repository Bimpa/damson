/* DAMSON emulator header
*/

#ifndef EMULATOR
#define EMULATOR

#include <stdio.h>
#include "compiler.h"

#define CLOCK_FREQUENCY (200000000)      /* 200 MHz */
#define MaxChannelOffsets 1000

enum ProcessState { Running, Waiting, Delaying, DMATransfer };

struct LogInfo 
{
	FILE                   *stream;
    unsigned int           node;
    char                   name[MaxStringSize];
	unsigned long long int startwindow;
	unsigned long long int stopwindow;
	unsigned long long int sampleinterval;
	char                   format[MaxStringSize];
	unsigned int           noffsets;
	unsigned int           offsets[MaxChannelOffsets];
    unsigned long long int lastlog;
	bool                   logmode;
};

struct PCB
{
    struct PCB             *nextPCB;
    struct PCB             *prevPCB;
    unsigned int           saved_PC;
    unsigned int           saved_SP;
    unsigned int           saved_FP;
    int                    *stack;
    unsigned int           stacksize;
    enum ProcessState      status;
    unsigned int           handle;
    unsigned int           priority;
    unsigned int           dticks;
    int                    *semaphore;
};

struct NodeInfo
{
    unsigned int           NodeNumber;
    char                   *NodeName;
    struct NodeInfo        *NextNode;
    struct NodeInfo        *PrevNode;
    struct NodeInfo        *Parent;
    struct LimitItem	   *LimitItem;
    unsigned int           copies;
    unsigned int           SP;
    unsigned int           PC;
    unsigned int           FP;
    int                    *S;
    int                    *G;
    int                    *E;
    InterruptVector        *IntVector;
    unsigned int           NumberOfInterrupts;
    struct PCB             *ProcessList;
    unsigned int           NumberOfProcesses;
    struct PCB             *CurrentProcess;
    struct PCB             **ProcessHashTable;
    unsigned int           HandleNumber;
    Instruction            *Instructions;
    unsigned int           ProgramSize;
    NametableItem          *Globals;
    NametableItem          *Externals;
    unsigned int           NumberOfGlobals;
    unsigned int           GlobalVectorSize;
    unsigned int           NumberOfExternals;
    unsigned int           ExternalVectorSize;
    unsigned int           *Labels;
    unsigned int           NumberOfLabels;
    ProcedureItem          *Procedures;
    unsigned int           NumberOfProcedures;
    unsigned int           PktsTX;
    unsigned int           PktsRX;
    unsigned int           Tickrate;
    unsigned long long int SystemTicks;
    unsigned long long int LastClockTick;         
    unsigned int           DMATicks;
    bool                   SyncWait;
};

struct LimitItem{
	struct LimitItem 		*NextItem;
	struct LimitItem 		*PrevItem;
	struct NodeInfo 		*LastNode;
	unsigned long long int 	Value;
	unsigned int 			items;
};

extern struct NodeInfo *CreatePrototype(char          *nodename,
                                        Instruction   *code,      unsigned int codesize, 
                                        int           *gv,        unsigned int gvsize,
                                        NametableItem *globals,   unsigned int nglobals,
                                        int           *ev,        unsigned int evsize,
                                        NametableItem *externals, unsigned int nexternals,
                                        ProcedureItem *procs,     unsigned int nprocs,
                                        unsigned int  *labs,      unsigned int nlabs);

extern struct NodeInfo *CreateNode(unsigned int    node, 
                                   char            *nodename, 
                                   int             *gv,       unsigned int gvsize, 
                                   int             *ev,       unsigned int evsize,
                                   InterruptVector *intv,     unsigned int intvsize);

extern unsigned int ProfileNode;

extern void                   Emulate(bool debugging, bool archecking);
extern void                   FetchInstruction(unsigned int *Op, int *Arg);
extern void                   ExecuteInstruction(unsigned int Op, int Arg);
extern struct                 NodeInfo *FindNode(unsigned int n);
extern struct                 NodeInfo *FindNamedNode(char *nodename);
extern void                   Tracing(bool mode);
extern unsigned int           OpenLog(unsigned int n, char Filename[], char PrototypeName[], char ChannelName[], char Format[], 
                                      unsigned long long int s1, unsigned long long int s2, unsigned long long int s3, bool logging);
extern void                   AddLog(unsigned int channel, unsigned int offset);
extern struct LogInfo         *GetLog(unsigned int channel);
extern void                   OpenProfile(unsigned int n, char Filename[]);
extern float                  TicksToTime(unsigned long long int t);
extern unsigned long long int TimeToTicks(float t);

#endif
