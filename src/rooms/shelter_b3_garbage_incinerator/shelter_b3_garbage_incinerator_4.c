#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

#include "main/text.h"
#include "actors/actor_342100.h"
#include "psyq/libgpu.h"

/* Shared in source with actors 342100 (the encounter's fade and spawn) and
   215100 (the caption drawing): their data here, with their types. */
extern u8         D_shelter_b3_garbage_incinerator_80186F70[];
extern TaskDesc   D_shelter_b3_garbage_incinerator_80185BAC;
extern GlyphUvwh  D_8010FB70[];
extern GlyphUvwh* D_shelter_b3_garbage_incinerator_8018FC44;
extern s16        D_shelter_b3_garbage_incinerator_8018FC4C;
extern s16        D_shelter_b3_garbage_incinerator_8018FC4E;
extern s16        D_shelter_b3_garbage_incinerator_8018FC50;
extern s16        D_shelter_b3_garbage_incinerator_8018FC54;
extern u16        D_shelter_b3_garbage_incinerator_8018FC58;
extern u16        D_shelter_b3_garbage_incinerator_8018FC5A;
extern s16        D_shelter_b3_garbage_incinerator_80187180;
extern s16        D_shelter_b3_garbage_incinerator_80187182;

void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0);
void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0);
void func_shelter_b3_garbage_incinerator_8017F968(void);
s32  func_shelter_b3_garbage_incinerator_8017FE74(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
s16  func_shelter_b3_garbage_incinerator_801808A8(u16* arg0);
s16  func_shelter_b3_garbage_incinerator_80180B18(u16* arg0);
s16  func_shelter_b3_garbage_incinerator_80180C28(u16* arg0, s32 arg1);
s32  func_shelter_b3_garbage_incinerator_80180E0C(u16* arg0);

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Work block of the task in `D_shelter_b3_garbage_incinerator_8018FC3C`.
/// `field_2C` is the task animation messages are dispatched to, `child` the
/// task spawned from the room's table, `field_34` the task started from spawn
/// entry 2 when the encounter is armed, `field_38` the last animation set
/// selected, and `field_3A` the arming state.
typedef struct {
    byte  pad_0[0x2C];
    Task* field_2C;
    Task* child;
    Task* field_34;
    s16   field_38;
    s16   field_3A;
} GarbageIncineratorWork;

/// Payload of message 0x3F7: a null-terminated pointer table and the number
/// of live entries the sender counted in it.
typedef struct {
    s32* table;
    s32  count;
} GarbageIncineratorMsg3F7;

/// One window of the caption schedule: while the scene clock lies in
/// (`field_4 * 30`, `field_0 * 30`], caption script `field_8` is started at
/// line key `field_C`. A `field_0` of -1 ends the table.
typedef struct {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    s32 field_C;
} GarbageIncineratorCapWindow;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[];

/// Null-terminated table counted and sent with message 0x3F7 on arming.
extern s32 D_shelter_b3_garbage_incinerator_80186F78[];

/// Table indexed by `field_38 - 0x2F`: each entry is the following animation
/// set less 0x2F, and a negative entry means there is none.
extern s16 D_shelter_b3_garbage_incinerator_80186F88[];

/// Model/animation set installed with `func_800E8614` on arming.
extern u8 D_shelter_b3_garbage_incinerator_80186FB8[];

s32          func_shelter_b3_garbage_incinerator_8017F318(Task* arg0);
extern Task* D_shelter_b3_garbage_incinerator_8018FC3C;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// Main-executable globals with no module header yet: `D_80073BA9` is the base
/// animation-set id and `D_8007218A` selects the alternate range when it is 1.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Caption schedule scanned by `func_shelter_b3_garbage_incinerator_8017FA58`.
extern GarbageIncineratorCapWindow D_shelter_b3_garbage_incinerator_801871A8[];
extern u8                          D_801153F4;
extern u8                          D_80071075;
extern u8                          D_80114CF8;

/// Task table entry spawned once when the controller starts.
extern TaskDesc D_shelter_b3_garbage_incinerator_80187184;
void            func_shelter_b3_garbage_incinerator_8017FB80(void);

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `D_shelter_b3_garbage_incinerator_80186F70` in `Task::msgTable`. States 2 and
/// 3 step `field_2` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `field_4` / `field_6` by 8; once
/// `field_4` passes 0xFF the display mode is switched, `Fs_ImgBuffers` is
/// filled white, the parent work block's `field_24` is raised, and state 5
/// draws the full-screen white `TILE` + `DR_TPAGE` packed into
/// `gGpuPrimCursor` before returning without the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0)
{
    Actor342100FadeWork* work;
    Actor342100FadeWork* alloc;
    Actor342100Work*     parent;
    TILE*                tile;
    DR_TPAGE*            dr;

    work = (Actor342100FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342100FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work           = alloc;
            work->field_6  = 0;
            work->field_4  = 0;
            work->field_2  = 0;
            arg0->msgTable = &D_shelter_b3_garbage_incinerator_80186F70;
            arg0->state   += 1;
            break;
        case 2:
            work->field_2 += 0xA;
            if ((s16)work->field_2 >= 0x51) {
                work->field_2 = 0x50;
                arg0->state   = 1;
            }
            break;
        case 3:
            work->field_2 += 1;
            if ((s16)work->field_2 >= 0x100) {
                work->field_2 = 0xFF;
                arg0->state   = 1;
            }
            break;
        case 4:
            work->field_4 += 8;
            work->field_6 += 8;
            if ((s16)work->field_4 >= 0x100) {
                parent           = (Actor342100Work*)((Task*)arg0->spawnArg2)->work;
                parent->field_24 = 2;
                Display_SetMode(0xD010);
                Mem_Set(Fs_ImgBuffers, 0xFF, 0x25800);
                work->field_6 = 0xFF;
                work->field_4 = 0xFF;
                arg0->state   = 5;
            }
            break;
        case 5:
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt - 16, dr);
            return;
    }
    Fade_DrawOverlay((u8)work->field_2, (u8)work->field_4, (u8)work->field_6, 1);
}

