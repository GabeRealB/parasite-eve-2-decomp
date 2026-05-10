#include "common.h"

#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004DC8C);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004DDF0);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004DE18);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004DEBC);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004DF10);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E060);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E200);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E44C);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E560);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E580);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E5A0);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E5C4);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E660);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E6A4);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E6C4);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E71C);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E764);

void F3E48C_ConfigSpuReverb(s32 mode)
{
    SpuReserveReverbWorkArea(SPU_ON);
    SpuSetReverbVoice(SPU_OFF, SPU_ALLCH);
    SpuSetReverb(SPU_ON);

    D648E0_SpuReverbCfg.attr.mask = SPU_REV_MODE;
    D648E0_SpuReverbCfg.attr.mode = mode;
    SpuSetReverbModeParam(&D648E0_SpuReverbCfg.attr);

    D648E0_SpuReverbCfg.attr.mask        = SPU_REV_DEPTHR | SPU_REV_DEPTHL;
    D648E0_SpuReverbCfg.attr.depth.right = 0;
    D648E0_SpuReverbCfg.attr.depth.left  = 0;
    SpuSetReverbDepth(&D648E0_SpuReverbCfg.attr);

    D648E0_SpuReverbCfg.attr.mask = 0;
}

void F3E48C_SetReverbDepth(s16 depth)
{
    D648E0_SpuReverbCfg.isDirty          = true;
    D648E0_SpuReverbCfg.attr.depth.right = depth;
    D648E0_SpuReverbCfg.attr.depth.left  = depth;
    D648E0_SpuReverbCfg.attr.mask       |= SPU_REV_DEPTHR | SPU_REV_DEPTHL;
}

void F3E48C_SetReverbMode(u32 mode)
{
    if (D648E0_SpuReverbCfg.reverbMode != mode && D648E0_SpuReverbCfg.reverbMode != SPU_REV_MODE_OFF) {
        SpuClearReverbWorkArea(D648E0_SpuReverbCfg.reverbMode);
        D648E0_SpuReverbCfg.isDirty    = true;
        D648E0_SpuReverbCfg.attr.mask |= SPU_REV_MODE;
        D648E0_SpuReverbCfg.attr.mode  = mode;
        D648E0_SpuReverbCfg.reverbMode = mode;
    }
}

void F3E48C_EnableVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    D648E0_SpuReverbCfg.isDirty        = true;
    channel                            = SPU_VOICECH(voiceIdx);
    D648E0_SpuReverbCfg.enableVoices  |= channel;
    D648E0_SpuReverbCfg.disableVoices &= ~channel;
}

void F3E48C_DisableVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    D648E0_SpuReverbCfg.isDirty        = true;
    channel                            = SPU_VOICECH(voiceIdx);
    D648E0_SpuReverbCfg.disableVoices |= channel;
    D648E0_SpuReverbCfg.enableVoices  &= ~channel;
}

bool F3E48C_ReverbVoiceIsEnabled(u32 voiceIdx)
{
    return ((s32)D648E0_8007E338.reverbVoiceStatus >> voiceIdx) & 1;
}

void F3E48C_ApplyReverbConfig(void)
{
    if (D648E0_SpuReverbCfg.disableVoices != 0) {
        SpuSetReverbVoice(SPU_OFF, D648E0_SpuReverbCfg.disableVoices);
        D648E0_SpuReverbCfg.disableVoices = 0;
    }

    if (D648E0_SpuReverbCfg.enableVoices != 0) {
        SpuSetReverbVoice(SPU_ON, D648E0_SpuReverbCfg.enableVoices);
        D648E0_SpuReverbCfg.enableVoices = 0;
    }

    if ((D648E0_SpuReverbCfg.attr.mask & SPU_REV_MODE) != 0) {
        SpuSetReverbModeParam(&D648E0_SpuReverbCfg.attr);
    }
    if ((D648E0_SpuReverbCfg.attr.mask & SPU_REV_DEPTHL) != 0) {
        SpuSetReverbDepth(&D648E0_SpuReverbCfg.attr);
    }

    D648E0_SpuReverbCfg.attr.mask = 0;
}

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E9D8);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004EA60);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004EAA0);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004EAF8);
