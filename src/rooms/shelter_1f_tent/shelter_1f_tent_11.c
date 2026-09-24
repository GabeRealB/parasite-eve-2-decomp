#include "common.h"

#include "gameplay/1A8.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Copies the incoming `GpSaveLoc` onto the outgoing one, hands both to
/// `func_80179B14` and returns 1.
s32 func_shelter_1f_tent_8017FC5C(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}
