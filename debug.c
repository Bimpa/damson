/*
 * DAMSON compiler and interpreter
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef WIN32
   #include <winsock2.h>
#else //Unix
   #include <sys/socket.h>
   #include <sys/un.h>
   #include <netinet/in.h>
#endif

#include "cbool.h"
#include "debug.h"



#ifndef WIN32
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif

/* Defines */
#define REQUEST_PORT        48174           /* port number used for debug requests */
#define EVENT_PORT          48474           /* port number used to send debug event responses */
#define RESPONSE_BUFFER_SIZE 2048
#define DEBUGGER_ERROR 3001			/* error code for debugger */

#define DEBUG_PRINT false



/* Prototypes */

void 	EstablishRequestConnection();
void 	EstablishEventConnection();
void 	SendDebugEvent(const char *event);
void	SendRequestResponse();
void 	ProcessDebugRequest(struct NodeInfo *CurrentNode);
void 	resetResponseBuffer();
void 	appendToResponseBuffer(char* format, ...);

void 	UIControlLoop(struct NodeInfo *CurrentNode);
void 	CheckForBreakpoint(struct NodeInfo *CurrentNode);
boolean isBreakableInstruction(unsigned int Op);
void 	debugPrint(char * out, ...);
unsigned int getArrayVariableOffset(unsigned int* dimensions, unsigned int* indices);

void 	debugSetBreakpoint();
void 	debugClearBreakpoint();

void 	debugStep(struct NodeInfo *CurrentNode);
void 	debugResume();
void 	debugSource(struct NodeInfo *CurrentNode);
void 	debugThreads(struct NodeInfo *CurrentNode);
void 	debugStack(struct NodeInfo *CurrentNode);
void 	debugVariable(struct NodeInfo *CurrentNode);
void 	debugGlobalVariable(struct NodeInfo *CurrentNode);
void 	debugArrayVariable(struct NodeInfo *CurrentNode);
void 	debugGlobalArrayVariable(struct NodeInfo *CurrentNode);
void 	debugSetVariable(struct NodeInfo *CurrentNode);
void 	debugSetGlobalVariable(struct NodeInfo *CurrentNode);
void 	debugSetArrayVariable(struct NodeInfo *CurrentNode);
void 	debugSetGlobalArrayVariable(struct NodeInfo *CurrentNode);
void 	debugExit();

void 			readStringFromRequestBuffer(char* result);
unsigned int 	readIntFromRequestBuffer();

int 		FindCurrentProcedure(struct NodeInfo *CurrentNode, unsigned int pc);
struct PCB 	*SearchProcess(struct NodeInfo *CurrentNode, unsigned int p_handle);
int 		getAliasLineNumber(struct NodeInfo *CurrentNode, unsigned int PC);
int 	    getPrototypeLineNumber(struct NodeInfo *PrototypeNode, unsigned int codeoffset);

/* Globals Variables */

enum step{
	NO_STEP,
	STEP_INTO,
	STEP_OVER,
	STEP_OUT}stepping;					/* step flag indicates if the execution is being stepped */

typedef struct
{
	unsigned char Op;	//original op code for breakpoint
	int           Arg;	//original arg for breakpoint
	unsigned int  AliasCondition; //alias condition (node number) for breakpoint
} DebugInstruction;



boolean 					    suspend;										/* flag for suspending execution of instructions */
int								stepStack;										/* when stepping over or out a stack count is required to keep track of any preceding function entries */
struct NodeInfo					*StepNode;										/* holds node step was called on */
struct PCB						*StepProcess;									/* holds pointer to the process step was called on */
unsigned int					StepPC;											/* holds PC step was called on */
unsigned int					StepLine;										/* holds source line step was called on */

char 							requestBuffer[256];								/* buffer to hold request characters */
unsigned int					request_buffer_index = 0;
char 							suspendBuffer[256];								/* buffer to hold suspend characters (sent back to the IDE) */
char 							responseBuffer[RESPONSE_BUFFER_SIZE];							/* buffer to hold response buffer sent back to IDE as event */
unsigned int 					response_buffer_index = 0;

//new debugger stuff
char               				FileSourceName[MaxStringSize];
struct NodeInfo 				*NamedNodeList;
struct NodeInfo 				*NodeList;
unsigned int 					nlines;
struct LineInfo 				*LineList;
DebugInstruction				DebugTable[MaxLines];                   		/* holds the original instruction which is replaced in the node by DEBUG and any breakpoint conditions*/


#ifdef WIN32
SOCKET 							request_connection;								/* handle for request connection */
SOCKET 							request_sock;									/* handle for request socket */
SOCKET 							event_connection;								/* handle for event connection */
SOCKET 							event_sock;										/* handle for event socket */
#else //Unix
int 							request_connection;								/* handle for request connection */
int 							request_sock;									/* handle for request socket */
int 							event_connection;								/* handle for event connection */
int 							event_sock;										/* handle for event socket */
#endif

const char* RequestStrings[] ={
	"resume",
	"step",
	"set",
	"clear",
	"source",
	"threads",
	"stack",
	"var",
	"gvar",
	"arrayvar",
	"garrayvar",
	"setvar",
	"setgvar",
	"setarrayvar",
	"setgarrayvar",
	"exit"
};

typedef enum
{
	DEBUG_COMMAND_RESUME,
	DEBUG_COMMAND_STEP,
	DEBUG_COMMAND_SET,
	DEBUG_COMMAND_CLEAR,
	DEBUG_COMMAND_SOURCE,
	DEBUG_COMMAND_THREADS,
	DEBUG_COMMAND_STACK,
	DEBUG_COMMAND_VARIABLE,
	DEBUG_COMMAND_GLOBAL_VARIABLE,
	DEBUG_COMMAND_ARRAY_VARIABLE,
	DEBUG_COMMAND_GLOBAL_ARRAY_VARIABLE,
	DEBUG_COMMAND_SET_VARIABLE,
	DEBUG_COMMAND_SET_GLOBAL_VARIABLE,
	DEBUG_COMMAND_SET_ARRAY_VARIABLE,
	DEBUG_COMMAND_SET_GLOBAL_ARRAY_VARIABLE,
	DEBUG_COMMAND_EXIT,
	DEBUG_COMMAND_UNDEFINED
}RequestCommand;

