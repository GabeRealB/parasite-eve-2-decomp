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

    ptr                = (s32*)&Spu_LVoiceTable;
    i                  = 0;
    Spu_KeyOnMask      = 0;
    Spu_KeyOnMaskExtra = 0;
    Spu_KeyOffMask     = 0;
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
            if (entry->count > 0) {
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
                } while (j < entry->count);
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

void Spu_FlushVoiceUpdates(void)
{
    i32             remaining;
    u8*             current;
    SpuLVoiceTable* dataPtr;

    if (Spu_ReverbCfg.isDirty) {
        Spu_ApplyReverbConfig();
        Spu_ReverbCfg.isDirty = false;
    }

    Spu_KeyOffMask |= Spu_KeyOnMask;
    if (Spu_KeyOffMask != 0) {
        SpuSetKey(SPU_OFF, Spu_KeyOffMask);
        Spu_KeyOffMask = 0;
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

    if ((Spu_KeyOnMask | Spu_KeyOnMaskExtra) != 0) {
        SpuSetKey(SPU_ON, Spu_KeyOnMask | Spu_KeyOnMaskExtra);
        if (Spu_KeyOnMask != 0) {
            Spu_VoiceState.field_1cc |= Spu_KeyOnMask;
            Spu_VoiceState.field_1cc &= ~Spu_VoiceState.field_1d0;
        }

        Spu_VoiceState.field_1d0 = 0;
        Spu_KeyOnMask            = 0;
        Spu_KeyOnMaskExtra       = 0;
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

    sIdx     = idx;
    p        = &Spu_VoiceRanges[sIdx];
    p->first = arg1;
    p->count = arg2;
    return 0;
}

s32 Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1)
{
    register s32 v1r asm("v1");
    register s8* base asm("t1");
    register s32 idx asm("a3");
    register s8* slotp asm("t0");
    register s32 a2r asm("a2");
    register s32 found asm("t2");
    register s32 v0r asm("v0");

    /* $a0 stays as voice id for sb; $v1 keeps %hi for dual lhu/lh of count. */
    __asm__ volatile(
        "lui %0, %%hi(Spu_LVoiceTable)\n\t"
        "addiu %1, %0, %%lo(Spu_LVoiceTable)\n\t"
        "sll %2, $4, 24\n\t"
        "sra %3, %2, 24"
        : "=&r"(v1r), "=&r"(base), "=&r"(v0r), "=r"(idx));
    slotp = (s8*)(idx + (s32)base);
    a2r   = (s8)slotp[0x664];
    found = 1;
    if (a2r != 0) {
        v0r           = ((a2r << 4) + a2r) << 2;
        v0r           = v0r + (s32)base;
        arg1->field_0 = arg0;
        arg1->field_4 = (SpuVoiceAttr*)(v0r - 0x3C);
    } else {
        __asm__ volatile("move %0, $0" : "=r"(found));
        __asm__ volatile(
            "lhu %0, %%lo(Spu_LVoiceTable)(%2)\n\t"
            "lh  %1, %%lo(Spu_LVoiceTable)(%2)\n\t"
            "addiu %0, %0, 1\n\t"
            "sh %0, %%lo(Spu_LVoiceTable)(%2)"
            : "=&r"(v0r), "=&r"(a2r)
            : "r"(v1r)
            : "memory");
        /* GCC reorders entry vs count+1; keep target's v1/v0 schedule. */
        __asm__ volatile(
            ".set\tnoreorder\n\t"
            "sll %0, %2, 4\n\t"
            "addu %0, %0, %2\n\t"
            "sll %0, %0, 2\n\t"
            "addu %1, %0, %3\n\t"
            "sh %4, 4(%1)\n\t"
            "addiu %1, %2, 1\n\t"
            "addu %0, %0, %3\n\t"
            "addiu %0, %0, 8\n\t"
            ".set\treorder"
            : "=&r"(v0r), "=&r"(v1r)
            : "r"(a2r), "r"(base), "r"(idx));
        slotp[0x664]          = v1r;
        arg1->field_0         = arg0;
        arg1->field_4         = (SpuVoiceAttr*)v0r;
        *(s32*)((u8*)v0r + 4) = 0;
        arg1->field_1         = 0;
        arg1->field_3         = 0;
        arg1->field_2         = 0;
    }
    return found;
}

s32 Spu_ReleaseVoiceSlot(u32 voiceIdx)
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
    pKeyOn                = &Spu_KeyOnMask;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    channel               = ~channel;
    p->field_1d0         &= channel;
    Spu_KeyOffMask       &= channel;
}

