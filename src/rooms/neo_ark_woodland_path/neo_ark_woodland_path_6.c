#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_woodland_path.h"

#include <psyq/libgs.h>

extern u16 D_801153F6;

void func_neo_ark_woodland_path_8018046C(Task* task, s32 arg1, s32 arg2)
{
    s16 i;
    s16 v;

    if (arg2 > 0) {
        for (i = 0; i < 5; i++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[i] == 0) {
                v                                           = arg2 * 0x6E / 100;
                ((s16*)D_neo_ark_woodland_path_80184A60)[i] = v;
                if (D_neo_ark_woodland_path_8018494C[0] < v) {
                    ((s16*)D_neo_ark_woodland_path_80184A60)[i] = D_neo_ark_woodland_path_8018494C[0];
                }
                if (D_801153F6 >= 2) {
                    Gp_ReleaseStateF0((GpObj20E*)task, 0xD);
                } else {
                    D_neo_ark_woodland_path_80184996 = 1;
                }
                D_neo_ark_woodland_path_8018498E += 0x5A;
                return;
            }
        }
        return;
    }
    D_neo_ark_woodland_path_8018498E += 0x5A;
}

/// Arming state, the sibling of `func_neo_ark_woodland_path_80180C6C` one step
/// earlier in the sequence: it parks its own 0x7DB handler table in the task,
/// folds the slot's spawn count into game flag 0x10C (remembering the slot in
/// 0x10D) and fills the five spawn slots with the room's ceiling - or zero.
/// Same shape as its sibling; only the flags, the slot-count array and the
/// handler table differ.
void func_neo_ark_woodland_path_80180568(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184980[Game_Session->field_9] == 0) {
        task->field_24 = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->field_24                   = D_neo_ark_woodland_path_80184998;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10C);
    nib                              = GameFlag_GetNibble(0x10D);
    if (Game_Session->field_9 != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184980[Game_Session->field_9];
        GameFlag_SetNibble(0x10C, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10D, Game_Session->field_9);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948[2];
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801806D8);

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, NeoArkWoodlandPathMsg* msg)
{
    s32                    result;
    u16                    cmd;
    NeoArkWoodlandPathObj* obj;

    result = 0;
    if (msg->field_0 == 0xB05) {
        cmd = msg->field_2;
        switch (cmd) {
            case 0:
                D_neo_ark_woodland_path_8018498E = -1;
                result                           = 0;
                return result;
            case 2:
                D_neo_ark_woodland_path_80184A5C.field_0 = 5;
                D_neo_ark_woodland_path_80184A5C.field_1 = 0xB;
                D_neo_ark_woodland_path_80184A5C.field_2 = 0xC;
                result                                   = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB,
                                   (s32)&D_neo_ark_woodland_path_80184A5C, 0);
                    obj                                                                  = ((Task*)Gp_LookupSlot4(0))->spawnArg2;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[0] = 5;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord.t[2] = -0x320;
                    if (obj != 0) {
                        *(u16*)((u8*)obj + 0x40)            = D_neo_ark_woodland_path_80184A60[0];
                        D_neo_ark_woodland_path_80184A60[0] = 0;
                        obj->field_4C                       = 0;
                    }
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8->coord,
                                   0x400, 1);
                    D_neo_ark_woodland_path_8018498E = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}

/// Arming state: with no spawns to arm for the session's slot it only advances;
/// otherwise it parks this room's 0x7DB handler table in the task, folds the
/// slot's spawn count into game flag 0x10A (remembering the slot in 0x10B), and
/// fills the five spawn slots with the room's ceiling - or zero.
void func_neo_ark_woodland_path_80180C6C(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184970[Game_Session->field_9] == 0) {
        task->field_24 = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->field_24                   = D_neo_ark_woodland_path_801849F4;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10A);
    nib                              = GameFlag_GetNibble(0x10B);
    if (Game_Session->field_9 != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184970[Game_Session->field_9];
        GameFlag_SetNibble(0x10A, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10B, Game_Session->field_9);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948[2];
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80180DDC);

s32 func_neo_ark_woodland_path_80181474(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_8018147C);

void func_neo_ark_woodland_path_801814D4(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801814E8);

s32 func_neo_ark_woodland_path_8018154C(void)
{
    D_neo_ark_woodland_path_8018498E += 0x5A;
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_80181568);

void func_neo_ark_woodland_path_801815C0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_6", func_neo_ark_woodland_path_801815D4);
