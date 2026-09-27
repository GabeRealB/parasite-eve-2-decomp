#include "common.h"

#include <psyq/abs.h>

#include "main/unknown_syms.h"
#include "main/cdaudio.h"

static volatile u8 D_80082138[0x10];

/// The loaded sound banks: one record per bank, at the slot its bank type maps
/// to.
///
/// A record is filled in by whichever load path brought the bank in — a
/// resident bank from the bank-init table when the sound system starts, a
/// streamed one as its load completes — and released through
/// `SndBankSlot_Free`, which returns the image to the sound heap and marks the
/// record free.
static SndBankSlot _gSndBankSlots[16];

static SndScript    SndScript_Slots[8];
static s32          D_80082548[0x80];
static s8           D_80082748;
static s8           D_80082749;
static s8           D_8008274A;
static s8           D_8008274B;
static volatile s32 D_8008274C;

static void         SndEvt_EnqueueTypeF(void);
static s32          SndScript_Exec(SndScript* script);
static s32          SndScript_FindOneA(u8* arg0, s16 arg1, SndOneAOut* arg2);
static void         SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* arg4, SndVoiceParams* arg5);
static s32          SndScript_TickVoices(SndScript* arg0);
static SndVoice*    SndVoice_Alloc(s32 arg0);
static void         SndVoice_Attach(SndVoiceOwner* arg0, SndVoice* arg1);
static void         SndVoice_ClearActive(void);
static s32          SndVoice_DriveSlots(s32* unused);
static void         SndVoice_Init(void);
static void         SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* arg2, LinInterp* arg3, s16* arg4);
static void         SndVoice_SetPriority(s8 arg0);
static void         SndVoice_SetPriorityLevel(s8 arg0);
static void         SndVoice_SetupEnvelope(SndVoice* voice, s16 envelopeOffset, u32 pitch, SndNote* note);
static s32          SndVoice_Tick(SndVoice* arg0);
static void         SndVoice_TickEnvelope(SndVoice* arg0);
static void         Snd_SetBusyFlag(s32 arg0);
static s8           func_80055EF8(SndVoicePick* arg0, s32 arg1);
static SndBankSlot* sndBankSlotFind(u16 bankId, s32 byType);

static u8               D_80068A54[]        = { 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x26, 0x20, 0x26, 0x2E, 0x05, 0x1E, 0xFF };
static SndBankInitEntry Snd_BankInitTable[] = {
    { 0x0002, 0x20FF, 0x00CE, 0x0210, 0x73810 },
    { 0x000E, 0xE0FF, 0x0078, 0x0168, 0x6F810 },
};
s32        D_80068A78   = 0;
static s16 D_80068A7C[] = { 1, 2 };

void Snd_InitFromStage(s32 arg0, s32 arg1)
{
    u8* var_s0;
    s32 var_a0;
    s32 var_v1;
    s32 temp_v1;

    D_8008274C = 0;
    SndVoice_ClearActive();
    arg0 = arg0 & 0xFF;
    SndEvt_EnqueueTypeF();
    SndEvt_EnqueueType7(0x50000000, 1);
    SndEvt_EnqueueType7(0x10000000, 1);
    SndEvt_EnqueueType7(0xFF0D, 1);
    SndEvt_EnqueueType7(0x20000000, 1);
    SndEvt_EnqueueType7(0xE0000000, 1);
    arg1       = arg1 & 0xFF;
    D_80082120 = arg0;
    D_80082136 = arg1;
    SndBankSlot_Free(1);
    SndBankSlot_Free(7);

    var_v1 = 0;
    if (arg1 == 5) {
        if (arg0 == 4) {
            var_a0 = 3;
        } else {
            goto block_5;
        }
    } else {
    block_5:
        do {
            if (D_80068A54[var_v1 + arg0 * 2] == arg1) {
                var_a0 = 2;
                goto block_done;
            }
            var_v1++;
        } while (var_v1 < 2);
        var_a0 = 1;
    }
block_done:
    SndVoice_SetPriority(var_a0);
    D_80082130 = 0x3D010;
    D_80082128 = 0;
    D_80082124 = D_80082128;

    temp_v1 = (s8)D_80082135;
    switch (temp_v1) {
        case 0:
            Snd_FreeBank(&Snd_Banks[4]);
            SndBankSlot_Free(4);
        case 1:
            D_80082122 = 0;
            break;
        case 2:
            D_80082122 = 1;
            break;
    }
    var_s0 = (u8*)&Snd_Banks[1];

    SndLoad_State.field_14 = 0;
    SndLoad_State.field_18 = 0;
    D_8008212C             = D_80082122;
    D_80082121             = D_80082135;
    Snd_FreeBank((SndBank*)var_s0);
    Snd_FreeBank((SndBank*)(var_s0 + 0xC0));
    Snd_FreeBank((SndBank*)(var_s0 + 0x80));
    SndBankSlot_Free(5);
    Snd_FreeBank((SndBank*)(var_s0 + 0xA0));
    SndBankSlot_Free(6);
    Snd_FreeBank((SndBank*)(var_s0 + 0x40));
    SndBankSlot_Free(3);
    SndBank_SetEnableFlags(1, 0x40000000);
}

s32 SndLoad_ResolveSpuAddr(s32 arg0, s32 arg1)
{
    s32 temp_a2;

    temp_a2 = (arg0 + 0x3F) & ~0x3F;
    switch ((u32)(arg1 & 0xF000) >> 0xC) {
        case 0:
            arg0 = 0x63810;
            break;
        case 1:
            D_80082128 = 0x63810 - temp_a2;
            arg0       = D_80082128;
            break;
        case 3:
            arg0 = 0x47010;
            break;
        case 4:
            if ((s8)D_80082135 == 1) {
                D_80082135 = 2;
                arg0       = 0x3D010;
                goto set_slot;
            }
            if ((s8)D_80082122 > 0 && (s8)D_80082122 < 3) {
                arg0 = Snd_Banks[(s8)D_80082122 + 3].spuAddr +
                       Snd_Banks[(s8)D_80082122 + 3].imageSize;
                D_80082122 += 1;
                goto store_size;
            }
            arg0 = 0;
            if (D_80082122 != 0) {
                goto clear_ret;
            }
            arg0 = 0x3D010;
        set_slot:
            D_80082122 = 1;
        store_size:
            D_80082130 = temp_a2 + arg0;
            break;
        clear_ret:
            D_80082130 = 0;
            break;
        case 5: {
            s32 top;

            top = D_80082128;
            if (top == 0) {
                top = 0x63810;
            } else {
                top = D_80082128;
            }
            D_80082124 = top - temp_a2;
            arg0       = D_80082124;
            break;
        }
        case 6:
            arg0 = 0x3D010 - temp_a2;
            break;
        case 2:
        case 7:
            arg0 = 0x7B010 - temp_a2;
            break;
        case 14:
            arg0 = 0x6F810;
            break;
        default:
            arg0 = 0;
            break;
    }
    return arg0;
}

