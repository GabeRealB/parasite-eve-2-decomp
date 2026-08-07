#include "common.h"

#include "main/unknown_syms.h"

void SndEvt_Process(void)
{
    SndEvt* next;
    SndEvt* cur;
    u32     i;
    s32*    ptr;

    if (SndEvt_Lock == 0) {
        return;
    }
    if (SndEvt_Head == NULL) {
        return;
    }

    do {
        cur = SndEvt_Head;
        if ((u16)cur->field_2 >= 0x10U) {
            ptr = (s32*)SndEvt_Pool;
            i   = 0;
            do {
                *ptr = 0;
                i++;
                ptr++;
            } while (i < 0x1C0U);
            SndEvt_Head = NULL;
            SndEvt_Tail = NULL;
            SndEvt_Lock = 1;
            return;
        }
        SndEvt_Handlers[cur->field_2](cur);
        cur  = SndEvt_Head;
        next = cur->field_18;
        SndEvt_Free(cur);
        if (next == NULL) {
            SndEvt_Tail = NULL;
            SndEvt_Head = NULL;
            break;
        }
        SndEvt_Head = next;
    } while (next != NULL);
}

void SndEvt_Reset(void)
{
    u32  i;
    s32* ptr;

    ptr = (s32*)SndEvt_Pool;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x1C0U);
    SndEvt_Head = NULL;
    SndEvt_Tail = NULL;
    SndEvt_Lock = 1;
}

SndEvt* SndEvt_Alloc(void)
{
    s32     i;
    s32     flag;
    SndEvt* ptr;

    i    = 0;
    flag = 1;
    for (ptr = SndEvt_Pool; i < 0x40; i++, ptr++) {
        if (ptr->field_0 == 0) {
            ptr->field_0 = flag;
            ptr->field_2 = 0;
            return ptr;
        }
    }
    return NULL;
}

void SndEvt_Enqueue(SndEvt* arg0)
{
    SndEvt* temp;

    if (arg0 != NULL) {
        SndEvt_Lock = 0;
        if (SndEvt_Head == NULL) {
            SndEvt_Tail    = arg0;
            SndEvt_Head    = arg0;
            arg0->field_14 = NULL;
        } else {
            temp           = SndEvt_Tail;
            SndEvt_Tail    = arg0;
            arg0->field_14 = temp;
            temp->field_18 = arg0;
        }
        arg0->field_18 = NULL;
        SndEvt_Lock    = 1;
    }
}

void SndEvt_Free(SndEvt* arg0)
{
    if (arg0 != NULL) {
        arg0->field_0  = 0;
        arg0->field_14 = NULL;
        arg0->field_18 = NULL;
    }
}

void func_80050AAC(void)
{
}

void SndEvt_HandleInitSequence(SndEvt* arg0)
{
    Midi_InitSequence(arg0->field_4, arg0->field_6);
}

void SndEvt_HandleStartFadeOut(SndEvt* arg0)
{
    Midi_StartFadeOut(arg0->field_4, arg0->field_6);
}

void SndEvt_HandleFadeOn(SndEvt* arg0)
{
    Midi_FadeVolume(arg0->field_4, 1);
}

void SndEvt_HandleFadeOff(SndEvt* arg0)
{
    Midi_FadeVolume(arg0->field_4, 0);
}

void SndEvt_HandleSetVolume(SndEvt* arg0)
{
    Midi_SetVolumeScale(arg0->field_4, arg0->field_5);
}

void SndEvt_HandleAllocVoice(SndEvt* arg0)
{
    SndEvtFrom4* temp;

    temp = (SndEvtFrom4*)&arg0->field_4;
    SndVoice_AllocSlot(temp->field_4, arg0->field_4, temp->field_1, temp->field_8, (SndVoiceParams*)temp->field_C);
}

void SndEvt_HandleType7(SndEvt* arg0)
{
    SndEvtFrom4* temp;

    temp = (SndEvtFrom4*)&arg0->field_4;
    SndScript_StopMatching(temp->field_4, temp->field_2);
}

void SndEvt_HandleFadeMatchingOn(SndEvt* arg0)
{
    SndVoice_FadeMatching(arg0->field_8, 1);
}

void SndEvt_HandleFadeMatchingOff(SndEvt* arg0)
{
    SndVoice_FadeMatching(arg0->field_8, 0);
}

void SndEvt_HandlePanRamp(SndEvt* arg0)
{
    s32          temp_v0;
    SndEvtFrom4* temp_s0;

    temp_s0 = (SndEvtFrom4*)&arg0->field_4;
    temp_v0 = SndVoice_FindById(temp_s0->field_4);
    if (temp_v0 >= 0) {
        SndVoice_SetPanRamp(temp_v0, (s8)arg0->field_4, (s8)temp_s0->field_1);
    }
}

void SndEvt_HandleVolumeRamp(SndEvt* arg0)
{
    s32          temp_v0;
    SndEvtFrom4* temp_s0;

    temp_s0 = (SndEvtFrom4*)&arg0->field_4;
    temp_v0 = SndVoice_FindById(temp_s0->field_4);
    if (temp_v0 >= 0) {
        SndVoice_SetVolumeRamp(temp_v0, temp_s0->field_1);
    }
}

void SndEvt_HandleRefCountInc(void)
{
    SndVoice_IncRefCount();
}

void SndEvt_HandleRefCountDec(void)
{
    SndVoice_TickRefCount();
}

void SndEvt_HandleKeyOffMatching(void)
{
    SndVoice_KeyOffMatching();
}

