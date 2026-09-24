#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both on to `func_80179B14`. Always returns 1.
s32 func_neo_ark_power_plant_2_8017D5D8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
