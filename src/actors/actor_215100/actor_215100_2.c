#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_215100.h"
#include "actors/actors_shared_80132614.h"
#include "actors/actors_shared_801326ac.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
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
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Eight-byte character appearance record `func_actor_215100_8014AA54` parks in
/// `D_actor_215100_8015E678` before it starts the actor's caption script.
///
/// That function copies its argument here whole and then only reads `field_5`:
/// non-zero means the character has already been committed, so it returns 2 and
/// leaves the record alone. The bytes are otherwise opaque to decompiled code
/// except through `func_actor_215100_8014A5C0`, which copies `field_0`,
/// `field_2` and `field_3` out one at a time into the task it spawns.
typedef struct Actor215100CharRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
    /* 0x4 */ u8 field_4;
    /* 0x5 */ u8 field_5; // non-zero: the character is already committed
    /* 0x6 */ u8 field_6;
    /* 0x7 */ u8 field_7;
} Actor215100CharRec;
STATIC_ASSERT_SIZEOF(Actor215100CharRec, 0x8);

/// One entry of the caption schedule `func_actor_215100_8014AFAC` scans while
/// the actor waits to be talked to.
///
/// The entry whose window contains the session's caption clock
/// (`GameSession::sceneClock`, which that function ticks down once the caption
/// system goes idle) names the script to start and the line key to start it at:
/// it is taken when `field_0 * 30 >= clock` and `field_4 * 30 < clock`, and the
/// table is ordered by descending `field_0`, so the first match wins. A
/// `field_0` of -1 terminates the scan. The table itself lives in the overlay's
/// trailing data (`D_actor_215100_80154514`), not in this unit.
typedef struct Actor215100CapWindow {
    /* 0x0 */ s32 field_0; // window upper bound, x30; -1 terminates the table
    /* 0x4 */ s32 field_4; // window lower bound, x30
    /* 0x8 */ s32 field_8; // caption script index, the `func_actor_215100_8014B2B8` arg0
    /* 0xC */ s32 field_C; // the line key to start that script at, its arg1
} Actor215100CapWindow;
STATIC_ASSERT_SIZEOF(Actor215100CapWindow, 0x10);

/// `Task` as this overlay's caption actor reads it in
/// `func_actor_215100_8014AFAC`: the dispatcher index, and the low half of
/// `Task::spawnArg1` — the task's own line delay, handed to
/// `func_actor_215100_8014B2B8` as its `arg2`. The rest of the overlay passes
/// the whole `Task` around.
typedef struct Actor215100 {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  state;
    /* 0x34 */ s16  spawnArg1Lo;
} Actor215100;

void func_actor_215100_8014C874(Task* task);
void func_actor_215100_8014CA80(GpEnemy* enemy, Task* task);
void func_actor_215100_8014CB04(Task* task);
void func_actor_215100_8014CB2C(Task* task);
void func_actor_215100_8014CBB8(Task* task);
void func_actor_215100_8014CC04(Task* task);
void func_actor_215100_8014CC7C(Task* task);