s32 Midi_InitSystem(u32 arg0)
{
    s32       i;
    MidiSong* state;
    SndBank*  bank;

    for (i = 0; i <= 0; i++) {
        Midi_InitSlot(i & 0xFF);
    }
    D_8007F2F0 = 0x40;
    D_800820E9 = 0;
    D_800820E0 = 0;
    D_800820E4 = 0;
    Spu_SetVoiceRange(0, 0, 0x10);
    state           = (MidiSong*)Midi_GetSlot(0xFF);
    state->field_1  = 0xFF;
    state->field_A  = 0x10;
    state->field_10 = D_8007F8E0;
    do {
        bank                      = &Snd_Banks[D_800680BB];
        state->field_40           = bank;
        bank->field_8             = 0xF0FF;
        state->field_40->field_1C = F3D458_Malloc(0x582);
    } while (0);
    state->field_40->field_0  = state->field_40->field_1C;
    state->field_40->field_4  = state->field_40->field_1C;
    state->field_40->field_10 = state->field_40->field_1C;
    D_8007E0D4                = (s32)state->field_40->field_1C;
    state->field_3C           = 0x10;
    return -1;
}

s32 Midi_InitSequence(u8 arg0, u16 arg1)
{
    s32        i;
    MidiSong*  obj;
    u8*        data;
    u32        magic;
    s32*       clearPtr;
    u8         sp10;
    u8*        trackPtr;
    s32        offset;
    u8*        table;
    u8*        end;
    u8*        cur;
    s32        d0;
    s32        d1;
    LinInterp* interp;

    i = 0;
    do {
        obj = &Midi_Song + i;
        if (obj->field_1 != 0xFF) {
            if ((obj->field_1 == arg0) && (obj->field_0 == 0)) {
                Midi_InitChannelTable((s32*)obj->field_484);
                data  = (u8*)obj->field_10;
                magic = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                if (magic != 0x4D546864) {
                    return -1;
                }

                {
                    register MidiTrack* entries asm("a1");
                    u32                 j;

                    entries      = obj->entries;
                    obj->field_2 = data[9];
                    clearPtr     = (s32*)entries;
                    obj->field_3 = data[0xB];

                    for (j = 0; j < (u32)((obj->field_3 << 4) - obj->field_3);
                         j++) {
                        *clearPtr++ = 0;
                    }

                    if (obj->field_3 != 0) {
                        register s32* p asm("s0");

                        j   = 0;
                        end = (u8*)&D_800820E0;
                        p   = &entries->field_38;
                        do {
                            trackPtr =
                                (u8*)Midi_ResolveTrackData(obj, j & 0xFF, (u8*)obj->field_10);
                            ((u8**)p)[-4] = trackPtr;
                            ((u8**)p)[-3] = trackPtr;
                            if ((trackPtr < D_8007F8E0) || (trackPtr >= end)) {
                                return -1;
                            }
                            p[-1] = Midi_ReadVlq(trackPtr, &sp10);
                            j++;
                            asm("" ::: "memory");
                            cur           = ((u8**)p)[-3] + sp10;
                            *p            = 0xE0F;
                            ((u8**)p)[-3] = cur;
                            p            += 15;
                        } while ((s32)j < obj->field_3);
                    }

                    interp        = &obj->field_14;
                    obj->field_44 = obj->field_40->field_0;
                    obj->field_48 = obj->field_40->field_4;
                    d0            = data[0xC];
                    d1            = data[0xD];
                    obj->field_6  = 0xFF;
                    obj->field_4  = 0xFF;
                    obj->field_7  = 0;
                    obj->field_5  = 0;
                    obj->field_34 = (d0 << 8) | d1;
                    table         = D_800689F0;
                    obj->field_8  = (table[obj->field_1] * 3) << 5;
                    LinInterp_Setup(interp, 0, D_8007F2F0, arg1);

                    if (arg1 != 0) {
                        obj->field_0 = 0x40;
                    } else {
                        obj->field_0 = 2;
                    }

                    j             = 0;
                    offset        = 0x504;
                    obj->field_C  = 0xFFFF;
                    obj->field_38 = 0;
                    do {
                        Midi_ClearVoiceEntry((s32*)((u8*)obj + offset));
                        j++;
                        offset += 0xC;
                    } while ((s32)j < 0x12);
                }

                return i;
            }
        } else {
            break;
        }
        i++;
    } while (i <= 0);

    return -5;
}

s32 Midi_Tick(void)
{
    MidiSong*    obj;
    s32          i;
    register s32 j asm("s2");
    register s32 off asm("s3");
    register u8* cursor asm("s1");
    s32          status;
    register s32 two asm("s6");
    register s32 eight asm("s7");
    register s32 ffff asm("s5");

    i     = 0;
    eight = 8;
    two   = 2;
    ffff  = 0xFFFF;
    obj   = &Midi_Song;

loop:
    if (obj->field_1 == 0xFF) {
        goto end;
    }
    status = obj->field_0;
    if (status == eight) {
        goto case_8;
    }
    if (status >= 9) {
        goto high;
    }
    if (status == two) {
        goto case_2;
    }
    if (status < 3) {
        goto end_switch;
    }
    if (status == 4) {
        goto case_4;
    }
    goto end_switch;

high:
    if (status == 0x40) {
        goto case_40_80;
    }
    if (status >= 0x41) {
        goto higher;
    }
    if (status == 0x10) {
        goto case_10;
    }
    goto end_switch;

higher:
    if (status != 0x80) {
        goto end_switch;
    }

case_40_80:
    if (obj->field_14.field_0 != obj->field_14.field_4) {
        goto case_8;
    }
    if (obj->field_0 == 0x40) {
        obj->field_0 = two;
        goto case_8;
    }
    obj->field_0 = 4;
    goto case_4;

case_8:
    LinInterp_Step(&obj->field_14);
    obj->field_C = ffff;
case_2:
    j = 0;
    if (obj->field_3 != 0) {
        off    = 0x4C;
        cursor = (u8*)obj;
        do {
            if (cursor[0x51] == 0) {
                Midi_DriveTrack(obj, (MidiTrack*)((u8*)obj + off));
            }
            off    += 0x3C;
            cursor += 0x3C;
        } while (++j < (s32)obj->field_3);
    }
    obj->field_4 = obj->field_6;
    obj->field_5 = obj->field_7;
    goto end_switch;

case_4:
    Midi_ResetTrackFlags(obj);
    Midi_KeyOffVoices(obj);
    obj->field_0 = 0;
    goto end_switch;

case_10:
    if (eight != status) {
        goto case_8;
    }
    if ((u32)obj->field_14.field_0 < (u32)obj->field_14.field_4) {
        goto case_8;
    }
    obj->field_C = ffff;
    obj->field_0 = two;
    goto case_2;

end_switch:
    if (obj->field_C != 0) {
        Midi_UpdateVoiceVolumes(obj);
        obj->field_C = 0;
    }
    i   += 1;
    obj += 1;
    if (i <= 0) {
        goto loop;
    }
end:
    return 0;
}