/* Function Declarations */

/* --------------------------------------------------------- */

/*
 * Begin the debugger by establishing debug connections
 */
void Debug_Init(struct NodeInfo *nnl, struct NodeInfo *nl, unsigned int l, struct LineInfo *ll)
{
#ifdef WIN32
	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		Error(DEBUGGER_ERROR, "Winsocket Startup failed\n");
	}
#endif

	/*int i; //temp code for displaying value of linelist. blank lines are causing a bug
	for (i=0; i<NumberOfLines; i++){
		printf("LINE %i: %i, %i\n", i, ll[i].pnode, ll[i].codeoffset);
	}*/

	EstablishRequestConnection();
	EstablishEventConnection();
	fflush(stdout);

	sprintf(FileSourceName, "%s.d", FileBaseName);
	nlines = l;
	LineList = ll;
	NamedNodeList = nnl;
	NodeList = nl;
	memset(DebugTable, 0, sizeof(DebugInstruction)*MaxLines);

	//initial state of debugger
	suspend = true;									//suspend initially to read any deferred breakpoints
	sprintf(suspendBuffer, "suspended\n");			//set suspend buffer text as initially the debugger is suspended
	stepping = false;
	SendDebugEvent("started\n");			//send started event to IDE

	//go to debug control loop to set any initial breakpoints (no current node)
	UIControlLoop(NULL);
}

void Debug_End(){
	//send terminate IDE
	SendDebugEvent("terminated\n");

#ifdef WIN32
	closesocket(request_sock);
	closesocket(event_sock);
	closesocket(request_connection);
	closesocket(event_connection);
	WSACleanup();
#else
	close(request_sock);
	close(event_sock);
	close(request_connection);
	close(event_connection);
#endif
	debugPrint("Debugger Connections Terminated\n");
}



void appendToResponseBuffer(char* format, ...)
{
	unsigned int r, max;
	va_list args;

	max = RESPONSE_BUFFER_SIZE - (strlen(responseBuffer)+1);
	va_start(args, format);
	r = vsnprintf(&responseBuffer[response_buffer_index], max, format, args);
	va_end(args);

	if (r >= max)
		Error(DEBUGGER_ERROR, "Buffer overflow writing the debugger response\n");

	response_buffer_index += r;
}

/*
 * Establishes a connection via the request port
 */
void EstablishRequestConnection()
{
	unsigned int 			clilen;
	struct sockaddr_in 		serv_addr;
	struct sockaddr_in 		cli_addr;
	int 					optVal = true;


	//create new socket
	request_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (request_sock == INVALID_SOCKET)
		Error(DEBUGGER_ERROR, "ERROR: Could Not Open Debug Request Socket\n");

	//set reuse address to true
	if (setsockopt(request_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal)) == SOCKET_ERROR) {
	    Error(DEBUGGER_ERROR, "ERROR: Could Not Set Debug Request Socket Options\n");
	    exit(1);
	}

	//set all struct values to 0
	memset((char *) &serv_addr, 0, sizeof(serv_addr));

	//set up server
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(REQUEST_PORT);		// network byte order

	if (bind(request_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))  == SOCKET_ERROR)
		Error(DEBUGGER_ERROR, "ERROR: Could not Bind Debug Request Socket (on %i)\n", REQUEST_PORT);

	listen(request_sock, 5);

	debugPrint("Request Socket Waiting for Debug Client\n");

	//accept the client
	clilen = sizeof(cli_addr);
	request_connection = accept(request_sock, (struct sockaddr *) &cli_addr, &clilen);
	if (request_connection == INVALID_SOCKET)
		Error(DEBUGGER_ERROR, "ERROR: Could not Accept Connection to Debug Request Socket\n");

	debugPrint("Request Socket Connected to Debug Client\n");
}


/*
 * Establishes a connection via the event port
 */
