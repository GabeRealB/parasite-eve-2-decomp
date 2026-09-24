#include "common.h"
#include "main/display.h"
#include "main/mem.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "gameplay/1A8.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_sterilization_room.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `gpf 1`. The `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
/// `rtps` with the two leading `nop`s the original emitted.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern TaskDesc D_shelter_b1_sterilization_room_80188504;
extern s32      D_shelter_b1_sterilization_room_8018C340;
extern s32      D_shelter_b1_sterilization_room_80188C94;
extern s32      D_shelter_b1_sterilization_room_80188E14;

extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32  D_shelter_b1_sterilization_room_8018873C;
extern s32  D_shelter_b1_sterilization_room_80188AB4;

extern u32     Gp_LcgState;
extern SVECTOR D_shelter_b1_sterilization_room_8018909C[];
extern SVECTOR D_shelter_b1_sterilization_room_80189334[];

void func_shelter_b1_sterilization_room_801826F0(GsCOORDINATE2* coord, s16 frame, s16 arg2, s16 arg3);

void func_shelter_b1_sterilization_room_801813A0(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(8);
            gGameSession->eventState = 1;
            arg0->state              = arg0->state + 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                func_800E8634((s32)&D_shelter_b1_sterilization_room_80188C94, 0, (s32)&D_shelter_b1_sterilization_room_80188E14);
                GameFlag_SetNibble(0x77, 1);
                gGameSession->restartMode = 0;
            } else {
                gGameSession->eventState = 0;
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            return;
        default:
            gGameSession->eventState = 0;
            taskKill(arg0);
            return;
    }
}

void func_shelter_b1_sterilization_room_801814B0(void)
{
    Mc_SaveData.at4.loc.area = 0x27;
    Mc_SaveData.at4.loc.warp = 3;
    Mc_SaveData.at4.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b1_sterilization_room_801814FC(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            gGameSession->viewDirty = 1;
            arg0->state            += 1;
            break;
        case 1:
            Mc_SaveData.at4.loc.room    = 2;
            gGameSession->at4.loc.room  = 2;
            gGameSession->roomObjsDirty = state;
            arg0->state                += 1;
            break;
        default:
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_sterilization_room_80181588(Task* arg0)
{
    if (arg0->state == 0) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(9);
        arg0->state += 1;
        return;
    }
    Gp_MsgPlayerWeapon(1);
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_801815EC(void)
{
    if (!(D_shelter_b1_sterilization_room_8018C340 & 0x20)) {
        D_shelter_b1_sterilization_room_8018C340 |= 0x20;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80188504, 5, 0, 0);
    }
}

void func_shelter_b1_sterilization_room_80181634(Task* arg0)
{
    D_shelter_b1_sterilization_room_8018C340 = 0;
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_80181658(void)
{
    SndEvt_EnqueueTypeA(0x54100006, 0, 0x24);
    SndEvt_EnqueueTypeA(0x54100007, 0, 0x24);
}

void func_shelter_b1_sterilization_room_80181698(s32 arg0)
{
    Gp_ResetCap();
    if (arg0 == 1) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x2C0, 0x100);
    }
}

void func_shelter_b1_sterilization_room_801816E0(Task* task)
{
    s32 cmd;
    s32 flag;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            if (task->spawnArg1 != 0) {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 8;
                if (flag == 0) {
                    cmd = 7;
                }
                Gp_RunCapCmd1(cmd);
                GameFlag_SetNibble(0x149, 1);
            } else {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 6;
                if (flag != 0) {
                    GameFlag_SetNibble(0x14A, 1);
                    GameFlag_SetNibble(0x151, 1);
                    cmd = 9;
                }
                Gp_RunCapCmd1(cmd);
            }
            task->state++;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b1_sterilization_room_801817EC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_RunCapCmd1(task->spawnArg1);
            task->state = task->state + 1;
            /* fallthrough */
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            break;
    }
}

