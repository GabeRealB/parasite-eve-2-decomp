#include "gpuext.h"

#include "common.h"

/// The GPU status register, as read from the GP1 port.
///
/// The drawing-mode fields mirror the last texture-page and mask commands sent
/// to GP0, and the display fields the last display-mode commands sent to GP1.
///
/// | Bits  | Field                                                                                      |
/// |-------|--------------------------------------------------------------------------------------------|
/// | 0-3   | Texture page X base (N*64)                                                                 |
/// | 4     | Texture page Y base (N*256)                                                                |
/// | 5-6   | Blend of back B and front F (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)                             |
/// | 7-8   | Texture page colour depth (0=4bit, 1=8bit, 2=15bit, 3=Reserved)                            |
/// | 9     | Dither 24-bit colour to 15-bit (0=Off/strip LSBs, 1=Dither)                                |
/// | 10    | Drawing to the display area (0=Prohibited, 1=Allowed)                                      |
/// | 11    | Set the mask bit of drawn pixels (0=No, 1=Yes)                                             |
/// | 12    | Leave masked pixels undrawn (0=Draw always, 1=Skip masked)                                 |
/// | 13    | Interlace field (always 1 when vertical interlace is off)                                  |
/// | 14    | Flip the screen horizontally (0=Off, 1=On; early GPU revision only)                        |
/// | 15    | Texture page Y base (N*512), only with 2 MB of VRAM                                        |
/// | 16    | 368-pixel width, overriding bits 17-18 (0=Off, 1=On)                                       |
/// | 17-18 | Display width in pixels (0=256, 1=320, 2=512, 3=640)                                       |
/// | 19    | Display height in lines (0=240, 1=480 when vertical interlace is on)                       |
/// | 20    | Video standard (0=NTSC/60Hz, 1=PAL/50Hz)                                                   |
/// | 21    | Display area colour depth (0=15bit, 1=24bit)                                               |
/// | 22    | Vertical interlace (0=Off, 1=On)                                                           |
/// | 23    | Display blanking (0=Display enabled, 1=Display disabled)                                   |
/// | 24    | GPU interrupt request, IRQ1 (0=Off, 1=Requested)                                           |
/// | 25    | Data request, by DMA direction: 0=always 0, 1=FIFO not full, 2=bit 28, 3=bit 27            |
/// | 26    | Ready to receive a command word (0=No, 1=Ready)                                            |
/// | 27    | Ready to send VRAM to the CPU (0=No, 1=Ready)                                              |
/// | 28    | Ready to receive a DMA block (0=No, 1=Ready)                                               |
/// | 29-30 | DMA direction (0=Off, 1=FIFO, 2=CPU to GP0, 3=GPUREAD to CPU)                              |
/// | 31    | Lines drawn in interlace mode (0=Even, or in vertical blank; 1=Odd)                        |
typedef u32 _GpuStatusRegister;

/// Bit index of the display-blanking flag within `_GpuStatusRegister`.
#define GPU_STATUS_REGISTER_DISPLAY_DISABLED_BIT 23

#define GPUEXT_GPU1 (void*)0x1f801814

/// Reads the GPU Status Register.
///
/// @return GPU Status Register.
static inline _GpuStatusRegister GpuExt_GetGpuStatusReg()
{
    return *(_GpuStatusRegister*)GPUEXT_GPU1;
}

i32 GpuExt_IsDisplayEnabled()
{
    // The original inverts the bit before masking it, which a one-bit bitfield
    // read does not reproduce (it masks first), so the register is a plain word.
    return (GpuExt_GetGpuStatusReg() >> GPU_STATUS_REGISTER_DISPLAY_DISABLED_BIT ^ 1) & 1;
}
