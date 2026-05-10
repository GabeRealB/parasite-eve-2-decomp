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

void F3E48C_8004E44C(void)
{
    i32        remaining;
    u8*        current;
    GStruct10* dataPtr;

    if (D648E0_SpuReverbCfg.isDirty) {
        F3E48C_ApplyReverbConfig();
        D648E0_SpuReverbCfg.isDirty = false;
    }

    D648E0_8007EBB0 |= D648E0_8007EBA8;
    if (D648E0_8007EBB0 != 0) {
        SpuSetKey(SPU_OFF, D648E0_8007EBB0);
        D648E0_8007EBB0 = 0;
    }

    // We take a pointer, as otherwise GCC will reload the address
    // when we reset the count to zero below.
    dataPtr = &D648E0_8007E518;
    if (dataPtr->count != 0) {
        SpuLSetVoiceAttr(dataPtr->count, dataPtr->attrs);

        // Clear the list. For some reason GCC does not like to cooperate with
        // the array indexing. Ideally we'd have the following:
        //
        // current = &dataPtr->field_664[remaining];
        // ...
        // *current-- = 0;
        //
        // This produces the following assembly:
        //
        // addu     v0, s0, 0x67B
        // sb       zero, 0(v0)
        //
        // Instead of what we actually want:
        //
        // addu     v0, s0, v1
        // sb       zero, 0x664(v0)
        //
        // Writing it this way forces GCC to perform the offsets in the correct
        // order.
        remaining = ARRAY_SIZE(dataPtr->field_664) - 1;
        current   = (u8*)dataPtr + remaining;
        do {
            current[OFFSET_OF(GStruct10, field_664)] = 0;
            current                                 -= 1;
            remaining                               -= 1;
        } while (remaining >= 0);
        dataPtr->count = 0;
    }

    if ((D648E0_8007EBA8 | D648E0_8007EBAC) != 0) {
        SpuSetKey(SPU_ON, D648E0_8007EBA8 | D648E0_8007EBAC);
        if (D648E0_8007EBA8 != 0) {
            D648E0_8007E338.field_1cc |= D648E0_8007EBA8;
            D648E0_8007E338.field_1cc &= ~D648E0_8007E338.field_1d0;
        }

        D648E0_8007E338.field_1d0 = 0;
        D648E0_8007EBA8           = 0;
        D648E0_8007EBAC           = 0;
    }
}

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E560);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E580);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E5A0);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E5C4);

s32 F3E48C_8004E660(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;
    if (sVoiceIdx > (u32)ARRAY_SIZE(D648E0_8007E338.field_94)) {
        return -1;
    }

    D648E0_8007E338.field_94[sVoiceIdx] = 0;
    D648E0_8007E338.field_ac[sVoiceIdx] = 0;
    D648E0_8007E338.field_4[sVoiceIdx]  = 0;
    return 0;
}

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E6A4);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E6C4);

INCLUDE_ASM("main/nonmatchings/3E48C", func_8004E71C);

void F3E48C_QueryReverbVoices(void)
{
    D648E0_8007E338.reverbVoiceStatus = SpuGetReverbVoice();
}

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
