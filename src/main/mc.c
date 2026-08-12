#include "common.h"

#include <psyq/libmcrd.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/text.h"
#include "main/ui.h"

void Mc_StateCompareBuffers(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           one;
    s32           ch;
    s32           i;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0: {
            s32           flags;
            u32           i0;
            register u32  j asm("a1");
            s32           size;
            u8*           src;
            u8*           dest;
            McBufferSlot* slots;

            arg1->field_24 = 9;
            flags          = 0;
            i0             = flags;
            slots          = Mc_BufferSlots;
            do {
                src  = (u8*)slots[8 - i0].field_0;
                size = slots[8 - i0].field_4;
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
                i0    += 1;
                flags *= 2;
            } while (i0 < 8U);
            flags         |= 0x103;
            arg1->field_2C = 1;
            arg1->field_28 = flags;
            arg0->state    = 0x1F;
            break;
        }
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
            arg1->field_24 = 9;
            arg1->field_28 = -1;
            arg1->field_2C = 1;
            arg0->state    = 0x1F;
            break;
        case 1:
            arg0->state = 0x14;
            break;
        case 4:
            arg0->state = 0x15;
            break;
        case 2:
        default:
            arg0->state = 0x18;
            break;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

void Mc_StateOpenRead(Task* arg0, McWork* arg1)
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
                arg0->state = 0x1A;
                break;
            case 1:
                arg0->state = 0x18;
                break;
            case 2:
                arg0->state = 0x18;
                break;
            case 3:
                arg0->state = 0x18;
                break;
            case 4:
                arg0->state = 0x18;
                break;
            case 5:
                arg0->state = 0x7;
                break;
            default:
                arg0->state = 0x18;
                break;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
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
                arg0->state = 0xA;
                break;
            case 1:
                arg0->state = 0x14;
                break;
            case 4:
                arg0->state = 0x15;
                break;
            case 7:
                arg0->state = 0x19;
                break;
            case 2:
            case 3:
            case 5:
            case 6:
            default:
                arg0->state = 0x2A;
                break;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StatePadFileName(Task* arg0, McWork* arg1)
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
            arg0->state    = 5;
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
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 0x2A;
    }
    arg1->field_18 = 0;

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateNameEntry(Task* arg0, McWork* arg1)
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
        status        = Mc_PromptDialogSpawn(arg0, 0x11, work->field_0);
        switch (status) {
            case 0:
                break;
            case 1:
                work->field_4  = 0xE;
                work->field_1C = 0;
                task->state    = 0x28;
                break;
            case -1:
                src = Mc_FileNameBuf;
                dst = Mc_FileName;
                for (i = 0; i < 0x15; i++) {
                    *dst++ = *src++;
                }
                task->killCountdown = 0xC;
                task->state         = 0x27;
                break;
        }
        syncResult = MemCardSync(1, (long*)&work->field_10, (long*)&work->field_14);
        if (syncResult != -1) {
            if (syncResult == 1) {
                if (work->field_14 != 0) {
                    child       = task->firstChild;
                    task->state = 2;
                    if (child != NULL) {
                        obj         = child->spawnArg2;
                        flag        = task->spawnArg2;
                        obj->status = 0;
                        Ui_TeardownTree(obj, obj->owner);
                        flag->status = syncResult;
                    }
                }
            }
        } else {
            MemCardExist(work->field_C);
        }
    } else {
        work->field_1C = 0;
        work->field_8  = 4;
        flag           = task->spawnArg2;
        task->state    = 0xF;
        idx            = work->field_8;
        ret            = Ui_LookupTable(flag, 1);
        flag->field_2E = 0;
        Ui_DrawTitle(flag, D_8001398C);
        base  = Mc_PromptTable;
        entry = &base[idx];
        Text_DrawPrompt(flag, flag->field_1C + 2, -2, entry->field_0, ret, 1, 0);
        Text_DrawPrompt(flag, flag->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    }
}

