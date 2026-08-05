#include "common.h"
#include "main/mc.h"

#include <psyq/libmcrd.h>
#include <psyq/rand.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/mc", func_800317DC);

void func_800319E4(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        MemCardClose();
        status         = MemCardOpen(arg1->field_C, Mc_FileName, 2);
        arg1->field_14 = status;
        switch (status) {
            case 0:
                arg0->field_30 = 0x1A;
                break;
            case 1:
                arg0->field_30 = 0x18;
                break;
            case 2:
                arg0->field_30 = 0x18;
                break;
            case 3:
                arg0->field_30 = 0x18;
                break;
            case 4:
                arg0->field_30 = 0x18;
                break;
            case 5:
                arg0->field_30 = 0x7;
                break;
            default:
                arg0->field_30 = 0x18;
                break;
        }
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateCreateFile(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        status         = MemCardCreateFile(arg1->field_C, Mc_FileName, 1);
        arg1->field_14 = status;
        switch (status) {
            case 0:
                arg0->field_30 = 0xA;
                break;
            case 1:
                arg0->field_30 = 0x14;
                break;
            case 4:
                arg0->field_30 = 0x15;
                break;
            case 7:
                arg0->field_30 = 0x19;
                break;
            case 2:
            case 3:
            case 5:
            case 6:
            default:
                arg0->field_30 = 0x2A;
                break;
        }
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80031C5C(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    if (status < 4U) {
        ptr1 = Mc_FileName;
        if (status == 0) {
            arg1->field_24 = 9;
            arg1->field_28 = -1;
            arg1->field_2C = 0;
            arg0->field_30 = 5;
        } else {
            goto pad;
        }
    } else {
        ptr1 = Mc_FileName;
    pad:
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 0x2A;
    }
    arg1->field_18 = 0;

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80031DA4(Task* arg0, McWork* arg1)
{
    register Task*     task asm("s3");
    register McWork*   work asm("s2");
    register UiObject* flag asm("s0");
    register s32       syncResult asm("s1");
    s32                status;
    s32                idx;
    s32                ret;
    Task*              child;
    UiObject*          obj;
    u8*                src;
    u8*                dst;
    s32                i;
    McPromptPair*      entry;
    McPromptPair*      base;

    task = arg0;
    work = arg1;
    arg1 = 0;
    if (work->field_2C == 1) {
        work->field_8 = 0x11;
        status        = func_800307AC(arg0, 0x11, work->field_0);
        switch (status) {
            case 0:
                break;
            case 1:
                work->field_4  = 0xE;
                work->field_1C = 0;
                task->field_30 = 0x28;
                break;
            case -1:
                src = Mc_FileNameBuf;
                dst = Mc_FileName;
                for (i = 0; i < 0x15; i++) {
                    *dst++ = *src++;
                }
                task->field_2a = 0xC;
                task->field_30 = 0x27;
                break;
        }
        syncResult = MemCardSync(1, (long*)&work->field_10, (long*)&work->field_14);
        if (syncResult != -1) {
            if (syncResult == 1) {
                if (work->field_14 != 0) {
                    child          = task->field_c;
                    task->field_30 = 2;
                    if (child != NULL) {
                        obj          = child->field_20;
                        flag         = task->field_20;
                        obj->field_0 = 0;
                        func_80048838(obj, obj->field_28);
                        flag->field_0 = syncResult;
                    }
                }
            }
        } else {
            MemCardExist(work->field_C);
        }
    } else {
        work->field_1C = 0;
        work->field_8  = 4;
        flag           = task->field_20;
        task->field_30 = 0xF;
        idx            = work->field_8;
        ret            = func_80048E10(flag, 1);
        flag->field_2E = 0;
        func_80048E38(flag, D_8001398C);
        base  = Mc_PromptTable;
        entry = &base[idx];
        func_8002FDCC(flag, flag->field_1C + 2, -2, entry->field_0, ret, 1, 0);
        func_8002FDCC(flag, flag->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    }
}

INCLUDE_ASM("main/nonmatchings/mc", func_80031F94);

void func_800322B0(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0:
            arg1->field_1C += Mc_BufferSlots[8 - arg1->field_24].field_8;
            arg0->field_30  = 0xF;
            break;
        case 1:
            MemCardClose();
            arg0->field_30 = 0x14;
            break;
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0 = 0;
            *ptr1 = 0;
            MemCardClose();
            arg0->field_30 = 0x2;
            break;
        case 2:
        case 4:
        case 5:
        default:
            arg0->field_30 = 0x2A;
            break;
    }
    Mem_Free((void*)arg1->field_18);
    arg1->field_18 = 0;

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateFormat(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           status;
    s32           idx;
    s32           next;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        status         = MemCardFormat(arg1->field_C);
        arg1->field_14 = status;
        if (status != 1) {
            if (status != 0) {
                next = 0x2B;
            } else {
                func_800300EC(Mc_FileName, 0);
                next            = 0x8;
                arg1->field_288 = 0;
            }
        } else {
            next = 0x14;
        }
        arg0->field_30 = next;
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

INCLUDE_ASM("main/nonmatchings/mc", func_80032578);

const McStateFuncTable26 D_80013ACC = { {
    (McStateFunc)0x80035A94,
    (McStateFunc)0x80035AD4,
    (McStateFunc)0x80035AF0,
    (McStateFunc)0x80035C2C,
    (McStateFunc)0x80035D14,
    (McStateFunc)0x80035E18,
    (McStateFunc)0x80035E48,
    (McStateFunc)0x80035ED4,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x800327A4,
    (McStateFunc)0x80035FD8,
    (McStateFunc)0x800360C8,
    (McStateFunc)0x800361C0,
    (McStateFunc)0x800328FC,
    (McStateFunc)0x80032AB0,
    (McStateFunc)0x800362A4,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x80032D54,
    (McStateFunc)0x800363AC,
    (McStateFunc)0x80036488,
    (McStateFunc)0x800365B0,
    (McStateFunc)0x800366BC,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x800367CC,
    (McStateFunc)0x80032578,
    (McStateFunc)0x800368DC,
} };

void func_800327A4(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0:
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0          = 0;
            *ptr1          = 0;
            arg0->field_30 = 0x12;
            break;
        case 1:
            arg0->field_30 = 0xA;
            break;
        case 4:
            arg0->field_30 = 0xB;
            break;
        case 2:
        default:
            arg0->field_30 = 0x6;
            break;
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800328FC(Task* arg0, McWork* arg1)
{
    register Task*   a0 asm("s5");
    register McWork* a1 asm("s3");
    s32              ret;
    s32              syncResult;
    u32              status;
    s32              idx;
    UiObject*        obj;
    McPromptPair*    entry;
    McPromptPair*    base;
    char*            fileName;

    a0            = arg0;
    a1            = arg1;
    obj           = a0->field_20;
    idx           = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);

    syncResult = MemCardSync(1, (long*)&a1->field_10, (long*)&a1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (a1->field_10 == syncResult) {
                if (a1->field_14 != 0) {
                    a0->field_30 = 6;
                }
            }
            a1->field_4 -= 1;
            if (a1->field_4 == 0) {
                fileName = Mc_FileName;
                MemCardClose();
                status       = MemCardOpen(a1->field_C, fileName, 1);
                a1->field_14 = status;
                switch (status) {
                    case 0:
                        a1->field_1C = 0;
                        a0->field_30 = 0xE;
                        break;
                    case 1:
                        a0->field_30 = 6;
                        break;
                    case 2:
                        a0->field_30 = 6;
                        break;
                    case 3:
                        a0->field_30 = 6;
                        break;
                    case 4:
                        a0->field_30 = 6;
                        break;
                    case 5:
                        a0->field_30 = 0xB;
                        break;
                    default:
                        a0->field_30 = 6;
                        break;
                }
            }
        }
    } else {
        MemCardExist(a1->field_C);
    }
}

INCLUDE_ASM("main/nonmatchings/mc", func_80032AB0);

INCLUDE_ASM("main/nonmatchings/mc", func_80032D54);

void func_80032F5C(GStruct60* arg0, UiObject* arg1)
{
    register GStruct60* s1 asm("s1");
    register UiObject*  s0 asm("s0");
    register s32        s2 asm("s2");
    s16                 var_v0;
    s32                 ok;
    s32                 var_s3;
    McSaveData*         temp_a2;
    register s16        sum asm("v1");
    s32                 i;
    s32                 limit;
    s32                 tmp;
    s32                 off;
    volatile u8*        ptr;

    s1 = arg0;
    s0 = arg1;
    asm("" : "+r"(s0), "+r"(s1));
    var_s3  = 1;
    off     = (s1->field_8 << 7) + 0x294;
    s2      = s0->field_28->field_34;
    temp_a2 = (McSaveData*)(s2 + off);
    sum     = 0;
    if ((u32)(temp_a2->field_12 - 1) >= 0x10U) {
        ok = 0;
    } else {
        ptr   = &temp_a2->field_4;
        limit = 0x38;
        i     = 0;
        do {
            i   += 1;
            tmp  = (s8)*ptr;
            sum  = sum + tmp;
            ptr += 1;
        } while (i < limit);
        ok = ((u16)temp_a2->field_1C ^ (sum & 0xFFFF)) == 0;
    }
    if (ok == 0) {
        var_s3 = 0;
        func_80048E10(s0, 2);
    }
    func_800330D8(s0, s2, s1->field_8, 0, s1->field_1A + 7);
    if (s1->field_C == 1) {
        if ((var_s3 != 0) && (Pad_CheckButtons(0, 1, D_8005ED70) != 0)) {
            func_8005414C(0x16, 0, 0);
            s0->field_2E = 6;
            var_v0       = (s8)(u8)s1->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            func_8005414C(0x3B, 0, 0);
            s0->field_2E = 6;
            var_v0       = -1;
        block_5:
            s0->field_2C = var_v0;
        }
    }
}

INCLUDE_ASM("main/nonmatchings/mc", func_800330D8);

u16* Mc_EncodeAsciiGlyphs(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;
    u8   ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        lower  = Mc_GlyphsLower;
        upper  = Mc_GlyphsUpper;
        symbol = Mc_GlyphsSymbol;
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx  = (ch - 0x61) * 2;
                idx += (s32)lower;
                goto store;
            }
            if (ch >= 0x41) {
                idx  = (ch - 0x41) * 2;
                idx += (s32)upper;
                goto store;
            }
            if (ch >= 0x20) {
                idx  = (ch - 0x20) * 2;
                idx += (s32)symbol;
            store:
                *arg1 = *(u16*)idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

void Mc_InitFileName(void)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    ptr1 = Mc_FileName;
    ptr0 = Mc_FileNameBuf;
    i    = 0;
    ch   = 0x5F;
    do {
        if (i >= 0xC) {
            *ptr0 = ch;
            *ptr1 = ch;
        }
        ptr1++;
        i++;
        ptr0++;
    } while (i < 0x14);
    *ptr0 = 0;
    *ptr1 = 0;
}

void Mc_CopyFileName(s32 arg0)
{
    u8* src;
    u8* dst;
    s32 i;

    if (arg0 == 0) {
        src = Mc_FileName;
        dst = Mc_FileNameBuf;
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
    }

    for (i = 0; i < 0x15; i++) {
        *dst++ = *src++;
    }
}

void Mc_WriteSaveHdrChecksum(void)
{
    s16 sum;
    u8* ptr;
    s32 limit;
    s32 i;
    s16 tmp;

    sum                  = 0;
    ptr                  = (u8*)&Mc_SaveData;
    ptr                 += 4;
    limit                = 0x38;
    i                    = 0;
    Mc_SaveData.field_1C = 0;
    Mc_SaveData.field_1E = 0xFFFF;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    Mc_SaveData.field_1C = sum;
    Mc_SaveData.field_1E = ~sum;
    Mc_VerifySaveHdrChecksum(&Mc_SaveData);
}

s32 Mc_VerifySaveHdrChecksum(McSaveData* arg0)
{
    register s16 sum asm("v1");
    volatile u8* ptr;
    s32          limit;
    s32          i;
    s32          tmp;

    if ((u32)(arg0->field_12 - 1) >= 0x10U) {
        return 0;
    }
    sum   = 0;
    ptr   = &arg0->field_4;
    limit = 0x38;
    i     = 0;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    return ((u16)arg0->field_1C ^ (sum & 0xFFFF)) == 0;
}

void Mc_WriteBlockChecksum(McChecksumBlock* arg0, s32 arg1)
{
    s16          sum;
    register u8* ptr asm("v1");
    u32          count;
    u32          i;

    sum   = 0;
    ptr   = arg0->field_4;
    count = arg1 - 4;
    i     = 0;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*ptr;
            ptr += 1;
        } while (i < count);
    }
    arg0->field_0 = sum;
    arg0->field_2 = ~sum;
}

void Mc_ResetSaveFlags(void)
{
    McSaveData* p;

    p            = &Mc_SaveData;
    p->field_21  = 0;
    p->field_1a8 = 0;
    p->field_1aa = 0;
    p->field_1ab = 0;
    p->field_1a9 = 0;
    p->field_25  = 0;
    func_800260B0(1);
    func_800429C8(0);
}

void Mc_ClearWorkBuffers(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    b = D_800734C8;
    Mem_Set(b, 0, 0xB0);
    c = D_80073628;
    Mem_Set(c, 0, 0x24);
    d = D_80073670;
    Mem_Set(d, 0, 0xE4);
    e = D_80073838;
    Mem_Set(e, 0, 0xA4);
    Mem_Set(a + 1, 0xFF, 0x6C);
    Mem_Set(b + 1, 0xFF, 0xB0);
    Mem_Set(c + 1, 0xFF, 0x24);
    Mem_Set(d + 1, 0xFF, 0xE4);
    Mem_Set(e + 1, 0xFF, 0xA4);
}

// TODO
void Mc_InitLib(void)
{
    MemCardInit(0); // 0 = No control routine
    MemCardStart();
    func_800303AC();
}

s32 Mc_VerifyBlockChecksum(McChecksumBlock* arg0, s32 arg1)
{
    s16          sum;
    register u8* ptr asm("a2");
    u32          count;
    u32          i;

    sum   = 0;
    ptr   = arg0->field_4;
    count = arg1 - 4;
    i     = 0;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*ptr;
            ptr += 1;
        } while (i < count);
    }
    return ((u16)arg0->field_0 ^ (sum & 0xFFFF)) == 0;
}

