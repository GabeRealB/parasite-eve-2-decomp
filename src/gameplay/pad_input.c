#include "pad_input.h"

#include "common.h"

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

enum {
    PAD_INPUT_LAYOUT_A                    = 0,
    PAD_INPUT_LAYOUT_B                    = 1,
    PAD_INPUT_LAYOUT_C                    = 2,
    PAD_INPUT_MOVEMENT_WALK               = 0,
    PAD_INPUT_MOVEMENT_RUN                = 1,
    PAD_INPUT_PLAYER_STATE_AIM            = 2,
    PAD_INPUT_PLAYER_STATE_ATTACHMENT_USE = 6,
    PAD_INPUT_MENU_UNLOCK_UPDATES         = 8,
    PAD_INPUT_WHEEL_BLOCK_UPDATES         = 4,
    PAD_INPUT_INTERACTION_REARM_DELAY     = 10,
    PAD_INPUT_STICK_RUN_THRESHOLD         = PAD_STICK_FULL_SCALE * 29 / 32,
};

static u16 _padInputRemapButtons(const GameActor* player, u16 buttons);

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

/// Publishes remapped input, applying suppression and an additional logical mask.
///
/// All three samples use active-high physical pad bits. The borrowed live
/// player selects the saved layout's mapping; it must remain valid throughout
/// the three remap calls. `keepMask` selects logical output bits (all bits for
/// the normal publish, all except Triangle during wheel suppression). Each
/// output truncates to the session's u16 mask after both filters. Samples are
/// mapped independently, so the published edges are not recomputed from held
/// buttons. No pointer is retained and suppression state is not changed.
static inline void _padInputPublishButtons(const GameActor* player, u16 heldButtons, u16 pressedButtons, u16 releasedButtons, s32 keepMask)
{
    gGameSession->padHeld     = _padInputRemapButtons(player, heldButtons) & ~Gp_PadSuppressMask & keepMask;
    gGameSession->padPressed  = _padInputRemapButtons(player, pressedButtons) & ~Gp_PadSuppressMask & keepMask;
    gGameSession->padReleased = _padInputRemapButtons(player, releasedButtons) & ~Gp_PadSuppressMask & keepMask;
}