/* Per-type arg1 limits for TaskIdMap_RemapIndex; sits between this TU's first
 * jtbl (SndLoad_ResolveSpuAddr) and TaskIdMap's jtbl at 0x80014130. */
static const GBytes6 D_80014124 = { { 0x00, 0x08, 0x07, 0x0B, 0x0C, 0x0A } };

s32 TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2)
{
    GBytes6 sp;
    s32     temp;

    sp   = D_80014124;
    arg2 = arg2 - 1;

    switch (arg0 & 0xFF) {
        case 1:
        case 2:
            break;
        case 3:
            temp = (s8)arg1;
            if (temp >= 9) {
                if ((temp == 0x1A) || (temp == 0x1D)) {
                    arg1 = 0xA;
                } else {
                    arg1 = 9;
                }
            }
            break;
        case 4:
            temp = (s8)arg1;
            if (temp >= 0x14) {
                switch ((s8)(arg1 - 0x17)) {
                    case 0:
                        arg1 = 0xB - arg2;
                        break;
                    case 3:
                        arg1 = 0x10 - arg2;
                        break;
                    case 5:
                        arg1 = 0x11 - arg2;
                        break;
                    case 6:
                        arg1 = 0x12 - arg2;
                        break;
                    case 7:
                        arg1 = 0x13 - arg2;
                        break;
                    default:
                        arg1 = 0xF - arg2;
                        break;
                }
            } else if (temp < 9) {
                arg1 = 0;
            } else {
                arg1 = arg1 - arg2;
            }
            break;
        case 5:
            temp = (s8)arg1;
            switch (temp) {
                case 0x14:
                    arg1 = 0;
                    break;
                case 0x1D:
                    arg1 = 1;
                    break;
                default:
                    temp = arg1 << 24;
                    temp = temp >> 24;
                    arg1 = arg1 - arg2;
                    temp = temp < ((arg2 & 0xFF) + 1);
                    if (temp != 0) {
                        arg1 = 0;
                    }
                    break;
            }
            break;
        default:
            arg1 = 0;
            break;
    }

    if ((u32)(arg1 & 0xFF) >= (u32)sp.data[arg0 & 0xFF]) {
        arg1 = 0;
    }
    return arg1 & 0xFF;
}

static void Snd_ClearBusy(void)
{
    Snd_SetBusyFlag(0);
}

static void Snd_SetBusyFlag(s32 arg0)
{
    if (arg0 == 0) {
        Snd_FreeBank(&Snd_Banks[12]);
        D_80082134 = 0;
        return;
    }
    D_80082134 = 1;
}

void Snd_SetModeFlag(s32 arg0)
{
    s8 temp;

    temp = (s8)D_80082135;
    if (temp == 0) {
        if (arg0 != 0) {
            D_80082135 = 1;
        }
    } else if (temp >= 0) {
        if ((temp < 3) && (arg0 == 0)) {
            D_80082135 = 0;
        }
    }
}

void Snd_PollAsync(s32 unused)
{
    AsyncCb_Poll();
}

void Snd_RegisterTickCallbacks(void)
{
    AudioTick_Insert(Midi_Tick, 0, 0x4800, 0);
    AudioTick_Insert(SndVoice_DriveSlots, 0, 0x8800, 0);
    D_80082130 = 0x3D010;
    D_80082128 = 0x63810;
    D_80082124 = D_80082128;
    D_80082122 = 0;
    D_8008212C = 0;
    D_80082135 = 0;
    D_80082121 = 0;
    D_8008274C = 0;
}

// K&R definition so a missing argument stays legal (indeterminate a0).
static s32 SndBank_RemapId(arg0)
s32        arg0;
{
    s32          var_s0;
    SndBankSlot* temp_v0;

    var_s0 = arg0;
    if ((var_s0 & 0xF0000000) == 0x10000000) {
        temp_v0 = sndBankSlotFind(0x1000, 1);
        if (temp_v0 != NULL) {
            var_s0 = (temp_v0->image->bankId << 0x10) + (var_s0 & 0xFFFF);
        }
    }
    return var_s0;
}

s32 Snd_ReverbWarmupCb(s32* arg0)
{
    s32 temp;

    temp  = *arg0 + 1;
    *arg0 = temp;
    if (temp < 0x3D) {
        return 0;
    }
    Spu_SetReverbDepth(0x2800);
    return -1;
}

void Snd_SetMutedVolumes(s32 arg0)
{
    s32 var_a0;

    if (arg0 == 0) {
        D_800689EC = 0;
        SndVoice_ApplyMasterVolume(0x7F);
        var_a0 = 0x40;
    } else {
        D_800689EC = 1;
        SndVoice_ApplyMasterVolume(0x28);
        var_a0 = 0;
    }
    Midi_SetMasterVolume(var_a0);
}

s32 Snd_InitBanks(u32 arg0)
{
    s32               i;
    s8                slot;
    SndBankSlot*      obj;
    SndBank*          bank;
    SndBankInitEntry* entry;
    s8*               map;
    SndBank*          banks;
    s32               id;

    *(volatile s32*)&D_80068A78 = 0xFF;
    Spu_SetVoiceRange(1, 0x12, 6);
    i = 0;
    SndVoice_Init();
    SndVoice_SetPriority(1);
    SndBank_SetEnableFlags(1, 0x80000000);

    map   = D_800680AC;
    banks = Snd_Banks;
    entry = Snd_BankInitTable;
loop:
    slot         = *(s8*)(entry->field_0 + (s32)map);
    obj          = SndBankSlot_Get(slot);
    id           = entry->field_2;
    bank         = (SndBank*)(((s32)slot << 5) + (s32)banks);
    obj->bank    = bank;
    obj->bankId  = id;
    bank->bankId = entry->field_2;
    i++;
    obj->bank->heapBlock  = SndHeap_Malloc(entry->field_4);
    obj->bank->groups     = obj->bank->heapBlock;
    obj->bank->notes      = obj->bank->heapBlock;
    obj->bank->groupIndex = obj->bank->heapBlock;
    obj->image            = SndHeap_Malloc(entry->field_6);
    obj->spuAddr          = entry->field_8;
    entry++;
    if (i < 2) {
        goto loop;
    }

    *(volatile s32*)&D_80068A78 = 0;
    return -1;
}

