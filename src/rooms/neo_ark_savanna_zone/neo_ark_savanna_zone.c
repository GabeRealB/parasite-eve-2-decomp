#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/neo_ark_savanna_zone.h"

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", D_neo_ark_savanna_zone_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D5E4);

/// Latches the room's pending event and starts the controller that runs it:
/// clears the "event running" flag, and once the event's flag nibble is clear
/// (or the event carries no flag) and `dst->field_5` does not ask for the side
/// effects to be suppressed, commits `dst` and the event and spawns the
/// controller task. Answers 2 for a started event, 1 when `field_5` held it
/// back.
static __inline__ s32 NeoArkSavannaZone_StartEvent(GpSaveLoc* dst, NeoArkSavannaZoneEvent* event)
{
    D_neo_ark_savanna_zone_80180998 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_savanna_zone_80180990 = *dst;
            D_neo_ark_savanna_zone_8018099C = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_savanna_zone_8017F9A0, 0, 0, 0);
            D_neo_ark_savanna_zone_80180998 = 1;
        }
        return 2;
    }
    return 1;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `func_80179B14`. Messages 0x13 and
/// 0x15 build the room's event record - cap command 3 / 2, the stage sound and
/// flag 0x15E / 0x15F - and hand it to `NeoArkSavannaZone_StartEvent`; every
/// other message is not consumed and answers 1.
s32 func_neo_ark_savanna_zone_8017D77C(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    NeoArkSavannaZoneEvent event;
    s32                    cmd;
    s32                    snd;
    s16                    flag;

    *out = *in;
    func_80179B14(in, out);
    if (*(u16*)in != 0x13) {
        goto message15;
    }
    snd           = 0x55120003;
    cmd           = 3;
    event.field_4 = snd;
    flag          = 0x15E;
start_event:
    event.field_0 = cmd;
    event.field_8 = flag;
    event.field_A = 0;
    return NeoArkSavannaZone_StartEvent(out, &event);
message15:
    if (*(u16*)in == 0x15) {
        snd           = 0x55120001;
        cmd           = 2;
        event.field_4 = snd;
        flag          = 0x15F;
        goto start_event;
    }
    return 1;
}

s32 func_neo_ark_savanna_zone_8017D8F0(void)
{
    return 0;
}

s32 func_neo_ark_savanna_zone_8017D8F8(void)
{
    return 0;
}

s32 func_neo_ark_savanna_zone_8017D900(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", RoomsShared8017d878Table);
