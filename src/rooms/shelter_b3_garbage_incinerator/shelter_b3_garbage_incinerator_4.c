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
void            RoomsShared801830f0Sub(s16 arg0, s16 arg1, s32 arg2);
void            func_shelter_b3_garbage_incinerator_8017FB80(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F0A8);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F930);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F968);

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
                RoomsShared801830f0Sub(script, key, (s16)task->spawnArg1);
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", RoomsShared801830f0Sub);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_801808A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180994);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180C28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180D44);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180E0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180EC4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F54);
