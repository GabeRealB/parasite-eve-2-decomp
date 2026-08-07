#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/display.h"
#include "main/unknown_syms.h"

s32 func_800567E4(void)
{
    volatile GStruct4*  p;
    volatile GStruct56* audio;
    GStruct76*          setup;
    CdlLOC*             loc;
    s32                 acc;
    register s32        i asm("s2");
    s32                 voice;
    s32                 status;
    s16                 half;
    s32                 two;
    s32                 temp;
    register s32        next asm("v0");
    void*               sector;

    setup = &D_800827C4;
    switch (D_80082798.field_0) {
        case 4:
            D_80082780.field_10 = 0;
            acc                 = 0;
            i                   = 0x16;
            do {
                voice = (s8)i;
                temp  = acc + ((s32 (*)(s32))func_8004E6A4)(voice);
                acc   = temp;
                asm("" : "+r"(temp));
                status = (s8)temp;
                if (status != 0) {
                    func_8004E71C(voice);
                }
                next = i + 1;
                i    = next;
                asm("" : "+r"(next));
            } while ((s8)next < 0x18);
            if (status != 0) {
                break;
            }
            D_80082798.field_0 = 1;
            /* fallthrough */
        case 1:
            p = &D_80082798;
            if (p->field_5 == 1) {
                p->field_2         = 4;
                D_80082798.field_0 = 3;
                break;
            }
            audio = (volatile GStruct56*)&D_800827A0;
            loc   = (CdlLOC*)&audio->field_10;
            CdIntToPos(audio->field_C, loc);
            if (D_8008277C != 0) {
                half = 0;
            } else {
                half = audio->field_2;
            }
            two             = 2;
            setup->field_1B = -1;
            setup->field_1A = -1;
            setup->field_18 = half;
            setup->field_1C = two;
            sector          = &Fs_CdSector;
            setup->field_8  = sector;
            setup->field_4  = audio->field_8;
            setup->field_0  = CdPosToInt(loc);
            setup->field_C  = func_80057D24;
            setup->field_10 = 0;
            setup->field_14 = 0;
            audio->field_1  = 0;
            func_80057FAC(setup);
            D_80082798.field_0 = two;
            break;
        case 2:
            if (D_800827A0.field_1 != 0) {
                p = &D_80082798;
                if (p->field_5 == 1) {
                    p->field_2 = 4;
                }
                D_80082798.field_0 = 3;
                D_800827A0.field_1 = 0;
            }
            break;
        case 0:
        case 3:
            break;
    }
    return 1;
}

/* Alignment pad after func_800567E4's 5-entry jump table so func_80056B28's
 * compiler-generated jtbl lands at 0x800141DC. */
static const s32 s_jtbl_pad_567E4 = 0;

s32 func_800569D4(void)
{
    volatile GStruct4*  p;
    s16                 ret;
    GStruct55*          interp;
    volatile GStruct56* parent;
    volatile GStruct61* voices;

    p   = &D_80082798;
    ret = 3;

    switch (p->field_2) {
        case 1:
            interp     = &D_800827B4;
            p->field_1 = 4;
            func_8004D2EC(interp);
            if (D_800827B4.field_0 == interp->field_4) {
                interp->field_E = 0;
                func_8005B84C(0);
                func_8005842C();
                p->field_2 = 2;
            } else {
                parent = (volatile GStruct56*)interp - 1;
                func_8005B84C((s16)func_8004D298(interp, parent->field_2));
            }
            break;
        case 2:
            voices = (volatile GStruct61*)&D_800827A0;
            func_8004E71C(voices->field_3E);
            func_8004E71C(voices->field_3F);
            p->field_2 = 3;
            /* fallthrough */
        case 3:
            if (func_8005BB4C() == 0) {
                D_80082798.field_2 = 4;
                ret                = 0;
            }
            break;
    }
    return ret;
}

typedef struct {
    /* 0x0 */ u8 pad[2];
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} SectorHdr;

void func_80057C74(s32 arg0);
void func_800572FC(s32 arg0);