void func_80033C38(void)
{
}

s32 func_80033C40(void)
{
    McBufferSlot* base;
    u8*           src;
    u8*           dest;
    s32           size;
    register s32  flags asm("a3");
    u32           i;
    register u32  j asm("a0");
    s32           idx;

    flags = 0;
    i     = 0;
    base  = Mc_BufferSlots;
    do {
        idx  = 8 - i;
        src  = (u8*)base[idx].field_0;
        size = base[idx].field_4;
        j    = 0;
        dest = src + size;
        if (size != 0) {
            do {
                if (*src != *dest) {
                    flags |= 1;
                }
                j    += 1;
                src  += 1;
                dest += 1;
            } while (j < (u32)size);
        }
        i     += 1;
        flags *= 2;
    } while (i < 8U);
    return flags | 0x103;
}

void func_80033CC0(void)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    s32              inv;
    u32              count;
    u32              i;
    register u32     j asm("a0");
    u8*              ptr;

    i    = 1;
    inv  = 0xFFFF;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        temp  = p->field_0;
        count = p->field_4;
        ptr   = temp->field_4;
        count = count - 4;
        if (count != 0) {
            do {
                j   += 1;
                sum += (s8)*ptr;
                ptr += 1;
            } while (j < count);
        }
        p            += 1;
        i            += 1;
        temp->field_2 = inv - sum;
        temp->field_0 = sum;
    } while (i < 9U);
}

