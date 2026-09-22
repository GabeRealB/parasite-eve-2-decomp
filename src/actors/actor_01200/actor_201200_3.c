#include "common.h"
#include "actors/actor_201200.h"
#include "actors/actor_201200_motion.h"
#include "actors/actors_shared_8014a7b0.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "psyq/inline_c.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
s16  Actor01200_Fn00130(GsCOORDINATE2* coord, GpRec18* rec, s32 n, SVECTOR* d);
s32  Actor01200_Fn0067C(GsCOORDINATE2* coord, GpRec18* rec, s32 n);
void Actor01200_Fn00820(Actor201200* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void ActorsShared8014c738(Actor201200Ctx* arg0, Actor201200* arg1);
void ActorsShared8014db78(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn03DC0(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn01040(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn03E78(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn01234(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn017DC(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn01FDC(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn02BE8(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn03294(Actor201200Ctx* arg0, Actor201200* arg1);
void Actor01200_Fn03F30(Actor201200Ctx* arg0, Actor201200* arg1);

extern u8  D_801153F4;
extern s32 Actor01200_D04044;
extern s32 Actor01200_D04050;

/// Patrol between the two `patrol` points: turn at most 0x20 toward the current
/// one, step 5 units, and swap points within 400 units or after 0x60 blocked
/// frames; state 6 when `Actor01200_Fn00130` reports 1, state 4 when the
/// camera target is within 2000 units and inside a quarter turn or 1000 units.
void Actor01200_Fn02BE8(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work*        work;
    Actor201200TurnScratch* head;
    Actor201200TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    TmdObject*              obj;
    s16                     angle;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->patrolIdx     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        Actor01200_Fn00820(arg1);
        work->field_6 = 0;
        return;
    }
    head                                      = *(Actor201200TurnScratch**)G_SCRATCH_HEAD;
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                        = head - 1;
    head[-1].d.vx                             = work->patrol[work->patrolIdx].vx - arg1->field_2C->coords->coord.t[0];
    sc->d.vy                                  = 0;
    sc->d.vz                                  = work->patrol[work->patrolIdx].vz - arg1->field_2C->coords->coord.t[2];
    coord                                     = arg1->field_2C->coords;
    angle                                     = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                                 = Actor201200_NormalizeYaw(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-arg1->field_2C->coords->coord.m[2][0], arg1->field_2C->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->coords->coord, sc->angle, 1);
    Actor201200_StepForward(arg1->field_2C->coords, 5);
    if (Actor01200_Fn0067C(arg1->field_2C->coords, &work->rec1B8, 5)) {
        work->field_6++;
    }
    if (!Actor201200_OutOfRange(&sc->d, 400) || work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    if (Actor01200_Fn00130(arg1->field_2C->coords, &work->rec250, 5, &sc->d) == 1) {
        work->field_0 = 6;
    }
    target   = arg1->field_2C->coords;
    sc->d.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!Actor201200_OutOfRange(&sc->d, 2000)) {
        coord = arg1->field_2C->coords;
        angle = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (Actor201200_NormalizeYaw(angle) < 0x400 || !Actor201200_OutOfRange(&sc->d, 1000)) {
            work->field_0 = 4;
        }
    }
    Actor01200_Fn00820(arg1);
    arg1->field_2C->coords->flg = 0;
    if ((work->field_58 & 2) && work->field_17C > 0x14) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 1;
        }
    }
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Walk back toward the spawn point: turn at most 0x10 toward it, step 8 units,
/// and hand over to state 7 once within 0x50 or after 0xDD frames (state 6 when
/// `Actor01200_Fn00130` reports 1).
void Actor01200_Fn03294(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    TmdObject*              obj;
    Actor201200TurnScratch* head;
    Actor201200TurnScratch* s;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        Actor01200_Fn00820(arg1);
        work->field_3DC = 0;
        work->field_6   = 0;
        return;
    }
    head                                      = *(Actor201200TurnScratch**)G_SCRATCH_HEAD;
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD = head - 1;
    s                                         = head - 1;
    Actor01200_Fn00820(arg1);
    arg1->field_2C->coords->flg = 0;
    head[-1].d.vx               = work->origin.vx - arg1->field_2C->coords->coord.t[0];
    s->d.vy                     = 0;
    s->d.vz                     = work->origin.vz - arg1->field_2C->coords->coord.t[2];
    coord                       = arg1->field_2C->coords;
    s->angle                    = Actor201200_NormalizeYaw(ratan2(head[-1].d.vx, s->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    facing    = arg1->field_2C->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->coords->coord, s->angle, 1);
    Actor201200_StepForward(arg1->field_2C->coords, 8);
    Actor01200_Fn0067C(arg1->field_2C->coords, &work->rec1B8, 5);
    work->field_6++;
    if (!Actor201200_OutOfRange(&s->d, 0x50) || work->field_6 >= 0xDD) {
        work->field_0 = 7;
    }
    if (Actor01200_Fn00130(arg1->field_2C->coords, &work->rec250, 5, &s->d) == 1) {
        work->field_0 = 6;
    }
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD += 1;
}

const Actor201200StateTable Actor01200_D000E4 = {
    {
        ActorsShared8014db78,
        Actor01200_Fn03DC0,
        Actor01200_Fn01040,
        Actor01200_Fn03E78,
        Actor01200_Fn01234,
        Actor01200_Fn017DC,
        Actor01200_Fn01FDC,
        Actor01200_Fn02BE8,
        Actor01200_Fn03294,
        Actor01200_Fn03F30,
    }
};

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `D_801153F4`, dispatches the substate handler and plays its sound.
void Actor01200_Fn036B0(Actor201200Ctx* arg0, Actor201200* arg1)
{
    VECTOR                pos;
    Actor201200StateTable table;
    Actor201200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                        = arg1->field_1C;
    table                       = Actor01200_D000E4;
    arg1->field_2C->coords->flg = 0;
    Gp_UpdateCoord(arg1->field_2C->coords);
    pos.vx = arg1->field_2C->coords->workm.t[0];
    pos.vy = arg1->field_2C->coords->workm.t[1];
    pos.vz = arg1->field_2C->coords->workm.t[2];
    Gp_UpdateActorColor((struct GpEnemy*)arg0, &pos, 0, 0);
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
        case 2:
            arg1->field_2C->flags = 0x80;
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (arg0->field_40 > 0) {
        ActorsShared8014c738(arg0, arg1);
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(&work->rec1B8);
    Gp_ClearRec18Occupied(&work->rec250);
    Gp_ClearRec18Occupied(&work->rec2E8);
    id = ActorsShared8014a7b0((ActorsShared8014a7b0Work*)work);
    if (id != 0) {
        snd = id | ((arg0->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan(arg1->field_2C->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(arg1->field_2C->coords));
    }
    if (work->field_3D8 != 0) {
        func_800D7A9C(arg1->field_2C, (VECTOR*)arg1->field_2C->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        arg1->field_2C->coords->flg = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_01200/actor_201200_3", ActorsShared80135df4Table);
