#include "common.h"

#include <psyq/libetc.h>

#include "main/unknown_syms.h"
#include "main/cdaudio.h"
#include "main/cdstream.h"
#include "main/fs.h"

void Fs_StreamReadyCb(u8 status, u8* result)
{
    SndLoadState* state;
    CdlLOC        loc[3];
    void*         buf;
    s32           ret;
    u8            errCount;

    state          = &SndLoad_State;
    state->field_3 = 0;
    if (status != CdlDiskError) {
        CdGetSector(loc, 3);
        if (CdPosToInt(loc) != Fs_ReqSector) {
            errCount        = Fs_CdErrorCount;
            Fs_CdOpStatus   = 0x80;
            Fs_CdErrorCount = errCount + 1;
            CdControlF(CdlPause, NULL);
            CdReadyCallback(NULL);
            return;
        }
        Fs_VBlank     = VSync(-1);
        buf           = state->field_4;
        Fs_ReqSector += 1;
        CdGetSector(buf, 0x200);
        ret = SndLoad_FeedSectorOrError(buf);
        if (ret != -1) {
            if (ret != 5) {
                return;
            }
            CdControlF(CdlPause, NULL);
            SndBank_FinalizeLoad(state);
            Fs_CdOpStatus = 0xFF;
            CdReadyCallback(NULL);
            return;
        }
    }
    Fs_CdErrorCount += 1;
    CdControlF(CdlPause, NULL);
    Fs_CdOpStatus = 0x80;
    CdReadyCallback(NULL);
}

s32 Cd_InitStateMachine(u32* arg0)
{
    struct {
        u8     result[8];
        s8     mode;
        u8     pad[7];
        CdlLOC loc;
    } sp;
    s32 sync;
    u32 flags;
    u32 temp;
    s16 counter;

    flags = *arg0;
    if ((flags >> 1) & 1) {
        temp  = flags & ~2;
        temp  = temp & ~0xFF0;
        *arg0 = temp | 0x10;
    }

    switch ((*arg0 >> 4) & 0xFF) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                case 2:
                    if (CdControlB(CdlGetTN, NULL, sp.result) != 0) {
                        *arg0 = (*arg0 & ~0xFF0) | 0x40;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                            } else if (sync == CdlComplete) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x40;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        *arg0 = (*arg0 & ~0xFF0) | 0x50;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x10;
                                            } else if (sync != CdlComplete) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x60;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        D_8006EBB8 = 0;
                                                        *arg0      = (*arg0 & ~0xFF0) | 0x70;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case 7:
            counter    = D_8006EBB8 + 1;
            D_8006EBB8 = counter;
            if (counter >= 4) {
                return 1;
            }
            break;
    }
    return 0;
}

void Spu_ResetCommonAttr(void)
{
    // Set all attributes.
    Fs_SpuAttr.mask = 0;

    // Set the master volume to maximum.
    Fs_SpuAttr.mvol.left      = 0x3FFF;
    Fs_SpuAttr.mvol.right     = 0x3FFF;
    Fs_SpuAttr.mvolmode.left  = SPU_VOICE_DIRECT;
    Fs_SpuAttr.mvolmode.right = SPU_VOICE_DIRECT;

    // Set the CD Input volume to the minimum.
    Fs_SpuAttr.cd.volume.left  = 0;
    Fs_SpuAttr.cd.volume.right = 0;
    Fs_SpuAttr.cd.reverb       = SPU_OFF;
    Fs_SpuAttr.cd.mix          = SPU_ON;

    // Disable External Digital Input.
    Fs_SpuAttr.ext.volume.left  = 0;
    Fs_SpuAttr.ext.volume.right = 0;
    Fs_SpuAttr.ext.reverb       = SPU_OFF;
    Fs_SpuAttr.ext.mix          = SPU_OFF;

    // Apply the settings.
    SpuSetCommonAttr(&Fs_SpuAttr);
    D5B498_8006EBF0 = 0;
}

void CdVol_SetMixMode(s32 arg0)
{
    CdlATV atv;
    s32    flag;

    D_8006EBBA = arg0 & 1;
    flag       = D_8006EBBA;
    Midi_SetMasterVolume(Midi_GetMasterVolume() & 0xFF);
    flag = (u8)flag;
    SndVoice_ApplyMasterVolume(SndVoice_GetMasterVolume());
    CdStream_SetLinkedPitch(flag ^ 1);
    if (flag == 0) {
        atv.val0 = 0x5A;
        atv.val1 = 0x5A;
        atv.val2 = 0x5A;
        atv.val3 = 0x5A;
    } else {
        atv.val0 = 0x78;
        atv.val1 = 0;
        atv.val2 = 0x78;
        atv.val3 = 0;
    }
    CdMix(&atv);
}

u8 CdVol_GetMixMode(void)
{
    return D_8006EBBA;
}

void CdVol_CacheFromSpu(void)
{
    D_8006EBF4 = (Fs_SpuAttr.cd.volume.left / 256) & 0x7F;
}

void CdVol_RegisterCallbacks(void)
{
    struct {
        s32  pad[2];
        s32  (*unk8)(u32*);
        void (*unkC)(void);
        s32  (*unk10)(void);
    } sp;
    s16* ptr;

    ptr      = &D_8006EBF2;
    sp.unk8  = Cd_InitStateMachine;
    sp.unkC  = CdVol_ClearCallbackSlot;
    sp.unk10 = Cd_Flush;
    *ptr     = func_8004DE18(&sp);
}

void CdVol_ClearCallbackSlot(void)
{
    D_8006EBF2 = 0;
}

s32 Cd_Flush(void)
{
    CdFlush();
    return 0;
}

s32 CdVol_Get(void)
{
    return (Fs_SpuAttr.cd.volume.left / 256) & 0x7F;
}

void CdVol_ApplyFromTable(u16 arg0)
{
    if (arg0 >= 0x28) {
        arg0 = 0;
    }
    D_8006EBF4 = D_80068AF0[arg0];
    CdVol_Set(D_80068AF0[arg0]);
}

void CdVol_Set(s32 arg0)
{
    s16 vol;

    Fs_SpuAttr.mask            = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    vol                        = (arg0 & 0x7F) << 8;
    Fs_SpuAttr.cd.volume.right = vol;
    Fs_SpuAttr.cd.volume.left  = vol;
    SpuSetCommonAttr(&Fs_SpuAttr);
}

s32 CdVol_StepDown(void)
{
    s16 vol;

    D_8006EBF4 -= 8;
    if (D_8006EBF4 < 0) {
        D_8006EBF4 = 0;
    }
    Fs_SpuAttr.mask            = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    vol                        = (D_8006EBF4 & 0x7F) << 8;
    Fs_SpuAttr.cd.volume.right = vol;
    Fs_SpuAttr.cd.volume.left  = vol;
    SpuSetCommonAttr(&Fs_SpuAttr);
    return D_8006EBF4;
}
