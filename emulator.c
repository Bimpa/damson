/* DAMSON emulator
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <limits.h>
#include <time.h>

#include "compiler.h"
#include "emulator.h"
#include "debug.h"

#define MaxProcesses         10000
#define ProcessHashTableSize (MaxProcesses * 2)
#define diagnostics          false
#define MaxLogChannels       1000

void                   *TOMBSTONE = &TOMBSTONE;
struct NodeInfo        *NodeList = NULL;
struct NodeInfo        *NodeListTail = NULL;
struct NodeInfo        *NameNodeList = NULL;
struct LimitItem	   *LimitList = NULL;
struct LimitItem	   *LimitListTail = NULL;
unsigned int           HashTableSize;
struct NodeInfo        **HashTable;
struct NodeInfo        *CurrentNode;
unsigned long long int ProcessingTicks;
unsigned long long int TotalTicks;
unsigned long long int t1;
unsigned int           workspace = 0;
bool                   TraceMode = false;
struct LogInfo         LogData[MaxLogChannels + 1];
unsigned int           LogStreams = 0;
unsigned int           mallocs = 0;
unsigned int           malloc_count = 0;
unsigned int           ProfileNode = 0;
FILE                   *ProfileStream = NULL;
float 				   AverageSearches = 0;  /*Used to keep a running average of the number of searches through the linked list*/
int 				   CountSearches = 0;

unsigned int           GetNodeLabel(unsigned int node, unsigned int lab);
void                   Interrupt(unsigned int dnode, unsigned int NodeId, int pkt);
unsigned int           Hash(unsigned int n, unsigned int size);
void                   AddNode(unsigned int n, struct NodeInfo *naddr);
void                   RestoreProcess(unsigned int n, struct PCB *p);
void                   SaveProcess(unsigned int n, struct PCB *p);
unsigned int           CreateProcess(unsigned int node, unsigned int StartAddress, unsigned int Size, unsigned int plevel);
void                   DeleteProcess(unsigned int node, unsigned int prev);
struct PCB             *FindProcess(unsigned int node, unsigned int prev);
void                   GetFileName(char infile[], char outfile[], char ext[]);
int                    GetLocalClock(int n);
void                   StackPush(int x);
int                    StackPop();
int                    StackTop();
void                   StackPushbool(bool Item);
bool                   StackPopbool();
struct tnode           *FindLink(unsigned int n, struct tnode *p);
void                   SendPkt(unsigned int SourceNode, unsigned int port, int DataValue);
void                   WriteTimeStamp(int n);
void                   ShowPkt(unsigned int snode, unsigned int dnode, int pkt, unsigned int tstamp);
int                    ConvertToFloat(int x);
int                    ConvertToInt(int x);
void                   DiadicOp(unsigned int Op);
void                   MonadicOp(unsigned int Op);
void                   myprintf(char *fmt, ...);
void                   myfprintf(FILE *stream, char *fmt, ...);
void                   AddProcess(unsigned int node, struct PCB *process);
void                   RemoveProcess(unsigned int node, unsigned int prev);
void                   Reschedule(unsigned int node);
void                   DeleteNode(unsigned int node);
void                   UpdateLog(unsigned int n, bool logging);
void                   CloseLogs();
void                   RemoveNode(unsigned int node);
void                   Runtime_Error(unsigned int code, char *fmt, ...);
char*                  NodeLocalName(struct NodeInfo *prev, unsigned int pc, unsigned int p);
char*                  NodeGlobalName(struct NodeInfo *prev, unsigned int p);
void 				   InsertLinkedNode(struct NodeInfo *n, struct NodeInfo *prev);
void				   ReorderLinkedListItem(struct NodeInfo *h);
void				   RemoveLinkedNode(struct NodeInfo *n);
void				   RemoveLinkedLimit(struct LimitItem *prev);

void				   SyncNodes(unsigned int node);
unsigned int 		   sync_count;
void                   CloseProfile();
bool                   ArithmeticChecking;

/* --------------------------------------------------------- */
void SyncNodes(unsigned int node){
    struct NodeInfo        *h;

    sync_count++;

    if (sync_count == NumberOfNodes){
        //printf("SYNC'D %d nodes\n", sync_count);
        //reschedule all
        h = NodeList;
        while (h != NULL)
        {
            //rewind clock
            if (CurrentNode->SystemTicks < h->SystemTicks)
            {
                h->SystemTicks = CurrentNode->SystemTicks;
                ReorderLinkedListItem(h);
            }
            h->SyncWait = false;
            Reschedule(h->NodeNumber);
            h = h->NextNode;
        }
        sync_count = 0;
    }
    else{
        //reschedule blocking node
        Reschedule(node);
    }

}

/* --------------------------------------------------------- */
char* NodeLocalName(struct NodeInfo *prev, unsigned int pc, unsigned int p)
{
    unsigned int i;
    unsigned int pnum;

    while (pc > 0)
    {
        if (prev->Instructions[pc].Op == s_ENTRY)
        {
            break;
        }
        pc = pc - 1;
    }

    pnum = prev->Instructions[pc].Arg;

    for (i=1; i<=prev->Procedures[pnum].nLocals; i+=1)
    {
        if (prev->Procedures[pnum].Args[i].vOffset == p)
        {
            return (char *) prev->Procedures[pnum].Args[i].Name;
        }
    }
    return NULL;
}

/* --------------------------------------------------------- */
char* NodeGlobalName(struct NodeInfo *prev, unsigned int p)
{
    unsigned int i;

    for (i=1; i<=prev->NumberOfGlobals; i+=1)
    {
        if (prev->Globals[i].vOffset == p)
        {
            return (char *) prev->Globals[i].Name;
        }
    }
    return NULL;
}

/* --------------------------------------------------------- */
unsigned int Hash(unsigned int n, unsigned int TableSize)
{
    unsigned int p;

    p = (n * 137 + 92731) % TableSize;
    return p;
}

/* --------------------------------------------------------- */
struct NodeInfo *FindNode(unsigned int node)
{
    unsigned int    h;
    unsigned int    n;
    struct NodeInfo *b;

    //printf("FindNode: node=%d\n", node);  // ***
    n = 0;
    h = Hash(node, HashTableSize);
    
    while (HashTable[h] != NULL) 
    {
        b = HashTable[h];
        if (b != TOMBSTONE)
        {
            if (b->NodeNumber == node) 
            {
                return b;
            }
        }
        
        n += 1;
        if (n >= HashTableSize) 
        {
            Runtime_Error(202, "Node missing in hash table (%d)\n", node);
        }
        h += 1;
        if (h >= HashTableSize) 
        {
            h = 0;
        }
    }
    return NULL;  /* can happen if node has been deleted */
}

/* --------------------------------------------------------- */
void AddNode(unsigned int node, struct NodeInfo *nodeptr)
{
    unsigned int h;
    unsigned int n;

    //printf("AddNode: node=%d\n", node); // *** 
    n = 0;
    h = Hash(node, HashTableSize);

    while (HashTable[h] != NULL) 
    {
        if (HashTable[h] == TOMBSTONE)
        {
            HashTable[h] = nodeptr;
            return;
        }
        
        n += 1;
        if (n >= HashTableSize) 
        {
            Runtime_Error(201, "Hashtable overflow: node=%d size=%d\n", node, HashTableSize);
        }
        h += 1;
        if (h >= HashTableSize) 
        {
            h = 0;
        }
    }
    HashTable[h] = nodeptr;
}

/* --------------------------------------------------------- */
void RemoveNode(unsigned int node)   /* remove a node from the hash table */
{
    unsigned int    h;
    unsigned int    n;
    struct NodeInfo *b;

    //printf("RemoveNode: node=%d\n", node);  // ***
    n = 0;
    h = Hash(node, HashTableSize);
    
    while (HashTable[h] != NULL) 
    {
        b = HashTable[h];
        if (b != TOMBSTONE)
        {
            if (b->NodeNumber == node) 
            {
                HashTable[h] = TOMBSTONE;
                return;
            }
        }
        n += 1;
        if (n >= HashTableSize) 
        {
            Runtime_Error(202, "Node missing in hash table (%d)\n", node);
        }
        h += 1;
        if (h >= HashTableSize) 
        {
            h = 0;
        }
    }
}

/* --------------------------------------------------------- */
struct NodeInfo *FindNamedNode(char *name)
{
    struct NodeInfo *p;

    p = NameNodeList;
    while (p != NULL)
    {
        if (strcmp(p->NodeName, name) == 0)
        {
            return p;
        }
        p = p->NextNode;
    }
    return NULL;
}