s32 SndEvt_EnqueueType1(s32 arg0, s32 arg1)
{
    SndEvt* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = SndEvt_Alloc();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 1;
    temp->field_4 = arg0;
    temp->field_6 = arg1;
    SndEvt_Enqueue(temp);
    return 0;
}

s32 SndEvt_EnqueueType2(s32 arg0, s32 arg1)
{
    SndEvt* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = SndEvt_Alloc();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 2;
    temp->field_4 = arg0;
    temp->field_6 = arg1 & 0xFFFC;
    SndEvt_Enqueue(temp);
    return 0;
}

s32 SndEvt_EnqueueType3(s32 arg0)
{
    SndEvt* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = SndEvt_Alloc();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 3;
    temp->field_4 = arg0;
    SndEvt_Enqueue(temp);
    return 0;
}

s32 SndEvt_EnqueueType4(s32 arg0)
{
    SndEvt* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = SndEvt_Alloc();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 4;
    temp->field_4 = arg0;
    SndEvt_Enqueue(temp);
    return 0;
}

s32 SndEvt_EnqueueType5(s32 arg0, s32 arg1)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = SndEvt_Alloc();
    if (temp == NULL) {
        return -2;
    }
    temp->field_4 = arg0;
    temp->field_2 = 5;
    mid           = (SndEvtFrom4*)&temp->field_4;
    if ((s8)arg1 >= 0) {
        mid->field_1 = arg1;
    } else {
        mid->field_1 = 0x7F;
    }
    SndEvt_Enqueue(temp);
    D_800820E8 = mid->field_1;
    return 0;
}

s32 Midi_IsBusy(s32 arg0)
{
    s32 i;

    arg0 &= 0xFF;
    if (arg0 == 0xFF) {
        return 0;
    }
    for (i = 0; i <= 0; i++) {
        if ((arg0 == (&Midi_Song)[i].field_1) || (arg0 == 0)) {
            if ((&Midi_Song)[i].field_0 & 0xCA) {
                return 1;
            }
        }
    }
    return 0;
}

s32 Midi_IsChannelFree(u8 arg0)
{
    s32 i;

    if (*(volatile s16*)&D_800689EC == 1) {
        return 0;
    }
    if (arg0 == 0xFF) {
        return 1;
    }
    for (i = 0; i <= 0; i++) {
        if ((&Midi_Song)[i].field_1 == arg0) {
            return 0;
        }
    }
    return 1;
}

void Midi_StartFadeOut(u8 arg0, u16 arg1)
{
    s32       i;
    MidiSong* ptr;

    for (i = 0; i <= 0; i++) {
        ptr = &Midi_Song + i;
        if ((arg0 == ptr->field_1) || (arg0 == 0)) {
            if (ptr->field_0 == 2) {
                ptr->field_0 = 0x80;
                LinInterp_Setup(&ptr->field_14, D_8007F2F0, 0, arg1);
            } else {
                ptr->field_0 = 4;
            }
        }
    }
}

void Midi_FadeVolume(u8 arg0, s32 arg1)
{
    s32       i;
    MidiSong* ptr;

    for (i = 0; i <= 0; i++) {
        ptr = &Midi_Song + i;
        if ((arg0 == ptr->field_1) || (arg0 == 0)) {
            if (arg1 == 0) {
                if (ptr->field_0 == 8) {
                    ptr->field_0 = 0x10;
                    LinInterp_Setup(&ptr->field_14, 0, D_8007F2F0, 8);
                }
            } else {
                if (ptr->field_0 & 0x12) {
                    ptr->field_0 = 8;
                    LinInterp_Setup(&ptr->field_14, D_8007F2F0, 0, 8);
                }
            }
        }
    }
}

void Midi_SetVolumeScale(u8 arg0, u8 arg1)
{
    s32       i;
    u8*       table;
    MidiSong* arr;
    s32       product;

    i   = 0;
    arr = &Midi_Song;
    for (; i <= 0; i++) {
        if ((arg0 == arr[i].field_1) || (arg0 == 0)) {
            table          = D_800689F0;
            product        = table[arr[i].field_1] * arg1;
            arr[i].field_C = 0xFFFF;
            arr[i].field_8 = product;
        }
    }
}

void Midi_SetMasterVolume(s32 arg0)
{
    s32 i;
    s32 val;
    u8* flag;

    flag = &D_8007F2F0;
    if ((s8)arg0 >= 0) {
        *flag = arg0;
    } else {
        *flag = 0x7F;
    }

    i   = 0;
    val = 0xFFFF;
    for (; i <= 0; i++) {
        (&Midi_Song)[i].field_C = val;
    }
}

s32 Midi_GetMasterVolume(void)
{
    return D_8007F2F0;
}

u8* Midi_GetSlot(s32 arg0)
{
    if (Midi_Song.field_0 != 0) {
        Midi_ResetTrackFlags(&Midi_Song);
        Midi_Song.field_0 = 4;
    }
    return (u8*)&Midi_Song;
}

void* Midi_GetFixedBuffer(s32 arg0, s32 arg1)
{
    return D_8007F8E0;
}

void Midi_ClearVoiceEntry(s32* arg0)
{
    u32  i;
    s32* ptr;

    ptr = arg0;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 3U);
    ((s8*)arg0)[1] = -1;
    ((s8*)arg0)[0] = -1;
}

void SndEvt_EnqueueType5Pending(void)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;

    D_800820E9 = 1;
    temp       = SndEvt_Alloc();
    if (temp != NULL) {
        mid           = (SndEvtFrom4*)&temp->field_4;
        temp->field_2 = 5;
        temp->field_4 = 0;
        mid->field_1  = 0;
        SndEvt_Enqueue(temp);
        D_800820E8 = mid->field_1;
    }
}

