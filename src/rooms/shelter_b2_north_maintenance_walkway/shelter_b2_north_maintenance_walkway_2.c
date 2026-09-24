#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_north_maintenance_walkway.h"

/// The pair of cutscene blocks the walkway's scene hands to `func_800E8634`.
extern s32 D_80165354;
extern s32 D_80165834;

/// Area records applied once the walkway's scene has started.
extern GpAreaApplyRec D_shelter_b2_north_maintenance_walkway_80186380[];

/// The walkway's message table, installed as the room task's `msgTable`.
extern GpMsgEntry D_shelter_b2_north_maintenance_walkway_80183B60[];

extern void func_8016268C(void);

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48;
extern u8       D_shelter_b2_north_maintenance_walkway_801863B0;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _walkwayStartEvent(RoomEventMsg* dst, _WalkwayEvent* event)
{
    D_shelter_b2_north_maintenance_walkway_801863B0 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_north_maintenance_walkway_801863A8 = *dst;
            D_shelter_b2_north_maintenance_walkway_801863C4 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_north_maintenance_walkway_80183B48, 0, 0, 0);
            D_shelter_b2_north_maintenance_walkway_801863B0 = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1D goes through the room's event gate on flag
/// 0xA8 with item 0x22 as prerequisite, answering 2 where the gate answers 0
/// and marking item 0x122 seen when the gate started the event. Message 0x20
/// starts the room's own event on flag 0x137; any other message answers 1.
s32 func_shelter_b2_north_maintenance_walkway_8017DA88(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq  req;
    _WalkwayEvent event;
    s32           result;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1D) {
        req.field_0 = 3;
        req.field_4 = 1;
        req.field_8 = 0x541E0003;
        req.field_C = 0x541E0001;
        req.flagId  = 0xA8;
        req.itemId  = 0x22;
        result      = func_shelter_b2_north_maintenance_walkway_8017D7B4(&req, out);
        if (result == 0) {
            result = 2;
        }
        if (D_shelter_b2_north_maintenance_walkway_801863C0 != 0) {
            Gp_SetItemSeenBit(0x122, 1);
        }
        return result;
    }
    if (in->msgId != 0x20) {
        return 1;
    }
    event.field_0 = 4;
    event.field_4 = 0x541E0004;
    event.flagId  = 0x137;
    event.field_A = 0;
    return _walkwayStartEvent(out, &event);
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC44(void)
{
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC4C(void)
{
    return 0;
}

/// Room message handler. On the visit whose sub-id (`field_2`) is 1, agrees with
/// the session's own sub-id and has not yet latched nibble 0x84, it starts the
/// cutscene pair, runs `func_800E3FAC(0xA2, 0x20)`, latches the nibble and
/// applies the room's area records. The outgoing record is never written.
s32 func_shelter_b2_north_maintenance_walkway_8017DC54(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x84) == 0 && gGameSession->at4.loc.place == subId) {
        func_800E8634((s32)&D_80165354, 0, (s32)&D_80165834);
        func_800E3FAC(0xA2, 0x20);
        GameFlag_SetNibble(0x84, 1);
        Gp_ApplyAreaRecs(D_shelter_b2_north_maintenance_walkway_80186380);
    }
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x541E0000 | 7, 0, 0);
    }
    return 0;
}

void func_shelter_b2_north_maintenance_walkway_8017DD18(Task* task)
{
    task->msgTable = D_shelter_b2_north_maintenance_walkway_80183B60;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_8016268C();
    }
    task->state = task->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", D_shelter_b2_north_maintenance_walkway_8017D5F4);