s32 func_80056B28(void)
{
    u8                  phase;
    register SectorHdr* hdr asm("a2");
    volatile GStruct44* stream;
    s32                 status;
    register s32        tmp asm("a0");
    s32                 val;
    register s32        ptr asm("v1");
    u8                  idx;
    volatile GStruct18* audio;
    volatile GStruct39* cd;

    phase = D_80082798.field_3;
    hdr   = (SectorHdr*)D_80082750;

    switch (phase) {
        case 5:
        do_setloc:
            D_80082780.field_0 = 0;
            D_80082798.field_3 = 1;
            CdControlF(CdlSetloc, D_800827B0);
            break;
        case 1:
            stream = &D_80082780;
            if (stream->field_0 < 0x259) {
                if (CdSync(1, NULL) == CdlDiskError) {
                    CdFlush();
                    goto do_setloc;
                }
                D_80082798.field_3 = 6;
                    /* fallthrough */
                case 6:
                    D_80082798.field_3 = 3;
                    CdReadyCallback(func_80057C74);
                    D_80082780.field_0 = 0;
                    D_80082770         = 0;
                    D_80082780.field_A = 0;
                    CdControlF(CdlReadN, NULL);
                    break;
            }
            goto timeout;
        case 3:
            if (CdSync(1, NULL) == CdlDiskError) {
                D_80082798.field_3 = 6;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                D_80082798.field_3 = 8;
                D_80082780.field_4 = 0;
            }
            break;
        case 8:
            stream = &D_80082780;
            if ((u8)stream->field_A != 0) {
                stream->field_8 = D_80082798.field_3;
                stream->field_9 = 2;
                goto error;
            }
            if (stream->field_0 < 0x259) {
                if (D_80082770 != 0) {
                    audio          = &D_800827A0;
                    idx            = hdr->field_3;
                    val            = D_80068B18[idx];
                    ptr            = D_80082750;
                    audio->field_8 = val;
                    cd             = &D_80082758;
                    idx            = hdr->field_2;
                    cd->field_C    = (u16*)(ptr + (idx * 4));
                    CdReadyCallback(NULL);
                    D_80082798.field_3 = 9;
                        /* fallthrough */
                    case 9:
                        CdControlF(CdlPause, NULL);
                        D_80082780.field_0 = 0;
                        D_80082798.field_3 = 0xA;
                        /* fallthrough */
                    case 10:
                        stream = &D_80082780;
                        if (stream->field_0 < 0x259) {
                            goto do_cdsync;
                        }
                } else {
                    break;
                }
            }
        timeout:
            stream->field_8 = D_80082798.field_3;
            stream->field_9 = 1;
            goto error;
        do_cdsync:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto set_state_4;
            }
            if (status < 3) {
                break;
            }
            if (status != CdlDiskError) {
                break;
            }
            CdFlush();
        set_state_4:
            D_80082798.field_3 = 4;
            break;
        default:
            break;
    }

    tmp                = D_80082780.field_0;
    tmp                = tmp + 1;
    D_80082780.field_0 = tmp;
    return 5;

error:
    D_80082798.field_3 = 0x80;
    return 0;
}