void SndEvt_FlushType5Pending(void)
{
    SndEvt*      temp;
    SndEvtFrom4* mid;
    u8           saved;

    if (D_800820E9 != 0) {
        saved      = D_800820E8;
        D_800820E9 = 0;
        temp       = SndEvt_Alloc();
        if (temp != NULL) {
            mid           = (SndEvtFrom4*)&temp->field_4;
            temp->field_2 = 5;
            temp->field_4 = 0;
            if ((s8)saved >= 0) {
                mid->field_1 = saved;
            } else {
                mid->field_1 = 0x7F;
            }
            SndEvt_Enqueue(temp);
            D_800820E8 = mid->field_1;
        }
    }
}

void Midi_InitSlot(s32 arg0)
{
    MidiSong*     obj;
    s32*          p;
    u32           i;
    s32           offset;
    u32           k;
    MidiNoteSlot* slot;
    s32*          q;
    s8            freemark;

    arg0 &= 0xFF;
    obj   = &(&Midi_Song)[arg0];

    p = (s32*)obj;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while (i < 0x177U);

    LinInterp_Setup(&obj->field_14, 0, 0, 0);
    Midi_InitChannelTable((s32*)obj->field_484);

    i        = 0;
    freemark = -1;
    offset   = 0;
    do {
        slot = (MidiNoteSlot*)(offset + (s32)obj);
        slot = ((MidiSong*)slot)->voiceSlots;
        q    = (s32*)slot;
        k    = 0;
        do {
            *q = 0;
            k++;
            q++;
        } while (k < 3U);
        offset += 0xC;
        i++;
        slot->field_1 = freemark;
        slot->field_0 = freemark;
    } while ((s32)i < 0x12);
}

void* Midi_ResolveTrackData(MidiSong* arg0, s32 arg1, u8* arg2)
{
    register s32 idx asm("v1");
    u32          offset;

    idx = arg1 & 0xFF;
    if (idx != 0) {
        idx  = idx - 1;
        arg2 = arg0->entries[idx].field_8[8];
        offset =
            (arg2[-4] << 24) | (arg2[-3] << 16) | (arg2[-2] << 8) | arg2[-1];
        return arg2 + offset + 8;
    }
    offset = (arg2[4] << 24) | (arg2[5] << 16) | (arg2[6] << 8) | arg2[7];
    offset = offset + (u32)arg2;
    return (void*)(offset + 0x10);
}

void Midi_ResetTrackFlags(MidiSong* arg0)
{
    s32 i;

    for (i = 0; i < arg0->field_3; i++) {
        arg0->entries[i].field_5 = 1;
    }
}

void Midi_KeyOffVoices(MidiSong* arg0)
{
    s32           i;
    MidiNoteSlot* slot;
    u8            status;
    SpuVoiceRef   sp10;
    u16           temp;

    i    = 0;
    slot = arg0->voiceSlots;
    do {
        if (slot->field_0 >= 0) {
            status = Spu_GetVoiceStatus(slot->field_0);
            if (status != 0) {
                Spu_GetVoiceRef(slot->field_0, &sp10);
                temp                = sp10.field_4->adsr2;
                temp                = (temp & 0xFFE0) | 5;
                sp10.field_4->adsr2 = temp;
                sp10.field_4->mask |= SPU_VOICE_ADSR_ADSR2;
                if (status != 2) {
                    Spu_KeyOff(slot->field_0);
                }
            }
        }
        i++;
        slot++;
    } while (i < 0x12);
}

void Midi_DriveTrack(MidiSong* arg0, MidiTrack* arg1)
{
    u8                  sp10;
    register MidiTrack* entry asm("s0");
    MidiHandler*        table;
    u32                 temp;
    s32                 quot;
    s32                 ticks;
    s32                 rem_factor;
    u8                  status;
    u32                 hi;
    s32                 status_arg;
    s32                 delta;
    MidiHandler         handler;
    u8*                 cursor;

    entry = arg1;
    temp  = entry->field_38 + (arg0->field_4 + arg0->field_5) * arg0->field_34;
    if (Display_State.field_124 == 1) {
        quot = temp / 6000U;
    } else {
        quot = temp / 3600U;
    }
    if (Display_State.field_124 == 1) {
        rem_factor = (temp / 6000U) * 0x177;
        goto rem_join;
    } else {
        goto rem_else;
    }

early_exit:
    entry->field_0  = 0;
    entry->field_38 = 0;
    arg0->field_0   = 4;
    return;

rem_else:
    rem_factor = (temp / 3600U) * 0xE1;
rem_join: {
    register s32 scaled asm("v0");
    register s32 rem asm("v1");
    scaled          = rem_factor * 0x10;
    rem             = temp - scaled;
    ticks           = quot;
    entry->field_38 = rem;
}
    arg0->field_38 += ticks;
    if (arg0->field_1 == 0x4F) {
        arg0->field_C = 0xFFFF;
    }
    if (ticks < entry->field_34) {
        goto end;
    }
    table  = D_800689C4;
    ticks -= entry->field_34;
loop_outer:
    entry->field_34 = 0;
loop_inner:
    status = *entry->field_2C;
    hi     = status & 0xF0;
    if (status & 0x80) {
        entry->field_3 = 0;
        if (hi != 0xF0) {
            entry->field_2 = status & 0xF;
        }
        status_arg      = status & 0xFF;
        handler         = *(MidiHandler*)(((hi >> 2) + (s32)table) - 0x20);
        entry->field_2C = handler(status_arg, entry->field_2C, arg0, entry);
    } else {
        entry->field_2C =
            (*(MidiHandler*)((u8*)table + 4))((entry->field_3 = 1, entry->field_2 | 0x90),
                                              entry->field_2C - 1, arg0, entry);
    }
    if (entry->field_5 != 0) {
        goto end;
    }
    cursor = entry->field_2C;
    if (cursor == NULL) {
        goto early_exit;
    }
    entry->field_34  = Midi_ReadVlq(cursor, &sp10);
    entry->field_2C += sp10;
    delta            = entry->field_34;
    if (delta == 0) {
        goto loop_inner;
    }
    {
        register s32 d asm("v1");
        s32          less;
        d      = delta;
        less   = ticks < d;
        ticks -= d;
        if (less) {
            ticks += d;
        } else {
            goto loop_outer;
        }
    }
end:
    entry->field_34 -= ticks;
}

