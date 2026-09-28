#ifndef MAIN_PRIVATE_CDSTREAM_H
#define MAIN_PRIVATE_CDSTREAM_H

#include "common.h"

/// CD/SPU stream setup block for CdAudio_DriveStream / CdStream_Start: sector
/// position, buffer, callbacks, and voice indices.
typedef struct _CdStreamParams {
    /* 0x00 */ s32   startSector;
    /* 0x04 */ s32   spuBase;
    /* 0x08 */ void* sectorBuf;
    /* 0x0C */ void  (*doneCb)(s32);
    /* 0x10 */ void  (*startCb)(s32);
    /* 0x14 */ void  (*voiceFreeCb)(s32);
    /* 0x18 */ s16   volume;
    /* 0x1A */ s8    voiceL;
    /* 0x1B */ s8    voiceR;
    /* 0x1C */ u8    mode;
    /* 0x1D */ u8    pad_1D[3];
} CdStreamParams;
STATIC_ASSERT_SIZEOF(CdStreamParams, 0x20);

// CD → SPU MTS stream
void CdStream_Reset(void);

void CdStream_ArmSpuIrq(void);

/// Sets both streaming voices' gains, respecting mono/stereo output.
void CdStream_SetVolume(s16 volume);

s32 CdStream_IsBusy(void);

/// When enabled, mix both input channels equally into both outputs.
void CdStream_SetMono(s32 enabled);

void CdStream_Start(CdStreamParams* arg0);

void CdStream_Stop(void);

void CdStream_Drive(void);

#endif // MAIN_PRIVATE_CDSTREAM_H
