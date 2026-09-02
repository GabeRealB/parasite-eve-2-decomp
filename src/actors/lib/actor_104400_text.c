#include "common.h"
#include "actors/actor_104400.h"
#include "main/task.h"
#include "main/tmd.h"

s16 Actor04400_Fn06618(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00220);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn006A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00874);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00B24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00D3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00F7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01418);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01584);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn017B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01CA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01E08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02008);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0216C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn022A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02B8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02D18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02E8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0304C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn031B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03390);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03538);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn039EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03B34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03CA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03E20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn03F8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn042C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn045A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn04718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn048A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn04A3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn04BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn04D44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn04EDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn05040);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn05260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn053FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn058F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn05A40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn05DE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn05FC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn061B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn062D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06328);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06374);

void Actor04400_L063DC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn063E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0648C);

void Actor04400_L064E4(void)
{
}

void Actor04400_Fn064EC(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06520);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn065F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06618);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn066DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0674C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn067A0);

void Actor04400_Fn06834(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn06848(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn0685C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06870);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn068F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06964);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn069D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06A24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06A78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06ACC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06B50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06BC4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06BF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06C70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06CF0);

void Actor04400_Fn06D90(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (Actor04400_Fn06618(arg0)) {
        if (work->field_44F == 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06DFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06F50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07050);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0710C);

void Actor04400_L07144(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0714C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn071C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0723C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07360);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn073C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07404);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07530);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn075F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn076D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07750);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0781C);

void Actor04400_L07870(void)
{
}

void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07890);

void Actor04400_L078CC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn078D4);

void Actor04400_Fn07968(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07984);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07A38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07C60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07CF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07D78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07F04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07F6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07FD0);

void Actor04400_L0808C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08094);

void Actor04400_L080E0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn080E8);

void Actor04400_L08158(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08160);

void Actor04400_L08200(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08208);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0823C);

void Actor04400_L08288(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08290);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn082E0);

void Actor04400_L08350(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08358);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn083CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0847C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08610);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn087E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08870);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08908);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn089C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08A40);

void Actor04400_Fn08A9C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08AA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08B3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08C08);

void Actor04400_L08C5C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08C64);

void Actor04400_Fn08DA4(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

s32 Actor04400_Fn08DBC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
