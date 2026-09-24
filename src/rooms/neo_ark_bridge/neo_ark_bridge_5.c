#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Room message handler for the bridge's save location: copies the incoming
/// record onto the outgoing one and forwards both to `func_80179B14`. Always
/// answers 1.
s32 func_neo_ark_bridge_8017E834(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