s32 func_actor_215100_8014B3C8(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
s16 func_actor_215100_8014C17C(u16* arg0, s32 arg1);
s32 func_actor_215100_8014C360(u16* arg0);

/// Imports from the 0x80180000 overlay; no header names them yet.
void              func_80180390(s32 arg0);
void              func_801811C0(s16 arg0);
void              func_801848B4(void);
void              func_80184954(void);
extern TaskDesc   D_80185384;
extern TaskDesc   D_801856B8;
extern s8         D_8007216C;
extern u8         D_80071085;
extern u8         D_80071075;
extern Task*      D_8018E0C4;
extern TaskDesc   D_actor_215100_8014CF6C;
extern s16        D_80071076;
extern s8         D_80073BAE;
extern TaskDesc   D_actor_215100_8014E13C;
extern s32        D_actor_215100_8014E370;
extern s32        D_actor_215100_8014E8F8;
extern s32        D_actor_215100_8014EA90;
extern s32        D_actor_215100_8014EB08;
extern s32        D_actor_215100_8014EBE0;
extern TaskDesc   D_actor_215100_801544FC;
extern TaskDesc   D_actor_215100_80154508;
extern Task*      D_actor_215100_8015E64C;
extern s32        D_actor_215100_8014D038;
extern s32        D_actor_215100_8014D03C;
extern s32        D_actor_215100_8014D044;
extern s32        D_actor_215100_80153ED4;
extern s32        D_actor_215100_80153FDC;
extern s32        D_actor_215100_801543E4;
extern TaskDesc   D_actor_215100_8015E5D0[];
extern u8         D_actor_215100_8015E5E8[];
extern GpMsgEntry D_actor_215100_8015E5A0[];
/// Glyph metrics table this overlay's caption metrics are read out of, the
/// counterpart of gameplay's `Gp_CapGlyphs`. `func_actor_215100_8014B1B0`
/// stores it and `func_actor_215100_8014C360` indexes it with a text stream's
/// `code & 0x3FF`.
extern GlyphUvwh* D_actor_215100_8015E654;
extern GlyphUvwh  D_8010FB70[];
/// Caption script table, and the script currently being played back with the
/// entry it is up to.
extern Actor215100Caption** D_actor_215100_8015E650;
extern Actor215100Caption*  D_actor_215100_8015E658;
extern s16                  D_actor_215100_8015E65C;
extern s16                  D_actor_215100_8015E65E;
extern s16                  D_actor_215100_8015E660;
extern s16                  D_actor_215100_8015E662;
extern s16                  D_actor_215100_8015E664;
extern s16                  D_actor_215100_8015E666;
/// Frames left before the caret starts drawing.
extern u8 D_actor_215100_8015E66C;
/// Caret grey level (pulses between 9 and 15) and its direction flag.
extern s32 D_actor_215100_801545E4;
extern s32 D_actor_215100_801545E8;
/// Caret position.
extern u16                D_actor_215100_8015E668;
extern u16                D_actor_215100_8015E66A;
extern s32                D_actor_215100_8015E670;
extern s16                D_actor_215100_801544EC;
extern s16                D_actor_215100_801544EE;
extern Actor215100CharRec D_actor_215100_8015E678;
/// Caption schedule `func_actor_215100_8014AFAC` scans, terminated by a -1
/// `field_0`.
extern Actor215100CapWindow D_actor_215100_80154514[];
extern u8                   D_801153F4;
extern u8                   D_80115690;
void                        func_actor_215100_8014B0D4(void);
s32                         func_actor_215100_8014B1B0(GpCapFile* file);
s32                         func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
s16                         func_actor_215100_8014BDFC(u16* arg0);
s16                         func_actor_215100_8014C06C(u16* arg0);
s16                         func_actor_215100_8014C298(u16* arg0);
s32                         func_actor_215100_8014C418(s32 arg0);
void                        func_actor_215100_8014BEE8(void);

extern u8 D_80072729;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Steps `coord` `amount` units along its local Z axis, unless movement is
/// frozen. The direction is staged on the scratchpad stack.
static __inline__ void Actor215100_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 == 1) {
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    Gfx_MatrixCol2(&coord->coord, vec);
    VectorNormalSS(vec, vec);
    gte_lddp(amount);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    coord->coord.t[0]          += head[-1].vx;
    coord->coord.t[1]          += vec->vy;
    coord->coord.t[2]          += vec->vz;
    coord->flg                  = 0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}

/// Arms the weapon pickup at this actor's spot while the event flag
/// `D_actor_215100_8014D038` is up and the story step has reached 3. A session
/// leave (`gGameSession->at4.loc.view == 0x12`) drops the `func_80180390` hold and
/// `D_actor_215100_8014D03C` with it, sub-states 2 and 3 of
/// `Gp_StateC08.field_A` start the 0x3C-frame cooldown in
/// `D_actor_215100_8014D044`, and while that cooldown runs the function only
/// ticks it down.
///
/// Otherwise the player has to be standing in the zone — its model root's X
/// below -0x1806 and its Z inside [0x10CD, 0x1644) — not aiming
/// (`GameActor.field_954 != 2`), with the caption system idle, `D_80115768`
/// and `D_80071075` clear, its yaw inside one of the two 0x3FF-wide windows
/// opening at 0x201 and 0xA01, and one of the 0x1000 / 0x4000 pad masks held.
/// Either mask runs the handoff `func_actor_215100_8014AA54` uses: the weapon
/// message, caption command 0x14 and the scene task `D_actor_215100_8014CF6C`.
void func_actor_215100_8014A398(void)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            z;
    s32            facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if (D_actor_215100_8014D038 != 0) {
        if (D_actor_215100_8015E670 >= 3) {
            if (gGameSession->at4.loc.view == 0x12) {
                func_80180390(0);
                D_actor_215100_8014D03C = 0;
            }
            if ((u32)((u8)Gp_StateC08.field_A - 2) < 2U) {
                D_actor_215100_8014D044 = 0x3C;
            }
            if (D_actor_215100_8014D044 != 0) {
                D_actor_215100_8014D044 -= 1;
                return;
            }
            if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (D_actor_215100_8014D03C == 0) &&
                (D_80115768 == 0) && (coord->coord.t[0] < -0x1806)) {
                z = coord->coord.t[2];
                if (z < 0x1644) {
                    if ((z >= 0x10CD) && (Gp_StateC08.field_A != 1) && (D_80071075 == 0)) {
                        facing = (u16)actor->field_52 & 0xFFF;
                        if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                            if ((u32)(facing - 0xA01) < 0x3FFU) {
                                Gp_MsgPlayerWeapon(0);
                                D_801153F4 = 1;
                                Gp_RunCapCmd(0x14, 0);
                                D_80115690 = 1;
                                Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 0, 0);
                            }
                        }
                        if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                            Gp_MsgPlayerWeapon(0);
                            D_801153F4 = 1;
                            Gp_RunCapCmd(0x14, 0);
                            D_80115690 = 1;
                            Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 0, 0);
                        }
                    }
                }
            }
        }
    }
}

