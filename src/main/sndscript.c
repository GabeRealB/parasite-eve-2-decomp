#include "common.h"

#include "main/unknown_syms.h"

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
            Snd_FreeBank((SndBank*)&D_8007E158);
            SndBankSlot_Free(4);
        case 1:
            D_80082122 = 0;
            break;
        case 2:
            D_80082122 = 1;
            break;
    }
    var_s0 = D_8007E0F8;

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

INCLUDE_ASM("main/nonmatchings/sndscript", func_80053A20);

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

void Snd_ClearBusy(void)
{
    Snd_SetBusyFlag(0);
}

void Snd_SetBusyFlag(s32 arg0)
{
    if (arg0 == 0) {
        Snd_FreeBank((SndBank*)&D_8007E258);
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

void Snd_PollAsync(void)
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

// K&R definition so the no-arg call in SndVoice_HasActiveId stays legal (indeterminate a0).
s32 SndBank_RemapId(arg0)
s32 arg0;
{
    s32          var_s0;
    SndBankSlot* temp_v0;

    var_s0 = arg0;
    if ((var_s0 & 0xF0000000) == 0x10000000) {
        temp_v0 = SndBankSlot_Find(0x1000, 1);
        if (temp_v0 != NULL) {
            var_s0 = (temp_v0->field_0->field_4 << 0x10) + (var_s0 & 0xFFFF);
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
    F3E48C_SetReverbDepth(0x2800);
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
    slot          = *(s8*)(entry->field_0 + (s32)map);
    obj           = SndBankSlot_Get(slot);
    id            = entry->field_2;
    bank          = (SndBank*)(((s32)slot << 5) + (s32)banks);
    obj->field_4  = bank;
    obj->field_8  = id;
    bank->field_8 = entry->field_2;
    i++;
    ((SndBank*)obj->field_4)->field_1C = F3D458_Malloc(entry->field_4);
    ((SndBank*)obj->field_4)->field_0 =
        ((SndBank*)obj->field_4)->field_1C;
    ((SndBank*)obj->field_4)->field_4 =
        ((SndBank*)obj->field_4)->field_1C;
    ((SndBank*)obj->field_4)->field_10 =
        ((SndBank*)obj->field_4)->field_1C;
    obj->field_0 = F3D458_Malloc(entry->field_6);
    obj->field_C = (void*)entry->field_8;
    entry++;
    if (i < 2) {
        goto loop;
    }

    *(volatile s32*)&D_80068A78 = 0;
    return -1;
}

s32 SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2)
{
    s32                  orig;
    SndBankSlot*         bank;
    register SndBankHdr* header asm("a0");
    SndVoiceParams*      entry;
    u16                  offset;
    u32                  index;
    SndEvt*              temp;
    SndEvtFrom4*         mid;

    orig = arg0;
    if ((arg0 != 0) && (arg0 != 8)) {
        if (*(volatile s32*)&D_800689E4 != 0xFF) {
            if ((*(volatile s32*)&D_800689E4 & 0xF000) ==
                (((u32)arg0 >> 16) & 0xF000)) {
                return -1;
            }
        }
        arg0  = SndBank_RemapId(arg0);
        bank  = SndBankSlot_Find((u32)arg0 >> 16, 0);
        index = (u32)arg0 & 0xFF;
        if ((bank == NULL) ||
            (header = bank->field_0, (index >= header->field_6))) {
            return -2;
        }
        offset =
            ((SndBankHdrOff*)((index * 2) + (s32)header))->field_8;
        if (offset == 0) {
            return -3;
        }
        entry = (SndVoiceParams*)((s32)header + offset);
        if (*(u16*)&D_800689EC != 0) {
            if ((entry->field_E & 0x80) != 0) {
                return -5;
            }
        }
        if (D_80082138[(u32)arg0 >> 28] == 0) {
            if ((entry->field_E & 1) == 0) {
                return -4;
            }
        }
        temp = SndEvt_Alloc();
        if (temp != NULL) {
            temp->field_2 = 6;
            mid           = (SndEvtFrom4*)&temp->field_4;
            mid->field_4  = arg0;
            temp->field_4 = arg1;
            mid->field_1  = arg2;
            mid->field_8  = (s32)bank;
            mid->field_C  = (s32)entry;
            SndEvt_Enqueue(temp);
            goto ret_orig;
        }
        goto ret_neg1;
    }
ret_orig:
    return orig;
ret_neg1:
    return -1;
}

void SndEvt_EnqueueType7(s32 arg0, s32 arg1)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    temp = SndEvt_Alloc();
    if (temp != NULL) {
        temp->field_2 = 7;
        mid           = (SndEvtFrom4*)&temp->field_4;
        mid->field_4  = SndBank_RemapId(arg0);
        mid->field_2  = arg1;
        SndEvt_Enqueue(temp);
    }
}

void SndEvt_EnqueueType8(s32 arg0)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = SndEvt_Alloc();
        if (temp != NULL) {
            temp->field_2 = 8;
            mid           = (SndEvtFrom4*)&temp->field_4;
            mid->field_4  = SndBank_RemapId(arg0);
            SndEvt_Enqueue(temp);
        }
    }
}

