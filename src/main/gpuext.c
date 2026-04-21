#include "main/gpuext.h"

i32 GpuExt_IsDisplayEnabled()
{
    // The logic is equivalent to the following:
    //     return GpuExt_GetGpuStatusReg().dispEnable ^ 1;
    //
    // This produces non-matching assembly, as GCC reorders
    // some instructions and utilizes a or for the offset
    // instead of utilizing the offset in `lw`.
    u32 gpustat;
    __asm__ volatile(
        "lui $2,0x1f80;"
        "lw $2,0x1814($2);"
        "nop;"
        : "=r"(gpustat)
        :
        : "memory");
    return (gpustat >> 0x17 ^ 1) & 1;
}