void func_80033D3C(void)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              next;
    s16              sum;
    u32              i;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        temp = p->field_0;
        p   += 1;
        i   += 1;
        next = sum + *(u8*)temp;
        sum  = next;
    } while (i < 9U);
    Mc_SaveData.field_940 = next;
    Mc_SaveData.field_942 = ~next;
}

s32 func_80033D88(void)
{
    s32           sum;
    u32           i;
    McBufferSlot* p;
    McBufferSlot* base;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum += *(u8*)p->field_0;
        p   += 1;
        i   += 1;
    } while (i < 9);
    return ((u16)D_80072AA8 ^ (sum & 0xFFFF)) == 0;
}

s32 func_80033DD4(void)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    u32              count;
    u32              i;
    register u32     j asm("a0");
    u8*              ptr;
    s32              flag;

    flag = 1;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        temp  = p->field_0;
        count = p->field_4;
        ptr   = temp->field_4;
        count = count - 4;
        if (count != 0) {
            do {
                j   += 1;
                sum += (s8)*ptr;
                ptr += 1;
            } while (j < count);
        }
        if ((u16)temp->field_0 != (sum & 0xFFFF)) {
            flag = 0;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
    return flag;
}

void func_80033E58(void)
{
    u32           i;
    u32           j;
    McBufferSlot* p;
    McBufferSlot* base;
    u8*           src;
    s32           size;
    u8*           dest;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src  = (u8*)p->field_0;
        size = p->field_4;
        j    = 0;
        dest = src + size;
        while (j < (u32)size) {
            j    += 1;
            *dest = *src;
            src  += 1;
            dest += 1;
        }
        i += 1;
        p += 1;
    } while (i < 9);
}

