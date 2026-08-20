#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
#define gte_gpf12_real()  __asm__ volatile("nop; nop; .word 0x4B98003D")

extern TaskDesc       D_80113340[];
extern GpEffArg       D_80113358;
extern TaskFuncTable3 D_800977FC;
extern u16            D_80112D68[];
extern GpAnimBlk*     D_80112D6C[];
extern u16            D_80112DF4[];
extern u16            D_80113360[];
extern void*          D_80113368[];
extern s32            D_80070F60;

s32  func_8010A854(s32 arg0);
void func_80101848(GpActorWork* arg0);
void func_80101A68(GpActorWork* arg0);
void func_80101F58(GpActorWork* arg0);
void func_80102348(GpActorWork* arg0, s32 arg1);
void func_80102634(GpActorWork* arg0);
void func_801029D4(GpActorWork* arg0);
void func_80102D20(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_801038F8(GpActorWork* arg0, s32 arg1);
void func_8010397C(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_80103A18(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_80103AC0(GpActorWork* arg0);
s16  func_80103E7C(s16 arg0, s16 arg1);
void func_80103F70(GpActorWork* arg0);
void func_801040A0(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2);
s32  func_80104B54(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2);
s32  func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2);
s32  func_80105070(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3);
s32  func_801053A0(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
s32  func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_80105B0C(GpActorWork* arg0);
s32  func_80105ED4(GpActorWork* arg0);
void func_8010615C(GpActorWork* arg0);
void func_801066DC(GpActorWork* arg0, s16 arg1);
void func_80107E1C(GpActorWork* arg0);
void func_80109210(GpActorWork* arg0);
void func_80109250(GpActorWork* arg0);
void func_80109FC4(GpActorWork* arg0);
void func_8010A670(GpActorWork* arg0);
void func_80108684(GpActorWork* arg0);
void func_8010870C(GpActorWork* arg0, s32 arg1);
void func_80108770(GpActorWork* arg0, s32 arg1);
void func_80108874(GpActorWork* arg0);
void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1);
void func_80109374(GpActorWork* arg0);
void func_801093DC(GpActorWork* arg0);
void func_801095BC(s32* arg0);
void func_80109720(GpActorWork* arg0);
void func_80109844(GpActorWork* arg0);
void func_80109A1C(GpActorWork* arg0);
void func_80109BB4(GpActorWork* arg0, GpRec18* arg1);
void func_8010AA28(GpActorWork* arg0, s32 arg1);
void func_8010AAB4(GpActorWork* arg0);
void func_8010ABD4(GpActorWork* arg0);
void func_8010AC54(GpActorWork* arg0);
void func_8010AD64(GpActorWork* arg0);
void func_8010B120(GpActorWork* arg0);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_800FBAB0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F77F8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F7AD4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F7E28);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F8244);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F8A38);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F91AC);

void func_800F9474(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    GpEffWork*     spawned;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 4) {
        if (arg0->state == 0) {
            coord->sub        = mem->field_8;
            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->field_24     = ((u16)arg0->spawnArg1 * 3u) >> 4;
            temp              = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_26     = temp;
            mem->field_28     = temp << 2;
            if (((u16)mem->field_18 | (u16)mem->field_1A | (u16)mem->field_1C) == 0) {
                D_80070F60    = D_80070F60 * 5 + 0x71357911;
                mem->field_18 = (((u32)D_80070F60 >> 16) & 0xFFF) - 0x800;
                D_80070F60    = D_80070F60 * 5 + 0x71357911;
                mem->field_1A = (((u32)D_80070F60 >> 16) & 0xFFF) - 0x800;
                D_80070F60    = D_80070F60 * 5 + 0x71357911;
                mem->field_1C = (((u32)D_80070F60 >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS((SVECTOR*)&mem->field_18, (SVECTOR*)&mem->field_10);
        }
        func_80098F58(coord);
        if (D_80115740->field_4 != 0) {
            return;
        }
        if (mem->field_22 < mem->field_28) {
            goto spawn;
        }
    }
    func_800EC7E4(mem, arg0);
    return;
spawn:
    spawned = func_800EA478(0x60055, coord, 0x12200, 0);
    if (spawned != NULL) {
        gte_lddp(mem->field_24 - mem->field_22 * (mem->field_26 + 5));
        gte_ldsv(&mem->field_10);
        gte_gpf12_real();
        gte_stsv(&spawned->field_10);
    }
    mem->field_22++;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F96B0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F9FBC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FA45C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FA7CC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FAA14);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FAC40);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FB148);

void func_800FB67C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            idx;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_E;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            func_800EC7E4(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        idx           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
        mem->field_28 = D_80112C6C[idx & 3];
        arg0->state   = 1;
    }

    func_80098F58(coord);
    rgb[0] = (mem->field_24 * (((u16)mem->field_28 >> 8) & 0xF)) >> 3;
    rgb[1] = (mem->field_24 * ((u8)mem->field_28 >> 4)) >> 3;
    rgb[2] = (mem->field_24 * ((u16)mem->field_28 & 0xF)) >> 3;
    func_800EBF18(coord, mem->field_26, 0x100, rgb);

    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        func_800EC7E4(mem, arg0);
    }
}

void func_800FB7E4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    Task*          slot;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (D_80115740->field_E != 0 ||
        (((GameActorExt*)((Task*)Game_GetPtrSlot(3))->extra)->field_C & 0x80)) {
        if (D_80115740->field_E < 4) {
            return;
        }
        goto kill;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        s32 x;
        s32 y;

        D_80115740->field_12 |= 0x400;
        slot                  = Game_GetPtrSlot(3);
        parent                = (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8;
        coord->coord.t[0]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[2]     = 0;
        coord->flg            = 0;
        coord->sub            = parent + 8;
        arg0->state           = 1;
        mem->field_20         = (D_80114C08.field_0 % 10U) - 1;
        __asm__ volatile("" : "+m"(mem->field_20));
        x             = mem->field_20;
        mem->field_26 = 0x20;
        y             = mem->field_20;
        mem->field_28 = (x << 7) + 0x180;
        mem->field_2A = (y << 8) + 0x400;
    }

    func_80098F58(coord);
    if (D_80115740->field_14 != 0) {
        rgb[2] = 0xC0;
        rgb[0] = 0xC0;
        rgb[1] = 0x60;
        func_800FBAB0(coord, (s16)((u16)mem->field_28 + 0x80), (s16)((u16)mem->field_20 + 6), rgb);
        func_800EAEB8(coord, mem->field_28, rgb);
        func_800EAEB8(coord, (s16)((u16)mem->field_28 << 1), rgb);
        D_80115740->field_14 = 0;
    }

    if (D_80114C08.field_12 == 0) {
        goto kill;
    }
    if (!(D_80115740->field_12 & 0x400)) {
        goto kill;
    }
    if (D_80115740->field_16 == 1) {
        goto lcg;
    }
kill:
    func_800EC7E4(mem, arg0);
    return;
lcg:
    D_80070F60 = D_80070F60 * 5 + 0x71357911;
    if (((u32)D_80070F60 >> 16) & 3) {
        return;
    }
    slot       = Game_GetPtrSlot(3);
    D_80070F60 = D_80070F60 * 5 + 0x71357911;
    func_800EA478(0x600F4,
                  (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8 +
                      ((((u32)D_80070F60 >> 16) & 1) * 3 + 15),
                  mem->field_2A | 0x8000, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FBAB0);

void func_800FBEBC(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    Task*          slot;
    s16            flag;
    s32            y;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_E;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            goto kill;
        }
        slot = Game_GetPtrSlot(3);
        if (((GameActorExt*)slot->extra)->field_C & 0x80) {
            return;
        }
        func_80098F58(coord);
        goto draw_lcg;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_10 = 0;
        mem->field_14 = 0;
        D_80070F60    = D_80070F60 * 5 + 0x71357911;
        mem->field_12 = 0xFFF0 - (((u32)D_80070F60 >> 16) & 0x3F);
        D_80070F60    = D_80070F60 * 5 + 0x71357911;
        mem->field_24 = ((u32)D_80070F60 >> 16) & 0xFFF;
        mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        arg0->state   = 1;
        mem->field_28 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xF000;
    }

    y                 = coord->coord.t[1] + mem->field_12;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    func_80098F58(coord);
    if ((mem->field_22 & 3) == 0) {
        mem->field_20++;
    }
    if (mem->field_20 >= 8) {
        goto kill;
    }
    if (mem->field_28 & 0x8000) {
    draw_lcg:
        D_80070F60 = D_80070F60 * 5 + 0x71357911;
        func_800EB2C8(coord, (u16)mem->field_20, mem->field_26,
                      (u16)mem->field_24 | (((u32)D_80070F60 >> 16) & 0x1000));
    } else {
        func_800EB2C8(coord, (u16)mem->field_20, mem->field_26,
                      (u16)mem->field_24 | (u16)mem->field_28);
    }
    return;
kill:
    func_800EC7E4(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC0B4);

void func_800FC500(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    Task*          slot;
    s16            flag;
    s32            one;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            func_800EC7E4(mem, arg0);
        }
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        D_80115740->field_12 |= 0x800;
        slot                  = Game_GetPtrSlot(3);
        parent                = (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8;
        one                   = ONE;
        *(s32*)&coord->coord  = one;
        coord->sub            = parent + 8;
        m                     = &coord->coord;
        *(s32*)&m->m[0][2]    = 0;
        *(s32*)&m->m[1][1]    = one;
        *(s32*)&m->m[2][0]    = 0;
        m->m[2][2]            = one;
        coord->coord.t[0]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[2]     = 0;
        coord->flg            = 0;
        arg0->state           = 1;
    }

    func_80098F58(coord);
    if (D_80115740->field_14 != 0) {
        rgb[0] = 0xC0;
        rgb[1] = 0x30;
        rgb[2] = 0x60;
        func_800FBAB0(coord, 0x200, 4, rgb);
        func_800EAEB8(coord, 0x180, rgb);
        func_800EAEB8(coord, 0x300, rgb);
        D_80115740->field_14 = 0;
    }

    if ((Wip_SysConfig.field_25 & 0x80) && (D_80115740->field_12 & 0x800) &&
        (D_80115740->field_16 == 1)) {
        return;
    }
    D_80115740->field_10 &= ~0x80;
    func_800EC7E4(mem, arg0);
}

void func_800FC6C0(void)
{
    D_80115740->field_1A |= 0x80;
}

void func_800FC6E0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_800FC6F4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800977FC;
    sp.funcs[arg0->state](arg0);
}

void func_800FC74C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            i;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            SndEvt_EnqueueType7(0xFF0D, 1);
            D_80115740->field_2 = 0;
            func_800EC7E4(mem, arg0);
        }
        return;
    }

    func_80098F58(coord);
    switch (arg0->state) {
        case 0:
            if (D_80115740->field_2 == 0) {
                temp = (s8)func_800D937C((GpObj38*)coord);
                SndEvt_EnqueueType6(0xD, temp, (s8)func_800D9340((GpObj38*)coord));
            }
            D_80115740->field_2++;
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (arg0->spawnArg1 == 0) {
                func_800EA478(0x600A6, coord, 1, 0);
                arg0->state = 2;
            } else if (mem->field_24 == 0) {
                for (i = 0; i < 3; i++) {
                    func_800EA478(0x600A6, coord, arg0->spawnArg1, 0);
                }
                mem->field_24++;
            } else {
                mem->field_26++;
                if (mem->field_26 >= 9) {
                    mem->field_24 = 0;
                    mem->field_26 = 0;
                    mem->field_20++;
                    if (mem->field_20 >= arg0->spawnArg1) {
                        arg0->state = 2;
                    }
                }
            }
            break;
        case 2:
            mem->field_22++;
            if (mem->field_22 >= 0x65) {
                D_80115740->field_2--;
                if (D_80115740->field_2 <= 0) {
                    SndEvt_EnqueueType7(0xFF0D, 1);
                    D_80115740->field_2 = 0;
                }
                func_800EC7E4(mem, arg0);
            }
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC9BC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FCD00);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FD49C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FDB18);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FE034);

void func_800FE41C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        func_800EC7E4(mem, arg0);
    } else {
        if (arg0->state == 0) {
            coord->sub        = mem->field_8;
            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->field_24     = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
            temp              = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_26     = temp;
            mem->field_28     = temp << 2;
        }
        func_80098F58(coord);
        if (D_80115740->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 >= mem->field_28) {
            func_800EC7E4(mem, arg0);
        } else {
            func_800EA478(0x60070, coord, (mem->field_24 >> 2) + 0x80021400, (s32)&mem->field_10);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FE56C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FEAF8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FEFA4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FF710);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FFA8C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100020);

void func_801005D8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    s16            flag;
    s32            one;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        if (arg0->state == 0) {
            if (arg0->spawnArg1 < 0) {
                parent               = mem->field_8;
                one                  = ONE;
                *(s32*)&coord->coord = one;
                coord->sub           = parent;
                m                    = &coord->coord;
                *(s32*)&m->m[0][2]   = 0;
                *(s32*)&m->m[1][1]   = one;
                *(s32*)&m->m[2][0]   = 0;
                m->m[2][2]           = one;
                coord->coord.t[0]    = mem->field_18;
                coord->coord.t[1]    = mem->field_1A;
                coord->coord.t[2]    = mem->field_1C;
                coord->flg           = 0;
            }
            temp          = (u16)arg0->spawnArg1 & 0xFFF;
            mem->field_2A = 0;
            D_80070F60    = D_80070F60 * 5 + 0x71357911;
            mem->field_24 = temp + (((u32)D_80070F60 >> 16) & 0xFF);
            D_80070F60    = D_80070F60 * 5 + 0x71357911;
            mem->field_26 = ((u32)D_80070F60 >> 16) & 0xFFF;
            arg0->state++;
        }
        func_80098F58(coord);
        if (!(mem->field_22 & 1)) {
            func_80100784(coord, (u16)((s16)mem->field_22 >> 1), mem->field_24 | mem->field_2A,
                          mem->field_26);
        }
        if (D_80115740->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 0xC) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    func_800EC7E4(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100784);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100B78);

void func_80100E40(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GpObj*         objs[2];
    s32            dy;
    s32            i;
    s8             bits;

    actor = arg0->actor;
    coord = (GsCOORDINATE2*)arg0->extra->field_8;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x300)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
        } else {
            actor->field_992 = 0;
        }
    }

    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_EC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;
    coord->flg       = 0;
    func_80098F58(coord);
}

