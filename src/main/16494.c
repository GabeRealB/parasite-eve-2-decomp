#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libspu.h>

#include "main/unknown_syms.h"

void func_80025C94(u8 status, u8* result)
{
    GStruct34* state;
    CdlLOC     loc[3];
    void*      buf;
    s32        ret;
    u8         errCount;

    state          = &D_800820F0;
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
        ret = func_80053414(buf);
        if (ret != -1) {
            if (ret != 5) {
                return;
            }
            CdControlF(CdlPause, NULL);
            func_80053448(state);
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

INCLUDE_ASM("main/nonmatchings/16494", func_80025DD8);

void F16494_ResetSpuAttr(void)
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

void func_800260B0(s32 arg0)
{
    CdlATV atv;
    s32    flag;

    D_8006EBBA = arg0 & 1;
    flag       = D_8006EBBA;
    func_800517B4(func_800517F8() & 0xFF);
    flag = (u8)flag;
    func_80055DFC(func_80055EE8());
    func_8005BBB0(flag ^ 1);
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

u8 func_80026138(void)
{
    return D_8006EBBA;
}

void func_80026148(void)
{
    D_8006EBF4 = (Fs_SpuAttr.cd.volume.left / 256) & 0x7F;
}

void func_80026178(void)
{
    struct {
        s32  pad[2];
        void (*unk8)(void);
        void (*unkC)(void);
        s32  (*unk10)(void);
    } sp;
    s16* ptr;

    ptr      = &D_8006EBF2;
    sp.unk8  = func_80025DD8;
    sp.unkC  = func_800261C8;
    sp.unk10 = func_800261D4;
    *ptr     = func_8004DE18(&sp);
}

void func_800261C8(void)
{
    D_8006EBF2 = 0;
}

s32 func_800261D4(void)
{
    CdFlush();
    return 0;
}

s32 func_800261F4(void)
{
    return (Fs_SpuAttr.cd.volume.left / 256) & 0x7F;
}

void func_80026218(u16 arg0)
{
    if (arg0 >= 0x28) {
        arg0 = 0;
    }
    D_8006EBF4 = D_80068AF0[arg0];
    func_80026268(D_80068AF0[arg0]);
}

void func_80026268(s32 arg0)
{
    s16 vol;

    Fs_SpuAttr.mask            = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    vol                        = (arg0 & 0x7F) << 8;
    Fs_SpuAttr.cd.volume.right = vol;
    Fs_SpuAttr.cd.volume.left  = vol;
    SpuSetCommonAttr(&Fs_SpuAttr);
}

INCLUDE_ASM("main/nonmatchings/16494", func_800262A8);