s32 SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2)
{
    s32              orig;
    SndBankSlot*     bank;
    SndBankHdr*      header;
    SndVoiceParams*  entry;
    u16              offset;
    u32              index;
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    orig = arg0;
    if ((arg0 == 0) || (arg0 == 8)) {
        return orig;
    }
    if (D_800689E4 != 0xFF) {
        if ((D_800689E4 & 0xF000) == (((u32)arg0 >> 16) & 0xF000)) {
            return -1;
        }
    }
    arg0 = SndBank_RemapId(arg0);
    bank = sndBankSlotFind((u32)arg0 >> 16, 0);
    if (bank == NULL) {
        return -2;
    }
    index  = (u32)arg0 & 0xFF;
    header = bank->image;
    if (index >= header->entryCount) {
        return -2;
    }
    // Pointer form: a subscript would emit the addition base-first, and the
    // target adds the index first.
    offset = *(header->entryOffsets + index);
    if (offset == 0) {
        return -3;
    }
    entry = (SndVoiceParams*)((u8*)header + offset);
    if (D_800689EC != 0) {
        if ((entry->flags & 0x80) != 0) {
            return -5;
        }
    }
    if (D_80082138[(u32)arg0 >> 28] == 0) {
        if ((entry->flags & 1) == 0) {
            return -4;
        }
    }
    temp = sndEvtAlloc();
    if (temp == NULL) {
        return -1;
    }
    temp->handlerIdx        = 6;
    args                    = &temp->args.voice;
    args->id                = arg0;
    args->pan               = arg1;
    args->level.attenuation = arg2;
    args->bank              = bank;
    args->params            = entry;
    sndEvtEnqueue(temp);
    return orig;
}

void SndEvt_EnqueueType7(s32 arg0, s32 arg1)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    temp = sndEvtAlloc();
    if (temp != NULL) {
        temp->handlerIdx = 7;
        args             = &temp->args.voice;
        args->id         = SndBank_RemapId(arg0);
        args->stopFrames = arg1;
        sndEvtEnqueue(temp);
    }
}

void SndEvt_EnqueueType8(s32 arg0)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = sndEvtAlloc();
        if (temp != NULL) {
            temp->handlerIdx = 8;
            args             = &temp->args.voice;
            args->id         = SndBank_RemapId(arg0);
            sndEvtEnqueue(temp);
        }
    }
}

void SndEvt_EnqueueType9(s32 arg0)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = sndEvtAlloc();
        if (temp != NULL) {
            temp->handlerIdx = 9;
            args             = &temp->args.voice;
            args->id         = SndBank_RemapId(arg0);
            sndEvtEnqueue(temp);
        }
    }
}

void SndEvt_EnqueueTypeA(s32 arg0, s32 arg1, s32 arg2)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = sndEvtAlloc();
        if (temp != NULL) {
            temp->handlerIdx        = 0xA;
            args                    = &temp->args.voice;
            args->id                = SndBank_RemapId(arg0);
            args->pan               = arg1;
            args->level.attenuation = arg2;
            sndEvtEnqueue(temp);
        }
    }
}

void SndEvt_EnqueueTypeB(s32 arg0, s32 arg1)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = sndEvtAlloc();
        if (temp != NULL) {
            temp->handlerIdx     = 0xB;
            args                 = &temp->args.voice;
            args->id             = SndBank_RemapId(arg0);
            args->level.loudness = arg1;
            if ((s8)arg1 < 0) {
                args->level.loudness = 0x7F;
            }
            sndEvtEnqueue(temp);
        }
    }
}

void SndBank_SetEnableFlags(s32 arg0, s32 arg1)
{
    SndEvt*          temp;
    SndEvtVoiceArgs* args;

    if (arg1 == 0x80000000) {
        for (arg1 = 0; arg1 < 0x10; arg1++) {
            D_80082138[arg1] = arg0 & 1;
        }
    } else {
        D_80082138[(u32)(arg1 & 0xF0000000) >> 28] = arg0 & 1;
        if (arg0 == 0 && (arg1 & 0xF0000000) == 0x40000000) {
            temp = sndEvtAlloc();
            if (temp != NULL) {
                temp->handlerIdx = 7;
                args             = &temp->args.voice;
                args->id         = SndBank_RemapId(0x40000000);
                args->stopFrames = 1;
                sndEvtEnqueue(temp);
            }
        }
    }
}

static void SndVoice_SetPriority(s8 arg0)
{
    SndVoice_SetPriorityLevel(arg0);
}

s32 SndVoice_HasActiveId(s32 arg0)
{
    return ~SndVoice_FindById(SndBank_RemapId(arg0)) != 0;
}

void SndEvt_EnqueueTypeD(void)
{
    SndEvt* temp;

    temp = sndEvtAlloc();
    if (temp != NULL) {
        temp->handlerIdx = 0xD;
        sndEvtEnqueue(temp);
    }
}

void SndEvt_EnqueueTypeE(void)
{
    SndEvt* temp;

    temp = sndEvtAlloc();
    if (temp != NULL) {
        temp->handlerIdx = 0xE;
        sndEvtEnqueue(temp);
    }
}

static void SndEvt_EnqueueTypeF(void)
{
    SndEvt* temp;

    temp = sndEvtAlloc();
    if (temp != NULL) {
        temp->handlerIdx = 0xF;
        sndEvtEnqueue(temp);
    }
}

s32 SndScript_StopMatching(s32 arg0, s32 arg1)
{
    SndScript* p;
    s32        i;
    s32        group;
    s32        ret;

    if (!(arg0 & 0xFF)) {
        for (i = 0; i < 8; i++) {
            p     = &SndScript_Slots[i];
            group = p->field_0 & 0xF0000000;
            if ((group == arg0) || ((arg0 == 0x80000000) && (group != 0x60000000))) {
                if ((p->field_16 != 4) && (p->field_16 != 0)) {
                    p->field_C  = (arg1 == 1);
                    p->field_16 = 4;
                }
            }
        }
        return -2;
    }

    ret = 0;
    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((p->field_0 == arg0) || ((p->field_0 | 0xFF00) == arg0)) {
            switch (p->field_16) {
                case 2:
                    if (arg1 != 0) {
                        if (arg1 != 1) {
                            LinInterp_Setup(&p->field_50, (u8)D_80082748, 0, arg1);
                            p->field_16 = 0x80;
                            break;
                        }
                        p->field_C = arg1;
                    }
                    /* fallthrough */
                case 4:
                case 8:
                case 16:
                    p->field_16 = 4;
                    break;
                case 1:
                    p->field_16 = 0;
                    break;
            }
        }
        ret = i;
    }
    return ret;
}

static void SndVoice_StepMasterLevel(void)
{
    s16 var_a0;
    s8  bound;

    var_a0 = SndVoice_GetMasterVolume();
    if (D_8008274A > 0) {
        var_a0 = var_a0 + D_8008274A;
        bound  = (u8)D_80082749;
        if (bound < var_a0) {
            if (bound != 0) {
                var_a0     = bound;
                D_80082749 = 0;
            }
            D_8008274A = 0;
        }
    } else if (D_8008274A < 0) {
        var_a0 = var_a0 + D_8008274A;
        if (var_a0 < 0x30) {
            var_a0     = 0x30;
            D_8008274A = 0;
        }
    }
    SndVoice_ApplyMasterVolume(var_a0);
}

