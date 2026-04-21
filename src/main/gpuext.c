#include "main/gpuext.h"

i32 GpuExt_IsDisplayEnabled()
{
    // Equivalent to:
    //  return GpuExt_GetGpuStatusReg().dispEnable ^ 1;
    GPUSTAT stat = GpuExt_GetGpuStatusReg();
    return (stat.bytes >> 0x17 ^ 1) & 1;
}