void func_80100FCC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    register s32           id asm("s4");
    register s32           kind asm("s5");
    register void**        scratch asm("v1");
    register s32           hi asm("v1");
    register u8*           head asm("v0");
    register void*         p asm("v0");
    register GameActor*    actor asm("s1");
    register GpObj*        obj asm("s2");
    register GpActorD4Rec* rec asm("s0");
    VECTOR*                tmp;
    GsCOORDINATE2*         src;
    Task*                  task;
    register void*         extra asm("v0");
    s16                    vz;
    s32                    scale;
    register s32           three asm("v0");
    register s32           packed asm("v1");
    register s32           flag asm("v0");

    id = arg1;
    asm("" : "+r"(id));
    kind = arg2;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(kind));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head  = *scratch;
    actor = arg0->actor;
    p     = head - 0x10;
    obj   = (GpObj*)actor->field_10C;
    rec   = (GpActorD4Rec*)actor->field_14C;
    asm("" : "+r"(obj), "+r"(rec) : "r"(p));
    *scratch = p;
    task     = actor->field_91C;
    if (task != NULL) {
        tmp                               = p;
        extra                             = task->extra;
        src                               = (GsCOORDINATE2*)((GameActorExt*)extra)->field_8;
        *(GsCOORDINATE2*)actor->field_3D4 = *src;
        Gfx_RotMatrixX(&((GsCOORDINATE2*)actor->field_3D4)->workm, 0x400, 0);
        obj->field_8                     = actor->field_3D4;
        ((GpActorSvec*)actor)->field_418 = 0;
        ((GpActorSvec*)actor)->field_41A = 0;
        ((GpActorSvec*)actor)->field_41C = 0;
        obj->field_C                     = (GpRec18*)actor->field_14C;
        __asm__ volatile("" ::: "memory");
        three            = 3;
        packed           = id << 8;
        obj->flags       = three;
        flag             = 0x20000;
        flag             = kind | flag;
        packed          |= flag;
        obj->field_10    = 0;
        obj->field_12    = 0;
        obj->field_14    = 0;
        actor->field_124 = packed;
        *tmp             = D_80112FA4[id];
        rec->field_8     = tmp->vx;
        rec->field_A     = tmp->vy;
        vz               = tmp->vz;
        rec->field_0     = rec->field_8;
        rec->field_C     = vz;
        rec->field_2     = rec->field_A;
        rec->field_4     = rec->field_C + D_80112F60[id];
        __asm__ volatile("" ::"r"(id));
        scale = 0x100;
        if (Wip_SysConfig.field_21 == 0x13) {
            scale = 0x280;
        }
        rec->field_12 = scale;
        if (kind != 0xD) {
            rec->field_10 = scale;
        } else {
            rec->field_10 = 0x900;
        }
        rec->field_14 = actor->field_32C;
        func_800E15AC(1, obj);
        func_800E18E0(rec->field_14, 6, 0);
        __asm__ volatile("" ::"r"(actor));
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

s32 func_801011D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3)
{
    void**          scratch;
    u8*             head;
    register void*  p asm("v0");
    GpDeltaScratch* s;
    s32             ret;
    s32             val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x10;
    s        = p;
    *scratch = p;
    ret      = func_800E0FEC(arg1, s, arg2, arg3);
    if (ret != 0) {
        val = ((GpDeltaScratch*)(head - 0x10))->vx.w;
        if ((val & 0xFFFF) != 0) {
            ((volatile GpDeltaScratch*)s)->vx.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        val = s->vy.w;
        if ((val & 0xFFFF) != 0) {
            s->vy.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        val = s->vz.w;
        if ((val & 0xFFFF) != 0) {
            s->vz.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        arg0->coord.t[0] += s->vx.h.hi;
        arg0->coord.t[1] += s->vy.h.hi;
        arg0->coord.t[2] += s->vz.h.hi;
        if (arg3 != NULL) {
            *arg3 = func_800E1ACC((u8*)arg3);
        }
        if ((s->vx.w | s->vz.w) == 0) {
            ret = 0;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

void func_8010133C(void)
{
    void**       scratch;
    u8*          head;
    GpScratch10* tmp;
    GpScratch10* s;
    s32          color;

    scratch    = (void**)G_SCRATCH_HEAD;
    color      = 0x808008;
    head       = *scratch;
    tmp        = (GpScratch10*)(head - 0x10);
    *scratch   = tmp;
    s          = tmp;
    s->field_8 = color;
    s->field_E = -0x58;
    for (s->field_0 = 0; s->field_0 < 2; s->field_0++) {
        s->field_4 = 0;
        s->field_C = -0x40;
        for (; s->field_4 < 3; s->field_4++) {
            s->field_C += 0x40;
            s->field_E -= 0x50;
        }
        s->field_8 = 0x37A78;
        s->field_E = 8;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_801013FC(Task* arg0)
{
    arg0->state = 3;
}

void func_80101408(GpActorWork* arg0)
{
    volatile GameActor* inner;
    Task*               task;

    inner          = arg0->actor;
    arg0->field_18 = NULL;
    D_80115760[0]  = NULL;
    task           = inner->field_914;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    func_800E1638((GpObj*)inner->field_AC);
    func_800E1638((GpObj*)inner->field_CC);
    func_800E1638((GpObj*)inner->field_EC);
    func_800E1638((GpObj*)inner->field_10C);
    func_800E1638((GpObj*)inner->field_12C);
    Task_Kill((Task*)arg0);
}

void func_801014E8(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097848;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010154C);

void func_80101848(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimRec* rec;
    s32        i;
    s32        anim;
    s32        extra;
    u16        flags;

    actor = arg0->actor;
    rec   = func_800B4668((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->pad_438 + 1);
    switch (actor->field_95C) {
        case 0:
        case 1:
            break;
        case 2:
            if (actor->field_95E != 0) {
                break;
            }
            if (func_8010583C(arg0, 0, 0, 0) != 0) {
                break;
            }
            anim              = 9;
            extra             = 5;
            i                 = 1;
            actor->field_95E += i;
            actor             = arg0->actor;
            if (i < actor->field_938) {
                do {
                    func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, anim, 0, 0, extra,
                                  actor->field_928);
                    ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 =
                        actor->field_985;
                    i++;
                } while (i < actor->field_938);
            }
            break;
        case 3:
            break;
        case 5:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_80108770(arg0, 3);
                }
            }
            break;
        case 4:
        case 6:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 7:
        case 9:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E++;
                }
            }
            break;
        case 8:
            if (rec != NULL) {
                flags = actor->field_448[1].field_0;
                if ((flags & 1) || (flags & 2)) {
                    actor->field_95E++;
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 10:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E = 0x3E8;
                }
            }
            break;
    }
}

static const s32 s_jtbl_pad = 0;

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80101A68);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80101F58);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102348);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102634);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801029D4);

