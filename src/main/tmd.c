#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/tmd.h"

#include "psyq/inline_c.h"

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

// Other model stream handlers (same ABI as TmdModelStreamHandler; not yet in hasm).
u32* D_80099994(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80099B94(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80099D40(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80099FF4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009A348(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009A57C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009AF90(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009B2F4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009B500(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009BD00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009C024(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009C414(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009CED0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009D0DC(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009D388(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009D518(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009D718(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009D900(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009DB00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009DCB8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009DE48(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009E048(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009E274(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009E4A0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009E770(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EAA4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EB84(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EC1C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009ECC0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009ED28(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009ED90(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EE28(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EECC(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EF64(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009EFFC(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F0A0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F144(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F1DC(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F280(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F360(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F3F8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F49C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F504(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F56C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F670(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F708(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F824(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F8C8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009F970(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009FA24(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009FB28(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009FC44(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8009FC90(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80136224(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80136500(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8013685C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80136C00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_8013700C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80137300(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_801375F8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_801379B4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_80138004(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* D_801386EC(TmdScratchModelBlock* ws, s32 flags, u32* stream);

void Tmd_InitSourceStream(TmdSource* arg0)
{
    u32*                  stream;
    u32                   id;
    u32                   dims;
    TmdModelStreamHandler handler;
    s32                   flag;
    register u32          tmp asm("v0");

    stream = arg0->field_20;
    if (arg0->field_0 == 0) {
        tmp  = *(u32*)&Game_Session->field_4;
        tmp  = (tmp & 0xFFFF0000) ^ 0x02100000;
        flag = tmp < 1;
        goto read_id;

        for (;;) {
            switch (id) {
                case 0x20:
                case 0x22:
                    handler = Tmd_StreamHandler_Op20;
                    break;
                case 0x60:
                case 0x62:
                    handler = Tmd_StreamHandler_Op60;
                    break;
                case 0xC0:
                    handler = Tmd_StreamHandler_OpC0;
                    break;
                case 0xC4:
                    handler = D_8009EAA4;
                    break;
                case 5:
                    handler = D_80099B94;
                    break;
                case 0x45:
                    handler = D_80099994;
                    break;
                case 0x21:
                case 0x121:
                    handler = Tmd_StreamHandler_Prim30;
                    break;
                case 0x61:
                case 0x161:
                    handler = Tmd_StreamHandler_Prim38;
                    break;
                case 0x38:
                    handler = Tmd_StreamHandler_Op38;
                    break;
                case 0x8038:
                    handler = D_80136224;
                    break;
                case 0x10038:
                    handler = D_8013700C;
                    break;
                case 0x20038:
                    handler = D_801379B4;
                    break;
                case 0x3A:
                    handler = Tmd_StreamHandler_Op3A;
                    break;
                case 0x1003A:
                    handler = D_80137300;
                    break;
                case 0x78:
                    handler = Tmd_StreamHandler_Op78;
                    break;
                case 0x8078:
                    handler = D_8013685C;
                    break;
                case 0x10078:
                    handler = D_801375F8;
                    break;
                case 0x20078:
                    handler = D_80138004;
                    break;
                case 0x7A:
                    handler = Tmd_StreamHandler_Op7A;
                    break;
                case 0xC8:
                    handler = Tmd_StreamHandler_OpC8;
                    break;
                case 0x40C8:
                    handler = D_8009AF90;
                    if (flag != 0) {
                        handler = D_8009B2F4;
                    }
                    break;
                case 0x200C8:
                    handler = D_801386EC;
                    break;
                case 0x31:
                case 0x39:
                case 0x131:
                    handler = Tmd_StreamHandler_Op39;
                    break;
                case 0x8039:
                    handler = D_80136500;
                    break;
                case 0x3B:
                    handler = Tmd_StreamHandler_Op3B;
                    break;
                case 0x71:
                case 0x79:
                case 0x171:
                    handler = Tmd_StreamHandler_Op79;
                    break;
                case 0x8079:
                    handler = D_80136C00;
                    break;
                case 0x7B:
                    handler = Tmd_StreamHandler_Op7B;
                    break;
                case 0x4039:
                    handler = D_80099D40;
                    if (flag != 0) {
                        handler = D_8009A348;
                    }
                    break;
                case 0x4079:
                    handler = D_80099FF4;
                    if (flag != 0) {
                        handler = D_8009A57C;
                    }
                    break;
                case 0:
                    handler = Tmd_StreamHandler_Op00;
                    break;
                case 0x40:
                    handler = Tmd_StreamHandler_Op40;
                    break;
                case 0x18:
                    handler = Tmd_StreamHandler_Op18;
                    break;
                case 0x1A:
                    handler = Tmd_StreamHandler_Op1A;
                    break;
                case 0x58:
                    handler = Tmd_StreamHandler_Op58;
                    break;
                case 0x5A:
                    handler = Tmd_StreamHandler_Op5A;
                    break;
                case 0x4078:
                    handler = D_8009C414;
                    if (flag != 0) {
                        handler = D_8009C024;
                    }
                    break;
                case 0x4038:
                    handler = D_8009B500;
                    if (flag != 0) {
                        handler = D_8009BD00;
                    }
                    break;
                case 0x120:
                    handler = D_8009E048;
                    break;
                case 0x122:
                    handler = D_8009E274;
                    break;
                case 0x160:
                    handler = D_8009E4A0;
                    break;
                case 0x162:
                    handler = D_8009E770;
                    break;
                case 0x1C:
                    handler = D_8009D388;
                    break;
                case 0x1E:
                    handler = D_8009DCB8;
                    break;
                case 0x5C:
                    handler = D_8009D518;
                    break;
                case 0x5E:
                    handler = D_8009DE48;
                    break;
                case 0x30:
                    handler = D_8009CED0;
                    break;
                case 0x130:
                    handler = Tmd_StreamHandler_Op130;
                    break;
                case 0x70:
                    handler = D_8009D0DC;
                    break;
                case 0x170:
                    handler = Tmd_StreamHandler_Op170;
                    break;
                case 0x156:
                    handler = D_8009D718;
                    break;
                case 4:
                    handler = D_8009DB00;
                    break;
                case 0x44:
                    handler = D_8009D900;
                    break;
                default:
                    handler = Tmd_StreamHandler_Default;
                    break;
            }

            stream++;
            *stream = (u32)handler;
            stream++;
            dims = *stream;
            stream++;
            id      = dims & 0xFFFF;
            stream += (dims >> 16) * id;
            id      = *stream;

            while (1) {
                if (id != -2U) {
                    break;
                }
                stream++;
            read_id:
                id = *stream;
                if (id == -1U) {
                    goto done;
                }
            }
        }
    done:
        arg0->field_0 = 1;
    }
}

void Tmd_ProcessStream(TmdObject* arg0)
{
    TmdScratchModelBlock* ws;
    TmdSource*            src;
    u32*                  stream;
    u32                   id;
    TmdModelStreamHandler handler;
    s32                   flag;
    void*                 buf;
    u32                   hi;

    flag = 0;
    {
        register void**                scratch asm("a0");
        register TmdScratchModelBlock* head asm("v1");
        void*                          tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        src      = arg0->field_10;
        tmp      = *scratch;
        stream   = src->field_20;
        hi       = *(u32*)&Game_Session->field_4;
        head     = (TmdScratchModelBlock*)((u8*)tmp - 0x88);
        hi      &= 0xFFFF0000;
        *scratch = head;
        if ((hi == 0x020F0000) || (hi == 0x02100000)) {
            flag = 1;
        }
        ws = head;
    }

    ws->field_80 = arg0;
    buf          = arg0->field_18;
    ws->field_0  = buf;
    if (arg0->field_14 != 0) {
        ws->field_0 = (u8*)buf + arg0->field_16;
    }
    ws->field_4     = ws->field_0;
    ws->field_0     = (u8*)ws->field_0 + src->field_8;
    arg0->field_14 ^= 1;
    ws->field_8     = arg0->field_10->field_14;
    ws->field_C     = arg0->field_10->field_18;
    ws->field_70    = (s8)arg0->field_24;
    ws->field_72    = (s8)arg0->field_25 << 6;
    goto read_id;

    for (;;) {
        switch (id) {
            case 0x4038:
                handler = D_8009F670;
                if (flag != 0) {
                    handler = D_8009F56C;
                }
                break;
            case 0x38:
            case 0x3A:
            case 0x8038:
            case 0x10038:
            case 0x1003A:
            case 0x20038:
                handler = D_8009ED90;
                break;
            case 0x4078:
                handler = D_8009F824;
                if (flag != 0) {
                    handler = D_8009F708;
                }
                break;
            case 0x78:
            case 0x7A:
            case 0x8078:
            case 0x10078:
            case 0x20078:
                handler = D_8009EE28;
                break;
            case 0x31:
            case 0x39:
            case 0x3B:
            case 0x131:
            case 0x8039:
                handler = D_8009EB84;
                break;
            case 0x71:
            case 0x79:
            case 0x7B:
            case 0x171:
            case 0x8079:
                handler = D_8009EC1C;
                break;
            case 0x4039:
                handler = D_8009F8C8;
                if (flag != 0) {
                    handler = D_8009FA24;
                }
                break;
            case 0x4079:
                handler = D_8009F970;
                if (flag != 0) {
                    handler = D_8009FB28;
                }
                break;
            case 0x18:
            case 0x1A:
                handler = D_8009F144;
                break;
            case 0x58:
            case 0x5A:
                handler = D_8009F1DC;
                break;
            case 0x1C:
            case 0x1E:
                handler = D_8009F360;
                break;
            case 0x5C:
            case 0x5E:
                handler = D_8009F3F8;
                break;
            case 0x30:
                handler = D_8009EECC;
                break;
            case 0x130:
                handler = D_8009EF64;
                break;
            case 0x70:
                handler = D_8009EFFC;
                break;
            case 0x170:
                handler = D_8009F0A0;
                break;
            case 0x156:
                handler = D_8009F280;
                break;
            case 4:
                handler = D_8009F504;
                break;
            case 0x44:
                handler = D_8009F49C;
                break;
            case 5:
                handler = D_8009ED28;
                break;
            case 0x45:
                handler = D_8009ECC0;
                break;
            case 0x40:
            case 0x60:
            case 0x160:
            case 0x4040:
            case 0x4060:
            case 0x4160:
                handler = D_8009FC44;
                break;
            default:
                handler = Tmd_StreamHandler_Default;
                break;
            case 0:
            case 0x20:
            case 0x120:
            case 0x4000:
            case 0x4020:
            case 0x4120:
                handler = D_8009FC90;
                break;
        }

        ws->field_20 = *stream;
        stream      += 2;
        ws->field_18 = ((u16*)stream)[0];
        ws->field_1C = ((u16*)stream)[1];
        stream       = (u32*)((u8*)stream + 4);
        stream       = handler(ws, 0, stream);
        id           = *stream;

        while (1) {
            if (id != -2U) {
                break;
            }
            stream++;
        read_id:
            id = *stream;
            if (id == -1U) {
                goto done;
            }
        }
    }
done:
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x88;
}

INCLUDE_ASM("main/nonmatchings/tmd", Tmd_Create);

void Tmd_SetupDraw(TmdObject* arg0)
{
    u8                            buf[0x1000];
    void**                        scratch;
    register void*                tmp asm("t1");
    register TmdScratchDrawBlock* ws asm("t0");
    register void*                stream asm("a2");
    register MATRIX*              colorMtx asm("t2");
    register short                t4 asm("t4");
    register short                t5 asm("t5");
    register short                t6 asm("t6");
    register u32                  flags asm("a1");
    void*                         bufptr;
    s32                           disp;

    scratch = (void**)G_SCRATCH_HEAD;
    {
        register u32        ds_hi asm("v1");
        register TmdSource* p asm("v0");
        register s32        d asm("v0");

        __asm__("lui %0, %%hi(Display_State)" : "=r"(ds_hi));
        p      = arg0->field_10;
        tmp    = *scratch;
        stream = p->field_20;
        __asm__ volatile("lbu %0, %%lo(Display_State+0x128)(%1)" : "=r"(d) : "r"(ds_hi));
        disp         = d;
        ws           = (TmdScratchDrawBlock*)((u8*)tmp - 0x98);
        ws->field_80 = arg0;
        ws->field_84 = disp;
    }
    bufptr      = arg0->field_18;
    ws->field_0 = bufptr;
    *scratch    = ws;
    if (arg0->field_14 != 0) {
        ws->field_0 = (u8*)bufptr + arg0->field_16;
    }
    ws->field_4     = ws->field_0;
    ws->field_0     = (u8*)ws->field_0 + arg0->field_10->field_8;
    arg0->field_14 ^= 1;
    ws->field_8     = arg0->field_10->field_14;
    {
        register u_long*    ot asm("a0");
        register TmdSource* p asm("v1");
        register s32        e asm("v0");
        register void*      b asm("v1");
        s32                 field18;

        COMPILER_BARRIER();
        ot           = Gpu_CurrentOt;
        p            = arg0->field_10;
        field18      = p->field_18;
        ws->field_14 = ot;
        ws->field_C  = field18;
        e            = arg0->field_E;
        b            = buf;
        ws->field_10 = b;
        ws->field_14 = ot + e;
    }

    colorMtx = (MATRIX*)arg0->field_20;
    gte_SetColorMatrix(colorMtx);
    {
        register MATRIX* m asm("v0");
        register s32     r asm("t3");
        register s32     g asm("t7");
        register s32     b asm("t2");
        m = colorMtx;
        r = m->t[0];
        g = m->t[1];
        b = m->t[2];
        gte_ldbkdir(r, g, b);
    }

    {
        register MATRIX* m asm("v0");
        register MATRIX* src asm("t3");
        register MATRIX* light asm("t7");

        m = (MATRIX*)((u8*)tmp - 0x48);
        TOUCH_REG(m);
        src = &Gfx_ViewWorldMtx;
        TOUCH_REG(src);
        flags = arg0->field_C;
        TOUCH_REG(flags);

        t4         = src->m[0][0];
        t5         = src->m[1][0];
        t6         = src->m[2][0];
        m->m[0][0] = t4;
        m->m[0][1] = t5;
        m->m[0][2] = t6;

        t4         = src->m[0][1];
        t5         = src->m[1][1];
        t6         = src->m[2][1];
        m->m[1][0] = t4;
        m->m[1][1] = t5;
        m->m[1][2] = t6;

        t4         = src->m[0][2];
        t5         = src->m[1][2];
        t6         = src->m[2][2];
        m->m[2][0] = t4;
        m->m[2][1] = t5;
        m->m[2][2] = t6;

        light = (MATRIX*)arg0->field_1C;
        gte_SetRotMatrix(light);

        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);

        m = (MATRIX*)((u8*)tmp - 0x46);
        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);

        m = (MATRIX*)((u8*)tmp - 0x44);
        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);
    }

    Tmd_SetupGteMatrices(ws, flags, stream, arg0);

    *scratch = (u8*)*scratch + 0x98;
}

void Tmd_FreeBuffers(TmdObject* arg0)
{
    if (arg0->field_18 != NULL) {
        Mem_Free2(arg0->field_18, 1);
        arg0->field_18 = NULL;
    }
}

s32 Tmd_AllocBuffers(TmdObject* arg0)
{
    s32   result;
    void* mem;

    result = 0;
    if (arg0->field_18 == NULL) {
        mem            = Mem_Calloc(arg0->field_10->field_4 * 2, 1);
        arg0->field_18 = mem;
        if (mem != NULL) {
            arg0->field_14 = 0;
            Tmd_ProcessStream(arg0);
            Tmd_ProcessStream(arg0);
            result = 1;
        }
    }
    return result;
}

s32 Tmd_SumBufferBytes(void)
{
    TmdObject* node;
    s32        result;

    result = 0;
    node   = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            result += node->field_10->field_4 * 2;
        }
        node = node->next;
    }
    return result;
}

void Tmd_RewriteOpcodes(TmdSource* arg0)
{
    u32* stream;
    u32  id;
    u32  dims;
    u32  lo;
    u32  stop;

    stream = arg0->field_20;
    if (*stream != -1U) {
        stop = -2;
        do {
            if (*stream != stop) {
                do {
                    id = *stream;
                    if (id == 0x3B) {
                        goto case_advance;
                    }
                    if (id < 0x3CU) {
                        if (id == 0x38) {
                            goto case_38;
                        }
                        goto default_advance;
                    }
                    if (id == 0x79) {
                        goto case_advance;
                    }
                    if (id >= 0x7AU) {
                        goto case_advance;
                    }
                    if (id == 0x78) {
                        goto case_78;
                    }
                    goto default_advance;

                case_38:
                    *stream = 0x4038;
                    goto case_advance;
                case_78:
                    *stream = 0x4078;
                case_advance:
                    stream += 2;
                    goto after;
                default_advance:
                    stream += 2;
                after:
                    dims = *stream;
                    lo   = dims & 0xFFFF;
                    stream++;
                    stream += (dims >> 16) * lo;
                } while (*stream != -2U);
            }
            stream++;
        } while (*stream != -1U);
    }
}

void Tmd_FlagAllNodes(TmdObject* arg0)
{
    TmdObject* node;

    node = Tmd_List.next;
    while (node != NULL) {
        node->field_C |= 0x80;
        node           = node->next;
    }
    arg0->field_30++;
}

void Tmd_FreeNodeBuffers(TmdObject* arg0)
{
    TmdObject* node;

    node = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            Mem_Free2(node->field_18, 1);
            node->field_18 = NULL;
        }
        node = node->next;
    }
    arg0->field_30++;
}

void Tmd_DispatchTask(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80013EDC;
    sp.funcs[arg0->state](arg0);
}

void Gpu_ResetGraphAndOt(void)
{
    TmdObject* node;

    node = Tmd_List.next;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    while (node != NULL) {
        if (node->field_18 != NULL) {
            node->field_18 = NULL;
        }
        node = node->next;
    }
}

void Tmd_AllocMissingBuffers(void)
{
    TmdObject* node;
    void*      mem;

    node = Tmd_List.next;
    Mem_InitAux();
    CdCmd_SetupMdecBuffers();
    while (node != NULL) {
        if (node->field_18 == NULL) {
            if (!(node->field_C & 4)) {
                mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
                if (mem != NULL) {
                    node->field_18 = mem;
                    node->field_14 = 0;
                    Tmd_ProcessStream(node);
                    Tmd_ProcessStream(node);
                }
            }
        }
        node = node->next;
    }
}

void Tmd_AllocNodeBuffers(Task* arg0)
{
    TmdObject* node;
    void*      mem;

    node = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 == NULL) {
            mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
            if (mem != NULL) {
                node->field_18 = mem;
                node->field_14 = 0;
                node->field_C &= ~0x80;
                Tmd_ProcessStream(node);
                Tmd_ProcessStream(node);
            }
        }
        node = node->next;
    }
    Task_Kill(arg0);
}

void Tmd_DrawFlaggedNodes(TmdObject* node)
{
    while (node != NULL) {
        if (node->field_C & 8) {
            if (node->field_18 != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = node->next;
    }
}

void Tmd_DrawActiveNodes(TmdObject* node)
{
    while (node != NULL) {
        if (!(node->field_C & 0x80)) {
            if (node->field_18 != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = node->next;
    }
}
