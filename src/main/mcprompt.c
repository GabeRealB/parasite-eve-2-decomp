#include "common.h"

#include "main/unknown_syms.h"
#include "main/text.h"
#include "main/ui.h"

#include "psyq/kernel.h"
#include "psyq/libmcrd.h"
#include "psyq/strings.h"

s32 Mc_PromptDialog(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->firstChild;
    if (p == NULL) {
        p = Ui_SpawnFromDesc(D_800612D0, one, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->baseX + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->baseY + obj->field_1A + 8;
            obj->field_2C = 0;
            obj->status   = 0;
        }
        return 0;
    }
    p = ((Task*)p)->spawnArg2;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        Ui_TeardownTree(p, p->owner);
        obj->status = one;
    }
    return obj->field_2C;
}

s32 Mc_PromptDialogChoice(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->firstChild;
    if (p == NULL) {
        p = Ui_SpawnFromDesc(D_800612D0, 0, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->baseX + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->baseY + obj->field_1A + 0x10;
            obj->field_2C = 0;
            obj->status   = 0;
        }
        return 0;
    }
    p = ((Task*)p)->spawnArg2;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        Ui_TeardownTree(p, p->owner);
        obj->status = one;
    }
    return obj->field_2C;
}

s32 Mc_PromptDialogSpawn(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->firstChild;
    if (p == NULL) {
        p = Ui_SpawnFromDesc(D_800612D0, 3, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->baseX + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->baseY + obj->field_1A + 0x10;
            obj->field_2C = 0;
            obj->status   = 0;
        }
        return 0;
    }
    p = ((Task*)p)->spawnArg2;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        Ui_TeardownTree(p, p->owner);
        obj->status = one;
    }
    return obj->field_2C;
}

s32 Mc_PromptDialogFile(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->firstChild;
    if (p == NULL) {
        p = Ui_SpawnFromDesc(D_800612D0, 2, one, 2, obj);
        if (p != NULL) {
            p->field_12   = 0x12;
            p->field_C    = (obj->baseX + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->baseY + obj->field_1A + 8;
            obj->field_2C = 0;
            obj->status   = 0;
        }
        return 0;
    }
    p = ((Task*)p)->spawnArg2;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        Ui_TeardownTree(p, p->owner);
        obj->status = one;
    }
    return obj->field_2C;
}