/* --------------------------------------------------------- */
struct NodeInfo *CreateNode(unsigned int    node, 
                            char            *name, 
                            int             *gv,   unsigned int gvsize, 
                            int             *ev,   unsigned int evsize,
                            InterruptVector *intv, unsigned int intvsize)
{
    struct NodeInfo *b;
    struct NodeInfo *p;
    unsigned int    s;
    unsigned int    i;
    
    p = NodeList;       /* remove node from nodelist */
    while (p != NULL)
    {
        if (p->NodeNumber == node)
        {
            Runtime_Error(203, "CreateNode: multiple node %d\n", node);
        }
        p = p->NextNode;
    }

    p = FindNamedNode(name);
    if (p == NULL)
    {
        Runtime_Error(203, "CreateNode: cannot find parent node %s\n", name);
    }
    	
    p->copies += 1;
    
    //printf("CreateNode: node=%d\n", node); // ***
    s = sizeof(struct NodeInfo);
    workspace += s;
    b = malloc(s);
    if (b == NULL)
    {
        Runtime_Error(204, "Unable to allocate memory for node: %s\n", name);
    }
    
    b->NodeNumber = node;
    b->PC = p->PC;
    b->Parent = p;
    
    if (NodeList == NULL) /* add to tail of linked list */
    	NodeListTail = b;
    else
    	NodeList->PrevNode = b;

    b->NextNode = NodeList;  /* add to head of linked list */
    b->PrevNode = NULL;
    NodeList = b;
    
    b->NodeName = p->NodeName;  /* copy of parent's node name */
    
    b->Instructions = p->Instructions;  /* copy of parent's instructions */
    b->ProgramSize = p->ProgramSize;
    
    b->Globals = p->Globals;/* copy of parent's globals information */
    b->NumberOfGlobals = p->NumberOfGlobals;
    
    s = sizeof(int) * (gvsize + 1);   /* global vector generated by the compiler */
    workspace += s;
    b->G = malloc(s);
    if (b->G == NULL)
    {
        Runtime_Error(205, "Unable to allocate memory for global vector: node %s\n", name);
    }
    memcpy(b->G, gv, s);
    b->GlobalVectorSize = gvsize;
    
//++++++++++++++ new section for externals
    b->Externals = p->Externals;/* copy of parent's externals information */
    b->NumberOfExternals = p->NumberOfExternals;

    s = sizeof(int) * (evsize + 1);  /* external vector generated by the compiler */
    workspace += s;
    b->E = malloc(s);
    if (b->E == NULL)
    {
        Runtime_Error(206, "Unable to allocate memory for external vector: node %s\n", name);
    }
    memcpy(b->E, ev, s);
    b->ExternalVectorSize = evsize;
//++++++++++++++    

    s = sizeof(InterruptVector) * (intvsize + 1);   /* copy interrupt vectors */
    workspace += s;
    b->IntVector = malloc(s);
    if (b->IntVector == NULL)
    {
        Runtime_Error(207, "Unable to allocate memory for interrupt vector: node %s\n", name);
    }
    memcpy(b->IntVector, intv, s);
    b->NumberOfInterrupts = intvsize;
    
    b->SP = 0;
    b->FP = 0;
    
    b->Labels = p->Labels;    /* copy of parent's labels information */
    b->NumberOfLabels = p->NumberOfLabels;

    b->Procedures = p->Procedures;  /* copy of parent's procedures information */
    b->NumberOfProcedures = p->NumberOfProcedures;

    b->ProcessHashTable = malloc(sizeof(int) * ProcessHashTableSize);  /* claim space for process hash table */
    if (b->ProcessHashTable == NULL)
    {
        Runtime_Error(208, "Unable to allocate memory for process hash table\n");
    }
    
    for (i=0; i<ProcessHashTableSize; i+=1)  /* initialise process hash table */
    {
        b->ProcessHashTable[i] = NULL;
    }
    
    b->NumberOfProcesses  = 0;   /* initialise node state */
    b->HandleNumber       = 0;
    b->ProcessList        = NULL;
    b->CurrentProcess     = NULL;
    b->SystemTicks        = 0;
    b->LastClockTick      = 0;    
    b->Tickrate           = CLOCK_FREQUENCY / 1000;  /* default 1 ms */
    b->PktsTX             = 0;
    b->PktsRX             = 0;
    b->DMATicks           = 0;
    b->SyncWait           = false;
    return b;
}

/* --------------------------------------------------------- */
struct NodeInfo *CreatePrototype(char          *name,
                                 Instruction   *code,      unsigned int codesize, 
                                 int           *gv,        unsigned int gvsize,
                                 NametableItem *globals,   unsigned int nglobals,
                                 int           *ev,        unsigned int evsize,
                                 NametableItem *externals, unsigned int nexternals,
                                 ProcedureItem *procs,     unsigned int nprocs,
                                 unsigned int  *labs,      unsigned int nlabs)
{
    struct NodeInfo *b;
    unsigned int    s;
    
    b = FindNamedNode(name);
    if (b != NULL)
    {
        Runtime_Error(209, "CreatePrototype: node previously defined %s\n", name);
    }
    
    //printf("CreatePrototype: node=%s\n", name); // ***
    s = sizeof(struct NodeInfo);
    workspace += s;
    b = malloc(s);
    if (b == NULL)
    {
        Runtime_Error(210, "Unable to allocate memory error for prototype node: %s\n", name);
    }
    
    b->NodeNumber = 0;
    b->PC = 1;
    b->Parent = 0;
    b->copies = 0;
    
    b->NextNode = NameNodeList;  /* add to head of linked list */
    NameNodeList = b;
    
    s = strlen(name) + 1;   /* copy node name */
    b->NodeName = malloc(s);
    memcpy(b->NodeName, name, s);
    
    s = sizeof(Instruction) * (codesize + 1);  /* copy instructions */
    workspace += s;
    b->Instructions = malloc(s);
    if (b->Instructions == NULL)
    {
        Runtime_Error(211, "Unable to allocate memory for instructions: code size %d\n", codesize);
    }
    memcpy(b->Instructions, code, s);
    b->ProgramSize = codesize;
    
    s = sizeof(NametableItem) * (nglobals + 1);  /* copy globals information */
    workspace += s;
    b->Globals = malloc(s);
    if (b->Globals == NULL)
    {
        Runtime_Error(212, "Unable to allocate memory for globals: prototype node %s\n", name);
    }
    memcpy(b->Globals, globals, s);
    b->NumberOfGlobals = nglobals;
    
    s = sizeof(int) * (gvsize + 1);   /* copy global vector */
    workspace += s;
    b->G = malloc(s);
    if (b->G == NULL)
    {
        Runtime_Error(213, "Unable to allocate memory  for global vector: prototype node %s\n", name);
    }
    memcpy(b->G, gv, s);
    b->GlobalVectorSize = gvsize;

//++++++++++++++ new section for externals
    s = sizeof(NametableItem) * (nexternals + 1);  /* copy externals information */
    workspace += s;
    b->Externals = malloc(s);
    if (b->Externals == NULL)
    {
        Runtime_Error(214, "Unable to allocate memory for externals: prototype node %s\n", name);
    }
    memcpy(b->Externals, externals, s);
    b->NumberOfExternals = nexternals;
    
    s = sizeof(int) * (evsize + 1);  /* copy externals vector */
    workspace += s;
    b->E = malloc(s);
    if (b->E == NULL)
    {
        Runtime_Error(215, "Unable to allocate memory  for external vector: prototype node %s\n", name);
    }
    memcpy(b->E, ev, s);
    b->ExternalVectorSize = evsize;
//++++++++++++++    

    b->SP = 0;
    b->FP = 0;
    
    s = sizeof(int) * (nlabs + 1);   /* copy labels information */
    workspace += s;
    b->Labels = malloc(s);
    if (b->Labels == NULL)
    {
        Runtime_Error(216, "Unable to allocate memory for Labels: prototype node %s\n", name);
    }
    memcpy(b->Labels, labs, s);
    b->NumberOfLabels = nlabs;

    s = sizeof(ProcedureItem) * (nprocs + 1);  /* copy procedures information */
    workspace += s;
    b->Procedures = malloc(s);
    if (b->Procedures == NULL)
    {
        Runtime_Error(217, "Unable to allocate memory for Procedures: prototype node %s\n", name);
    }
    memcpy(b->Procedures, procs, s);

    b->NumberOfProcedures = nprocs;

    b->NumberOfProcesses  = 0;   /* initialise node state */
    b->HandleNumber       = 0;
    b->ProcessList        = NULL;
    b->CurrentProcess     = NULL;
    b->SystemTicks        = 0;
    b->LastClockTick      = 0;    
    b->Tickrate           = CLOCK_FREQUENCY / 1000;  /* default 1 ms */
    b->PktsTX             = 0;
    b->PktsRX             = 0;
    
    return b;
}
    
/* --------------------------------------------------------- */
void DeleteNode(unsigned int node)
{
    struct NodeInfo *b;
    struct NodeInfo *p;
    
    //printf("Deletenode: node=%d\n", node); // ***

    if (node == ProfileNode)
    {
        CloseProfile();
    } 
    
    b = FindNode(node);
    RemoveNode(node);

    if (b->PktsTX > 0 || b->PktsRX > 0)
    {
        printf("Node=%d TxPkts=%d RxPkts=%d\n", b->NodeNumber, b->PktsTX, b->PktsRX);
    }
   
    TotalTicks += b->SystemTicks;
    
    free(b->G);         /* remove node's global vector, external vector and proc hash table */
    free(b->E);
    free(b->ProcessHashTable);
    free(b->IntVector);

    /* remove from linked list */
    RemoveLinkedNode(b);


    p = b->Parent;
    free(b);    /* remove node info */

    p->copies -= 1;
    if (p->copies == 0)  /* OK to remove parent from named nodelist? */
    {
        free(p->NodeName);
        free(p->Instructions);
        free(p->Globals);
        free(p->G);
        free(p->Externals);
        free(p->E);
        free(p->Labels);
        free(p->Procedures);

        b = p;
        p = NameNodeList;   /* remove node from named node list */
        while (p != NULL)
        {
            if (p->NextNode == b)
            {
                p->NextNode = b->NextNode;
            }
            p = p->NextNode;
        }
    
        if (b == NameNodeList)
        {
            NameNodeList = b->NextNode;
        }
        free(b);
    }
    
    NumberOfNodes -= 1;
}
    
/* --------------------------------------------------------- */
unsigned int GetNodeLabel(unsigned int node, unsigned int lab)
{
    struct NodeInfo *d;

    d = FindNode(node);
    return d->Labels[lab];
}