void Mc_StateBackupBuffers(Task* arg0, McWork* arg1)
{
    register Task*            task asm("s6");
    register McWork*          work asm("s0");
    register s32              nine asm("s5");
    register s32              bufSize asm("s3");
    register McChecksumBlock* bufPtr asm("s2");
    register s32              doubled asm("s4");
    register void*            mem asm("s1");
    s32                       field24;
    register s32              f24 asm("v0");
    s32                       size;
    s32                       tmp;
    s32                       ret;
    s32                       one;
    UiObject*                 obj;
    McPromptPair*             entry;
    McPromptPair*             promptBase;

    work    = arg1;
    task    = arg0;
    field24 = work->field_24;
    if (field24 == 0) {
        register u32           i0 asm("t0");
        register u32           j asm("v1");
        register McBufferSlot* p asm("a3");
        register McBufferSlot* base asm("v0");
        u32                    count;
        u8*                    ptr;
        u8*                    dest;

        i0   = 1;
        base = Mc_BufferSlots;
        p    = base + 1;
        do {
            ptr   = (u8*)p->field_0;
            count = p->field_4;
            j     = 0;
            dest  = ptr + count;
            if (count != 0) {
                do {
                    j    += 1;
                    *dest = *ptr;
                    ptr  += 1;
                    dest += 1;
                } while (j < count);
            }
            i0 += 1;
            p  += 1;
        } while (i0 < 9U);
        work->field_A18 = 0x33;
        task->state     = 0x13;
    } else if (work->field_28 & 1) {
        nine = 9;
        {
            register McBufferSlot* slots asm("a0");
            slots   = Mc_BufferSlots;
            bufSize = slots[nine - field24].field_4;
            bufPtr  = slots[nine - field24].field_0;
        }
        doubled        = bufSize * 2;
        size           = doubled - 1;
        size           = (u32)size >> 7;
        size           = size + 1;
        size           = size << 7;
        work->field_20 = size;
        mem            = Mem_Calloc(size, 0);
        if (mem != 0) {
            work->field_18 = (s32)mem;
            if (work->field_24 == nine) {
                func_80030AB0(work);
                memcpy(mem, bufPtr, doubled);
            } else {
                {
                    s16              sum;
                    u8*              src;
                    u32              ji;
                    u32              cnt;
                    McChecksumBlock* blk;
                    s16              inv;

                    sum = 0;
                    src = bufPtr->field_4;
                    ji  = 0;
                    cnt = bufSize - 4;
                    blk = bufPtr;
                    if (cnt != 0) {
                        do {
                            ji  += 1;
                            sum += (s8)*src;
                            src += 1;
                        } while (ji < cnt);
                    }
                    inv          = ~sum;
                    blk->field_0 = sum;
                    blk->field_2 = inv;
                }
                if (work->field_24 == 8) {
                    McChecksumBlock* temp;
                    McBufferSlot*    bp;
                    McBufferSlot*    bbase;
                    s16              next;
                    s16              sum;
                    u32              i;

                    sum   = 0;
                    i     = 1;
                    bbase = Mc_BufferSlots;
                    bp    = bbase + 1;
                    do {
                        temp = bp->field_0;
                        bp  += 1;
                        i   += 1;
                        next = sum + *(u8*)temp;
                        sum  = next;
                    } while (i < 9U);
                    Mc_SaveData.field_940 = next;
                    Mc_SaveData.field_942 = ~next;
                }
                memcpy(mem, bufPtr, bufSize);
                memcpy((u8*)mem + bufSize, bufPtr, bufSize);
            }
            work->field_4 = 0;
            task->state   = task->state + 1;
            f24           = work->field_24;
            goto update;
        }
        work->field_4 = work->field_4 + 1;
    } else {
        tmp = work->field_1C + Mc_BufferSlots[9 - field24].field_8;
        asm("" ::: "memory");
        f24            = work->field_24;
        work->field_1C = tmp;
    update: {
        register s32 f28 asm("v1");
        f28 = work->field_28;
        f24 = f24 - 1;
        f28 = (u32)f28 >> 1;
        asm volatile("" : "+r"(f24), "+r"(f28));
        work->field_24 = f24;
        work->field_28 = f28;
    }
    }

    work->field_8 = 4;
    obj           = task->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    one        = 1;
    promptBase = Mc_PromptTable;
    entry      = &promptBase[4];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

void Mc_StateFreeBuffer(Task* arg0, McWork* arg1)
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
            arg0->state     = 0xF;
            break;
        case 1:
            MemCardClose();
            arg0->state = 0x14;
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
            arg0->state = 0x2;
            break;
        case 2:
        case 4:
        case 5:
        default:
            arg0->state = 0x2A;
            break;
    }
    Mem_Free((void*)arg1->field_18);
    arg1->field_18 = 0;

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
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
                Mc_BuildFileName(Mc_FileName, 0);
                next            = 0x8;
                arg1->field_288 = 0;
            }
        } else {
            next = 0x14;
        }
        arg0->state = next;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateSyncFileSelect(Task* arg0, McWork* arg1)
{
    register Task*     task asm("s6");
    register s32       one asm("s5");
    register UiObject* saved asm("s3");
    register s32       syncResult asm("s1");
    McWork*            work;
    s32                ret;
    s32                i;
    s32                next;
    Task*              child;
    UiObject*          obj;
    UiObject*          childObj;
    UiObject*          flag;
    u8*                src;
    u8*                dst;
    McPromptPair*      entry;
    McPromptPair*      base;

    task          = arg0;
    work          = arg1;
    one           = 1;
    saved         = task->spawnArg2;
    work->field_8 = 0x16;
    obj           = task->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[0x16];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    syncResult = MemCardSync(one, (long*)&work->field_10, (long*)&work->field_14);
    if (syncResult != -1) {
        if (syncResult == one) {
            if (work->field_14 != 0) {
                {
                    register Task* ch asm("v1");
                    ch          = task->firstChild;
                    task->state = 7;
                    if (ch != NULL) {
                        childObj         = ch->spawnArg2;
                        flag             = task->spawnArg2;
                        childObj->status = 0;
                        Ui_TeardownTree(childObj, childObj->owner);
                        flag->status = syncResult;
                    }
                }
            } else {
                goto block_6;
            }
        } else {
            goto block_6;
        }
    } else {
        MemCardExist(work->field_C);
    block_6:
        child = task->firstChild;
        if (child == NULL) {
            if (Ui_SpawnFromDesc(D_80061200, (s32)work, 1, 2, saved) != 0) {
                do {
                    D_80061170 = work->field_288;
                } while (0);
                saved->field_2C = 0;
                saved->status   = 0;
            }
        } else {
            childObj = child->spawnArg2;
            if (childObj->field_2E == 6) {
                saved->field_2C  = childObj->field_2C;
                childObj->status = 0;
                Ui_TeardownTree(childObj, childObj->owner);
                saved->status = 1;
                if (saved->field_2C >= 0) {
                    src = (u8*)work->field_30[saved->field_2C];
                    dst = Mc_FileName;
                    i   = 0;
                    do {
                        *dst = *src;
                        src += 1;
                        i   += 1;
                        dst += 1;
                    } while (i < 0x14);
                    next = 0xC;
                    *dst = 0;
                } else {
                    next = 3;
                }
                task->state = next;
            }
        }
    }
}

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