s32 func_80056E38(void)
{
    volatile GStruct61* voices;
    volatile GStruct44* stream;
    volatile GStruct44* p;
    volatile GStruct56* audio;
    volatile GStruct39* cd;
    s32                 status;
    u8                  mode;
    s32                 ret;

    switch (D_80082798.field_4) {
        case 1:
            voices = (volatile GStruct61*)&D_800827A0;
            func_8004E71C(voices->field_3E);
            func_8004E71C(voices->field_3F);
            if (func_8005BB4C() != 0) {
                break;
            }
        do_setmode:
            D_80082798.field_4 = 2;
            D_80082780.field_0 = 0;
            mode               = CdlModeSpeed | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            break;
        case 2:
            stream = &D_80082780;
            if (stream->field_0 < 0x259) {
                goto case2_sync;
            }
            stream->field_8 = D_80082798.field_4;
            stream->field_9 = 1;
            goto error;
        case2_sync:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto case2_ok;
            }
            if (status < 3) {
                break;
            }
            if (status != CdlDiskError) {
                break;
            }
            CdFlush();
            goto do_setmode;
        case2_ok:
            stream->field_C    = 5;
            D_80082798.field_4 = 3;
            break;
        case 3:
            D_80082780.field_C = D_80082780.field_C - 1;
            if (D_80082780.field_C >= 0) {
                break;
            }
            D_80082798.field_4 = 4;
            break;
        case 4:
        do_setloc:
            D_80082780.field_0 = 0;
            D_80082758.field_8 = ((volatile GStruct56*)&D_800827A0)->field_4;
            audio              = (volatile GStruct56*)&D_800827A0;
            CdIntToPos(audio->field_4, (CdlLOC*)&audio->field_10);
            D_80082798.field_4 = 5;
            CdControlF(CdlSetloc, (u8*)&audio->field_10);
            break;
        case 5:
            p = &D_80082780;
            if (p->field_0 < 0x259) {
                status = CdSync(1, NULL);
                if (status == CdlComplete) {
                    goto case5_ok;
                }
                if (status < 3) {
                    break;
                }
                if (status != CdlDiskError) {
                    break;
                }
                CdFlush();
                goto do_setloc;
            case5_ok:
                D_80082798.field_4 = 6;
                break;
            }
            goto timeout;
        case 6:
            D_80082798.field_4 = 7;
            D_80082758.field_1 = 0;
            D_80082780.field_B = 0;
            D_80082780.field_0 = 0;
            SpuSetTransferStartAddr(D_80082758.field_10);
            CdReadyCallback(func_800572FC);
            CdControlF(CdlReadN, NULL);
            break;
        case 7:
            if (CdSync(1, NULL) == CdlDiskError) {
                D_80082798.field_4 = 6;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                D_80082798.field_4 = 8;
                D_80082780.field_4 = 0;
            }
            break;
        case 8:
            p = &D_80082780;
            if (p->field_B != 0) {
                p->field_8 = D_80082798.field_4;
                p->field_9 = 2;
                goto error;
            }
            if (D_80082780.field_0 < 0x259) {
                if (p->field_4 < 0x259) {
                    cd = &D_80082758;
                    if (cd->field_1 == 0) {
                        goto case8_inc;
                    }
                    if (cd->field_1 == 1) {
                        goto do_pause;
                    }
                    goto error;
                case8_inc:
                    p->field_4 = p->field_4 + 1;
                    break;
                }
                goto timeout;
            }
            goto timeout;
        case 11:
        do_pause:
            CdReadyCallback(NULL);
            D_80082780.field_0 = 0;
            D_80082798.field_4 = 9;
            CdControlF(CdlPause, NULL);
            break;
        case 9:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto case9_ok;
            }
            if (status < 3) {
                goto case9_check;
            }
            if (status != CdlDiskError) {
                goto case9_check2;
            }
            CdFlush();
            goto do_pause;
        case9_ok:
            ret                = 0;
            D_80082798.field_4 = 0xA;
            D_80082798.field_2 = 4;
            return ret;
        case9_check:
            p = &D_80082780;
        case9_check2:
            p = &D_80082780;
            if (p->field_0 < 0x259) {
                break;
            }
        timeout:
            p->field_8 = D_80082798.field_4;
            p->field_9 = 1;
        error:
            CdReadyCallback(NULL);
            CdFlush();
            CdControlF(CdlPause, NULL);
            D_80082798.field_4 = 1;
            goto do_setmode;
        case 10:
        default:
            break;
    }

    D_80082780.field_0 = D_80082780.field_0 + 1;
    return 6;
}

void func_800572FC(s32 arg0)
{
    s32                 arg;
    s32                 pos;
    s32                 ret;
    GStruct34*          state;
    s32                 spuIdx;
    volatile GStruct44* stream;
    volatile GStruct39* cdState;
    FsSector*           sector;
    volatile GStruct18* audio;

    sector = &Fs_CdSector;
    stream = &D_80082780;
    if (stream->field_B != 0) {
        return;
    }
    cdState = &D_80082758;
    if (cdState->field_1 != 0) {
        return;
    }
    arg = arg0 & 0xFF;
    if (arg != 1) {
        stream->field_B = 2;
        return;
    }
    CdGetSector(sector, 3);
    pos = CdPosToInt((CdlLOC*)sector);
    if (cdState->field_8 != pos) {
        cdState->field_1 = 0xFF;
        stream->field_B  = arg;
        return;
    }
    CdGetSector(sector, 0x200);
    audio = &D_800827A0;
    if (audio->field_4 == cdState->field_8) {
        state           = &D_800820F0;
        state->field_0  = 0x10;
        state->field_26 = 0;
        state->field_1  = 0;
        state->field_3  = 0;
        state->field_10 = 0x7C0;
        state->field_2  = 4;
        state->field_C  = sector->words[1];
        state->field_28 = sector->bytes[0];
        spuIdx          = sector->words[2];
        if (spuIdx != 0) {
            SpuSetTransferStartAddr(D_80068B2C[spuIdx]);
        }
        if (func_80052B30(&sector->bytes[0x40]) == 7) {
            stream->field_B = 3;
            return;
        }
        state->field_10 = 0x800;
    } else {
        ret = func_80052B30(sector);
        if (ret == 5) {
            cdState->field_1 = arg;
            CdReadyCallback(0);
        } else if (ret == 7) {
            stream->field_B = 4;
        }
    }
    D_80082758.field_8 += 1;
}