/// Step `field_2C` to the next animation set in the table. Returns 0 when
/// message 0x3ED to it returns nonzero, and 1 otherwise: with no `field_2C`,
/// with `field_38` below 0x2F, or with a negative table entry nothing is sent;
/// else the entry plus 0x2F is recorded in `field_38` and sent with message
/// 0x3E8. The set's block is `D_80073BA9 + 1` when `D_8007218A` is 1 and
/// `D_80073BA9 + 0x22` otherwise.
s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork* msgWork;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    if (work->field_2C == NULL) {
    ret1:
        COMPILER_BARRIER();
        return 1;
    }
    if (Gp_DispatchMsg(work->field_2C, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if (work->field_38 < 0x2F) {
        return 1;
    }
    if (D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] < 0) {
        goto ret1;
    }
    anim              = (u16)D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] + 0x2F;
    msgWork           = (GarbageIncineratorWork*)arg0->work;
    weaponId          = D_80073BA9;
    setId             = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0       = (void*)setId;
    msgWork->field_38 = anim;
    msg.field_4       = anim;
    msg.field_8       = 1;
    msg.field_C       = 0xA;
    msg.field_10      = 0;
    Gp_DispatchMsg(msgWork->field_2C, 0x3E8, (s32)&msg, 0);
    return 1;
}

/// Effect record handed to `func_800FDB18`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern GpEffArg D_shelter_b3_garbage_incinerator_80186F90;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 D_shelter_b3_garbage_incinerator_80186F98[];

/// Global counter whose low bits gate the state-1 picks.
extern s32 D_80070F70;

/// Random-number state rolled once per tick.
extern u32 Gp_LcgState;

/// Each tick rolls the LCG and aims the effect record at one part of the
/// model owned by `gameGetPtrSlot(3)`. State 0 fires with one of the first
/// four parts at scale 0x100 and steps to state 1. State 1 fires only on
/// frames `D_80070F70` lets through: with `spawnArg1` zero, one of the first
/// four parts at scale 0x10 every sixteenth frame; otherwise one of the first
/// sixteen at scale 0x100 every eighth frame.
void func_shelter_b3_garbage_incinerator_8017F410(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot        = gameGetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    idx         = Gp_LcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                                                 &= 3;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1 == 0) {
                if (D_80070F70 & 0xF) {
                    return;
                }
                idx                                                 &= 3;
                D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x10;
                D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
                func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
                return;
            }
            if (D_80070F70 & 7) {
                return;
            }
            idx                                                 &= 0xF;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            return;
    }
}

