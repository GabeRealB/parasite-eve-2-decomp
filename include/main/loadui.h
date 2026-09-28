#ifndef MAIN_LOADUI_H
#define MAIN_LOADUI_H

#include "types.h"

extern u8 D_800626E8;

extern u8 D_8007A394;

extern s16 D_8007A396;

/// Displays the disk-swap prompt and waits for the required stage disk.
extern s32 LoadUi_PollDiskSwap(void);

#endif // MAIN_LOADUI_H
