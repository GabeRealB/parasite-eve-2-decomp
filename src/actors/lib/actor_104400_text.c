#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

void Actor04400_Fn006A8(Task* arg0);
s16  Actor04400_Fn06618(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00220);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn006A8);

/// Same body as `ActorsShared801639a8`.
void Actor04400_Fn00874(Task* arg0)
{
    SVECTOR                       rot;
    ActorsShared801639a8Mat       mtx;
    ActorsShared801639a8MatWords* ident;
    Actor104400Work*              work;
    GsCOORDINATE2*                coords;
    MATRIX*                       m5;
    MATRIX*                       m4;
    MATRIX*                       m3;

    work   = (Actor104400Work*)arg0->idMap;
    ident  = &mtx.ident;
    coords = ((TmdObject*)arg0->extra)->field_8;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]   = (u16)mtx.mat.m[0][0];
    m5->m[0][1]   = (u16)mtx.mat.m[0][1];
    m5->m[0][2]   = (u16)mtx.mat.m[0][2];
    m5->m[1][0]   = (u16)mtx.mat.m[1][0];
    m5->m[1][1]   = (u16)mtx.mat.m[1][1];
    m5->m[1][2]   = (u16)mtx.mat.m[1][2];
    m5->m[2][0]   = (u16)mtx.mat.m[2][0];
    m5->m[2][1]   = (u16)mtx.mat.m[2][1];
    m5->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[5].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]   = (u16)mtx.mat.m[0][0];
    m4->m[0][1]   = (u16)mtx.mat.m[0][1];
    m4->m[0][2]   = (u16)mtx.mat.m[0][2];
    m4->m[1][0]   = (u16)mtx.mat.m[1][0];
    m4->m[1][1]   = (u16)mtx.mat.m[1][1];
    m4->m[1][2]   = (u16)mtx.mat.m[1][2];
    m4->m[2][0]   = (u16)mtx.mat.m[2][0];
    m4->m[2][1]   = (u16)mtx.mat.m[2][1];
    m4->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[4].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]   = (u16)mtx.mat.m[0][0];
    m3->m[0][1]   = (u16)mtx.mat.m[0][1];
    m3->m[0][2]   = (u16)mtx.mat.m[0][2];
    m3->m[1][0]   = (u16)mtx.mat.m[1][0];
    m3->m[1][1]   = (u16)mtx.mat.m[1][1];
    m3->m[1][2]   = (u16)mtx.mat.m[1][2];
    m3->m[2][0]   = (u16)mtx.mat.m[2][0];
    m3->m[2][1]   = (u16)mtx.mat.m[2][1];
    m3->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[3].flg = 0;
}

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

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 Actor04400_Fn063E4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0648C);

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

/// Same body as `ActorsShared8016974c`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
s16 Actor04400_Fn06618(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn06658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn066DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0674C);

/// Same body as `ActorsShared801698d4`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn067A0(Task* arg0, s32 step)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}

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

void Actor04400_Fn06BF8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work            = (Actor104400Work*)arg0->idMap;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0714C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn071C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0723C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07360);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn073C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07404);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07530);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn075F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn076D0);

void Actor04400_Fn07750(Task* arg0)
{
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    Actor104400Work* objWork;

    enemy->field_54 = 0;

    objWork = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0781C);

void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn07890);

void Actor04400_Fn078D4(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    Actor04400_Fn006A8(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    work            = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor104400Work*)arg0->idMap;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08094);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn080E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08160);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08208);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0823C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08290);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn082E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08358);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn083CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0847C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08610);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn087E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08870);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08908);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn089C0);

void Actor04400_Fn08A40(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;

    work                                  = (Actor104400Work*)arg0->idMap;
    ((GpEnemy*)arg0->spawnArg2)->field_54 = 0;
    work2                                 = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn08A9C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08AA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08B3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn08C08);

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
