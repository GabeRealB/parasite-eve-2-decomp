#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b1_main_corridor.h"

/// Parameters of an event the corridor's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag). What reads the other fields back is
/// still undecompiled.
typedef struct _CorridorEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _CorridorEvent;

extern s32            func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc       D_shelter_b1_main_corridor_80183098;
extern RoomEventMsg   D_shelter_b1_main_corridor_80185D3C;
extern u8             D_shelter_b1_main_corridor_80185D44;
extern _CorridorEvent D_shelter_b1_main_corridor_80185D68;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_main_corridor/shelter_b1_main_corridor", func_shelter_b1_main_corridor_8017D8F4);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _corridorStartEvent(RoomEventMsg* dst, _CorridorEvent* event)
{
    D_shelter_b1_main_corridor_80185D44 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_main_corridor_80185D3C = *dst;
            D_shelter_b1_main_corridor_80185D68 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_main_corridor_80183098, 0, 0, 0);
            D_shelter_b1_main_corridor_80185D44 = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0xD, 0xE, 0x10 and 0x19 start the room's events on
/// flags 0xEE, 0xEF, 0x12C and 0x12D; 0x18 does the same on flag 0x12E once
/// nibble 0xAC is set, and before that runs CAP command 1. Message 9 runs CAP
/// command 5 once nibble 0x7A reaches 6, and otherwise goes through the rooms'
/// event gate on flag 0xA5. Any other message answers 1.
s32 func_shelter_b1_main_corridor_8017DA8C(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq   req;
    _CorridorEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xD) {
        event.field_0 = 3;
        event.field_4 = 0x540F0001;
        event.flagId  = 0xEE;
        event.field_A = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->msgId == 0xE) {
        event.field_0 = 4;
        event.field_4 = 0x540F0001;
        event.flagId  = 0xEF;
        event.field_A = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->msgId == 0x10) {
        event.field_0 = 6;
        event.field_4 = 0x540F0001;
        event.flagId  = 0x12C;
        event.field_A = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->msgId == 9) {
        if (GameFlag_GetNibble(0x7A) >= 6) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(5);
            }
            return 0;
        }
        req.field_0 = 2;
        req.field_4 = 1;
        req.field_8 = 0;
        req.field_C = 0x540F0001;
        req.flagId  = 0xA5;
        req.itemId  = 0;
        return func_shelter_b1_main_corridor_8017D620(&req, out);
    }
    if (in->msgId == 0x18) {
        if (GameFlag_GetNibble(0xAC) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(1);
            }
            return 2;
        }
        event.field_0 = 8;
        event.field_4 = 0x540F0001;
        event.flagId  = 0x12E;
        event.field_A = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->msgId == 0x19) {
        event.field_0 = 7;
        event.field_4 = 0x540F0001;
        event.flagId  = 0x12D;
        event.field_A = 0;
        return _corridorStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_main_corridor_8017DCEC(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCF4(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCFC(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DD04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 sndId;

    if (arg2 != 0xD) {
        if (arg2 == 0xE) {
            sndId = 0x540F0000 | 0xE;
            goto play;
        }
    } else {
        sndId = 0x540F000D;
    play:
        SndEvt_EnqueueType6(sndId, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_main_corridor/shelter_b1_main_corridor", D_shelter_b1_main_corridor_8017D5F0);
