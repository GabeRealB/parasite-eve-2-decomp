#include "common.h"
#include "main/mc.h"

#include <psyq/libmcrd.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

void func_80036A1C(void)
{
    char pad[0x10];
}

void func_80036A2C(void* arg0, GStruct20* arg1)
{
    func_80048C10(arg0, arg1);
    if (arg1->field_0 == 1) {
        func_80048D58(arg1, arg1->field_1c + 2, 0);
    }
}

void func_80036A70(Task* arg0)
{
    GStruct20* obj;
    UiList*    menu;

    obj  = arg0->field_20;
    menu = &D_8006116C;
    func_80047F40(obj, D_80013B64);
    if (arg0->field_30 == 0) {
        func_800489A0(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = 0;
        menu->field_9  = 0;
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
        if (obj->field_0 == 1) {
            func_80048D58(obj, obj->field_1c + 2, 0);
        }
    }
}

void func_80036B2C(GStruct60* arg0, UiObject* arg1)
{
    s16 var_v0;
    s32 temp;
    s8  temp2;

    temp2 = arg0->field_8;
    temp  = arg1->field_28->field_34;
    func_800330D8(arg1, temp, temp2, 0, arg0->field_1A + 7);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            var_v0         = (s8)(u8)arg0->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            var_v0         = -1;
        block_5:
            arg1->field_2C = var_v0;
        }
    }
}

void func_80036C04(Task* arg0)
{
    GStruct20* obj;
    UiList*    menu;
    GStruct64* ctx;
    s32        temp;

    obj  = arg0->field_20;
    ctx  = (GStruct64*)arg0->field_34;
    menu = &D_80061194;
    func_80047F40(obj, D_80013B64);
    if (arg0->field_30 == 0) {
        func_800489A0(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = ctx->field_290;
        temp           = (u8)menu->field_10 - menu->field_5 + 1;
        menu->field_9  = temp;
        if ((s8)temp < 0) {
            menu->field_9 = 0;
        }
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
        if (obj->field_0 == 1) {
            func_80048D58(obj, obj->field_1c + 2, 0);
        }
    }
}

void func_80036CF0(Task* arg0)
{
    void*   obj;
    s32     data;
    UiList* menu;
    s32     val;

    obj = arg0->field_20;
    if (arg0->field_30 == 0) {
        arg0->field_2a  = (u16)arg0->field_34;
        data            = arg0->field_8->field_34;
        arg0->field_30 += 1;
        arg0->field_34  = data;
    }
    data = arg0->field_34;
    func_80048E38(obj, D_80013BB4);
    if (arg0->field_2a == 1) {
        menu = &D_80061194;
    } else {
        menu = &D_8006116C;
    }
    val = menu->field_10;
    func_800330D8(obj, data, val, 0, 0);
}

void func_80036D98(GStruct60* arg0, UiObject* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A54, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            func_8005414C(0x15, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void func_80036E78(GStruct60* arg0, UiObject* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A64, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void func_80036F18(GStruct60* arg0, UiObject* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A5C, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void func_80036FB8(GStruct60* arg0, UiObject* arg1)
{
    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A58, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = -1;
        }
    }
}

void func_80037068(Task* arg0)
{
    GStruct30* obj;
    UiList*    menu;
    s32        mode;

    mode = arg0->field_34;
    obj  = arg0->field_20;
    if (mode == 2) {
        goto block_2;
    }
    if (mode >= 3) {
        goto block_default;
    }
    if (mode != 1) {
        goto block_default;
    }
    menu = &D_80061284;
    goto block_done;
block_2:
    menu = &D_800612AC;
    goto block_done;
block_default:
    menu = &D_8006125C;
block_done:
    if (arg0->field_30 == 0) {
        func_80046830(menu, obj);
        obj->field_C.y -= obj->field_C.h / 2;
        if (arg0->field_34 != 3) {
            menu->field_10 = 0;
        } else {
            menu->field_10 = 1;
        }
        menu->field_9 = 0;
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
    }
}
