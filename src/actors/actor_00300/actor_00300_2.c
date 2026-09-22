#include "common.h"

#include "actors/actor_100300.h"
#include "actors/actors_shared_80132074.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/gfx.h"

#include <psyq/inline_c.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

s32 SndEvt_EnqueueType6(s32 sound, s32 pan, s32 depth);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8        D_801153F4;
extern s32       D_80115720;
extern s32       D_80115728;
extern s32       D_80115744;
extern s32       D_8011573C;
extern s32       Gp_LcgState;
extern GpU16Pair Actor00300_D15FD8;

void Actor00300_Fn00078(GsCOORDINATE2* arg0, s16 arg1);
void Actor00300_Fn04528(Actor100300* arg0);
void Actor00300_Fn00E54(Actor100300* arg0);
void Actor00300_Fn01678(Actor100300* arg0);
void Actor00300_Fn019C0(Actor100300* arg0);
void Actor00300_Fn01F9C(Actor100300* arg0);
void Actor00300_Fn02620(Actor100300* arg0);
void Actor00300_Fn028D0(Actor100300* arg0);
void Actor00300_Fn02CE8(Actor100300* arg0);
void Actor00300_Fn030B8(Actor100300* arg0);
void Actor00300_Fn032BC(Actor100300* arg0);
void Actor00300_Fn0340C(Actor100300* arg0);
void Actor00300_Fn03A1C(Actor100300* arg0);
void Actor00300_Fn04A2C(Actor100300* arg0);
void Actor00300_Fn04C20(Actor100300* arg0);
void Actor00300_Fn04D28(Actor100300* arg0);
void Actor00300_Fn04E30(Actor100300* arg0);
void Actor00300_Fn04ED4(Actor100300* arg0);
void Actor00300_Fn04FB0(Actor100300* arg0);
void Actor00300_Fn05008(Actor100300* arg0);
void Actor00300_Fn0505C(Actor100300* arg0, MATRIX* arg1, s16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern GpPairSrcE           Actor00300_D15FE8;
extern Actor00300AreaConfig Actor00300_D16020[];
extern s32                  Actor00300_D16278[][2];
extern TaskDesc             Actor00300_D162F0;
extern u32                  Actor00300_D16314;
extern u32                  Actor00300_D1633C;

void Actor00300_Fn04770(Actor100300* arg0)
{
    Actor100300StateFuncTable3 sp;

    sp = Actor00300_D00004;
    sp.funcs[arg0->field_30]((Actor100300Ctx*)arg0->field_20, arg0);
}

void Actor00300_Fn047CC(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Work* work;

    work = arg1->field_1C;
    switch (D_801153F4) {
        case 0:
            arg1->field_2C->field_C            = 0;
            work->field_43C->field_2C->field_C = 0;
            arg0->field_14                     = work->field_698 != 0;
            break;
        case 1:
            Actor00300_Fn04FB0(arg1);
            Actor00300_Fn05008(arg1);
            return;
        case 2:
            arg1->field_2C->field_C            = 0x80;
            work->field_43C->field_2C->field_C = 0x80;
            arg0->field_14                     = 1;
            return;
    }
    if (gGameSession->eventState != 0) {
        Actor00300_Fn048D4(arg0, arg1);
        return;
    }
    Actor00300_Fn04958(arg0, arg1);
}

void Actor00300_Fn048D4(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Obj2C* obj;
    Actor100300Work*  work;
    s16               flags;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (gGameSession->eventState != 0) {
        flags        = ((work->field_678 & 1) == 0) << 7;
        obj->field_C = flags;
        if (work->field_678 & 2) {
            obj->field_C = flags | 4;
        }
    }
    Actor00300_Fn04ED4(arg1);
    Actor00300_Fn04FB0(arg1);
    Actor00300_Fn05008(arg1);
}

void Actor00300_Fn04958(Actor100300Ctx* arg0, Actor100300* arg1)
{
    GsCOORDINATE2*   coord;
    Actor100300Work* work;

    work  = arg1->field_1C;
    coord = arg1->field_2C->field_8;
    if (work->field_648 != 0) {
        if (arg0->field_4C != 0) {
            Actor00300_Fn04A2C(arg1);
        }
        Actor00300_Fn00E54(arg1);
        Actor00300_Fn04C20(arg1);
        if (work->field_67C != 0) {
            Actor00300_Fn032BC(arg1);
        }
        Actor00300_Fn04E30(arg1);
        Actor00300_Fn04ED4(arg1);
        if (work->field_664 != 0) {
            Actor00300_Fn0340C(arg1);
        }
        Actor00300_Fn03A1C(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Actor00300_Fn04FB0(arg1);
        Actor00300_Fn05008(arg1);
    }
}

void Actor00300_Fn04A2C(Actor100300* arg0)
{
    Actor100300Work* work;
    GpEnemy*         enemy;
    s16              damage;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->reactionFlags;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
    }
    if ((enemy->reactionFlags & 2) && (work->field_684 != 5)) {
        work->field_684 = 6;
        work->field_686 = 0;
    }
    if (enemy->reactionFlags & 0xC) {
        damage          = Gp_TickObjFlag4((GpObj5C*)enemy);
        work->field_690 = damage;
        if (damage != 0) {
            func_800DA6E8(&enemy->node, (s32)damage, 0);
            enemy->hp       = (u16)enemy->hp - (u16)work->field_690;
            work->field_684 = 5;
            work->field_686 = 0;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
}
