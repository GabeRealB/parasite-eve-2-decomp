#include "common.h"

#include "main/game.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/unknown_syms.h"

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
    Ui_DrawTitle(obj, D_8001398C);
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
    Ui_DrawTitle(obj, D_8001398C);
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
    Ui_DrawTitle(obj, D_8001398C);
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
    Ui_DrawTitle(obj, D_8001398C);
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

INCLUDE_ASM("main/nonmatchings/mcprompt", func_80030AB0);

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
    register s32        i asm("a3");
    register McDirWalk* walk asm("t0");
    register s32        val asm("v1");
    register u8*        p asm("v0");
    register s32        fill asm("t1");
    register s32        size asm("v0");
    register s32        head asm("a0");
    register s32        headAdj asm("a2");
    register s32        sizeAdj asm("v1");
    register s32        blocks asm("a1");
    register s32        start asm("a0");
    register s32        new28c asm("v0");
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
    Ui_DrawTitle(obj, D_8001398C);
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
    Ui_DrawTitle(obj, D_8001398C);
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

    one           = 1;
    saved         = arg0->spawnArg2;
    arg1->field_8 = 0x16;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_8001398C);
    base  = Mc_PromptTable;
    entry = &base[0x16];
    Text_DrawPrompt(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    child = arg0->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(D_8006121C, (s32)arg1, 1, 2, saved) != 0) {
            {
                UiList*     menu;
                register u8 t asm("a2");

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
                        u8*          src;
                        u8*          name;
                        register s32 matchCount asm("t0");
                        register u8* walk asm("a2");
                        u8*          dst;
                        register s32 i asm("v1");
                        s32          j;

                        src        = (u8*)arg1->field_30[saved->field_2C];
                        name       = Mc_FileName;
                        matchCount = 0x14;
                        walk       = name;
                        dst        = Mc_FileNameBuf;
                        i          = 0;
                        do {
                            {
                                register u8 ch asm("v0");
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
                                register u8 ch asm("v1");
                                register u8 n asm("v0");
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
                        u8*          name;
                        u8*          dst;
                        register s32 i asm("v1");

                        name = Mc_FileName;
                        dst  = Mc_FileNameBuf;
                        i    = 0;
                        do {
                            {
                                register u8 ch asm("v0");
                                ch    = *name;
                                name += 1;
                                i    += 1;
                                *dst  = ch;
                            }
                            dst += 1;
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
