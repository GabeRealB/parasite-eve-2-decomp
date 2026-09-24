#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Message handler the room's message table names for one of its entries:
/// copies the incoming message onto the outgoing one, passes both to
/// `func_80179B14` and returns 1.
s32 func_neo_ark_south_promenade_8017D5D8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