static s32 SndVoice_DriveSlots(s32* unused)
{
    SpuVoiceRef   ref;
    s16           vol[2];
    SpuVoiceAttr* attr;
    SndScript*    p;
    SndVoice*     node;
    SndVoice*     voice;
    s32           i;
    s32           count;
    s32           level;
    s32           temp;
    s8            step;
    s16           pan;

    if (D_8008274A != 0) {
        SndVoice_StepMasterLevel();
    }

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        switch (p->field_16) {
            case 0:
                break;

            case 1:
                p->field_C          = 0;
                p->field_D          = 0;
                p->field_8          = 0;
                p->field_4          = 0;
                p->field_40         = NULL;
                p->field_16         = 2;
                p->field_50.field_E = 0;
                p->field_12         = 0;
                p->field_15         = 0;
                p->field_E          = 0;
                goto run;

            case 0x80:
                if (p->field_50.field_0 == p->field_50.field_4) {
                    p->field_16 = 4;
                    goto stop;
                }
                LinInterp_Step(&p->field_50);
                p->field_E = 1;
                /* fallthrough */
            case 2:
            run:
                p->field_4++;
                while (SndScript_Exec(p) != 0) {
                }
            update:
                count = 0;
                if (p->field_40 != NULL) {
                    node = p->field_40;
                    do {
                        SndVoice_Tick(node);
                        step = p->field_12;
                        count++;
                        if (step != 0) {
                            level = step + (s8)p->field_10 * 4;
                            if (step > 0) {
                                if ((s8)p->field_11 * 4 < level) {
                                    p->field_10 = p->field_11;
                                    p->field_12 = 0;
                                } else {
                                    /* level / 4, rounded toward zero */
                                    temp = level;
                                    if (temp < 0) {
                                        temp += 3;
                                    }
                                    p->field_10 = temp >> 2;
                                }
                            } else if (level < (s8)p->field_11 * 4) {
                                p->field_10 = p->field_11;
                                p->field_12 = 0;
                            } else {
                                temp = level;
                                if (temp < 0) {
                                    temp += 3;
                                }
                                p->field_10 = temp >> 2;
                            }
                            p->field_E = 1;
                        }
                        step = p->field_15;
                        if (step != 0) {
                            pan = (s8)p->field_13 + step;
                            if (step > 0) {
                                if ((s8)p->field_14 < pan) {
                                    p->field_13 = p->field_14;
                                    p->field_15 = 0;
                                } else {
                                    p->field_13 = pan;
                                }
                            } else if (pan < (s8)p->field_14) {
                                p->field_13 = p->field_14;
                                p->field_15 = 0;
                            } else {
                                p->field_13 = pan;
                            }
                            p->field_E = 1;
                        }
                        if (p->field_E == 1) {
                            Spu_GetVoiceRef(node->field_0, &ref);
                            attr = ref.field_4;
                            SndVoice_ScaleVolume(p->field_10, p->field_13, node, &p->field_50, vol);
                            attr->volume.left   = vol[0];
                            attr->volume.right  = vol[1];
                            attr->volmode.left  = 0;
                            attr->volmode.right = 0;
                            attr->mask         |= 0xF;
                        }
                        node = node->field_3C;
                    } while (node != NULL);
                    p->field_E = 0;
                }
                if (count == 0 && p->field_D == 1) {
                    goto release;
                }
                break;

            case 8:
                LinInterp_Step(&p->field_50);
                p->field_E = 1;
                goto update;

            case 0x10:
                p->field_E = 1;
                LinInterp_Step(&p->field_50);
                if (p->field_50.field_0 == p->field_50.field_4) {
                    p->field_16 = 2;
                    goto run;
                }
                goto update;

            case 4:
            stop:
                p->field_D = 1;
                if (SndScript_TickVoices(p) != 0) {
                    p->field_16 = 0x20;
                    break;
                }
                goto release;

            case 0x20:
                count = 0;
                if (p->field_40 != NULL) {
                    node = p->field_40;
                    do {
                        if (node->field_10 != 0) {
                            count++;
                            SndVoice_TickEnvelope(node);
                        }
                        node = node->field_3C;
                    } while (node != NULL);
                }
                if (count == 0 && p->field_D == 1) {
                release:
                    p->field_D  = 0;
                    p->field_0  = -1;
                    p->field_16 = 0;
                    for (voice = p->field_40; voice != NULL; voice = voice->field_3C) {
                        voice->field_34 = 0;
                    }
                    p->field_40         = NULL;
                    p->field_50.field_E = 0;
                }
                break;
        }
    }
    return 0;
}

static void SndVoice_ScanCandidates(SndVoicePick* arg0, u16 arg1, s32 arg2, u16 arg3)
{
    s8         i;
    SndScript* p;
    u16        temp;
    s32        score;

    arg0->field_0  = -1;
    arg0->field_10 = -1;
    arg0->field_5  = -1;
    arg0->field_14 = -1;
    arg0->field_6  = -1;
    arg0->field_4  = -1;
    arg0->field_3  = -1;
    arg0->field_1  = -1;
    arg0->field_2  = -1;
    arg0->field_8  = arg1;
    arg0->field_C  = 0xFFFF;
    arg0->field_7  = 0;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if (p->field_16 == 0) {
            arg0->field_3 = i;
        } else if (p->field_16 != 4) {
            temp = p->field_4C->priority;
            if (temp < (u32)arg0->field_8) {
                arg0->field_8 = temp;
                arg0->field_4 = i;
            } else if (arg0->field_8 == temp) {
                if ((arg0->field_5 == -1) || (arg0->field_10 < p->field_4)) {
                    score          = p->field_4;
                    arg0->field_5  = i;
                    arg0->field_10 = score;
                }
            }
            if (((p->field_0 & 0xFFFF00FF) == (arg2 & 0xFFFF00FF)) ||
                (((temp = p->field_4C->flags) & 0x10) && (arg3 == temp))) {
                arg0->field_1 = i;
                if ((arg0->field_2 == -1) || (arg0->field_C > p->field_4)) {
                    score         = p->field_4;
                    arg0->field_2 = i;
                    arg0->field_C = score;
                }
                arg0->field_7 += 1;
                if ((arg0->field_6 == -1) || (arg0->field_14 < p->field_4)) {
                    score          = p->field_4;
                    arg0->field_6  = i;
                    arg0->field_14 = score;
                }
            }
        }
    }
}

