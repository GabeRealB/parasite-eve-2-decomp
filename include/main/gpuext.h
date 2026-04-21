#ifndef GPUEXT_H
#define GPUEXT_H

#include "common.h"

#define GPUEXT_GPU0 (void*)0x1f801810
#define GPUEXT_GPU1 (void*)0x1f801814

/// Reads the GPU Status Register.
///
/// @return GPU Status Register.
#define GpuExt_GetGpuStatusReg() (*(volatile GPUSTAT*)GPUEXT_GPU1)

/// GPU Status Register.
///
/// Contains various information about the GPU.
typedef struct {
    u32 texPageXBase     : 4; // Texture page X Base   (N*64)
    u32 texPageYBase1    : 1; // Texture page Y Base 1 (N*256) (ie. 0, 256, 512 or 768)
    u32 semiTransparency : 2; // Semi-transparency     (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
    u32 texPageColors    : 2; // Texture page colors   (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
    u32 dither24To15Bit  : 1; // Dither 24bit to 15bit (0=Off/strip LSBs, 1=Dither Enabled)
    u32 drawingToDisp    : 1; // Drawing to display area (0=Prohibited, 1=Allowed)
    u32 setMaskBit       : 1; // Set Mask-bit when drawing pixels (0=No, 1=Yes/Mask)
    u32 drawPixels       : 1; // Draw Pixels           (0=Always, 1=Not to Masked areas)
    u32 interlaceField   : 1; // Interlace Field       (or, always 1 when GP1(08h).5=0)
    u32 flipH            : 1; // Flip screen horizontally (0=Off, 1=On, v1 only)
    u32 texPageYBase2    : 1; // Texture page Y Base 2 (N*512) (only for 2 MB VRAM)
    u32 hRes2            : 1; // Horizontal Resolution 2     (0=256/320/512/640, 1=368)
    u32 hRes1            : 2; // Horizontal Resolution 1     (0=256, 1=320, 2=512, 3=640)
    u32 vRes             : 1; // Vertical Resolution         (0=240, 1=480, when Bit22=1)
    u32 videoMode        : 1; // Video Mode                  (0=NTSC/60Hz, 1=PAL/50Hz)
    u32 dacd             : 1; // Display Area Color Depth    (0=15bit, 1=24bit)
    u32 vInterlace       : 1; // Vertical Interlace          (0=Off, 1=On)
    u32 dispEnable       : 1; // Display Enable              (0=Enabled, 1=Disabled)
    u32 irq1             : 1; // Interrupt Request (IRQ1)    (0=Off, 1=IRQ)
    u32 dma              : 1; // DMA / Data Request, meaning depends on GP1(04h) DMA Direction:
                              // When GP1(04h)=0 ---> Always zero (0)
                              // When GP1(04h)=1 ---> FIFO State  (0=Full, 1=Not Full)
                              // When GP1(04h)=2 ---> Same as GPUSTAT.28
                              // When GP1(04h)=3 ---> Same as GPUSTAT.27
    u32 readyCmd         : 1; // Ready to receive Cmd Word   (0=No, 1=Ready)
    u32 readyVram        : 1; // Ready to send VRAM to CPU   (0=No, 1=Ready)
    u32 readyDma         : 1; // Ready to receive DMA Block  (0=No, 1=Ready)
    u32 dmaDir           : 2; // DMA Direction (0=Off, 1=?, 2=CPUtoGP0, 3=GPUREADtoCPU)
    u32 interlaceOddEven : 1; // Drawing even/odd lines in interlace mode (0=Even or Vblank, 1=Odd)
} GPUSTAT;
STATIC_ASSERT_SIZEOF(GPUSTAT, sizeof(u32));

/// Check if the display flag is set in the GPU.
///
/// @return Whether display is enabled.
i32 GpuExt_IsDisplayEnabled();

#endif // GPUEXT_H
