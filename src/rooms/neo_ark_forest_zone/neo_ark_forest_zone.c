#include "common.h"

#include "gameplay/1A8.h"
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

/// Event parameters copied to the room's pending event `D_..._80182E48`: the
/// cap command the room's event state machine runs, the stage sound, the game
/// flag checked and set when the event starts, and the helper-spawn switch.
typedef struct NeoArkForestZoneEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} NeoArkForestZoneEvent;
STATIC_ASSERT_SIZEOF(NeoArkForestZoneEvent, 0xC);

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern TaskDesc              D_neo_ark_forest_zone_80181DBC;
extern GpSaveLoc             D_neo_ark_forest_zone_80182E38;
extern s8                    D_neo_ark_forest_zone_80182E40;
extern NeoArkForestZoneEvent D_neo_ark_forest_zone_80182E48;

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", func_neo_ark_forest_zone_8017D644);

s32 func_neo_ark_forest_zone_8017D7DC(void)
{
    return 0;
}

/// Latches the room's pending event and starts the controller that runs it:
/// clears the "event running" flag, and once the event's flag nibble is clear
/// (or the event carries no flag) and `dst->field_5` does not ask for the side
/// effects to be suppressed, commits `dst` and the event and spawns the
/// controller task. Answers 2 for a started event, 1 when `field_5` held it
/// back.
static __inline__ s32 NeoArkForestZone_StartEvent(GpSaveLoc* dst, NeoArkForestZoneEvent* event)
{
    D_neo_ark_forest_zone_80182E40 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_forest_zone_80182E38 = *dst;
            D_neo_ark_forest_zone_80182E48 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_forest_zone_80181DBC, 0, 0, 0);
            D_neo_ark_forest_zone_80182E40 = 1;
        }
        return 2;
    }
    return 1;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `func_80179B14`. On a first pass
/// (`field_5` clear, the flag that asks a handler to only report what *would*
/// happen) it also restarts the room's ambience sound. Message 0x1D builds the
/// room's event record - cap command 2, the stage sound, flag 0x140 - and hands
/// it to `NeoArkForestZone_StartEvent`; every other message is not consumed and
/// answers 1.
s32 func_neo_ark_forest_zone_8017D7E4(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    NeoArkForestZoneEvent event;

    *out = *in;
    func_80179B14(in, out);
    if (in->field_5 == 0) {
        SndEvt_EnqueueType7(0x550B0006, 0x3C);
    }
    if (*(u16*)in != 0x1D) {
        return 1;
    }
    event.field_0 = 2;
    event.field_4 = 0x550B0003;
    event.field_8 = 0x140;
    event.field_A = 0;
    return NeoArkForestZone_StartEvent(out, &event);
}

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
        if (GameFlag_GetNibble(0xBD) == 0 && gGameSession->field_9 == visit) {
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
    if (gGameSession->field_9 == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_neo_ark_forest_zone_80181E30, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// Room entry task tick: on the first visit (`gGameSession::field_9 == 1`) with
/// flag 0xBD unset, broadcast message 0x7DB to the room's own task carrying its
/// first payload record, then advance state.
void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (gGameSession->field_9 == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E30, 0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5D8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone", D_neo_ark_forest_zone_8017D5F8);