void func_80102D20(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    register void** scratch asm("v0");
    register s32    hi asm("v0");
    u8*             head;
    register u8*    tmp asm("v1");
    GameActor*      actor;
    GpPitchScratch* block;
    VECTOR3*        lock;
    VECTOR3*        dest;
    GsCOORDINATE2*  src;
    GpAimRot*       rec;
    register s32    val asm("v0");
    register s32    dist asm("a1");
    register s32    cmp asm("v0");
    s32             dz;
    s32             angle;
    register s32    thresh asm("s4");

    thresh = arg2;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(thresh));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        __asm__ volatile("" : "+r"(block));
        rec           = &D_801131B4[arg1];
        src           = (GsCOORDINATE2*)((GameActorExt*)actor->field_91C->extra)->field_8;
        block->rot.vx = rec->vx;
        block->rot.vy = rec->vy;
        block->rot.vz = rec->vz;
        func_801040A0(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        func_800DAE50((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx = ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest                          = (VECTOR3*)(head - 0x34);
        dest->vy                      = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz                      = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val                           = block->delta.vx;
        val                           = ABS(val);
        val                           = val * val;
        dz                            = block->delta.vz;
        dz                            = ABS(dz);
        dz                            = dz * dz;
        val                           = SquareRoot0(val + dz);
        dist                          = val;
        block->dist                   = dist;
        cmp                           = (s16)thresh;
        if (cmp < dist) {
            block->angle = ratan2(-block->delta.vy, dist);
            angle        = block->angle - actor->field_70;
            block->angle = angle;
            if (ABS(angle) >= 0x20) {
                if (angle >= 0x31) {
                    block->angle = 0x30;
                } else if (angle < -0x30) {
                    block->angle = -0x30;
                }
                if (ABS(actor->field_70 + block->angle) < 0x281) {
                    actor->field_70 += block->angle;
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

void func_80102F10(GpActorWork* arg0)
{
    register void** scratch asm("v0");
    u8*             head;
    register u8*    tmp asm("v1");
    GameActor*      actor;
    GpPitchScratch* block;
    VECTOR3*        lock;
    VECTOR3*        dest;
    GsCOORDINATE2*  src;
    s32             val;
    s32             dz;
    s32             angle;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        __asm__ volatile("" : "+r"(block));
        src           = (GsCOORDINATE2*)((GameActorExt*)actor->field_91C->extra)->field_8;
        block->rot.vx = 0;
        block->rot.vy = 0;
        block->rot.vz = 0;
        func_801040A0(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        func_800DAE50((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx = ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest                          = (VECTOR3*)(head - 0x34);
        dest->vy                      = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz                      = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val                           = block->delta.vx;
        val                           = ABS(val);
        val                           = val * val;
        dz                            = block->delta.vz;
        dz                            = ABS(dz);
        dz                            = dz * dz;
        val                           = SquareRoot0(val + dz);
        block->dist                   = val;
        block->angle                  = ratan2(-block->delta.vy, val);
        angle                         = block->angle - actor->field_78;
        block->angle                  = angle;
        if (ABS(angle) >= 0x20) {
            if (angle >= 0x31) {
                block->angle = 0x30;
            } else if (angle < -0x30) {
                block->angle = -0x30;
            }
            if (ABS(actor->field_78 + block->angle) < 0x281) {
                actor->field_78 += block->angle;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

void func_801030CC(GpActorWork* arg0)
{
    register void**      scratch asm("v0");
    u8*                  head;
    register s32         temp asm("v1");
    register RECT*       rect asm("s1");
    GameActor*           actor;
    register GpImgRec*** table asm("a0");
    GpImgRec*            img;
    register s32         idx asm("v0");

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    temp     = (s32)(head - 8);
    *scratch = (void*)temp;
    rect     = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_80112E74;
            idx   = (s8)actor->field_987;
            temp  = Wip_SysConfig.field_26;
            idx   = idx * 4 - 5;
            idx   = idx + temp;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x4E;
                rect->w             = 0x19;
                rect->h             = 0x10;
                func_800DB28C(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_80112EB4;
            idx   = (s8)actor->field_98A;
            temp  = Wip_SysConfig.field_26;
            idx   = idx * 4 - 5;
            idx   = idx + temp;
            img   = table[idx][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 0xC;
                rect->y = 0x68;
                rect->w = 0xE;
                rect->h = 0x14;
                func_800DB28C(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

inline static Task* spawn_tmd_attach(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*         task;
    GameActor*    actor;
    GameActorExt* extra;
    GpCoordExt*   coord;
    TmdObject*    obj;
    s32*          saved;
    u8*           table;
    s32           type;

    extra = arg0->extra;
    actor = arg0->actor;
    saved = (s32*)&((GsCOORDINATE2*)extra->field_8)[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = Wip_SysConfig.field_26 - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)((GameActorExt*)task->extra)->field_8;
    coord->sub      = saved;
    coord->field_44 = 0;
    obj             = (TmdObject*)task->extra;
    if (actor->field_910 != NULL) {
        obj->field_24 = 4;
        obj->field_25 = 6;
    } else {
        obj->field_24 = 6;
        obj->field_25 = 0;
    }
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    return task;
}

Task* func_80103294(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_920 != NULL) {
        Task_Kill(actor->field_920);
    }
    actor->field_920 = spawn_tmd_attach(arg0, 0, arg1, arg2);
    if (actor->field_924 != NULL) {
        Task_Kill(actor->field_924);
    }
    actor->field_924 = spawn_tmd_attach(arg0, 1, arg1, arg2);
    return actor->field_924;
}

inline static Task* spawn_attach(Task* parent, s32 row, s32 item)
{
    s32*          saved;
    Task*         task;
    GameActorExt* extra;
    GpCoordExt*   coord;
    s32           type;

    saved = ((GameActorExt*)parent->extra)->field_8;
    if (item == 0) {
        return NULL;
    }
    type = D_80112DF4[row] - 1;
    task = Task_Spawn(7, type + item, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    extra           = (GameActorExt*)task->extra;
    task->parent    = parent;
    coord           = (GpCoordExt*)extra->field_8;
    coord->sub      = saved;
    coord->field_44 = 1;
    return task;
}

s32 func_801034C0(void)
{
    GpActorWork*  work;
    GameActor*    actor;
    Task*         parent;
    Task*         task;
    WipSysConfig* cfg;
    s32           kind;
    s32           id;
    s32           arg2;
    GameActorExt* extra;
    GpCoordExt*   coord;
    GpEffWork*    eff;
    GameActor*    inner;
    GpAnimObj*    anim;
    register s32  ret asm("v0");

    work  = Game_GetPtrSlot(3);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    parent = actor->field_924;
    if (parent == NULL) {
        goto join_4C;
    }

    task             = spawn_attach(parent, Mc_SaveData.field_22, Wip_SysConfig.field_21);
    actor->field_91C = task;
    if (task == NULL) {
        goto join_4C;
    }

    cfg = &Wip_SysConfig;
    func_80100FCC(work, cfg->field_21, cfg->field_22);
    if (actor->field_914 != NULL) {
        goto join_50;
    }

    kind  = 0x16;
    extra = (GameActorExt*)actor->field_91C->extra;
    id    = cfg->field_21;
    coord = (GpCoordExt*)extra->field_8;
    if (id != kind) {
        goto check_19;
    }
    id   = 0x80060024;
    arg2 = 0;
    goto do_call;

do_success:
    actor->field_914 = eff->field_0;
    Task_Reparent(work, eff->field_0);
    func_80106350(work, Wip_SysConfig.field_21, 0);
    goto join_50;

check_19:
    if (id != 0x19) {
        goto check_1C;
    }
    id = 0x80060029;
    goto do_call_item;

check_1C:
    if (id != 0x1C) {
        goto join_50;
    }
    id = 0x8006002A;
do_call_item:
    arg2 = cfg->field_21;
do_call:
    eff = func_800EA478(id, (GsCOORDINATE2*)coord, arg2, 0);
    if (eff != NULL) {
        goto do_success;
    }

join_4C:
join_50:
    actor->field_98F = 0;
    inner            = work->actor;
    anim             = (GpAnimObj*)work->extra;
    inner->field_93A = D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    inner->field_928 = D_80112D6C[inner->field_93A];
    func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, anim, &inner->field_7A8,
                  (GpAnimSlot*)inner->pad_438);
    func_801066DC(work, 1);
    ret                               = (s32)actor->field_91C;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    return ret;
}

Task* func_801036FC(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;

    task = Task_Spawn(7, Wip_SysConfig.field_26 + 3, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
    Task_Kill(task);
    return NULL;

have_actor:
    Game_SetPtrSlot(task, 3);
    task->idMap = (TaskIdMap*)actor;
    Mem_Set(actor, 0, 0x998);
    actor->field_93C  = arg3->field_0;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    D_80115768        = 0;
    if (arg3->field_2 != 0) {
        actor->field_954 = 2;
    }
    return task;
}

void func_80103804(GpActorWork* arg0)
{
    GameActor* actor;
    u16        buttons;
    s32        flag;

    actor            = arg0->actor;
    actor->field_974 = actor->field_973;
    actor->field_976 = actor->field_975;
    actor->field_964 = actor->field_962;
    buttons          = Game_Session->field_58;
    actor->field_978 = actor->field_977;
    actor->field_962 = buttons;
    actor->field_966 = actor->field_962 & ~actor->field_964;
    actor->field_968 = actor->field_964 & ~actor->field_962;
    flag             = 1;
    actor->field_977 = (actor->field_962 >> 6) & flag;
}

void func_80103874(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    actor->field_928 = D_80112D6C[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->pad_438);
}

void func_801038F8(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B3FA8((GpAnimCtx*)actor->field_424, i, arg1);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_8010397C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, 0, actor->field_928);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_80103A18(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, arg3, actor->field_928);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_80103AC0(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            func_800B4514((GpAnimCtx*)inner->field_424, i);
            i++;
        } while (i < inner->field_938);
    }
}

s32 func_80103B1C(void)
{
    WipSysConfig* p;
    s32           temp;
    s32           ret;

    p    = &Wip_SysConfig;
    temp = (u16)p->field_1a << 16;
    if ((temp >> 17) < p->field_18) {
        ret = 0;
    } else {
        ret = 1;
        if ((temp >> 18) >= p->field_18) {
            ret = 2;
        }
    }
    return ret;
}

void func_80103B5C(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != NULL) {
        node->field_5    = 0;
        inner->field_90C = NULL;
    }
    inner->field_97E = 1;
}

s32 func_80103B88(GpActorWork* arg0, GpDirArg* arg1)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            delta;
    register s32   temp asm("v1");
    s32            val;

    actor = arg0->actor;
    if (arg1->field_10 == 7) {
        if ((arg1->field_0 != 0) || (arg1->field_8 != 0)) {
            coord = (GsCOORDINATE2*)arg0->extra->field_8;
            delta = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            delta = delta - ratan2(arg1->field_0, arg1->field_8);
            temp  = delta;
            if ((s16)delta >= 0x802) {
                temp = delta - 0x1000;
            }
            if ((s16)temp < -0x800) {
                temp += 0x1000;
            }
            val = temp << 16;
            val = val >> 16;
            if (val < 0) {
                val = -val;
            }
            if (val < 0x400) {
                actor->field_973 = 1;
            } else {
                actor->field_973 = -1;
            }
        }
    }
    return actor->field_973;
}

void func_80103C74(GsCOORDINATE2* arg0, VECTOR3* arg1, VECTOR3* arg2)
{
    arg2->vx = arg1->vx - arg0->coord.t[0];
    arg2->vy = arg1->vy - arg0->coord.t[1];
    arg2->vz = arg1->vz - arg0->coord.t[2];
}

void func_80103CB4(GsCOORDINATE2* arg0, s32 arg1, VECTOR3* arg2, VECTOR3* arg3)
{
    void**  scratch;
    u8*     head;
    VECTOR* vec;

    scratch                      = (void**)G_SCRATCH_HEAD;
    head                         = *scratch;
    vec                          = (VECTOR*)(head - 0x10);
    *scratch                     = vec;
    ((VECTOR*)(head - 0x10))->vx = 0;
    vec->vy                      = -0x600;
    vec->vz                      = 0;
    ApplyMatrixLV(&arg0->coord, vec, vec);
    arg3->vx = arg2->vx - (arg0->coord.t[0] + ((VECTOR*)(head - 0x10))->vx);
    *scratch = (u8*)*scratch + 0x10;
    arg3->vy = arg2->vy - (arg0->coord.t[1] + vec->vy);
    arg3->vz = arg2->vz - (arg0->coord.t[2] + vec->vz);
}

s32 func_80103D8C(s32 arg0, s32 arg1)
{
    arg0 = ABS(arg0);
    arg0 = arg0 * arg0;
    arg1 = ABS(arg1);
    arg1 = arg1 * arg1;
    return SquareRoot0(arg0 + arg1);
}

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1)
{
    void**       scratch;
    u8*          head;
    VECTOR3*     vec;
    s32          vz;
    register s32 absz asm("v1");
    register s32 vx asm("a0");

    scratch                       = (void**)G_SCRATCH_HEAD;
    head                          = *scratch;
    ((VECTOR3*)(head - 0x10))->vx = arg0->vx - arg1->vx;
    vec                           = (VECTOR3*)(head - 0x10);
    vec->vy                       = arg0->vy - arg1->vy;
    vz                            = arg0->vz - arg1->vz;
    absz                          = ABS(vz);
    vec->vz                       = vz;
    absz                          = absz * absz;
    vx                            = ((VECTOR3*)(head - 0x10))->vx;
    vx                            = ABS(vx);
    vx                            = vx * vx;
    *scratch                      = vec;
    vx                            = SquareRoot0(vx + absz);
    *scratch                      = (u8*)*scratch + 0x10;
    return vx;
}

s16 func_80103E7C(s16 arg0, s16 arg1)
{
    void**          scratch;
    u8*             head;
    GpAngleScratch* block;
    register s32    tmp asm("v0");
    s32             delta;
    u16             ret;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    delta   = arg1 - arg0;
    tmp     = (s32)(head - 0xC);
    block   = (GpAngleScratch*)tmp;
    __asm__ volatile("" : "+r"(block) : "r"(tmp));
    ((GpAngleScratch*)(head - 0xC))->field_0 = delta;
    delta                                   += 0x1000;
    block->field_4                           = delta;
    tmp                                      = ((GpAngleScratch*)(head - 0xC))->field_0;
    *scratch                                 = block;
    delta                                    = tmp - 0x1000;
    block->field_8                           = delta;
    if (ABS(((GpAngleScratch*)(head - 0xC))->field_0) < ABS(block->field_4) &&
        ABS(((GpAngleScratch*)(head - 0xC))->field_0) < ABS(delta)) {
        ret = ((GpAngleScratch*)(head - 0xC))->field_0;
    } else if (ABS(block->field_4) < ABS(block->field_8)) {
        ret = block->field_4;
    } else {
        ret = block->field_8;
    }
    tmp          = (s32)G_SCRATCH_HEAD;
    *(void**)tmp = (u8*)*(void**)tmp + 0xC;
    return ret;
}

void func_80103F70(GpActorWork* arg0)
{
    GameActor*    actor;
    GpLinkNode*   node;
    WipSysConfig* p;
    s32           val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL) {
        actor->field_97E = 1;
        return;
    }
    if (node->field_4 & 1) {
        node->field_5    = 0;
        actor->field_90C = NULL;
        actor->field_97E = 1;
        return;
    }
    if ((s8)actor->field_97E == 2) {
        p = &Wip_SysConfig;
        if (p->field_21 == 0x17) {
            val = 0x200;
        } else {
            val = 0x180;
        }
        func_80102348(arg0, val);
        if (p->field_21 == 0x17) {
            func_801029D4(arg0);
        } else {
            func_80102634(arg0);
        }
    }
}

GsCOORDINATE2* func_8010403C(s32 arg0)
{
    Task* slot;
    u8    idx;

    slot = Game_GetPtrSlot(3);
    idx  = D_80112E2C[Mc_SaveData.field_22 - 1][arg0];
    return &((GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8)[idx];
}

void func_801040A0(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2)
{
    MATRIX* world;

    arg0->flg = 0;
    func_80098F58(arg0);
    arg1->workm = arg0->workm;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0tr_real();
    gte_stlvnl(arg1->workm.t);
    world = &D_80070F34;
    func_800A8864(world, &arg1->workm, &arg1->coord);
    arg1->sub = (GsCOORDINATE2*)((u8*)world - OFFSET_OF(GsCOORDINATE2, workm));
    arg1->flg = 0;
    func_80098F58(arg1);
}

s32 func_801041B4(GpActorWork* arg0)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    for (i = 0; i < 0x12; i++) {
        if ((actor->field_17C[i].field_4 & 0x100100) == 0x100000) {
            return 1;
        }
    }
    return 0;
}

void func_801041FC(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    GpPadEvt*  entry;
    u8         count;
    s32        idx;

    actor = arg0->actor;
    count = actor->field_981;
    if ((s8)actor->field_981 == 0) {
        idx = arg1 & 0xFFFF;
        asm("");
        actor->field_981 = count + 1;
        entry            = &D_80112E28[idx];
        Pad_PostEvent(0, 1, entry->field_0, entry->field_2);
    }
}

Task* func_80104258(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*         task;
    GameActor*    actor;
    GameActorExt* extra;
    GpCoordExt*   coord;
    TmdObject*    obj;
    s32*          saved;
    u8*           table;
    s32           type;

    extra = arg0->extra;
    actor = arg0->actor;
    saved = (s32*)&((GsCOORDINATE2*)extra->field_8)[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = Wip_SysConfig.field_26 - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)((GameActorExt*)task->extra)->field_8;
    coord->sub      = saved;
    coord->field_44 = 0;
    obj             = (TmdObject*)task->extra;
    if (actor->field_910 != NULL) {
        obj->field_24 = 4;
        obj->field_25 = 6;
    } else {
        obj->field_24 = 6;
        obj->field_25 = 0;
    }
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    return task;
}

Task* func_80104364(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*         task;
    s32*          saved;
    GameActorExt* extra;
    GpCoordExt*   coord;
    s32           type;

    saved = arg0->extra->field_8;
    if (arg2 == 0) {
        return NULL;
    }
    type = D_80112DF4[arg1] - 1;
    task = Task_Spawn(7, type + arg2, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra           = (GameActorExt*)task->extra;
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)extra->field_8;
    coord->sub      = saved;
    coord->field_44 = 1;
    return task;
}

s32 func_801043F4(void)
{
    GpActorWork* work;
    GameActor*   actor;
    Task*        task;

    work  = Game_GetPtrSlot(3);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_918 = NULL;
    }

    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_91C = NULL;
    }

    task = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_914 = NULL;
    }

    func_800E1638((GpObj*)actor->field_10C);
    return 1;
}

Task* func_80104490(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GsCOORDINATE2* saved;
    GameActorExt*  extra;

    saved  = (GsCOORDINATE2*)((GameActorExt*)arg0->actor->field_91C->extra)->field_8;
    arg2 <<= 2;
    arg1  += 0x60;
    task   = Task_Spawn(7, arg2 + arg1, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                                 = (GameActorExt*)task->extra;
    task->parent                          = (Task*)arg0;
    ((GsCOORDINATE2*)extra->field_8)->sub = saved;
    return task;
}

s32 func_80104508(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor*    actor;
    GpAnimObj*    extra;
    WipSysConfig* p;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 1;
    if (actor->field_928 != D_80112D6C[(s32)arg2->field_0]) {
        actor->field_928 = D_80112D6C[(s32)arg2->field_0];
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        actor->field_93A = (u16)arg2->field_0;
    }
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_801038F8(arg0, arg2->field_4);
    } else {
        func_80103A18(arg0, arg2->field_4, 0, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_80104684(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    register GameActor* actor asm("s0");
    register TmdObject* extra asm("s1");
    register void       (*func)(TmdObject*) asm("s2");
    register Task*      child asm("s3");
    register Task*      cur asm("s0");
    register s32        hi asm("v1");
    register Task*      temp asm("v0");
    u16                 flags;

    actor = arg0->actor;
    extra = (TmdObject*)arg0->extra;
    func  = NULL;
    switch (arg2) {
        case 0:
            asm("lui %0, %%hi(Tmd_AllocBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_AllocBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = (flags | 0x80) & 0xFFFB;
            break;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            break;
        case 2:
            asm("lui %0, %%hi(Tmd_FreeBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_FreeBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = flags | 0x84;
            break;
        case 3:
            extra->field_C = extra->field_C | 0x84;
            break;
        case 4:
            asm("lui %0, %%hi(Tmd_AllocBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_AllocBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = flags & 0xFF7B;
            break;
    }
    if (func != NULL) {
        func(extra);
    }
    if (actor->field_920 != NULL) {
        ((TmdObject*)actor->field_920->extra)->field_C = extra->field_C;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->field_924 != NULL) {
        ((TmdObject*)actor->field_924->extra)->field_C = extra->field_C;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->field_91C != NULL) {
        {
            register TmdObject* dest asm("v1");
            dest          = (TmdObject*)actor->field_91C->extra;
            dest->field_C = extra->field_C;
        }
        temp = actor->field_91C->firstChild;
        if (temp != NULL) {
            child                               = temp;
            ((TmdObject*)child->extra)->field_C = extra->field_C;
            cur                                 = child;
            if (func != NULL) {
                func(extra);
            }
            if (child->nextSibling != child) {
                do {
                    cur                               = cur->nextSibling;
                    ((TmdObject*)cur->extra)->field_C = extra->field_C;
                    if (func != NULL) {
                        func(extra);
                    }
                } while (cur->nextSibling != child);
            }
        }
    }
    return 0;
}

s32 func_80104838(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActorExt*  extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* next;
    u16            mode;
    VECTOR         vec;

    extra = arg0->extra;
    actor = arg0->actor;
    coord = (GsCOORDINATE2*)extra->field_8;
    mode  = actor->field_954;
    next  = coord + 1;
    if (mode != 2) {
        return 1;
    }
    if (arg2 != mode) {
        vec.vx = next->coord.t[0];
        vec.vy = next->coord.t[1];
        vec.vz = next->coord.t[2];
        ApplyMatrixLV(&coord->coord, &vec, &vec);
        coord->coord.t[0] += vec.vx;
        coord->coord.t[2] += vec.vz;
        next->coord.t[0]   = 0;
        next->coord.t[2]   = 0;
    }
    actor->field_10                   = coord->coord.t[0];
    actor->field_14                   = coord->coord.t[1];
    actor->field_18                   = coord->coord.t[2];
    actor->field_93A                  = D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    actor->field_928                  = D_80112D6C[actor->field_93A];
    actor->field_985                  = 0x10;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    if (D_801153F0.field_0 == 1) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        if (arg2 == mode) {
            func_80108770(arg0, 0);
        } else {
            func_8010870C(arg0, 0);
        }
        return 0;
    }
    if (arg2 == mode) {
        func_80108874(arg0);
        return 0;
    }
    if (arg2 == 1) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        func_801066DC(arg0, 1);
    } else {
        func_801066DC(arg0, 0);
    }
    return 0;
}

void func_80104A4C(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor       = arg0->actor;
    p           = &Wip_SysConfig;
    p->field_24 = 0;
    if (actor->field_954 != 2) {
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (actor->field_966 & 0x20) {
                    p->field_24 = 1;
                }
            }
        }
    }
}

void func_80104AAC(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
}

s32 func_80104B54(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor*    actor;
    GpAnimObj*    extra;
    WipSysConfig* p;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 1;
    actor->field_928 = arg2->field_0;
    actor->field_93A = 0x7FFF;
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        func_801038F8(arg0, arg2->field_4);
    } else {
        func_80103A18(arg0, arg2->field_4, 0, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_80104CAC(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor* actor;
    GpAnimObj* extra;
    s32        flag;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_928 = arg2->field_0;
    actor->field_93A = 0x7FFF;
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        func_801038F8(arg0, arg2->field_4);
    } else {
        func_80103A18(arg0, arg2->field_4, 0, arg2->field_C);
    }
    flag = arg2->field_10;
    if (flag == 0) {
        flag = 0x38;
    } else {
        flag = 7;
    }
    actor->field_983 = flag;
    return 0;
}

s32 func_80104D68(Task* arg0, s32 arg1, GpXformArg* arg2)
{
    GameActorExt*  extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    extra             = (GameActorExt*)arg0->extra;
    actor             = (GameActor*)arg0->idMap;
    coord             = (GsCOORDINATE2*)extra->field_8;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    coord->coord.t[2] = arg2->field_8;
    actor->field_50   = arg2->field_10;
    actor->field_52   = arg2->field_12;
    actor->field_54   = arg2->field_14;
    mtx               = &coord->coord;
    RotMatrix((SVECTOR*)&actor->field_50, mtx);
    MatrixNormal(mtx, mtx);
    coord->flg = 0;
    func_80098F58(coord);
    return 0;
}

s32 func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2)
{
    register GameActor* actor asm("s0");
    register GameActor* inner asm("s1");
    WipSysConfig*       p;
    void**              scratch;
    u8*                 head;
    s32                 val;
    s32                 mode;
    s32                 angle;
    s32                 flag;

    flag              = 2;
    scratch           = (void**)G_SCRATCH_HEAD;
    head              = *scratch;
    *scratch          = head - 0x10;
    inner             = arg0->actor;
    actor             = inner;
    p                 = &Wip_SysConfig;
    actor->field_954  = flag;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    inner->field_982     = 1;
    inner->field_956     = flag;
    inner->field_983     = 0x38;
    angle                = (u16)arg2->field_12;
    inner->field_82      = angle;
    val                  = func_80103E7C(inner->field_52, angle);
    *(s32*)(head - 0x10) = val;
    mode                 = 6;
    if (val < 0) {
        mode = 5;
    }
    func_8010397C(arg0, mode, 0);
    *scratch = (u8*)*scratch + 0x10;
    return 0;
}

s32 func_80104F5C(GpActorWork* arg0, s32 arg1, GpFacingArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;
    s32           mode;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 3;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_80  = arg2->field_0;
    actor->field_82  = arg2->field_4;
    mode             = 0x24;
    if (arg2->field_0 != 0) {
        mode = 0x25;
    }
    func_8010397C(arg0, mode, 0);
    return 0;
}

s32 func_80105070(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 4;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_20  = arg2->field_0;
    actor->field_24  = arg2->field_4;
    actor->field_28  = arg2->field_8;
    if (arg3 != NULL) {
        actor->field_93C = arg3->field_0;
        actor->field_93E = arg3->field_4;
    } else {
        actor->field_93C = 0;
        actor->field_93E = 0;
    }
    return 0;
}

s32 func_80105190(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 4;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_20  = arg2->field_0;
    actor->field_24  = arg2->field_4;
    actor->field_28  = arg2->field_8;
    if (arg3 != NULL) {
        actor->field_93C = arg3->field_0;
        actor->field_93E = arg3->field_4;
    } else {
        actor->field_93C = 0;
        actor->field_93E = 0;
    }
    actor->field_956 = 8;
    return 0;
}

s32 func_801052B8(GpActorWork* arg0, s32 arg1, GpCountArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 5;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_93E = arg2->field_0;
    actor->field_934 = arg2->field_4;
    return 0;
}

s32 func_801053A0(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    WipSysConfig*  p;

    actor = arg0->actor;
    coord = (GsCOORDINATE2*)arg0->extra->field_8;
    if (arg2->field_12 == 0) {
        p                 = &Wip_SysConfig;
        actor->field_954  = 2;
        actor->field_95E  = 0;
        actor->field_973  = 0;
        actor->field_975  = 0;
        p->field_24       = 0;
        actor->field_97E  = 0;
        actor->field_60   = 0;
        actor->field_58   = 0;
        actor->field_64   = 0;
        actor->field_5C   = 0;
        actor->field_6A   = 0;
        actor->field_68   = 0;
        actor->field_70   = 0;
        actor->field_96C  = 0;
        actor->field_12A &= 0x3FFF;
        func_80106350(arg0, p->field_21, 0);
        if (Game_Session->field_1 != 0) {
            ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
        }
        actor->field_956 = 1;
        actor->field_982 = 1;
    }
    actor->field_983   = arg2->field_10;
    coord->coord.t[0] += arg2->field_0;
    coord->coord.t[1] += arg2->field_4;
    coord->coord.t[2] += arg2->field_8;
    func_80103B88(arg0, (GpDirArg*)arg2);
    return func_801041B4(arg0);
}

s32 func_801054D8(GpActorWork* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor = arg0->actor;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956    = 6;
    D_80114C08.field_6 |= 1;
    actor->field_934    = arg2->field_14;
    actor->field_93E    = 0;
    return 0;
}

s32 func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 0xA;
    actor->field_983 = 0x38;
    return 0;
}

s32 func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 7;
    actor->field_934 = arg2;
    return 0;
}

s32 func_80105754(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;
    s32           ret;

    actor = arg0->actor;
    ret   = 0;
    if (actor->field_954 != 2) {
        p                 = &Wip_SysConfig;
        actor->field_954  = 2;
        actor->field_95E  = 0;
        actor->field_973  = 0;
        actor->field_975  = 0;
        p->field_24       = 0;
        actor->field_97E  = 0;
        actor->field_60   = 0;
        actor->field_58   = 0;
        actor->field_64   = 0;
        actor->field_5C   = 0;
        actor->field_6A   = 0;
        actor->field_68   = 0;
        actor->field_70   = 0;
        actor->field_96C  = 0;
        actor->field_12A &= 0x3FFF;
        func_80106350(arg0, p->field_21, ret);
        if (Game_Session->field_1 != 0) {
            ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
        }
        actor->field_956 = 0xB;
    } else {
        ret = 1;
    }
    return ret;
}

s32 func_80105828(GpActorWork* arg0)
{
    return arg0->actor->field_982;
}

s32 func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;
    s32        ret;

    actor = arg0->actor;
    ret   = 0;
    for (i = actor->field_938 - 1; i > 0; i--) {
        if ((actor->field_448[i].field_0 & 0x100) == 0) {
            ret = 1;
            break;
        }
    }
    return ret;
}

s32 func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;

    actor = (GameActor*)((arg1 * sizeof(GameActorSlot)) + (s32)arg0->actor);
    return (actor->field_448[0].field_0 & 0x102) == 0;
}

s32 func_801058BC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    if (arg2 <= 0) {
        arg2 = 1;
    } else if (arg2 >= 0x80) {
        arg2 = 0x7F;
    }
    i = 1;
    if (i < actor->field_938) {
        do {
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = arg2;
            i++;
        } while (i < actor->field_938);
    }
    actor->field_985 = arg2;
    return 0;
}

s32 func_80105914(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)D_80112D6C[D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 func_801059AC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        ret;
    s32        out;

    actor = arg0->actor;
    ret   = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        ret = func_8010A854((s16)func_800E2438(arg2, 0, &out, 0));
        if (ret != 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
        } else if (actor->field_910 == 0) {
            func_8010A42C(arg0, (u8)out);
        }
    }
    return ret;
}

s32 func_80105A60(Task* arg0, s32 arg1, s32 arg2)
{
    func_800B57EC((GsCOORDINATE2*)arg2, (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8);
    return 0;
}

s32 func_80105A8C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_958 = 1;
    } else {
        inner->field_958 = 3;
    }
    return 0;
}

s32 func_80105AB0(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_987 = 1;
        inner->field_98A = 2;
        inner->field_988 = 0;
        inner->field_98B = 0;
        inner->field_989 = 0;
        inner->field_98C = 0;
    } else if (arg2 < 4) {
        inner->field_987 = arg2 + 1;
        inner->field_988 = 0;
        inner->field_989 = 0;
    } else {
        inner->field_98A = arg2 - 3;
        inner->field_98B = 0;
        inner->field_98C = 0;
    }
    return 0;
}

void func_80105B0C(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            func_800B3DF4((GpAnimCtx*)inner->field_424, (GpAnimSlot*)inner->pad_438 + i);
            i++;
        } while (i < inner->field_938);
    }
}