/* --------------------------------------------------------- */
void Reschedule(unsigned int node)
{
    struct NodeInfo *h;
    unsigned int    plevel;
    struct PCB      *d;
    struct PCB      *pr;
   
    //printf("reschedule: node=%d\n", node); // ***
    h = FindNode(node);
    if (h->CurrentProcess != NULL)
    {
        SaveProcess(h->NodeNumber, h->CurrentProcess);
    }
    plevel = 0;
    pr = NULL;
    d = h->ProcessList;
    
    if (h->SyncWait){
        h->CurrentProcess = NULL;
        return;
    }

    while (d != NULL)
    {

        if (d->status == Delaying)
        {
            if (d->dticks == 0)
            {
                d->status = Running;
            }
        }
 
        if (d->status == Waiting && *(d->semaphore) > 0)
        {
            *(d->semaphore) -= 1;
            d->status = Running;
            d->semaphore = NULL;
        }

        if (d->status == Running)
        {
            if (d->priority >= plevel)
            {
                pr = d;
                plevel = d->priority;
            }
        }
        
        if (d->status == DMATransfer && h->DMATicks == 0)
        {
            d->status = Running;
            pr = d;
            plevel = d->priority;
            break;
        }
        
        d = d->nextPCB;
    }
    
    h->CurrentProcess = pr;
    if (pr != NULL)
    {
        RestoreProcess(h->NodeNumber, pr);
    }
}

/* --------------------------------------------------------- */
void Interrupt(unsigned int dnode, unsigned int NodeId, int pkt)
{
    unsigned int    h;
    unsigned int    i;
    unsigned int    node;
    unsigned int    port;
    struct NodeInfo *d;
    struct PCB      *p;
    unsigned int    pr;
    
    //printf("Interrupt: dnode=%d NodeID=%d pkt=%d\n", dnode, NodeId, pkt); // ***
   
    node = NodeId >> 11;
    port = NodeId & 0x7ff;
    pr = (node == 0) ? 3 : 1;
    
    d = FindNode(dnode);  /* d=0 is allowed - destination node may have terminated */
    
    for (i=1; i<=d->NumberOfInterrupts; i+=1)
    {
        if (d->IntVector[i].iNumber == node)
        {
            if (NodeId !=0 && d->CurrentProcess == NULL)  /* may need to move destination node clock backwards */
            {
                if (CurrentNode->SystemTicks < d->SystemTicks)
                {
                    //printf("Clock rewound %d ticks\n", d->SystemTicks - CurrentNode->SystemTicks);
                    d->SystemTicks = CurrentNode->SystemTicks;
                    ReorderLinkedListItem(d);
                }
            }
            h = CreateProcess(dnode, d->IntVector[i].iVector, StackSize, pr);
            if (h == 0)
            {
                Runtime_Error(218, "Interrupt: too many processes (%d)\n", MaxProcesses);
            }

            p = FindProcess(dnode, h);
            p->saved_SP           += 1;   /* push source node addr */
            p->stack[p->saved_SP] = node;
            p->saved_SP           += 1;   /* push port */
            p->stack[p->saved_SP] = port;
            p->saved_SP           += 1;   /* push arg */
            p->stack[p->saved_SP] = pkt;
            p->saved_SP           += 1;   /* push time pkt received */
            p->stack[p->saved_SP] = GetLocalClock(dnode);
            p->saved_SP           += 1;   /* push return address = 0 */
            p->stack[p->saved_SP] = 0;
            if (node != 0)
            {
                d->PktsRX += 1;
            }
            return;
        }
    }
    
    if (node != 0)  /* it's a clock interrupt but node has no entry point */
    {
        Runtime_Error(219, "Interrupt: unknown interrupt %d\n", node);
    }
}

/* --------------------------------------------------------- */
void RestoreProcess(unsigned int n, struct PCB *p)
{
    struct NodeInfo *d;

    //printf("RestoreProcess: n=%d p=%p\n", n, p); // ***
    d = FindNode(n);
    d->PC = p->saved_PC;
    d->SP = p->saved_SP;
    d->FP = p->saved_FP;
    d->S  = p->stack;
    d->CurrentProcess = p;
}

/* --------------------------------------------------------- */
void SaveProcess(unsigned int n, struct PCB *p)
{
    struct NodeInfo *d;

    //printf("SaveProcess: n=%d p=%p\n", n, p); // ***
    d = FindNode(n);
    p->saved_PC = d->PC;
    p->saved_SP = d->SP;
    p->saved_FP = d->FP;
}

/* --------------------------------------------------------- */
unsigned int CreateProcess(unsigned int node, unsigned int StartAddress, unsigned int Size, unsigned int plevel)
{
    struct NodeInfo *d;
    struct PCB      *p;
    
    d = FindNode(node);
    
    p = malloc(sizeof(struct PCB));
    if (p == NULL)
    {
        Runtime_Error(220, "Createprocess: unable to create process (%d)\n", node);
    }
    
    d->NumberOfProcesses += 1;
    if (d->NumberOfProcesses > MaxProcesses)
    {
        Runtime_Error(221, "Too many processes (%d)\n", MaxProcesses);
    }
    
    p->nextPCB = d->ProcessList;  /* set forward and backward pointers for new link */
    p->prevPCB = NULL;
    if (d->ProcessList != NULL)
    {
        d->ProcessList->prevPCB = p;
    }
    d->ProcessList = p;           /* add new link to head of list */
    
    p->saved_PC = StartAddress;
    p->stack = malloc(sizeof(int) * Size);
    if (p->stack == NULL)
    {
        Runtime_Error(222, "CreateProcess: unable to allocate stack (%d)\n", Size);
    }
    
    p->stacksize = Size;
    p->saved_SP  = 0;
    p->saved_FP  = 0;
    p->status    = Running;
    d->HandleNumber  += 1;
    p->handle    = d->HandleNumber;
    p->priority  = plevel;
    p->dticks    = 0;
    p->semaphore = NULL;

    p->saved_SP += 1;            /* push zero return addr */
    p->stack[p->saved_SP] = 0;
    
    //printf("CreateProcess: node=%d start=%d h=%d processes=%d\n", node, StartAddress, p->handle, d->NumberOfProcesses); // ***
    AddProcess(node, p);
    return p->handle;
}

/* --------------------------------------------------------- */
void DeleteProcess(unsigned int node, unsigned int handle)
{
    struct NodeInfo *d;
    struct PCB      *p;
    
    d = FindNode(node);

    //printf("DeleteProcess: node=%d h=%d processes=%d\n", node, handle, d->NumberOfProcesses); // ***
    p = FindProcess(node, handle);
    if (p == NULL)
    {
        Runtime_Error(223, "DeleteProcess: unknown process %d\n", node);
    }
    
    free(p->stack);
    RemoveProcess(node, handle);
    if (p->prevPCB != NULL)
    {
        p->prevPCB->nextPCB = p->nextPCB;
    }
    if (p->nextPCB != NULL)
    {
        p->nextPCB->prevPCB = p->prevPCB;
    }
    
    if (p == d->ProcessList)
    {
        d->ProcessList = p->nextPCB;
    }
    free(p);
    
    d->NumberOfProcesses -= 1;
    d->CurrentProcess = NULL;
}

/* --------------------------------------------------------- */
struct PCB *FindProcess(unsigned int node, unsigned int prev)
{
    unsigned int    k;
    unsigned int    n;
    struct NodeInfo *d;
    struct PCB      *p;
    
    n = 0;
    d = FindNode(node);
    
    k = Hash(prev, ProcessHashTableSize);
    
    while (d->ProcessHashTable[k] != NULL)
    {
        p = d->ProcessHashTable[k];
        if (p != TOMBSTONE)
        {
            if (p->handle == prev)
            {
                //printf("FindProcess: node=%d h=%d res=%p\n", node, h, p); // ***
                return p;
            }
        }
        n += 1;
        if (n >= ProcessHashTableSize)
        {
            break;  /* hash table must be full */
        } 
        
        k += 1;
        if (k >= ProcessHashTableSize) 
        {
            k = 0;
        }
    }
    
    Runtime_Error(224, "Process missing in hash table node=%d handle=%d\n", node, prev);
    return NULL;  /* can't happen - stops on Error */
}

/* --------------------------------------------------------- */
void AddProcess(unsigned int node, struct PCB *p)   /* add a process to the hash table */
{
    unsigned int    k;
    unsigned int    n;
    struct NodeInfo *d;

    d = FindNode(node);
    n = 0;
    k = Hash(p->handle, ProcessHashTableSize);

    while (d->ProcessHashTable[k] != NULL)
    {
        if (d->ProcessHashTable[k] == TOMBSTONE)
        {
            d->ProcessHashTable[k] = p;
            return;
        }
        
        n += 1;
        if (n >= ProcessHashTableSize) 
        {
            Runtime_Error(225, "Process hash table overflow (node=%d handle=%d size=%d)\n", node, p->handle, ProcessHashTableSize);
        }
        k += 1;
        if (k >= ProcessHashTableSize) 
        {
            k = 0;
        }
    }
    //printf("AddProcess: node=%d pcb=%p res=%d\n", node, p, k); // ***
    d->ProcessHashTable[k] = p;
}

/* --------------------------------------------------------- */
void RemoveProcess(unsigned int node, unsigned int prev)   /* remove a process from the hash table */
{
    unsigned int    k;
    unsigned int    n;
    struct NodeInfo *d;
    struct PCB      *p;
    
    n = 0;
    d = FindNode(node);
    
    k = Hash(prev, ProcessHashTableSize);
    
    while (d->ProcessHashTable[k] != NULL)
    {
        p = d->ProcessHashTable[k];
        if (p != TOMBSTONE)
        {
            if (p->handle == prev)
            {
                //printf("RemoveProcess: node=%d h=%d res=%p\n", node, h, p); // ***
                d->ProcessHashTable[k] = TOMBSTONE;
                return;
            }
        }
        n += 1;
        if (n >= ProcessHashTableSize)
        {
            break;  /* hash table must be full */
        } 
        
        k += 1;
        if (k >= ProcessHashTableSize) 
        {
            k = 0;
        }
    }
    
    Runtime_Error(224, "Process missing in hash table node=%d handle=%d\n", node, prev);
}

