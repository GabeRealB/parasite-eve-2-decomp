#include "common.h"
#include "gameplay/gameplay.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern s8        D_8007106B;
extern s32       D_80070F70;
extern s32       Gp_LcgState;
extern SVECTOR   D_acropolis_helicopter_landing_pad_80184E80[12];
extern TaskDesc  D_acropolis_helicopter_landing_pad_80184E68;
extern GpSaveLoc D_acropolis_helicopter_landing_pad_80187F90;

void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s16 level);

void func_acropolis_helicopter_landing_pad_8017ED00(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_helicopter_landing_pad_80184E68, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

/// Asks the slot-7 task to warp to stage 0xF, room 3 (message 0x13EE with the
/// room's `GpSaveLoc`); advances on success, otherwise kills the task.
void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0)
{
    Task* slot = Game_GetPtrSlot(7);

    D_acropolis_helicopter_landing_pad_80187F90.field_4 = 1;
    D_acropolis_helicopter_landing_pad_80187F90.field_3 = 1;
    *(u16*)&D_acropolis_helicopter_landing_pad_80187F90 = 0xF;
    D_acropolis_helicopter_landing_pad_80187F90.field_2 = 3;
    D_acropolis_helicopter_landing_pad_80187F90.field_5 = 0;
    if (Gp_DispatchMsg(slot, 0x13EE, (s32)&D_acropolis_helicopter_landing_pad_80187F90,
                       (s32)&D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        arg0->state += 1;
    } else {
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot         = Game_GetPtrSlot(3);
    msg.field_10 = 0;
    msg.field_12 = 0;
    msg.field_14 = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017EE2C);

void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0)
{
    s32   args[2];
    Task* slot;

    slot    = Game_GetPtrSlot(3);
    args[0] = 1;
    args[1] = 3;
    Gp_DispatchMsg(slot, 0x3EF, (s32)args, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        Mc_SaveData.field_6 = D_acropolis_helicopter_landing_pad_80187F90.field_0;
        Mc_SaveData.field_8 = D_acropolis_helicopter_landing_pad_80187F90.field_2;
        Mc_SaveData.field_5 = D_acropolis_helicopter_landing_pad_80187F90.field_3;
        Task_Spawn(0, 0x11, 0, 0);
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EF60(void)
{
    Task_Spawn(2, 0xF, 0, 0);
}

/// Five-state dispatcher of the room's intro task; the handler table is built
/// on the stack. Marks the player actor's `field_930` as 2 before every step.
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0)
{
    GameActor* actor     = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    TaskFunc   states[5] = {
        func_acropolis_helicopter_landing_pad_8017ED50,
        func_acropolis_helicopter_landing_pad_8017EDD4,
        func_acropolis_helicopter_landing_pad_8017EE2C,
        func_acropolis_helicopter_landing_pad_8017EE80,
        func_acropolis_helicopter_landing_pad_8017EEDC,
    };

    actor->field_930 = 2;
    states[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017F010);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017FA30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_801802E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80180664);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80180A64);

/// Effect task for the helipad beacon anchored to `Gp_RoomCoords[4]`. State 0
/// spawns two 0x6005E effects, takes the slot (refcount 4) and seeds its
/// light parameters from the coord and an LCG draw; state 1 spawns two more
/// with arg 0; state 2 fires a 0x6005A effect on 1-in-16 LCG rolls every
/// 64th frame; state 3 releases the state-1C memory. Idle while
/// `Gp_State1C::field_4` is set.
void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;

    base  = &Gp_RoomCoords[4];
    slot  = (GpCoordTail*)&base->coord;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (arg0->state == 3) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (Gp_State1C->field_4 != 0 && arg0->state < 3) {
        return;
    }
    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            base->field_0          = 4;
            slot->field_58         = 0x1900;
            slot->field_5C         = 0x1C20;
            slot->field_50         = 0x800;
            slot->field_52         = 0x800;
            Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
            slot->field_54         = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            slot->coord.coord.t[0] = coord->coord.t[0];
            slot->coord.coord.t[1] = coord->coord.t[1];
            slot->coord.coord.t[2] = coord->coord.t[2];
            base->coord.flg        = 0;
            break;
        case 1:
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            break;
        case 2:
            if (D_80070F70 & 0x40) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2, NULL);
                }
            }
            break;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80181064);

/// Per-frame driver of the twelve helipad lights. Flags `Gp_State1C::field_8`
/// while view 0x12 is active, folds the frame counter `D_80070F70 * 4` into a
/// 0..0xFE triangle wave kept in the effect work's `field_24` (the low two bits
/// are dropped on the rising half so the ramp steps in fours), then runs
/// `func_acropolis_helicopter_landing_pad_8017F010` once per light position.
void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0)
{
    GpEffWork* work = (GpEffWork*)arg0->spawnArg2;
    SVECTOR*   pos;
    s32        i;
    s32        v;
    s32        level;

    if ((Gp_GetViewIndex() & 0xFF) == 0x12) {
        Gp_State1C->field_8 = -1;
    } else {
        Gp_State1C->field_8 = 0;
    }

    v              = D_80070F70 << 2;
    work->field_24 = v;
    if (v & 0x80) {
        level = 0x7F - (v & 0x7F);
    } else {
        level = v & 0x7C;
    }
    work->field_24 = level * 2;

    i   = 0;
    pos = D_acropolis_helicopter_landing_pad_80184E80;
    for (; i < 12; i++) {
        func_acropolis_helicopter_landing_pad_8017F010(pos++, i, work->field_24);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_801819C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80181B64);
