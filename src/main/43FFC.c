#include "common.h"

#include "main/unknown_syms.h"

void func_800537FC(s32 arg0, s32 arg1)
{
    u8* var_s0;
    s32 var_a0;
    s32 var_v1;
    s32 temp_v1;

    D_8008274C = 0;
    func_800566A4();
    arg0 = arg0 & 0xFF;
    func_800546C0();
    func_800542D0(0x50000000, 1);
    func_800542D0(0x10000000, 1);
    func_800542D0(0xFF0D, 1);
    func_800542D0(0x20000000, 1);
    func_800542D0(0xE0000000, 1);
    arg1       = arg1 & 0xFF;
    D_80082120 = arg0;
    D_80082136 = arg1;
    func_800561EC(1);
    func_800561EC(7);

    var_v1 = 0;
    if (arg1 == 5) {
        if (arg0 == 4) {
            var_a0 = 3;
        } else {
            goto block_5;
        }
    } else {
    block_5:
        do {
            if (D_80068A54[var_v1 + arg0 * 2] == arg1) {
                var_a0 = 2;
                goto block_done;
            }
            var_v1++;
        } while (var_v1 < 2);
        var_a0 = 1;
    }
block_done:
    func_80054608(var_a0);
    D_80082130 = 0x3D010;
    D_80082128 = 0;
    D_80082124 = D_80082128;

    temp_v1 = (s8)D_80082135;
    switch (temp_v1) {
        case 0:
            func_8004D0F0((GStruct42*)&D_8007E158);
            func_800561EC(4);
        case 1:
            D_80082122 = 0;
            break;
        case 2:
            D_80082122 = 1;
            break;
    }
    var_s0 = D_8007E0F8;

    D_800820F0.field_14 = 0;
    D_800820F0.field_18 = 0;
    D_8008212C          = D_80082122;
    D_80082121          = D_80082135;
    func_8004D0F0((GStruct42*)var_s0);
    func_8004D0F0((GStruct42*)(var_s0 + 0xC0));
    func_8004D0F0((GStruct42*)(var_s0 + 0x80));
    func_800561EC(5);
    func_8004D0F0((GStruct42*)(var_s0 + 0xA0));
    func_800561EC(6);
    func_8004D0F0((GStruct42*)(var_s0 + 0x40));
    func_800561EC(3);
    func_8005454C(1, 0x40000000);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053A20);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053BF4);

void func_80053D90(void)
{
    func_80053DB0(0);
}

void func_80053DB0(s32 arg0)
{
    if (arg0 == 0) {
        func_8004D0F0((GStruct42*)&D_8007E258);
        D_80082134 = 0;
        return;
    }
    D_80082134 = 1;
}

void func_80053DF4(s32 arg0)
{
    s8 temp;

    temp = (s8)D_80082135;
    if (temp == 0) {
        if (arg0 != 0) {
            D_80082135 = 1;
        }
    } else if (temp >= 0) {
        if ((temp < 3) && (arg0 == 0)) {
            D_80082135 = 0;
        }
    }
}

void func_80053E48(void)
{
    func_8004DC8C();
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053E68);

// K&R definition so the no-arg call in func_8005462C stays legal (indeterminate a0).
s32 func_80053F00(arg0)
s32 arg0;
{
    s32        var_s0;
    GStruct31* temp_v0;

    var_s0 = arg0;
    if ((var_s0 & 0xF0000000) == 0x10000000) {
        temp_v0 = func_80056104(0x1000, 1);
        if (temp_v0 != NULL) {
            var_s0 = (temp_v0->field_0->field_4 << 0x10) + (var_s0 & 0xFFFF);
        }
    }
    return var_s0;
}

s32 func_80053F60(s32* arg0)
{
    s32 temp;

    temp  = *arg0 + 1;
    *arg0 = temp;
    if (temp < 0x3D) {
        return 0;
    }
    F3E48C_SetReverbDepth(0x2800);
    return -1;
}

void func_80053FA0(s32 arg0)
{
    s32 var_a0;

    if (arg0 == 0) {
        D_800689EC = 0;
        func_80055DFC(0x7F);
        var_a0 = 0x40;
    } else {
        D_800689EC = 1;
        func_80055DFC(0x28);
        var_a0 = 0;
    }
    func_800517B4(var_a0);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053FF4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005414C);

void func_800542D0(s32 arg0, s32 arg1)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 7;
        mid           = (GStruct16From4*)&temp->field_4;
        mid->field_4  = func_80053F00(arg0);
        mid->field_2  = arg1;
        func_80050A38(temp);
    }
}