/// Arms the encounter on state 0: sends `field_2C` message 0x3F7 with the
/// table and its live-entry count, raises `Gp_StateC08.field_6` bit 0,
/// installs the model set, hands slot 6 message 0xFA4, starts spawn entry 2
/// with the task itself and steps to state 1. State 1 returns 1 while
/// `gGameSession->eventState` is clear; every other path calls
/// `func_shelter_b3_garbage_incinerator_8017F318` with the task and returns 0.
s32 func_shelter_b3_garbage_incinerator_8017F588(Task* arg0)
{
    GarbageIncineratorWork*  work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork*  msgWork;
    GarbageIncineratorMsg3F7 msg;
    s32                      n;

    switch (work->field_3A) {
        case 0:
            msgWork = work;
            n       = 0;
            while (D_shelter_b3_garbage_incinerator_80186F78[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.table = &D_shelter_b3_garbage_incinerator_80186F78[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsg(msgWork->field_2C, 0x3F7, (s32)&msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614((s32)&D_shelter_b3_garbage_incinerator_80186FB8, 0);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            work->field_34 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 2, 0, (s32)arg0);
            work->field_3A = work->field_3A + 1;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    func_shelter_b3_garbage_incinerator_8017F318(arg0);
    return 0;
}

/// Does nothing while `gGameSession->field_65`, `Gp_StateC08.field_9`,
/// `D_801153F4` or `D_80114CF8` is set. State 0 allocates and clears the work block (killing the task if that
/// fails), records `gameGetPtrSlot(3)` in `field_2C` and the task in
/// `D_shelter_b3_garbage_incinerator_8018FC3C`, spawns the table entry and,
/// with `spawnArg1` zero, queues sound event 0x54280005. State 1 advances once
/// the scene clock has run out while the player is alive, unless
/// `field_135` is 1 in view 0x21. State 2 advances when
/// `func_shelter_b3_garbage_incinerator_8017F588` returns nonzero.
void func_shelter_b3_garbage_incinerator_8017F6D8(Task* arg0)
{
    GameSession*            session = gGameSession;
    GarbageIncineratorWork* work;
    s32                     ok;
    PlayerStatus*           ps;

    if (session->field_65 != 0 || (s8)Gp_StateC08.field_9 != 0 || D_801153F4 != 0 || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || D_80071075 != 0) {
                return;
            }
            work       = Mem_Malloc(0x40, false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x40);
                work->field_2C                            = gameGetPtrSlot(3);
                D_shelter_b3_garbage_incinerator_8018FC3C = arg0;
            }
            Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80187184, 0, 0xD0, 0);
            if (arg0->spawnArg1 == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
            }
            break;
        case 1:
            ps = &Player_Status;
            if (session->sceneClock > 0) {
                ok = 0;
            } else if (ps->hp <= 0) {
                ok = 0;
            } else if (session->field_135 != 1 || session->at4.loc.view != 0x21) {
                ok = 1;
            } else {
                ok = 0;
            }
            if (!ok) {
                return;
            }
            break;
        case 2:
            if ((s16)func_shelter_b3_garbage_incinerator_8017F588(arg0) == 0) {
                return;
            }
            break;
        default:
            return;
    }
    arg0->state++;
}

void func_shelter_b3_garbage_incinerator_8017F8A4(GarbageIncineratorState* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

/// Select animation set `arg0 + 0x2F`, record it in the work block, and send
/// it to `field_2C` with message 0x3E8. The set's block is `D_80073BA9 + 1`
/// when `D_8007218A` is 1 and `D_80073BA9 + 0x22` otherwise.
void func_shelter_b3_garbage_incinerator_8017F8AC(s32 arg0)
{
    GarbageIncineratorWork* work;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    work           = D_shelter_b3_garbage_incinerator_8018FC3C->work;
    anim           = arg0 + 0x2F;
    weaponId       = D_80073BA9;
    setId          = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0    = (void*)setId;
    work->field_38 = anim;
    msg.field_4    = anim;
    msg.field_8    = 1;
    msg.field_C    = 0xF;
    msg.field_10   = 0;
    Gp_DispatchMsg(work->field_2C, 0x3E8, (s32)&msg, 0);
}

void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_shelter_b3_garbage_incinerator_8017F968(void)
{
    Actor342100Work* work = (Actor342100Work*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    work->field_20 = 0x258;
    work->field_22 = 0x100;
    Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80185BAC, 0, 0, (s32)&work->field_20);
}

void func_shelter_b3_garbage_incinerator_8017F9B4(s32 arg0)
{
    GarbageIncineratorWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54280008, 0, 0);
        Gp_PulseState1C();
        gGameSession->enemyCullZone = 0x10;
        work->child                 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 3, 0, 0);
        return;
    }
    work->child->spawnArg1 = 1;
}