void func_80105B74(VECTOR3* arg0)
{
    GameActor* actor;

    actor           = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_40 = arg0->vx;
    actor->field_44 = arg0->vy;
    actor->field_48 = arg0->vz;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105BC4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105ED4);

s32 func_801060E0(GpActorWork* arg0)
{
    GameActor* actor;
    u16        mode;
    s32        flags;
    s32        mask1;
    s32        mask2;

    actor = arg0->actor;
    mode  = actor->field_954;
    if (mode != 2) {
        flags = actor->field_962;
        mask1 = 8;
        mask2 = 2;
    } else {
        flags = ((PadState*)&Pad_States)->buttons;
        if (Mc_SaveData.field_1a8 == mode) {
            mask1 = 0x80;
            mask2 = 0x10;
        } else {
            mask1 = 8;
            mask2 = 2;
        }
    }
    actor->field_97F = 0;
    if (flags & mask1) {
        actor->field_97F = 1;
    } else if (flags & mask2) {
        actor->field_97F = 2;
    }
    return actor->field_97F;
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_800978BC\n"
        "D_800978BC:\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D8\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1D8\n"
        "\t.word 0x8011DDA0\n"
        "\t.word func_801065A0\n"
        "\t.word func_801065A0\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D8\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D4\n"
        "\t.word 0x8011D1D4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011DBFC\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011F724\n"
        "\t.word 0x8011E040\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011E710\n"
        "\t.word 0x8011DA34\n"
        "\t.word 0x8011D1EC\n"
        "\t.word 0x8011E4F8\n"
        "\t.word 0x8011F5D4\n"
        "\t.word 0x8011DDA4\n"
        "\t.word 0x8011DDA4\n"
        "\t.word 0x8011DDA4\n"
        ".section .text\n");
#endif

void func_8010615C(GpActorWork* arg0)
{
    GameActor*         actor;
    GpActorFuncTable33 sp;

    sp               = D_800978BC;
    actor            = arg0->actor;
    actor->field_96A = 0xF89A;
    actor->field_973 = 0;
    sp.funcs[Wip_SysConfig.field_21](arg0);
}

void func_801061F0(void)
{
    WipSysConfig* p;
    GpActorWork*  work;
    s32           flag;
    register s32  f21 asm("a1");
    s32           f22;

    work                   = Game_GetPtrSlot(3);
    p                      = &Wip_SysConfig;
    flag                   = 0x20000;
    f21                    = p->field_21;
    f22                    = p->field_22;
    work->actor->field_124 = (f21 << 8) | (f22 | flag);
}

void func_80106238(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_124 = (actor->field_124 & 0xFFFF3FFF) | (((arg1 << 1) | arg2) << 14);
}

s32 func_80106264(s32 arg0)
{
    s32 item;
    s32 ret;

    item = Wip_SysConfig.field_21 + 0x7F;
    ret  = 0;
    if (arg0 & 1) {
        ret = func_800B6DA4(item, 0);
    }
    if (arg0 & 2) {
        ret |= func_800B6DA4(item, 0x100) << 16;
    }
    return ret;
}

s32 func_801062DC(GpActorWork* arg0, s32 arg1)
{
    s32 ret;
    s32 flag;
    s32 item;

    ret  = 0;
    item = Wip_SysConfig.field_21;
    flag = arg1 != 1;
    if (func_800BB418(item + 0x7F, flag) == 1) {
        func_801088D4(arg0, flag, ret);
        ret = 1;
    }
    return ret;
}

void func_80106350(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        value;

    actor = arg0->actor;
    if (arg1 == 0x16) {
        if (actor->field_914 != NULL) {
            actor->field_914->spawnArg1 = -1;
        }
        SndEvt_EnqueueType7(0x20160003, 0);
        SndEvt_EnqueueType7(0x20160004, 0);
        SndEvt_EnqueueType7(0x20160005, 0);
    } else if (arg1 == 0x19) {
        if (actor->field_914 != NULL) {
            if (func_800B6DA4(0x98, 0x100) != 0) {
                actor->field_914->spawnArg1 = 1;
            } else {
                actor->field_914->spawnArg1 = 0;
            }
        }
    } else if (arg1 == 0x1C) {
        if (actor->field_914 != NULL) {
            if (func_800B6DA4(0x9B, 0x100) != 0) {
                value = 1;
                if (actor->field_914->spawnArg1 == 2) {
                    value = 3;
                }
                actor->field_914->spawnArg1 = value;
            } else {
                actor->field_914->spawnArg1 = (actor->field_914->spawnArg1 == 2) << 2;
            }
            if (actor->field_910 == NULL) {
                SndEvt_EnqueueType7(0x201C0005, 0);
            } else {
                SndEvt_EnqueueType7(0x40680002, 0);
            }
        }
    }
    actor->field_97E  = 1;
    actor->field_12A &= 0x3FFF;
}

void func_801064A4(GpObj38* arg0, s32 arg1, s32 arg2)
{
    s32 temp;

    temp = (s8)func_800D937C(arg0);
    SndEvt_EnqueueType6(arg1, temp, (s8)func_800D9340(arg0));
    if (arg2 == 1) {
        func_800DB500(1);
    }
}

void func_80106518(s32 arg0)
{
    register s32 cap asm("v0");
    s32          idx;

    cap = 0x1869E;
    idx = arg0 - 1;
    if (Mc_SaveData.field_888[idx] <= cap) {
        Mc_SaveData.field_888[idx]++;
    }
}

void func_80106550(GpActorWork* arg0)
{
    if (arg0->actor->field_97D & 4) {
        func_801055D4(arg0, 0, 0, 0);
    } else {
        func_80108770(arg0, 3);
    }
}

void func_801065A0(void)
{
}

void func_801065A8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        inner->field_97C = 1;
        func_8010870C(arg0, 5);
    } else if (inner->field_973 != inner->field_974 ||
               (inner->field_977 != inner->field_978 && inner->field_973 == 1)) {
        func_801066DC(arg0, 0);
    } else if (inner->field_973 == 0 && inner->field_975 != inner->field_976) {
        func_80108620(arg0);
    } else if ((inner->field_962 & 0xF000) == 0) {
        if (inner->field_942 < 0x7FFF) {
            inner->field_942++;
            if (inner->field_942 == 0x12C) {
                func_80103A18(arg0, func_80103B1C() + 0x17, 0, 5);
            }
        }
    }
}

void func_801066DC(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    temp             = inner->field_973;
    inner->field_956 = 0;
    inner->field_95C = 0;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            if (inner->field_975 == 1) {
                mode = 6;
            } else {
                mode = 5;
            }
        } else {
            mode = 1;
        }
        inner->field_958 = 0;
        inner->field_95A = 3;
    } else if ((inner->field_962 & 0x40) && (temp != -1)) {
        temp             = 1;
        inner->field_95A = temp;
        if (Mc_SaveData.field_25 == 0 && inner->field_991 == 0) {
            inner->field_958 = 3;
            mode             = 4;
        } else {
            mode             = 2;
            inner->field_958 = temp;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
        }
    } else {
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            if (Mc_SaveData.field_25 != 0 && inner->field_991 == 0) {
                inner->field_958 = 3;
                mode             = 4;
            } else {
                inner->field_958 = 1;
                mode             = 2;
                if (inner->field_91C == NULL) {
                    mode = 0x13;
                }
            }
        } else {
            inner->field_958 = 2;
            mode             = 3;
        }
    }
    inner->field_954 = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (arg1 != 0) {
        func_801038F8(arg0, mode);
    } else {
        func_80103A18(arg0, mode, 0, 4);
    }
}