void Midi_UpdateVoiceVolumes(MidiSong* arg0)
{
    SpuVoiceRef              sp10;
    s16                      sp18[2];
    register MidiSong*       obj asm("s4");
    LinInterp*               interp;
    s32                      volume;
    s32                      i;
    MidiNoteSlot*            slot;
    register MidiOpcodeSlot* entry asm("t0");
    s32                      product;
    u32                      vol;
    register s32             channel asm("t0");
    register s32             temp asm("v0");
    register s32             scale asm("v1");
    s8                       voice;
    s32                      one;
    register s32             f3 asm("v0");

    obj    = arg0;
    interp = &obj->field_14;
    if (obj->field_1 == 0x4F) {
        if (D_80082120 == 5) {
            volume = func_80179BE4((u16)obj->field_8, D_80082136, interp);
            goto after_volume;
        }
    }
    if (obj->field_1 == 0x5A) {
        temp  = Midi_GetMasterVolume() & 0xFF;
        scale = (D_80068A4A * 3) << 5;
    } else {
        temp  = Midi_GetMasterVolume();
        scale = (u16)obj->field_8;
        temp &= 0xFF;
    }
    volume = LinInterp_Apply(interp, (u32)(temp * scale) / 127U);
after_volume:
    i    = 0;
    slot = obj->voiceSlots;
    one  = 1;
    do {
        voice = slot->field_0;
        if (voice >= 0) {
            channel = (u8)slot->field_1;
            if (obj->field_C & (one << channel)) {
                entry   = &obj->field_484[channel];
                product = entry->field_1 * entry->field_2 * D_80068E78[slot->field_3];
                product = product / 2080641;
                vol     = (u32)(volume * slot->field_4 * product) / 16129U;
                f3      = entry->field_3;
                f3     -= 0x40;
                Spu_ApplyPanVolume(sp18, slot->field_5 + f3, vol);
                Spu_GetVoiceRef(voice, &sp10);
                if ((D_800820E9 == one) && (obj->field_1 != 0x5A)) {
                    sp10.field_4->volume.left  = 0;
                    sp10.field_4->volume.right = 0;
                } else {
                    sp10.field_4->volume.left  = sp18[0];
                    sp10.field_4->volume.right = sp18[1];
                }
                sp10.field_4->volmode.left  = 0;
                sp10.field_4->volmode.right = 0;
                sp10.field_4->mask         |= 0xF;
            }
        }
        i++;
        slot++;
    } while (i < 0x12);
}

INCLUDE_ASM("main/nonmatchings/sndevt", func_800520A8);

u8* func_80052488(s32 arg0, u8* arg1, MidiSong* arg2, MidiTrack* arg3)
{
    u8  channel;
    u8  ctrl;
    s32 value;
    u8  status;

    channel = arg0 & 0xF;
    ctrl    = arg1[1];

    switch (ctrl) {
        case 6:
            status = arg3->field_0;
            if (status != 0x10) {
                if (status != 0x14) {
                    return arg1 + 3;
                }
                if (arg3->field_4 != 0) {
                    return arg1 + 3;
                }
                arg3->field_30 = arg1 + 3;
                if ((s8)arg1[2] >= 0) {
                    arg3->field_4 = arg1[2];
                } else {
                    arg3->field_4 = 0x7F;
                }
                arg3->field_0 = 0;
            } else {
                if (arg3->field_1 != status) {
                    return arg1 + 3;
                }
                F3E48C_SetReverbDepth((s16)(arg1[2] << 8));
                arg3->field_0 = 0;
                arg3->field_1 = 0;
            }
            break;

        case 7:
            arg2->field_484[channel].field_1 = arg1[2];
            arg2->field_C                   |= 1 << channel;
            break;

        case 0xA:
            if (CdVol_GetMixMode() & 0xFF) {
                arg2->field_484[channel].field_3 = arg1[2];
            } else {
                arg2->field_484[channel].field_3 = 0x40;
            }
            arg2->field_C |= 1 << channel;
            break;

        case 0xB:
            arg2->field_484[channel].field_2 = arg1[2];
            arg2->field_C                   |= 1 << channel;
            break;

        case 0x62:
            arg3->field_1 = arg1[2];
            break;

        case 0x63:
            value         = arg1[2];
            arg3->field_0 = value;
            if ((value & 0xFF) == 0x14) {
                break;
            }
            if ((value & 0xFF) != 0x1E) {
                return arg1 + 3;
            }
            if ((arg3->field_4 & 0xFF) < 0x7F) {
                if ((arg3->field_4 & 0xFF) == 0) {
                    arg3->field_4 = 0;
                    break;
                }
                arg3->field_4 = arg3->field_4 - 1;
            }
            return arg3->field_30;

        default:
            return arg1 + 3;
    }

    return arg1 + 3;
}

