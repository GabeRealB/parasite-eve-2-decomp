#include "common.h"

#include "main/game.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/unknown_syms.h"

#include "psyq/kernel.h"
#include "psyq/libmcrd.h"
#include "psyq/strings.h"

s32 func_800304AC(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->field_c;
    if (p == NULL) {
        p = func_800486F0(D_800612D0, one, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->field_20 + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->field_22 + obj->field_1A + 8;
            obj->field_2C = 0;
            obj->field_0  = 0;
        }
        return 0;
    }
    p = ((Task*)p)->field_20;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        func_80048838(p, p->field_28);
        obj->field_0 = one;
    }
    return obj->field_2C;
}

s32 func_8003062C(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->field_c;
    if (p == NULL) {
        p = func_800486F0(D_800612D0, 0, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->field_20 + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->field_22 + obj->field_1A + 0x10;
            obj->field_2C = 0;
            obj->field_0  = 0;
        }
        return 0;
    }
    p = ((Task*)p)->field_20;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        func_80048838(p, p->field_28);
        obj->field_0 = one;
    }
    return obj->field_2C;
}

s32 func_800307AC(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->field_c;
    if (p == NULL) {
        p = func_800486F0(D_800612D0, 3, one, 2, obj);
        if (p != NULL) {
            p->field_C    = (obj->field_20 + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->field_22 + obj->field_1A + 0x10;
            obj->field_2C = 0;
            obj->field_0  = 0;
        }
        return 0;
    }
    p = ((Task*)p)->field_20;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        func_80048838(p, p->field_28);
        obj->field_0 = one;
    }
    return obj->field_2C;
}

s32 func_8003092C(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    UiObject*     p;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);

    p = (UiObject*)arg0->field_c;
    if (p == NULL) {
        p = func_800486F0(D_800612D0, 2, one, 2, obj);
        if (p != NULL) {
            p->field_12   = 0x12;
            p->field_C    = (obj->field_20 + obj->field_1E + 5) - p->field_10;
            p->field_E    = obj->field_22 + obj->field_1A + 8;
            obj->field_2C = 0;
            obj->field_0  = 0;
        }
        return 0;
    }
    p = ((Task*)p)->field_20;
    if (p->field_2E == 6) {
        obj->field_2C = p->field_2C;
        func_80048838(p, p->field_28);
        obj->field_0 = one;
    }
    return obj->field_2C;
}

INCLUDE_ASM("main/nonmatchings/20CAC", func_80030AB0);

INCLUDE_ASM("main/nonmatchings/20CAC", func_80031118);

void func_800312DC(Task* arg0, McWork* arg1)
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
            var_v0 = arg0->field_30 + 1;
        } else {
            var_v0 = 0x26;
        }
    }
    arg0->field_30 = var_v0;

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

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, one, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, one, 0);
}

INCLUDE_ASM("main/nonmatchings/20CAC", func_800314D0);