INCLUDE_RODATA("gameplay/nonmatchings/3FB8", D_80097940);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_8009794C\n"
        "D_8009794C:\n"
        "\t.word func_80108FA0\n"
        "\t.word func_80108FD4\n"
        "\t.word func_80106A3C\n"
        "\t.word func_801090E8\n"
        "\t.word func_80109138\n"
        "\t.word func_80106C6C\n"
        "\t.word func_8010747C\n"
        "\t.word func_8010771C\n"
        "\t.word 0x00000000\n"
        ".section .text\n");
#endif

void func_80106838(GpActorWork* arg0)
{
    GameActor*        actor;
    GameActor*        inner;
    WipSysConfig*     p;
    u16               prev;
    GpActorFuncTable8 sp;

    sp    = D_8009794C;
    actor = arg0->actor;
    if (Wip_SysConfig.field_25 & 0x40) {
        func_8010A670(arg0);
    }
    if (actor->field_97B == 0) {
        func_80109250(arg0);
        func_80109210(arg0);
    } else {
        actor->field_973 = 0;
        actor->field_975 = 0;
    }
    p = &Wip_SysConfig;
    if (p->field_25 & 2) {
        if (actor->field_956 != 7) {
            actor->field_98E++;
            if ((s8)actor->field_98E >= 0x5A) {
                inner            = arg0->actor;
                prev             = inner->field_956;
                inner->field_954 = 0;
                inner->field_956 = 7;
                inner->field_958 = 0;
                inner->field_95A = 0;
                inner->field_95C = 0;
                inner->field_95E = 0;
                inner->field_981 = 0;
                inner->field_973 = 0;
                inner->field_975 = 0;
                inner->field_960 = prev;
                func_80103B5C(arg0);
                inner->field_12A   &= 0x3FFF;
                D_80114C08.field_6 |= 1;
                func_80106350(arg0, p->field_21, 0);
                func_80103A18(arg0, 0x19, 3, 6);
            }
        }
    }
    sp.funcs[actor->field_956](arg0);
    func_80109FC4(arg0);
    func_8010B120(arg0);
    func_80101848(arg0);
    func_80103AC0(arg0);
    func_80101F58(arg0);
    func_80101A68(arg0);
    if (Wip_SysConfig.field_18 <= 0) {
        func_8010AA28(arg0, 4);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106A3C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106C6C);

void func_8010747C(GpActorWork* arg0)
{
    register GpActorWork* work asm("s1");
    register s32          fade asm("s2");
    register s32          a3 asm("a3");
    register s32          next asm("v0");
    register s32          kind asm("v1");
    GameActor*            actor;
    GameActor*            inner;
    s32                   mode;
    s32                   temp;
    s32                   flag;
    s32                   snd;
    s32                   val;

    work             = arg0;
    actor            = work->actor;
    actor->field_973 = 0;
    if (D_80114C08.field_3 == 2) {
        actor->field_95E = 5;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95C    = 9;
            actor->field_95E   += 1;
            D_80114C08.field_6 |= 4;
            mode                = 0x1A;
            if (actor->field_93C != 0) {
                mode = 0x2A;
                if (actor->field_93C == 1) {
                    mode = 0x1D;
                }
            }
            func_80103A18(work, mode, 0, 6);
            goto do_db500_2;
        case 2:
            next             = actor->field_95E;
            kind             = actor->field_93C;
            actor->field_95C = 0;
            actor->field_95E = next + 1;
            asm volatile("" : : : "memory");
            mode = 0x1B;
            if (kind != 0) {
                mode = 0x2B;
                if (kind == 1) {
                    mode = 0x1E;
                }
            }
            func_801038F8(work, mode);
        case 3:
            if (D_80114C08.field_2 != 0) {
                goto do_db500_2;
            }
            actor->field_95C  = 9;
            actor->field_95E += 1;
            snd               = 4;
            if ((u32)(D_80114C08.field_0 - 0x12C) >= 0x12DU) {
                snd = 3;
            }
            func_800DB500(snd);
            mode = 0x1C;
            if (actor->field_93C != 0) {
                mode = 0x2C;
                if (actor->field_93C == 1) {
                    mode = 0x1F;
                }
            }
            func_801038F8(work, mode);
            break;
        case 1:
        do_db500_2:
            func_800DB500(2);
            break;
        case 4:
            break;
        case 5:
            flag = 1;
            if (actor->field_960 == 0) {
                func_801066DC(work, 0);
                break;
            }
            a3 = 8;
            if (actor->field_960 == flag) {
                a3 = 6;
            }
            temp             = 3;
            inner            = work->actor;
            val              = 2;
            fade             = a3;
            inner->field_954 = 0;
            inner->field_956 = val;
            inner->field_958 = 0;
            if (inner->field_973 != 0) {
                temp = 1;
            }
            inner->field_95A = temp;
            inner->field_95C = 0;
            inner->field_95E = 0;
            if (Wip_SysConfig.field_25 & 1) {
                func_80103B5C(work);
                inner->field_97E = flag;
            } else {
                inner->field_97E = val;
            }
            temp = inner->field_973;
            if (temp == 0) {
                if (inner->field_975 != 0) {
                    mode = 0xD;
                } else {
                    mode = 9;
                }
            } else if (temp == 1) {
                mode             = 0xC;
                inner->field_958 = 3;
                inner->field_97E = temp;
            } else {
                inner->field_958 = 2;
                mode             = 0xD;
            }
            if (fade == 0) {
                func_801038F8(work, mode);
            } else {
                func_80103A18(work, mode, 0, fade);
            }
            break;
    }
}

void func_8010771C(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor            = arg0->actor;
    actor->field_973 = 0;
    if (!(Wip_SysConfig.field_25 & 2)) {
        actor->field_95E = 1;
        actor->field_98E = 0;
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            actor->field_98E = 0xF;
        case 1:
            if (actor->field_966 & 0xF0F0) {
                actor->field_98E--;
            }
            if ((s8)actor->field_98E > 0) {
                break;
            }
            if (actor->field_960 == 0) {
                func_801066DC(arg0, 0);
                break;
            }
            inner            = arg0->actor;
            inner->field_954 = 0;
            inner->field_956 = 2;
            inner->field_958 = 0;
            if (inner->field_973 != 0) {
                temp = 1;
            } else {
                temp = 3;
            }
            inner->field_95A = temp;
            inner->field_95C = 0;
            inner->field_95E = 0;
            if (Wip_SysConfig.field_25 & 1) {
                func_80103B5C(arg0);
                inner->field_97E = 1;
            } else {
                inner->field_97E = 2;
            }
            temp = inner->field_973;
            if (temp == 0) {
                if (inner->field_975 != 0) {
                    mode = 0xD;
                } else {
                    mode = 9;
                }
            } else if (temp == 1) {
                mode             = 0xC;
                inner->field_958 = 3;
                inner->field_97E = temp;
            } else {
                inner->field_958 = 2;
                mode             = 0xD;
            }
            func_80103A18(arg0, mode, 0, 6);
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801078AC);

void func_80107E1C(GpActorWork* arg0)
{
    register u8*       tmp asm("a0");
    register s32       dx asm("v0");
    void**             scratch;
    u8*                head;
    GameActorExt*      extra;
    GpApproachScratch* block;
    GsCOORDINATE2*     coord;
    GameActor*         actor;
    s32                angle;
    s32                val;
    s32                mode;

    scratch                                      = (void**)G_SCRATCH_HEAD;
    head                                         = *scratch;
    extra                                        = arg0->extra;
    actor                                        = arg0->actor;
    tmp                                          = head - 0x14;
    coord                                        = (GsCOORDINATE2*)extra->field_8;
    block                                        = (GpApproachScratch*)tmp;
    block->vec.vx                                = actor->field_20 - coord->coord.t[0];
    *scratch                                     = block;
    block->vec.vy                                = actor->field_24 - coord->coord.t[1];
    block->vec.vz                                = actor->field_28 - coord->coord.t[2];
    angle                                        = ratan2(block->vec.vx, block->vec.vz);
    actor->field_82                              = angle;
    val                                          = func_80103E7C(actor->field_52, angle);
    ((GpApproachScratch*)(head - 0x14))->field_0 = val;
    if (val >= 0x41) {
        ((GpApproachScratch*)(head - 0x14))->field_0 = 0x40;
    } else if (val < -0x40) {
        ((GpApproachScratch*)(head - 0x14))->field_0 = -0x40;
    } else if (actor->field_95E == 0) {
        actor->field_95E = 1;
    }
    actor->field_52 = ((u16)actor->field_52 + (u16)block->field_0) & 0xFFF;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            mode             = 6;
            if (block->field_0 < 0) {
                mode = 5;
            }
            func_8010397C(arg0, mode, 1);
        case 1:
            if (block->field_0 == 0) {
                actor->field_958 = 1;
                actor->field_95E++;
                if (actor->field_93C == 0) {
                    mode = 2;
                    if (actor->field_91C == NULL) {
                        mode = 0x13;
                    }
                } else {
                    mode = actor->field_93C;
                }
                func_80103A18(arg0, mode, 0, 5);
            }
            break;
        case 2:
            dx  = coord->coord.t[0];
            dx -= actor->field_20;
            if (dx < 0) {
                dx = -dx;
            }
            if (dx < 0x69) {
                dx  = coord->coord.t[2];
                dx -= actor->field_28;
                if (dx < 0) {
                    dx = -dx;
                }
                if (dx < 0x69) {
                    actor->field_982 = 0;
                    actor->field_956 = 1;
                    mode             = 1;
                    if (actor->field_93E != 0) {
                        mode = actor->field_93E;
                    }
                    func_80103A18(arg0, mode, 0, 5);
                } else {
                    dx               = 1;
                    actor->field_973 = dx;
                    func_80101A68(arg0);
                    func_80105ED4(arg0);
                }
            } else {
                dx               = 1;
                actor->field_973 = dx;
                func_80101A68(arg0);
                func_80105ED4(arg0);
            }
            break;
    }
    func_80103AC0(arg0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108084);

void func_80108224(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor = arg0->actor;
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            func_80103A18(arg0, 0x28, 0, 6);
            break;
        case 1:
            if (func_800B4668((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->pad_438 + 1) !=
                NULL) {
                if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                    inner            = arg0->actor;
                    inner->field_954 = 0;
                    inner->field_956 = 2;
                    inner->field_958 = 0;
                    if (inner->field_973 != 0) {
                        temp = 1;
                    } else {
                        temp = 3;
                    }
                    inner->field_95A = temp;
                    inner->field_95C = 0;
                    inner->field_95E = 0;
                    if (Wip_SysConfig.field_25 & 1) {
                        func_80103B5C(arg0);
                        inner->field_97E = 1;
                    } else {
                        inner->field_97E = 2;
                    }
                    temp = inner->field_973;
                    if (temp == 0) {
                        if (inner->field_975 != 0) {
                            mode = 0xD;
                        } else {
                            mode = 9;
                        }
                    } else if (temp == 1) {
                        mode             = 0xC;
                        inner->field_958 = 3;
                        inner->field_97E = temp;
                    } else {
                        inner->field_958 = 2;
                        mode             = 0xD;
                    }
                    func_80103A18(arg0, mode, 0, 4);
                }
            }
            break;
    }
    func_80103AC0(arg0);
    func_8010B120(arg0);
}

void func_801083A0(GpActorWork* arg0)
{
    GameActor*        inner;
    GpActorFuncTable3 sp;

    sp    = D_80097940;
    inner = arg0->actor;
    func_80104A4C(arg0);
    if (inner->field_940 > 0) {
        inner->field_940--;
    }
    if ((s8)inner->field_97A > 0) {
        inner->field_97A--;
    }
    inner->field_986 = 0;
    sp.funcs[inner->field_954](arg0);
    func_80109720(arg0);
    func_801030CC(arg0);
}

void func_80108458(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    inner            = arg0->actor;
    node             = func_800DAD54(arg0);
    inner->field_973 = 0;
    if ((node != NULL && D_801153F0.field_0 < 2) || (flag = 1, D_801153F0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            func_800DB4E0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            func_80108770(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            func_80103B5C(arg0);
            func_80108874(arg0);
        }
    }
}

void func_80108568(GpActorWork* arg0)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_973 != actor->field_974) {
        func_80108770(arg0, 4);
    } else if (actor->field_973 == 0) {
        if (actor->field_975 != actor->field_976) {
            func_80108684(arg0);
        }
    }
}

void func_801085D0(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

void func_80108620(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 3;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (inner->field_975 == 0) {
        mode = 1;
    } else if (inner->field_975 == 1) {
        mode = 6;
    } else {
        mode = 5;
    }
    func_80103A18(arg0, mode, 0, 5);
}

void func_80108684(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (inner->field_973 != 0) {
        if (inner->field_973 == 1) {
            temp = 3;
        } else {
            temp = 2;
        }
        mode             = 0xD;
        inner->field_958 = temp;
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            mode = 0xC;
        }
    } else {
        mode             = 0xD;
        inner->field_95A = 3;
        if (inner->field_975 == 0) {
            mode = 9;
        }
    }
    func_80103A18(arg0, mode, 0, 5);
}

void func_8010870C(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 1;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 2;
    inner->field_95E = 0;
    inner->field_940 = 0;
    if (arg1 == 0) {
        func_801038F8(arg0, 7);
    } else {
        func_80103A18(arg0, 7, 0, arg1);
    }
}

void func_80108770(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 2;
    inner->field_958 = 0;
    if (inner->field_973 != 0) {
        temp = 1;
    } else {
        temp = 3;
    }
    inner->field_95A = temp;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (Wip_SysConfig.field_25 & 1) {
        func_80103B5C(arg0);
        inner->field_97E = 1;
    } else {
        inner->field_97E = 2;
    }
    temp = inner->field_973;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            mode = 0xD;
        } else {
            mode = 9;
        }
    } else if (temp == 1) {
        mode             = 0xC;
        inner->field_958 = 3;
        inner->field_97E = temp;
    } else {
        inner->field_958 = 2;
        mode             = 0xD;
    }
    if (arg1 == 0) {
        func_801038F8(arg0, mode);
    } else {
        func_80103A18(arg0, mode, 0, arg1);
    }
}

void func_80108874(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 3;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 2;
    inner->field_95C = 4;
    inner->field_95E = 0;
    func_80103A18(arg0, 8, 0, 6);
    func_80103B5C(arg0);
}

void func_801088D4(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    if (arg2 == 2) {
        if (func_80106264(arg1) != 0) {
            if (D_80112F1C[Wip_SysConfig.field_21][0] == 0) {
                inner->field_95E = 0x3E8;
                return;
            }
        }
        inner->field_95C = 0xA;
        mode             = 0x14;
        if (Mc_SaveData.field_13 == 1) {
            func_80166E94(Game_GetPtrSlot(0xA), 0);
        }
    } else {
        if (arg2 == 1) {
            if (inner->field_954 == 2) {
                return;
            }
        }
        inner->field_95C = 5;
        mode             = arg1 + 0xE;
    }
    inner->field_956 = 5;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95E = 0;
    inner->field_960 = arg1;
    inner->field_93E = arg2;
    func_80106350(arg0, Wip_SysConfig.field_21, 0);
    func_80103A18(arg0, mode, 0, 3);
}