void func_shelter_b3_garbage_incinerator_8017FA3C(void)
{
    D_80073BA0                = 0;
    gGameSession->restartMode = 3;
}

/// Drives the caption schedule: state 0 arms it, and state 1 scans
/// `D_shelter_b3_garbage_incinerator_801871A8` for the first window containing
/// `gGameSession->sceneClock`; when one is found its script is started at its
/// line key with the low half of the task's `spawnArg1`. The clock then ticks
/// down one unless the caption system is busy or `D_801153F4` is set.
void func_shelter_b3_garbage_incinerator_8017FA58(Task* task, s32 arg1)
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
            for (i = 0; D_shelter_b3_garbage_incinerator_801871A8[i].field_0 != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_shelter_b3_garbage_incinerator_801871A8[i].field_0 * 30 >= time) &&
                    (D_shelter_b3_garbage_incinerator_801871A8[i].field_4 * 30 < time)) {
                    script = D_shelter_b3_garbage_incinerator_801871A8[i].field_8;
                    key    = D_shelter_b3_garbage_incinerator_801871A8[i].field_C;
                    break;
                }
            }
            if (script != 0) {
                func_shelter_b3_garbage_incinerator_8017FD64(script, key, (s16)task->spawnArg1);
                func_shelter_b3_garbage_incinerator_8017FB80();
            }
            if ((Gp_CapBusy() == 0) && (D_801153F4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FB80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FC5C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FD64);

s32 func_shelter_b3_garbage_incinerator_8017FE74(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
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
    x       = func_shelter_b3_garbage_incinerator_80180C28((u16*)arg0, 0) - 0xA0;
    y       = (u16)D_shelter_b3_garbage_incinerator_8018FC4E - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
    bg->y0 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x1 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - D_shelter_b3_garbage_incinerator_8018FC4C * 2 + 0xAB;
    bg->y1 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x2 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
    bg->y2 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54 + (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x3 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - D_shelter_b3_garbage_incinerator_8018FC4C * 2 + 0xAB;
    bg->y3 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54 + (u16)D_shelter_b3_garbage_incinerator_8018FC54;
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
        top        = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
        ft->x0     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        titleWidth = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->x2     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u;
        ft->v0     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v;
        ft->u1     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w;
        ft->v1     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v;
        ft->u2     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u;
        ft->v2     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->u3     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w;
        ft->v3     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
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
            t2                                        = lineIdx + 1;
            lineIdx                                   = t2;
            D_shelter_b3_garbage_incinerator_8018FC5A = y - 2;
            D_shelter_b3_garbage_incinerator_8018FC58 = x + 4;
            y                                        += func_shelter_b3_garbage_incinerator_80180E0C(&body[i + 1]);
            if (centered != 0) {
                x = func_shelter_b3_garbage_incinerator_80180C28((u16*)arg0, t2) - 0xA0;
            } else {
                x = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA0;
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
            gt->tpage = getTPage(0, 1, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
            gt->y0    = glyphY - D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->x1    = x + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->y1    = glyphY - D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u;
            gt->v0    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v;
            gt->u1    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->v1    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v;
            gt->u2    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u;
            gt->v2    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->u3    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->v3    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_shelter_b3_garbage_incinerator_8018FC44[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_shelter_b3_garbage_incinerator_8018FC50`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0, and the two pins are
/// what that twin carries; unpinned the body lands at 92%.
s16 func_shelter_b3_garbage_incinerator_801808A8(u16* arg0)
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
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)D_shelter_b3_garbage_incinerator_8018FC44);
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
    return (s16)(D_shelter_b3_garbage_incinerator_8018FC50 - total);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180994);

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_shelter_b3_garbage_incinerator_8018FC44`)
/// advances it by that glyph's `w - 1`.
///
/// The three pins are what gameplay's twin carries; leaving them out keeps the
/// block structure and instruction count but moves 71 register choices.
s16 func_shelter_b3_garbage_incinerator_80180B18(u16* arg0)
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
        table = D_shelter_b3_garbage_incinerator_8018FC44;
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
s16 func_shelter_b3_garbage_incinerator_80180C28(u16* arg0, s32 arg1)
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
        table = D_shelter_b3_garbage_incinerator_8018FC44;
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180D44);

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_shelter_b3_garbage_incinerator_8018FC44`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_shelter_b3_garbage_incinerator_80180E0C(u16* arg0)
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
    table   = D_shelter_b3_garbage_incinerator_8018FC44;
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180EC4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F54);