void SndVoice_KeyOffMatching(void)
{
    SpuVoiceRef ref;
    s32         i;
    SndScript*  p;
    SndVoice*   head;
    SndVoice*   node;
    s32         type;
    s32         emptyType;

    for (i = 0; i < 8; i++) {
        p    = &SndScript_Slots[i];
        head = p->field_40;
        if (head != NULL) {
            type = p->field_0 & 0xF0000000;
            if (type != 0x60000000) {
                node = head;
                if ((type == 0x10000000) || (type == 0x50000000)) {
                    do {
                        Spu_GetVoiceRef(p->field_40->field_0, &ref);
                        ref.field_4->adsr2  = (ref.field_4->adsr2 & 0xFFE0) | 0xB;
                        ref.field_4->adsr2 |= 0x20;
                        ref.field_4->mask  |= SPU_VOICE_ADSR_ADSR2;
                        Spu_KeyOff(node->field_0);
                        node = node->field_3C;
                    } while (node != NULL);
                    p->field_16 = 0;
                    p->field_0  = -1;
                }
            }
        } else {
            emptyType = p->field_0 & 0xF0000000;
            if ((emptyType == 0x50000000) || (emptyType == 0x10000000)) {
                p->field_16 = 0;
                p->field_0  = -1;
            }
        }
    }
}

/// Advances a script's 16.16 tick clock by one step: a whole tick, or 0.6 of
/// one when the display region is 1.
static inline void _sndScriptAdvanceClock(SndScript* script)
{
    script->field_8 += (gDisplayState.region == 1 ? 0x9999 : 0x10000);
}

/// Decides whether a note plays with reverb, from its own level against a
/// global one. A note at level 3 gets reverb whenever the global level is at
/// least 2; a global level of 3 turns it off for every other note; otherwise a
/// note with a non-negative level gets reverb once the global level reaches it.
static inline u8 _sndScriptUseReverb(SndOneV* oneV)
{
    s32 on;

    if (oneV->field_E == 3 && D_8008274B >= 2) {
        on = 1;
    } else if (oneV->field_E != 3 && D_8008274B == 3) {
        on = 0;
    } else {
        on = 0;
        if (oneV->field_E >= 0) {
            on = D_8008274B >= oneV->field_E;
        }
    }
    return on;
}

static s32 SndScript_Exec(SndScript* script)
{
    SpuVoiceRef   voiceRef;
    s16           volume[2];
    SndScriptCmd* cmd;
    SndOneV*      oneV;
    SndVoice*     voice;
    SndNote*      note;
    SpuVoiceAttr* attr;
    SndBankSlot*  bankSlot;
    SndBank*      bank;
    SndBankHdr*   header;
    s32           result;
    s32           ticks;
    s32           wait;
    s32           index;
    s32           masterVolume;
    u8            noteVolume;
    s32           panSum;
    s16           pan;
    s16           voicePan;
    s32           pitchValue;
    u16           pitch;
    s32           countdown;
    s16           envelopeOffset;

    cmd = script->field_48;
    switch ((u32)cmd->magic) {
        case 0x45656E6F:
            break;
        case 0x43646E65:
        stop:
            script->field_D = 1;
            break;
        case 0x706F6F4C:
            if (script->field_17 >= 8U) {
                goto stop;
            }
            ticks = script->field_8;
            if ((ticks >> 16) >= cmd->field_6) {
                script->field_18[script->field_17] = cmd->field_4;
                script->field_48                   = (SndScriptCmd*)((u8*)script->field_48 + 8);
                script->field_20[script->field_17] = script->field_48;
                script->field_17++;
                script->field_8 -= cmd->field_6 << 16;
                result           = 1;
                goto done;
            } else {
                _sndScriptAdvanceClock(script);
            }
            break;
        case 0x4C646E65:
            if (script->field_17 == 0) {
                goto stop;
            }
            index = script->field_17 - 1;
            if (script->field_18[index] == 1) {
                script->field_48 = (SndScriptCmd*)((u8*)cmd + 4);
                script->field_17--;
            } else {
                script->field_48 = script->field_20[index];
                if (script->field_18[script->field_17 - 1] != 0) {
                    script->field_18[script->field_17 - 1]--;
                }
            }
            result = 1;
            goto done;
        case 0x43656E6F:
            header = script->field_44->image;
            // Pointer form: a subscript would emit the addition base-first, and
            // the target adds the index first.
            script->field_4C = (SndVoiceParams*)((u8*)header + *(header->entryOffsets + (u8)script->field_0));
            script->field_48 = (SndScriptCmd*)((u8*)script->field_48 + 0x10);
        case 0x56656E6F:
            oneV  = (SndOneV*)script->field_48;
            ticks = script->field_8;
            if ((ticks >> 16) < oneV->field_8) {
                _sndScriptAdvanceClock(script);
                result = 0;
                goto done;
            }
            voice  = SndVoice_Alloc(oneV->field_10);
            result = 1;
            if (voice != NULL) {
                bankSlot = script->field_44;
                if (oneV->field_4 != 0) {
                    bank = Snd_FindBank(oneV->field_4);
                    if (bank == 0) {
                        voice->field_8 = 0;
                        Spu_ReleaseVoiceSlot(voice->field_0);
                        Spu_ClearVoiceCallbacks(voice->field_0);
                        voice->field_0 = 0;
                        return 0;
                    }
                    goto setup_voice;
                }
                bank = bankSlot->bank;
            setup_voice:
                Spu_GetVoiceRef(voice->field_0, &voiceRef);
                note         = Snd_GetNote(bank, (u8)oneV->field_6, oneV->field_7);
                attr         = voiceRef.field_4;
                masterVolume = D_80082748;
                attr->addr   = note->waveAddr;
                if ((D_80082749 != 0) && (script->field_4C->flags & 2)) {
                    masterVolume = D_80082749;
                }
                noteVolume = (u8)oneV->field_D;
                if (oneV->field_D < 0) {
                    noteVolume = note->volume;
                }
                voice->field_A = noteVolume;
                voice->field_2 = (s8)((masterVolume * script->field_4C->volume * voice->field_A) / 16129);
                pan            = script->field_4C->pan;
                panSum         = oneV->field_C;
                if (panSum < 0) {
                    panSum = note->pan;
                }
                panSum  += (s16)(pan - 0x40);
                voicePan = panSum;
                if (voicePan < 0x80) {
                    if (voicePan >= 0) {
                        voice->field_3 = panSum;
                    } else {
                        voice->field_3 = 0;
                    }
                } else {
                    voice->field_3 = 0x7F;
                }
                if (SndScript_FindOneA((u8*)script->field_44->image, oneV->field_12, (SndOneAOut*)attr) == -1) {
                    attr->adsr1 = note->adsr1;
                    attr->adsr2 = note->adsr2;
                }
                pitchValue = pitch = oneV->field_14 + (note->keyMin << 7);
                attr->pitch        = Spu_CalcVolume((u32)(pitch & 0xFFFF) >> 7, (pitchValue & 0x7F) * 2, note->rootKey, note->rootFine);
                if (_sndScriptUseReverb(oneV) == 0) {
                    Spu_DisableReverbVoice(voice->field_0);
                    voice->field_1 = 1;
                } else {
                    Spu_EnableReverbVoice(voice->field_0);
                    voice->field_1 = 1;
                }
                SndVoice_ScaleVolume(script->field_10, script->field_13, voice, &script->field_50, volume);
                attr->volume.left   = volume[0];
                attr->volume.right  = volume[1];
                attr->volmode.left  = 0;
                attr->volmode.right = 0;
                attr->mask          = 0x6009F;
                Spu_KeyOn(voice->field_0);
                voice->field_C = oneV;
                countdown      = oneV->field_A == 0 ? 0x7FFFFFFF : oneV->field_A << 16;
                voice->field_4 = countdown;
                SndVoice_Attach((SndVoiceOwner*)script, voice);
                envelopeOffset = oneV->field_16;
                if (envelopeOffset != -1) {
                    SndVoice_SetupEnvelope(voice, envelopeOffset, pitch & 0xFFFF, note);
                    result = 1;
                } else {
                    voice->field_10 = 0;
                    result          = 1;
                }
            }
            script->field_8  = (s32)(script->field_8 - (oneV->field_8 << 0x10));
            script->field_48 = (void*)((u8*)script->field_48 + 0x18);

            goto done;
        case 0x74696157:
            ticks = script->field_8;
            wait  = ((SndWaitCmd*)cmd)->duration;
            if ((ticks >> 16) < wait) {
                _sndScriptAdvanceClock(script);
                result = 0;
                goto done;
            }
            script->field_8  = ticks - (wait << 16);
            script->field_48 = (SndScriptCmd*)((u8*)script->field_48 + 8);
            result           = 1;
            goto done;
        case 0x41656E6F:
        default:
            result = 0;
            goto done;
    }
    result = 0;
done:
    return result;
}