void func_80054334(s32 arg0)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 8;
            mid           = (GStruct16From4*)&temp->field_4;
            mid->field_4  = func_80053F00(arg0);
            func_80050A38(temp);
        }
    }
}

void func_800543AC(s32 arg0)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 9;
            mid           = (GStruct16From4*)&temp->field_4;
            mid->field_4  = func_80053F00(arg0);
            func_80050A38(temp);
        }
    }
}

void func_80054424(s32 arg0, s32 arg1, s32 arg2)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 0xA;
            mid           = (GStruct16From4*)&temp->field_4;
            mid->field_4  = func_80053F00(arg0);
            temp->field_4 = arg1;
            mid->field_1  = arg2;
            func_80050A38(temp);
        }
    }
}

void func_800544B8(s32 arg0, s32 arg1)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 0xB;
            mid           = (GStruct16From4*)&temp->field_4;
            mid->field_4  = func_80053F00(arg0);
            mid->field_1  = arg1;
            if ((s8)arg1 < 0) {
                mid->field_1 = 0x7F;
            }
            func_80050A38(temp);
        }
    }
}

void func_8005454C(s32 arg0, s32 arg1)
{
    u8*             ptr;
    register s32    flag asm("v1");
    GStruct16*      temp;
    GStruct16From4* mid;

    if (arg1 == 0x80000000) {
        arg1 = 0;
        ptr  = D_80082138;
        flag = arg0 & 1;
    loop:
        *(u8*)(arg1 + (s32)ptr) = flag;
        arg1                   += 1;
        if (arg1 < 0x10) {
            goto loop;
        }
    } else {
        flag                                  = (s32)D_80082138;
        arg1                                 &= 0xF0000000;
        ((volatile u8*)flag)[(u32)arg1 >> 28] = arg0 & 1;
        if (arg0 == 0) {
            if (arg1 == 0x40000000) {
                temp = func_800509F4();
                if (temp != NULL) {
                    temp->field_2 = 7;
                    mid           = (GStruct16From4*)&temp->field_4;
                    mid->field_4  = func_80053F00(0x40000000);
                    mid->field_2  = 1;
                    func_80050A38(temp);
                }
            }
        }
    }
}

void func_80054608(s8 arg0)
{
    func_80055D78(arg0);
}

s32 func_8005462C(void)
{
    return ~func_80055DAC(func_80053F00()) != 0;
}

void func_80054658(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xD;
        func_80050A38(temp);
    }
}

void func_8005468C(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xE;
        func_80050A38(temp);
    }
}

void func_800546C0(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xF;
        func_80050A38(temp);
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_800546F4);

void func_8005488C(void)
{
    s16 var_a0;
    s8  bound;

    var_a0 = func_80055EE8();
    if (D_8008274A > 0) {
        var_a0 = var_a0 + D_8008274A;
        bound  = *(u8*)&D_80082749;
        if (bound < var_a0) {
            if (bound != 0) {
                var_a0     = bound;
                D_80082749 = 0;
            }
            D_8008274A = 0;
        }
    } else if (D_8008274A < 0) {
        var_a0 = var_a0 + D_8008274A;
        if (var_a0 < 0x30) {
            var_a0     = 0x30;
            D_8008274A = 0;
        }
    }
    func_80055DFC(var_a0);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054938);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054D58);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054F1C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055078);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055678);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800558E8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800559BC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055A9C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055B70);

void func_80055C00(void)
{
    s8 temp;

    D_8008274C += 1;
    if (D_8008274C == 1) {
        if (D_8008274A == 0) {
            if (D_80082749 == 0) {
                temp = func_80055EE8();
                if (temp >= 0x30) {
                    D_80082749 = temp;
                    D_8008274A = -8;
                }
            }
        }
    }
}

