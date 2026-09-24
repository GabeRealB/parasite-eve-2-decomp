#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Message handler for the save location: copies the incoming `GpSaveLoc`
/// onto the outgoing one and passes both to `func_80179B14`. Returns 1.
s32 func_neo_ark_r31_8017D8B8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
