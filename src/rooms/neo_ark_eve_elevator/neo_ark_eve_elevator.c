#include "common.h"

#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "rooms/room_common.h"

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Handler for the elevator's arrival message `0x18`: mirrors the incoming
/// record onto the outgoing one and lets `func_80179B14` act on both. Answers 1
/// while the script raises a different id or a CD command is still running, and
/// otherwise, with no sub-state pending, wakes cap event 1 and answers 0 either
/// way. Same gate as `func_neo_ark_garden_8017E848` and
/// `func_neo_ark_shrine_8017D6AC`, which latch a nibble and run a cap command
/// where this one starts a cap event.
s32 func_neo_ark_eve_elevator_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0x18) {
        return 1;
    }
    if (CdCmd_IsIdle() != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SpawnIfCapIdle(1, 1);
    return 0;
}

s32 func_neo_ark_eve_elevator_8017D668(void)
{
    return 0;
}

s32 func_neo_ark_eve_elevator_8017D670(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_elevator/neo_ark_eve_elevator", D_neo_ark_eve_elevator_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_elevator/neo_ark_eve_elevator", RoomsShared8017d878Table);