void func_80108A0C(GpActorWork* arg0)
{
    GameActor*   inner;
    u16          prev;
    u16          tens;
    register s32 temp asm("v0");

    inner            = arg0->actor;
    prev             = inner->field_956;
    inner->field_956 = 6;
    inner->field_97E = 1;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_973 = 0;
    inner->field_960 = prev;
    temp             = (D_80114C08.field_0 % 100U) / 10U;
    tens             = temp;
    if (D_80114C08.field_0 >= 0x259U) {
        if (tens == 1) {
            inner->field_93C = 0;
        } else {
            inner->field_93C = 1;
        }
    } else if (tens == 3) {
        inner->field_93C = 2;
    } else if (D_80114C08.field_0 < 0x12CU) {
        inner->field_93C = 1;
    } else {
        inner->field_93C = 0;
    }
}

void func_80108AD4(GpActorWork* arg0)
{
    GameActor* inner;
    u16        prev;

    inner            = arg0->actor;
    prev             = inner->field_956;
    inner->field_954 = 0;
    inner->field_956 = 7;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_981 = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    inner->field_960 = prev;
    func_80103B5C(arg0);
    inner->field_12A   &= 0x3FFF;
    D_80114C08.field_6 |= 1;
    func_80106350(arg0, Wip_SysConfig.field_21, 0);
    func_80103A18(arg0, 0x19, 3, 6);
}

void func_80108B80(GpActorWork* arg0)
{
    func_80105B0C(arg0);
    func_80105ED4(arg0);
}

void func_80108BAC(GpActorWork* arg0)
{
    func_80103AC0(arg0);
    func_80105ED4(arg0);
}

void func_80108BD8(GpActorWork* arg0)
{
    GameActor* inner;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    inner = arg0->actor;
    cur   = inner->field_52;
    tgt   = inner->field_82;
    raw   = inner->field_82;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x41 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x41)) {
        flag             = 1;
        inner->field_52  = raw;
        inner->field_982 = 0;
        inner->field_956 = flag;
        func_80103A18(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x40) {
            delta = 0x40;
        } else if (delta < -0x40) {
            delta = -0x40;
        }
        inner->field_52 = ((u16)inner->field_52 + delta) & 0xFFF;
    }
    func_80103AC0(arg0);
}

void func_80108CC4(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
        case 1:
            func_80107E1C(arg0);
            if (inner->field_95E == 2) {
                inner->field_958 = 3;
                mode             = 4;
                if (inner->field_93C != 0) {
                    mode = inner->field_93C;
                }
                func_80103A18(arg0, mode, 0, 5);
            }
            break;
        case 2:
            func_80107E1C(arg0);
            break;
    }
}

void func_80108D68(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_93E >= inner->field_934) {
        inner->field_97A = 0x12;
        if (inner->field_95E == 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            inner->field_95E = 1;
        }
    } else if (inner->field_966 & 0xF0F0) {
        inner->field_93E++;
    }
    func_80103AC0(arg0);
}

void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        inner->field_90C = arg1;
    }
    arg1->field_5 = 1;
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_800979F8\n"
        "D_800979F8:\n"
        "\t.word func_80109170\n"
        "\t.word func_80109170\n"
        "\t.word func_80109170\n"
        "\t.word func_80109208\n"
        ".section .text\n");
#endif

void func_80108E40(GpActorWork* arg0)
{
    GpActorFuncTable4 sp;

    sp = D_800979F8;
    sp.funcs[(u16)arg0->actor->field_96C](arg0);
    func_80101848(arg0);
    func_80103AC0(arg0);
    func_80101F58(arg0);
    func_80101A68(arg0);
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_80097A08\n"
        "D_80097A08:\n"
        "\t.word func_80108B80\n"
        "\t.word func_80108BAC\n"
        "\t.word func_80108BD8\n"
        "\t.word func_801078AC\n"
        "\t.word func_80107E1C\n"
        "\t.word func_801094D4\n"
        "\t.word func_80108D68\n"
        "\t.word func_80109684\n"
        "\t.word func_80108CC4\n"
        "\t.word func_80109700\n"
        "\t.word func_80108084\n"
        "\t.word func_80108224\n"
        ".section .text\n");
#endif

void func_80108ED4(GpActorWork* arg0)
{
    GameActor*         inner;
    GpActorFuncTable12 sp;

    sp    = D_80097A08;
    inner = arg0->actor;
    sp.funcs[inner->field_956](arg0);
    func_80101F58(arg0);
    if (Wip_SysConfig.field_18 <= 0 && inner->field_956 != 0xA) {
        func_80103874(arg0);
        func_8010AA28(arg0, 4);
    }
}

void func_80108FA0(GpActorWork* arg0)
{
    func_801065A8(arg0);
    func_80109290(arg0);
    func_80105ED4(arg0);
}

void func_80108FD4(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    func_80103F70(arg0);
    inner            = arg0->actor;
    node             = func_800DAD54(arg0);
    inner->field_973 = 0;
    if ((node != NULL && D_801153F0.field_0 < 2) || (flag = 1, D_801153F0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            func_800DB4E0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            func_80108770(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            func_80103B5C(arg0);
            func_80108874(arg0);
        }
    }
}

void func_801090E8(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

void func_80109138(GpActorWork* arg0)
{
    func_8010615C(arg0);
    func_801041FC(arg0, 0);
    func_801093DC(arg0);
}

void func_80109170(GpActorWork* arg0)
{
    GameActor* inner;
    u8         kind;

    inner = arg0->actor;
    kind  = inner->field_972;
    switch (kind) {
        case 0:
        case 1:
        case 2:
        case 8:
        case 9:
        case 10:
        case 11:
            func_8010ABD4(arg0);
            break;
        case 5:
            func_8010AC54(arg0);
            break;
        case 6:
            func_80109A1C(arg0);
            break;
        case 3:
            func_8010AD64(arg0);
            break;
        case 7:
            func_80109844(arg0);
            break;
    }
}

void func_80109208(void)
{
}

void func_80109210(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0xA000) {
        if (flags & 0x8000) {
            inner->field_975 = -1;
        } else {
            inner->field_975 = 1;
        }
    } else {
        inner->field_975 = 0;
    }
}

void func_80109250(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0x5000) {
        if (flags & 0x4000) {
            inner->field_973 = -1;
        } else {
            inner->field_973 = 1;
        }
    } else {
        inner->field_973 = 0;
    }
}

s32 func_80109290(GpActorWork* arg0)
{
    GameActor*   inner;
    u16          prev;
    u16          tens;
    s32          ret;
    register s32 temp asm("v0");

    ret = 0;
    if (D_80114C08.field_3 == -2) {
        inner            = arg0->actor;
        prev             = inner->field_956;
        inner->field_956 = 6;
        inner->field_954 = 0;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 0;
        inner->field_95E = 0;
        inner->field_973 = 0;
        inner->field_97E = 1;
        inner->field_960 = prev;
        temp             = (D_80114C08.field_0 % 100U) / 10U;
        tens             = temp;
        if (D_80114C08.field_0 >= 0x259U) {
            if (tens == 1) {
                inner->field_93C = 0;
            } else {
                inner->field_93C = 1;
            }
        } else if (tens == 3) {
            inner->field_93C = 2;
        } else if (D_80114C08.field_0 < 0x12CU) {
            inner->field_93C = 1;
        } else {
            inner->field_93C = 0;
        }
        ret = 1;
    }
    return ret;
}

void func_80109374(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if ((inner->field_962 & 0x80) && (D_80114C08.field_3 == 0) && (Wip_SysConfig.field_21 != 0) &&
        (inner->field_991 == 0)) {
        inner->field_97D = 1;
    } else {
        inner->field_97D = 2;
    }
}

void func_801093DC(GpActorWork* arg0)
{
    register GameActor* inner asm("a1");
    GpLinkNode*         next;
    u16                 flags;

    inner = arg0->actor;
    if (inner->field_90C != NULL) {
        flags = inner->field_966;
        if (flags & 0x40) {
            func_80103B5C(arg0);
            return;
        }
        if (((inner->field_962 & 0x80) && (flags & 0xA000)) || (flags & 0x80)) {
            next = func_800DAD78(arg0);
            goto install;
        }
    } else if ((inner->field_966 & 0x80) && !(Wip_SysConfig.field_25 & 1)) {
        register GpActorWork* a asm("a0");

        a                = arg0;
        inner->field_97E = 2;
        next             = func_800DAD54(a);
        goto install;
    }
    return;

install: {
    register GpLinkNode* arg1 asm("a1");
    register GpLinkNode* node asm("v1");
    GameActor*           actor;
    s32                  flag;

    actor = arg0->actor;
    node  = actor->field_90C;
    arg1  = next;
    flag  = 1;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg1;
    }
    arg1->field_5 = flag;
}
}

void func_801094D4(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;
    s32        arg2;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 2;
            flag             = 1;
            inner->field_95E = flag;
            inner->field_958 = flag;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
            arg2 = 1;
            if (inner->field_934 == 0) {
                arg2 = 6;
            }
            func_8010397C(arg0, mode, arg2);
        case 1:
            if (func_80105ED4(arg0) != 0) {
                inner->field_93E--;
                if (inner->field_93E <= 0) {
                    inner->field_982 = 0;
                    inner->field_956 = 1;
                    func_80103A18(arg0, 1, 0, 5);
                }
            } else {
                inner->field_973 = 1;
                func_80101A68(arg0);
            }
            break;
    }
    func_80103AC0(arg0);
}

void func_801095BC(s32* arg0)
{
    WipSysConfig*          p;
    volatile WipSysConfig* vp;

    p = &Wip_SysConfig;
    if (p->field_21 == 0x1B) {
        *arg0 = func_800BAFE0(p->field_21 + 0x7F)->field_2 - 0x9F;
        if (*arg0 < 0) {
            *arg0 = 0xA;
        }
        *arg0 = (*arg0 - 0xA) << 24;
    } else {
        vp = p;
        if ((u32)(vp->field_22 - 0xA) < 6U) {
            *arg0 = ((vp->field_22 - 1) % 3) << 24;
            if (*arg0 < 0) {
                *arg0 = 0;
            }
        } else {
            *arg0 = 0;
        }
    }
}

void func_80109684(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 0x20;
            flag             = 1;
            inner->field_95E = flag;
            if (inner->field_934 != 0) {
                mode = 0x21;
            }
            func_8010397C(arg0, mode, 1);
        case 1:
            func_80103AC0(arg0);
            break;
    }
    func_8010B120(arg0);
}

void func_80109700(GpActorWork* arg0)
{
    func_80103AC0(arg0);
}

void func_80109720(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    u16            flags;
    s16            delta;
    s32            val;
    s32            temp;

    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    actor        = arg0->actor;
    coord[4].flg = 0;
    flags        = actor->field_962;
    if ((flags & 0xA000) && (actor->field_954 == 0)) {
        if (flags & 0x8000) {
            delta = -0x20;
        } else {
            delta = 0x20;
        }
        if (ABS(actor->field_6A + delta) < 0x1A1) {
            actor->field_6A += delta;
        }
    } else if (actor->field_6A != 0) {
        val   = actor->field_6A >> 3;
        delta = val;
        temp  = val;
        if (ABS(temp) < 0x40) {
            val = 0x40;
            if (temp < 0) {
                val = -0x40;
            }
            delta = val;
        }
        actor->field_6A -= delta;
        if (ABS(actor->field_6A) < 0x41) {
            actor->field_6A = 0;
        }
    }
}

void func_80109818(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 4;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 5;
    inner->field_95E = 0;
    inner->field_981 = 0;
}

void func_80109844(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GameActor*     inner;
    GameActor*     inner2;
    GpEffArg*      params;
    GsCOORDINATE2* coord;
    s32            idx;
    s32            temp;
    s32            val;

    inner    = arg0->actor;
    temp     = (u16)((u16)inner->field_96E / 12);
    idx      = 2;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    params   = &D_80113358;
    head    -= 8;
    *scratch = head;
    vec      = (SVECTOR*)head;
    if (temp < 3) {
        idx = temp;
    }
    temp = idx;
    __asm__ volatile("" : "+r"(temp) : "r"(head), "r"(vec));
    switch (inner->field_95E) {
        case 0:
            inner->field_95E   = 1;
            coord              = (GsCOORDINATE2*)((GpObj*)inner->field_AC)[(s8)inner->field_993].field_8;
            params->field_4    = (temp * 0x20) + 0x120;
            params->field_6    = temp + 1;
            D_80113358.field_0 = coord;
            inner->field_934   = 0;
            inner->field_93E   = temp;
            /* fallthrough */
        case 1:
            if (inner->field_934 == 0) {
                idx = 5;
                if (temp < 3) {
                    idx = 6;
                }
                inner->field_93E--;
                if (inner->field_93E == 0) {
                    inner->field_95E++;
                } else {
                    inner->field_934 = 6;
                }
                vec->vx = 0;
                val     = 0;
                if ((s8)inner->field_993 == 0) {
                    val = -0x190;
                }
                vec->vy = val;
                vec->vz = 0;
                func_800FDB18(idx, params->field_0, vec, params);
            } else {
                inner->field_934--;
            }
            break;
        case 2:
            break;
        case 3:
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_80109A1C(GpActorWork* arg0)
{
    GameActor*     inner;
    GpEffArg*      params;
    GsCOORDINATE2* coords;
    s32            idx;
    s32            temp;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            inner->field_95E = 1;
            inner->field_934 = 0;
            inner->field_93E = 0;
            /* fallthrough */
        case 1:
            if (inner->field_934 == 0) {
                params = &D_80113358;
                inner->field_93E++;
                if (inner->field_93E == 3) {
                    inner->field_95E++;
                } else {
                    inner->field_934 = 6;
                }
                coords          = &((GsCOORDINATE2*)arg0->extra->field_8)[inner->field_93E + 1];
                params->field_0 = coords;
                temp            = (u16)((u16)inner->field_96E / 12);
                idx             = 2;
                if (temp < 3) {
                    idx = temp;
                }
                temp            = idx;
                params->field_4 = (temp * 0x60) + 0xC0;
                params->field_6 = temp + 1;
                func_800FDB18(3, coords, 0, params);
            } else {
                inner->field_934--;
            }
            break;
        case 2:
            break;
        case 3:
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109BB4);

void func_80109FC4(GpActorWork* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        temp;
    s32        mode;

    flags = Wip_SysConfig.field_25;
    actor = arg0->actor;
    if (flags != 0) {
        if (flags & 1) {
            temp             = (u16)actor->field_944 - 1;
            actor->field_944 = temp;
            if ((s16)temp <= 0) {
                flags &= ~1;
            }
        }
        if (flags & 2) {
            temp             = (u16)actor->field_946 - 1;
            actor->field_946 = temp;
            if ((s16)temp <= 0) {
                flags &= ~2;
            }
        }
        if (flags & 4) {
            temp             = actor->field_98D - 1;
            actor->field_98D = temp;
            if ((s8)temp <= 0) {
                func_8010A854(1);
                mode = (u16)actor->field_958;
                if (mode == 0) {
                    actor->field_98D = 0x78;
                } else if (mode == 3) {
                    actor->field_98D = 0x14;
                } else {
                    actor->field_98D = 0x3C;
                }
            }
            temp             = (u16)actor->field_948 - 1;
            actor->field_948 = temp;
            if ((s16)temp <= 0) {
                flags &= ~4;
            }
        }
        if (flags & 0x10) {
            temp             = (u16)actor->field_94A - 1;
            actor->field_94A = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x10;
            }
        }
        if (flags & 0x20) {
            temp             = (u16)actor->field_94C - 1;
            actor->field_94C = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x20;
            }
        }
        if (flags & 0x40) {
            temp             = (u16)actor->field_94E - 1;
            actor->field_94E = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x40;
            }
        }
        if (flags & 0x80) {
            if ((u32)((u8)D_80114C08.field_A - 2) >= 2U) {
                temp             = (u16)actor->field_950 - 1;
                actor->field_950 = temp;
                if ((s16)temp <= 0) {
                    flags &= ~0x80;
                }
            }
        }
        Wip_SysConfig.field_25 = flags;
    }
}