u8* Midi_HandleMetaSysex(s32 arg0, u8* arg1, MidiSong* arg2, MidiTrack* arg3)
{
    u8  sp0;
    s32 var_a0;
    u8* var_a1;
    u8* var_a2;
    u8* var_t0;
    u8  temp_v1;
    s8  temp_v0;

    var_t0 = arg1;
    switch (*var_t0) {
        case 0xF0:
            temp_v1 = *var_t0;
            var_t0 += 1;
            if (temp_v1 != 0xF7) {
                do {
                } while (*var_t0++ != 0xF7);
            }
            goto f7_body;
        case 0xF5:
            if (arg3->field_7 < 9) {
                arg3->field_6                = 1;
                arg3->field_8[arg3->field_7] = var_t0 + 3;
                arg3->field_7                = (u8)arg3->field_7 + 1;
                var_t0 =
                    var_t0 + ((s16)((var_t0[1] << 8) | var_t0[2]) + 3);
            } else {
                var_t0 = NULL;
            }
            break;
        case 0xF6:
            if (arg3->field_7 < 0) {
                arg3->field_6 = 0;
                var_t0        = NULL;
            } else {
                temp_v0       = (u8)arg3->field_7 - 1;
                arg3->field_7 = temp_v0;
                var_t0        = arg3->field_8[temp_v0];
            }
            break;
        case 0xF7:
            goto f7_body;
        case 0xFF:
            var_t0 += 1;
            temp_v1 = *var_t0;
            if (temp_v1 == 0x2F) {
                goto eot;
            }
            var_a1 = var_t0 + 1;
            if (temp_v1 == 0x51) {
                goto tempo;
            }
            goto vlq;
        eot:
            arg3->field_5 = 1;
        f7_body:
            var_t0 += 1;
            break;
        tempo: {
            u32 tempo_val;
            tempo_val     = var_t0[2] << 16;
            tempo_val    |= var_t0[3] << 8;
            tempo_val    |= var_t0[4];
            var_t0       += 5;
            arg2->field_7 = 0;
            arg2->field_6 = 0x3938700U / tempo_val;
        } break;
        vlq:
            var_a2  = &sp0;
            var_a0  = 0;
            *var_a2 = 0;
            do {
                var_a0 <<= 7;
                var_a0  |= *var_a1 & 0x7F;
                *var_a2  = *var_a2 + 1;
            } while (*var_a1++ & 0x80);
            {
                register s32 n asm("v0");
                n      = sp0;
                n      = n + 1;
                n      = var_a0 + n;
                var_t0 = var_t0 + n;
            }
            break;
        default:
            var_t0 = NULL;
            break;
    }
    return var_t0;
}

s32 Midi_ReadVlq(u8* arg0, u8* arg1)
{
    register s32 result asm("a2");

    result = 0;
    *arg1  = 0;
    do {
        result <<= 7;
        result  |= *arg0 & 0x7F;
        *arg1    = *arg1 + 1;
    } while (*arg0++ & 0x80);
    return result;
}

void Midi_InitChannelTable(s32* arg0)
{
    s32 i;

    if (arg0 != NULL) {
        for (i = 0; i < 0x10; i++) {
            *arg0++ = 0x407F4000;
            *arg0++ = 0;
        }
    }
}

s32 Midi_IncPtr(s32 arg0, s32 arg1)
{
    return arg1 + 1;
}

u8* Midi_KeyOffChannel(s32 arg0, u8* arg1, MidiSong* arg2)
{
    s32 i;
    u8  t;
    u8  param;
    u8* ptr;

    ptr   = arg1;
    t     = arg0 & 0xF;
    param = ptr[1];
    if ((arg0 & 0xF0) == 0x90) {
        ptr += 1;
    }
    if (arg2->field_484[t].field_0 == 0) {
        for (i = 0; i < 0x12; i++) {
            if ((arg2->voiceSlots[i].field_2 == param) &&
                (arg2->voiceSlots[i].field_1 == t)) {
                Spu_KeyOff(arg2->voiceSlots[i].field_0);
            }
        }
    }
    return ptr + 2;
}

u8* Midi_SetProgram(s32 arg0, u8* arg1, MidiOpcodeCtx* arg2)
{
    arg2->field_484[arg0 & 0xF].field_4 = arg1[1];
    return arg1 + 2;
}

u8* Midi_PitchBend(s32 arg0, u8* arg1, MidiSong* arg2)
{
    SpuVoiceRef       sp10;
    register s32      channel asm("s4");
    s32               i;
    s32               offset;
    s16               pitchBend;
    MidiNoteSlot*     slot;
    register SndNote* note asm("a3");
    register s32      scale asm("a1");
    s32               prod;
    s16               pitch;
    SpuVoiceAttr*     attr;
    s32               key;

    channel                          = arg0 & 0xF;
    i                                = 0;
    offset                           = 0x504;
    pitchBend                        = (arg1[1] | (arg1[2] << 7)) - 0x2000;
    arg2->field_484[channel].field_6 = pitchBend;
    do {
        slot = (MidiNoteSlot*)((u8*)arg2 + offset);
        if (slot->field_1 == channel) {
            Spu_GetVoiceRef(slot->field_0, &sp10);
            note = Snd_GetNote(arg2->field_40, slot->field_6, slot->field_7);
            if (pitchBend >= 0) {
                scale = note->field_B << 8;
            } else {
                scale = note->field_A << 8;
            }
            prod  = scale * pitchBend;
            key   = (s8) * (volatile u8*)&slot->field_2 & 0xFFFF;
            pitch = prod / 8191;
            asm volatile("" ::"r"(prod));
            slot->field_8 = pitch;
            attr          = sp10.field_4;
            attr->pitch =
                Spu_CalcVolume(key, pitch, note->field_4, note->field_5);
            attr->mask |= SPU_VOICE_PITCH;
        }
        i      += 1;
        offset += 0xC;
    } while (i < 0x12);
    return arg1 + 3;
}

