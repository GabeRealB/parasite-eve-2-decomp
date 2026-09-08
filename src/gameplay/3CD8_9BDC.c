#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>
#include <psyq/strings.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

extern u8  Gp_MenuLockNow;
extern u8  Gp_MenuLockPrev;
extern u16 Gp_PadSuppressMask;
extern u16 Gp_PadSuppressPrev;
extern u16 Gp_PadSuppressRise;
extern u16 Gp_PadSuppressFall;
extern u16 Gp_PadSuppressRefs;
extern u8  Gp_MenuLockHold;
extern s16 Gp_MenuLockDelay;
extern s16 Gp_PadSuppressTimer;

void func_800E9BDC(u8 arg0, s32 arg1)
{
    switch (arg0) {
        case 1:
        case 5:
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask |= arg1;
            break;
        case 3:
            Gp_PadSuppressMask |= arg1;
            Gp_MenuLockHold     = 1;
            break;
        case 0:
        case 2:
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask &= ~arg1;
        case 4:
            break;
    }
}

void Gp_ResetMenuLock(void)
{
    Gp_PadSuppressRefs  = 0;
    Gp_PadSuppressMask  = 0;
    Gp_PadSuppressPrev  = 0;
    Gp_PadSuppressRise  = 0;
    Gp_PadSuppressFall  = 0;
    Gp_MenuLockNow      = 0;
    Gp_MenuLockPrev     = 0;
    Gp_MenuLockHold     = 0;
    Gp_MenuLockDelay    = 8;
    Gp_PadSuppressTimer = 4;
}
