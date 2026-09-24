#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `GpSaveLoc` onto the outgoing one and passes both on to `func_80179B14`.
/// Always returns 1.
s32 func_neo_ark_power_plant_1_8017D7B4(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