void EstablishEventConnection()
{
	unsigned int 				clilen;
	struct 	sockaddr_in 	serv_addr;
	struct sockaddr_in 		cli_addr;
	int 					optVal = true;


	//create new socket
	event_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (event_sock == INVALID_SOCKET)
		Error(DEBUGGER_ERROR, "ERROR: Could Not Open Debug Event Socket\n");

	//set reuse address to true
	if (setsockopt(event_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal)) == SOCKET_ERROR) {
	    Error(DEBUGGER_ERROR, "ERROR: Could Not Set Debug Request Socket Options\n");
	    exit(1);
	}

	//set all struct values to 0
	memset((char *) &serv_addr, 0, sizeof(serv_addr));

	//set up server
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(EVENT_PORT);		/* network byte order */

	if (bind(event_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
		Error(DEBUGGER_ERROR, "ERROR: Could not Bind Debug Request Socket (on %i)\n", EVENT_PORT);

	listen(event_sock, 5);

	debugPrint("Event Port Waiting for Debug Client\n");

	//accept the client
	clilen = sizeof(cli_addr);
	event_connection = accept(event_sock, (struct sockaddr *) &cli_addr, &clilen);
	if (event_connection == INVALID_SOCKET)
		Error(DEBUGGER_ERROR, "ERROR: Could not Accept Connection to Debug Event Socket\n");

	debugPrint("Event Port Connected to Debug Client\n");
}

/*
 * Sends a debug event to the IDE
 * Usually of the for "OK ***" where *** mirrors the request i.e. get, set, exit
 */
void SendDebugEvent(const char *event)
{
	int res;

	debugPrint("SEND EVENT: %s\n", event);

	res = send(event_connection, event, strlen(event), 0);
	if (res < 0){
			Error(DEBUGGER_ERROR, "ERROR: Error Writing to Debug Event Socket\n");
	}
}

/*
 * Sends a debug response to the IDE
 */
void SendRequestResponse()
{
	int res;
	int len;

	len = strlen(responseBuffer);

	debugPrint("SEND RESPONSE (%i): %s\n", len, responseBuffer);
	res = send(request_connection, responseBuffer, len, 0);

	if (res < 0)
			Error(DEBUGGER_ERROR, "ERROR: Error Writing to Debug Request Socket\n");
}

/*
 * Reads a debug request and processes it by stripping the request and arguments
 */
void ProcessDebugRequest(struct NodeInfo *CurrentNode)
{
	int 	n;
	char 	requestString[256];
	int 	i;

	//reset buffer
	memset(requestBuffer, 0, 256);

	//read from client
	n = recv(request_connection, requestBuffer, 255, 0);
	if (n < 0)
		Error(DEBUGGER_ERROR, "ERROR: Error Reading from Debug Request Socket\n");
	debugPrint("RECIEVED REQUEST: %s\n", requestBuffer);

	//reset request and response buffer index
	request_buffer_index = 0;
	responseBuffer[response_buffer_index=0] = '\0';

	//get request command
	readStringFromRequestBuffer(requestString);

	//get the request command item
	RequestCommand cmd = DEBUG_COMMAND_UNDEFINED;
	for (i=0; i<(sizeof(RequestStrings)/sizeof(*RequestStrings)); i++)
	{
		if (strcmp(RequestStrings[i], requestString)==0)
		{
			cmd = i;
			break;
		}
	}

	//process request command item
	switch(cmd)
	{
		case(DEBUG_COMMAND_RESUME):
			debugResume();
			break;
		case(DEBUG_COMMAND_STEP):
			debugStep(CurrentNode);
			break;
		case(DEBUG_COMMAND_SET):
			debugSetBreakpoint();
			break;
		case(DEBUG_COMMAND_CLEAR):
			debugClearBreakpoint();
			break;
		case(DEBUG_COMMAND_SOURCE):
			debugSource(CurrentNode);
			break;
		case(DEBUG_COMMAND_THREADS):
			debugThreads(CurrentNode);
			break;
		case(DEBUG_COMMAND_STACK):
			debugStack(CurrentNode);
			break;
		case(DEBUG_COMMAND_VARIABLE):
			debugVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_GLOBAL_VARIABLE):
			debugGlobalVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_ARRAY_VARIABLE):
			debugArrayVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_GLOBAL_ARRAY_VARIABLE):
			debugGlobalArrayVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_SET_VARIABLE):
			debugSetVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_SET_GLOBAL_VARIABLE):
			debugSetGlobalVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_SET_ARRAY_VARIABLE):
			debugSetArrayVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_SET_GLOBAL_ARRAY_VARIABLE):
			debugSetGlobalArrayVariable(CurrentNode);
			break;
		case(DEBUG_COMMAND_EXIT):
			debugExit();
			break;
		default:
			Error(DEBUGGER_ERROR, "Unsupported Debug Request!\n");
			exit(0);
			break;
	}
}


/**
 * Main debug loop which checks for breakpoints and yields to the UI
 */
void Debug(unsigned int node, unsigned int pc)
{
    //debugPrint("**DEBUG**\n");
	struct NodeInfo *CurrentNode;
    int source_line;
    Instruction instruction;

    //find the node
    CurrentNode = FindNode(node);

    //check for breakpoint (or step) and update suspend
	CheckForBreakpoint(CurrentNode);

	if (suspend)
		UIControlLoop(CurrentNode);

	//execute the debug instruction (do nothing if step)
	if (CurrentNode->Instructions[pc].Op == s_DEBUG)
	{
		//get the original instruction from the debug table
		source_line = CurrentNode->Instructions[pc].Arg;
		instruction.Op = DebugTable[source_line].Op;
		instruction.Arg = DebugTable[source_line].Arg;

		//execute the original instruction (make sure tracing is off or debug will get called again!!)
		Tracing(false);
		ExecuteInstruction(instruction.Op, instruction.Arg);
	}

	//Set tracing in emulator only after current debug instruction has been executed (otherwise endless loop)
	if (stepping)
		Tracing(true);

}

/*
 * While suspended recursively get new requests and then process them
 */
void UIControlLoop(struct NodeInfo *CurrentNode)
{
	//send suspend event
	SendDebugEvent(suspendBuffer);

	//reset step (don't bother resetting step PC or step node)
	stepping = false;

	while (suspend)
	{
		ProcessDebugRequest(CurrentNode);
	}

	if (stepping)
		SendDebugEvent("resumed step\n");
	else
		SendDebugEvent("resumed breakpoint\n");
}

/*
 * Checks for breakpoints by consulting the debug table.
 * If stepping then checks the current instruction to see if execution should be suspended.
 */
