#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/tmd.h"

#include "psyq/inline_c.h"

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// A model-path stream command's handler: the function a command in a model's
/// stream is resolved to, and the signature every handler of that stream shares.
///
/// A stream ships an opcode per command, and resolving a model's stream writes
/// the address of the function that runs each command into the stream beside its
/// opcode. A handler is given the walk's scratch frame, its flags and the cursor
/// the command's data starts at, and returns the cursor the walk resumes from.
typedef u32* (*_TmdModelStreamHandler)(TmdScratchModelBlock* ws, s32 flags, u32* stream);

// Other model stream handlers (same ABI as _TmdModelStreamHandler; not yet in hasm).
u32* func_80099994(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_80099B94(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_80099D40(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_80099FF4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009A348(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009A57C(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009AF90(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009B2F4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009B500(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009BD00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009C024(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009C414(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009CED0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009D0DC(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009D388(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009D518(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009D718(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009D900(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009DB00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009DCB8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009DE48(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009E048(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009E274(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009E4A0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009E770(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* func_8009EAA4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
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

void Tmd_InitSourceStream(TmdSource* src)
{
    u32*                   stream;
    u32                    id;
    u32                    dims;
    _TmdModelStreamHandler handler;
    s32                    flag;
    u32                    tmp;

    stream = src->stream;
    if (src->handlersResolved == 0) {
        tmp  = *(u32*)&gGameSession->at4.loc;
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
                    handler = func_8009EAA4;
                    break;
                case 5:
                    handler = func_80099B94;
                    break;
                case 0x45:
                    handler = func_80099994;
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
                    handler = func_8009AF90;
                    if (flag != 0) {
                        handler = func_8009B2F4;
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
                    handler = func_80099D40;
                    if (flag != 0) {
                        handler = func_8009A348;
                    }
                    break;
                case 0x4079:
                    handler = func_80099FF4;
                    if (flag != 0) {
                        handler = func_8009A57C;
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
                    handler = func_8009C414;
                    if (flag != 0) {
                        handler = func_8009C024;
                    }
                    break;
                case 0x4038:
                    handler = func_8009B500;
                    if (flag != 0) {
                        handler = func_8009BD00;
                    }
                    break;
                case 0x120:
                    handler = func_8009E048;
                    break;
                case 0x122:
                    handler = func_8009E274;
                    break;
                case 0x160:
                    handler = func_8009E4A0;
                    break;
                case 0x162:
                    handler = func_8009E770;
                    break;
                case 0x1C:
                    handler = func_8009D388;
                    break;
                case 0x1E:
                    handler = func_8009DCB8;
                    break;
                case 0x5C:
                    handler = func_8009D518;
                    break;
                case 0x5E:
                    handler = func_8009DE48;
                    break;
                case 0x30:
                    handler = func_8009CED0;
                    break;
                case 0x130:
                    handler = Tmd_StreamHandler_Op130;
                    break;
                case 0x70:
                    handler = func_8009D0DC;
                    break;
                case 0x170:
                    handler = Tmd_StreamHandler_Op170;
                    break;
                case 0x156:
                    handler = func_8009D718;
                    break;
                case 4:
                    handler = func_8009DB00;
                    break;
                case 0x44:
                    handler = func_8009D900;
                    break;
                default:
                    handler = tmdSkipStreamRecord;
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
        src->handlersResolved = 1;
    }
}

void tmdProcessStream(TmdObject* obj)
{
    TmdScratchModelBlock*  ws;
    TmdSource*             src;
    u32*                   stream;
    u32                    id;
    _TmdModelStreamHandler handler;
    s32                    flag;
    void*                  buf;
    u32                    hi;
    void**                 scratch;
    TmdScratchModelBlock*  head;
    void*                  tmp;

    flag     = 0;
    scratch  = (void**)G_SCRATCH_HEAD;
    src      = obj->source;
    tmp      = *scratch;
    stream   = src->stream;
    hi       = *(u32*)&gGameSession->at4.loc;
    head     = (TmdScratchModelBlock*)((u8*)tmp - 0x88);
    hi      &= 0xFFFF0000;
    *scratch = head;
    if ((hi == 0x020F0000) || (hi == 0x02100000)) {
        flag = 1;
    }
    ws = head;

    ws->obj       = obj;
    buf           = obj->buffer;
    ws->primWrite = buf;
    if (obj->bufferIndex != 0) {
        ws->primWrite = (u8*)buf + obj->halfSize;
    }
    ws->preXformWrite = ws->primWrite;
    ws->primWrite     = (u8*)ws->primWrite + src->firstRegionSize;
    obj->bufferIndex ^= 1;
    ws->verts         = obj->source->verts;
    ws->normals       = obj->source->normals;
    ws->tpage         = (s8)obj->tpage;
    ws->clut          = (s8)obj->clut << 6;
    goto read_id;

    for (;;) {
        switch (id) {
            case 0x4038:
                handler = gpStreamPrimGt3Base;
                if (flag != 0) {
                    handler = gpStreamPrimGt3OffsetLayer;
                }
                break;
            case 0x38:
            case 0x3A:
            case 0x8038:
            case 0x10038:
            case 0x1003A:
            case 0x20038:
                handler = gpStreamPrimGt3;
                break;
            case 0x4078:
                handler = gpStreamPrimGt4Base;
                if (flag != 0) {
                    handler = gpStreamPrimGt4OffsetLayer;
                }
                break;
            case 0x78:
            case 0x7A:
            case 0x8078:
            case 0x10078:
            case 0x20078:
                handler = gpStreamPrimGt4;
                break;
            case 0x31:
            case 0x39:
            case 0x3B:
            case 0x131:
            case 0x8039:
                handler = gpStreamPrimGt3PreXform;
                break;
            case 0x71:
            case 0x79:
            case 0x7B:
            case 0x171:
            case 0x8079:
                handler = gpStreamPrimGt4PreXform;
                break;
            case 0x4039:
                handler = gpStreamPrimGt3PreXformFixedLayer;
                if (flag != 0) {
                    handler = gpStreamPrimGt3PreXformOffsetLayer;
                }
                break;
            case 0x4079:
                handler = gpStreamPrimGt4PreXformLayer;
                if (flag != 0) {
                    handler = gpStreamPrimGt4PreXformOffsetLayer;
                }
                break;
            case 0x18:
            case 0x1A:
                handler = gpStreamPrimGt3OneNormal;
                break;
            case 0x58:
            case 0x5A:
                handler = gpStreamPrimGt4OneNormal;
                break;
            case 0x1C:
            case 0x1E:
                handler = gpStreamPrimFt3;
                break;
            case 0x5C:
            case 0x5E:
                handler = gpStreamPrimFt4;
                break;
            case 0x30:
                handler = gpStreamPrimGt3ElemColor;
                break;
            case 0x130:
                handler = gpStreamPrimGt3CornerColors;
                break;
            case 0x70:
                handler = gpStreamPrimGt4ElemColor;
                break;
            case 0x170:
                handler = gpStreamPrimGt4CornerColors;
                break;
            case 0x156:
                handler = gpStreamPrimGt4Unlit;
                break;
            case 4:
                handler = gpStreamPrimF3;
                break;
            case 0x44:
                handler = gpStreamPrimF4;
                break;
            case 5:
                handler = gpStreamPrimF3PreXform;
                break;
            case 0x45:
                handler = gpStreamPrimF4PreXform;
                break;
            case 0x40:
            case 0x60:
            case 0x160:
            case 0x4040:
            case 0x4060:
            case 0x4160:
                handler = gpStreamPrimG4;
                break;
            default:
                handler = tmdSkipStreamRecord;
                break;
            case 0:
            case 0x20:
            case 0x120:
            case 0x4000:
            case 0x4020:
            case 0x4120:
                handler = gpStreamPrimG3;
                break;
        }

        ws->opcode     = *stream;
        stream        += 2;
        ws->elemStride = ((u16*)stream)[0];
        ws->elemCount  = ((u16*)stream)[1];
        stream         = (u32*)((u8*)stream + 4);
        stream         = handler(ws, 0, stream);
        id             = *stream;

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

TmdObject* Tmd_Create(TmdSource* src, s32 flags)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    TmdBone*       bone;
    u32            i;
    void*          mem = NULL;

    Tmd_InitSourceStream(src);
    obj = memCalloc((src->partCount * sizeof(GsCOORDINATE2)) + sizeof(TmdObject), 0);
    if (obj != NULL) {
        obj->flags       = 0x80;
        obj->partCount   = src->partCount;
        obj->coords      = (GsCOORDINATE2*)(obj + 1);
        obj->bufferIndex = 0;
        coord            = obj->coords;
        obj->halfSize    = src->halfSize;
        obj->lightMtx    = &GsLIGHTWSMATRIX;
        obj->colorMtx    = &D_80074080;
        obj->tpage       = 0;
        obj->clut        = 0;
        obj->source      = src;
        bone             = src->skeleton;
        for (i = 0; i < (u32)obj->partCount; i++) {
            coord->coord = bone->local;
            if (bone->parent != i) {
                coord->sub = &obj->coords[bone->parent];
            } else {
                coord->sub = &gGfxViewCoord;
            }
            coord->flg = 0;
            coord++;
            bone++;
        }
        obj->buffer = NULL;
        if (flags == 0) {
            mem = memCalloc(src->halfSize * 2, 1);
            if (mem != NULL) {
                obj->buffer = mem;
                tmdProcessStream(obj);
                tmdProcessStream(obj);
            }
        } else if (flags & 1) {
            obj->flags |= 4;
        }
    }
    return obj;
}

void Tmd_SetupDraw(TmdObject* obj)
{
    u8                   buf[0x1000];
    void**               scratch;
    void*                tmp;
    TmdScratchDrawBlock* ws;
    void*                stream;
    register MATRIX*     colorMtx asm("t2");
    register short       t4 asm("t4");
    register short       t5 asm("t5");
    register short       t6 asm("t6");
    u32                  flags;
    void*                bufptr;
    s32                  disp;
    u_long*              ot;
    TmdSource*           p;
    s32                  e;
    void*                b;
    s32                  field18;

    scratch = (void**)G_SCRATCH_HEAD;
    {
        TmdSource* p;

        p            = obj->source;
        tmp          = *scratch;
        stream       = p->stream;
        disp         = gDisplayState.otDepthShift;
        ws           = (TmdScratchDrawBlock*)((u8*)tmp - 0x98);
        ws->field_80 = obj;
        ws->field_84 = disp;
    }
    bufptr      = obj->buffer;
    ws->field_0 = bufptr;
    *scratch    = ws;
    if (obj->bufferIndex != 0) {
        ws->field_0 = (u8*)bufptr + obj->halfSize;
    }
    ws->field_4       = ws->field_0;
    ws->field_0       = (u8*)ws->field_0 + obj->source->firstRegionSize;
    obj->bufferIndex ^= 1;
    ws->field_8       = (s32)obj->source->verts;
    COMPILER_BARRIER();
    ot           = Gpu_CurrentOt;
    p            = obj->source;
    field18      = (s32)p->normals;
    ws->field_14 = ot;
    ws->field_C  = field18;
    e            = obj->otOffset;
    b            = buf;
    ws->field_10 = b;
    ws->field_14 = ot + e;

    colorMtx = (MATRIX*)obj->colorMtx;
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
        MATRIX*          m;
        register MATRIX* src asm("t3");
        register MATRIX* light asm("t7");

        m = (MATRIX*)((u8*)tmp - 0x48);
        TOUCH_REG(m);
        src = &Gfx_ViewWorldMtx;
        TOUCH_REG(src);
        flags = obj->flags;
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

        light = (MATRIX*)obj->lightMtx;
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

    Tmd_SetupGteMatrices(ws, flags, stream, obj);

    *scratch = (u8*)*scratch + 0x98;
}

void Tmd_FreeBuffers(TmdObject* obj)
{
    if (obj->buffer != NULL) {
        memFreeFromHeap(obj->buffer, 1);
        obj->buffer = NULL;
    }
}

s32 Tmd_AllocBuffers(TmdObject* obj)
{
    s32   result;
    void* mem;

    result = 0;
    if (obj->buffer == NULL) {
        mem         = memCalloc(obj->source->halfSize * 2, 1);
        obj->buffer = mem;
        if (mem != NULL) {
            obj->bufferIndex = 0;
            tmdProcessStream(obj);
            tmdProcessStream(obj);
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
    node   = (TmdObject*)gTmdList.next;
    while (node != NULL) {
        if (node->buffer != NULL) {
            result += node->source->halfSize * 2;
        }
        node = (TmdObject*)node->next;
    }
    return result;
}

void Tmd_RewriteOpcodes(TmdSource* src)
{
    u32* stream;
    u32  id;
    u32  dims;
    u32  lo;
    u32  stop;

    stream = src->stream;
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

void Tmd_FlagAllNodes(Task* task)
{
    TmdObject* node;

    node = (TmdObject*)gTmdList.next;
    while (node != NULL) {
        node->flags |= 0x80;
        node         = (TmdObject*)node->next;
    }
    task->state++;
}

void Tmd_FreeNodeBuffers(Task* task)
{
    TmdObject* node;

    node = (TmdObject*)gTmdList.next;
    while (node != NULL) {
        if (node->buffer != NULL) {
            memFreeFromHeap(node->buffer, 1);
            node->buffer = NULL;
        }
        node = (TmdObject*)node->next;
    }
    task->state++;
}

void Tmd_DispatchTask(Task* task)
{
    TaskFuncTable3 sp;

    sp = Tmd_TaskStates;
    sp.funcs[task->state](task);
}

void Gpu_ResetGraphAndOt(void)
{
    TmdObject* node;

    node = (TmdObject*)gTmdList.next;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    while (node != NULL) {
        if (node->buffer != NULL) {
            node->buffer = NULL;
        }
        node = (TmdObject*)node->next;
    }
}

void Tmd_AllocMissingBuffers(void)
{
    TmdObject* node;
    void*      mem;

    node = (TmdObject*)gTmdList.next;
    Mem_InitAux();
    CdCmd_SetupMdecBuffers();
    while (node != NULL) {
        if (node->buffer == NULL) {
            if (!(node->flags & 4)) {
                mem = memCalloc(node->source->halfSize * 2, 1);
                if (mem != NULL) {
                    node->buffer      = mem;
                    node->bufferIndex = 0;
                    tmdProcessStream(node);
                    tmdProcessStream(node);
                }
            }
        }
        node = (TmdObject*)node->next;
    }
}

void Tmd_AllocNodeBuffers(Task* task)
{
    TmdObject* node;
    void*      mem;

    node = (TmdObject*)gTmdList.next;
    while (node != NULL) {
        if (node->buffer == NULL) {
            mem = memCalloc(node->source->halfSize * 2, 1);
            if (mem != NULL) {
                node->buffer      = mem;
                node->bufferIndex = 0;
                node->flags      &= ~0x80;
                tmdProcessStream(node);
                tmdProcessStream(node);
            }
        }
        node = (TmdObject*)node->next;
    }
    taskKill(task);
}

void Tmd_DrawFlaggedNodes(TmdObject* node)
{
    while (node != NULL) {
        if (node->flags & 8) {
            if (node->buffer != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = (TmdObject*)node->next;
    }
}

void Tmd_DrawActiveNodes(TmdObject* node)
{
    while (node != NULL) {
        if (!(node->flags & 0x80)) {
            if (node->buffer != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = (TmdObject*)node->next;
    }
}