/* --------------------------------------------------------- */
int GetLocalClock(int n)
{
    struct NodeInfo *d;

    d = FindNode(n);
    return d->SystemTicks;
}

/* --------------------------------------------------------- */
float TicksToTime(unsigned long long int t)  /* convert ticks to seconds */
{
    return (float) t / (float) CLOCK_FREQUENCY;
}

/* --------------------------------------------------------- */
unsigned long long int TimeToTicks(float t)  /* convert time (seconds) to ticks */
{
    return (unsigned long long int) (t * (float) CLOCK_FREQUENCY);
}

/* --------------------------------------------------------- */
void PrintStatistics()
{
    struct timeval         tv;
    unsigned long long int t2;
    unsigned long long int StandbyTicks;

    printf("Workspace: %d bytes\n", workspace);
    
    gettimeofday(&tv, NULL);
    t2 = tv.tv_sec * 1000000LL + tv.tv_usec;
    printf("Execution time: %f secs\n", (float) (t2 - t1) / 1.0E6);
    
    printf("Computing ticks: %llu (%f s)\n", ProcessingTicks, TicksToTime(ProcessingTicks));
    StandbyTicks = TotalTicks - ProcessingTicks;
    printf("Standby ticks: %llu (%f s) %6.2f%%\n", StandbyTicks, TicksToTime(StandbyTicks), 
            100.0 * (double) StandbyTicks / (double) TotalTicks); 
    printf("Average Search Length in Node List %f\n", AverageSearches);
}

/* --------------------------------------------------------- */
void StackPush(int x)
{
    struct PCB *p;
    int UpperLimit;

    p = CurrentNode->CurrentProcess;
    UpperLimit = p->stacksize;

    if (CurrentNode->SP >= UpperLimit)
    {
        Runtime_Error(228, "Stack overflow (%d)\n", UpperLimit);
    }
    else
    {
        CurrentNode->SP += 1;
        CurrentNode->S[CurrentNode->SP] = x;
    }
}

/* --------------------------------------------------------- */
int StackPop()
{
    int LowerLimit = 1;

    if (CurrentNode->SP < LowerLimit)
    {
        Runtime_Error(229, "Stack underflow\n");
        return 0;
    }
    else
    {
        CurrentNode->SP -= 1;
        return CurrentNode->S[CurrentNode->SP + 1];
    }
}

/* --------------------------------------------------------- */
int StackTop()
{
    return CurrentNode->S[CurrentNode->SP];
}

/* --------------------------------------------------------- */
void StackPushbool(bool Item)
{
    if (Item)
    {
        StackPush(1);
    }
    else
    {
        StackPush(0);
    }
}

/* --------------------------------------------------------- */
bool StackPopbool()
{
    return StackPop() != 0;
}

/* --------------------------------------------------------- */
struct tnode *FindLink(unsigned int n, struct tnode *p)
{
    if (p == NULL)
    {
        return NULL;
    }
    else
    {
        if (n < p->node)
        {
            return FindLink(n, p->left);
        }
        else if (n > p->node)
        {
            return FindLink(n, p->right);
        }
        else
        {
            return p;
        }
    }
}

/* --------------------------------------------------------- */
void SendPkt(unsigned int SourceNode, unsigned int port, int DataValue)
{
    int             i;
    struct tnode    *p;
    struct NodeInfo *d;
    struct NodeInfo *s;
  
    if (port > 2047)
    {
        Runtime_Error(3, "Invalid port (%d)\n", port);
    }
    
    //printf("SendPkt: sourcenode=%d port=%d data=%d\n", SourceNode, port, DataValue); // ***
    s = FindNode(SourceNode);
    p = FindLink(SourceNode, Links);
    if (p == NULL)
    {
        return;
        //Runtime_Error(230, "Unknown source node (%d)\n", SourceNode);
    }

    for (i=1; i<=p->ndest; i+=1)
    {
        d = FindNode(p->destinations[i]);
        if (d != NULL)
        {
            //dont interrupt blocked cores
            if (!d->SyncWait){
                Interrupt(p->destinations[i], (SourceNode << 11) + port, DataValue);
                Reschedule(p->destinations[i]);
                ShowPkt((SourceNode << 11) + port, p->destinations[i], DataValue, s->SystemTicks);
            }
        }
    }

    s->PktsTX += 1;
}

/* --------------------------------------------------------- */
void WriteTimeStamp(int n)
{
    struct NodeInfo *d;

    d = FindNode(n);
    
    if (TimeStamping == On)
    {
        printf("%f: ", TicksToTime(d->SystemTicks));
    }
}

/* --------------------------------------------------------- */
void ShowPkt(unsigned int snode, unsigned int dnode, int pkt, unsigned int tstamp)
{
    if (Monitoring)
    {
        WriteTimeStamp(snode >> 11);
        printf(" %d->%d port %d [%d] Rx:%d\n", snode >> 11, dnode, snode & 0x7ff, pkt, tstamp);
    }
}

/* --------------------------------------------------------- */
int ConvertToFloat(int x)
{
    return x * 65536;
}

/* --------------------------------------------------------- */
int ConvertToInt(int x)
{
    return (int) ((float) x / 65536.0);
}

/* --------------------------------------------------------- */
void DiadicOp(unsigned int Op)
{
    int           x1, x2;
    long long int r;
    
    x1 = StackPop();
    x2 = StackPop();
    
    switch (Op)
    {
    case s_OR:
        StackPushbool(x1 || x2);
        break;
    case s_AND:
        StackPushbool(x1 && x2);
        break;
    case s_PLUS:
        StackPush(x1 + x2);
        break;
    case s_MINUS:
        StackPush(x2 - x1);
        break;

    case s_MULT:
        if (ArithmeticChecking)
        {
            r = (long long int) abs(x1) * (long long int) abs(x2);
            if (r > 0x7fffffffLL)
            {
                Runtime_Error(1001, "Integer multiply overflow (%d*%d)\n", x1, x2);
            }
        }
        StackPush(x1 * x2);
        break;

    case s_MULTF:
        {
            int ax1 = abs(x1);
            int ax2 = abs(x2);
			
            r = (((long long int) ax1 * (long long int) ax2) + 32768LL) / 65536LL;  /* rounding added 23/1/13 */
            if (ArithmeticChecking)
            {
			    if (r > 0x7fffffffLL)
                {
                    Runtime_Error(1002, "Floating multiply overflow (%f*%f)\n", (float) x1 / 65536.0, (float) x2 / 65536.0);
                }
                if (ax1 > 0 && ax2 > 0 && r == 0)
                {
                    Runtime_Error(1003, "Floating multiply underflow (%f*%f)\n", (float) x2 / 65536.0, (float) x1 / 65536.0);
                }
			}
            if ((x1 ^ x2) < 0)
            {
                r = -r;
            }
        }
        StackPush((int) r);
        break;

    case s_DIV:
        if (x1 < 0)
        {
            x1 = -x1;
            x2 = -x2;
        }
        if (x1 == 0)
        {
            Runtime_Error(10, "Integer division by zero (%d/0)\n", x2);
        }
        StackPush(x2 / x1);
        break;

    case s_DIVF:
        {
            int ax1 = abs(x1);
            int ax2 = abs(x2);
            int dx1 = ax1 / 2;
            
            if (x1 == 0)
            {
                Runtime_Error(11, "Floating division by zero (%f/0.0)\n", (float) x2 / 65536.0);
            }
            r = (((long long int) ax2 * 65536LL + (long long int) dx1) / (long long int) ax1);  /* rounding added 23/1/13 */
            if (ArithmeticChecking)
            {
			    if (r > 0x7fffffffLL)
                {
                    Runtime_Error(1004, "Floating division overflow (%f/%f)\n", (float) x2 / 65536.0, (float) x1 / 65536.0);
                }
                if (ax2 > 0 && r == 0)
                {
                    Runtime_Error(1005, "Floating division underflow (%f/%f)\n", (float) x2 / 65536.0, (float) x1 / 65536.0);
                }
			}
            if ((x1 ^ x2) < 0)
            {
                r = -r;
            }
        }
        StackPush((int) r);
        break;

    case s_REM:
        StackPush(x2 % x1);
        break;
    case s_LOGAND:
        StackPush(x2 & x1);
        break;
    case s_LOGOR:
        StackPush(x2 | x1);
        break;
    case s_NEQV:
        StackPush(x2 ^ x1);
        break;
    case s_LSHIFT:
        StackPush(x2 << x1);
        break;
    case s_RSHIFT:
        StackPush(x2 >> x1);
        break;
    case s_EQ:
        StackPushbool(x1 == x2);
        break;
    case s_NE:
        StackPushbool(x1 != x2);
        break;
    case s_LS:
        StackPushbool(x1 > x2);
        break;
    case s_GR:
        StackPushbool(x1 < x2);
        break;
    case s_LE:
        StackPushbool(x1 >= x2);
        break;
    case s_GE:
        StackPushbool(x1 <= x2);
        break;
    }
}

