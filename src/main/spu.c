#include "common.h"

#include <psyq/libspu.h>

#include "main/unknown_syms.h"

void AsyncCb_Poll(void)
{
    AsyncCbEntry* entry;
    u32           flags;
    s32           ret;
    s32           mask;
    s8            idx;
    s8            current;

    current = AsyncCb_Queue.field_0;
    if (AsyncCb_Queue.field_1 != current) {
        entry = &AsyncCb_Queue.entries[current];
        flags = entry->field_0;
        if (flags & 1) {
            if (entry->field_8(entry) != 0) {
                if (entry->field_C != NULL) {
                    entry->field_C(entry);
                }
                entry->field_0       &= ~1;
                entry->field_0       &= ~4;
                idx                   = (u8)AsyncCb_Queue.field_0 + 1;
                AsyncCb_Queue.field_0 = idx;
                if (idx >= 4) {
                    AsyncCb_Queue.field_0 = 0;
                }
            }
        } else if (!((flags >> 2) & 1) || ((flags >> 1) & 1) || (entry->field_10 == NULL) ||
                   (ret = entry->field_10(entry), mask = ~8,
                    entry->field_0 = (entry->field_0 & mask) | ((ret & 1) * 8), ((ret & 1) == 0))) {
            entry->field_0       &= ~4;
            idx                   = (u8)AsyncCb_Queue.field_0 + 1;
            AsyncCb_Queue.field_0 = idx;
            if (idx >= 4) {
                AsyncCb_Queue.field_0 = 0;
            }
        }
    }
}

void AsyncCb_Reset(void)
{
    u32  i;
    s32* ptr;

    ptr = (s32*)&AsyncCb_Queue;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x15U);
}

INCLUDE_ASM("main/nonmatchings/spu", func_8004DE18);

void AsyncCb_Cancel(s32 arg0)
{
    AsyncCbEntry* entry;
    s32           flags;

    if ((arg0 << 0x10) != 0) {
        entry = &AsyncCb_Entries[(s16)(arg0 - 1)];
        flags = entry->field_0;
        if (flags & 1) {
            entry->field_0 = (flags & ~1) | 4;
        }
    }
}

void Spu_InitVoices(void)
{
    SpuVoiceRef sp10;
    s32*        ptr;
    s32         i;
    s8          sVoiceIdx;
    u32         spuAddr;

    spuAddr = 0x7B440;
    SpuSetTransferStartAddr(spuAddr);
    SpuWrite(&D_80068184, 0x30U);
    SpuIsTransferCompleted(1);

    ptr             = (s32*)&Spu_LVoiceTable;
    i               = 0;
    D648E0_8007EBA8 = 0;
    D648E0_8007EBAC = 0;
    D648E0_8007EBB0 = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while ((u32)i < 0x19FU);

    ptr = (s32*)&Spu_VoiceState;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while ((u32)i < 0x75U);

    i = 0;
    do {
        sVoiceIdx = i;
        Spu_GetVoiceRef(sVoiceIdx, &sp10);

        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->loop_addr    = spuAddr;
            attr->addr         = spuAddr;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->volume.right = 0;
            attr->volume.left  = 0;
        }
        {
            SpuVoiceAttr* attr  = sp10.field_4;
            attr->volmode.right = 0;
            attr->volmode.left  = 0;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->adsr1        = 0x80FF;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->adsr2        = 0xFFE0;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->mask         = 0x7008FU;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->voice        = 1 << i;
        }

        Spu_KeyOnClearOff(sVoiceIdx);
        i++;
    } while (i < 0x18);

    Spu_SetVoiceRange(2, 0x10, 2);
}