void SndEvt_EnqueueType9(s32 arg0)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = SndEvt_Alloc();
        if (temp != NULL) {
            temp->field_2 = 9;
            mid           = (SndEvtFrom4*)&temp->field_4;
            mid->field_4  = SndBank_RemapId(arg0);
            SndEvt_Enqueue(temp);
        }
    }
}

void SndEvt_EnqueueTypeA(s32 arg0, s32 arg1, s32 arg2)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = SndEvt_Alloc();
        if (temp != NULL) {
            temp->field_2 = 0xA;
            mid           = (SndEvtFrom4*)&temp->field_4;
            mid->field_4  = SndBank_RemapId(arg0);
            temp->field_4 = arg1;
            mid->field_1  = arg2;
            SndEvt_Enqueue(temp);
        }
    }
}

void SndEvt_EnqueueTypeB(s32 arg0, s32 arg1)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = SndEvt_Alloc();
        if (temp != NULL) {
            temp->field_2 = 0xB;
            mid           = (SndEvtFrom4*)&temp->field_4;
            mid->field_4  = SndBank_RemapId(arg0);
            mid->field_1  = arg1;
            if ((s8)arg1 < 0) {
                mid->field_1 = 0x7F;
            }
            SndEvt_Enqueue(temp);
        }
    }
}

void SndBank_SetEnableFlags(s32 arg0, s32 arg1)
{
    u8*          ptr;
    register s32 flag asm("v1");
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if (arg1 == 0x80000000) {
        arg1 = 0;
        ptr  = D_80082138;
        flag = arg0 & 1;
    loop:
        *(u8*)(arg1 + (s32)ptr) = flag;
        arg1                   += 1;
        if (arg1 < 0x10) {
            goto loop;
        }
    } else {
        flag                                  = (s32)D_80082138;
        arg1                                 &= 0xF0000000;
        ((volatile u8*)flag)[(u32)arg1 >> 28] = arg0 & 1;
        if (arg0 == 0) {
            if (arg1 == 0x40000000) {
                temp = SndEvt_Alloc();
                if (temp != NULL) {
                    temp->field_2 = 7;
                    mid           = (SndEvtFrom4*)&temp->field_4;
                    mid->field_4  = SndBank_RemapId(0x40000000);
                    mid->field_2  = 1;
                    SndEvt_Enqueue(temp);
                }
            }
        }
    }
}

void SndVoice_SetPriority(s8 arg0)
{
    SndVoice_SetPriorityLevel(arg0);
}

s32 SndVoice_HasActiveId(void)
{
    return ~SndVoice_FindById(SndBank_RemapId()) != 0;
}

