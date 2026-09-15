#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern s32      D_neo_ark_forest_zone_80181DC8;
extern s32      D_neo_ark_forest_zone_80181E30;
extern s32      D_neo_ark_forest_zone_80181E38;
extern Task*    D_neo_ark_forest_zone_80181E68;
extern u8       D_neo_ark_forest_zone_80181E6C[];
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

/// Room message handler: on the first-visit sub-id (`field_2 == 1`) with flag
/// 0xBD unset and the session's visit count equal to that sub-id, latches flag
/// 0xBD and starts the room's fade with the record at `D_..._80181E6C`. Then
/// forwards the message to the room's own task, answering -1 while that task
/// does not exist yet.
s32 func_neo_ark_forest_zone_8017D958(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 visit;

    visit = in->field_2;
    if (visit == 1) {
        if (GameFlag_GetNibble(0xBD) == 0 && Game_Session->field_9 == visit) {
            GameFlag_SetNibble(0xBD, 1);
            func_800E8614((s32)D_neo_ark_forest_zone_80181E6C, 0);
        }
    }
    if (D_neo_ark_forest_zone_80181E68 != NULL) {
        return Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, arg1, (s32)in, (s32)out);
    }
    return -1;
}

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