void func_8010A1B0(s32 arg0, s32 arg1)
{
    GpActorWork* work;
    GameActor*   inner;
    s32          mask;

    mask = arg1;
    if (arg0 == 0) {
        work = Game_GetPtrSlot(3);
        if (arg1 & 1) {
            inner = work->actor;
            if (func_800B9D80(0x101) == 0) {
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
                func_800EC9C8();
                func_80103B5C(work);
                func_800ECA10(1);
            }
        }
        if (mask & 2) {
            inner = work->actor;
            if (func_800B9D80(0x102) == 0) {
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
                func_8010B210(work);
                func_800ECA10(2);
            }
        }
        if (mask & 4) {
            inner = work->actor;
            if (func_800B9D80(0x104) == 0) {
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
                func_800ECA10(4);
            }
        }
        if (mask & 0x10) {
            inner = work->actor;
            if (func_800B9D80(0x108) == 0) {
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                func_800ECA10(0x10);
            }
        }
        if (mask & 0x20) {
            inner = work->actor;
            if (func_800B9D80(0x110) == 0) {
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                func_800ECA10(0x20);
            }
        }
        if (mask & 0x40) {
            inner = work->actor;
            if (func_800B9D80(0x120) == 0) {
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                func_800ECA10(0x40);
            }
        }
        if (mask & 0x80) {
            inner = work->actor;
            if (func_800B9D80(0x140) == 0) {
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                func_800ECA10(0x80);
                func_800ECA54();
            }
        }
    } else {
        Wip_SysConfig.field_25 &= ~arg1;
    }
}

void func_8010A42C(GpActorWork* arg0, s32 arg1)
{
    u8 kind;

    kind = arg1;
    if (kind != 0) {
        switch (kind) {
            case 0:
                break;
            case 1: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x101) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
                func_800EC9C8();
                func_80103B5C(arg0);
                func_800ECA10(1);
                break;
            }
            case 2: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x102) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
                func_8010B210(arg0);
                func_800ECA10(2);
                break;
            }
            case 3: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x104) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
                func_800ECA10(4);
                break;
            }
            case 4:
                func_800ECA10(8);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x108) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                func_800ECA10(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x110) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                func_800ECA10(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x120) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                func_800ECA10(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x140) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                func_800ECA10(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

void func_8010A670(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         left;
    s32         right;
    s32         pad;
    s32         bits;
    s32         timer;
    s32         next;
    s32         mode;
    s32         dir;

    inner            = arg0->actor;
    timer            = inner->field_990 - 1;
    inner->field_990 = timer;
    if ((s8)timer == 0) {
        left             = 0x8000;
        next             = (rand() & 0x1F) + 0xA;
        pad              = inner->field_962;
        inner->field_990 = next;
        bits             = pad & 0xF000;
        if (bits == left || bits == (right = 0x2000)) {
            if (!(inner->field_970 & 0x5000)) {
                if (rand() & 1) {
                    dir = 0x4000;
                } else {
                    dir = 0x1000;
                }
                inner->field_970 = dir;
            }
        } else {
            bits = pad & 0x5000;
            if (bits) {
                if (rand() & 4) {
                    inner->field_970 &= 0xAFFF;
                } else if (rand() & 1) {
                    inner->field_970 = left;
                } else {
                    inner->field_970 = right;
                }
            }
        }
        if (D_801153F0.field_0 == 1) {
            if (inner->field_90C != NULL) {
                if (rand() & 3) {
                    func_80103B5C(arg0);
                }
            } else {
                mode = inner->field_956;
                if (mode == 2 && !(Wip_SysConfig.field_25 & 1) && (rand() & 3)) {
                    node = func_800DAD54(arg0);
                    if (node != NULL) {
                        inner->field_97E = mode;
                        func_80108E0C(arg0, node);
                    }
                }
            }
        }
    }
    if (Game_Session->field_58 & 0xF000) {
        inner->field_962 |= inner->field_970;
    }
}

s32 func_8010A854(s32 arg0)
{
    register s32  amount asm("s1");
    s32           ret;
    register s32  tmp asm("v0");
    WipSysConfig* p;
    Task*         slot;
    s32*          coords;

    amount = arg0;
    ret    = 0;
    if (func_800B9D80(0x40000) != 0) {
        tmp    = (s16)arg0 >> 2;
        amount = arg0 - tmp;
    }
    if (func_800B9D80(0x800) != 0) {
        Wip_SysConfig.field_1c += (s16)amount / 5;
        if (Wip_SysConfig.field_1e < Wip_SysConfig.field_1c) {
            Wip_SysConfig.field_1c = Wip_SysConfig.field_1e;
        }
    }
    if (func_800B9D80(0x200) != 0) {
        p = &Wip_SysConfig;
        if (p->field_18 >= 5 && (s16)amount >= p->field_18) {
            slot        = Game_GetPtrSlot(3);
            coords      = ((GameActorExt*)slot->extra)->field_8;
            p->field_18 = 1;
            func_800EA478(0x6009C, (GsCOORDINATE2*)coords + 1, 5, 0);
            return 0;
        }
    }
    Wip_SysConfig.field_18 -= amount;
    if (Wip_SysConfig.field_18 > 0) {
        return ret;
    }
    if (Game_Session->field_1 != 0) {
        Wip_SysConfig.field_18 = 1;
    } else {
        ret = 1;
        Display_AcquireRef();
    }
    return ret;
}

void func_8010A9D0(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    func_8010AAB4(arg0);
    if ((u16)inner->field_96C == 1) {
        mode = 0x10;
    } else {
        mode = 0x11;
    }
    func_80103A18(arg0, mode, 0, 3);
}

void func_8010AA28(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 1;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_96C = 3;
    if (arg1 == 0) {
        func_801038F8(arg0, 0x12);
    } else {
        func_80103A18(arg0, 0x12, 0, arg1);
    }
    func_80103B5C(arg0);
    inner->field_983 |= 0x18;
}

void func_8010AAB4(GpActorWork* arg0)
{
    GameActor*    inner;
    WipSysConfig* p;

    p                   = &Wip_SysConfig;
    inner               = arg0->actor;
    D_80114C08.field_6 |= 1;
    func_80106350(arg0, p->field_21, 0);
    if (p->field_18 > 0) {
        inner->field_954 = 1;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 7;
        inner->field_95E = 0;
        inner->field_973 = 0;
        func_8010A854(inner->field_96E);
        inner->field_12A &= 0x3FFF;
        if ((s8)inner->field_97E == 2) {
            inner->field_97E = 1;
        }
        func_8010A42C(arg0, inner->field_972);
    }
}

void func_8010AB70(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    if (inner->field_956 != 0) {
        func_8010870C(arg0, 0xC);
    } else {
        func_801066DC(arg0, 0);
    }
}

void func_8010ABD4(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_95E != 0) {
        if (inner->field_95E == 1) {
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        }
    }
}

void func_8010AC54(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;

    inner = arg0->actor;
    if (inner->field_95E == 0) {
        inner->field_95E = 1;
        inner->field_934 = 0;
        inner->field_93E = 0;
    }
    if (inner->field_934 == 0) {
        inner->field_93E++;
        if (inner->field_93E == 3) {
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        } else {
            inner->field_934 = 5;
        }
        func_800EA478(
            0x600E0, &((GsCOORDINATE2*)arg0->extra->field_8)[4 - inner->field_93E], 0x320, 0);
    } else {
        inner->field_934--;
    }
}

void func_8010AD64(GpActorWork* arg0)
{
    void**              scratch;
    register u8*        head asm("t1");
    SVECTOR*            vec;
    register GameActor* inner asm("a0");
    GameActor*          inner2;
    GpEffArg*           params;
    GsCOORDINATE2*      coord;
    s32                 val;
    s32                 idx;

    scratch = (void**)G_SCRATCH_HEAD;
    __asm__ volatile("" : "+r"(scratch));
    head     = *scratch;
    params   = &D_80113358;
    vec      = (SVECTOR*)(head - 8);
    *scratch = vec;
    inner    = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            idx                     = (s8)inner->field_993;
            inner->field_95E        = 1;
            coord                   = (GsCOORDINATE2*)((GpObj*)inner->field_AC)[idx].field_8;
            params->field_4         = 0xC0;
            params->field_6         = 2;
            D_80113358.field_0      = coord;
            ((SVECTOR*)head)[-1].vx = 0;
            val                     = 0;
            if ((s8)inner->field_993 == 0) {
                val = -0x190;
            }
            vec->vy = val;
            vec->vz = 0;
            func_800FDB18(2, D_80113358.field_0, vec, params);
            break;
        case 1:
            break;
        case 2:
            asm("" ::: "memory");
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_8010AE98(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x101) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 1;
    inner->field_944        = 0x258;
    func_800EC9C8();
    func_80103B5C(arg0);
    func_800ECA10(1);
}

void func_8010AF04(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x102) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 2;
    inner->field_946        = 0x258;
    inner->field_98E        = 0;
    func_8010B210(arg0);
    func_800ECA10(2);
}

void func_8010AF6C(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x104) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 4;
    inner->field_948        = 0x258;
    inner->field_98D        = 0;
    func_800ECA10(4);
}

void func_8010AFC0(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x108) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x10;
    inner->field_94A        = 0x258;
    func_800ECA10(0x10);
}

void func_8010B010(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x110) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x20;
    inner->field_94C        = 0x258;
    func_800ECA10(0x20);
}

void func_8010B060(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x120) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x40;
    inner->field_94E        = 0x258;
    inner->field_990        = (rand() & 0x1F) + 0xA;
    inner->field_970        = 0;
    func_800ECA10(0x40);
}

void func_8010B0C8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x140) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x80;
    inner->field_950        = 0x258;
    func_800ECA10(0x80);
    func_800ECA54();
}

void func_8010B120(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;
    GpObj38*   obj;
    s32        mode;
    s32        snd;
    s32        temp;
    s32        temp2;

    inner = arg0->actor;
    obj   = (GpObj38*)arg0->extra->field_8;
    if (Mc_SaveData.field_5C2 != 0) {
        return;
    }
    if ((s8)inner->field_97A != 0) {
        return;
    }
    func_80109BB4(arg0, inner->field_17C);
    if ((u16)inner->field_96C == 0) {
        return;
    }
    inner2 = arg0->actor;
    func_8010AAB4(arg0);
    mode = 0x11;
    if ((u16)inner2->field_96C == 1) {
        mode = 0x10;
    }
    func_80103A18(arg0, mode, 0, 3);
    temp  = (s8)func_800D937C(obj);
    temp2 = (s8)func_800D9340(obj);
    snd   = 7;
    if ((u16)inner->field_96C == 1) {
        snd = 6;
    }
    SndEvt_EnqueueType6(snd, temp, temp2);
}

void func_8010B210(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_96C = 0;
    inner->field_972 = 0;
    inner->field_96E = 0;
}

s32 func_8010B228(s32 arg0)
{
    WipSysConfig* p;
    u16           saved18;
    u16           saved1c;
    s32           out;
    s32           ret;

    p           = &Wip_SysConfig;
    saved18     = p->field_18;
    saved1c     = p->field_1c;
    ret         = func_8010A854((s16)func_800E2438(arg0, 0, &out, 0));
    p->field_18 = saved18;
    p->field_1c = saved1c;
    if (ret != 0) {
        Display_ReleaseRef();
    }
    return ret;
}

void func_8010B2A0(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_80113340, arg0, arg1, 0);
}

void func_8010B2D4(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    s32        out;
    s32        flag;

    inner = arg0->actor;
    flag  = inner->field_910 != 0;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        arg2             = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_96E = func_800E2438(arg1->field_4, 0, &out, flag);
        inner->field_972 = out;
    }
}

void func_8010B348(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    u32        kind;

    inner = arg0->actor;
    kind  = (u16)arg1->field_4;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        if (kind == 2) {
            goto case24;
        }
        if (kind < 3) {
            goto do_call;
        }
        if (kind == 3) {
            goto case3;
        }
        if (kind != 4) {
            goto do_call;
        }
    case24:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 5;
        goto do_call;
    case3:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 0;
    do_call:
        inner->field_96E = func_800E2CD4(arg1->field_4, 0);
    }
}

void func_8010B3F8(Task* arg0)
{
    Task*          slot;
    GpEffArg*      params;
    GsCOORDINATE2* coords;
    s32            idx;
    u16            count;
    s16            next;

    slot = Game_GetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0;
            /* fallthrough */
        case 1:
            count = arg0->killCountdown;
            if ((count & 0xF) == 0) {
                next                = count + 0x100;
                params              = &D_80113358;
                arg0->killCountdown = next;
                idx                 = arg0->spawnArg1 & 3;
                if (next >= 0x300) {
                    Task_Kill(arg0);
                } else {
                    arg0->killCountdown = next | 6;
                }
                coords          = (GsCOORDINATE2*)slot->extra;
                count           = arg0->killCountdown;
                coords          = (GsCOORDINATE2*)((GameActorExt*)coords)->field_8;
                params->field_4 = (idx * 0x60) + 0xC0;
                params->field_6 = idx + 1;
                coords          = &coords[((count & 0xF00) >> 8) + 1];
                params->field_0 = coords;
                func_800FDB18(3, coords, 0, params);
            } else {
                arg0->killCountdown = count - 1;
            }
            break;
    }
}

void func_8010B520(Task* arg0)
{
    s32*           raw;
    Task*          slot;
    GameActorExt*  extra;
    GpEffArg*      params;
    GsCOORDINATE2* coords;

    params          = &D_80113358;
    slot            = Game_GetPtrSlot(3);
    extra           = slot->extra;
    raw             = extra->field_8;
    params->field_4 = 0xC0;
    coords          = &((GsCOORDINATE2*)raw)[3];
    params->field_0 = coords;
    params->field_6 = (u16)arg0->spawnArg1 + 1;
    func_800FDB18(2, coords, 0, params);
    Task_Kill(arg0);
}