s32 Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2)
{
    SpuVoiceRange* entry;
    s32            maxField4;
    s32            bestPriority;
    s32            i;
    s32            j;
    s8             bestVoice;
    u8             voice;
    u8             field64;
    u32            fieldAc;
    s32            field4;
    s32            (*callback)(s32);
    s32            cbArg;
    SpuVoiceState* base;

    maxField4    = 0;
    bestPriority = arg2;
    bestVoice    = -1;
    i            = 0;
    base         = &Spu_VoiceState;

    if (arg1 > 0) {
        do {
            entry = &Spu_VoiceRanges[*arg0];
            voice = *(u8*)entry;
            j     = 0;
            if (entry->field_2 > 0) {
                do {
                    if (base->field_94[(s8)voice] == 0) {
                        field64 = base->field_64[(s8)voice];
                        if ((field64 == 0) || (field64 == 3)) {
                            base->field_ac[(s8)voice] = arg2;
                            base->field_94[(s8)voice] = 1;
                            base->field_4[(s8)voice]  = 0;
                            return (s8)voice;
                        }
                    } else {
                        fieldAc = base->field_ac[(s8)voice];
                        if (fieldAc < (u32)bestPriority) {
                            bestPriority = fieldAc;
                            bestVoice    = voice;
                        } else if (bestPriority == (s32)fieldAc) {
                            field4 = base->field_4[(s8)voice];
                            if (maxField4 < field4) {
                                maxField4 = field4;
                                bestVoice = voice;
                            }
                        }
                    }
                    j++;
                    voice++;
                } while (j < entry->field_2);
            }
            i++;
            arg0++;
        } while (i < arg1);
    }

    if (bestVoice >= 0) {
        callback = (s32 (*)(s32))base->field_10c[bestVoice];
        if (callback != NULL) {
            cbArg = base->field_16c[bestVoice];
            if (cbArg != 0) {
                callback(cbArg);
            }
        }
        base->field_ac[bestVoice] = arg2;
        base->field_4[bestVoice]  = 0;
        base->field_64[bestVoice] = 1;
    }
    return bestVoice;
}

INCLUDE_ASM("main/nonmatchings/spu", func_8004E200);

void F3E48C_8004E44C(void)
{
    i32             remaining;
    u8*             current;
    SpuLVoiceTable* dataPtr;

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
    dataPtr = &Spu_LVoiceTable;
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
            current[OFFSET_OF(SpuLVoiceTable, field_664)] = 0;
            current                                      -= 1;
            remaining                                    -= 1;
        } while (remaining >= 0);
        dataPtr->count = 0;
    }

    if ((D648E0_8007EBA8 | D648E0_8007EBAC) != 0) {
        SpuSetKey(SPU_ON, D648E0_8007EBA8 | D648E0_8007EBAC);
        if (D648E0_8007EBA8 != 0) {
            Spu_VoiceState.field_1cc |= D648E0_8007EBA8;
            Spu_VoiceState.field_1cc &= ~Spu_VoiceState.field_1d0;
        }

        Spu_VoiceState.field_1d0 = 0;
        D648E0_8007EBA8          = 0;
        D648E0_8007EBAC          = 0;
    }
}

void Spu_SetVoiceCallbacks(u32 voiceIdx, s32 arg1, s32 arg2)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    Spu_VoiceState.field_10c[sVoiceIdx] = arg1;
    Spu_VoiceState.field_16c[sVoiceIdx] = arg2;
}

void Spu_ClearVoiceCallbacks(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    Spu_VoiceState.field_10c[sVoiceIdx] = 0;
    Spu_VoiceState.field_16c[sVoiceIdx] = 0;
}

s32 Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2)
{
    SpuVoiceRange* p;
    s16            sIdx;

    sIdx       = idx;
    p          = &Spu_VoiceRanges[sIdx];
    p->field_0 = arg1;
    p->field_2 = arg2;
    return 0;
}

s32 Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1)
{
    register s32 ret asm("t2");

    /*
     * Leaf lookup/alloc into Spu_LVoiceTable. Pure C fails to keep $a0 as the
     * voice id across the prologue (needed for sb into arg1) while also doing
     * the dual lhu/lh count load and the slot*0x44-0x3C found-path address.
     * Hand-scheduled body matches target instruction-for-instruction.
     */
    __asm__ volatile(
        ".set\tnoreorder\n\t"
        "lui $3, %%hi(Spu_LVoiceTable)\n\t"
        "addiu $9, $3, %%lo(Spu_LVoiceTable)\n\t"
        "sll $2, %1, 24\n\t"
        "sra $7, $2, 24\n\t"
        "addu $8, $7, $9\n\t"
        "lb $6, 1636($8)\n\t"
        "nop\n\t"
        "beqz $6, 1f\n\t"
        "li %0, 1\n\t"
        "sll $2, $6, 4\n\t"
        "addu $2, $2, $6\n\t"
        "sll $2, $2, 2\n\t"
        "addu $2, $2, $9\n\t"
        "addiu $2, $2, -60\n\t"
        "sb %1, 0(%2)\n\t"
        "j 2f\n\t"
        "sw $2, 4(%2)\n\t"
        "1:\n\t"
        "move %0, $0\n\t"
        "lhu $2, %%lo(Spu_LVoiceTable)($3)\n\t"
        "lh $6, %%lo(Spu_LVoiceTable)($3)\n\t"
        "addiu $2, $2, 1\n\t"
        "sh $2, %%lo(Spu_LVoiceTable)($3)\n\t"
        "sll $2, $6, 4\n\t"
        "addu $2, $2, $6\n\t"
        "sll $2, $2, 2\n\t"
        "addu $3, $2, $9\n\t"
        "sh $7, 4($3)\n\t"
        "addiu $3, $6, 1\n\t"
        "addu $2, $2, $9\n\t"
        "addiu $2, $2, 8\n\t"
        "sb $3, 1636($8)\n\t"
        "sb %1, 0(%2)\n\t"
        "sw $2, 4(%2)\n\t"
        "sw $0, 4($2)\n\t"
        "sb $0, 1(%2)\n\t"
        "sb $0, 3(%2)\n\t"
        "sb $0, 2(%2)\n\t"
        "2:\n\t"
        ".set\treorder"
        : "=&r"(ret)
        : "r"(arg0), "r"(arg1)
        : "$2", "$3", "$6", "$7", "$8", "$9", "memory");
    return ret;
}