/* --------------------------------------------------------- */
void MonadicOp(unsigned int Op)
{
    int x;

    x = StackPop();
    
    switch (Op)
    {
    case s_NEG:
        StackPush(-x);
        break;
        
    case s_ABS:
        if (x < 0)
        {
            x = -x;
        }
        StackPush(x);
        break;
        
    case s_COMP:
        StackPush(~x);
        break;
        
    case s_NOT:
        if (x != 0)
        {
            x = 0;
        }
        else
        {
            x = 1;
        }
        StackPush(x);
        break;
        
    case s_FLOAT:
        StackPush(x * 65536);
        break;
        
    case s_INT:
        if (x >= 0)
        {
            StackPush((int) (((float) x / 65536.0) + 0.5));
        }
        else
        {
            StackPush((int) (((float) x / 65536.0) - 0.5));
        }
        break;
    }
}

/* --------------------------------------------------------- */
void myprintf(char *fmt, ...)
{
    va_list  list;
    char         *p, *r;
    int          e;
    double       x;
    char         c;
    long int     u;
    char         str[50];
    unsigned int q;
    char         t;
    bool      sfound;
    
    va_start(list, fmt);

    for (p=fmt; *p; ++p)
    {
        if (*p != '%')
        {
            printf("%c", *p);
        }
        else
        {
            q = 1;
            sfound = false;
            str[0] = '%';
            
            do
            {
                t = *++p;
                str[q] = t;
                q += 1;
                
                switch (t)
                {
                    case 'd': case 'i': case 'o': case 'x': case 'X': case 'u': case 'c':
                    case 's': case 'f': case 'e': case 'E': case 'g': case 'G': case 'p': case '%':
                        str[q] = '\0';
                        sfound = true;
                        break;
                    
                    default:
                        break;
                }
            } while (!sfound);
            
            switch(t)
            {
                case 'd': case 'i': case 'o': case 'x': case 'X': case 'u': 
                    e = va_arg(list, int);
                    printf(str, e);
                    continue;
                
                case 'c':
                    c = va_arg(list, int);
                    printf(str, c);
                    continue;
                    
                case 's':
                    r = va_arg(list, char *);
                    printf(str, r);
                    continue;
                    
                case 'f': case 'e': case 'E': case 'g': case 'G': 
                    x = (double) va_arg(list, int) / 65536.0;
                    printf(str, x);
                    continue;

                case 'p':
                    u = (long int) va_arg(list, void *);
                    printf(str, u);
                    continue;

                case '%':
                    printf("%%");
                    continue;
                
                default:
                    printf("%c", *p);
            }
        }
    }
    va_end(list);
}

/* --------------------------------------------------------- */
void myfprintf(FILE *stream, char *fmt, ...)
{
    va_list  list;
    char         *p, *r;
    int          e;
    double       x;
    char         c;
    long int     u;
    char         str[50];
    unsigned int q;
    char         t;
    bool      sfound;
    
    va_start(list, fmt);

    for (p=fmt; *p; ++p)
    {
        if (*p != '%')
        {
            fprintf(stream, "%c", *p);
        }
        else
        {
            q = 1;
            sfound = false;
            str[0] = '%';
            
            do
            {
                t = *++p;
                str[q] = t;
                q += 1;
                
                switch (t)
                {
                    case 'd': case 'i': case 'o': case 'x': case 'X': case 'u': case 'c':
                    case 's': case 'f': case 'e': case 'E': case 'g': case 'G': case 'p': case '%':
                        str[q] = '\0';
                        sfound = true;
                        break;
                    
                    default:
                        break;
                }
            } while (!sfound);
            
            switch(t)
            {
                case 'd': case 'i': case 'o': case 'x': case 'X': case 'u': 
                    e = va_arg(list, int);
                    fprintf(stream, str, e);
                    continue;
                
                case 'c':
                    c = va_arg(list, int);
                    fprintf(stream, str, c);
                    continue;
                    
                case 's':
                    r = va_arg(list, char *);
                    fprintf(stream, str, r);
                    continue;
                    
                case 'f': case 'e': case 'E': case 'g': case 'G': 
                    x = (double) va_arg(list, int) / 65536.0;
                    fprintf(stream, str, x);
                    continue;

                case 'p':
                    u = (long int) va_arg(list, void *);
                    fprintf(stream, str, u);
                    continue;

                case '%':
                    fprintf(stream, "%%");
                    continue;
                
                default:
                    fprintf(stream, "%c", *p);
            }
        }
    }
    va_end(list);
}

void InsertLinkedNode(struct NodeInfo *n, struct NodeInfo *prev){


	//insert
	if (prev != NULL){
		n->PrevNode = prev;
		n->NextNode = prev->NextNode;
		if (prev->NextNode != NULL)
			prev->NextNode->PrevNode = n;
		else
			NodeListTail = n;
		prev->NextNode = n;
	}else {
		// if no prev node then this node should be the front of the queue
		if (NodeList != NULL)
			NodeList->PrevNode = n;
		n->NextNode = NodeList;
		n->PrevNode = NULL;
		NodeList = n;
	}

	//if NodeListTail is NULL then this is the only node so is both the front and back of the queue
	if (NodeListTail == NULL)
		NodeListTail = n;

}


void ReorderLinkedListItem(struct NodeInfo *n)
{
	struct LimitItem *i;
	struct LimitItem *l; //possible new limit item
	int c = 0;

	RemoveLinkedNode(n);

	//find insertion point in limit list
	i = LimitListTail;

	//find insertion point in linked limit list
	while (i != NULL){
		c++; //count number of lookups
		if (i->Value == n->SystemTicks){
			//add node
			InsertLinkedNode(n, i->LastNode);
			//add limit item to end of existing limit
			l = i;
			l->LastNode = n;
			l->items++;
			n->LimitItem = l;
			break;
		}
		else if (i->Value < n->SystemTicks){
			//add node
			InsertLinkedNode(n, i->LastNode);
			//insert new limit
			l = (struct LimitItem*)malloc(sizeof(struct LimitItem));
			l->LastNode = n;
			l->Value = n->SystemTicks;
			l->items = 1;
			n->LimitItem = l;

			//insert limit into linked limit list
			l->PrevItem = i;
			l->NextItem = i->NextItem;
			if (i->NextItem != NULL)
				i->NextItem->PrevItem = l;
			else
				LimitListTail = l;
			i->NextItem = l;
			break;

		}
		i = i->PrevItem;
	}
	// if NULL then this limit item should be the front linked list
	if (i == NULL){
		//add node
		InsertLinkedNode(n, NULL);
		//insert new limit
		l = (struct LimitItem*)malloc(sizeof(struct LimitItem));
		l->LastNode = n;
		l->Value = n->SystemTicks;
		l->items = 1;
		n->LimitItem = l;

		//update head of linked list
		if (LimitList != NULL)
			LimitList->PrevItem = l;
		l->NextItem = LimitList;
		l->PrevItem = NULL;
		LimitList = l;
	}

	//if NodeListTail is NULL then this was the only node so is both the front and back of the queue
	if (LimitListTail == NULL)
		LimitListTail = l;

	//Running average of lookups
	CountSearches += 1;
	AverageSearches = ((float)c+ ((float)CountSearches*AverageSearches))/((float)CountSearches+1.0f);
}

void RemoveLinkedNode(struct NodeInfo *n){

	struct LimitItem* CurrentLimit;

	//remove node from linked list
	if (n->PrevNode != NULL)
		n->PrevNode->NextNode = n->NextNode;
	if (n->NextNode != NULL)
		n->NextNode->PrevNode = n->PrevNode;
	if (n == NodeList)
		NodeList = n->NextNode;
	if (n == NodeListTail)
		NodeListTail = n->PrevNode;

	//if node is tail of the limit list then update the limit list
	CurrentLimit = n->LimitItem;
	CurrentLimit->items --;
	if (CurrentLimit->LastNode == n){
		if (CurrentLimit->items == 0){ //last item in limit so delete
			RemoveLinkedLimit(CurrentLimit);
			free(CurrentLimit);
		}else{	//update last item to node previous
			CurrentLimit->LastNode = n->PrevNode;
			n->LimitItem = NULL;
		}
	}
}

void RemoveLinkedLimit(struct LimitItem *i){
	//remove limit from linked list
	if (i->PrevItem != NULL)
		i->PrevItem->NextItem = i->NextItem;
	if (i->NextItem != NULL)
		i->NextItem->PrevItem = i->PrevItem;
	if (i == LimitList)
		LimitList = i->NextItem;
	if (i == LimitListTail)
		LimitListTail = i->PrevItem;
}

