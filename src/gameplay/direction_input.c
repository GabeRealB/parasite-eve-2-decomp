#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_entry.h"
#include "area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/direction.h"
#include "direction.h"
#include "geometry.h"
#include "hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 D_80114CD0;

u16 Gp_DirFlags;

u16 D_80114CD4;

u16 Gp_DirPhase;

u8 Gp_DirByte;

u8 Gp_DirNibble;

u8 Gp_DirAlt;

u8 Gp_DirAltNibble;

u8 D_80114CDC;

u8 D_80114CDD;

u8 D_80114CDE;

s16 D_80114CE0;

GpSaveLoc Gp_WarpLoc;

s32 D_80114CF0;

s16 D_80114CF4;

u16 Gp_DirFadeLevel;

u8 D_80114CF8;

s32 Gp_AreaIdBits[2];

s16 D_80114D08;

#include "direction_input.h"

#include "gameplay/direction_input.h"

/// Five stage counts, followed by three unexplained nonzero bytes.
/// The tail is retained for review, not interpreted as additional stages.
s8 Gp_AreaIdCounts[8] = {
    17,
    38,
    38,
    49,
    33,
    -31,
    -1,
    34,
};
u8 D_8010CAF8[50] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 };

void func_800AD6BC(void)
{
    Task*            slot;
    PlayerStatus*    cfg;
    u32              flags;
    u32              action;
    u32              mask;
    GpDirActionTable funcs;

    funcs = Gp_DirActionFns;
    cfg   = &Player_Status;
    slot  = gameGetPtrSlot(1);
    if (slot != NULL) {
        if (slot->spawnArg1.value != Mc_SaveData[0].state.at4.loc.view) {
            func_800A7F24();
            D_80114D08 = 0xA;
        }
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        D_80114D08 = 0xA;
    }
    if (D_80114CF8 == 0) {
        if (Gp_StateC08.field_A == 0) {
            gGameSession->dirActionBusy = 0;
            if (D_80114D08 != 0) {
                D_80114D08 = (u16)D_80114D08 - 1;
            }
            if (Gp_TakePendingObj4C(&Gp_DirFlags, &Gp_DirByte, &Gp_DirNibble) != 0) {
                if (D_80114CD0 != (s16)Gp_DirFlags) {
                    D_80114CDC = 1;
                } else {
                    D_80114CDC = 0;
                }
                Gp_DirPhase = 0;
                flags       = Gp_DirFlags;
                mask        = flags & 0x8000;
                if (Gp_StateF0.prefix.bytes.field_1 == 0) {
                    if (mask && (gDisplayState.pendingMode == DISPLAY_MODE_NONE) && !(gGameSession->padPrev & 0x10)) {
                        if (!(flags & 0x4000)) {
                            D_80114CF8 = 1;
                        } else if (Gp_StateF0.prefix.bytes.field_0 != 1) {
                            D_80114CF8 = 1;
                        }
                    } else if (cfg->field_24 != 0) {
                        if (!(gGameSession->padPrev & 0x10)) {
                            if (!(Gp_DirFlags & 0x4000)) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            } else if (Gp_StateF0.prefix.bytes.field_0 != 1) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    D_80114CD0 = (s16)Gp_DirFlags;
    if (D_80114CF8 != 0) {
        gGameSession->dirActionBusy = 1;
        action                      = (u8)Gp_DirFlags;
        if (action != 0xFF) {
            funcs.funcs[action]();
        } else {
            Gp_DirNibble    = 0;
            Gp_DirByte      = 0;
            Gp_DirAltNibble = 0;
            Gp_DirAlt       = 0;
            Gp_DirFlags     = 0;
            D_80114CD4      = 0;
            D_80114CF8      = 0;
        }
    } else {
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        Gp_DirFlags     = 0;
        D_80114CD4      = 0;
    }
    D_80114CDE = Gp_StateF0.prefix.bytes.field_0;
}

void Gp_SetupDirWarp(void)
{
    Task*         slot7;
    Task*         slot3;
    PlayerStatus* cfg;
    GameActor*    actor;
    GpAreaKey*    sess;
    GpWarpRec     rec;
    GpXformArg    msg;
    SVECTOR       pos;
    SVECTOR       pos2;
    s32           stage;
    s32           room;
    s16           ret;

    sess  = &gGameSession->at4.loc;
    stage = sess->stage;
    room  = sess->area;
    slot7 = gameGetPtrSlot(7);
    slot3 = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    actor = slot3->work;

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        return;
    }

    D_80114CF4      = 0;
    Gp_DirFadeLevel = 0;
    rec             = Gp_WarpTables[stage - 1][room - 1][(Gp_DirNibble >> 4) - 1];

    Gp_WarpLoc.field_4       = 1;
    Gp_WarpLoc.field_3       = 1;
    Gp_WarpLoc.field_5       = 1;
    Gp_WarpLoc.prefix.packed = Gp_DirByte;
    Gp_WarpLoc.field_2       = Gp_DirNibble & 0xF;
    Gp_WarpLoc.field_6       = rec.field_36;

    ret        = Gp_DispatchMsgPtrs(slot7, 0x13EE, &Gp_WarpLoc, &Gp_WarpLoc);
    D_80114CF4 = ret;

    switch (ret) {
        case 1:
            if (rec.field_2C != 0) {
                D_80114CF0 = rec.field_2C;
            } else {
                D_80114CF0 = 0;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (rec.player.words.field_0 + 0x800) & 0xFFF;
            if (rec.player.words.field_0 == 0x7800 || rec.player.words.field_0 == 0x7FFF) {
                pos.vx     = -0x5C1;
                pos.vy     = 0;
                pos.vz     = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ(Gp_ActorSlots[0], (GpPosXZ*)&pos);
            } else if (rec.player.words.field_0 == 0x7FFE) {
                msg.rot.vy = actor->field_52;
            }
            Gp_DispatchMsgPtr(slot3, 0x3EE, &msg, 0);
            if (rec.field_35 & 2) {
                Gp_DirFadeLevel = 0x1E;
            }
            Gp_DirPhase++;
            break;

        case 0:
            if (rec.field_30 != 0) {
                D_80114CF0 = rec.field_30;
            } else {
                D_80114CF0 = 0;
            }
            if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                Gp_WarpLoc.field_4       = Gp_StateF0.prefix.bytes.field_0;
                Gp_WarpLoc.field_3       = Gp_StateF0.prefix.bytes.field_0;
                Gp_WarpLoc.field_5       = 0;
                Gp_WarpLoc.prefix.packed = Gp_DirByte;
                Gp_WarpLoc.field_2       = Gp_DirNibble & 0xF;
                Gp_WarpLoc.field_6       = rec.field_36;
                Gp_DispatchMsgPtrs(slot7, 0x13EE, &Gp_WarpLoc, &Gp_WarpLoc);
                D_80114CF8    = 0;
                Gp_DirNibble  = 0;
                Gp_DirByte    = 0;
                Gp_DirFlags   = 0;
                cfg->field_24 = 0;
                if (D_80114CF0 != 0 && cfg->hp > 0) {
                    SndEvt_EnqueueType6(D_80114CF0, 0, 0);
                }
                return;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (rec.player.words.field_0 + 0x800) & 0xFFF;
            if (rec.player.words.field_0 == 0x7800 || rec.player.words.field_0 == 0x7FFF) {
                pos2.vx    = -0x5C1;
                pos2.vy    = 0;
                pos2.vz    = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ(Gp_ActorSlots[0], (GpPosXZ*)&pos2);
            } else if (rec.player.words.field_0 == 0x7FFE) {
                msg.rot.vy = actor->field_52;
            }
            Gp_DispatchMsgPtr(slot3, 0x3EE, &msg, 0);
            Gp_DirPhase++;
            break;

        case 2:
            Gp_WarpLoc.field_4       = 1;
            Gp_WarpLoc.field_3       = 1;
            Gp_WarpLoc.field_5       = 0;
            Gp_WarpLoc.prefix.packed = Gp_DirByte;
            Gp_WarpLoc.field_2       = Gp_DirNibble & 0xF;
            Gp_WarpLoc.field_6       = rec.field_36;
            Gp_DispatchMsgPtrs(slot7, 0x13EE, &Gp_WarpLoc, &Gp_WarpLoc);
            D_80114CF8    = 0;
            Gp_DirNibble  = 0;
            Gp_DirByte    = 0;
            Gp_DirFlags   = 0;
            cfg->field_24 = 0;
            break;
    }
}

void Gp_FadeDirWaitMsg(void)
{
    void* slot;
    u8    fade;

    slot = gameGetPtrSlot(3);
    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
        if (D_80114CF4 != 0) {
            Gp_StateF0.field_4 = 1;
        }
        Gp_DirPhase++;
    }
}

void Gp_CommitWarp(void)
{
    Task*         slot3;
    Task*         slot7;
    PlayerStatus* cfg;
    GpAreaKey*    sess;
    GpWarpRec     rec;
    GpSaveLoc*    loc;
    u8            fade;

    slot3 = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    slot7 = gameGetPtrSlot(7);

    sess = &gGameSession->at4.loc;
    rec  = Gp_WarpTables[sess->stage - 1][sess->area - 1][(Gp_DirNibble >> 4) - 1];

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }

    loc                      = &Gp_WarpLoc;
    loc->field_4             = 1;
    loc->field_3             = 1;
    loc->field_5             = 0;
    Gp_WarpLoc.prefix.packed = Gp_DirByte;
    loc->field_2             = Gp_DirNibble & 0xF;
    loc->field_6             = rec.field_36;
    Gp_DispatchMsgPtrs(slot7, 0x13EE, loc, loc);

    if (D_80114CF0 != 0) {
        if (cfg->hp > 0) {
            SndEvt_EnqueueType6(D_80114CF0, 0, 0);
        }
    }

    if (D_80114CF4 == 0) {
        Gp_DispatchMsg(slot3, 0x3F1, 0, 0);
        D_80114CF8    = 0;
        Gp_DirNibble  = 0;
        Gp_DirByte    = 0;
        Gp_DirFlags   = 0;
        cfg->field_24 = 0;
    } else {
        Gp_DirPhase++;
    }
}

void Gp_WarpPhase4(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (D_80114CF0 == 0 || SndVoice_HasActiveId(D_80114CF0) == 0) {
        Gp_DirPhase++;
    }
}