void Spu_KeyOff(u32 voiceIdx)
{
    u32* pKeyOff;
    u32  channel;

    pKeyOff  = &Spu_KeyOffMask;
    voiceIdx = (s8)voiceIdx;

    channel             = SPU_VOICECH(voiceIdx);
    *pKeyOff           |= channel;
    Spu_KeyOnMask      &= ~channel;
    Spu_KeyOnMaskExtra &= ~channel;
}

void Spu_QueryReverbVoices(void)
{
    Spu_VoiceState.reverbVoiceStatus = SpuGetReverbVoice();
}

void Spu_ConfigReverb(s32 mode)
{
    SpuReserveReverbWorkArea(SPU_ON);
    SpuSetReverbVoice(SPU_OFF, SPU_ALLCH);
    SpuSetReverb(SPU_ON);

    Spu_ReverbCfg.attr.mask = SPU_REV_MODE;
    Spu_ReverbCfg.attr.mode = mode;
    SpuSetReverbModeParam(&Spu_ReverbCfg.attr);

    Spu_ReverbCfg.attr.mask        = SPU_REV_DEPTHR | SPU_REV_DEPTHL;
    Spu_ReverbCfg.attr.depth.right = 0;
    Spu_ReverbCfg.attr.depth.left  = 0;
    SpuSetReverbDepth(&Spu_ReverbCfg.attr);

    Spu_ReverbCfg.attr.mask = 0;
}

void Spu_SetReverbDepth(s16 depth)
{
    Spu_ReverbCfg.isDirty          = true;
    Spu_ReverbCfg.attr.depth.right = depth;
    Spu_ReverbCfg.attr.depth.left  = depth;
    Spu_ReverbCfg.attr.mask       |= SPU_REV_DEPTHR | SPU_REV_DEPTHL;
}

void Spu_SetReverbMode(u32 mode)
{
    if (Spu_ReverbCfg.reverbMode != mode && Spu_ReverbCfg.reverbMode != SPU_REV_MODE_OFF) {
        SpuClearReverbWorkArea(Spu_ReverbCfg.reverbMode);
        Spu_ReverbCfg.isDirty    = true;
        Spu_ReverbCfg.attr.mask |= SPU_REV_MODE;
        Spu_ReverbCfg.attr.mode  = mode;
        Spu_ReverbCfg.reverbMode = mode;
    }
}

void Spu_EnableReverbVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    channel                      = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.enableVoices  |= channel;
    Spu_ReverbCfg.disableVoices &= ~channel;
}

void Spu_DisableReverbVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    channel                      = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.disableVoices |= channel;
    Spu_ReverbCfg.enableVoices  &= ~channel;
}

bool Spu_ReverbVoiceIsEnabled(u32 voiceIdx)
{
    return ((s32)Spu_VoiceState.reverbVoiceStatus >> voiceIdx) & 1;
}

void Spu_ApplyReverbConfig(void)
{
    if (Spu_ReverbCfg.disableVoices != 0) {
        SpuSetReverbVoice(SPU_OFF, Spu_ReverbCfg.disableVoices);
        Spu_ReverbCfg.disableVoices = 0;
    }

    if (Spu_ReverbCfg.enableVoices != 0) {
        SpuSetReverbVoice(SPU_ON, Spu_ReverbCfg.enableVoices);
        Spu_ReverbCfg.enableVoices = 0;
    }

    if ((Spu_ReverbCfg.attr.mask & SPU_REV_MODE) != 0) {
        SpuSetReverbModeParam(&Spu_ReverbCfg.attr);
    }
    if ((Spu_ReverbCfg.attr.mask & SPU_REV_DEPTHL) != 0) {
        SpuSetReverbDepth(&Spu_ReverbCfg.attr);
    }

    Spu_ReverbCfg.attr.mask = 0;
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
    pKeyOn                = &Spu_KeyOnMask;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    p->field_1d0         |= channel;
    Spu_KeyOffMask       &= ~channel;
}

void Spu_ArmKeyOn(u32 voiceIdx)
{
    SpuVoiceState* p;
    u32*           pKeyOn;
    u32            channel;

    p                     = &Spu_VoiceState;
    pKeyOn                = &Spu_KeyOnMaskExtra;
    voiceIdx              = (s8)voiceIdx;
    p->field_7c[voiceIdx] = 5;
    channel               = SPU_VOICECH(voiceIdx);
    *pKeyOn              |= channel;
    p->field_1d0         &= ~channel;
    Spu_KeyOnMask        &= ~channel;
    Spu_KeyOffMask       &= ~channel;
}
