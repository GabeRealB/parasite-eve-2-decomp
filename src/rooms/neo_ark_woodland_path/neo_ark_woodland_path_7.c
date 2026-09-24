#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Room message handler for the path's save location: copies the incoming
/// record onto the outgoing one and forwards both to `func_80179B14`. Always
/// answers 1.
s32 func_neo_ark_woodland_path_8017E890(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