void func_80055C8C(void)
{
    if (D_8008274C > 0) {
        D_8008274C -= 1;
        if (D_8008274C == 0) {
            if (D_80082749 != 0) {
                D_8008274A = 8;
            }
        }
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055CE0);

void func_80055D78(s8 arg0)
{
    if (arg0 < 0) {
        D_8008274B = -1;
        return;
    }
    D_8008274B = arg0;
    if (arg0 == 0) {
        D_8008274B = 1;
    }
}

s32 func_80055DAC(s32 arg0)
{
    s32        i;
    GStruct54* p;

    i = 0;
    p = D_80082248;
    do {
        if ((p->field_16 & 0xA3) && (p->field_0 == arg0)) {
            return i;
        }
        i++;
        p++;
    } while (i < 8);
    return -1;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055DFC);

s8 func_80055EE8(void)
{
    return D_80082748;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055EF8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055F70);

void func_80056068(GStruct43* arg0)
{
    GStruct43* temp_v0;
    GStruct43* temp_v1;

    if (arg0 != NULL) {
        temp_v0       = arg0->field_38;
        arg0->field_8 = 0;
        arg0->field_0 = 0;
        if (temp_v0 == NULL) {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0 = (GStruct43*)arg0->field_34;
                if (temp_v0 != NULL) {
                    ((GStruct57*)temp_v0)->field_40 = NULL;
                }
            } else {
                temp_v0 = (GStruct43*)arg0->field_34;
                if (temp_v0 != NULL) {
                    ((GStruct57*)temp_v0)->field_40 = temp_v1;
                }
                temp_v0           = arg0->field_3C;
                temp_v0->field_38 = NULL;
            }
        } else {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0->field_3C = NULL;
            } else {
                temp_v0->field_3C = temp_v1;
                temp_v1           = arg0->field_3C;
                temp_v0           = arg0->field_38;
                temp_v1->field_38 = temp_v0;
            }
        }
        arg0->field_38 = NULL;
        arg0->field_3C = NULL;
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056104);

GStruct31* func_800561C0(s32 arg0)
{
    if ((u8)arg0 < 0x10) {
        return &D_80082148[(s8)arg0];
    }
    return NULL;
}

void func_800561EC(s32 arg0)
{
    GStruct31* temp_s0;
    GStruct31* base;

    if ((u8)arg0 < 0x10) {
        base    = D_80082148;
        temp_s0 = &base[(s8)arg0];
        F3D458_Free(temp_s0->field_0);
        temp_s0->field_8 = -1;
        temp_s0->field_0 = NULL;
    }
}

GStruct43* func_80056240(s32 arg0)
{
    s32        voiceIdx;
    GStruct43* ptr;

    voiceIdx = (s8)func_8004E060(D_80068A7C, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    ptr          = (GStruct43*)D_80082148 + voiceIdx;
    ptr->field_0 = voiceIdx;
    func_8004E560(voiceIdx, (s32)func_80056068, (s32)ptr);
    ptr->field_8 = 1;
    return ptr;
}

void func_800562B4(GStruct57* arg0, GStruct43* arg1)
{
    GStruct43* temp_v0;

    if (arg0 != NULL) {
        temp_v0 = arg0->field_40;
        if (temp_v0 != NULL) {
            arg0->field_40    = arg1;
            arg1->field_3C    = temp_v0;
            temp_v0->field_38 = arg1;
            arg1->field_38    = NULL;
            arg1->field_34    = arg0;
            return;
        }
        arg0->field_40 = arg1;
        arg1->field_34 = arg0;
        arg1->field_3C = NULL;
        arg1->field_38 = NULL;
        return;
    }
    arg1->field_3C = NULL;
    arg1->field_38 = NULL;
    arg1->field_34 = NULL;
}

s32 func_80056308(GStruct43* arg0)
{
    s32 temp;

    temp = arg0->field_4;
    if (temp <= 0) {
        arg0->field_4 = 0;
        func_8004E71C(arg0->field_0);
        if (arg0->field_10 != 0) {
            if (arg0->field_12 == 0) {
                arg0->field_12 = 1;
            }
            goto block_8;
        }
    } else {
        if (temp <= 0x7FFFFFFE) {
            if (D_80070F68.field_124 == 1) {
                arg0->field_4 = temp + 0xFFFF6667;
            } else {
                arg0->field_4 = temp + 0xFFFF0000;
            }
        }
    block_8:
        if (arg0->field_10 != 0) {
            func_80055678(arg0);
        }
    }
    return 0;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_800563B4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800564C4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800565B8);

s32 func_8005664C(u8* arg0, s16 arg1, GStruct59* arg2)
{
    GStruct58* chunk;

    if (arg1 != -1) {
        chunk = (GStruct58*)&arg0[arg1];
        if (chunk->field_0 == 0x41656E6F) {
            arg2->field_3A = chunk->field_4;
            arg2->field_3C = chunk->field_6;
            return 1;
        }
        return -1;
    }
    return -1;
}

void func_800566A4(void)
{
    s32        i;
    s32        mask;
    s32        c600;
    s32        c500;
    s32        c100;
    GStruct54* p;
    s32        temp;

    i    = 0;
    mask = 0xF0000000;
    c600 = 0x60000000;
    c500 = 0x50000000;
    c100 = 0x10000000;
    p    = D_80082248;
    do {
        temp = p->field_0 & mask;
        if (temp != c600) {
            if ((temp == c500) || (temp == c100)) {
                p->field_16 = 0;
            }
        }
        i++;
        p++;
    } while (i < 8);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056700);
