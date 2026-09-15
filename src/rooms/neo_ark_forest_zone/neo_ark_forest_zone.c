#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s32      D_neo_ark_forest_zone_80181DC8;
extern s32      D_neo_ark_forest_zone_80181E30;
extern s32      D_neo_ark_forest_zone_80181E38;
extern Task*    D_neo_ark_forest_zone_80181E68;
extern TaskDesc D_neo_ark_forest_zone_80182E18;

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D644);

s32 func_neo_ark_forest_zone_8017D7DC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D7E4);

s32 func_neo_ark_forest_zone_8017D950(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D958);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017DA14);

/// Once the room's own task exists, broadcast message 0x7DB to it, carrying
/// the room's payload record as `arg2`.
void func_neo_ark_forest_zone_8017DA48(void)
{
    if (D_neo_ark_forest_zone_80181E68 != 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E38, 0);
    }
}

/// Room setup task tick: install the message table and pointer slot, start the
/// ambience, spawn the room's own task, and on the first visit (`field_9 == 1`)
/// with flag 0xBD unset have the slot-4 task relay message 0x7DA carrying the
/// first payload record. Then advance state.
void func_neo_ark_forest_zone_8017DA80(Task* arg0)
{
    arg0->field_24 = &D_neo_ark_forest_zone_80181DC8;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550B0006, 0, 0);
    D_neo_ark_forest_zone_80181E68 = Task_SpawnFromTable(&D_neo_ark_forest_zone_80182E18, 0, 0, 0);
    if (Game_Session->field_9 == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_neo_ark_forest_zone_80181E30, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// Room entry task tick: on the first visit (`Game_Session::field_9 == 1`) with
/// flag 0xBD unset, broadcast message 0x7DB to the room's own task carrying its
/// first payload record, then advance state.
void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (Game_Session->field_9 == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E30, 0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5D8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5F8);