void func_800574BC(void)
{
    u32  i;
    s32* p;

    p = (s32*)&D_80082798;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 2U);

    p = (s32*)&D_800827A0;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 0x11U);

    D_8008277C         = 0;
    D_80082750         = 0;
    D_800827A0.field_8 = 0x51010;
    func_8004E5A0(3, 0x16, 2);
    func_8005B6EC();
    func_800260B0(1);
}

u8 func_80057554(void)
{
    return D_800827A0.field_0;
}

void func_80057564(void)
{
    if ((D_800827A0.field_4 != 0) && (D_800827A0.field_0 != 0)) {
        D_800827A0.field_0 = D_80068B34[D_800827A0.field_0 & 7]();
        func_80059348();
    }
}

s32 func_800575D8(s32 arg0)
{
    volatile GStruct44* p;

    D_800827E4         = 0;
    D_80082754         = 0;
    p                  = &D_80082780;
    p->field_C         = 0;
    p->field_8         = 0;
    p->field_9         = 0;
    D_800827A0.field_4 = arg0;
    D_80082750         = 0;
    return 0;
}

s32 func_80057618(void)
{
    u8                  mode;
    s32                 mem;
    s32                 buf;
    volatile GStruct56* p;

    D_80082798.field_3 = 5;
    p                  = (volatile GStruct56*)&D_800827A0;
    CdIntToPos(p->field_4, (CdlLOC*)&p->field_10);
    buf = D_80082750;
    if (buf != 0) {
        F3D458_Free((void*)buf);
    }
    mem                = (s32)F3D458_Malloc(0x800);
    D_80082750         = mem;
    D_80082778         = mem + 4;
    D_800827A0.field_0 = 5;
    mode               = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &mode, NULL);
    return 0;
}

s32 func_800576BC(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        func_80057A88(D_800827A0.field_4 + func_80057A1C((arg0 - 1) & 0xFF));
    }
    return temp_s0;
}

s32 func_80057704(void)
{
    return func_80057ACC();
}

s32 func_80057724(void)
{
    GStruct52* temp;
    s32        ret;

    if (D_80082798.field_0 != 3) {
        return -1;
    }
    if (D_80082798.field_2 != 0) {
        ret = 1;
    } else {
        temp = D_80082794 + D_80082758.field_2;
        func_80057A1C((temp[1].field_3 - temp->field_3 - 1) & 0xFF);
        func_80057B24(0x20);
        ret = 0;
    }
    return ret;
}

s32 func_800577AC(s32 arg0, s32 arg1)
{
    if (func_8005BB4C() != 0) {
        return -1;
    }
    func_80057824(arg0);
    D_800827A0.field_2 = D_80068A80[arg1 & 0xFF] << 7;
    func_800542D0(0x80000000, 0);
    return func_80057894(arg0);
}

s32 func_80057824(s32 arg0)
{
    volatile GStruct44* p;
    volatile GStruct18* r;
    volatile GStruct4*  q;
    s32                 field4;

    D_800827E4 = 0;
    r          = &D_800827A0;
    field4     = r->field_4;
    D_80082754 = 0;
    if (field4 == 0) {
        r->field_4 = arg0;
    }
    p           = &D_80082780;
    p->field_C  = 0;
    p->field_8  = 0;
    p->field_9  = 0;
    p->field_10 = 1;
    r->field_8  = D_80068B1C;
    q           = &D_80082798;
    q->field_2  = 0;
    q->field_0  = 0;
    q->field_1  = 0;
    q->field_4  = 0;
    q->field_5  = 0;
    return 0;
}

s32 func_80057894(s32 arg0)
{
    if (arg0 != 0) {
        func_80057A88(arg0);
    }
    return arg0;
}

s32 func_800578C4(void)
{
    return func_80057ACC();
}

s32 func_800578E4(s32 arg0)
{
    if (arg0 == 0) {
        return -1;
    }
    func_80057B88(arg0, D_80082124);
    return 0;
}

void func_8005791C(s32 arg0)
{
    D_800827A0.field_4 = arg0;
}

void func_80057930(s8 arg0, s32* arg1)
{
    GStruct48 sp10;
    s32*      dest;
    u32       i;

    func_8004E560(arg0, 0, 0);
    func_8004E5C4(arg0, &sp10);
    dest = (s32*)sp10.field_4;
    i    = 0;
    do {
        *dest = *arg1;
        arg1++;
        i++;
        dest++;
    } while (i < 0x10U);
}

void func_800579A0(s8* arg0, s8* arg1)
{
    *arg0 = func_8004E060(D_80068B28, 3, 0xFFFF);
    *arg1 = func_8004E060(D_80068B28, 3, 0xFFFF);
    F3E48C_DisableVoice(*arg0);
    F3E48C_DisableVoice(*arg1);
}