/// Watches the caption system while the actor waits to be talked to.
///
/// State 0 first honours the spawn argument: `spawnArg1 == 2` means the actor
/// was placed already committed, so it just steps to state 1, and only
/// `spawnArg1 == 0` is the interactive case. Otherwise it waits for
/// `Gp_CapBusy` to drop and switches on the key `Gp_GetCapEventKey` returns.
/// Key 1 is the plain "talk to me" — it takes the player's weapon away and
/// clears `D_801153F4`; every other key ends the encounter, and which ending
/// depends on `spawnArg1`: non-zero plays caption command 0x17 behind story
/// flag 0xED and steps to state 1, while zero starts the full ending from here
/// (the caption system is stopped, the scene task `D_8018E0C4` gets its exit,
/// the sound plays and the weapon is taken). All of those finish by killing
/// this task.
///
/// State 1 commits the character to the save slot once the caption system is
/// idle again: it copies `D_actor_215100_8015E678`'s appearance bytes into
/// `Mc_SaveData`, clears the inventory, then spawns task 0x11 and kills itself.
void func_actor_215100_8014A5C0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1 == 2) {
                arg0->state = 1;
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 1) {
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                taskKill(arg0);
                break;
            }
            if (arg0->spawnArg1 != 0) {
                if (GameFlag_GetNibble(0xED) != 0) {
                    Gp_RunCapCmd1(0x17);
                }
                gGameSession->field_126 = 1;
                arg0->state            += 1;
                break;
            }
            if (D_actor_215100_8015E670 == 3) {
                Gp_StateC08.field_6 &= 0xFD;
            }
            D_actor_215100_8014D038 = 0;
            func_80180390(1);
            D_actor_215100_8014D03C = 1;
            Task_CallExit(D_8018E0C4);
            gGameSession->field_126  = 1;
            gGameSession->flowFlags |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            Gp_MsgPlayerWeapon(1);
            D_801153F4 = 0;
            taskKill(arg0);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                D_80073BAE             = 3;
                Mc_SaveData.sceneEvent = 1;
                Gp_ClearInventory();
                gGameSession->hideHud = 1;
                SndEvt_EnqueueType6(0x51140005, 0, 0);
                D_80071076               = 1;
                Mc_SaveData.at4.loc.area = D_actor_215100_8015E678.field_0;
                Mc_SaveData.at4.loc.warp = D_actor_215100_8015E678.field_2;
                Mc_SaveData.at4.loc.room = D_actor_215100_8015E678.field_3;
                Task_Spawn(0, 0x11, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `Gp_CapBusy` / `Gp_GetCapEventKey`, and on key 2 hands the scene task
/// `D_8018E0C4` its exit and steps to state 1, while any other key kills the
/// task outright. State 1 starts the caption playback and steps to state 2,
/// which commits the ending: it flags the save-slot session, plays the sound,
/// clears the actor's own 0x97B, drops the story flag the sibling
/// `func_actor_215100_8014A908` sets, and releases the display reference.
void func_actor_215100_8014A7C4(Task* arg0)
{
    GameActor* actor;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 2) {
                Task_CallExit(D_8018E0C4);
                arg0->state++;
            } else {
                taskKill(arg0);
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            D_8007216C = 8;
            func_801811C0(0);
            arg0->state++;
            break;
        case 2:
            gGameSession->field_126  = 1;
            gGameSession->flowFlags |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            actor->field_97B        = 0;
            D_actor_215100_8014D038 = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_StateC08.field_6 &= 0xFD;
            if (D_80071085 != 0) {
                Display_ReleaseRef();
            }
            taskKill(arg0);
            break;
    }
}

