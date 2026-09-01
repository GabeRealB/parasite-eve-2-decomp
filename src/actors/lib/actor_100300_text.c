#include "common.h"

#include "actors/actor_100300.h"

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
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00078);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn005D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00970);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00E54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01678);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn019C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01D60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01F9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn02620);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn028D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn02CE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn030B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn032BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn0340C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03618);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03A1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03F40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn040A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04370);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04528);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04664);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04770);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn047CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn048D4);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04A2C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04B14);

void Actor00300_Fn04C20(Actor100300* arg0)
{
    Actor100300Work* work;

    work = arg0->field_1C;
    switch (work->field_684) {
        case 0:
            Actor00300_Fn01678(arg0);
            break;
        case 1:
            Actor00300_Fn019C0(arg0);
            break;
        case 2:
            Actor00300_Fn01F9C(arg0);
            break;
        case 3:
            Actor00300_Fn02620(arg0);
            break;
        case 4:
            Actor00300_Fn028D0(arg0);
            break;
        case 5:
            Actor00300_Fn02CE8(arg0);
            break;
        case 6:
            Actor00300_Fn04D28(arg0);
            break;
        case 7:
            Actor00300_Fn030B8(arg0);
            break;
        case 8:
            break;
    }

    if (work->field_684 != 3) {
        if (work->field_676 != 0) {
            if (work->field_676 > 0) {
                work->field_676 -= 0x100;
            } else if (work->field_676 < 0) {
                work->field_676 = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04D28);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04E30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04ED4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn04FB0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05008);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn0505C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05138);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05194);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn0521C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05278);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05304);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05388);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn053EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn05434);
