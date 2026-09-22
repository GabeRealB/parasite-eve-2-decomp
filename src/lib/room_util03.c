#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Default save-location message handler: copies the incoming `GpSaveLoc`
/// onto the outgoing one and forwards both to `func_80179B14`. Always returns
/// 1 (not consumed).
s32 Room_Util03(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