void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670 < 3) {
        D_8007216C = 8;
        func_801811C0(0);
    } else {
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.field_6 &= 0xFD;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
        gGameSession->field_126 = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        gGameSession->flowFlags |= 0x80;
    }
    if (D_actor_215100_8015E670 < 3) {
        Gp_RunCapCmd(0x1D, 3);
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

/// Hands the actor off to its caption script, or starts one, depending on
/// whether the script for the current story flag has already run.
///
/// The `else` arm is a `do { } while (0)` whose `break` is the "already
/// committed" exit. It is not vestigial: the loop notes it emits make `reorg`
/// mark that branch's label as leaving a loop, so the delay-slot pass predicts
/// it not-taken and fills its slot from the fall-through rather than from the
/// shared `return 2` tail. Without the loop the branch reaches the same label
/// by a copied `li v0,2`, one instruction longer.
s32 func_actor_215100_8014AA54(Actor215100CharRec* arg0)
{
    if (D_actor_215100_8014D038 != 0) {
        if (arg0->field_5 != 0) {
            return 2;
        }
        D_actor_215100_8015E678 = *arg0;
        Gp_MsgPlayerWeapon(0);
        D_801153F4 = 1;
        Gp_RunCapCmd(0x14, 0);
        D_80115690 = 1;
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 1, 0);
    } else {
        do {
            if (GameFlag_GetNibble(0xED) == 0) {
                return 1;
            }
            if (arg0->field_5 != 0) {
                break;
            }
            D_actor_215100_8015E678 = *arg0;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(0x17);
            Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 2, 0);
        } while (0);
    }
    return 2;
}

void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_80184954();
        return;
    }
    Gp_SpawnIfCapIdle(0x11, 1);
}