void SndEvt_EnqueueTypeD(void)
{
    SndEvt* temp;

    temp = SndEvt_Alloc();
    if (temp != NULL) {
        temp->field_2 = 0xD;
        SndEvt_Enqueue(temp);
    }
}

void SndEvt_EnqueueTypeE(void)
{
    SndEvt* temp;

    temp = SndEvt_Alloc();
    if (temp != NULL) {
        temp->field_2 = 0xE;
        SndEvt_Enqueue(temp);
    }
}

void SndEvt_EnqueueTypeF(void)
{
    SndEvt* temp;

    temp = SndEvt_Alloc();
    if (temp != NULL) {
        temp->field_2 = 0xF;
        SndEvt_Enqueue(temp);
    }
}

s32 SndScript_StopMatching(s32 arg0, s32 arg1)
{
    s32 i;

    if (!(arg0 & 0xFF)) {
        register SndScript* p asm("v1");
        s32                 four;
        s32                 k60;
        s32                 k80;
        s32                 mask;
        s32                 flag;
        s32                 hi;

        i    = 0;
        mask = 0xF0000000;
        k80  = 0x80000000;
        k60  = 0x60000000;
        four = 4;
        flag = (arg1 == 1);
        {
            register s32 hi asm("v0");
            __asm__ volatile(
                "lui %0, %%hi(SndScript_Slots)\n\t"
                "addiu %1, %0, %%lo(SndScript_Slots)"
                : "=&r"(hi), "=r"(p));
        }
        do {
            hi = p->field_0 & mask;
            if ((hi == arg0) || ((arg0 == k80) && (hi != k60))) {
                if ((p->field_16 != four) && (p->field_16 != 0)) {
                    p->field_C  = flag;
                    p->field_16 = four;
                }
            }
            i++;
            p++;
        } while (i < 8);
        return -2;
    } else {
        register s32 ret asm("v1");
        SndScript*   p;

        i = 0;
        do {
            p = &SndScript_Slots[i];
            if ((p->field_0 == arg0) || ((p->field_0 | 0xFF00) == arg0)) {
                switch (p->field_16) {
                    case 2:
                        if (arg1 == 0) {
                            goto set_status_4;
                        }
                        if (arg1 == 1) {
                            p->field_C = arg1;
                            goto set_status_4;
                        }
                        LinInterp_Setup(&p->field_50, (u8)D_80082748, 0, arg1);
                        p->field_16 = 0x80;
                        break;
                    case 4:
                    case 8:
                    case 16:
                    set_status_4:
                        p->field_16 = 4;
                        break;
                    case 1:
                        p->field_16 = 0;
                        break;
                }
            }
            ret = i;
            i   = ret + 1;
        } while (i < 8);
        return ret;
    }
}