s32 func_80057A1C(s32 arg0)
{
    u32  temp_v0;
    u16* table;

    temp_v0            = ((u32*)D_80082778)[D_80082754 + (arg0 & 0xFF)];
    D_800827A0.field_2 = (temp_v0 >> 17) & 0x3F80;
    D_80082758.field_0 = temp_v0 >> 31;
    table              = D_80082758.field_C;
    D_80082758.field_4 = table[((temp_v0 >> 14) & 0x3FC) / 2];
    return temp_v0 & 0xFFFF;
}

s32 func_80057A88(s32 arg0)
{
    s32 temp_v0;

    temp_v0 = D_80082798.field_1;
    if ((temp_v0 == 4) || (temp_v0 == 0)) {
        D_800827A0.field_C = arg0;
        D_800827A0.field_0 = 1;
        D_80082798.field_0 = 4;
    }
    return 0;
}

s32 func_80057ACC(void)
{
    volatile GStruct4* p;
    s32                ret;

    p = &D_80082798;
    if (p->field_0 != 3) {
        ret        = -1;
        p->field_1 = 4;
        p->field_2 = 1;
    } else {
        ret                = 0;
        p->field_2         = 0;
        p->field_1         = 1;
        D_800827A0.field_0 = 2;
    }
    return ret;
}

void func_80057B24(s32 arg0)
{
    GStruct55*          p;
    volatile GStruct56* parent;

    p      = &D_800827B4;
    parent = (volatile GStruct56*)p;
    parent = parent - 1;
    func_8004D200(p, (parent->field_2 >> 7) & 0xFF, 0, arg0);
    D_80082798.field_1 = 4;
    D_80082798.field_2 = 1;
    parent->field_0    = 3;
}

void func_80057B88(s32 arg0, s32 arg1)
{
    D_800827A0.field_4  = arg0;
    D_80082758.field_10 = arg1;
    D_80082798.field_4  = 1;
    D_800827A0.field_0  = 6;
}

s32 func_80057BC0(void)
{
    return 0;
}

s32 func_80057BC8(void)
{
    s16 ret;

    ret = 2;
    switch (D_80082798.field_1) {
        case 0:
            break;
        case 1:
        case 2:
            func_8005B78C();
            D_80082798.field_1 = 3;
            break;
        case 3:
            if (func_8005BB4C() == 0) {
                ret                = 3;
                D_80082798.field_1 = 4;
                D_80082798.field_2 = 2;
            }
            break;
        case 4:
            break;
    }
    return ret;
}

void func_80057C74(s32 arg0)
{
    s32                 temp;
    s32                 pos;
    FsSector*           sector;
    volatile GStruct18* state;

    if (D_80082770 != 0) {
        return;
    }

    temp = arg0 & 0xFF;
    if (temp == 1) {
        sector = &Fs_CdSector;
        CdGetSector(sector, 3);
        state = &D_800827A0;
        pos   = CdPosToInt((CdlLOC*)sector);
        if (state->field_4 != pos) {
            D_80082780.field_A = -2;
        }
        CdGetSector((void*)D_80082750, 0x200);
        D_80082770 = temp;
    } else {
        D_80082780.field_A = -1;
    }
}

void func_80057D24(void)
{
    D_800827A0.field_1 = 1;
}

s32 func_80057D3C(GStruct32Entry* arg0)
{
    u8                       saved;
    s32                      field2;
    s32                      next;
    u32                      flags;
    s32                      temp;
    volatile GStruct32Entry* entry;
    volatile GStruct32*      p;

    p                  = &D_800828F0;
    saved              = D_800828F0.field_0;
    D_800828F0.field_0 = 1;

    field2 = (s8)p->field_2;
    next   = (s8)p->field_3;
    next   = next + 1;
    if (next >= 4) {
        next = 0;
    }

    if (next == field2) {
        D_800828F0.field_0 = saved;
        return 0;
    }

    entry              = &D_800828F0.entries[(s8)p->field_3];
    entry->field_8     = arg0->field_8;
    entry->field_4     = arg0->field_4;
    temp               = arg0->field_C;
    flags              = entry->field_0;
    flags              = flags | 1;
    entry->field_C     = temp;
    temp               = ~4;
    flags              = flags & temp;
    flags              = flags & ~8;
    flags              = flags & ~0x1FE0;
    temp               = arg0->field_10;
    flags              = flags | 2;
    entry->field_0     = flags;
    entry->field_10    = temp;
    field2             = (s8)p->field_3;
    p->field_3         = next;
    D_800828F0.field_0 = saved;
    return field2 + 1;
}

