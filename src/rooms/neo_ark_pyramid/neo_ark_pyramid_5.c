#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Handler for message 0x13EE in the room's message table: copies the
/// incoming save-location record onto the outgoing one and forwards both to
/// `func_80179B14`. Always answers 1.
s32 func_neo_ark_pyramid_8017D9F8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
