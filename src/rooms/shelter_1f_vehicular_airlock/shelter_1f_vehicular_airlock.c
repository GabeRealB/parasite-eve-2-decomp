#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_1f_vehicular_airlock.h"

extern s32 func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc                       D_shelter_1f_vehicular_airlock_80182028;
extern RoomEventMsg                   D_shelter_1f_vehicular_airlock_80182AA8;
extern s8                             D_shelter_1f_vehicular_airlock_80182AB0;
extern Shelter1fVehicularAirlockEvent D_shelter_1f_vehicular_airlock_80182AB4;

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", func_shelter_1f_vehicular_airlock_8017D644);

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, Shelter1fVehicularAirlockEvent* event)
{
    D_shelter_1f_vehicular_airlock_80182AB0 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_vehicular_airlock_80182AA8 = *dst;
            D_shelter_1f_vehicular_airlock_80182AB4 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_vehicular_airlock_80182028, 0, 0, 0);
            D_shelter_1f_vehicular_airlock_80182AB0 = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    Shelter1fVehicularAirlockEvent event;

    *out = *in;
    func_80179B14(in, out);
    if (in->msgId == 3) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        event.field_0 = 4;
        event.field_4 = 0x55020003;
        event.field_8 = 0x15B;
        event.field_A = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    if (in->msgId == 5) {
        event.field_0 = 6;
        event.field_4 = 0x55020001;
        event.field_8 = 0x15A;
        event.field_A = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D988(void)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && GameFlag_GetNibble(0x7A) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", RoomsShared8017d878Table);