static inline u16* Mc_EncodeTitleText(s8* arg0, u16* arg1)
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
                idx   = (ch - 0x61) * 2;
                idx  += (s32)lower;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x41) {
                idx   = (ch - 0x41) * 2;
                idx  += (s32)upper;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x20) {
                idx   = (ch - 0x20) * 2;
                idx  += (s32)symbol;
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

static inline u16* Mc_EncodeTitleLiteral(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;

    if (*arg0 != 0) {
        lower  = Mc_GlyphsLower;
        upper  = Mc_GlyphsUpper;
        symbol = Mc_GlyphsSymbol;
        do {
            ch = *arg0;
            if (ch >= 0x61) {
                idx   = (ch - 0x61) * 2;
                idx  += (s32)lower;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x41) {
                idx   = (ch - 0x41) * 2;
                idx  += (s32)upper;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x20) {
                idx   = (ch - 0x20) * 2;
                idx  += (s32)symbol;
                *arg1 = *(u16*)idx;
            }
            arg0++;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static inline void Mc_UpdateTitleHeaderChecksum(void)
{
    u16 sum;
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
    Mc_SaveData.field_1E = 0xFFFF - (u32)sum;
    Mc_VerifySaveHdrChecksum(&Mc_SaveData);
}

static inline u8* Mc_CopyTitleBytes(u8* src, u8* dst)
{
    while (*src != 0) {
        *dst++ = *src++;
    }
    *dst = 0;
    return dst;
}

static inline void Mc_UpdateTitleDataChecksum(void)
{
    u16              sum;
    s32              count;
    u8*              src;
    McChecksumBlock* dst;
    s32              i;

    sum          = 0;
    count        = 0x200;
    src          = Mc_DefaultChecksumSrc;
    dst          = (McChecksumBlock*)&Mc_SaveData.field_93C;
    i            = 0;
    dst->field_0 = sum;
    dst->field_2 = 0xFFFF - (u32)sum;
    do {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    } while (i < count);
    dst->field_0 = sum;
    dst->field_2 = 0xFFFF - (u32)sum;
}

void func_80030AB0(McWork* work)
{
    u8          buffer[0x20];
    u16*        title;
    s32         number;
    s32         i;
    s32         candidate;
    s32         available;
    McSaveData* slot;

    title  = Mc_SaveHeaderBody;
    number = 1;
    if (work->field_288 > 0) {
        for (i = 0; i < work->field_288; i++) {
            slot = (McSaveData*)((s32)work + 0x294 + i * 0x80);
            if ((s8)slot->field_12 == (s8)Mc_SaveData.field_12) {
                if (slot->unknown_11 >= number) {
                    number = slot->unknown_11 + 1;
                }
            }
        }
        if (number >= 100) {
            for (candidate = 1; candidate < 100; candidate++) {
                available = 1;
                for (i = 0; i < work->field_288; i++) {
                    slot = (McSaveData*)((s32)work + 0x294 + i * 0x80);
                    if ((s8)slot->field_12 == (s8)Mc_SaveData.field_12 && slot->unknown_11 == candidate) {
                        available = 0;
                        break;
                    }
                }
                if (available == 1) {
                    number = candidate;
                    break;
                }
            }
        }
    }
    Mc_SaveData.unknown_11      = number;
    title                       = Mc_EncodeTitleLiteral(D_80013998, title);
    title                       = Mc_EncodeTitleText((s8*)Text_FormatTime(buffer, Mc_SaveData.field_C), title);
    title                       = Mc_EncodeTitleLiteral(D_800139A0, title);
    Mc_DefaultChecksumSrc[0x43] = 0;
    Mc_DefaultChecksumSrc[0x42] = 0;
    title                       = (u16*)Mc_CopyTitleBytes(D_800675F0[(s8)Mc_SaveData.field_12], (u8*)title);
    title                       = Mc_EncodeTitleLiteral(D_800139A4, title);
    title                       = Mc_EncodeTitleText((s8*)Text_ItoaSigned(buffer, Mc_SaveData.unknown_11), title);
    title                       = Mc_EncodeTitleLiteral((s8*)D_800139A8, title);
    *title                      = 0;
    if (Mc_SaveData.field_92B == 0xFF) {
        Mc_SaveData.field_92B = 0;
    } else if (Mc_SaveData.field_92B < 99) {
        Mc_SaveData.field_92B++;
    }
    Mc_UpdateTitleHeaderChecksum();
    Mc_UpdateTitleDataChecksum();
    Mc_SaveData.field_940         = 0;
    *(u16*)&Mc_SaveData.field_942 = 0xFFFF;
}

/* Overlay: DIRENTRY.size/head at McWork+0x48/0x50 when walk starts at McWork. */
typedef struct {
    u8  _pad[0x48];
    s32 size;
    s32 _pad4C;
    s32 head;
} McDirWalk;

void Mc_StateScanDirFlags(Task* arg0, McWork* arg1)
{
    s32                 ret;
    s32                 one;
    s32                 j;
    UiObject*           obj;
    McPromptPair*       prompt;
    McPromptPair*       base;
    s32                 idx;
    s32                 i;
    register McDirWalk* walk asm("t0");
    s32                 val;
    u8*                 p;
    s32                 fill;
    register s32        size asm("v0");
    s32                 head;
    register s32        headAdj asm("a2");
    register s32        sizeAdj asm("v1");
    register s32        blocks asm("a1");
    register s32        start asm("a0");
    s32                 new28c;
    register s32        n asm("v1");

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        arg1->field_288 = 0;
        val             = -1;
        i               = 0xE;
        p               = (u8*)arg1 + i;
        do {
            p[0xA24] = val;
            i       -= 1;
            p       -= 1;
        } while (i >= 0);

        i = 0xF;
        MemCardGetDirentry(
            arg1->field_C, D_80013A5C, (struct DIRENTRY*)arg1->field_30, &arg1->field_288, 0,
            i);

        arg1->field_28C = 0;
        if (arg1->field_288 != 0) {
            i = 0;
            if (arg1->field_288 > 0) {
                fill = -2;
                walk = (McDirWalk*)arg1;
                do {
                    size = walk->size;
                    head = walk->head;

                    sizeAdj = size;
                    if (size < 0) {
                        sizeAdj = size + 0x1FFF;
                    }
                    headAdj = head;
                    sizeAdj = sizeAdj >> 13;
                    blocks  = sizeAdj + ((size & 0x1FFF) != 0);

                    if (head < 0) {
                        headAdj = head + 0x3F;
                    }
                    start = (headAdj >> 6) - 1;

                    j = 0;
                    if (blocks > 0) {
                        do {
                            arg1->field_A24[start + j] = fill;
                            j                         += 1;
                        } while (j < blocks);
                    }

                    walk            = (McDirWalk*)((u8*)walk + sizeof(struct DIRENTRY));
                    i              += 1;
                    new28c          = arg1->field_28C + blocks;
                    n               = arg1->field_288;
                    arg1->field_28C = new28c;
                } while (i < n);
            }
        }
        arg0->state += 1;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one    = 1;
    base   = Mc_PromptTable;
    prompt = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, prompt->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, prompt->field_4, ret, one, 0);
}

void Mc_StateListDirectory(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           one;
    s32           var_s0;
    s32           temp_v0;
    s32           temp_v0_2;
    s32           temp_v0_3;
    s32           var_v0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_288 = 0;
    MemCardGetDirentry(
        arg1->field_C, (char*)D_80060DC8, (struct DIRENTRY*)arg1->field_30, &arg1->field_288, 0,
        0xF);
    temp_v0         = arg1->field_28C - arg1->field_288;
    arg1->field_28C = temp_v0;
    if (temp_v0 == 0xF) {
        var_v0 = 0x19;
    } else {
        if (arg1->field_28 == -1) {
            arg1->field_290 = 0;
        } else {
            temp_v0_2       = arg1->field_288;
            arg1->field_290 = 0;
            if (temp_v0_2 != 0) {
                var_s0 = 0;
                if (temp_v0_2 > 0) {
                    do {
                        if (strncmp(arg1->field_30[var_s0], (char*)Mc_FileName, 0x14) == 0) {
                            arg1->field_290 = var_s0;
                            break;
                        }
                        temp_v0_2 = arg1->field_288;
                        var_s0   += 1;
                    } while (var_s0 < temp_v0_2);
                }
            }
        }
        arg1->field_A14 = 0;
        if (arg1->field_288 > 0) {
            var_v0 = arg0->state + 1;
        } else {
            var_v0 = 0x26;
        }
    }
    arg0->state = var_v0;

    temp_v0_3 = arg1->field_288;
    if (temp_v0_3 > 0) {
        {
            register s32 var_v1 asm("v1");
            register s32 sh asm("v0");
            s32          var_a0;
            s32          temp_v0_4;

            for (var_a0 = 0; var_a0 < arg1->field_288; var_a0++) {
                temp_v0_4 = ((struct DIRENTRY*)arg1->field_30)[var_a0].head;
                var_v1    = temp_v0_4;
                if (temp_v0_4 < 0) {
                    var_v1 = temp_v0_4 + 0x3F;
                }
                sh                      = var_v1 >> 6;
                ((u8*)arg1)[sh + 0xA23] = var_a0;
            }
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

void Mc_StateFileSelect(Task* arg0, McWork* arg1)
{
    UiObject*     saved;
    UiObject*     obj;
    UiObject*     childObj;
    UiObject*     flag;
    Task*         child;
    McPromptPair* entry;
    McPromptPair* base;
    s32           ret;
    s32           one;
    s32           syncResult;
    u8            ch;

    one           = 1;
    saved         = arg0->spawnArg2;
    arg1->field_8 = 0x16;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[0x16];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    child = arg0->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(D_8006121C, (s32)arg1, 1, 2, saved) != 0) {
            {
                UiList* menu;
                u8      t;

                menu          = &D_80061194;
                t             = arg1->field_288;
                menu->field_4 = t;
                if (arg1->field_288 < (0xF - arg1->field_28C)) {
                    {
                        register u8 sum asm("v0");
                        sum           = t + one;
                        menu->field_4 = sum;
                    }
                }
                menu->field_10  = arg1->field_290;
                saved->field_2C = 0;
                saved->status   = 0;
            }
        }
    } else {
        childObj = child->spawnArg2;
        if (childObj->field_2E == 6) {
            saved->field_2C  = childObj->field_2C;
            childObj->status = 0;
            Ui_TeardownTree(childObj, childObj->owner);
            saved->status = one;
            if (saved->field_2C >= 0) {
                if (saved->field_2C < arg1->field_288) {
                    {
                        u8* src;
                        u8* name;
                        s32 matchCount;
                        u8* walk;
                        u8* dst;
                        s32 i;
                        s32 j;

                        src        = (u8*)arg1->field_30[saved->field_2C];
                        name       = Mc_FileName;
                        matchCount = 0x14;
                        walk       = name;
                        dst        = Mc_FileNameBuf;
                        i          = 0;
                        do {
                            {
                                u8 ch;
                                ch    = *walk;
                                walk += 1;
                                i    += 1;
                                *dst  = ch;
                            }
                            dst += 1;
                        } while (i < 0x15);
                        j = 0;
                        do {
                            {
                                u8 ch;
                                u8 n;
                                ch = *src;
                                n  = *name;
                                if (n == ch) {
                                    matchCount -= 1;
                                }
                                *name = ch;
                            }
                            name += 1;
                            j    += 1;
                            src  += 1;
                        } while (j < 0x14);
                        *name = 0;
                        if (matchCount != 0) {
                            arg1->field_28 = -1;
                        }
                    }
                } else {
                    {
                        u8* name;
                        u8* dst;
                        s32 i;

                        name = Mc_FileName;
                        dst  = Mc_FileNameBuf;
                        i    = 0;
                        do {
                            ch    = *name;
                            name += 1;
                            i    += 1;
                            *dst  = ch;
                            dst  += 1;
                        } while (i < 0x15);
                    }
                    {
                        register u8* fn asm("a0");
                        fn = Mc_FileName;
                        Mc_BuildFileName(fn, saved->field_2C);
                    }
                }
                arg1->field_8 = 1;
                arg0->state   = 5;
                return;
            }
            arg0->state = 0x29;
            return;
        }
    }

    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                {
                    register Task* ch asm("v1");

                    ch          = arg0->firstChild;
                    arg0->state = 2;
                    if (ch != NULL) {
                        childObj         = ch->spawnArg2;
                        flag             = arg0->spawnArg2;
                        childObj->status = 0;
                        Ui_TeardownTree(childObj, childObj->owner);
                        flag->status = syncResult;
                    }
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}
