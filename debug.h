/* DAMSON debug header
*/

#ifndef DEBUG
#define DEBUG

#include "compiler.h"
#include "emulator.h"

extern void Debug_Init(struct NodeInfo *NamedNodeList, struct NodeInfo *NodeList, unsigned int nlines, struct LineInfo *LineList);

extern void Debug(unsigned int node, unsigned int pc);

extern void Debug_End();


#endif