void SndVoice_StepMasterLevel(void)
{
    s16 var_a0;
    s8  bound;

    var_a0 = SndVoice_GetMasterVolume();
    if (D_8008274A > 0) {
        var_a0 = var_a0 + D_8008274A;
        bound  = *(u8*)&D_80082749;
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

s32 SndVoice_DriveSlots(void)
{
    SpuVoiceRef   sp18;
    s16           sp20[2];
    SpuVoiceAttr* voice;
    SndScript*    p;
    SndVoice*     node;
    SndVoice*     head;
    SndVoice*     walk;
    s32           i;
    register s32  one asm("s4");
    register s32  four asm("s7");
    register s32  offset asm("s5");
    s32           count;
    s32           new_val;
    register s32  temp asm("v0");
    s32           status;
    s32           tmp;
    s8            step;
    u16           right;
    u32           mask;

    if (D_8008274A != 0) {
        SndVoice_StepMasterLevel();
    }

    i      = 0;
    four   = 4;
    one    = 1;
    offset = i;
    do {
        p      = (SndScript*)((u8*)SndScript_Slots + offset);
        status = p->field_16;
        if (status == four) {
            goto case_4;
        }
        if (status < 5) {
            if (status == one) {
                goto case_1;
            }
            if (status < 2) {
                goto next;
            }
            if (status == 2) {
                goto case_2;
            }
            offset += 0x60;
            goto loop_inc;
        }
        if (status == 0x10) {
            goto case_10;
        }
        if (status < 0x11) {
            if (status == 8) {
                goto case_8;
            }
            offset += 0x60;
            goto loop_inc;
        }
        if (status == 0x20) {
            goto case_20;
        }
        if (status == 0x80) {
            goto case_80;
        }
        offset += 0x60;
        goto loop_inc;

    case_1:
        p->field_C          = 0;
        p->field_D          = 0;
        p->field_8          = 0;
        p->field_4          = 0;
        p->field_40         = NULL;
        tmp                 = 2;
        p->field_16         = tmp;
        p->field_50.field_E = 0;
        p->field_12         = 0;
        p->field_15         = 0;
        p->field_E          = 0;
        goto case_2;

    case_80:
        if (p->field_50.field_0 == p->field_50.field_4) {
            p->field_16 = four;
            goto case_4;
        }
        LinInterp_Step(&p->field_50);
        p->field_E = one;
        /* fallthrough */
    case_2:
        p->field_4 = p->field_4 + 1;
        do {
        } while (SndScript_Exec(p) != 0);

    process_voices:
        head  = p->field_40;
        count = 0;
        if (head != NULL) {
            node = head;
            do {
                SndVoice_Tick(node);
                step   = p->field_12;
                count += 1;
                if (step != 0) {
                    new_val = step + ((s8)p->field_10 * 4);
                    if (step > 0) {
                        if (((s8)p->field_11 * 4) < new_val) {
                            p->field_10 = p->field_11;
                            p->field_12 = 0;
                        } else {
                            temp = new_val;
                            if (temp < 0) {
                                temp += 3;
                            }
                            p->field_10 = temp >> 2;
                        }
                    } else if (new_val < ((s8)p->field_11 * 4)) {
                        p->field_10 = p->field_11;
                        p->field_12 = 0;
                    } else {
                        temp = new_val;
                        if (temp < 0) {
                            temp += 3;
                        }
                        p->field_10 = temp >> 2;
                    }
                    p->field_E = one;
                }
                step = p->field_15;
                if (step != 0) {
                    temp    = (s8)(*(volatile u8*)&p->field_13);
                    temp    = temp + step;
                    new_val = temp;
                    if (step > 0) {
                        temp <<= 16;
                        temp >>= 16;
                        if ((s8)p->field_14 < temp) {
                            p->field_13 = (u8)p->field_14;
                            p->field_15 = 0;
                        } else {
                            goto store13;
                        }
                    } else {
                        temp <<= 16;
                        temp >>= 16;
                        if (temp < (s8)p->field_14) {
                            p->field_13 = (u8)p->field_14;
                            p->field_15 = 0;
                        } else {
                        store13:
                            p->field_13 = new_val;
                        }
                    }
                    p->field_E = one;
                }
                if (p->field_E == one) {
                    Spu_GetVoiceRef(node->field_0, &sp18);
                    voice = sp18.field_4;
                    SndVoice_ScaleVolume((s8)p->field_10, (s8)p->field_13, node, &p->field_50, sp20);
                    voice->volume.left   = sp20[0];
                    right                = sp20[1];
                    mask                 = voice->mask;
                    voice->volmode.left  = 0;
                    voice->volmode.right = 0;
                    voice->mask          = mask | 0xF;
                    voice->volume.right  = right;
                }
                node = node->field_3C;
            } while (node != NULL);
            p->field_E = 0;
        }
        if (count != 0) {
            goto next;
        }
        if (p->field_D == one) {
            goto cleanup;
        }
        offset += 0x60;
        goto loop_inc;

    case_8:
        LinInterp_Step(&p->field_50);
        p->field_E = one;
        goto process_voices;

    case_10:
        p->field_E = one;
        LinInterp_Step(&p->field_50);
        if (p->field_50.field_0 != p->field_50.field_4) {
            goto process_voices;
        }
        tmp         = 2;
        p->field_16 = tmp;
        goto case_2;

    case_4:
        p->field_D = one;
        if (SndScript_TickVoices(p) != 0) {
            p->field_16 = 0x20;
            goto next;
        }
        goto cleanup;

    case_20:
        head  = p->field_40;
        count = 0;
        if (head != NULL) {
            node = head;
            do {
                if (node->field_10 != 0) {
                    count += 1;
                    SndVoice_TickEnvelope(node);
                }
                node = node->field_3C;
            } while (node != NULL);
        }
        if (count != 0) {
            goto next;
        }
        if (p->field_D != one) {
            goto next;
        }

    cleanup:
        walk        = p->field_40;
        p->field_D  = 0;
        p->field_0  = -1;
        p->field_16 = 0;
        if (walk != NULL) {
            do {
                walk->field_34 = 0;
                walk           = walk->field_3C;
            } while (walk != NULL);
        }
        p->field_40         = NULL;
        p->field_50.field_E = 0;

    next:
        offset += 0x60;

    loop_inc:
        i += 1;
    } while (i < 8);
    return 0;
}

void SndVoice_ScanCandidates(SndVoicePick* arg0, u16 arg1, s32 arg2, u16 arg3)
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
            temp = p->field_4C->field_C;
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
                (((temp = p->field_4C->field_E) & 0x10) && (arg3 == temp))) {
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

INCLUDE_ASM("main/nonmatchings/sndscript", SndVoice_KeyOffMatching);

INCLUDE_ASM("main/nonmatchings/sndscript", SndScript_Exec);

void SndVoice_TickEnvelope(SndVoice* arg0)
{
    SpuVoiceRef   sp10;
    SndVoiceFx*   fx;
    SndOneE*      chunk;
    s32           pitch;
    s32           temp;
    s32           rate;
    s32           new_var;
    s32           new_var2;
    SpuVoiceAttr* attr;
    s32           t;

    fx    = (SndVoiceFx*)&arg0->field_10;
    chunk = fx->field_20;

    if (fx->field_2 == 1) {
        fx->field_1 = 5;
        temp        = (fx->field_10 - chunk->field_14) * chunk->field_16;
        rate        = (u16)chunk->field_16;
        if (temp > 0) {
            rate = -rate;
        }
        fx->field_E = rate;
        asm volatile("" ::: "memory");
        t            = fx->field_10;
        fx->field_C  = 0;
        fx->field_2  = 2;
        fx->field_1C = t;
    }

    switch (fx->field_1) {
        case 0:
            if ((s32)fx->field_C < chunk->field_4) {
                fx->field_C = fx->field_C + 1;
                break;
            }
            fx->field_1  = 1;
            fx->field_C  = 0;
            fx->field_14 = 0;
            fx->field_10 = 0;
        case 1:
            pitch = (fx->field_4 << 1) + fx->field_14;
            if (fx->field_C < chunk->field_A) {
                s32 v;
                fx->field_C  = fx->field_C + 1;
                v            = (new_var = fx->field_14);
                fx->field_14 = (v = v + chunk->field_8);
                fx->field_10 = v;
                goto apply;
            }
            fx->field_1 = 2;
            fx->field_C = 0;
        case 2:
            pitch = (fx->field_4 << 1) + chunk->field_6;
            if ((s32)fx->field_C < chunk->field_C) {
                fx->field_C = fx->field_C + 1;
                goto apply;
            }
            fx->field_1  = 3;
            fx->field_18 = chunk->field_6;
            rate         = chunk->field_6;
            fx->field_C  = 0;
            fx->field_10 = rate;
        case 3:
            pitch = (fx->field_4 << 1) + fx->field_18;
            if (fx->field_C < chunk->field_E) {
                s32 v;
                fx->field_C  = fx->field_C + 1;
                v            = (new_var2 = fx->field_18);
                fx->field_18 = (v = v + chunk->field_10);
                fx->field_10 = v;
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
                s32 v;
                v            = fx->field_1C + (s16)fx->field_E;
                fx->field_1C = v;
                fx->field_10 = v;
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

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndVoiceParams* arg4)
{
    SndVoicePick sp18;

    SndVoice_ScanCandidates(&sp18, arg4->field_C, arg0, arg4->field_E);
    if ((sp18.field_7 < arg4->field_7) && (sp18.field_3 != -1)) {
        sp18.field_0 = sp18.field_3;
    } else {
        sp18.field_0 = func_80055EF8(&sp18, arg4->field_8);
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
    SndScript*   p;
    register s32 val asm("a0");
    register s32 t asm("v0");

    p      = &SndScript_Slots[arg0 & 7];
    t      = *(volatile u8*)&p->field_13;
    arg1   = (~arg1) & 0x7F;
    val    = arg1;
    t      = (s8)t;
    arg1  -= t;
    arg1 <<= 16;
    arg1 >>= 16;
    t      = arg1;
    if (arg1 < 0) {
        t = -t;
    }
    if (t >= 0x21) {
        p->field_14 = val;
        if (arg1 <= 0) {
            if (arg1 < 0) {
                p->field_15 = -8;
                goto end;
            }
        } else {
            p->field_15 = 8;
            goto end;
        }
    } else {
        p->field_13 = val;
    }
    p->field_15 = 0;
end:
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

void SndVoice_Init(void)
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

    ptr = (s32*)SndBank_Slots;
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

void SndVoice_SetPriorityLevel(s8 arg0)
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
                    node->field_2 = (arg0 * p->field_4C->field_5 * node->field_A) / 16129;
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

INCLUDE_ASM("main/nonmatchings/sndscript", func_80055EF8);

void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, s32 arg4, SndVoiceParams* arg5)
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
            F3E48C_8004E660(node->field_0);
            node->field_0 = 0;
            node          = node->field_3C;
        } while (node != NULL);
    }
    p->field_16 = 1;
    p->field_40 = NULL;
    p->field_44 = (SndScriptCtx*)arg4;
    p->field_0  = arg3;
    p->field_4  = 0;
    p->field_10 = arg1;
    p->field_13 = arg2;
    p->field_17 = 0;
    flags       = desc->field_E;
    p->field_48 = (SndScriptCmd*)arg5;
    p->field_F  = (flags >> 1) & 1;
}

void SndVoice_Detach(SndVoice* arg0)
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

SndBankSlot* SndBankSlot_Find(u16 arg0, s32 arg1)
{
    s32          i;
    SndBankSlot* slot;
    SndBank*     bank;
    s32          key;

    switch (arg1) {
        case 0:
            i    = 0;
            key  = arg0;
            slot = SndBank_Slots;
            do {
                bank = (SndBank*)slot->field_4;
                if (bank != NULL) {
                    if (bank->field_8 == key) {
                        return slot;
                    }
                }
                i++;
                slot++;
            } while (i < 0x10);
            return NULL;
        case 1:
            i    = 0;
            key  = arg0 & 0xF000;
            slot = SndBank_Slots;
            do {
                bank = (SndBank*)slot->field_4;
                if (bank != NULL) {
                    if ((bank->field_8 & 0xF000) == key) {
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
        return &SndBank_Slots[(s8)arg0];
    }
    return NULL;
}

void SndBankSlot_Free(s32 arg0)
{
    SndBankSlot* temp_s0;
    SndBankSlot* base;

    if ((u8)arg0 < 0x10) {
        base    = SndBank_Slots;
        temp_s0 = &base[(s8)arg0];
        F3D458_Free(temp_s0->field_0);
        temp_s0->field_8 = -1;
        temp_s0->field_0 = NULL;
    }
}

SndVoice* SndVoice_Alloc(s32 arg0)
{
    s32       voiceIdx;
    SndVoice* ptr;

    voiceIdx = (s8)Spu_AllocVoice(D_80068A7C, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    ptr          = (SndVoice*)SndBank_Slots + voiceIdx;
    ptr->field_0 = voiceIdx;
    Spu_SetVoiceCallbacks(voiceIdx, (s32)SndVoice_Detach, (s32)ptr);
    ptr->field_8 = 1;
    return ptr;
}

void SndVoice_Attach(SndVoiceOwner* arg0, SndVoice* arg1)
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

s32 SndVoice_Tick(SndVoice* arg0)
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
            if (Display_State.field_124 == 1) {
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

s32 SndScript_TickVoices(SndScript* arg0)
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

void SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* arg2, LinInterp* arg3, s16* arg4)
{
    register s32 temp_v0 asm("v0");
    s32          temp_v1;

    if (arg2->field_0 >= 0) {
        temp_v0 = arg1;
        if (temp_v0 < 0) {
            temp_v0 = -temp_v0;
        }
        asm volatile("");
        temp_v1 = 0x7F - temp_v0;
        temp_v0 = arg2->field_2;
        if (temp_v1 < 0) {
            temp_v1 = -temp_v1;
        }
        asm volatile("");
        temp_v0 *= temp_v1;
        temp_v1  = temp_v0 / 127;
        if (temp_v1 < 0x80) {
            temp_v0 = temp_v1;
            if (temp_v1 < 0) {
                temp_v0 = 0;
            }
        } else {
            temp_v0 = 0x7F;
        }
        Spu_ApplyPanVolume(arg4, (s16)((s8)arg2->field_3 + (arg0 * 3)),
                           LinInterp_Apply(arg3, D_80068E78[temp_v0]));
    }
}

void SndVoice_SetupEnvelope(SndVoice* arg0, s16 arg1, u32 arg2, SndNote* arg3)
{
    SndVoiceFx* p;
    u8*         base;
    SndOneE*    chunk;
    s32         magic;
    s16         temp;

    p = (SndVoiceFx*)&arg0->field_10;
    if (arg1 == -1) {
        arg0->field_10 = 0;
        return;
    }
    if (arg0->field_34 == NULL) {
        arg0->field_10 = 0;
        return;
    }
    base        = *arg0->field_34->field_44;
    chunk       = (SndOneE*)&base[arg1];
    p->field_20 = chunk;
    magic       = chunk->magic;
    if (magic == 0x45656E6F) {
        arg0->field_10 = 1;
        p->field_1     = 0;
        p->field_2     = 0;
        p->field_4     = arg2 & 0xFFFF;
        p->field_8     = arg3->field_4;
        temp           = arg3->field_5;
        p->field_C     = 0;
        p->field_14    = 0;
        p->field_18    = 0;
        p->field_1C    = 0;
        p->field_A     = temp;
    }
}

s32 SndScript_FindOneA(u8* arg0, s16 arg1, SndOneAOut* arg2)
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

void SndVoice_ClearActive(void)
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

s32 CdAudio_Begin(void)
{
    volatile CdAudioPhase* p;

    p          = &CdAudio_Phase;
    p->field_5 = 1;
    if (CdAudio_Ctl.field_10 == 1) {
        CdAudio_Ctl.field_10 = 0;
        return -3;
    }
    if ((p->field_1 == 0) || (p->field_1 == 4)) {
        p->field_2 = 4;
        return -2;
    }
    if (p->field_0 != 3) {
        p->field_5 = 1;
        return -1;
    }
    if (CdAudio_Loc.field_0 == 6) {
        if (p->field_4 != 0) {
            p->field_4 = 0xB;
            return -2;
        }
    }
    if (CdAudio_Phase.field_2 != 0) {
        return 1;
    }
    CdAudio_StartVolumeRamp(0x20);
    return 0;
}