void CheckForBreakpoint(struct NodeInfo *CurrentNode)
{
	unsigned int 	Op;
	unsigned int 	source_line;

	Op  = CurrentNode->Instructions[CurrentNode->PC].Op;
	source_line = getAliasLineNumber(CurrentNode, CurrentNode->PC);

	//check for marked breakpoints by iterating break list
	if (CurrentNode->Instructions[CurrentNode->PC].Op == s_DEBUG)
	{
		if ((DebugTable[source_line].AliasCondition == 0)||(DebugTable[source_line].AliasCondition == CurrentNode->NodeNumber))
		{
			suspend = true;
			sprintf(suspendBuffer, "suspended breakpoint %i\n", source_line);
			return;
		}
	}

	//check for step command
	if ((stepping)&&(StepNode==CurrentNode)&&(StepProcess==CurrentNode->CurrentProcess))
	{
		switch (stepping)
		{
			case(STEP_INTO):
			{
				//STEP INTO: Check that the PC has changed line or if the op is an entry or exit
				if ((StepLine != source_line)||(Op==s_RTRN))
				{
					if (isBreakableInstruction(Op))
					{
						suspend = true;
						sprintf(suspendBuffer, "suspended step\n");
						return;
					}
				}
				break;
			}
			case(STEP_OUT):
			{
				//STEP OUT: Return as long as the step stack (number of entry calls  since step over) has not increased
				if (Op==s_RTRN)
				{
					if (stepStack > 0)
					{
						stepStack--;
						return;
					}
					else
					{
						stepping = STEP_INTO; //stop at next instruction
						return;
					}
				}
				else if (Op==s_ENTRY)	//increase step stack
				{
					stepStack++;//why is s_RETURN called twice
					return;
				}

				break;

			}
			case(STEP_OVER):
			{
				if ((StepLine != source_line)||(Op==s_RTRN))
				{
					if (isBreakableInstruction(Op)&&(stepStack<=0))
					{
						suspend = true;
						sprintf(suspendBuffer, "suspended step\n");
						return;
					}
				}
				if (Op==s_ENTRY)
				{
					stepStack++;
					return;
				}
				else if (Op==s_RTRN)
				{
					stepStack--;
					return;
				}

				break;
			}
			default:
				break;
		}
	}

	suspend = false;

}

/*
 * checks if the given instruction is breakable by ensuring it is not one of the ignored instructions (i.e. instruction without context in DAMSON C source)
 */
boolean isBreakableInstruction(unsigned int Op)
{
	/* dont want to break on ignored instructions as they have no context in the C program */
	return ((Op != s_LAB)&&
			(Op != s_QUERY)&&
			(Op != s_STORE)&&
			(Op != s_ENTRY)&&
			(Op != s_RSTACK));
}

/*
 * gets the next breakable instruction from a node given the code offset
 */
unsigned int getNextBreakableInstruction(struct NodeInfo *n, unsigned int p)
{
	/* dont want to break on ignored instructions as they have no context in the C program */
	while(!(isBreakableInstruction(n->Instructions[p].Op)))
		p++;
	return p;
}

/*
 * printf statement used for debugging the debugger.
 * Uses a forced flush of the output stream (required for some reason when displaying in Eclipse console).
 * If DEBUG_PRINT is false then printing is ignored
 */
void debugPrint(char * out, ...)
{
	if (DEBUG_PRINT)
	{
		va_list args;
		va_start (args, out);
		vprintf (out, args);
		va_end (args);
		fflush(stdout);
	}
}

/*
 * Gets an array index offset given the dimensions of the indices
 */
unsigned int getArrayVariableOffset(unsigned int* dimensions, unsigned int* indices)
{
	int i;
	int j;
	int d;
	int r;

	r=0;

	for (i=1; i<=indices[0]; i++)
	{
		d = 1;								//calculate offset in memory for a given dimension
		for(j=(i+1); j<=dimensions[0];j++)
		{
			d *= dimensions[j];
		}
		r += d*indices[i];					//use dimension offset with index to find offset for this index value
	}

	return r;
}


/*
 * Sets the stepping mode depending on the request arguments
 */
void debugStep(struct NodeInfo *CurrentNode)
{
	char step_cmd[16];

	readStringFromRequestBuffer(step_cmd);
	if(strcmp(step_cmd, "over")==0)
	{
		stepping = STEP_OVER;
	}
	else if(strcmp(step_cmd, "out")==0)
	{
		//if current op is return then step into will return anyway
		if (CurrentNode->Instructions[CurrentNode->PC].Op == s_RTRN)
			stepping = STEP_INTO;
		else
			stepping = STEP_OUT;
	}
	else //step into
	{
		stepping = STEP_INTO;
	}

	suspend = false;
	StepNode = CurrentNode;
	StepProcess = CurrentNode->CurrentProcess;
	stepStack = 0;
	StepPC = CurrentNode->PC;
	StepLine = getAliasLineNumber(CurrentNode, CurrentNode->PC);
	appendToResponseBuffer("OK step\n");
	SendRequestResponse();
}

/*
 * Sets resume variable to continue execution
 */
void debugResume()
{
	Tracing(false); //tracing off
	suspend = false;
	appendToResponseBuffer("OK resume\n");
	SendRequestResponse();
}

/*
 * Sets a debug breakpoint on a given line
 */
void debugSetBreakpoint()
{
    int source_line;
    int breakable_source_line;
    int alias_condition;
    struct LineInfo line_info;
    int i;

    //source line is in requestBuffer
    source_line = readIntFromRequestBuffer();
    alias_condition = readIntFromRequestBuffer();

    if (source_line < MaxLines){
    	line_info = LineList[source_line];

    	//get code position of next breakable instruction (including debug)
    	i = getNextBreakableInstruction(line_info.pnode, line_info.codeoffset);
    	breakable_source_line = getPrototypeLineNumber(line_info.pnode, line_info.codeoffset);
    	if (breakable_source_line != source_line){
    		source_line = breakable_source_line;
    		line_info = LineList[breakable_source_line];
    	}

    	//if breakpoint exists (i.e. debug instruction already present) then update rather than install
    	if (line_info.pnode->Instructions[i].Op == s_DEBUG){
    		DebugTable[source_line].AliasCondition = alias_condition;
    		debugPrint("Breakpoint Updated at line %i (%i) alias condition is %i\n", source_line, i, DebugTable[source_line].AliasCondition);
    	}else{
        	//get next breakable instruction!!!!
    		DebugTable[source_line].Op = line_info.pnode->Instructions[i].Op;
			DebugTable[source_line].Arg = line_info.pnode->Instructions[i].Arg;
			DebugTable[source_line].AliasCondition = alias_condition;

			//replace original instruction with debug instruction
			line_info.pnode->Instructions[i].Op = s_DEBUG;
			line_info.pnode->Instructions[i].Arg = source_line;
			debugPrint("Breakpoint Set at line %i (%i) alias condition is %i\n", source_line, i, DebugTable[source_line].AliasCondition);
    	}

		appendToResponseBuffer("OK set\n");
		SendRequestResponse();
		return;
    }

	appendToResponseBuffer("FAILED set\n");
	SendRequestResponse();
}