static void SndVoice_TickEnvelope(SndVoice* arg0)
{
    SpuVoiceRef   sp10;
    SndVoiceFx*   fx;
    SndOneE*      chunk;
    s32           pitch;
    s32           temp;
    s32           level;
    SpuVoiceAttr* attr;

    fx    = (SndVoiceFx*)&arg0->field_10;
    chunk = fx->field_20;

    if (fx->field_2 == 1) {
        fx->field_1 = 5;
        temp        = (fx->field_10 - chunk->field_14) * chunk->field_16;
        if (temp > 0) {
            fx->field_E = -chunk->field_16;
        } else {
            fx->field_E = chunk->field_16;
        }

        fx->field_C  = 0;
        fx->field_2  = 2;
        fx->field_1C = fx->field_10;
    }

    switch (fx->field_1) {
        case 0:
            if (fx->field_C < chunk->field_4) {
                fx->field_C++;
                break;
            }
            fx->field_1  = 1;
            fx->field_C  = 0;
            fx->field_14 = 0;
            fx->field_10 = 0;
        case 1:
            pitch = (fx->field_4 << 1) + fx->field_14;
            if (fx->field_C < chunk->field_A) {
                fx->field_C++;
                fx->field_10 = fx->field_14 += chunk->field_8;
                goto apply;
            }
            fx->field_1 = 2;
            fx->field_C = 0;
        case 2:
            pitch = (fx->field_4 << 1) + chunk->field_6;
            if (fx->field_C < chunk->field_C) {
                fx->field_C++;
                goto apply;
            }
            fx->field_1  = 3;
            fx->field_18 = chunk->field_6;
            level        = chunk->field_6;
            fx->field_C  = 0;
            fx->field_10 = level;
        case 3:
            pitch = (fx->field_4 << 1) + fx->field_18;
            if (fx->field_C < chunk->field_E) {
                fx->field_C++;
                fx->field_10 = fx->field_18 += chunk->field_10;
                goto apply;
            }
            fx->field_1 = 4;
        case 4:
            pitch = (fx->field_4 << 1) + chunk->field_12;
            goto apply;
        case 5:
            temp = (fx->field_10 - chunk->field_14) * chunk->field_16;
            if (temp >= 0) {
                fx->field_1 = 6;
            } else {
                fx->field_10 = fx->field_1C += fx->field_E;
            }
            pitch = (fx->field_4 << 1) + fx->field_1C;
            goto apply;
        case 6:
            pitch = (fx->field_4 << 1) + chunk->field_14;
            goto apply;
        default:
            break;
    }
    return;

apply:
    Spu_GetVoiceRef(arg0->field_0, &sp10);
    attr = sp10.field_4;
    attr->pitch =
        Spu_CalcVolume((pitch >> 8) & 0xFFFF, pitch & 0xFF, (u16)fx->field_8, (u16)fx->field_A);
    attr->mask |= SPU_VOICE_PITCH;
}

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, SndBankSlot* arg3, SndVoiceParams* arg4)
{
    SndVoicePick sp18;

    SndVoice_ScanCandidates(&sp18, arg4->priority, arg0, arg4->flags);
    if ((sp18.field_7 < arg4->maxVoices) && (sp18.field_3 != -1)) {
        sp18.field_0 = sp18.field_3;
    } else {
        sp18.field_0 = func_80055EF8(&sp18, arg4->retriggerFrames);
    }
    if (sp18.field_0 >= 0) {
        SndScript_Play(sp18.field_0, arg1, arg2, arg0, arg3, arg4);
    }
    return sp18.field_0;
}

void SndVoice_FadeMatching(s32 arg0, s32 arg1)
{
    s32        i;
    SndScript* p;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((arg0 == p->field_0) || ((p->field_0 & 0xF0000000) == arg0)) {
            if (arg1 == 0) {
                if (p->field_16 == 8) {
                    p->field_16 = 0x10;
                    LinInterp_Setup(&p->field_50, 0, (u8)D_80082748, 8);
                }
            } else {
                if (p->field_16 & 0x22) {
                    p->field_16 = 8;
                    LinInterp_Setup(&p->field_50, (u8)D_80082748, 0, 8);
                }
            }
        }
    }
}

void SndVoice_SetPanRamp(s32 arg0, s32 arg1, s32 arg2)
{
    SndScript* p;
    s32        t;

    arg0       &= 7;
    p           = &SndScript_Slots[arg0];
    t           = *(volatile u8*)&p->field_10;
    t           = arg1 - t;
    p->field_12 = t;
    t           = (s8)t;
    if (t < 0) {
        t = -t;
    }
    if ((t * 4) >= 0x21) {
        p->field_11 = arg1;
        if (p->field_12 <= 0) {
            if (p->field_12 < 0) {
                t = -8;
            } else {
                t = 0;
            }
        } else {
            t = 8;
        }
        p->field_12 = t;
    } else {
        p->field_10 = arg1;
        p->field_12 = 0;
    }
    p->field_E = 1;

    arg1 = (s8)arg2;
    if (arg1 < 0) {
        arg1 = arg1 + 0x7F;
    } else {
        arg1 = 0x7F - arg1;
    }
    SndVoice_SetVolumeRamp(arg0, arg1);
    p->field_E = 1;
}