/* --------------------------------------------------------- */
void Emulate(bool debugging, bool archecking)
{
    unsigned int           Op  = 0;
    int                    Arg = 0;
    unsigned int           i;
    struct NodeInfo        *h;
    unsigned int           phandle;
    struct PCB             *d;
    unsigned int           timeout;
    struct timeval         tv;
    unsigned int           *ProcList = NULL;
    
    const int InstructionTime[68] =
    {
        0, /*                0  */
        1, /* s_LG           1  */
        1, /* s_LP           2  */
        1, /* s_LN           3  */
        1, /* s_LSTR         4  */
        1, /* s_LL           5  */
        1, /* s_LLG          6  */
        1, /* s_LLP          7  */
        1, /* s_LLL          8  */
        1, /* s_EQ           9  */
        1, /* s_NE          10  */
        1, /* s_LS          11  */
        1, /* s_GR          12  */
        1, /* s_LE          13  */
        1, /* s_GE          14  */
        1, /* s_JT          15  */
        1, /* s_JF          16  */
        1, /* s_JUMP        17  */
        1, /* s_OR          18  */
        1, /* s_AND         19  */
        1, /* s_PLUS        20  */
        1, /* s_MINUS       21  */
        1, /* s_MULT        22  */
       11, /* s_MULTF       23  */
       57, /* s_DIV         24  */
      129, /* s_DIVF        25  */
       57, /* s_REM         26  */
        1, /* s_NEG         27  */
        1, /* s_NOT         28  */
        1, /* s_ABS         29  */
        1, /* s_SG          30  */
        1, /* s_SP          31  */
        1, /* s_SL          32  */
        1, /* s_SYSCALL     33  */
        1, /* s_LOGAND      34  */
        1, /* s_LOGOR       35  */
        1, /* s_NEQV        36  */
        1, /* s_LSHIFT      37  */
        1, /* s_RSHIFT      38  */
        1, /* s_COMP        39  */
        1, /* s_SWITCHON    40  */
        0, /* s_FNAP        41  */
        0, /* s_RTAP        42  */
        1, /* s_RTRN        43  */
        1, /* s_ENTRY       44  */
        1, /* s_RV          45  */
        1, /* s_STIND       46  */
        1, /* s_PUSHTOS     47  */
        1, /* s_VCOPY       48  */
        1, /* s_FLOAT       49  */
        1, /* s_INT         50  */
        1, /* s_SWAP        51  */
        0, /* s_DEBUG       52  */
        0, /* s_BOUNDSCHECK 53  */
        0, /* spare         54  */
        0, /* spare         55  */
        0, /* spare         56  */
        0, /* spare         57  */
        0, /* spare         58  */
        0, /* spare         59  */

        0, /* s_STACK       60  */
        0, /* s_QUERY       61  */
        0, /* s_STORE       62  */
        0, /* s_SAVE        63  */
        0, /* s_RES         64  */
        0, /* s_RSTACK      65  */
        0, /* s_LAB         66  */
        0, /* s_DISCARD     67  */
    };

    ArithmeticChecking = archecking;
    
    if (debugging)
    {
        Debug_Init(NameNodeList, NodeList, NumberOfLines, LineNumberList);
    }
    
    ProcessingTicks = 0;
    TotalTicks = 0;
    
    if (NumberOfNodes < 5)
    {
         HashTableSize = 10;
    }
    else
    {
        HashTableSize = NumberOfNodes * 2;
    }
    
    HashTable = malloc(sizeof(int) * HashTableSize);
    if (HashTable == NULL)
    {
        Runtime_Error(231, "Unable to allocate hash table\n");
    }
    
    for (i=0; i<HashTableSize; i+=1)
    {
        HashTable[i] = NULL;
    }
    
    if (ProfileNode != 0)
    {
        h = NodeList;
        while (h != NULL)
        {
            if (h->NodeNumber == ProfileNode)
            {
                unsigned int p = 0;
                
                ProcList = malloc(sizeof(unsigned int) * (h->ProgramSize + 1));
                if (ProcList == NULL)
                {
                    Runtime_Error(232, "Unable to allocate procedure table\n");
                }
                for (i=1; i<=h->ProgramSize; i+=1)
                {
                    if (h->Instructions[i].Op == s_ENTRY)
                    {
                        p = h->Instructions[i].Arg;
                    }
                    ProcList[i] = p;
                }
            }
            h = h->NextNode;
        }
    }
    
    //create main process for each node
    h = NodeList;
    while (h != NULL)
    {
        AddNode(h->NodeNumber, h);

        phandle = CreateProcess(h->NodeNumber, h->PC, StackSize, 0);
        if (phandle == 0)
        {
            Runtime_Error(232, "Cannot create <main> process\n");
        }
        d = FindProcess(h->NodeNumber, phandle);
        
        h->CurrentProcess = d;
        h->SP = d->saved_SP;
        h->FP = d->saved_FP;
        h->S  = d->stack;
        h->SP += 1;
        h->S[h->SP] = 0;  /* push dummy FP for return from main() */
        h->SP += 1;
        h->S[h->SP] = 0;  /* push dummy return addr 0 to trap return from main() */
        h = h->NextNode;
    }

    //initialise the timer
    gettimeofday(&tv, NULL);
    t1 = tv.tv_sec * 1000000LL + tv.tv_usec;
    timeout = 0;
    sync_count = 0;

    //construct Limit List (all items are within the same limit as all SystemtTicks are at 0)
    LimitList = (struct LimitItem*)malloc(sizeof(struct LimitItem));
    LimitList->NextItem = NULL;
    LimitList->PrevItem = NULL;
    LimitList->Value = 0;
    LimitList->LastNode = NodeListTail;
    LimitList->items = NumberOfNodes;
    LimitListTail = LimitList;
    h = NodeList; //set all nodes within current limit list
    while (h != NULL)
    {
        h->LimitItem = LimitList;
        h = h->NextNode;
    }

    //Main execution loop
    while (1)
    {
        //get the next node (front of the queue)
        CurrentNode = NodeList;

        if (CurrentNode->SystemTicks >= CurrentNode->LastClockTick + CurrentNode->Tickrate)
        {

            //interrupt if not blocked
            if (!CurrentNode->SyncWait){
                d = CurrentNode->ProcessList;
                while (d != NULL)
                {
                    if (d->status == Delaying)
                    {
                        d->dticks -= 1;
                    }
                    d = d->nextPCB;
                }
                UpdateLog(CurrentNode->NodeNumber, true);
                Interrupt(CurrentNode->NodeNumber, 0, 0);
                Reschedule(CurrentNode->NodeNumber);
            }
            CurrentNode->LastClockTick += CurrentNode->Tickrate;
        }

        if (CurrentNode->DMATicks > 0)
        {
            CurrentNode->DMATicks -= 1;
            if ((CurrentNode->DMATicks == 0)&&(!CurrentNode->SyncWait))
            {
                Reschedule(CurrentNode->NodeNumber);
            }
        }


        if (CurrentNode->CurrentProcess == NULL)
        {
            if (CurrentNode->DMATicks == 0)
            {
               CurrentNode->SystemTicks = CurrentNode->LastClockTick + CurrentNode->Tickrate;  // jump ahead to next clk tick
               ReorderLinkedListItem(CurrentNode);
            }
            timeout += 1;
            
            if (timeout > 1000000)
            {
                if (debugging)
                {
                    Debug_End();
                }
                printf("Timeout:\n");
                free(LineNumberList);
                free(HashTable);
                if (ProcList != NULL)
                {
                    free(ProcList);
                }
                CloseLogs();
                PrintStatistics();
                return;
            }
            continue;
        }
        else
        {
            timeout = 0;
        }
        


        if (CurrentNode->PC < 1 || CurrentNode->PC > CurrentNode->ProgramSize)
        {
            Runtime_Error(233, "PC out of range node=%d PC=%d\n", CurrentNode->NodeNumber, CurrentNode->PC);
        }

        FetchInstruction(&Op, &Arg);
        
        CurrentNode->SystemTicks += InstructionTime[Op];
        ProcessingTicks += InstructionTime[Op];
        if (ProfileNode != 0)
        {
            CurrentNode->Procedures[ProcList[CurrentNode->PC]].TickCount += InstructionTime[Op];
        }
        
        if (diagnostics)
        { 
            h = NodeList;
            while (h != NULL)
            {
                printf("n%d: ", h->NodeNumber);
                d = h->ProcessList;
                while(d != NULL)
                {
                    char c;
                    
                    switch (d->status)
                    {
                        case Running:  c = 'R'; break;
                        case Waiting:  c = 'W'; break;
                        case Delaying: c = 'D'; break;
                        default:       c = '?'; break;
                    }
                    printf("%c", c);
                    d = d->nextPCB;
                }
                printf(" ");
                h = h->NextNode;
            }
            printf("\nNode=%d Process=%p PC=%d Op=%d FP=%d SP=%d ", CurrentNode->NodeNumber, 
                   CurrentNode->CurrentProcess, 
                   CurrentNode->PC, Op, CurrentNode->FP, CurrentNode->SP);
            for (i=CurrentNode->FP; i<=CurrentNode->SP; i+=1)
            {
                printf(" s[%d]=%d", i, CurrentNode->S[i]);
            }
            printf("\n");
            printf(" Clk=%llu\n", CurrentNode->SystemTicks);
        }
        
        ExecuteInstruction(Op, Arg);

        //as long as the instruction was not exit then reorder node list
        if ((Op != s_SYSCALL)&&(Arg != 4)){
        	ReorderLinkedListItem(CurrentNode);
        }

        if (NumberOfNodes == 0)
        {
            if (debugging)
            {
                Debug_End();
            }
            free(LineNumberList);
            free(HashTable);
            if (ProcList != NULL)
            {
                free(ProcList);
            }
            CloseLogs();
            PrintStatistics();
            return;
        }
    }
}

/* --------------------------------------------------------- */
void FetchInstruction(unsigned int *Op, int *Arg)
{
    *Op  = CurrentNode->Instructions[CurrentNode->PC].Op;
    *Arg = CurrentNode->Instructions[CurrentNode->PC].Arg;
}