/*
 * Clears a debug breakpoint on a given line
 */
void debugClearBreakpoint()
{
    int source_line;
    int breakable_source_line;
    struct LineInfo line_info;
    int i;

    //source line is in requestBuffer
	source_line = readIntFromRequestBuffer();

	if (source_line < MaxLines){
		line_info = LineList[source_line];

		//get code position of next breakable instruction (including debug)
		i = getNextBreakableInstruction(line_info.pnode, line_info.codeoffset);
		breakable_source_line = getPrototypeLineNumber(line_info.pnode, line_info.codeoffset);
		if (breakable_source_line != source_line){
			source_line = breakable_source_line;
			line_info = LineList[breakable_source_line];
		}

		//replace instruction with original from debug table
		line_info.pnode->Instructions[i].Op = DebugTable[source_line].Op;
		line_info.pnode->Instructions[i].Arg = DebugTable[source_line].Arg;

		//reset debug table
		DebugTable[source_line].Op = 0;
		DebugTable[source_line].Arg = 0;
		DebugTable[source_line].AliasCondition = 0;

		debugPrint("Breakpoint Cleared at line %i (%i)\n", source_line, i);
		appendToResponseBuffer("OK clear\n");
		SendRequestResponse();
		return;
	}

	appendToResponseBuffer("FAILED clear\n");
	SendRequestResponse();
}

void debugSource(struct NodeInfo *CurrentNode)
{
	appendToResponseBuffer("%s|%u\n", FileSourceName, CurrentNode->NodeNumber);
	SendRequestResponse();
}


/*
 * Returns a response representing the processes of the current node
 * in the format thread#thread#...#thread where thread is process_handle|state
 */
void debugThreads(struct NodeInfo *CurrentNode)
{
	struct PCB* p;

	appendToResponseBuffer("%u|%u", CurrentNode->CurrentProcess->handle, CurrentNode->CurrentProcess->status);

	p = CurrentNode->CurrentProcess->nextPCB;
	//Iterate the process list
	while(p != NULL)
	{
		appendToResponseBuffer("#%u|%u", p->handle, p->status);

		p = p->nextPCB;
	}

	//end line and print to socket
	appendToResponseBuffer("\n");
	SendRequestResponse();
}

/*
 * Returns a response representing the current call stack in the following format:
 * "frame#frame...#frame" where each frame is a string: "source line|function name|variable name|variable name|...|variable name"
 */
void debugStack(struct NodeInfo *CurrentNode)
{
	int p;
	int i;
	int PC;
	int FP;
	unsigned int phandle;
	struct PCB* process;

	phandle = readIntFromRequestBuffer();

	process = SearchProcess(CurrentNode, phandle);
	if (CurrentNode->CurrentProcess->handle != phandle)
	{
		PC = process->saved_PC;
		FP = process->saved_FP;
	}
	else
	{
		PC = CurrentNode->PC;
		FP = CurrentNode->FP;
	}


	//move up through the call stack
	while(PC != 0)
	{
		if (strlen(responseBuffer)>0){
			appendToResponseBuffer("#");
		}

		//current procedure
		p = FindCurrentProcedure(CurrentNode, PC);
		appendToResponseBuffer("%i|%s", getAliasLineNumber(CurrentNode, PC), CurrentNode->Procedures[p].Name);


		//iterate current stack variables
		for (i=1; i<= CurrentNode->Procedures[p].nLocals; i++)
		{
			appendToResponseBuffer("|%s", CurrentNode->Procedures[p].Args[i].Name);
		}
		//append any global variables
		for (i=(GLOBALBASE+1); i<=CurrentNode->NumberOfGlobals; i++)
		{
			appendToResponseBuffer("|&%s", CurrentNode->Globals[i].Name);	//prefix with '&' to indicate a global
		}


		PC = process->stack[FP + CurrentNode->Procedures[p].BP + 1];
		FP = process->stack[FP + CurrentNode->Procedures[p].BP + 2];
	}


	//end line and print to socket
	appendToResponseBuffer("\n");
	SendRequestResponse();
}


/*
 * Returns a response representing a variable value or [N] if the value is an array (where N is the array size)
 * The requestBuffer should contain a string in the following format "process_handle proc_id name value" where
 *   process_handle is the current process handle
 *   proc_id = integer value representing the procedure number of the current call function
 *   name = string value representing the name of the variable in the identified procedure
 */
void debugVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	int i;
	int j;
	int p;
	int PC;
	int FP;
	unsigned int stack_frame;
	unsigned int p_handle;
	struct PCB* process;

	i = 0;
	j = 0;
	stack_frame = 0;
	p_handle = 0;

	//get process handle reading number from requestBuffer
	p_handle = readIntFromRequestBuffer();

	//get procedure id by reading number from requestBuffer
	stack_frame = readIntFromRequestBuffer();

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get the PC and FP for the process handle
	process = SearchProcess(CurrentNode, p_handle);
	if (CurrentNode->CurrentProcess->handle != p_handle)
	{
		PC = process->saved_PC;
		FP = process->saved_FP;
	}
	else
	{
		PC = CurrentNode->PC;
		FP = CurrentNode->FP;
	}
	p = FindCurrentProcedure(CurrentNode, PC);

	//Recursively iterate the call stack
	for (i = 0; i < stack_frame; i++) {
		PC = process->stack[FP + CurrentNode->Procedures[p].BP + 1];
		FP = process->stack[FP + CurrentNode->Procedures[p].BP + 2];
		p = FindCurrentProcedure(CurrentNode, PC);
	}

	//iterate current procedure arguments to find variable name location then lookup in stack
	for (i=1; i<= CurrentNode->Procedures[p].nLocals; i++)
	{
		if (strcmp(CurrentNode->Procedures[p].Args[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Procedures[p].Args[i].vOffset;
			if (CurrentNode->Procedures[p].Args[i].vDimensions[0] > 0)
				sprintf(strBuffer, "[%i]", CurrentNode->Procedures[p].Args[i].vDimensions[1]);	/* print array size i.e. [3] */
			else
			{
				switch(CurrentNode->Procedures[p].Args[i].vType){
					case(IntType):
						sprintf(strBuffer, "%i", process->stack[FP + j]);						/* print single integer value */
						break;
					case(FloatType):
						sprintf(strBuffer, "%f", (double)process->stack[FP + j]/65536.0);		/* print single float value */
						break;
					default:

						break;
				}

			}

			break;
		}
	}

	appendToResponseBuffer("%s\n", strBuffer);
	SendRequestResponse();
}

void debugGlobalVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	int i;
	int j;


	i = 0;
	j = 0;

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	for (i=(GLOBALBASE+1);i<=CurrentNode->NumberOfGlobals; i++)
	{
		if (strcmp(CurrentNode->Globals[i].Name, strBuffer)==0)
		{
			j = CurrentNode->Globals[i].vOffset;
			if (CurrentNode->Globals[i].vDimensions[0] > 0)
				sprintf(strBuffer, "[%i]", CurrentNode->Globals[i].vDimensions[1]);	/* print array size i.e. [3] */
			else
			{
				switch(CurrentNode->Globals[i].vType){
					case(IntType):
						sprintf(strBuffer, "%i", CurrentNode->G[j]);						/* print single integer value */
						break;
					case(FloatType):
						sprintf(strBuffer, "%f", (double)CurrentNode->G[j]/65536.0);						/* print single float value */
						break;
					default:

						break;
				}

			}
		}
	}

	appendToResponseBuffer("%s\n", strBuffer);
	SendRequestResponse();
}


/*
 * Returns a response representing an array variable value or [N] if the value is an array (where N is the array size)
 * The requestBuffer should contain a string in the following format "proc_id name value" where
 *   proc_id = integer value representing the procedure number of the current call function
 *   name = string value representing the name of the variable in the identified procedure
 *   indices = string value of integer characters where first value represents the dimensionality and the following values represent indices for each dimension
 */
void debugArrayVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	unsigned int indices[MaxDimensions+1];
	int i;
	int j;
	int d;
	int p;
	int PC;
	int FP;
	unsigned int stack_frame;
	unsigned int p_handle;
	struct PCB* process;

	indices[0] = 0;
	stack_frame = 0;
	i=0;
	p_handle = 0;

	//get process handle reading number from requestBuffer
	p_handle = readIntFromRequestBuffer();

	//get procedure id by reading number from requestBuffer
	stack_frame = readIntFromRequestBuffer();

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get indices
	indices[0] = readIntFromRequestBuffer();
	for (j=1; j<=indices[0]; j++)
	{
		indices[j] = readIntFromRequestBuffer();
	}

	//get the PC and FP for the process handle
	process = SearchProcess(CurrentNode, p_handle);
	if (CurrentNode->CurrentProcess->handle != p_handle)
	{
		PC = process->saved_PC;
		FP = process->saved_FP;
	}
	else
	{
		PC = CurrentNode->PC;
		FP = CurrentNode->FP;
	}
	p = FindCurrentProcedure(CurrentNode, PC);

	//Recursively iterate the call stack
	for (i = 0; i < stack_frame; i++) {
		PC = process->stack[FP + CurrentNode->Procedures[p].BP + 1];
		FP = process->stack[FP + CurrentNode->Procedures[p].BP + 2];
		p = FindCurrentProcedure(CurrentNode, PC);
	}

	//iterate current procedure arguments to find variable name location
	for (i=1; i<= CurrentNode->Procedures[p].nLocals; i++)
	{
		if (strcmp(CurrentNode->Procedures[p].Args[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Procedures[p].Args[i].vOffset;

			if (indices[0] == CurrentNode->Procedures[p].Args[i].vDimensions[0])
			{
				//return 1 value
				d = getArrayVariableOffset(CurrentNode->Procedures[p].Args[i].vDimensions, indices);
				switch(CurrentNode->Procedures[p].Args[i].vType){
					case(IntType):
						sprintf(strBuffer, "%i", process->stack[FP + j + d]);						/* print single integer value */
						break;
					case(FloatType):
						sprintf(strBuffer, "%f", (double)process->stack[FP + j + d]/65536.0);						/* print single float value */
						break;
					default:

						break;
				}
			}
			else
			{
				//current dimension is less than total dimensions so return the dimension size as value
				sprintf(strBuffer, "[%i]", CurrentNode->Procedures[p].Args[i].vDimensions[indices[0]+1]);	// print array size i.e. [3]
			}

			break;
		}
	}

	appendToResponseBuffer("%s\n", strBuffer);
	SendRequestResponse();
}

void debugGlobalArrayVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	unsigned int indices[MaxDimensions+1];
	int i;
	int j;
	int d;

	indices[0] = 0;
	i=0;

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get indices
	indices[0] = readIntFromRequestBuffer();
	for (j=1; j<=indices[0]; j++)
	{
		indices[j] = readIntFromRequestBuffer();
	}
	//iterate current procedure arguments to find variable name location
	for (i=(GLOBALBASE+1); i<=CurrentNode->NumberOfGlobals; i++)
	{
		if (strcmp(CurrentNode->Globals[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Globals[i].vOffset;

			if (indices[0] == CurrentNode->Globals[i].vDimensions[0])
			{
				//return 1 value
				d = getArrayVariableOffset(CurrentNode->Globals[i].vDimensions, indices);
				switch(CurrentNode->Globals[i].vType){
					case(IntType):
						sprintf(strBuffer, "%i", CurrentNode->G[j + d]);						/* print single integer value */
						break;
					case(FloatType):
						sprintf(strBuffer, "%f", (double)CurrentNode->G[j + d]/65536.0);						/* print single float value */
						break;
					default:

						break;
				}
			}
			else
			{
				//current dimension is less than total dimensions so return the dimension size as value
				sprintf(strBuffer, "[%i]", CurrentNode->Globals[i].vDimensions[indices[0]+1]);	// print array size i.e. [3]
			}

			break;
		}
	}

	appendToResponseBuffer("%s\n", strBuffer);
	SendRequestResponse();
}

/*
 * Sets a variable value.
 * The requestBuffer should contain a string in the following format "proc_id name value" where
 *   proc_id = integer value representing the procedure number of the current call function
 *   name = string value representing the name of the variable in the identified procedure
 *   value = integer value representing the value the named variable should be set to (always an integer so floats need to be converted by the IDE)
 */
void debugSetVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	char strValueBuffer[32];	//assume 32 chars is big enough
	int i;
	int j;
	int p;
	int PC;
	int FP;
	unsigned int stack_frame;
	unsigned int p_handle;
	struct PCB* process;

	i = 0;
	j = 0;
	stack_frame = 0;
	p_handle = 0;

	//get process handle reading number from requestBuffer
	p_handle = readIntFromRequestBuffer();

	//get procedure id by reading number from requestBuffer
	stack_frame = readIntFromRequestBuffer();

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get variable value by reading it from the requestBuffer
	readStringFromRequestBuffer(strValueBuffer);

	//get the PC and FP for the process handle
	process = SearchProcess(CurrentNode, p_handle);
	if (CurrentNode->CurrentProcess->handle != p_handle)
	{
		PC = process->saved_PC;
		FP = process->saved_FP;
	}
	else
	{
		PC = CurrentNode->PC;
		FP = CurrentNode->FP;
	}
	p = FindCurrentProcedure(CurrentNode, PC);

	//Recursively iterate the call stack
	for (i = 0; i < stack_frame; i++) {
		PC = process->stack[FP + CurrentNode->Procedures[p].BP + 1];
		FP = process->stack[FP + CurrentNode->Procedures[p].BP + 2];
		p = FindCurrentProcedure(CurrentNode, PC);
	}

	//iterate current procedure arguments to find variable name location then lookup in stack
	for (i=1; i<= CurrentNode->Procedures[p].nLocals; i++)
	{
		if (strcmp(CurrentNode->Procedures[p].Args[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Procedures[p].Args[i].vOffset;
			if (CurrentNode->Procedures[p].Args[i].vDimensions[0] > 0)
			{
				appendToResponseBuffer("FAILED: setvar on array\n");
				SendRequestResponse();
				return;
			}
			else
			{
				process->stack[FP + j] = atoi(strValueBuffer);
				appendToResponseBuffer("OK: setvar\n");
				SendRequestResponse();
				return;
			}

		}
	}

	//send variable value response
	appendToResponseBuffer("FAILED: setvar\n");
	SendRequestResponse();
}

void debugSetGlobalVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	char strValueBuffer[32];	//assume 32 chars is big enough
	int i;
	int j;

	i = 0;
	j = 0;

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get variable value by reading it from the requestBuffer
	readStringFromRequestBuffer(strValueBuffer);

	//iterate current procedure arguments to find variable name location then lookup in stack
	for (i=(GLOBALBASE+1); i<=CurrentNode->NumberOfGlobals; i++)
	{
		if (strcmp(CurrentNode->Globals[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Globals[i].vOffset;
			if (CurrentNode->Globals[i].vDimensions[0] > 0)
			{
				appendToResponseBuffer("FAILED: setvar on array\n");
				SendRequestResponse();
				return;
			}
			else
			{
				CurrentNode->G[j] = atoi(strValueBuffer);
				appendToResponseBuffer("OK: setvar\n");
				SendRequestResponse();
				return;
			}

		}
	}

	//send variable value response
	appendToResponseBuffer("FAILED: setvar\n");
	SendRequestResponse();
}

/*
 * Sets a variable value belonging to an array.
 * The requestBuffer should contain a string in the following format "proc_id name value" where
 *   proc_id = integer value representing the procedure number of the current call function
 *   name = string value representing the name of the variable in the identified procedure
 *   indices = string value of integer characters where first value represents the dimensionality and the following values represent indices for each dimension
 *   value = integer value representing the value the named variable should be set to (always an integer so floats need to be converted by the IDE)
 */
void debugSetArrayVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	unsigned int indices[MaxDimensions+1];
	char strValueBuffer[32];	//assume 32 chars is big enough
	int i;
	int j;
	int d;
	int p;
	int PC;
	int FP;
	unsigned int stack_frame;
	unsigned int p_handle;
	struct PCB* process;

	i = 0;
	j = 0;
	stack_frame = 0;
	p_handle = 0;

	//get process handle reading number from requestBuffer
	p_handle = readIntFromRequestBuffer();

	//get procedure id by reading number from requestBuffer
	stack_frame = readIntFromRequestBuffer();

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get indices
	indices[0] = readIntFromRequestBuffer();
	for (j=1; j<=indices[0]; j++)
	{
		indices[j] = readIntFromRequestBuffer();
	}

	//get variable value by reading it from the requestBuffer
	readStringFromRequestBuffer(strValueBuffer);


	//get the PC and FP for the process handle
	process = SearchProcess(CurrentNode, p_handle);
	if (CurrentNode->CurrentProcess->handle != p_handle)
	{
		PC = process->saved_PC;
		FP = process->saved_FP;
	}
	else
	{
		PC = CurrentNode->PC;
		FP = CurrentNode->FP;
	}
	p = FindCurrentProcedure(CurrentNode, PC);

	//Recursively iterate the call stack
	for (i = 0; i < stack_frame; i++) {
		PC = process->stack[FP + CurrentNode->Procedures[p].BP + 1];
		FP = process->stack[FP + CurrentNode->Procedures[p].BP + 2];
		p = FindCurrentProcedure(CurrentNode, PC);
	}

	//iterate current procedure arguments to find variable name location
	for (i=1; i<= CurrentNode->Procedures[p].nLocals; i++)
	{
		if (strcmp(CurrentNode->Procedures[p].Args[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Procedures[p].Args[i].vOffset;

			if (indices[0] == CurrentNode->Procedures[p].Args[i].vDimensions[0])
			{
				//return 1 value
				d = getArrayVariableOffset(CurrentNode->Procedures[p].Args[i].vDimensions, indices);
				process->stack[FP + j + d] = atoi(strValueBuffer);
				appendToResponseBuffer("OK: setvar\n");
				SendRequestResponse();
				return;
			}
			else
			{
				appendToResponseBuffer("FAILED: setvar on array\n");
				SendRequestResponse();
				return;
			}

			break;
		}
	}

	//send variable value response
	appendToResponseBuffer("FAILED: setvar\n");
	SendRequestResponse();
}

void debugSetGlobalArrayVariable(struct NodeInfo *CurrentNode)
{
	char strBuffer[512];
	unsigned int indices[MaxDimensions+1];
	char strValueBuffer[32];	//assume 32 chars is big enough
	int i;
	int j;
	int d;

	i = 0;
	j = 0;

	//get variable name by reading it from requestBuffer
	readStringFromRequestBuffer(strBuffer);

	//get indices
	indices[0] = readIntFromRequestBuffer();
	for (j=1; j<=indices[0]; j++)
	{
		indices[j] = readIntFromRequestBuffer();
	}

	//get variable value by reading it from the requestBuffer
	readStringFromRequestBuffer(strValueBuffer);

	//iterate current procedure arguments to find variable name location
	for (i=(GLOBALBASE+1); i<=CurrentNode->NumberOfGlobals; i++)
	{
		if (strcmp(CurrentNode->Globals[i].Name, strBuffer) == 0) /* found matching variable name */
		{
			j = CurrentNode->Globals[i].vOffset;

			if (indices[0] == CurrentNode->Globals[i].vDimensions[0])
			{
				//return 1 value
				d = getArrayVariableOffset(CurrentNode->Globals[i].vDimensions, indices);
				CurrentNode->G[j + d] = atoi(strValueBuffer);
				appendToResponseBuffer("OK: setvar\n");
				SendRequestResponse();
				return;
			}
			else
			{
				appendToResponseBuffer("FAILED: setvar on array\n");
				SendRequestResponse();
				return;
			}

			break;
		}
	}

	//send variable value response
	appendToResponseBuffer("FAILED: setvar\n");
	SendRequestResponse();
}

/*
 * Exits the debugger and sends a response to indicate it has closed
 */
void debugExit()
{
	appendToResponseBuffer("OK Exit\n");
	SendRequestResponse();
	Debug_End();
	Shutdown();
	free(LineList);
	exit(0);
}



/**
 * Gets the next integer string from the request buffer and stores it as an int in result.
 * Returns the number of characters read (including trailing space or EOL marker).
 */
unsigned int readIntFromRequestBuffer()
{
	char c;
	char value[12];
	int i;
	char *buffer = &requestBuffer[request_buffer_index];

	i = 0;
	c = buffer[i];

	while (c >= '0' && c <= '9')
	{
		value[i++] = c;
		c = buffer[i];
	}
	value[i] = '\0';

	request_buffer_index += i+1;
	return atoi(value);
}

/**
 * Gets the next string from the request buffer (ending with either space or end of string)
 * Returns the number of characters read (including trailing space or EOL marker).
 */
void readStringFromRequestBuffer(char* result)
{
	char c;
	int i;
	char *buffer = &requestBuffer[request_buffer_index];

	i = 0;
	c = buffer[i];

	while (c != ' ' && c != '\0')
	{
		if (c == '\n')
			break;

		result[i++] = c;
		c = buffer[i];
	}
	result[i] = '\0';

	request_buffer_index += i+1;
}

struct PCB *SearchProcess(struct NodeInfo *CurrentNode, unsigned int p_handle)
{
    struct PCB *p;

    p = CurrentNode->CurrentProcess;

    while (p != NULL)
    {
        if (p->handle == p_handle)
            return p;

        p = p->nextPCB;
    }
    return NULL;  /* program not found */
}

int FindCurrentProcedure(struct NodeInfo *CurrentNode, unsigned int pc)
{
    while (pc > 0)
    {
        if (CurrentNode->Instructions[pc].Op == s_ENTRY)
        {
            break;
        }
        pc--;
    }

    return CurrentNode->Instructions[pc].Arg;
}

int getAliasLineNumber(struct NodeInfo *CurrentNode, unsigned int PC)
{
	int i;
	struct LineInfo l;
	unsigned int line;

	line = 0;

	//iterate line number to get last line
	for (i=0;i<NumberOfLines;i++){
		l = LineList[i];
		if (l.pnode == CurrentNode->Parent) //check the alias parent to see if it is associated with a line number
		{
			if ((l.codeoffset<=PC)&&(line<i))
				line = i;
		}
	}

	return line;
}

int getPrototypeLineNumber(struct NodeInfo *PrototypeNode, unsigned int codeoffset)
{
	int i;
	struct LineInfo l;
	unsigned int line;

	line = 0;

	//iterate line number to get last line
	for (i=0;i<NumberOfLines;i++){
		l = LineList[i];
		if (l.pnode == PrototypeNode) //Get the line number for a code offset given a prototype node
		{
			if ((l.codeoffset<=codeoffset)&&(line<i))
				line = i;
		}
	}

	return line;
}