void Mc_StateBlankFileName(Task* arg0, McWork* arg1)
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
            *ptr0       = 0;
            *ptr1       = 0;
            arg0->state = 0x12;
            break;
        case 1:
            arg0->state = 0xA;
            break;
        case 4:
            arg0->state = 0xB;
            break;
        case 2:
        default:
            arg0->state = 0x6;
            break;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateSyncOpen(Task* arg0, McWork* arg1)
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
    obj           = a0->spawnArg2;
    idx           = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);

    syncResult = MemCardSync(1, (long*)&a1->field_10, (long*)&a1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (a1->field_10 == syncResult) {
                if (a1->field_14 != 0) {
                    a0->state = 6;
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
                        a0->state    = 0xE;
                        break;
                    case 1:
                        a0->state = 6;
                        break;
                    case 2:
                        a0->state = 6;
                        break;
                    case 3:
                        a0->state = 6;
                        break;
                    case 4:
                        a0->state = 6;
                        break;
                    case 5:
                        a0->state = 0xB;
                        break;
                    default:
                        a0->state = 6;
                        break;
                }
            }
        }
    } else {
        MemCardExist(a1->field_C);
    }
}

void Mc_StateVerifyFinish(Task* arg0, McWork* arg1)
{
    McChecksumBlock*       temp;
    register McBufferSlot* p asm("a3");
    McBufferSlot*          base;
    s16                    sum;
    u32                    count;
    register u32           i asm("t1");
    register u32           j asm("a0");
    u8*                    ptr;
    s32                    flag;
    register s32           slotSum asm("a1");
    register u32           i2 asm("a0");
    register McBufferSlot* p2 asm("v1");
    s32                    size;
    void*                  mem;
    s32                    ret;
    s32                    one;
    UiObject*              obj;
    McPromptPair*          entry;
    McPromptPair*          promptBase;

    if (arg1->field_24 == 0) {
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

        if (flag != 0) {
            i2      = 1;
            slotSum = 0;
            base    = Mc_BufferSlots;
            p2      = base + 1;
            do {
                slotSum += *(u8*)p2->field_0;
                p2      += 1;
                i2      += 1;
            } while (i2 < 9);
            {
                register u32 chk asm("v1");
                register u32 masked asm("v0");
                chk    = D_80072AA8;
                masked = slotSum & 0xFFFF;
                if (chk != masked) {
                    goto fail;
                }
            }
            Game_ClearEd68();
            Display_State.field_101 = 1;
            arg0->state             = 3;
        } else {
        fail:
            Mc_InitBufferSlots();
            arg0->state = 0x19;
        }
    } else if (arg1->field_28 & 1) {
        size           = Mc_BufferSlots[9 - arg1->field_24].field_4;
        size         <<= 1;
        size          -= 1;
        size           = (u32)size >> 7;
        size          += 1;
        size         <<= 7;
        arg1->field_20 = size;
        mem            = Mem_Malloc(size, 0);
        arg1->field_18 = (s32)mem;
        if (mem != 0) {
            arg1->field_4  = 0;
            arg0->state    = arg0->state + 1;
            arg1->field_24 = arg1->field_24 - 1;
            arg1->field_28 = (u32)arg1->field_28 >> 1;
        } else {
            arg1->field_4 = arg1->field_4 + 1;
        }
    } else {
        arg1->field_1C = arg1->field_1C + Mc_BufferSlots[9 - arg1->field_24].field_8;
        arg1->field_24 = arg1->field_24 - 1;
        arg1->field_28 = (u32)arg1->field_28 >> 1;
    }

    arg1->field_8 = 5;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    one        = 1;
    promptBase = Mc_PromptTable;
    entry      = &promptBase[5];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

void Mc_StateFinishWrite(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    s32           one;
    s16           sum;
    s16           tmp;
    s32           limit;
    s32           slotIdx;
    u8*           ptr1;
    u8*           ptr0;
    u8*           src;
    s16*          dst;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    if (status < 4U) {
        if (status == 0) {
            slotIdx = 8 - arg1->field_24;
            if (slotIdx == 0) {
                sum   = 0;
                limit = 0x200;
                asm volatile("" : "=r"(slotIdx) : "0"(0));
                dst             = (s16*)&arg1->field_A1C;
                src             = (u8*)arg1->field_18;
                arg1->field_A1C = 0;
                *(u16*)&dst[1]  = 0xFFFF;
                do {
                    slotIdx += 1;
                    tmp      = (s8)*src;
                    sum      = sum + tmp;
                    src     += 1;
                } while (slotIdx < limit);
                dst[0] = sum;
                dst[1] = ~sum;
            } else {
                {
                    register McBufferSlot* slots asm("v0");
                    register s32           size asm("a2");
                    register void*         dest asm("a0");

                    slots = Mc_BufferSlots;
                    size  = slots[slotIdx].field_4;
                    dest  = slots[slotIdx].field_0;
                    memcpy(dest, (void*)arg1->field_18, size << 1);
                }
            }
            arg1->field_1C += Mc_BufferSlots[8 - arg1->field_24].field_8;
            arg0->state     = 0xE;
        } else {
            goto pad;
        }
    } else {
    pad:
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
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 6;
    }

    one = 1;
    Mem_Free((void*)arg1->field_18);
    arg1->field_18 = 0;

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

void Mc_StateSaveSlotUi(DialogPrompt* arg0, UiObject* arg1)
{
    register DialogPrompt* s1 asm("s1");
    register UiObject*     s0 asm("s0");
    register s32           s2 asm("s2");
    s16                    var_v0;
    s32                    ok;
    s32                    var_s3;
    McSaveData*            temp_a2;
    register s16           sum asm("v1");
    s32                    i;
    s32                    limit;
    s32                    tmp;
    s32                    off;
    volatile u8*           ptr;

    s1 = arg0;
    s0 = arg1;
    asm("" : "+r"(s0), "+r"(s1));
    var_s3  = 1;
    off     = (s1->field_8 << 7) + 0x294;
    s2      = s0->owner->spawnArg1;
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
        Ui_LookupTable(s0, 2);
    }
    func_800330D8(s0, s2, s1->field_8, 0, s1->field_1A + 7);
    if (s1->field_C == 1) {
        if ((var_s3 != 0) && (Pad_CheckButtons(0, 1, D_8005ED70) != 0)) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            s0->field_2E = 6;
            var_v0       = (s8)(u8)s1->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
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
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
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
    Mc_InitBufferSlots();
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

s32 Mc_CompareBufferHalves(void)
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

void Mc_WriteSlotChecksums(void)
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

void Mc_WriteFirstByteChecksum(void)
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

s32 Mc_VerifyFirstByteChecksum(void)
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

s32 Mc_VerifySlotChecksums(void)
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

void Mc_DuplicateBuffers(void)
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

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_HideChildUi(Task* arg0)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = arg0->firstChild;
    if (child != NULL) {
        obj         = child->spawnArg2;
        flag        = arg0->spawnArg2;
        obj->status = 0;
        Ui_TeardownTree(obj, obj->owner);
        flag->status = 1;
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
    arg0->state++;
}

void Mc_WriteSlotChecksumsEx(Task* arg0, McWork* arg1)
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

    if (arg0->spawnArg1 != 0) {
        arg0->killCountdown = 2;
        arg0->state         = 0x27;
    } else {
        arg0->state = 0xE;
    }
}

void Mc_StateAcceptMode1(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 1;
    if (MemCardAccept(arg1->field_C) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    arg1->field_4 = arg1->field_4 + 1;
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void Mc_StateSyncAdvance(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void Mc_StateDrawPromptAdvance(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

void Mc_StatePromptChoiceB(Task* arg0, McWork* arg1)
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
    ret           = Mc_PromptDialogChoice(arg0, 0xB, arg1->field_0);
    if (ret != -1) {
        if (ret == 1) {
            arg0->state = 8;
        }
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
        for (i = 0; i < 0x15; i++) {
            *dst++ = *src++;
        }
        arg0->killCountdown = 0xC;
        arg0->state         = 0x27;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child       = arg0->firstChild;
                arg0->state = 2;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

void Mc_StateDrawPrompt4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

void Mc_StateEnterDialog4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0;
    arg0->state++;
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateWriteFile(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteFile(arg1->field_C, Mc_FileName, (unsigned long*)Mc_DefaultChecksumSrc, 0,
                         0x200) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StatePromptChoiceGeneric(Task* arg0, McWork* arg1)
{
    s32 ret;

    arg1->field_8 = 7;
    ret           = Mc_PromptDialogChoice(arg0, 7, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->killCountdown = 0xC;
            arg0->state         = 0x27;
            break;
        case -1:
            arg0->state = 0x13;
            break;
    }
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

void Mc_StateWriteData(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateClosePrompt(Task* arg0, McWork* arg1)
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
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 0x1B;
    flag        = arg0->spawnArg2;
    if (flag != NULL) {
        val            = arg1->field_A18;
        flag->field_2E = -1;
        flag->field_2C = val;
    }
}

void Mc_KillIfCountdown(Task* arg0, McWork* arg1)
{
    if (arg0->killCountdown != 0) {
        Task_Kill(arg0);
    }
}

void Mc_StateSyncPromptFile3(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (Mc_PromptDialogFile(arg0, 3, arg1->field_0) != 0) {
        arg0->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = syncResult;
                }
                arg0->state = 2;
            }
            return;
        case 0:
            return;
    }
}

void Mc_StatePromptChoice9(Task* arg0, McWork* arg1)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 9;
    ret           = Mc_PromptDialogSpawn(arg0, 9, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->state = 0x16;
            break;
        case -1:
            arg0->killCountdown = 0xC;
            arg0->state         = 0x29;
            break;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child       = arg0->firstChild;
                arg0->state = 2;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

void Mc_StateColdBoot(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 6;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[6];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg1->field_4 = 0xE;
    arg0->state   = arg0->state + 1;
}

void Mc_StateSyncPrompt13(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0x13;
    if (Mc_PromptDialogFile(arg0, 0x13, arg1->field_0) != 0) {
        arg0->state = 0x13;
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
                child = arg0->firstChild;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = rslt;
                }
                arg0->state = 0x14;
            }
            return;
        case 0:
            return;
    }
}

void Mc_StateEnterPrompt0(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, arg1->field_8, 0) != 0) {
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
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 0x13;
    }
}

void Mc_StatePromptCountdown(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->spawnArg2;
    idx            = arg1->field_8;
    ret            = Ui_LookupTable(obj, 1);
    obj->field_2E  = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->killCountdown = 0;
        arg0->state         = -1;
    }
}

void Mc_StateDrawPromptTo1F(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 0x1F;
}

void Mc_StateCountdownPrompt4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_4-- <= 0) {
        arg1->field_A18 = 0x33;
        arg0->state     = 0x13;
    }
}

void Mc_StateDrawPrompt1Advance(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
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
        a1->field_4 = 0;
        a0->state   = a0->state + 1;
    } else {
        a0->state = 0x18;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateReadHeader(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
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
            a0->state = 0x22;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x18;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateUiCountdown2(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->killCountdown -= 1;
    if (arg0->killCountdown <= 0) {
        arg0->state = 2;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateUiCountdownE(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->killCountdown -= 1;
    if (arg0->killCountdown <= 0) {
        arg0->state = 0xE;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateUiCountdownF(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 <= 0) {
        arg0->state = 0xF;
    }
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateEnterPromptE(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xE;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xE, 0) != 0) {
        arg0->state = 0x13;
    }
}

void Mc_StateEnterPromptD(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xD;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xD, 0) != 0) {
        arg0->state = 0x13;
    }
}

void Mc_DispatchStateTable(Task* arg0)
{
    McStateFuncTable44 sp;
    McWork*            work;
    s32                state;

    sp    = D_800139AC;
    work  = &D_80071730;
    state = arg0->state;
    if (state < 0) {
        Mc_KillIfCountdown(arg0, work);
        return;
    }
    sp.funcs[state](arg0, work);
    if (work->field_4 >= 0xB5) {
        if (work->field_18 != 0) {
            Mem_Free((void*)work->field_18);
            work->field_18 = 0;
        }
        arg0->state = 0x18;
    }
    D_80073C08 = rand();
}

void Mc_StateInitWorkDefaults(Task* arg0, McWork* arg1)
{
    arg1->field_0           = 0x10;
    arg1->field_8           = 0x8;
    arg1->field_A20         = 1;
    arg1->field_4           = 0;
    arg1->field_18          = 0;
    arg1->field_C           = 0;
    Display_State.field_101 = 0;
    arg0->state            += 1;
}

void Mc_StateSetOpenDefaults(Task* arg0, McWork* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg0->state    = 7;
}

void Mc_StateCountdownPrompt(Task* arg0, McWork* arg1)
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
        status        = Mc_PromptDialogChoice(arg0, 8, arg1->field_0);
        switch (status) {
            case 0:
                break;
            case 1:
                arg0->state = 7;
                break;
            case -1:
                arg0->state = 3;
                break;
        }
    } else {
        obj           = arg0->spawnArg2;
        idx           = arg1->field_8;
        ret           = Ui_LookupTable(obj, 1);
        obj->field_2E = 0;
        Ui_DrawTitle(obj, D_8001398C);
        base  = Mc_PromptTable;
        entry = &base[idx];
        Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
        Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
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
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 4;
    if (arg0->spawnArg2 != NULL) {
        ((UiObject*)arg0->spawnArg2)->field_2E = -1;
    }
}

void Mc_StatePromptTimeout(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->spawnArg2;
    idx            = arg1->field_8;
    ret            = Ui_LookupTable(obj, 1);
    obj->field_2E  = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->killCountdown = 0;
        arg0->state         = arg0->state + 1;
    }
}

void Mc_KillIfCountdownAlt(Task* arg0)
{
    if (arg0->killCountdown != 0) {
        Task_Kill(arg0);
    }
}

void Mc_StateEnterPromptF(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0xF;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xF, 0) != 0) {
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
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 3;
    }
}

