#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// The room's handler for message 0x13EE: copies the incoming save location
/// onto the outgoing one, passes both to `func_80179B14` and returns 1.
s32 func_shelter_1f_airlock_8017D5D8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