void padInputUpdate(void)
{
    const PadState*     pad;
    const PlayerStatus* playerStatus;
    const Task*         playerTask;
    const GameActor*    player;
    u16                 heldButtons;
    u16                 pressedButtons;
    u16                 releasedButtons;

    pad          = &gPadStates[0];
    playerStatus = &gPlayerStatus;
    playerTask   = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (playerTask == NULL) {
        return;
    }
    player = playerTask->work;
    padScriptClearHalt();
    // Explicit held suppression freezes the automatic menu-lock decision.
    if (Gp_MenuLockHold == false) {
        if (player->mode == GAME_ACTOR_MODE_NORMAL && gGameSession->eventState == 0 && gGameSession->cutsceneHold == 0 &&
            player->state != PAD_INPUT_PLAYER_STATE_ATTACHMENT_USE && playerStatus->hp > 0 && gGameSession->deathVariant == 0) {
            if (Gp_MenuLockDelay > 0) {
                Gp_MenuLockDelay--;
                Gp_MenuLockNow = true;
            } else {
                Gp_MenuLockNow = false;
            }
        } else {
            Gp_MenuLockNow      = true;
            Gp_PadSuppressTimer = PAD_INPUT_WHEEL_BLOCK_UPDATES;
            Gp_MenuLockDelay    = PAD_INPUT_MENU_UNLOCK_UPDATES;
            D_80114D08          = PAD_INPUT_INTERACTION_REARM_DELAY;
        }
        if (Gp_MenuLockNow == true && Gp_MenuLockPrev == false) {
            Gp_MenuLockHold     = false;
            Gp_PadSuppressMask |= PAD_INPUT_SUPPRESS_MENU;
        } else if (Gp_MenuLockNow == false && Gp_MenuLockPrev == true) {
            Gp_MenuLockHold     = false;
            Gp_PadSuppressMask &= (0xFFFF ^ PAD_INPUT_SUPPRESS_MENU);
        }
        Gp_MenuLockPrev = Gp_MenuLockNow;
    }
    // Only suppression edges acquire or release this module's display holds.
    Gp_PadSuppressRise = ~Gp_PadSuppressPrev & Gp_PadSuppressMask;
    Gp_PadSuppressFall = Gp_PadSuppressPrev & ~Gp_PadSuppressMask;
    Gp_PadSuppressPrev = Gp_PadSuppressMask;
    if (gDisplayState.demoScene == DISPLAY_DEMO_NONE) {
        if (Gp_PadSuppressRise & PAD_INPUT_SUPPRESS_MENU) {
            displayAcquireMenuHold();
            Gp_PadSuppressRefs++;
        }
        if (Gp_PadSuppressFall & PAD_INPUT_SUPPRESS_MENU) {
            while (Gp_PadSuppressRefs != 0) {
                displayReleaseMenuHold();
                Gp_PadSuppressRefs--;
            }
        }
    }
    // Analog forward strength selects the saved walk/run modifier before mapping.
    if (pad->inputFormat == PAD_INPUT_FORMAT_ANALOG) {
        heldButtons     = pad->buttons;
        pressedButtons  = pad->pressedButtons;
        releasedButtons = pad->releasedButtons;
        if (pad->stickAxes[PAD_STICK_LEFT_Y] < -PAD_STICK_DIRECTION_THRESHOLD) {
            heldButtons |= PAD_BUTTON_UP;
            if (player->mode != GAME_ACTOR_MODE_NORMAL || player->state < PAD_INPUT_PLAYER_STATE_AIM) {
                if (pad->stickAxes[PAD_STICK_LEFT_Y] < -PAD_INPUT_STICK_RUN_THRESHOLD) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == PAD_INPUT_MOVEMENT_WALK) {
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != PAD_INPUT_LAYOUT_B) {
                            heldButtons |= PAD_BUTTON_CIRCLE;
                        } else {
                            heldButtons |= PAD_BUTTON_SQUARE;
                        }
                    } else {
                        // Layouts A and C share Circle; separate arms preserve allocation.
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout == PAD_INPUT_LAYOUT_B) {
                            heldButtons &= (0xFFFF ^ PAD_BUTTON_SQUARE);
                        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout == PAD_INPUT_LAYOUT_A) {
                            heldButtons &= (0xFFFF ^ PAD_BUTTON_CIRCLE);
                        } else {
                            heldButtons &= (0xFFFF ^ PAD_BUTTON_CIRCLE);
                        }
                    }
                } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == PAD_INPUT_MOVEMENT_RUN) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != PAD_INPUT_LAYOUT_B) {
                        heldButtons |= PAD_BUTTON_CIRCLE;
                    } else {
                        heldButtons |= PAD_BUTTON_SQUARE;
                    }
                }
            }
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                heldButtons |= PAD_BUTTON_RIGHT;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                heldButtons |= PAD_BUTTON_LEFT;
            }
        } else if (pad->stickAxes[PAD_STICK_LEFT_Y] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
            heldButtons |= PAD_BUTTON_DOWN;
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                heldButtons |= PAD_BUTTON_RIGHT;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                heldButtons |= PAD_BUTTON_LEFT;
            }
        } else {
            if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                heldButtons |= PAD_BUTTON_RIGHT;
            } else if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                heldButtons |= PAD_BUTTON_LEFT;
            }
        }
    } else {
        heldButtons     = pad->buttons;
        pressedButtons  = pad->pressedButtons;
        releasedButtons = pad->releasedButtons;
    }
    // Suppression masks address logical buttons, so apply them after remapping.
    _padInputPublishButtons(player, heldButtons, pressedButtons, releasedButtons, ~0);
    if (Gp_PadSuppressTimer != 0) {
        Gp_PadSuppressTimer--;
        _padInputPublishButtons(player, heldButtons, pressedButtons, releasedButtons, ~PAD_BUTTON_TRIANGLE);
    }
}