void Mc_DrawPrompt(Task* arg0, s32 arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_HideChildUi(Task* arg0)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = arg0->field_c;
    if (child != NULL) {
        obj          = child->field_20;
        flag         = arg0->field_20;
        obj->field_0 = 0;
        func_80048838(obj, obj->field_28);
        flag->field_0 = 1;
    }
}

void Mc_WriteDataChecksum(s32 arg0, McWork* arg1)
{
    s16          sum;
    s32          count;
    u8*          src;
    s16*         dst;
    register s32 i asm("v1");

    sum   = 0;
    count = 0x200;
    if (arg0 == 0) {
        src = Mc_DefaultChecksumSrc;
        dst = &D_80072AA4;
    } else {
        src = (u8*)arg1->field_18;
        dst = (s16*)&arg1->field_A1C;
    }

    i      = 0;
    dst[0] = sum;
    dst[1] = ~sum;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*src;
            src += 1;
        } while (i < count);
    }
    dst[0] = sum;
    dst[1] = ~sum;
}

s32 Mc_CompareSaveChecksum(McSaveData* arg0, McWork* arg1)
{
    if (arg0->field_5C2 != 0) {
        return 0;
    }
    if (arg0->field_23 != 0) {
        return 0;
    }
    return arg0->field_93C == arg1->field_A1C;
}