void func_80057E1C(void)
{
    volatile GStruct32* p;
    GStruct32Entry*     entry;
    u32                 flags;

    p = &D_800828F0;
    if (p->field_0 == 0 && p->field_3 != p->field_2) {
        entry = (GStruct32Entry*)&D_800828F0.entries[(s8)p->field_2];
        flags = entry->field_0;
        if (flags & 1) {
            if (((s32 (*)(GStruct32Entry*))entry->field_8)(entry) != 0) {
                if (entry->field_C != 0) {
                    ((void (*)(void))entry->field_C)();
                }
                entry->field_0 &= ~1;
                entry->field_0 &= ~4;
                p->field_2      = p->field_2 + 1;
                if ((s8)p->field_2 >= 4) {
                    p->field_2 = 0;
                }
            }
        } else if (((flags >> 2) & 1) && !((flags >> 1) & 1)) {
            if (entry->field_10 != 0) {
                ((void (*)(GStruct32Entry*))entry->field_10)(entry);
            }
            if (!((entry->field_0 >> 3) & 1)) {
                goto advance;
            }
        } else {
        advance:
            entry->field_0    &= ~4;
            D_800828F0.field_2 = D_800828F0.field_2 + 1;
            if ((s8)D_800828F0.field_2 >= 4) {
                D_800828F0.field_2 = 0;
            }
        }
    }
}

void func_80058320(void);

void func_80057FAC(GStruct76* arg0)
{
    GStruct32Entry               entry;
    register volatile GStruct19* p asm("s1");
    s32                          flag;
    volatile GStruct19*          ap;
    volatile GStruct19*          a3;
    GStruct74Entry*              t0;
    GStruct74Entry*              ch1;
    register s32                 sectors asm("a0");
    s16                          pitch;
    u8                           saved;
    s16                          f6;
    s16                          idx;
    u32                          flags;
    GStruct32Entry*              e;
    s32                          one;
    s32                          cflags;
    s16                          vff;
    s16                          v1fc3;
    s16                          v1000;
    s32                          temp;
    u8                           mode;
    register s32                 base asm("v1");

    p           = &D_80082818;
    p->field_50 = arg0->field_1A;
    p->field_51 = arg0->field_1B;
    flag        = ((u8)D_80082818.unknown_0[0] >> 4) & 1;
    if (flag == 1) {
        func_8004E71C((s8)p->field_50);
        func_8004E71C((s8)p->field_51);
        if (arg0->field_14 != 0) {
            ((void (*)(s32))arg0->field_14)(
                (flag << (s8)p->field_50) | (flag << (s8)p->field_51));
        }
    }
    SpuSetIRQ(0);
    SpuSetIRQCallback(NULL);
    SpuSetTransferCallback(NULL);
    CdSyncCallback(NULL);

    ap                 = &D_80082818;
    *(s32*)&D_80082818 = 0;
    if (ap->field_6 != 0) {
        a3    = (volatile GStruct19*)&D_800828F0;
        f6    = ap->field_6;
        saved = D_800828F0.field_0;
        if (f6 != 0) {
            idx   = f6 - 1;
            e     = (GStruct32Entry*)&D_800828F0.entries[idx];
            flags = e->field_0;
            if (flags & 1) {
                e->field_0 = (flags & ~1) | 4;
            }
            D_800828F0.field_0 = saved;
        }
        D_80082818.field_6 = 0;
    }

    a3               = &D_80082818;
    a3->field_C      = (void (*)(s32))arg0->field_10;
    a3->field_10     = (void (*)(s32))arg0->field_14;
    a3->unknown_0[4] = 0;
    a3->field_8      = (void (*)(s32))arg0->field_C;
    a3->field_18     = 0;
    a3->field_28     = arg0->field_0;
    a3->field_2C     = arg0->field_0;
    one              = 1;
    a3->field_30     = arg0->field_0;
    a3->field_34     = 0;
    a3->field_38     = one;
    base             = arg0->field_4;
    sectors          = 0x18;
    {
        s32 ds       = Display_State.field_124;
        a3->field_3C = base;
        if (ds == one) {
            sectors = 0x14;
        }
    }
    a3->field_40   = sectors;
    a3->field_42   = 0x2770;
    t0             = (GStruct74Entry*)(a3 + 1);
    a3->field_48   = (GStruct19Sector*)arg0->field_8;
    a3->field_50   = arg0->field_1A;
    vff            = 0xFF;
    a3->field_51   = arg0->field_1B;
    mode           = arg0->field_1C;
    v1fc3          = 0x1FC3;
    v1000          = 0x1000;
    cflags         = 0x6009F;
    t0->field_4    = cflags;
    t0[1].field_4  = cflags;
    t0->field_C    = 0;
    t0->field_E    = 0;
    t0->field_14   = v1000;
    t0->field_3A   = vff;
    t0->field_3C   = v1fc3;
    t0[1].field_C  = 0;
    t0[1].field_E  = 0;
    t0[1].field_14 = v1000;
    a3->field_52   = mode;
    a3->field_1C   = 0;
    a3->field_20   = 0;
    a3->field_54   = 0;
    t0->field_0    = one << a3->field_50;
    t0->field_1C   = a3->field_3C;
    {
        register s32 addr asm("v0");
        register s32 mask asm("v1");
        addr          = a3->field_3C;
        mask          = a3->field_51;
        addr          = addr + 0x10;
        mask          = one << mask;
        t0->field_20  = addr;
        t0[1].field_0 = mask;
    }
    temp           = a3->field_3C;
    temp           = temp + 0x40;
    temp           = temp + ((s32)((u16)a3->field_42 << 16) >> 15);
    t0[1].field_1C = temp;
    temp           = a3->field_3C;
    {
        s32 shift      = (s32)((u16)a3->field_42 << 16) >> 15;
        t0[1].field_3A = vff;
        t0[1].field_3C = v1fc3;
        temp           = temp + shift;
    }
    {
        u8 f53         = a3->field_53;
        temp           = temp + 0x50;
        t0[1].field_20 = temp;
        if (f53 & 2) {
            ch1          = (GStruct74Entry*)(a3 + 1) + 1;
            pitch        = (arg0->field_18 * 0xB5) >> 8;
            ch1->field_A = pitch;
            ch1->field_8 = pitch;
            t0->field_A  = pitch;
            t0->field_8  = pitch;
        } else {
            u16 pitch_u;
            pitch_u       = *(u16*)&arg0->field_18;
            t0->field_A   = 0;
            t0[1].field_8 = 0;
            t0->field_8   = pitch_u;
            t0[1].field_A = *(u16*)&arg0->field_18;
        }
    }

    {
        register s32 rem_tmp asm("a0");
        register s32 temp_v1 asm("v1");

        rem_tmp            = (s32)&entry;
        entry.field_8      = (s32)func_80059EE0;
        temp_v1            = arg0->field_0;
        entry.field_C      = (s32)func_80058320;
        entry.field_10     = (s32)func_8005BA8C;
        entry.field_4      = temp_v1;
        D_80082818.field_6 = func_80057D3C((GStruct32Entry*)rem_tmp);
    }
    D_80082818.unknown_0[3] = 2;
    D_80068B74              = -1;
}