/// Converts an active-high pad mask to logical buttons for the saved layout.
///
/// Borrows the live player to select layout C's alternate mapping in normal
/// states at or above aiming (2). Each table maps all sixteen input bits to
/// output bit indices; multiple inputs may set the same output. Invalid layouts
/// return zero.
static u16 _padInputRemapButtons(const GameActor* player, u16 buttons)
{
    /// Adds remapped bits from a complete sixteen-entry table of bit indices 0..15.
    ///
    /// Scalar arguments must be side-effect-free; destination and cursor are
    /// distinct lvalues. Arguments are reused per bit; the destination is not cleared.
#define PAD_INPUT_REMAP_BUTTON_BITS(sourceButtons, buttonMap, destinationButtons, bitCursor) \
    do {                                                                                     \
        for ((bitCursor) = 0; (bitCursor) < ARRAY_SIZE(buttonMap); (bitCursor)++) {          \
            if (((sourceButtons) >> (bitCursor)) & 1) {                                      \
                (destinationButtons) |= 1 << (buttonMap)[bitCursor];                         \
            }                                                                                \
        }                                                                                    \
    } while (0)

    u16 mappedButtons;
    s32 buttonBit;

    mappedButtons = 0;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout) {
        case PAD_INPUT_LAYOUT_A:
            PAD_INPUT_REMAP_BUTTON_BITS(buttons, Gp_BtnMap0, mappedButtons, buttonBit);
            break;
        case PAD_INPUT_LAYOUT_B:
            PAD_INPUT_REMAP_BUTTON_BITS(buttons, Gp_BtnMap1, mappedButtons, buttonBit);
            break;
        case PAD_INPUT_LAYOUT_C:
            if (player->mode == GAME_ACTOR_MODE_NORMAL && player->state >= PAD_INPUT_PLAYER_STATE_AIM) {
                PAD_INPUT_REMAP_BUTTON_BITS(buttons, Gp_BtnMap2Alt, mappedButtons, buttonBit);
            } else {
                PAD_INPUT_REMAP_BUTTON_BITS(buttons, Gp_BtnMap2, mappedButtons, buttonBit);
            }
            break;
    }
#undef PAD_INPUT_REMAP_BUTTON_BITS
    return mappedButtons;
}

void padInputChangeSuppression(u8 command, s32 buttonMask)
{
    switch (command) {
        case PAD_INPUT_SUPPRESSION_SET:
        case PAD_INPUT_SUPPRESSION_SET_ALIAS:
            Gp_MenuLockHold     = false;
            Gp_PadSuppressMask |= buttonMask;
            break;
        case PAD_INPUT_SUPPRESSION_SET_AND_HOLD:
            Gp_PadSuppressMask |= buttonMask;
            Gp_MenuLockHold     = true;
            break;
        case PAD_INPUT_SUPPRESSION_CLEAR:
        case PAD_INPUT_SUPPRESSION_CLEAR_ALIAS:
            Gp_MenuLockHold     = false;
            Gp_PadSuppressMask &= ~buttonMask;
        case PAD_INPUT_SUPPRESSION_KEEP:
            break;
    }
}

void padInputResetSuppression(void)
{
    Gp_PadSuppressRefs  = 0;
    Gp_PadSuppressMask  = 0;
    Gp_PadSuppressPrev  = 0;
    Gp_PadSuppressRise  = 0;
    Gp_PadSuppressFall  = 0;
    Gp_MenuLockNow      = false;
    Gp_MenuLockPrev     = false;
    Gp_MenuLockHold     = false;
    Gp_MenuLockDelay    = PAD_INPUT_MENU_UNLOCK_UPDATES;
    Gp_PadSuppressTimer = PAD_INPUT_WHEEL_BLOCK_UPDATES;
}