s32 SndLoad_ProcessSector(s32* arg0)
{
    SndLoadState* state;
    s32*          src;
    u32           i;
    s32*          dst;
    s32           nibble;
    s32           count;
    s32           aligned;
    void*         mem;
    s32           len;
    s32           spuAddr;

    state = &SndLoad_State;
    switch (state->field_2) {
        case 0:
            src = arg0;
            dst = (s32*)&state->field_1C;
            i   = 0;
            do {
                *dst = *src;
                src++;
                i++;
                dst++;
            } while (i < 5U);

            nibble = state->field_20 & 0xF000;
            if ((u32)(nibble - 0x8000) < 0x5001U) {
                D_800689E8     = 1;
                state->field_2 = 7;
                break;
            }
            if (nibble == 0x1000) {
                D_80082128 = 0;
            }
            {
                s32 id;
                id                          = state->field_20;
                *(volatile s32*)&D_800689E4 = id;
                if (SndBank_FreeById(state->field_20, state->field_22) == -1) {
                    state->field_2 = 6;
                    break;
                }
            }
            {
                s32 tmp;
                tmp             = (s32)Snd_AllocBank((SndBankPayload*)&state->field_1C);
                state->field_18 = tmp;
                if (tmp == 0) {
                    state->field_2 = 6;
                    break;
                }
                src = arg0 + 5;
                dst = ((SndBank*)tmp)->field_1C;
            }
            count = (state->field_24 * 5) + state->field_23;
            i     = 0;
            if (count != 0) {
                do {
                    *dst = *src;
                    src++;
                    i++;
                    dst++;
                } while ((s32)i < count);
            }
            ((SndBank*)state->field_18)->field_B  = state->field_23;
            ((SndBank*)state->field_18)->field_C  = state->field_24;
            ((SndBank*)state->field_18)->field_8  = state->field_20;
            ((SndBank*)state->field_18)->field_14 = (void*)state->field_2C;
            state->field_2                        = 1;
            break;

        case 1:
            aligned         = (state->field_2A + 3) & 0xFFFC;
            state->field_C  = aligned;
            mem             = SndLoad_AllocBuffer(state->field_20, state->field_22, aligned);
            state->field_14 = (s32)mem;
            if (mem == 0) {
                state->field_2 = 6;
                Snd_FreeBank((SndBank*)state->field_18);
                state->field_18 = 0;
                break;
            }
            state->field_8 = (s32)mem;
            state->field_2 = 2;
            /* fallthrough */
        case 2:
            len = (u32)state->field_C >> 2;
            if ((u32)state->field_C < (u32)state->field_10) {
                state->field_2 = 3;
            } else {
                len             = (u32)state->field_10 >> 2;
                state->field_C -= state->field_10;
            }
            src = arg0;
            dst = (s32*)state->field_8;
            i   = 0;
            if (len != 0) {
                do {
                    *dst = *src;
                    src++;
                    i++;
                    dst++;
                } while (i < (u32)len);
            }
            state->field_8 += len * 4;
            break;

        case 3: {
            s32 size;
            size                                  = state->field_2C;
            state->field_C                        = size;
            ((SndBank*)state->field_18)->field_18 = SndLoad_LookupMode(
                state->field_22, ((SndBank*)state->field_18)->field_8, size);
            spuAddr = ((SndBank*)state->field_18)->field_18;
        }
            if (spuAddr == 0) {
                D_800689E8     = 4;
                state->field_2 = 6;
                Snd_FreeBank((SndBank*)state->field_18);
                state->field_18 = 0;
                break;
            }
            SpuSetTransferStartAddr(spuAddr + (state->field_26 << 6));
            state->field_2 = 4;
            /* fallthrough */
        case 4: {
            s32 rem;
            s32 step;
            rem  = state->field_C;
            step = state->field_10;
            if ((u32)step >= (u32)rem) {
                len            = rem;
                state->field_2 = 5;
            } else {
                len            = step;
                state->field_C = rem - step;
            }
        }
            if (state->field_3 == 0) {
                if (SpuIsTransferCompleted(0) == 0) {
                    if (state->field_0 != 0x10) {
                        Snd_FreeBank((SndBank*)state->field_18);
                        state->field_18 = 0;
                    }
                    D_800689E8     = 5;
                    state->field_2 = 7;
                    break;
                }
                SpuWritePartly((u8*)arg0, len);
            } else {
                SpuWritePartly((u8*)arg0, len);
                SpuIsTransferCompleted(1);
            }
            break;

        case 5:
            break;

        case 6:
            if ((state->field_1 + 1) >= (s32)state->field_28) {
                D_800689E8 = 6;
                if ((state->field_20 & 0xF000) == 0x5000) {
                    if (D_80082128 == 0) {
                        D_80082124 = 0x63810 - ((state->field_2C + 0x3F) & ~0x3F);
                    } else {
                        D_80082124 = D_80082128 - ((state->field_2C + 0x3F) & ~0x3F);
                    }
                }
                if ((state->field_20 & 0xF000) == 0x1000) {
                    D_80082128 = 0x63810 - ((state->field_2C + 0x3F) & ~0x3F);
                }
                state->field_2 = 5;
            }
            break;
    }

    state->field_1 += 1;
    return state->field_2;
}

s32 SndBank_SetupFromLoad(SndLoadState* arg0)
{
    SndBank*     bank;
    SndBankSlot* obj;
    register u32 index asm("a1");
    register u32 temp asm("v1");
    register s32 slot asm("v0");
    register s32 a asm("a0");
    s32          i;
    SndNote*     entry;
    SndNote*     raw;
    s32          base;
    s32          end;
    s32          neg;
    s32          id;
    s32          mask;

    bank = (SndBank*)arg0->field_18;
    if (D_800689E8 == 0) {
        index = bank->field_8;
        mask  = 0xFFFF;
        asm("" : "+r"(index), "+r"(mask));
        temp = index & 0xFFFF;
        if (temp != mask) {
            goto success;
        }
    }
fail:
    D_800689E4 = 0xFF;
    return -1;

success:
    temp >>= 12;
    slot   = D_800680AC[temp];
    neg    = -1;
    if (slot == neg) {
        goto fail;
    }
    a    = slot;
    temp = index & 0xF000;
    if (temp == 0x4000) {
        a = a + (D_80082122 + neg);
    }
    obj = SndBankSlot_Get((s8)a);
    if (obj == NULL) {
        goto fail;
    }
    id           = bank->field_8;
    obj->field_4 = bank;
    obj->field_8 = id;
    obj->field_0 = (SndBankHdr*)arg0->field_14;
    obj->field_C = (void*)bank->field_18;
    i            = arg0->field_24;
    base         = ((volatile SndBank*)bank)->field_18;
    raw          = ((volatile SndBank*)bank)->field_4;
    i            = i - 1;
    if (i != neg) {
        end   = -1;
        entry = raw;
        do {
            i               -= 1;
            entry->field_10 += base;
            entry++;
        } while (i != end);
    }
    Snd_BuildGroupIndex((SndBank*)obj->field_4);
    D_800689E4     = 0xFF;
    arg0->field_18 = 0;
    arg0->field_14 = 0;
    D_8008212C     = D_80082122;
    D_80082121     = D_80082135;
    return 0;
}

