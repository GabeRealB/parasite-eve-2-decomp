#include "common.h"

#include "main/game.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/20CAC", func_800304AC);

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

INCLUDE_ASM("main/nonmatchings/20CAC", func_800307AC);

INCLUDE_ASM("main/nonmatchings/20CAC", func_8003092C);

INCLUDE_ASM("main/nonmatchings/20CAC", func_80030AB0);

INCLUDE_ASM("main/nonmatchings/20CAC", func_80031118);

INCLUDE_ASM("main/nonmatchings/20CAC", func_800312DC);

INCLUDE_ASM("main/nonmatchings/20CAC", func_800314D0);
