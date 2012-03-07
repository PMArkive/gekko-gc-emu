// emu_win.h
// (c) 2005,2006 Gekko Team

#ifndef _EMU_DBG_H_
#define _EMU_DBG_H_

#if EMU_PLATFORM == PLATFORM_WINDOWS
#include "emu_win.h"
#endif

////////////////////////////////////////////////////////////

void Debugger_Open(void);
void dbgUseCallstack(void);
void dbgSetCursor(u32 addr);

//

#endif