/* --------------------------------------------------------- */
void ExecuteInstruction(unsigned int Op, int Arg)
{
    struct NodeInfo        *h;
    int                    *a;
    int                    x = 0;
    int                    y = 0;
    int                    *vdest;   /* copying const array */
    int                    *vsource;
    unsigned int           vsize;
    unsigned int           lab;
    int                    n = 0;
    int                    d = 0;
    unsigned int           oldFP;
    unsigned int           returnaddress;
    unsigned int           i;
    unsigned int           j;
    int                    k;
    unsigned int           size;
    unsigned char          *v;
    unsigned int           nArgs;
    int                    Args[30];
    struct PCB             *p;
    unsigned int           handle;
    unsigned int           lobound;
    unsigned int           hibound;
    
    h = CurrentNode;
    
    if (TraceMode)
    {
        Debug(h->NodeNumber, h->PC);
    }
    
    switch (Op)
    {
    case s_LG:
        StackPush(h->G[Arg]);
        h->PC += 1;
        break;

    case s_LP:
        StackPush(h->S[h->FP + Arg]);
        h->PC += 1;
        break;

    case s_RV:
        a = (int *) StackPop();
        StackPush(*a);
        h->PC += 1;
        break;

    case s_LLP:
        StackPush((int) &h->S[h->FP + Arg]);
        h->PC += 1;
        break;

    case s_LSTR:
    case s_LLG:
        StackPush((int) &h->G[Arg]);
        h->PC += 1;
        break;

    case s_LLL:
        StackPush(GetNodeLabel(h->NodeNumber, Arg));
        h->PC += 1;
        break;

    case s_STIND:
        a = (int *) StackPop();
        Arg = StackPop();
        *a = Arg;
        h->PC += 1;
        break;
        
    case s_SG:
        h->G[Arg] = StackPop();
        h->PC += 1;
        break;

    case s_SP:
        h->S[h->FP + Arg] = StackPop();
        h->PC += 1;
        break;

    case s_LN:
        StackPush(Arg);
        h->PC += 1;
        break;

    case s_PUSHTOS:
        StackPush(StackTop());
        h->PC += 1;
        break;

    case s_SWAP:
        x = StackPop();
        y = StackPop();
        StackPush(x);
        StackPush(y);
        h->PC += 1;
        break;
        
    case s_VCOPY:
        vsize = Arg;
        vsource = (int *) StackPop();
        vdest = (int *) StackPop();
        memcpy(vdest, vsource, vsize);
        h->PC += 1;
        break;
        
    case s_EQ:
    case s_NE:
    case s_LS:
    case s_GR:
    case s_LE:
    case s_GE:    
        DiadicOp(Op);
        h->PC += 1;
        break;

    case s_JT:
        if (StackPopbool())
        {
            h->PC = h->Labels[Arg];
        }
        else
        {
            h->PC += 1;
        }
        break;

    case s_JF:
        if (StackPopbool())
        {
            h->PC += 1;
        }
        else
        {
            h->PC = h->Labels[Arg];
        }
        break;

    case s_SWITCHON:
        x = StackPop();
        n = Arg;
        h->PC += 1;
        d = h->Instructions[h->PC].Arg;
        lab = 0;
        
        for (i=1; i<=n; i+=1)
        {
            h->PC += 1;
            k = h->Instructions[h->PC].Arg;
            h->PC += 1;
            if (x == k)
            {
                lab = (unsigned int) h->Instructions[h->PC].Arg;
                break;
            }
        }

        if (lab !=0)
        {
            h->PC = h->Labels[lab];
        }
        else if (d !=0)
        {
            h->PC = h->Labels[d]; 
        }
        else
        {
            h->PC += 1;
        }
        break;

    case s_RES:
    case s_JUMP:
        h->PC = h->Labels[Arg];
        break;

    case s_OR: case s_AND: case s_PLUS: case s_MINUS: case s_MULT:
    case s_MULTF: case s_DIV: case s_DIVF: case s_REM:
    case s_LOGAND: case s_LOGOR: case s_NEQV: case s_LSHIFT: case s_RSHIFT:
        DiadicOp(Op);
        h->PC += 1;
        break;

    case s_NEG: case s_NOT: case s_COMP: case s_FLOAT: case s_INT: case s_ABS:
        MonadicOp(Op);
        h->PC += 1;
        break;

    case s_ENTRY:
        oldFP = h->FP;
        returnaddress = StackPop();
        h->FP = h->SP - h->Procedures[Arg].nArgs;

        if (h->NodeNumber == ProfileNode)
        {
            h->Procedures[Arg].CallCount += 1;
        }
        
        for (i=h->Procedures[Arg].nArgs+1; i<=h->Procedures[Arg].nLocals; i+=1)
        {
            size = 1;
            if (h->Procedures[Arg].Args[i].vDimensions[0] > 0)
            {
                for (j=1; j<=h->Procedures[Arg].Args[i].vDimensions[0]; j+=1)
                {
                    size = size * h->Procedures[Arg].Args[i].vDimensions[j];
                }
            }
            for (j=0; j<size; j+=1)
            {
                h->S[h->FP+h->Procedures[Arg].Args[i].vOffset+j] = 0;
            }
        }

        h->SP = h->FP + h->Procedures[Arg].BP;
        StackPush(returnaddress);
        StackPush(oldFP);
        h->PC += 1;
        break;

    case s_FNAP:
    case s_RTAP:
        lab = StackPop();
        StackPush(h->PC + 1);
        h->PC = GetNodeLabel(h->NodeNumber, lab);
        break;

    case s_RTRN:
        if (h->Procedures[Arg].ProcType != VoidType)
        {
            x = StackPop();   /* remember result */
        }
        h->FP = StackPop();
        returnaddress = StackPop();
        h->SP = h->SP - h->Procedures[Arg].BP;
        if (h->Procedures[Arg].ProcType != VoidType)
        {
            StackPush(x);  /* leave result on top of stack */
        }
        h->PC = returnaddress;
        if (returnaddress == 0)  /* must be return from process */
        {
            DeleteProcess(h->NodeNumber, h->CurrentProcess->handle);
            Reschedule(h->NodeNumber);
        }
        break;

    case s_LBOUNDSCHECK:
        hibound = StackPop();
        lobound = StackPop();
        hibound += lobound;
        if (StackTop() < lobound || StackTop() >= hibound)
        {
            Runtime_Error(2, "array bound %s[%d]\n", NodeLocalName(h, h->PC, Arg), (StackTop() - lobound) / bpw);
            //Runtime_Error(2, "array bound %s[%d] %d:%d:%d\n", NodeLocalName(h, h->PC, Arg), (StackTop() - lobound) / bpw, lobound, hibound, StackTop());
        } 
        h->PC += 1;
        break;
        
    case s_GBOUNDSCHECK:
        hibound = StackPop();
        lobound = StackPop();
        hibound += lobound;
        if (StackTop() < lobound || StackTop() >= hibound)
        {
            Runtime_Error(2, "array bound %s[%d]\n", NodeGlobalName(h, Arg), (StackTop() - lobound) / bpw);
            //Runtime_Error(2, "array bound %s[%d] %d:%d:%d\n", NodeGlobalName(h, Arg), (StackTop() - lobound) / bpw, lobound, hibound, StackTop());
        } 
        h->PC += 1;
        break;
        
    case s_DEBUG:
        Debug(h->NodeNumber, h->PC);
        break;
        
    case s_DISCARD:  /* only needed by interpreter if no result is returned by a function */
        Arg = StackPop();
        h->PC += 1;
        break;
        
    case s_STACK:  /* ignore */
    case s_QUERY:
    case s_STORE:
    case s_SAVE:
    case s_RSTACK:
    case s_LAB:
        h->PC += 1;
        break;
            
    case s_SYSCALL:
        Arg = StackPop();
        nArgs = StackPop();

        for (i=nArgs; i>=1; i-=1)
        {
            Args[i] = StackPop();
        }

        switch (Arg)
        {
            case 1:  /* sendpkt */
                UpdateLog(h->NodeNumber, false);
                h->PC += 1;
                SendPkt(h->NodeNumber, Args[1], Args[2]);
                break;

            case 2:  /* delay */
            	//PR: HACK TO AVOID DISCREPANCY WITH RUNTIME SYSTEM DELAYS
                h->CurrentProcess->dticks = (unsigned int) (TimeToTicks(Args[1] / 65536.0F) / (unsigned long long int) h->Tickrate) +1;
                //printf("delay: %u\n", h->CurrentProcess->dticks);
                h->CurrentProcess->status = Delaying; 
                h->PC += 1;
                Reschedule(h->NodeNumber);
                break;

            case 3:  /* printf */
                WriteTimeStamp(h->NodeNumber);
                printf("%d   ", h->NodeNumber);
                myprintf((char *) Args[1], Args[2], Args[3], Args[4], Args[5], Args[6], Args[7], Args[8], Args[9], Args[10], Args[11]);
                h->PC += 1;
                break;

            case 4:  /* exit */
                printf("Node (%d) Exit %d\n", h->NodeNumber, Args[1]);

                while (h->ProcessList != NULL)  /* remove all processes */
                {
                    DeleteProcess(h->NodeNumber, h->ProcessList->handle);
                }
                DeleteNode(h->NodeNumber);
                CurrentNode = NULL;
                break;
            
            case 5:  /* signal */
                a = (int *) Args[1];
                *a += 1;
                h->PC += 1;
                Reschedule(h->NodeNumber);
                break;

            case 6:  /* wait */
                a = (int *) Args[1];
                if (*a > 0)
                {
                    *a -= 1;
                    h->PC += 1;
                }
                else
                {
                    h->PC += 1;
                    h->CurrentProcess->status = Waiting;
                    h->CurrentProcess->semaphore = a;
                    Reschedule(h->NodeNumber);
                }
                break;

            case 7:  /* tickrate */
                h->Tickrate = CLOCK_FREQUENCY / Args[1];
                h->PC += 1;
                break;
            
            case 8: /* putbyte */
                v = (unsigned char *) Args[1];
                v[Args[2]] = (unsigned char) Args[3];
                h->PC += 1;
                break;

            case 9: /* putword */
                a = (int *) Args[1];
                a[Args[2]] = Args[3];
                h->PC += 1;
                break;

            case 10: /* readsdram */
                //printf("readsdram: a1=%d, a2=%d a3=%d\n", Args[1], Args[2], Args[3]);
                if ((Args[1] / sizeof(int) + Args[3] - 1) > h->ExternalVectorSize)
                {
                    Runtime_Error(1, "readsdram bounds error (%d:%d)\n", Args[1] / sizeof(int) + Args[3] - 1, h->ExternalVectorSize);
                }
                vsource = &h->E[Args[1] / sizeof(int)];
                vdest = (int *) Args[2];
                memcpy(vdest, vsource, sizeof(int) * Args[3]);
                h->DMATicks = Args[3];
                h->CurrentProcess->status = DMATransfer;
                h->PC += 1;
                Reschedule(h->NodeNumber);
                break;
                
            case 11: /* writesdram */
                //printf("writesdram: a1=%d, a2=%d a3=%d\n", Args[1], Args[2], Args[3]);
                if ((Args[1] / sizeof(int) + Args[3] - 1) > h->ExternalVectorSize)
                {
                    Runtime_Error(1, "writesdram bounds error (%d:%d)\n", Args[1] / sizeof(int) + Args[3] - 1, h->ExternalVectorSize);
                }
                vdest = &h->E[Args[1] / sizeof(int)];
                vsource = (int *) Args[2];
                memcpy(vdest, vsource, sizeof(int) * Args[3]);
                h->DMATicks = Args[3];
                h->CurrentProcess->status = DMATransfer;
                h->PC += 1;
                Reschedule(h->NodeNumber);
                break;
                
            case 12: /* syncnodes */
                //printf("syncnodes: %d\n", h->NodeNumber);
                h->PC += 1;
                h->SyncWait = true;
                SyncNodes(h->NodeNumber);
                break;

            case 101:  /* getclk */
                StackPush((unsigned int) (TicksToTime(GetLocalClock(h->NodeNumber)) * 65536.0));
                h->PC += 1;
                break;

            case 102: /* abs */
            case 103: /* fabs */
                if (Args[1] < 0)
                {
                    Args[1] = -Args[1];
                }
                StackPush(Args[1]);
                h->PC += 1;
                break;

            case 104:  /* createprocess */
                handle = CreateProcess(h->NodeNumber, Args[1], Args[2], 0);
                p = FindProcess(h->NodeNumber, handle);

                if (nArgs > 2)
                {
                    for (i=3; i<=nArgs; i+=1)
                    {
                        p->saved_SP += 1;
                        p->stack[p->saved_SP] = Args[i];
                    }
                    p->saved_SP += 1;
                    p->stack[p->saved_SP] = 0;  /* return addr 0 */
                }

                StackPush(handle);
                h->PC += 1;
                break;

            case 105:  /* deleteprocess */
                DeleteProcess(h->NodeNumber, Args[1]);
                h->PC += 1;
                Reschedule(h->NodeNumber);
                break;

            case 106:  /* getbyte */
                v = (unsigned char *) Args[1];
                StackPush((int) v[Args[2]]);
                h->PC += 1;
                break;

            case 107:  /* getword */
                a = (int *) Args[1];
                StackPush(a[Args[2]]);
                h->PC += 1;
                break;

            default:
                Runtime_Error(234, "Execute: unknown instruction (%d) at %d\n", Op, h->PC);
                break;
        }
        break;
    }
}

