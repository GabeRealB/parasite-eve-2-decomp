#include "gpuext.h"

#include "common.h"

/// The GPU status register, as read from the GP1 port.
///
/// `word` is the register as read; `bits` names every field of it. The
/// drawing-mode fields mirror the last texture-page and mask commands sent to
/// GP0, and the display fields the last display-mode commands sent to GP1.
typedef union {
    u32 word;                      // The whole register, for tests that cannot be written through `bits`
    struct {
        u32 texPageXBase      : 4; // Texture page X base (N*64)
        u32 texPageYBase      : 1; // Texture page Y base (N*256)
        u32 semiTransparency  : 2; // Blend of back B and front F (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
        u32 texPageColors     : 2; // Texture page colour depth (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
        u32 dither            : 1; // Dither 24-bit colour to 15-bit (0=Off/strip LSBs, 1=Dither)
        u32 drawToDisplayArea : 1; // Drawing to the display area (0=Prohibited, 1=Allowed)
        u32 setMaskBit        : 1; // Set the mask bit of drawn pixels (0=No, 1=Yes)
        u32 checkMaskBit      : 1; // Leave masked pixels undrawn (0=Draw always, 1=Skip masked)
        u32 interlaceField    : 1; // Interlace field (always 1 when `verticalInterlace` is 0)
        u32 flipHorizontal    : 1; // Flip the screen horizontally (0=Off, 1=On; early GPU revision only)
        u32 texPageYBaseHigh  : 1; // Texture page Y base (N*512), only with 2 MB of VRAM
        u32 horizontalRes368  : 1; // 368-pixel width, overriding `horizontalRes` (0=Off, 1=On)
        u32 horizontalRes     : 2; // Display width in pixels (0=256, 1=320, 2=512, 3=640)
        u32 verticalRes       : 1; // Display height in lines (0=240, 1=480 when `verticalInterlace` is 1)
        u32 videoMode         : 1; // Video standard (0=NTSC/60Hz, 1=PAL/50Hz)
        u32 displayColorDepth : 1; // Display area colour depth (0=15bit, 1=24bit)
        u32 verticalInterlace : 1; // Vertical interlace (0=Off, 1=On)
        u32 displayDisabled   : 1; // Display blanking (0=Display enabled, 1=Display disabled)
        u32 interruptRequest  : 1; // GPU interrupt request, IRQ1 (0=Off, 1=Requested)
        u32 dataRequest       : 1; // By `dmaDirection`: 0=always 0, 1=FIFO not full, 2=`readyDmaBlock`, 3=`readyVramToCpu`
        u32 readyCommand      : 1; // Ready to receive a command word (0=No, 1=Ready)
        u32 readyVramToCpu    : 1; // Ready to send VRAM to the CPU (0=No, 1=Ready)
        u32 readyDmaBlock     : 1; // Ready to receive a DMA block (0=No, 1=Ready)
        u32 dmaDirection      : 2; // DMA direction (0=Off, 1=FIFO, 2=CPU to GP0, 3=GPUREAD to CPU)
        u32 interlaceOddLines : 1; // Lines drawn in interlace mode (0=Even, or in vertical blank; 1=Odd)
    } bits;
} _GpuStatusRegister;
STATIC_ASSERT_SIZEOF(_GpuStatusRegister, sizeof(u32));

/// Bit index of `bits.displayDisabled` within `_GpuStatusRegister.word`.
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
    // Reading `bits.displayDisabled` masks the bit before inverting it, and the
    // original inverts first, so the test is made on the whole register.
    return (GpuExt_GetGpuStatusReg().word >> GPU_STATUS_REGISTER_DISPLAY_DISABLED_BIT ^ 1) & 1;
}