void func_80058320(void)
{
    GStruct32Entry      entry;
    volatile GStruct19* p;

    D_80082818.field_6 = 0;

    if ((D_80082818.unknown_0[0] >> 2) & 1) {
        if (D_80082818.field_1C == 0) {
            D_80082818.unknown_0[3] = 1;
            D_80082818.unknown_0[1] = D_80082818.unknown_0[1] | 1;
            D_80082818.unknown_0[2] = D_80082818.unknown_0[2] & 0xFD;
            D_80082818.unknown_0[2] = D_80082818.unknown_0[2] & 0xFB;
            if (D_80082818.field_8 != NULL) {
                D_80082818.field_8(1);
            }
            return;
        }
    }

    p               = &D_80082818;
    entry.field_8   = (s32)func_80059EE0;
    entry.field_C   = (s32)func_80058320;
    p->unknown_0[2] = p->unknown_0[2] & 0xFD;
    entry.field_10  = (s32)func_8005BA8C;
    entry.field_4   = p->field_28;
    D_80068B63      = D_80068B63 + 1;
    p->field_6      = func_80057D3C(&entry);
}

void func_8005842C(void)
{
    u8                  saved;
    u8                  temp;
    s16                 arg0;
    s16                 idx;
    u32                 flags;
    GStruct32Entry*     entry;
    volatile GStruct19* p;

    saved              = D_800828F0.field_0;
    D_800828F0.field_0 = 1;

    if (D_80082818.unknown_0[0] & 1) {
        D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 0x40;
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
    } else {
        if (D_80082818.field_6 != 0) {
            arg0 = D_80082818.field_6;
            temp = D_800828F0.field_0;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                entry = (GStruct32Entry*)&D_800828F0.entries[idx];
                flags = entry->field_0;
                if (flags & 1) {
                    entry->field_0 = (flags & ~1) | 4;
                }
                D_800828F0.field_0 = temp;
            }
            D_80082818.field_6 = 0;
        }

        p = &D_80082818;
        if ((p->unknown_0[2] >> 3) & 1) {
            func_800B0118(0, 0);
            p->unknown_0[2] = p->unknown_0[2] & 0xF7;
        }
    }

    D_800828F0.field_0 = saved;
}