void func_8010B590(Task* arg0)
{
    GameActorExt* extra;
    GpCoordExt*   coord;

    extra = (GameActorExt*)arg0->extra;
    coord = (GpCoordExt*)extra->field_8;
    arg0->state++;
    coord->flg = 0;
    if (coord->field_44 != 0) {
        extra->field_C = 0;
    }
}

void func_8010B5C0(Task* arg0)
{
    Task*         parent;
    GameActorExt* extra;

    parent          = arg0->parent;
    extra           = (GameActorExt*)arg0->extra;
    extra->field_C  = ((GameActorExt*)parent->extra)->field_C;
    *extra->field_8 = 0;
}

void func_8010B5E4(Task* arg0)
{
    arg0->state = 3;
}

void func_8010B5F0(Task* arg0)
{
    Task_Kill(arg0);
}

void func_8010B610(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097AB0;
    sp.funcs[arg0->state](arg0);
}

void func_8010B674(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    GameActor* next;
    GpAnimObj* extra;
    Task*      task;

    actor = arg0->actor;
    task  = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_91C = NULL;
        extra            = (GpAnimObj*)arg0->extra;
        inner            = arg0->actor;
        inner->field_93A = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
        inner->field_928 = D_80113368[inner->field_93A];
        func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, extra, &inner->field_7A8,
                      (GpAnimSlot*)inner->pad_438);
        func_801038F8(arg0, 1);
        next            = arg0->actor;
        next->field_954 = 0;
        next->field_956 = 0;
        next->field_958 = 0;
        next->field_95A = 0;
        next->field_95C = 0;
        next->field_95E = 0;
        next->field_942 = 0;
        next->field_93E = 0;
        next->field_973 = 0;
        next->field_975 = 0;
        func_80103A18(arg0, 1, 0, 4);
    }
    task = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_914 = NULL;
    }
}

s32 func_8010B79C(void)
{
    GpActorWork* work;
    GameActor*   actor;
    GameActor*   inner;
    GameActor*   next;
    Task*        task;
    McSaveData*  save;
    GpActorD4*   block;
    s16          val1;
    s16          val2;
    GpEffWork*   eff;
    GpAnimObj*   extra;
    register s32 ret asm("v0");

    work  = Game_GetPtrSlot(0xA);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    if (actor->field_924 != NULL) {
        save             = &Mc_SaveData;
        task             = func_80104364((GpActorWork*)actor->field_924, save->field_13 + 1, save->field_5C7, 0);
        actor->field_91C = task;
        if (task != NULL) {
            block = actor->field_910;
            val1  = D_80167218[save->field_5C7];
            val2  = D_80167224[save->field_5C7];
            func_80100FCC(work, val1, val2);
            actor->field_124 |= 0x80;
            block->field_CD   = D_80167230[save->field_5C7];
            if ((u8)save->field_5C7 == 4 && actor->field_914 == NULL) {
                eff = func_800EA478(
                    0x80060180, (GsCOORDINATE2*)((GameActorExt*)actor->field_91C->extra)->field_8, val1, 0);
                if (eff != NULL) {
                    actor->field_914 = eff->field_0;
                    func_80106350(work, val1, 0);
                }
            }
        }
    }

    inner            = work->actor;
    extra            = (GpAnimObj*)work->extra;
    inner->field_93A = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    inner->field_928 = D_80113368[inner->field_93A];
    func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, extra, &inner->field_7A8,
                  (GpAnimSlot*)inner->pad_438);
    next            = work->actor;
    next->field_954 = 0;
    next->field_956 = 0;
    next->field_958 = 0;
    next->field_95A = 0;
    next->field_95C = 0;
    next->field_95E = 0;
    next->field_942 = 0;
    next->field_93E = 0;
    next->field_973 = 0;
    next->field_975 = 0;
    func_801038F8(work, 1);
    ret              = (s32)actor->field_91C;
    actor->field_983 = 7;
    return ret;
}

void func_8010B9A4(GpActorWork* arg0)
{
    GameActor*  actor;
    McSaveData* save;
    s32         field13;
    u16         temp;
    u16         anim;

    actor            = arg0->actor;
    actor->field_954 = 1;
    actor->field_95C = 7;
    save             = &Mc_SaveData;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95E = 0;
    actor->field_960 = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    if (save->field_5C2 == 0 && (field13 = save->field_13) == 1) {
        temp            = save->field_6C8 - actor->field_96E;
        save->field_6C8 = temp;
        if ((s16)temp <= 0 && Game_Session->field_1 != 0) {
            save->field_6C8 = field13;
        }
    }
    actor->field_12A &= 0x3FFF;
    if ((s8)actor->field_97E == 2) {
        actor->field_97E = 1;
    }
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    anim = 0x11;
    if ((u16)actor->field_96C == 1) {
        anim = 0x10;
    }
    func_80103A18(arg0, anim, 0, 3);
}

Task* func_8010BAC8(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3)
{
    Task*          task;
    GameActor*     actor;
    GpActorD4*     block;
    GsCOORDINATE2* coord;
    s32            type;

    if (arg1 == 1) {
        type = Mc_SaveData.field_5C7 + 0x7F;
    } else {
        type = arg1 + 0x82;
    }
    task = Task_Spawn(7, type, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
fail:
    Task_Kill(task);
    return NULL;

have_actor:
    block = Mem_Calloc(0xD4, 0);
    if (block == NULL) {
        goto fail;
    }
    Game_SetPtrSlot(task, 0xA);
    Mem_Set(actor, 0, 0x998);
    Mem_Set(block, 0, 0xD4);
    task->idMap      = (TaskIdMap*)actor;
    actor->field_910 = block;
    func_800A9DF0(task);
    actor->field_93C  = *arg3;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    return task;
}

void func_8010BC04(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    inner->field_93E = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    if (arg1 != 0) {
        func_801038F8(arg0, 1);
    } else {
        func_80103A18(arg0, 1, 0, 4);
    }
}

s32 func_8010BC70(GsCOORDINATE2* arg0)
{
    void**        scratch;
    u8*           head;
    VECTOR3*      vec;
    GameActorExt* extra;
    s32           ret;

    extra    = (GameActorExt*)((Task*)Game_GetPtrSlot(3))->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    func_80103C74(arg0, (VECTOR3*)((GsCOORDINATE2*)extra->field_8)->coord.t, vec);
    ret      = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

s16 func_8010BCF4(Task* arg0, VECTOR3* arg1)
{
    void**              scratch;
    u8*                 head;
    VECTOR3*            vec;
    GameActorExt*       extra;
    register GameActor* actor asm("s2");
    s16                 ret;

    extra    = (GameActorExt*)arg0->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    actor    = (GameActor*)arg0->idMap;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, vec);
    ret      = func_80103E7C(actor->field_52, ratan2(((VECTOR3*)(head - 0x10))->vx, vec->vz));
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

void func_8010BD88(GpActorWork* arg0, VECTOR3* arg1)
{
    void**         scratch;
    u8*            head;
    GpTurnScratch* vec;
    GameActorExt*  extra;
    GameActor*     actor;
    s32            val;

    extra   = arg0->extra;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vec = *scratch = (GpTurnScratch*)(head - 0x14);
    actor          = arg0->actor;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, (VECTOR3*)vec);
    vec->angle = ratan2(((GpTurnScratch*)(head - 0x14))->vx, vec->vz);
    val        = func_80103E7C(actor->field_52, vec->angle);
    vec->angle = val;
    if (val > 0x40) {
        vec->angle = 0x40;
    } else if (val < -0x40) {
        vec->angle = -0x40;
    }
    actor->field_52         = (actor->field_52 + vec->angle) & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_8010BE5C(GpActorWork* arg0, VECTOR3* arg1)
{
    void**         scratch;
    u8*            head;
    GpAimScratch*  block;
    GsCOORDINATE2* coord;
    SVECTOR*       rot;
    GameActorExt*  extra;
    GameActor*     actor;
    s32            val;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    extra   = arg0->extra;
    actor   = arg0->actor;
    coord   = (GsCOORDINATE2*)(head - 0x50);
    rot     = (SVECTOR*)(head - 0x58);
    extra   = (GameActorExt*)extra->field_8;
    block = *scratch = (GpAimScratch*)(head - 0x68);
    block->rot.vx    = 0;
    block->rot.vy    = 0;
    block->rot.vz    = 0;
    func_801040A0((GsCOORDINATE2*)extra + 4, coord, rot);
    func_80103C74(coord, arg1, (VECTOR3*)block);
    val = ratan2(((VECTOR3*)(head - 0x68))->vx, block->vec.vz) - actor->field_52;
    val = func_80103E7C(actor->field_6A, val);
    if (val > 0x20) {
        val = 0x20;
    } else if (val < -0x20) {
        val = -0x20;
    }
    if (ABS(actor->field_6A + val) < 0x1A0) {
        actor->field_6A += val;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x68;
}

void func_8010BF7C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    arg0->actor->field_910->field_C4 = arg1 + (arg2 & rand());
}

void func_8010BFCC(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    actor->field_928 = D_80113368[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->pad_438);
}

s32 func_8010C058(void)
{
    s32 ret;

    if (((s16)Mc_SaveData.field_6CA >> 1) < (s16)Mc_SaveData.field_6C8) {
        ret = 0;
    } else if (((s16)Mc_SaveData.field_6CA >> 2) >= (s16)Mc_SaveData.field_6C8) {
        ret = 2;
    } else {
        ret = 1;
    }
    return ret;
}

void func_8010C098(GpActorWork* arg0, s32 arg1)
{
    GameActor*  actor;
    GpLinkNode* node;
    s32         val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL || (node->field_4 & 1)) {
        actor->field_90C = NULL;
        actor->field_97E = 1;
    } else if ((s8)actor->field_97E == 2) {
        if (arg1 & 1) {
            val = 0;
            if (arg1 != 1) {
                val = 0x380;
            }
            func_80102348(arg0, val);
        }
        if (arg1 & 2) {
            if (D_80113388[Mc_SaveData.field_5C7] != 0) {
                func_80102634(arg0);
            } else {
                func_80102D20(arg0, D_80167218[Mc_SaveData.field_5C7], 0x380);
            }
        }
    }
}

void func_8010C180(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* actor;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    actor            = arg0->actor;
    actor->field_954 = 0;
    actor->field_956 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    func_80103A18(arg0, 1, 0, 4);
}

void func_8010C1FC(GpActorWork* arg0, SVECTOR3* arg1, s32 arg2)
{
    GsCOORDINATE2* dest;
    GsCOORDINATE2* src;
    GpActorD4*     block;
    GpObj*         obj;
    GpActorD4Rec*  rec;
    s16            vz;

    block         = arg0->actor->field_910;
    src           = (GsCOORDINATE2*)arg0->extra->field_8;
    obj           = (GpObj*)block->field_68;
    rec           = &block->field_88;
    dest          = (GsCOORDINATE2*)block->field_18;
    *dest         = *src;
    obj->field_8  = block->field_18;
    obj->field_14 = -0xA0;
    obj->field_18 = 0x60000;
    obj->field_C  = (GpRec18*)rec;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->flags    = 3;
    rec->field_8  = arg1->vx;
    rec->field_A  = arg1->vy;
    vz            = arg1->vz;
    rec->field_4  = arg2;
    rec->field_0  = rec->field_8;
    rec->field_12 = 0x80;
    rec->field_10 = 0x80;
    rec->field_14 = &block->field_A0;
    rec->field_C  = vz;
    rec->field_2  = rec->field_A;
    func_800E15AC(1, obj);
    func_800E18E0(rec->field_14, 1, 0);
    obj->flags |= 0xC800;
}

s32 func_8010C30C(GpActorWork* arg0)
{
    GameActorExt*  extra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* next;
    GameActor*     actor;
    VECTOR         vec;
    void*          prev;
    void*          anim;
    s32            changed;

    extra  = arg0->extra;
    coord  = (GsCOORDINATE2*)extra->field_8;
    actor  = arg0->actor;
    next   = coord + 1;
    vec.vx = next->coord.t[0];
    vec.vy = next->coord.t[1];
    vec.vz = next->coord.t[2];
    ApplyMatrixLV(&coord->coord, &vec, &vec);
    coord->coord.t[0] += vec.vx;
    coord->coord.t[2] += vec.vz;
    next->coord.t[0]   = 0;
    next->coord.t[2]   = 0;
    actor->field_10    = coord->coord.t[0];
    actor->field_14    = coord->coord.t[1];
    actor->field_18    = coord->coord.t[2];
    prev               = actor->field_928;
    actor->field_93A   = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    anim               = D_80113368[actor->field_93A];
    changed            = prev != anim;
    actor->field_928   = anim;
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->pad_438);
    actor->field_985 = 0x10;
    actor->field_983 = 7;
    actor->field_95E = 0;
    actor->field_960 = 0;
    func_8010BC04(arg0, changed);
    return 0;
}

void func_8010C46C(GpActorWork* arg0)
{
    GameActor* actor;

    actor             = arg0->actor;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
}

s32 func_8010C4F0(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    actor->field_956 = 1;
    if (actor->field_928 != D_80113368[(s32)arg2->field_0]) {
        actor->field_928 = D_80113368[(s32)arg2->field_0];
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        actor->field_93A = (u16)arg2->field_0;
    }
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_801038F8(arg0, arg2->field_4);
    } else {
        func_80103A18(arg0, arg2->field_4, 1, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_8010C648(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104B54(arg0, arg1, arg2);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C688(GpActorWork* arg0, s32 arg1, GpXformArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104E00(arg0, arg1, arg2);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C6C8(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80105070(arg0, arg1, arg2, arg3);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C708(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    WipSysConfig* p;
    u8            saved;
    GameActor*    actor;

    p     = &Wip_SysConfig;
    actor = arg0->actor;
    saved = p->field_24;
    func_80105070(arg0, arg1, arg2, arg3);
    p->field_24      = saved;
    actor->field_956 = 8;
    return 0;
}

s32 func_8010C75C(GpActorWork* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor* actor;

    actor = arg0->actor;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    actor->field_956 = 6;
    actor->field_934 = arg2->field_14;
    actor->field_93E = 0;
    return 0;
}

void func_8010C81C(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_801053A0(arg0, arg1, arg2);
    p->field_24 = saved;
}

s32 func_8010C858(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)D_80113368[D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 func_8010C8F0(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    s32 ret;

    ret = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        Mc_SaveData.field_6C8 -= func_800E2438(arg2, 0, 0, 1);
        if ((s16)Mc_SaveData.field_6C8 <= 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            ret = 1;
        }
    }
    return ret;
}

void func_8010C980(void* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5)
{
    arg1->field_8  = arg0;
    arg1->field_C  = arg2;
    arg1->field_10 = 0;
    arg1->field_12 = 0;
    arg1->field_14 = 0;
    arg1->flags    = 1;
    arg1->field_18 = arg4 | 0x30000;
    arg1->field_1C = arg5;
    func_800E15AC(2, arg1);
    arg1->flags |= 0x8000;
    func_800E18E0(arg1->field_C, (s16)arg3, 0);
}