void Mc_StateAccept(Task* arg0, McWork* arg1)
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
        a1->field_4 = 0;
        a0->state   = a0->state + 1;
    } else {
        a1->field_4 = a1->field_4 + 1;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateSyncPrompt3(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (Mc_PromptDialogFile(arg0, 3, arg1->field_0) != 0) {
        arg0->state = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = syncResult;
                }
                arg0->state = 7;
            }
            return;
        case 0:
            return;
    }
}

void Mc_StateSyncPromptA(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0xA;
    if (Mc_PromptDialogFile(arg0, 0xA, arg1->field_0) != 0) {
        arg0->state = 3;
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
                child = arg0->firstChild;
                if (child != NULL) {
                    obj         = child->spawnArg2;
                    flag        = arg0->spawnArg2;
                    obj->status = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->status = rslt;
                }
                arg0->state = 0xA;
            }
            return;
        case 0:
            return;
    }
}

void Mc_StateDrawCurrentPrompt(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_4 = 4;
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

void Mc_StateReadData(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateDrawPrompt1(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

void Mc_StateGetDirentry(Task* arg0, McWork* arg1)
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
            arg0->state     = arg0->state + 1;
        } else {
            arg0->state = 0xB;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateOpenDirEntry(Task* arg0, McWork* arg1)
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
        a1->field_4 = 0;
        a0->state   = a0->state + 1;
    } else {
        a0->state = 6;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateReadSlot(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateWalkDirectory(Task* arg0, McWork* arg1)
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
            a0->state = 0x14;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x6;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void Mc_StateEnterPrompt17(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0x17;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0x17, 0) != 0) {
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
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 3;
    }
}

void Mc_DispatchStateTable26(Task* arg0)
{
    McStateFuncTable26 sp;
    McWork*            work;

    sp   = D_80013ACC;
    work = &D_80071730;
    sp.funcs[arg0->state](arg0, work);
    if (work->field_4 >= 0xB5) {
        arg0->state = 6;
    }
}