INCLUDE_ASM("main/nonmatchings/sndevt", func_800530DC);

void SndLoad_FromSectorMode8(void* arg0)
{
    SndLoad_Init(8, arg0);
}

void SndLoad_BeginFromBuffer(u8 arg0, void* arg1)
{
    D_800820F3 = arg0;
    D_8008212C = D_80082122;
    D_80082121 = D_80082135;
    SndLoad_Init(0, arg1);
}

void SndLoad_Teardown(void)
{
    SndLoadState* temp;

    D_80082122 = D_8008212C;
    D_80082135 = D_80082121;
    temp       = &SndLoad_State;
    if (temp->field_2 != 6) {
        temp->field_2 = 8;
        F3D458_Free((void*)temp->field_14);
        temp->field_14 = 0;
        Snd_FreeBank((SndBank*)temp->field_18);
        temp->field_18 = 0;
    }
}

s32 SndLoad_FeedSector(void* arg0)
{
    SndLoadState* temp_s1;
    s32           temp_s0;

    if (D_80068A78 != 0) {
        return -1;
    }
    temp_s1 = &SndLoad_State;
    if (temp_s1->field_3 != 0) {
        temp_s1->field_10 = 0x800;
    } else {
        switch (temp_s1->field_2) {
            case 0:
            case 1:
            case 3:
                temp_s1->field_10 = 0x7F0;
                arg0              = (u8*)arg0 + 0x10;
                break;
            case 2:
            case 4:
            case 7:
                temp_s1->field_10 = 0x800;
                break;
            case 5:
                return 5;
            case 8:
                return 0;
        }
    }
    temp_s0 = SndLoad_ProcessSector(arg0);
    if (temp_s0 == 7) {
        return -1;
    }
    if (temp_s0 == 5) {
        func_800530DC(temp_s1);
    }
    return temp_s0;
}

s32 SndLoad_FeedSectorOrError(void* arg0)
{
    s32 temp;

    temp = SndLoad_ProcessSector(arg0);
    if (temp == 7) {
        return -1;
    }
    return temp;
}

s32 SndBank_FinalizeLoad(SndLoadState* arg0)
{
    SndBank*  bank;
    MidiSong* state;
    u16       index;
    s32       i;
    s32*      ptr;
    s32       base;
    s32       temp;
    s32       end;

    bank = (SndBank*)arg0->field_18;
    if (D_800689E8 == 0) {
        index = bank->field_8;
        if (index != 0xFFFF) {
            goto success;
        }
    }
    D_800689E4 = 0xFF;
    return -1;

success:
    index          &= 0xFF;
    state           = (MidiSong*)Midi_GetSlot(index);
    state->field_1  = index;
    state->field_A  = (arg0->field_2A + 3) & 0xFFFC;
    temp            = arg0->field_14;
    state->field_40 = bank;
    state->field_10 = (void*)temp;
    state->field_3C = arg0->field_2C;
    i               = arg0->field_24;
    base            = ((volatile SndBank*)bank)->field_18;
    ptr             = (s32*)((volatile SndBank*)bank)->field_4;
    i               = i - 1;
    if (i != -1) {
        end = -1;
        ptr = (s32*)((u8*)ptr + 0x10);
        do {
            i    -= 1;
            *ptr += base;
            ptr   = (s32*)((u8*)ptr + 0x14);
        } while (i != end);
    }
    Snd_BuildGroupIndex(state->field_40);
    D_800689E4     = 0xFF;
    arg0->field_18 = 0;
    arg0->field_14 = 0;
    return 0;
}

void* SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2)
{
    u16 x;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return Midi_GetFixedBuffer(0, arg2 & 0xFFFF);
    }
    if (D_800680AC[x >> 12] == -1) {
        return 0;
    }
    switch (arg0 & 0xF000) {
        case 0x2000:
            if (arg2 < 0x210U) {
                arg2 = 0x210;
            }
            break;
        case 0xE000:
            if (arg2 < 0x168U) {
                arg2 = 0x168;
            }
            break;
    }
    return F3D458_Malloc(arg2);
}

s32 SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2)
{
    s32 result;

    arg0  &= 3;
    result = 0;
    switch (arg0) {
        case 0:
            result = 0x1010;
            break;
        case 2:
            result = func_80053A20(arg2, arg1 & 0xFFFF);
            break;
    }
    return result;
}

void SndLoad_Init(s32 arg0, void* arg1)
{
    SndLoadState* temp;
    s32           size;

    D_800689E8 = 0;
    temp       = &SndLoad_State;
    if (arg0 == 8) {
        size          = 0x800;
        temp->field_0 = arg0;
    } else {
        size          = 0x7F0;
        temp->field_0 = 0;
    }
    temp->field_10 = size;
    temp->field_2  = 0;
    temp->field_1  = 0;
    temp->field_4  = arg1;
    temp->field_14 = 0;
    temp->field_18 = 0;
    temp->field_8  = 0;
    temp->field_C  = 0;
}

s32 SndBank_FreeById(s16 arg0, s32 arg1)
{
    u16      x;
    u8       slot;
    s32      i;
    SndBank* base;
    SndBank* ptr;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return 0;
    }
    slot = D_800680AC[x >> 12];
    if ((s8)slot == -1) {
        return -1;
    }
    switch ((u32)(arg0 & 0xF000) >> 12) {
        case 4:
            i    = 4;
            base = Snd_Banks;
            ptr  = base + 4;
            do {
                if (ptr->field_8 == x) {
                    return -1;
                }
                i++;
                ptr++;
            } while (i < 7);
            slot = D_80082122 + 4;
            break;
        case 0xF:
            break;
        default:
            if (Snd_Banks[(s8)slot].field_8 == (arg0 & 0xFFFF)) {
                return -1;
            }
            break;
    }
    SndBankSlot_Free((s8)slot);
    return 0;
}
