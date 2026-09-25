#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_sterilization_room.h"

extern s32     D_shelter_b1_sterilization_room_80188C94;
extern s32     D_shelter_b1_sterilization_room_80188E14;
extern SVECTOR D_shelter_b1_sterilization_room_8018909C[];
extern SVECTOR D_shelter_b1_sterilization_room_80189334[];

extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

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
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 5, 0, 0);
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
/// in `spawnArg1`, scaled through the GTE by `period` and jittered into the
/// velocity `move`. Each tick then moves the coordinate by that velocity
/// and adds `step` to `scale`; while an event is running the tick
/// counter is held instead. The drawn frame advances every `index` ticks
/// and the task is released once ten frames have passed.
void func_shelter_b1_sterilization_room_801823D8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            base;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    work->age++;
    switch (task->state) {
        case 0:
            base             = ((GpEffSpawnArg*)&task->spawnArg1)->field_2;
            work->scale      = ((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF) + 0x180) + base;
            work->angle      = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
            task->spawnArg1 &= 0xFFF;
            work->index      = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 1;
            work->period     = (work->scale >> 5) + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->step       = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF;
            gte_lddp(work->period);
            gte_ldsv(&D_shelter_b1_sterilization_room_80189334[task->spawnArg1 / 16]);
            gte_gpf12();
            vec = &work->move;
            gte_stsv(vec);
            work->move.vx -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->move.vy -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->move.vz -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            task->state    = 1;
        case 1:
            if (Gp_State1C->eventState == 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                work->scale       += work->step;
            } else {
                work->age--;
            }
            func_shelter_b1_sterilization_room_801826F0(coord, (work->age - 1) / work->index,
                                                        work->scale, work->angle);
            if (work->index * 10 - 1 < work->age) {
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
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              v0;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&coord->workm.t[1];
    vz                                        = *(u16*)&coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
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

void func_shelter_b1_sterilization_room_80182B34(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b1_sterilization_room_80183378(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_shelter_b1_sterilization_room_80183718(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-step disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then
/// wedges reaching between that radius and an inner one of
/// `(s16)arg2 * 8 / otz`. Only the centre vertex is lit, on green and blue, at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses.
void func_shelter_b1_sterilization_room_80183B8C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*              head;
    RoomDiscScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDiscScratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDiscScratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDiscScratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDiscScratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDiscScratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