void SndVoice_SetVolumeRamp(s32 arg0, s32 arg1)
{
    SndScript* p;
    u8         vol;
    s16        diff;

    p     = &SndScript_Slots[arg0 & 7];
    diff  = ~arg1 & 0x7F;
    vol   = diff;
    diff -= (s8)p->field_13;
    if (ABS(diff) > 0x20) {
        p->field_14 = vol;
        if (diff <= 0) {
            if (diff < 0) {
                p->field_15 = -8;
            } else {
                p->field_15 = 0;
            }
        } else {
            p->field_15 = 8;
        }
    } else {
        p->field_13 = vol;
        p->field_15 = 0;
    }
    p->field_E = 1;
}

void SndVoice_IncRefCount(void)
{
    s8 temp;

    D_8008274C += 1;
    if (D_8008274C == 1) {
        if (D_8008274A == 0) {
            if (D_80082749 == 0) {
                temp = SndVoice_GetMasterVolume();
                if (temp >= 0x30) {
                    D_80082749 = temp;
                    D_8008274A = -8;
                }
            }
        }
    }
}

void SndVoice_TickRefCount(void)
{
    if (D_8008274C > 0) {
        D_8008274C -= 1;
        if (D_8008274C == 0) {
            if (D_80082749 != 0) {
                D_8008274A = 8;
            }
        }
    }
}

static void SndVoice_Init(void)
{
    u32  i;
    s32* ptr;

    ptr = (s32*)SndScript_Slots;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0xC0U);

    ptr = (s32*)_gSndBankSlots;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x40U);

    ptr = (s32*)D_80082548;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x80U);

    D_8008274A = 0;
    D_80082749 = 0;
    SndVoice_ApplyMasterVolume(0x7F);
    SndVoice_SetPriorityLevel(1);
}

static void SndVoice_SetPriorityLevel(s8 arg0)
{
    if (arg0 < 0) {
        D_8008274B = -1;
        return;
    }
    D_8008274B = arg0;
    if (arg0 == 0) {
        D_8008274B = 1;
    }
}

s32 SndVoice_FindById(s32 arg0)
{
    s32        i;
    SndScript* p;

    i = 0;
    p = SndScript_Slots;
    do {
        if ((p->field_16 & 0xA3) && (p->field_0 == arg0)) {
            return i;
        }
        i++;
        p++;
    } while (i < 8);
    return -1;
}

void SndVoice_ApplyMasterVolume(s8 arg0)
{
    SndScript* p;
    s32        i;
    SndVoice*  node;
    SndVoice*  temp;
    s8         vol;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((p->field_F != 1) || (D_80082749 == 0)) {
            temp = p->field_40;
            if (temp != NULL) {
                node = temp;
                do {
                    node->field_2 = (arg0 * p->field_4C->volume * node->field_A) / 16129;
                    node          = node->field_3C;
                } while (node != NULL);
                p->field_E = 0;
            }
            p->field_E = 1;
        }
    }
    vol = arg0;
    if (vol < 0) {
        vol = 0;
    }
    D_80082748 = vol;
}

s8 SndVoice_GetMasterVolume(void)
{
    return D_80082748;
}

static s8 func_80055EF8(SndVoicePick* arg0, s32 arg1)
{
    s32 v;
    u8  u;
    s32 none;

    none = -1;
    if (arg1 == none) {
        return -9;
    }
    if (arg0->field_C < arg1) {
        return -5;
    }
    if (arg0->field_2 != none) {
        goto field6;
    }
    v = arg0->field_4;
    u = arg0->field_4;
    if (v != none) {
        goto store;
    }
    v = arg0->field_5;
    u = arg0->field_5;
join:
    if (v == none) {
        goto ret_m6;
    }
store:
    arg0->field_0 = u;
    return v;
field6:
    v = arg0->field_6;
    u = arg0->field_6;
    goto join;
ret_m6:
    return -6;
}

static void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* arg4, SndVoiceParams* arg5)
{
    SndScript*      p;
    SndVoice*       node;
    SndVoiceParams* desc;
    u16             flags;

    desc = arg5;
    p    = &SndScript_Slots[arg0];
    node = p->field_40;
    if (node != NULL) {
        do {
            Spu_KeyOff(node->field_0);
            node->field_8 = 0;
            Spu_ClearVoiceCallbacks(node->field_0);
            Spu_ReleaseVoiceSlot(node->field_0);
            node->field_0 = 0;
            node          = node->field_3C;
        } while (node != NULL);
    }
    p->field_16 = 1;
    p->field_40 = NULL;
    p->field_44 = arg4;
    p->field_0  = arg3;
    p->field_4  = 0;
    p->field_10 = arg1;
    p->field_13 = arg2;
    p->field_17 = 0;
    flags       = desc->flags;
    p->field_48 = (SndScriptCmd*)arg5;
    p->field_F  = (flags >> 1) & 1;
}

static void SndVoice_Detach(SndVoice* arg0)
{
    SndVoice* temp_v0;
    SndVoice* temp_v1;

    if (arg0 != NULL) {
        temp_v0       = arg0->field_38;
        arg0->field_8 = 0;
        arg0->field_0 = 0;
        if (temp_v0 == NULL) {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0 = (SndVoice*)arg0->field_34;
                if (temp_v0 != NULL) {
                    ((SndVoiceOwner*)temp_v0)->field_40 = NULL;
                }
            } else {
                temp_v0 = (SndVoice*)arg0->field_34;
                if (temp_v0 != NULL) {
                    ((SndVoiceOwner*)temp_v0)->field_40 = temp_v1;
                }
                temp_v0           = arg0->field_3C;
                temp_v0->field_38 = NULL;
            }
        } else {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0->field_3C = NULL;
            } else {
                temp_v0->field_3C = temp_v1;
                temp_v1           = arg0->field_3C;
                temp_v0           = arg0->field_38;
                temp_v1->field_38 = temp_v0;
            }
        }
        arg0->field_38 = NULL;
        arg0->field_3C = NULL;
    }
}

/// Finds the slot holding the loaded bank that `bankId` names.
///
/// A bank id carries the bank's type in its high nibble, and `byType` selects
/// how it is compared with the id each loaded bank carries: 0 matches the whole
/// id, 1 only the `0xF000` type band. A search that matches nothing returns
/// `NULL`.
static SndBankSlot* sndBankSlotFind(u16 bankId, s32 byType)
{
    s32          i;
    SndBankSlot* slot;
    SndBank*     bank;
    s32          key;

    switch (byType) {
        case 0:
            i    = 0;
            key  = bankId;
            slot = _gSndBankSlots;
            do {
                bank = slot->bank;
                if (bank != NULL) {
                    if (bank->bankId == key) {
                        return slot;
                    }
                }
                i++;
                slot++;
            } while (i < 0x10);
            return NULL;
        case 1:
            i    = 0;
            key  = bankId & 0xF000;
            slot = _gSndBankSlots;
            do {
                bank = slot->bank;
                if (bank != NULL) {
                    if ((bank->bankId & 0xF000) == key) {
                        return slot;
                    }
                }
                i++;
                slot++;
            } while (i < 0x10);
            break;
    }
    return NULL;
}