void func_actor_215100_8014ABAC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            if (GameFlag_GetNibble(0x121) == 0) {
                GameFlag_SetNibble(0x121, 1);
                func_800E3FAC(0xA2, 0x3A);
                func_800E8634((s32)&D_actor_215100_8014E370, 1, (s32)&D_actor_215100_8014E8F8);
                arg0->state++;
            } else {
                Task_SpawnFromTable(&D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            break;
        case 2:
            func_800E8614((s32)&D_actor_215100_8014EA90, 1);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                if (Gp_GetCapEventKey() != 0) {
                    arg0->state = 10;
                } else {
                    arg0->state++;
                }
            }
            break;
        case 4:
            Gp_StartCapSlot(8, 0, 0);
            func_800E8614((s32)&D_actor_215100_8014EBE0, 1);
            taskKill(arg0);
            break;
        case 10:
            Gp_StartCapSlot(7, 0, 0);
            func_800E8614((s32)&D_actor_215100_8014EB08, 1);
            arg0->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                Task_SpawnFromTable(&D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_215100_8014AD50(Task* arg0)
{
    if (arg0->killCountdown % 48 == 0) {
        Task_SpawnFromTable(&D_801856B8, 1, 0, 0);
    }
    arg0->killCountdown = arg0->killCountdown + 1;
}

void func_actor_215100_8014ADD8(void)
{
    Task_SpawnFromTable(&D_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_801848B4();
    }
}

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = Task_SpawnFromTable(&D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        taskKill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_215100_8014AF0C(void)
{
    switch (GameFlag_GetNibble(0xF5)) {
        case 0:
            GameFlag_SetNibble(0xF5, 1);
            func_800E8614((s32)&D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_215100_80153FDC, 0);
            GameFlag_SetNibble(0xF5, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_215100_801543E4, 0);
            break;
    }
}

/// Drives the caption schedule while the actor waits to be talked to: state 0
/// arms it, and state 1 scans `D_actor_215100_80154514` for the window
/// containing `gGameSession.sceneClock` - the first entry whose `field_0 * 30`
/// has not dropped below the clock and whose `field_4 * 30` has - and, when it
/// finds one, starts that entry's script at its own line key with the task's
/// `spawnArg1` as the line delay. It then ticks the clock down one, unless the
/// caption system is busy or `D_801153F4` is up.
void func_actor_215100_8014AFAC(Actor215100* task, s32 arg1)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            script = 0;
            key    = arg1;
            for (i = 0; D_actor_215100_80154514[i].field_0 != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_actor_215100_80154514[i].field_0 * 30 >= time) &&
                    (D_actor_215100_80154514[i].field_4 * 30 < time)) {
                    script = D_actor_215100_80154514[i].field_8;
                    key    = D_actor_215100_80154514[i].field_C;
                    break;
                }
            }
            if (script != 0) {
                func_actor_215100_8014B2B8(script, key, task->spawnArg1Lo);
                func_actor_215100_8014B0D4();
            }
            if ((Gp_CapBusy() == 0) && (D_801153F4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

void func_actor_215100_8014B0D4(void)
{
    if ((D_actor_215100_8015E658 != NULL) &&
        (D_actor_215100_8015E658[D_actor_215100_8015E662].field_8 != -1) &&
        (Gp_CapBusy() == 0)) {
        func_actor_215100_8014B3C8(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8, 0x80, 1,
                                   D_actor_215100_8015E658[D_actor_215100_8015E662].field_0 |
                                       ((D_actor_215100_8015E658[D_actor_215100_8015E662].field_1 & 0x10) * 0x10));
        if (!(D_actor_215100_8015E658[D_actor_215100_8015E662].field_4 & 1)) {
            func_actor_215100_8014BEE8();
        }
    }
}

/// Relocates a caption file in place, the counterpart of gameplay's
/// `Gp_RelocCapFile`, and publishes its glyph and script tables. Returns 0
/// when the "CAP" magic is missing.
s32 func_actor_215100_8014B1B0(GpCapFile* file)
{
    s32            i;
    s32            count;
    s32            flag;
    GpEvt12*       rec;
    s32*           ptr;
    GpCapEvtTable* evts;
    GpCapPtrTable* ptrs;

    if (strncmp(file->magic, "CAP", 3) != 0) {
        return 0;
    }

    i = 0;
    if (file->field_8 > 0) {
        file->field_8  += (s32)file;
        file->field_C  += (s32)file;
        file->field_10 += (s32)file;
        evts            = (GpCapEvtTable*)file->field_C;
        rec             = (GpEvt12*)(evts + 1);
        count           = evts->count;
        if (count > 0) {
            flag = -1;
            do {
                if (rec->field_8 != flag) {
                    rec->field_8 += (s32)file;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        ptrs  = (GpCapPtrTable*)file->field_10;
        i     = 0;
        count = ptrs->count;
        ptr   = (s32*)(ptrs + 1);
        if (count > 0) {
            do {
                if (*ptr != 0) {
                    *ptr += (s32)file;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    D_actor_215100_8015E654 = (GlyphUvwh*)file->field_8;
    D_actor_215100_8015E650 = (Actor215100Caption**)((GpCapPtrTable*)file->field_10 + 1);
    return 1;
}

/// Starts playing the caption script `arg0` picks out of
/// `D_actor_215100_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `func_actor_215100_8014B0D4` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
s32 func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2)
{
    Actor215100Caption* caption;
    s16                 entry;

    caption                 = D_actor_215100_8015E650[arg0];
    D_actor_215100_8015E658 = caption;
    if (caption == NULL) {
        return 1;
    }
    D_actor_215100_8015E666 = arg1;
    entry                   = func_actor_215100_8014C418(1);
    D_actor_215100_8015E662 = entry;
    D_actor_215100_8015E660 = arg2;
    D_actor_215100_8015E65C = func_actor_215100_8014C06C((u16*)D_actor_215100_8015E658[entry].field_8);
    D_actor_215100_8015E65E = func_actor_215100_8014BDFC((u16*)D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E664 = func_actor_215100_8014C298((u16*)D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E66C = 0x1E;
    return 0;
}

s32 func_actor_215100_8014B3C8(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16*       text;
    u16*       body;
    s32        title;
    s16        sc;
    u32        shifted;
    s32        titleWidth;
    s16        lineIdx;
    s16        x;
    s32        y;
    s16        i;
    u16        code;
    s16        centered;
    s32        palette;
    s16        t;
    s16        t2;
    s16        glyphY;
    s32        top;
    POLY_G4*   bg;
    POLY_G4*   bg2;
    DR_MODE*   dm;
    POLY_FT4*  ft;
    POLY_GT4*  gt;
    POLY_GT4*  gt2;
    GlyphUvwh* icon;

    lineIdx = 0;
    title   = arg3;
    text    = (u16*)arg0;
    x       = func_actor_215100_8014C17C((u16*)arg0, 0) - 0xA0;
    y       = (u16)D_actor_215100_8015E65E - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_actor_215100_8015E65C - 0xA7;
    bg->y0 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664;
    bg->x1 = (u16)D_actor_215100_8015E65C - D_actor_215100_8015E65C * 2 + 0xAB;
    bg->y1 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664;
    bg->x2 = (u16)D_actor_215100_8015E65C - 0xA7;
    bg->y2 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664 + (u16)D_actor_215100_8015E664;
    bg->x3 = (u16)D_actor_215100_8015E65C - D_actor_215100_8015E65C * 2 + 0xAB;
    bg->y3 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664 + (u16)D_actor_215100_8015E664;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg2 + 1);
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dm + 1);
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(ft + 1);
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)D_actor_215100_8015E660 - 0x77) - (u16)D_actor_215100_8015E664;
        ft->x0     = (u16)D_actor_215100_8015E65C - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_actor_215100_8015E654[title & 0xFF].h;
        titleWidth = D_actor_215100_8015E654[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_actor_215100_8015E65C + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_actor_215100_8015E654[title & 0xFF].h;
        ft->x2     = (u16)D_actor_215100_8015E65C - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_actor_215100_8015E654[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_actor_215100_8015E65C + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_actor_215100_8015E654[title & 0xFF].u;
        ft->v0     = D_actor_215100_8015E654[title & 0xFF].v;
        ft->u1     = D_actor_215100_8015E654[title & 0xFF].u + D_actor_215100_8015E654[title & 0xFF].w;
        ft->v1     = D_actor_215100_8015E654[title & 0xFF].v;
        ft->u2     = D_actor_215100_8015E654[title & 0xFF].u;
        ft->v2     = D_actor_215100_8015E654[title & 0xFF].v + D_actor_215100_8015E654[title & 0xFF].h;
        ft->u3     = D_actor_215100_8015E654[title & 0xFF].u + D_actor_215100_8015E654[title & 0xFF].w;
        ft->v3     = D_actor_215100_8015E654[title & 0xFF].v + D_actor_215100_8015E654[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_actor_215100_801544EC, D_actor_215100_801544EE);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                      = lineIdx + 1;
            lineIdx                 = t2;
            D_actor_215100_8015E66A = y - 2;
            D_actor_215100_8015E668 = x + 4;
            y                      += func_actor_215100_8014C360(&body[i + 1]);
            if (centered != 0) {
                x = func_actor_215100_8014C17C((u16*)arg0, t2) - 0xA0;
            } else {
                x = (u16)D_actor_215100_8015E65C - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(ft + 1);
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->h;
            ft->x1    = x + icon->w;
            ft->y1    = t - icon->h;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->w;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->w;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->h;
            ft->u3    = icon->u + icon->w;
            ft->v3    = icon->v + icon->h;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->w;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt + 1);
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, D_actor_215100_801544EC, D_actor_215100_801544EE);
            gt->y0    = glyphY - D_actor_215100_8015E654[code & 0x3FF].h;
            gt->x1    = x + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->y1    = glyphY - D_actor_215100_8015E654[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_actor_215100_8015E654[code & 0x3FF].u;
            gt->v0    = D_actor_215100_8015E654[code & 0x3FF].v;
            gt->u1    = D_actor_215100_8015E654[code & 0x3FF].u + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->v1    = D_actor_215100_8015E654[code & 0x3FF].v;
            gt->u2    = D_actor_215100_8015E654[code & 0x3FF].u;
            gt->v2    = D_actor_215100_8015E654[code & 0x3FF].v + D_actor_215100_8015E654[code & 0x3FF].h;
            gt->u3    = D_actor_215100_8015E654[code & 0x3FF].u + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->v3    = D_actor_215100_8015E654[code & 0x3FF].v + D_actor_215100_8015E654[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_actor_215100_801544EC, D_actor_215100_801544EE);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_actor_215100_8015E654[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_actor_215100_8015E660`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0, and the two pins are
/// what that twin carries; unpinned the body lands at 92%.
s16 func_actor_215100_8014BDFC(u16* arg0)
{
    s32                 lineH;
    s32                 total;
    s32                 i;
    s32                 seenBreak;
    u16                 code;
    s32                 shifted;
    register s32        next asm("v1");
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");

    lineH     = 0;
    total     = lineH;
    i         = lineH;
    code      = arg0[0];
    shifted   = code << 16;
    seenBreak = lineH;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        do {
            v0tmp = seenBreak;
            if (shifted >> 16 == -2) {
                if (v0tmp != 0) {
                    if (lineH == 0) {
                        lineH = 2;
                    }
                    total += lineH;
                } else {
                    seenBreak = 1;
                }
                lineH = 0;
            } else if (shifted >> 16 != -3) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)D_actor_215100_8015E654);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            next    = i + 1;
            code    = arg0[(s16)next];
            i       = next;
            shifted = code << 16;
            v0tmp   = -1;
        } while (shifted >> 16 != v0tmp);
    }
    return (s16)(D_actor_215100_8015E660 - total);
}

/// Draws the pulsing "more text" caret: a Gouraud triangle at
/// (`D_actor_215100_8015E668`, `D_actor_215100_8015E66A`) whose grey level
/// ramps up to 15 and back down to 9. Same body as gameplay's `Gp_DrawCapCaret`
/// without the VRAM Y offset.
void func_actor_215100_8014BEE8(void)
{
    POLY_G3* prim;
    s32      c1;
    s32      c2;

    if (D_actor_215100_8015E66C != 0) {
        D_actor_215100_8015E66C -= 1;
        return;
    }
    prim           = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    c1 = (D_actor_215100_801545E4 << 7) / 15;
    setRGB0(prim, c1, c1, c1);
    c1 = (D_actor_215100_801545E4 * 192) / 15;
    c2 = c1;
    setRGB1(prim, c2, c2, c2);
    setRGB2(prim, c2, c2, c2);
    prim->x0 = D_actor_215100_8015E668 + 3;
    prim->y0 = D_actor_215100_8015E66A;
    prim->x1 = D_actor_215100_8015E668;
    prim->x2 = D_actor_215100_8015E668 + 7;
    prim->y1 = D_actor_215100_8015E66A - 7;
    prim->y2 = D_actor_215100_8015E66A - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_actor_215100_801545E8 == 0) {
        D_actor_215100_801545E4 += 1;
        if (D_actor_215100_801545E4 >= 0xF) {
            D_actor_215100_801545E8 = 1;
        }
    } else {
        D_actor_215100_801545E4 -= 1;
        if (D_actor_215100_801545E4 < 9) {
            D_actor_215100_801545E8 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_actor_215100_8015E654`)
/// advances it by that glyph's `w - 1`.
///
/// The three pins are what gameplay's twin carries; leaving them out keeps the
/// block structure and instruction count but moves 71 register choices.
s16 func_actor_215100_8014C06C(u16* arg0)
{
    register s32        lineW asm("t0");
    s32                 maxW;
    s32                 i;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW   = 0;
    maxW    = lineW;
    i       = lineW;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_actor_215100_8015E654;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((lineW << 16) > (maxW << 16)) {
                    maxW = lineW;
                }
                lineW = 0;
                goto do_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)maxW;
    return (0x140 - width) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes, pins included.
s16 func_actor_215100_8014C17C(u16* arg0, s32 arg1)
{
    register s32        lineW asm("t1");
    s32                 selectedW;
    s32                 i;
    s32                 lineIndex;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW     = 0;
    selectedW = lineW;
    i         = lineW;
    lineIndex = lineW;
    code      = arg0[0];
    shifted   = code << 16;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_actor_215100_8015E654;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((s16)lineIndex == arg1) {
                    selectedW = lineW;
                }
                lineW = 0;
                v0tmp = i + 1;
                i     = v0tmp;
                lineIndex++;
                goto after_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
            after_inc:
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)selectedW;
    return (0x140 - width) / 2 - 5;
}

/// Total height of the caption block the text stream `arg0` holds: every `-2`
/// line break adds the line's height (the tallest glyph's `h + 2`, or 2 when
/// the line is empty). Gameplay's `Gp_CapTextHeight` is the same walk plus a
/// final `2 -> 0` clamp. `i` is initialised just before the loop test and
/// declared ahead of `total` so their global-alloc priorities tie and the
/// pseudo order hands `i` $t0.
s16 func_actor_215100_8014C298(u16* arg0)
{
    s32                 lineH;
    s32                 i;
    s32                 total;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;
    s32                 newline;
    s32                 skip;

    lineH   = 0;
    total   = lineH;
    code    = arg0[0];
    shifted = code << 16;
    i       = lineH;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        newline = -2;
        skip    = -3;
        table   = D_actor_215100_8015E654;
        do {
            if (shifted >> 16 == newline) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
                lineH  = 0;
            } else if (shifted >> 16 != skip) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            v0tmp   = i + 1;
            code    = arg0[(s16)v0tmp];
            i       = v0tmp;
            shifted = code << 16;
        } while (shifted >> 16 != -1);
    }
    return total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_actor_215100_8015E654`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_actor_215100_8014C360(u16* arg0)
{
    s32                 height;
    s32                 i;
    s32                 cont;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    GlyphUvwh*          table;
    s32                 next;
    s32                 htmp;
    s32                 v0tmp;

    height  = 0;
    i       = height;
    cont    = 1;
    code    = arg0[0];
    table   = D_actor_215100_8015E654;
    shifted = code << 16;
    for (;;) {
        shifted = shifted >> 16;
        if (shifted == -2) {
            cont = 0;
        } else if (shifted == -1) {
            cont   = 0;
            height = 0xD;
        } else {
            if (shifted >= 0) {
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                if (height < glyph->h + 2) {
                    htmp   = glyph->h;
                    height = htmp + 2;
                    goto do_inc;
                }
                next = i + 1;
            } else {
            do_inc:
                next = i + 1;
            }
            i = next;
            TOUCH_REG(next);
            code = arg0[(s16)next];
        }
        v0tmp = cont;
        TOUCH_REG(v0tmp);
        if (v0tmp == 0) {
            break;
        }
        shifted = code << 16;
    }
    if (height == 0) {
        height = 2;
    }
    return height;
}

s32 func_actor_215100_8014C418(s32 arg0)
{
    s32                 flag;
    s32                 id;
    s32                 base;
    Actor215100Caption* p;

    flag = -1;
    id   = D_actor_215100_8015E666;
    base = (s32)D_actor_215100_8015E658;
    p    = (Actor215100Caption*)(arg0 * sizeof(Actor215100Caption) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_actor_215100_8014C46C(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1 - 1;
    task->spawnArg1 = remaining;
    if (remaining <= 0) {
        taskKill(task);
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C4A8(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1 - 1;
            task->spawnArg1 = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                taskKill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_actor_215100_801544FC, 0, arg2, 0);
}

void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_actor_215100_80154508, arg2, 0, 0);
}

void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                   = 0;
    D_actor_215100_801544EC = arg0;
    D_actor_215100_801544EE = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_actor_215100_8014B1B0((GpCapFile*)D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 0xC, installs the message table whose handlers
/// are the actor's script opcodes, and starts the animation.
void func_actor_215100_8014C660(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GpAreaKey        key;
    Actor215100Work* work;
    Actor215100Work* mem;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    TmdObject*       model;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     place;
    s32              idx;
    u32              raw;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor215100Work*)memCalloc(0x4F8, false);
    work       = (Actor215100Work*)mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_215100_8014CB04;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->flags           = 0;
    obj->otOffset        = 1;
    mem->enemy           = enemy;
    spawned              = Gp_SpawnEnemyFromTable(D_actor_215100_8015E5D0, 1, 0, enemy);
    model                = (TmdObject*)spawned->task->extra;
    raw                  = enemy->placeKey;
    sessionKey           = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage            = sessionKey->stage;
    key.area             = sessionKey->area;
    key.room             = sessionKey->room;
    idx                  = raw >> 12;
    areaByte0            = sessionKey->view;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->field_4F0 = spawned->task;
    work->animId    = 0xC;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_215100_8015E5E8, obj,
                  &work->field_374, work->slots);
    work->state    = 2;
    task->msgTable = D_actor_215100_8015E5A0;
    func_actor_215100_8014C874(task);
    task->state++;
}

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
void func_actor_215100_8014C874(Task* task)
{
    Actor215100Work* work;
    s16              animId;

    work = (Actor215100Work*)task->work;
    if (work->state == 1) {
        func_actor_215100_8014CC7C(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_215100_8014CC04(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            Actor215100_MoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_215100_8014CBB8(task);
        return;
    }
}

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_215100_8014CA2C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_215100_8014C660,
        func_actor_215100_8014CA80,
    };

    fns[task->state](task->spawnArg2, task);
}

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
void func_actor_215100_8014CA80(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_215100_8014C874(task);
    func_actor_215100_8014CB2C(task);
}

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
void func_actor_215100_8014CB04(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model's
/// `flags` bit 0x80 is set or it has no buffer. The position is the root
/// part's world translation, staged on the scratchpad stack.
void func_actor_215100_8014CB2C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks animation slots 1..0x13.
void func_actor_215100_8014CBB8(Task* task)
{
    Actor215100Work* work;
    s32              i;

    work = (Actor215100Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
void func_actor_215100_8014CC04(Task* task)
{
    Actor215100Work* work;
    s32              i;

    work = (Actor215100Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_215100_8014CC7C(Task* task)
{
    Actor215100Work* work;
    s32              i;

    work = (Actor215100Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Script opcode: plays clip `args->animId` (ids from 0x19 up are refused
/// with -1). With `args->withArg` set the slots are reseeded with
/// `args->animArg`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, Actor215100AnimArgs* args)
{
    Actor215100Work* work;

    work = (Actor215100Work*)task->work;
    if (args->animId >= 0x19) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_215100_8014C874(task);
    return 0;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 hides both
/// (`TmdObject::flags` 0) and its absence restores 0x80; bit 1 also sets 0x4.
/// The middle argument is the one every opcode of the table receives.
s32 func_actor_215100_8014CD4C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor215100Work*)task->work)->field_4F0->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode: yaws the actor's root coordinate to `placement->rot.vy`,
/// caching the yaw in the work block, and moves it to `placement->pos`.
s32 func_actor_215100_8014CDB0(Task* task, s32 arg1, ActorsShared80132614Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor215100Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor215100Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode that does nothing.
s32 func_actor_215100_8014CE28(void)
{
    return 0;
}

/// Script opcode "walk to": turns the actor's root coordinate to face
/// `target` horizontally, caching the yaw, and stores the horizontal distance
/// in steps of 12 as `travel` for the step body to walk off.
s32 func_actor_215100_8014CE30(Task* task, s32 arg1, ActorsShared801326acTarget* target)
{
    GsCOORDINATE2*   coord;
    Actor215100Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor215100Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `flg` so it is recomputed from that part.
void func_actor_215100_8014CEF8(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra;
    GsCOORDINATE2*   coord  = obj->coords;
    GsCOORDINATE2*   sub    = &((TmdObject*)parent->extra)->coords[4];
    Actor215100Work* work   = (Actor215100Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->flags    = 0;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
