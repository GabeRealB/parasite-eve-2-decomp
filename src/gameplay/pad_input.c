#include "pad_input.h"

#include "types.h"

#include "gameplay/area_entry.h"
#include "gameplay/direction_input.h"
#include "pad_script.h"

#include "main/display.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
u8 Gp_MenuLockNow;

u8 Gp_MenuLockPrev;

u16 Gp_PadSuppressMask;

u16 Gp_PadSuppressPrev;

u16 Gp_PadSuppressRise;

u16 Gp_PadSuppressFall;

u16 Gp_PadSuppressRefs;

u8 Gp_MenuLockHold;

s16 Gp_MenuLockDelay;

s16 Gp_PadSuppressTimer;

#include "gameplay/pad_input.h"

extern u8 Gp_BtnMap0[16];

extern u8 Gp_BtnMap1[16];

extern u8 Gp_BtnMap2[16];

extern u8 Gp_BtnMap2Alt[16];

static u16 Gp_RemapButtons(GameActor* actor, u16 mask);

u8 Gp_BtnMap0[16] = {
    0,
    1,
    2,
    3,
    4,
    6,
    5,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

u8 Gp_BtnMap1[16] = {
    0,
    1,
    2,
    3,
    4,
    7,
    5,
    6,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

u8 Gp_BtnMap2[16] = {
    0,
    4,
    2,
    7,
    7,
    6,
    5,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

u8 Gp_BtnMap2Alt[16] = {
    0,
    4,
    2,
    7,
    1,
    6,
    5,
    3,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

void Gp_UpdatePadInput(void)
{
    PadState*     pad;
    PlayerStatus* cfg;
    Task*         work;
    GameActor*    actor;
    u16           mask;
    register u16  pressedButtons asm("s2"); // pinned: global-alloc otherwise ranks `actor` just above %hi(gGameSession) and gives it $s2
    u16           releasedButtons;

    pad  = &gPadStates[0];
    cfg  = &gPlayerStatus;
    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (work == NULL) {
        return;
    }
    actor = work->work;
    Gp_ClearPadHalt();
    if (Gp_MenuLockHold == 0) {
        if (actor->mode == GAME_ACTOR_MODE_NORMAL && gGameSession->eventState == 0 && gGameSession->cutsceneHold == 0 &&
            actor->state != 6 && cfg->hp > 0 && gGameSession->deathVariant == 0) {
            if (Gp_MenuLockDelay > 0) {
                Gp_MenuLockDelay--;
                Gp_MenuLockNow = 1;
            } else {
                Gp_MenuLockNow = 0;
            }
        } else {
            Gp_MenuLockNow      = 1;
            Gp_PadSuppressTimer = 4;
            Gp_MenuLockDelay    = 8;
            D_80114D08          = 0xA;
        }
        if (Gp_MenuLockNow == 1 && Gp_MenuLockPrev == 0) {
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask |= 0x900;
        } else if (Gp_MenuLockNow == 0 && Gp_MenuLockPrev == 1) {
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask &= 0xF6FF;
        }
        Gp_MenuLockPrev = Gp_MenuLockNow;
    }
    Gp_PadSuppressRise = ~Gp_PadSuppressPrev & Gp_PadSuppressMask;
    Gp_PadSuppressFall = Gp_PadSuppressPrev & ~Gp_PadSuppressMask;
    Gp_PadSuppressPrev = Gp_PadSuppressMask;
    if (gDisplayState.demoScene == DISPLAY_DEMO_NONE) {
        if (Gp_PadSuppressRise & 0x900) {
            Display_AcquireRef();
            Gp_PadSuppressRefs++;
        }
        if (Gp_PadSuppressFall & 0x900) {
            while (Gp_PadSuppressRefs != 0) {
                displayReleaseMenuHold();
                Gp_PadSuppressRefs--;
            }
        }
    }
    if (pad->inputFormat == PAD_INPUT_FORMAT_ANALOG) {
        mask            = pad->buttons;
        pressedButtons  = pad->pressedButtons;
        releasedButtons = pad->releasedButtons;
        if (pad->stickAxes[PAD_STICK_LEFT_Y] < -PAD_STICK_DIRECTION_THRESHOLD) {
            mask |= 0x1000;
            if (actor->mode != GAME_ACTOR_MODE_NORMAL || actor->state < 2) {
                if (pad->stickAxes[PAD_STICK_LEFT_Y] < -0xE80) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == 0) {
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 1) {
                            mask |= 0x20;
                        } else {
                            mask |= 0x80;
                        }
                    } else {
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout == 1) {
                            mask &= 0xFF7F;
                        } else {
                            mask &= 0xFFDF;
                        }
                    }
                } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == 1) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 1) {
                        mask |= 0x20;
                    } else {
                        mask |= 0x80;
                    }
                }
            }
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                mask |= 0x2000;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                mask |= 0x8000;
            }
        } else if (pad->stickAxes[PAD_STICK_LEFT_Y] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
            mask |= 0x4000;
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                mask |= 0x2000;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                mask |= 0x8000;
            }
        } else {
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                mask |= 0x2000;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                mask |= 0x8000;
            }
        }
    } else {
        mask            = pad->buttons;
        pressedButtons  = pad->pressedButtons;
        releasedButtons = pad->releasedButtons;
    }
    gGameSession->padHeld     = Gp_RemapButtons(actor, mask) & ~Gp_PadSuppressMask;
    gGameSession->padPressed  = Gp_RemapButtons(actor, pressedButtons) & ~Gp_PadSuppressMask;
    gGameSession->padReleased = Gp_RemapButtons(actor, releasedButtons) & ~Gp_PadSuppressMask;
    if (Gp_PadSuppressTimer != 0) {
        Gp_PadSuppressTimer--;
        gGameSession->padHeld     = Gp_RemapButtons(actor, mask) & ~Gp_PadSuppressMask & ~0x10;
        gGameSession->padPressed  = Gp_RemapButtons(actor, pressedButtons) & ~Gp_PadSuppressMask & ~0x10;
        gGameSession->padReleased = Gp_RemapButtons(actor, releasedButtons) & ~Gp_PadSuppressMask & ~0x10;
    }
}

static u16 Gp_RemapButtons(GameActor* actor, u16 mask)
{
    u16 result;
    s32 i;

    result = 0;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout) {
        case 0:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap0[i];
                }
            }
            break;
        case 1:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap1[i];
                }
            }
            break;
        case 2:
            if (actor->mode == GAME_ACTOR_MODE_NORMAL && actor->state >= 2) {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2Alt[i];
                    }
                }
            } else {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2[i];
                    }
                }
            }
            break;
    }
    return result;
}

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