void func_80058748(void);

void func_8005854C(void)
{
    GStruct32Entry      entry;
    volatile GStruct19* p;
    s32                 flag;
    s16                 arg0;
    s16                 idx;
    u32                 flags;
    GStruct32Entry*     e;
    u8                  saved;
    u8                  t;
    s32                 dead;
    register s32        rem_tmp asm("a0");
    register s32        temp asm("v0");

    p = &D_80082818;
    if (p->unknown_0[2] & 1) {
        dead                    = p->field_20;
        t                       = p->unknown_0[2];
        t                       = t & 0xFE;
        p->unknown_0[2]         = t;
        t                       = D_80082818.unknown_0[0];
        t                       = t & 0xFE;
        D_80082818.unknown_0[0] = t;
        p->unknown_0[2]         = p->unknown_0[2] & 0xFD;
        D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xDF;
        flag                    = (D_80082818.unknown_0[0] >> 4) & 1;
        if (flag == 1) {
            func_8004E71C((s8)p->field_50);
            func_8004E71C((s8)p->field_51);
            D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xEF;
            if (p->field_10 != NULL) {
                p->field_10((flag << (s8)p->field_50) | (flag << (s8)p->field_51));
            }
        }
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        if (D_80082818.field_6 != 0) {
            arg0  = D_80082818.field_6;
            saved = D_800828F0.field_0;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                e     = (GStruct32Entry*)&D_800828F0.entries[idx];
                flags = e->field_0;
                if (flags & 1) {
                    e->field_0 = (flags & ~1) | 4;
                }
                D_800828F0.field_0 = saved;
            }
            D_80082818.field_6 = 0;
        }
        rem_tmp = (s32)&entry;
        asm("" : "+r"(rem_tmp));
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        entry.field_8           = temp;
        temp                    = (s32)func_80058748;
        p                       = &D_80082818;
        entry.field_C           = temp;
        entry.field_4           = p->field_30;
        D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xFB;
        entry.field_10          = (s32)func_8005BA8C;
        p->field_6              = func_80057D3C((GStruct32Entry*)rem_tmp);
        p->unknown_0[3]         = 2;
    }
}

void func_80058748(void)
{
    GStruct32Entry      entry;
    volatile GStruct19* p;
    register s32        rem_tmp asm("a0");
    register s32        field18 asm("a1");
    register s32        temp asm("v0");
    register s32        rem asm("v1");
    register s32        quot asm("a3");

    p          = &D_80082818;
    field18    = p->field_18;
    temp       = field18 / p->field_40;
    p->field_6 = 0;
    quot       = temp + 1;

    if ((D_80082818.unknown_0[0] >> 2) & 1) {
        p->unknown_0[3] = 1;
        p->unknown_0[1] = p->unknown_0[1] | 1;
        p->unknown_0[2] = p->unknown_0[2] & 0xFD;
        if (p->field_1C & 1) {
            p->unknown_0[4] = 0;
        } else {
            rem_tmp                 = (u16)p->field_40;
            rem                     = field18 % p->field_40;
            rem_tmp                 = rem_tmp - rem;
            rem_tmp                 = rem_tmp + 1;
            p->unknown_0[4]         = rem_tmp;
            D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 0x20;
        }
        func_800588D8();
    } else {
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        rem             = p->unknown_0[2];
        entry.field_8   = temp;
        temp            = (s32)func_80058748;
        rem             = rem & 0xFD;
        p->unknown_0[2] = rem;
        entry.field_C   = temp;
        D_80068B63      = D_80068B63 + 1;
        temp            = (s32)func_8005BA8C;
        rem             = p->field_30;
        entry.field_10  = temp;
        p->field_20     = quot;
        rem_tmp         = (s32)&entry;
        entry.field_4   = rem;
        p->field_6      = func_80057D3C((GStruct32Entry*)rem_tmp);
    }
}

void func_800588D8(void)
{
    volatile GStruct19* p;
    u8                  temp;

    if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xF7;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xBF;
    func_800B0118(0, 0);
    p                       = &D_80082818;
    temp                    = p->unknown_0[2];
    p->unknown_0[2]         = temp & 0xF7;
    D_80082808              = 0;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 1;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005896C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058ED4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80059348);