void Mc_ResetWork(Task* arg0, McWork* arg1)
{
    arg1->field_0   = 0x10;
    arg1->field_4   = 0;
    arg1->field_18  = 0;
    arg1->field_C   = 0;
    arg1->field_A18 = 0x34;
    arg1->field_A20 = 0;
    arg0->field_30++;
}

void func_800340A4(Task* arg0, McWork* arg1)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    s32              inv;
    u32              count;
    u32              i;
    register u32     j asm("a1");
    u8*              ptr;
    register s32     val asm("v0");

    i              = 1;
    inv            = 0xFFFF;
    base           = Mc_BufferSlots;
    p              = base + 1;
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    val            = i;
    arg1->field_2C = val;
    do {
        sum   = 0;
        j     = 0;
        temp  = p->field_0;
        count = p->field_4;
        ptr   = temp->field_4;
        count = count - 4;
        if (count != 0) {
            do {
                j   += 1;
                sum += (s8)*ptr;
                ptr += 1;
            } while (j < count);
        }
        p            += 1;
        i            += 1;
        temp->field_2 = inv - sum;
        temp->field_0 = sum;
    } while (i < 9U);

    if (arg0->field_34 != 0) {
        arg0->field_2a = 2;
        arg0->field_30 = 0x27;
    } else {
        arg0->field_30 = 0xE;
    }
}