/// Per-frame task for the room's view-dependent effects. In state 0 it switches
/// on the camera view: some views draw glows at fixed points of the position
/// table, view 6 pulses `Gp_State1C` once, view 14 moves the task to state 1,
/// and views 20-24 set `spawnArg1` and, while no event runs, place one or two
/// points on a random circle (12-bit angle, radius 0x100-0x2FF) around fixed
/// centres and spawn effect 0x60070 at each. In state 1 it spawns effect
/// 0x6017D at random entries of the position table, the entries and the
/// argument depending on the view.
///
/// Three constructs exist only to reproduce the original code generation: the
/// `do { } while (0)` around the view cases, the `(s16)` cast on the `rsin`
/// argument, and the high-half round trip through `hi` / `hiShift` in the
/// angle draw. The last gives `hi` a first life that combine folds away after
/// recording a use of it, so its reuse for the radius draw is a value combine
/// cannot bound and the radius keeps its `s16` sign extension.
void func_shelter_b1_sterilization_room_8018188C(Task* task)
{
    GsCOORDINATE2* coord;

    s32 angle;
    s32 i;
    s32 j;
    s32 idx;

    coord = ((TmdObject*)task->extra)->coords;

    if (task->state == 0) {
        switch (Gp_GetViewIndex() & 0xFF) {
            case 2:
                func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                break;
            case 3:
                func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                break;
                do {
                    case 6:
                        if (task->spawnArg1 != 0) {
                            Gp_PulseState1C();
                            task->spawnArg1 = 0;
                        }
                        break;
                    case 8:
                        func_shelter_b1_sterilization_room_80183378(&D_shelter_b1_sterilization_room_8018909C[0x50], 0x100, 0x440);
                        func_shelter_b1_sterilization_room_80183718(&D_shelter_b1_sterilization_room_8018909C[0x4F], 0x60, 0x80);
                        break;
                    case 14:
                        task->state = 1;
                        break;
                    case 19:
                        func_shelter_b1_sterilization_room_80183378(&D_shelter_b1_sterilization_room_8018909C[0x50], 0x100, 0x440);
                        func_shelter_b1_sterilization_room_80183B8C(&D_shelter_b1_sterilization_room_8018909C[0x4F], 0x60, 0x80);
                        break;
                    case 20:
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x48], 0x200, 0x222);
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x4A], 0x200, 0x222);
                        task->spawnArg1 = 1;
                        if (Gp_State1C->eventState == 0) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi          = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
                            hiShift     = hi << 16;
                            angle       = hiShift >> 16;
                            angle      &= 0xFFF;
                            rnd         = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rnd;
                            hi          = (rnd >> 16) & 0x1FF;
                            radius      = hi + 0x100;
                            vec0->vx    = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy    = 0;
                            vec0->vz    = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx    = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy    = 0;
                            vec1->vz    = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 21:
                        task->spawnArg1 = 1;
                        if (Gp_State1C->eventState == 0) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec1;
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi          = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
                            hiShift     = hi << 16;
                            angle       = hiShift >> 16;
                            angle      &= 0xFFF;
                            rnd         = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rnd;
                            hi          = (rnd >> 16) & 0x1FF;
                            radius      = hi + 0x100;
                            vec1->vx    = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy    = 0;
                            vec1->vz    = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 22:
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        task->spawnArg1 = 1;
                        if (Gp_State1C->eventState == 0) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec1;
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi          = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
                            hiShift     = hi << 16;
                            angle       = hiShift >> 16;
                            angle      &= 0xFFF;
                            rnd         = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rnd;
                            hi          = (rnd >> 16) & 0x1FF;
                            radius      = hi + 0x100;
                            vec1->vx    = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy    = 0;
                            vec1->vz    = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 23:
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                        task->spawnArg1 = 1;
                        if (Gp_State1C->eventState == 0) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi          = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
                            hiShift     = hi << 16;
                            angle       = hiShift >> 16;
                            angle      &= 0xFFF;
                            rnd         = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rnd;
                            hi          = (rnd >> 16) & 0x1FF;
                            radius      = hi + 0x100;
                            vec0->vx    = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy    = 0;
                            vec0->vz    = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx    = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy    = 0;
                            vec1->vz    = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 24:
                        func_shelter_b1_sterilization_room_80182B34(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        task->spawnArg1 = 1;
                        if (Gp_State1C->eventState == 0) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi          = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
                            hiShift     = hi << 16;
                            angle       = hiShift >> 16;
                            angle      &= 0xFFF;
                            rnd         = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rnd;
                            hi          = (rnd >> 16) & 0x1FF;
                            radius      = hi + 0x100;
                            vec0->vx    = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy    = 0;
                            vec0->vz    = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx    = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy    = 0;
                            vec1->vz    = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(0x60070, coord, ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                } while (0);
        }
    } else {
        switch (Gp_GetViewIndex() & 0xFF) {
            case 14:
                if (Gp_State1C->eventState == 0) {
                    for (i = 8; i < 0x10; i += 4) {
                        idx = i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                        Gp_SpawnEff(0x6017D, coord, idx, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
            case 15:
                if (Gp_State1C->eventState == 0) {
                    for (i = 4; i < 0x10; i += 4) {
                        if (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 1) {
                            idx = i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                            Gp_SpawnEff(0x6017D, coord, idx - 0x800000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                        }
                    }
                }
                break;
            case 16:
                if (Gp_State1C->eventState == 0) {
                    for (i = 0; i < 0x40; i += 4) {
                        if (!(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3)) {
                            idx = i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                            Gp_SpawnEff(0x6017D, coord, idx, &D_shelter_b1_sterilization_room_8018909C[idx]);
                        }
                    }
                }
                break;
            case 11:
                func_shelter_b1_sterilization_room_80183378(&D_shelter_b1_sterilization_room_8018909C[0x4C], 0x300, 0x800);
                if (Gp_State1C->eventState == 0) {
                    for (j = 0; j < 0x40; j += 0x10) {
                        for (i = 4; i < 0x10; i += 4) {
                            if (!(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3)) {
                                idx = j + i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                                Gp_SpawnEff(0x6017D, coord, idx + 0x600000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                            }
                        }
                    }
                }
                break;
            case 10:
                func_shelter_b1_sterilization_room_80183378(&D_shelter_b1_sterilization_room_8018909C[0x4D], 0x300, 0x800);
                func_shelter_b1_sterilization_room_80183378(&D_shelter_b1_sterilization_room_8018909C[0x4E], 0x300, 0x800);
                if (Gp_State1C->eventState == 0) {
                    for (j = 0; j < 0x40; j += 0x10) {
                        for (i = 0; i < 0xC; i += 4) {
                            if (!(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3)) {
                                idx = j + i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                                Gp_SpawnEff(0x6017D, coord, idx + 0x600000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                            }
                        }
                    }
                }
                break;
            case 17:
                if (Gp_State1C->eventState == 0) {
                    for (i = 0xC; i < 0x40; i += 0x10) {
                        idx = i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                        Gp_SpawnEff(0x6017D, coord, idx + 0x1800000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
            case 18:
                if (Gp_State1C->eventState == 0) {
                    for (i = 0xC; i < 0x40; i += 0x10) {
                        idx = i + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3);
                        Gp_SpawnEff(0x6017D, coord, idx + 0x1000000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
        }
    }
}

/// Per-frame update of a drifting effect drawn by
/// `func_shelter_b1_sterilization_room_801826F0`. State 0 seeds the work block
/// from the LCG and takes a direction from a table indexed by the 12-bit angle
/// in `spawnArg1`, scaled through the GTE by `field_28` and jittered into the
/// velocity `field_10`. Each tick then moves the coordinate by that velocity
/// and adds `field_2A` to `field_24`; while an event is running the tick
/// counter is held instead. The drawn frame advances every `field_20` ticks
/// and the task is released once ten frames have passed.
void func_shelter_b1_sterilization_room_801823D8(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            base;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    work->field_22++;
    switch (task->state) {
        case 0:
            base             = ((GpEffSpawnArg*)&task->spawnArg1)->field_2;
            work->field_24   = (u16)((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF) + 0x180) + base;
            work->field_26   = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
            task->spawnArg1 &= 0xFFF;
            work->field_20   = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 1;
            work->field_28   = ((s16)work->field_24 >> 5) + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->field_2A   = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF;
            gte_lddp(work->field_28);
            gte_ldsv(&D_shelter_b1_sterilization_room_80189334[task->spawnArg1 / 16]);
            gte_gpf12_real();
            vec = &work->field_10;
            gte_stsv(vec);
            work->field_10.vx -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->field_10.vy -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->field_10.vz -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            task->state        = 1;
        case 1:
            if (Gp_State1C->eventState == 0) {
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
                work->field_24    += work->field_2A;
            } else {
                work->field_22--;
            }
            func_shelter_b1_sterilization_room_801826F0(coord, ((s16)work->field_22 - 1) / (s16)work->field_20,
                                                        work->field_24, work->field_26);
            if ((s16)work->field_20 * 10 - 1 < (s16)work->field_22) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent textured
/// `POLY_FT4` (tpage 0x2B, clut 0x43D0) rotated about the projected point.
/// `frame` picks a 48x48 cell from a 5-column sheet; the half-extent is
/// `arg2 * 47 / otz` and `arg3` is the spin angle.
void func_shelter_b1_sterilization_room_801826F0(GsCOORDINATE2* coord, s16 frame, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                v0;
    s32                ang2;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D0;
        u0          = (s16)(frame % 5) * 0x30;
        v0          = (s16)(frame / 5) * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}