SndBankSlot* SndBankSlot_Get(s32 arg0)
{
    if ((u8)arg0 < 0x10) {
        return &_gSndBankSlots[(s8)arg0];
    }
    return NULL;
}

void SndBankSlot_Free(s32 arg0)
{
    SndBankSlot* temp_s0;
    SndBankSlot* base;

    if ((u8)arg0 < 0x10) {
        base    = _gSndBankSlots;
        temp_s0 = &base[(s8)arg0];
        SndHeap_Free(temp_s0->image);
        temp_s0->bankId = -1;
        temp_s0->image  = NULL;
    }
}

static SndVoice* SndVoice_Alloc(s32 arg0)
{
    s32       voiceIdx;
    SndVoice* ptr;

    voiceIdx = (s8)Spu_AllocVoice(D_80068A7C, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    /* The voice records are addressed from the bank-slot table's symbol at a
     * SndVoice stride; which object really lives there is unresolved. */
    ptr          = (SndVoice*)_gSndBankSlots + voiceIdx;
    ptr->field_0 = voiceIdx;
    Spu_SetVoiceCallbacks(voiceIdx, (s32)SndVoice_Detach, (s32)ptr);
    ptr->field_8 = 1;
    return ptr;
}

static void SndVoice_Attach(SndVoiceOwner* arg0, SndVoice* arg1)
{
    SndVoice* temp_v0;

    if (arg0 != NULL) {
        temp_v0 = arg0->field_40;
        if (temp_v0 != NULL) {
            arg0->field_40    = arg1;
            arg1->field_3C    = temp_v0;
            temp_v0->field_38 = arg1;
            arg1->field_38    = NULL;
            arg1->field_34    = arg0;
            return;
        }
        arg0->field_40 = arg1;
        arg1->field_34 = arg0;
        arg1->field_3C = NULL;
        arg1->field_38 = NULL;
        return;
    }
    arg1->field_3C = NULL;
    arg1->field_38 = NULL;
    arg1->field_34 = NULL;
}

static s32 SndVoice_Tick(SndVoice* arg0)
{
    s32 temp;

    temp = arg0->field_4;
    if (temp <= 0) {
        arg0->field_4 = 0;
        Spu_KeyOff(arg0->field_0);
        if (arg0->field_10 != 0) {
            if (arg0->field_12 == 0) {
                arg0->field_12 = 1;
            }
            goto block_8;
        }
    } else {
        if (temp <= 0x7FFFFFFE) {
            if (gDisplayState.region == 1) {
                arg0->field_4 = temp + 0xFFFF6667;
            } else {
                arg0->field_4 = temp + 0xFFFF0000;
            }
        }
    block_8:
        if (arg0->field_10 != 0) {
            SndVoice_TickEnvelope(arg0);
        }
    }
    return 0;
}

static s32 SndScript_TickVoices(SndScript* arg0)
{
    SpuVoiceRef sp10;
    SndVoice*   node;
    SndVoice*   head;
    s32         count;
    u8          status;
    u16         temp;

    head  = arg0->field_40;
    count = 0;
    if (head != NULL) {
        node = head;
        do {
            if (node->field_0 >= 0) {
                if (arg0->field_C != 1) {
                    status = Spu_GetVoiceStatus(node->field_0);
                    if (status != 0) {
                        Spu_GetVoiceRef(node->field_0, &sp10);
                        temp                = sp10.field_4->adsr2;
                        temp                = (temp & 0xFFE0) | 5;
                        sp10.field_4->adsr2 = temp;
                        sp10.field_4->mask |= SPU_VOICE_ADSR_ADSR2;
                        if (status != 2) {
                            Spu_KeyOff(node->field_0);
                        }
                    }
                } else {
                    Spu_KeyOff(node->field_0);
                }
                if (node->field_10 != 0) {
                    count         += 1;
                    node->field_12 = 1;
                }
            }
            node = node->field_3C;
        } while (node != NULL);
    }
    return count;
}

static void SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* arg2, LinInterp* arg3, s16* arg4)
{
    s32 vol;

    if (arg2->field_0 >= 0) {
        vol = 0x7F - abs(arg1);
        vol = arg2->field_2 * abs(vol) / 127;
        vol = (vol < 0x80) ? ((vol < 0) ? 0 : vol) : 0x7F;
        Spu_ApplyPanVolume(arg4, (s8)arg2->field_3 + arg0 * 3,
                           LinInterp_Apply(arg3, D_80068E78[vol]));
    }
}

static void SndVoice_SetupEnvelope(SndVoice* voice, s16 envelopeOffset, u32 pitch, SndNote* note)
{
    SndVoiceFx* p;
    u8*         base;
    SndOneE*    chunk;
    s32         magic;
    s16         temp;

    p = (SndVoiceFx*)&voice->field_10;
    if (envelopeOffset == -1) {
        voice->field_10 = 0;
        return;
    }
    if (voice->field_34 == NULL) {
        voice->field_10 = 0;
        return;
    }
    base        = *voice->field_34->field_44;
    chunk       = (SndOneE*)&base[envelopeOffset];
    p->field_20 = chunk;
    magic       = chunk->magic;
    if (magic == 0x45656E6F) {
        voice->field_10 = 1;
        p->field_1      = 0;
        p->field_2      = 0;
        p->field_4      = pitch & 0xFFFF;
        p->field_8      = note->rootKey;
        temp            = note->rootFine;
        p->field_C      = 0;
        p->field_14     = 0;
        p->field_18     = 0;
        p->field_1C     = 0;
        p->field_A      = temp;
    }
}

static s32 SndScript_FindOneA(u8* arg0, s16 arg1, SndOneAOut* arg2)
{
    SndOneA* chunk;

    if (arg1 != -1) {
        chunk = (SndOneA*)&arg0[arg1];
        if (chunk->field_0 == 0x41656E6F) {
            arg2->field_3A = chunk->field_4;
            arg2->field_3C = chunk->field_6;
            return 1;
        }
        return -1;
    }
    return -1;
}

static void SndVoice_ClearActive(void)
{
    s32        i;
    s32        mask;
    s32        c600;
    s32        c500;
    s32        c100;
    SndScript* p;
    s32        temp;

    i    = 0;
    mask = 0xF0000000;
    c600 = 0x60000000;
    c500 = 0x50000000;
    c100 = 0x10000000;
    p    = SndScript_Slots;
    do {
        temp = p->field_0 & mask;
        if (temp != c600) {
            if ((temp == c500) || (temp == c100)) {
                p->field_16 = 0;
            }
        }
        i++;
        p++;
    } while (i < 8);
}