void func_8003415C(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 1;
    if (MemCardAccept(arg1->field_C) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    arg1->field_4 = arg1->field_4 + 1;
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void func_8003429C(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void func_800343D0(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_800344B4(Task* arg0, McWork* arg1)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;
    u8*       src;
    u8*       dst;
    s32       i;

    arg1->field_8 = 0xB;
    ret           = func_8003062C(arg0, 0xB, arg1->field_0);
    if (ret != -1) {
        if (ret == 1) {
            arg0->field_30 = 8;
        }
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
        for (i = 0; i < 0x15; i++) {
            *dst++ = *src++;
        }
        arg0->field_2a = 0xC;
        arg0->field_30 = 0x27;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child          = arg0->field_c;
                arg0->field_30 = 2;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

void func_800345CC(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_800346AC(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0;
    arg0->field_30++;
    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_8003477C(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteFile(arg1->field_C, Mc_FileName, (unsigned long*)Mc_DefaultChecksumSrc, 0,
                         0x200) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80034894(Task* arg0, McWork* arg1)
{
    s32 ret;

    arg1->field_8 = 7;
    ret           = func_8003062C(arg0, 7, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->field_2a = 0xC;
            arg0->field_30 = 0x27;
            break;
        case -1:
            arg0->field_30 = 0x13;
            break;
    }
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void func_80034938(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80034A40(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;
    UiObject*     flag;
    s16           val;

    MemCardClose();
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = 0x1B;
    flag           = arg0->field_20;
    if (flag != NULL) {
        val            = arg1->field_A18;
        flag->field_2E = -1;
        flag->field_2C = val;
    }
}

void func_80034B38(Task* arg0, McWork* arg1)
{
    if (arg0->field_2a != 0) {
        Task_Kill(arg0);
    }
}

void func_80034B68(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (func_8003092C(arg0, 3, arg1->field_0) != 0) {
        arg0->field_30 = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
                arg0->field_30 = 2;
            }
            return;
        case 0:
            return;
    }
}

void func_80034C54(Task* arg0, McWork* arg1)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 9;
    ret           = func_800307AC(arg0, 9, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->field_30 = 0x16;
            break;
        case -1:
            arg0->field_2a = 0xC;
            arg0->field_30 = 0x29;
            break;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child          = arg0->field_c;
                arg0->field_30 = 2;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

void func_80034D50(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 6;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[6];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg1->field_4  = 0xE;
    arg0->field_30 = arg0->field_30 + 1;
}

void func_80034E3C(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0x13;
    if (func_8003092C(arg0, 0x13, arg1->field_0) != 0) {
        arg0->field_30 = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = rslt;
                }
                arg0->field_30 = 0x14;
            }
            return;
        case 0:
            return;
    }
}

void func_80034F2C(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, arg1->field_8, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 0x13;
    }
}

void func_80034FB4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->field_20;
    idx            = arg1->field_8;
    ret            = func_80048E10(obj, 1);
    obj->field_2E  = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->field_2a = 0;
        arg0->field_30 = -1;
    }
}

void func_800350B0(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = 0x1F;
}

void func_80035180(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_4-- <= 0) {
        arg1->field_A18 = 0x33;
        arg0->field_30  = 0x13;
    }
}

void func_8003527C(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void Mc_StateOpenSelected(Task* arg0, McWork* arg1)
{
    register McWork*   a1 asm("s1");
    register Task*     a0 asm("s2");
    register s32       openIdx asm("s0");
    register UiObject* obj asm("s0");
    register s32       modeIdx asm("s1");
    s32                ret;
    s32                openResult;
    McPromptPair*      entry;
    McPromptPair*      base;

    a1      = arg1;
    openIdx = a1->field_A14;
    MemCardClose();
    a0           = arg0;
    openResult   = MemCardOpen(a1->field_C, a1->field_30[openIdx], 1);
    a1->field_14 = openResult;
    if (openResult == 0) {
        a1->field_4  = 0;
        a0->field_30 = a0->field_30 + 1;
    } else {
        a0->field_30 = 0x18;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80035464(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateOpenNext(Task* arg0, McWork* arg1)
{
    register McWork*   a1 asm("s1");
    register Task*     a0 asm("s0");
    register UiObject* obj asm("s0");
    register s32       modeIdx asm("s1");
    register s32       ret asm("s3");
    s32                temp_v0;
    McPromptPair*      entry;
    McPromptPair*      base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->field_30 = 0x22;
        } else {
            a0->field_30 = a0->field_30 + 1;
        }
    } else {
        a0->field_30 = 0x18;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80035684(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->field_2a -= 1;
    if (arg0->field_2a <= 0) {
        arg0->field_30 = 2;
    }
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80035764(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->field_2a -= 1;
    if (arg0->field_2a <= 0) {
        arg0->field_30 = 0xE;
    }
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80035844(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 <= 0) {
        arg0->field_30 = 0xF;
    }
    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_8003591C(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xE;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xE, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

void func_80035960(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xD;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xD, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

void func_800359A4(Task* arg0)
{
    McStateFuncTable44 sp;
    McWork*            work;
    s32                state;

    sp    = D_800139AC;
    work  = &D_80071730;
    state = arg0->field_30;
    if (state < 0) {
        func_80034B38(arg0, work);
        return;
    }
    sp.funcs[state](arg0, work);
    if (work->field_4 >= 0xB5) {
        if (work->field_18 != 0) {
            Mem_Free((void*)work->field_18);
            work->field_18 = 0;
        }
        arg0->field_30 = 0x18;
    }
    D_80073C08 = rand();
}

void func_80035A94(Task* arg0, McWork* arg1)
{
    arg1->field_0           = 0x10;
    arg1->field_8           = 0x8;
    arg1->field_A20         = 1;
    arg1->field_4           = 0;
    arg1->field_18          = 0;
    arg1->field_C           = 0;
    Display_State.field_101 = 0;
    arg0->field_30         += 1;
}

void func_80035AD4(Task* arg0, McWork* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg0->field_30 = 7;
}

void func_80035AF0(Task* arg0, McWork* arg1)
{
    s32           status;
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 == 0) {
        arg1->field_8 = 8;
        status        = func_8003062C(arg0, 8, arg1->field_0);
        switch (status) {
            case 0:
                break;
            case 1:
                arg0->field_30 = 7;
                break;
            case -1:
                arg0->field_30 = 3;
                break;
        }
    } else {
        obj           = arg0->field_20;
        idx           = arg1->field_8;
        ret           = func_80048E10(obj, 1);
        obj->field_2E = 0;
        func_80048E38(obj, D_8001398C);
        base  = Mc_PromptTable;
        entry = &base[idx];
        func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
        func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    }
}

void Mc_StateCloseReturn(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    MemCardClose();
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = 4;
    if (arg0->field_20 != NULL) {
        ((UiObject*)arg0->field_20)->field_2E = -1;
    }
}

void func_80035D14(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->field_20;
    idx            = arg1->field_8;
    ret            = func_80048E10(obj, 1);
    obj->field_2E  = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->field_2a = 0;
        arg0->field_30 = arg0->field_30 + 1;
    }
}

void func_80035E18(Task* arg0)
{
    if (arg0->field_2a != 0) {
        Task_Kill(arg0);
    }
}

void func_80035E48(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0xF;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xF, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 3;
    }
}

void func_80035ED4(Task* arg0, McWork* arg1)
{
    register McWork*   a1 asm("s1");
    register Task*     a0 asm("s0");
    register UiObject* obj asm("s0");
    register s32       modeIdx asm("s1");
    register s32       ret asm("s3");
    McPromptPair*      entry;
    McPromptPair*      base;

    a0          = arg0;
    a1          = arg1;
    a1->field_8 = 1;
    if (MemCardAccept(a1->field_C) != 0) {
        a1->field_4  = 0;
        a0->field_30 = a0->field_30 + 1;
    } else {
        a1->field_4 = a1->field_4 + 1;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_80035FD8(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (func_8003092C(arg0, 3, arg1->field_0) != 0) {
        arg0->field_30 = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
                arg0->field_30 = 7;
            }
            return;
        case 0:
            return;
    }
}

void func_800360C8(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0xA;
    if (func_8003092C(arg0, 0xA, arg1->field_0) != 0) {
        arg0->field_30 = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = rslt;
                }
                arg0->field_30 = 0xA;
            }
            return;
        case 0:
            return;
    }
}

void func_800361C0(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_4 = 4;
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_800362A4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800363AC(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_80036488(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        arg1->field_288 = 0;
        MemCardGetDirentry(
            arg1->field_C, (char*)D_80060DC8, (struct DIRENTRY*)arg1->field_30, &arg1->field_288, 0,
            0xF);
        if (arg1->field_288 != 0) {
            arg1->field_290 = 0;
            arg1->field_A14 = 0;
            arg0->field_30  = arg0->field_30 + 1;
        } else {
            arg0->field_30 = 0xB;
        }
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800365B0(Task* arg0, McWork* arg1)
{
    register McWork*   a1 asm("s1");
    register Task*     a0 asm("s2");
    register s32       openIdx asm("s0");
    register UiObject* obj asm("s0");
    register s32       modeIdx asm("s1");
    s32                ret;
    s32                openResult;
    McPromptPair*      entry;
    McPromptPair*      base;

    a1      = arg1;
    openIdx = a1->field_A14;
    MemCardClose();
    a0           = arg0;
    openResult   = MemCardOpen(a1->field_C, a1->field_30[openIdx], 1);
    a1->field_14 = openResult;
    if (openResult == 0) {
        a1->field_4  = 0;
        a0->field_30 = a0->field_30 + 1;
    } else {
        a0->field_30 = 6;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800366BC(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4  = 0;
        arg0->field_30 = arg0->field_30 + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800367CC(Task* arg0, McWork* arg1)
{
    register McWork*   a1 asm("s1");
    register Task*     a0 asm("s0");
    register UiObject* obj asm("s0");
    register s32       modeIdx asm("s1");
    register s32       ret asm("s3");
    s32                temp_v0;
    McPromptPair*      entry;
    McPromptPair*      base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->field_30 = 0x14;
        } else {
            a0->field_30 = a0->field_30 + 1;
        }
    } else {
        a0->field_30 = 0x6;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800368DC(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0x17;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0x17, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 3;
    }
}

void func_80036968(Task* arg0)
{
    McStateFuncTable26 sp;
    McWork*            work;

    sp   = D_80013ACC;
    work = &D_80071730;
    sp.funcs[arg0->field_30](arg0, work);
    if (work->field_4 >= 0xB5) {
        arg0->field_30 = 6;
    }
}