/* --------------------------------------------------------- */
void Tracing(bool mode)
{
    TraceMode = mode;
}

/* --------------------------------------------------------- */
void OpenProfile(unsigned int n, char Filename[])
{
    char fname[MaxStringSize];

    sprintf(fname, "%s_%d.pr", Filename, n);
    ProfileStream = fopen(fname, "w");
    if (ProfileStream == NULL)
    {
        Runtime_Error(123, "Unable to open profile file %s\n", fname);
    }
    fprintf(ProfileStream, "node %d\n  %%time      ticks       time      calls       name\n", n);
}

/* --------------------------------------------------------- */
void CloseProfile()
{
    struct NodeInfo *h;
    unsigned int    i;
    unsigned int    idle;
    
    h = NodeList;
    while (h != NULL)
    {
        if (h->NodeNumber == ProfileNode)
        {
            idle = h->SystemTicks;
            for (i=1; i<=h->NumberOfProcedures; i+=1)
            {
                fprintf(ProfileStream, "%6.2f%% %10u %10.6f %10u %10s\n", (float) h->Procedures[i].TickCount * 100.0 / (float) h->SystemTicks, h->Procedures[i].TickCount,
                        TicksToTime(h->Procedures[i].TickCount), h->Procedures[i].CallCount, h->Procedures[i].Name);
                idle -= h->Procedures[i].TickCount;
            }
            fprintf(ProfileStream, "%6.2f%% %10u %10.6f %21s\n", (float) idle * 100.0 / (float) h->SystemTicks, idle, TicksToTime(idle), "idle");
            break;
        }
        h = h->NextNode;
    }
    
    if (ProfileStream != NULL)
    {
        fclose(ProfileStream);
    }
}

/* --------------------------------------------------------- */
unsigned int OpenLog(unsigned int n, char Filename[], char PrototypeName[], char ChannelName[], char Format[], 
                     unsigned long long int t1, unsigned long long int t2, unsigned long long int t3, bool logging)
{
    FILE *s;
    char fname[MaxStringSize];

    sprintf(fname, "%s_%s_%d_%s.dat", Filename, PrototypeName, n, ChannelName);
	if (LogFiles)
	{
        s = fopen(fname, "w");
        if (s == NULL)
        {
            Runtime_Error(123, "Unable to open log file %s\n", fname);
        }
	}
	else
	{
	    s = NULL;
	}
    LogStreams += 1;
    if (LogStreams > MaxLogChannels)
    {
        Runtime_Error(123, "Too many log files (%d)\n", LogStreams);
    }
    
    strcpy(LogData[LogStreams].format, Format);
    strcpy(LogData[LogStreams].name, fname);
    LogData[LogStreams].stream = s;
    LogData[LogStreams].node = n;
    LogData[LogStreams].noffsets = 0;
    LogData[LogStreams].logmode = logging;
    LogData[LogStreams].lastlog = 0;
    LogData[LogStreams].startwindow = t1;
    LogData[LogStreams].stopwindow = t2;
    LogData[LogStreams].sampleinterval = t3;
   
    return LogStreams;
}

/* --------------------------------------------------------- */
void AddLog(unsigned int chn, unsigned x)
{
    unsigned int n = LogData[chn].noffsets + 1;
	
	if (n >= MaxChannelOffsets)
	{
        Runtime_Error(124, "Too many log offsets (%d)\n", n);
	}
    LogData[chn].offsets[n] = x;
	LogData[chn].noffsets = n;
}

/* --------------------------------------------------------- */
struct LogInfo *GetLog(unsigned int channel)
{
    return &LogData[channel];
}

/* --------------------------------------------------------- */
void CloseLogs()
{
    unsigned int i;

    for (i=1; i<=LogStreams; i+=1)
    {
        if (LogFiles)
		{
    		fclose(LogData[i].stream);
		}
        LogData[i].stream = NULL;
    }
    LogStreams = 0;
}

/* --------------------------------------------------------- */
void UpdateLog(unsigned int n, bool logging)
{
    unsigned int           i;
    struct NodeInfo        *h;
    unsigned long long int t;

    h = FindNode(n);
    
    for (i=1; i<=LogStreams; i+=1)
    {
        t = h->SystemTicks;
        if (LogData[i].node == n && LogData[i].logmode == logging && 
            t >= LogData[i].startwindow && t <= LogData[i].stopwindow &&
            (t >= (LogData[i].lastlog + LogData[i].sampleinterval) || !logging))
        {
            myfprintf(LogData[i].stream, LogData[i].format, h->G[LogData[i].offsets[1]],  h->G[LogData[i].offsets[2]], 
                                                            h->G[LogData[i].offsets[3]],  h->G[LogData[i].offsets[4]],
                                                            h->G[LogData[i].offsets[5]],  h->G[LogData[i].offsets[6]],
                                                            h->G[LogData[i].offsets[7]],  h->G[LogData[i].offsets[8]],
                                                            h->G[LogData[i].offsets[9]],  h->G[LogData[i].offsets[10]],
                                                            h->G[LogData[i].offsets[11]], h->G[LogData[i].offsets[12]],
                                                            h->G[LogData[i].offsets[13]], h->G[LogData[i].offsets[14]],
                                                            h->G[LogData[i].offsets[15]], h->G[LogData[i].offsets[16]],
                                                            h->G[LogData[i].offsets[17]], h->G[LogData[i].offsets[18]],
                                                            h->G[LogData[i].offsets[19]], h->G[LogData[i].offsets[20]],
                                                            h->G[LogData[i].offsets[21]], h->G[LogData[i].offsets[22]],
                                                            h->G[LogData[i].offsets[23]], h->G[LogData[i].offsets[24]],
                                                            h->G[LogData[i].offsets[25]]);
            LogData[i].lastlog = t;
        } 
    }
}

/* --------------------------------------------------------- */
void Runtime_Error(unsigned int code, char *fmt, ...)
{
    va_list      list;
    char         *p, *r;
    int          e;
    float        f;
    unsigned int n = 0;
    unsigned int i;
    
	if (CurrentNode != NULL)
	{
        for (i=1; i<=NumberOfLines; i+=1)
        {
            if (LineNumberList[i].pnode == CurrentNode->Parent)
            {
                if (LineNumberList[i].codeoffset > CurrentNode->PC)
                {
                    n = i-1;
                    break;
                }
            }
        }

        printf("%s %d: node=%d line=%d ", (code < 1000) ? "Runtime error" : "WARNING", code, CurrentNode->NodeNumber, n);
    }
 
    va_start(list, fmt);

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

    if (code < 1000)
    {
        Shutdown();
        exit(EXIT_FAILURE);
    }
}