s32 F3E48C_8004E660(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;
    if (sVoiceIdx > (u32)ARRAY_SIZE(Spu_VoiceState.field_94)) {
        return -1;
    }

    Spu_VoiceState.field_94[sVoiceIdx] = 0;
    Spu_VoiceState.field_ac[sVoiceIdx] = 0;
    Spu_VoiceState.field_4[sVoiceIdx]  = 0;
    return 0;
}

u8 Spu_GetVoiceStatus(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    return Spu_VoiceState.field_64[sVoiceIdx];
}

void Spu_KeyOn(u32 voiceIdx)
{
    SpuVoiceState* p;
    u32*           pKeyOn;
    u32            channel;

    p                     = &Spu_VoiceState;
    pKeyOn                = &D648E0_8007EBA8;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    channel               = ~channel;
    p->field_1d0         &= channel;
    D648E0_8007EBB0      &= channel;
}

void Spu_KeyOff(u32 voiceIdx)
{
    u32* pKeyOff;
    u32  channel;

    pKeyOff  = &D648E0_8007EBB0;
    voiceIdx = (s8)voiceIdx;

    channel          = SPU_VOICECH(voiceIdx);
    *pKeyOff        |= channel;
    D648E0_8007EBA8 &= ~channel;
    D648E0_8007EBAC &= ~channel;
}

void F3E48C_QueryReverbVoices(void)
{
    Spu_VoiceState.reverbVoiceStatus = SpuGetReverbVoice();
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
    return ((s32)Spu_VoiceState.reverbVoiceStatus >> voiceIdx) & 1;
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

u16 Spu_CalcVolume(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u32  temp;
    u32  hi;
    u32  lo;
    u32  offset;
    u16* base;

    temp  = arg1 + (arg0 << 8);
    temp  = temp - ((arg2 << 8) - (arg3 << 1));
    temp += 0x4800;

    lo     = (temp & 0xFF) >> 1;
    offset = 0;
    hi     = temp & 0xFFFF;
    do {
        base = D_80068BB8;
        hi >>= 8;
        if (hi != 0) {
            offset = hi << 1;
        }
    } while (0);
    lo = ((u32) * (u16*)((u8*)base + offset) * (u32)D_80068C78[lo]) >> 8;
    if ((lo & 0xFFFF) >= 0x4000) {
        lo = 0x3FFF;
    }
    return lo;
}

SndNote* Snd_GetNote(SndBank* arg0, u8 arg1, u8 arg2)
{
    if (arg0 != NULL) {
        return &arg0->field_4[arg0->field_10[arg1] + arg2];
    }
    return NULL;
}

void Spu_KeyOnClearOff(u32 voiceIdx)
{
    SpuVoiceState* p;
    u32*           pKeyOn;
    u32            channel;

    p                     = &Spu_VoiceState;
    pKeyOn                = &D648E0_8007EBA8;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    p->field_1d0         |= channel;
    D648E0_8007EBB0      &= ~channel;
}

void Spu_ArmKeyOn(u32 voiceIdx)
{
    SpuVoiceState* p;
    u32*           pKeyOn;
    u32            channel;

    p                     = &Spu_VoiceState;
    pKeyOn                = &D648E0_8007EBAC;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    p->field_1d0         &= ~channel;
    D648E0_8007EBA8      &= ~channel;
    D648E0_8007EBB0      &= ~channel;
}
