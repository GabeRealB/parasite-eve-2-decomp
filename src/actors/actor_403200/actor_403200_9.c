#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_403200.h"
#include "actors/actor_403200_view.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern MATRIX* D_80073B8C;
/// Global freeze flag: 1 while the game is halted, which stops the per-frame
/// body below from walking its model out.
extern u8 D_80072729;
/// The script pair the per-frame body's two one-shot sound cues spawn.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;
/// Non-zero while the overlay is shutting down, which is what makes the spawn
/// state below tear its enemy down instead of standing it up.
extern s16 D_actor_403200_80141C50;
/// LCG state the spawn state below rolls a random yaw out of.
extern u32 Gp_LcgState;

/// Non-zero while the game is paused; only the byte is read here.
extern u8 D_801153F4;

/// Equipped character and weapon variant, which pick the player's current
/// animation block out of `Gp_PlayerAnimBlkTbl`.
extern s8         D_8007218A;
extern u8         D_80073BA9;
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

/// Set while the grab's animation is installed on the player; the player's
/// side clears it on release.
extern s32 D_actor_403200_80141C54;

/// Reply buffer the rise state passes with its message 0x3F8.
extern Actor403200Msg3F8 D_actor_403200_8015F900;

/// Animation-set table the grab states send the player as message 0x3FF;
/// entry 2 is refreshed from the player's own weapon block.
extern GpAnimSet* D_actor_403200_8015E710[];

/// Drop-point group the falling enemies use this round, rerolled whenever a
/// spawn arrives with `spawnArg1` 0.
extern u8 D_actor_403200_8015E70C;
/// Per-`spawnArg1` offset from the host model to the point the falling enemy
/// is stood up at.
extern SVECTOR D_actor_403200_8015E780[];
/// The drop points: `vz` is added to the ring x coordinate and `vx` (less
/// 0x189C) becomes the z coordinate.
extern SVECTOR D_actor_403200_8015E7C0[];
/// `[group][spawnArg1]` index into `D_actor_403200_8015E7C0`.
extern u8 D_actor_403200_8015E840[][8];

/// The head of a `Gp_PlayerAnimBlkTbl` entry as the hold state reads it: an
/// array of animation-set pointers, of which it copies entry 9.
typedef struct Actor403200AnimTable {
    /* 0x00 */ GpAnimSet* sets[10];
} Actor403200AnimTable;
STATIC_ASSERT_SIZEOF(Actor403200AnimTable, 0x28);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static __inline__ void Actor403200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->coords;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if (view == 0x21) {
                value = 0x20;
                flag  = dist < 0x189D;
                if (flag) {
                    value = 0x21;
                }
                return value;
            }
            value = 0x21;
            flag  = dist < 0x1770;
            if (!flag) {
                value = 0x20;
            }
            return value;
        case 1:
            if ((view != 9) && (view != 10)) {
                value = 9;
                flag  = dist < 0x27D8;
            } else {
                flag = view;
                if (flag == 9) {
                    value = 0xA;
                    flag  = dist < 0x27D9;
                    if (flag) {
                        value = 9;
                    }
                    return value;
                }
                if (flag == 10) {
                    value = 9;
                    flag  = dist < 0x24EA;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xA;
            }
            return value;
        case 2:
            return 0x1B;
    }
    return 1;
}

s32 func_actor_403200_80134374(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->coords;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
        case 1:
            if ((view != 7) && (view != 8)) {
                value = 7;
                flag  = dist < 0x26AC;
            } else {
                flag = view;
                if (flag == 7) {
                    value = 8;
                    flag  = dist < 0x26AD;
                    if (flag) {
                        value = 7;
                    }
                    return value;
                }
                if (flag == 8) {
                    value = 7;
                    flag  = dist < 0x2328;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 8;
            }
            return value;
        case 2:
            return 0x1A;
    }
    return 1;
}

s32 func_actor_403200_801344C4(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    Task*          task;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    task   = (Task*)gameGetPtrSlot(3);
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->coords;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            SOFT_TOUCH_REG(flag);
            if (flag == 0x25) {
                flag  = ((TmdObject*)task->extra)->coords->coord.t[0];
                value = 0x1E;
                flag  = flag < 0x4651;
                if (flag) {
                    value = 0x25;
                }
                goto done;
            }
            flag  = ((TmdObject*)task->extra)->coords->coord.t[0];
            value = 0x25;
            flag  = flag < 0x4268;
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

s32 func_actor_403200_80134748(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->coords;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 5) && (view != 6)) {
                value = 5;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 5) {
                    value = 6;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 5;
                    }
                    return value;
                }
                if (flag == 6) {
                    value = 5;
                    flag  = dist < 0x2198;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 6;
            }
            return value;
        case 1:
            if ((view != 0xB) && (view != 0xC)) {
                value = 0xB;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 0xB) {
                    value = 0xC;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 0xB;
                    }
                    return value;
                }
                if (flag == 0xC) {
                    value = 0xB;
                    flag  = dist < 0x1A90;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xC;
            }
            return value;
        case 2:
            return 0x1C;
    }
    return 1;
}

s32 func_actor_403200_80134900(Task* arg0, s16 arg1)
{
    SVECTOR        pos;
    SVECTOR*       p;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            flag;
    s32            value;
    s32            view;

    view   = Gp_GetViewIndex() & 0xFF;
    p      = &pos;
    coords = ((TmdObject*)arg0->extra)->coords;
    p->vx  = D_80073B8C->t[0] - coords->coord.t[0];
    p->vy  = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = pos.vx * pos.vx;
    p->vz  = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += pos.vy * pos.vy;
    dist   = SquareRoot0(dist + (pos.vz * pos.vz));
    switch (arg1) {
        case 0:
        case 1:
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                SOFT_BARRIER();
                flag = dist < 0x1E5A;
            } else if (flag == 0x25) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                value = 0x25;
                flag  = dist < 0x1B58;
            }
            if (!flag) {
                value = 0x19;
            }
            return value;
    }
    return 1;
}

s32 func_actor_403200_80134A14(Task* arg0, s16 arg1)
{
    SVECTOR               vec;
    Actor403200ViewPoints tab;
    Task*                 obj;
    s32                   dist;
    s32                   value;
    s32                   view;
    s32                   flag;

    view   = Gp_GetViewIndex() & 0xFF;
    obj    = (Task*)gameGetPtrSlot(3);
    tab    = D_actor_403200_80131E64;
    vec.vx = ((TmdObject*)obj->extra)->coords->coord.t[0] - tab.v[arg1].vx;
    dist   = vec.vx * vec.vx;
    vec.vy = ((TmdObject*)obj->extra)->coords->coord.t[1] - tab.v[arg1].vy;
    dist  += vec.vy * vec.vy;
    vec.vz = ((TmdObject*)obj->extra)->coords->coord.t[2] - tab.v[arg1].vz;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            SOFT_TOUCH_REG(flag);
            if (flag == 0x25) {
                flag  = ((TmdObject*)obj->extra)->coords->coord.t[0];
                value = 0x1E;
                flag  = flag < 0x4651;
                if (flag) {
                    value = 0x25;
                }
                goto done;
            }
            flag  = ((TmdObject*)obj->extra)->coords->coord.t[0];
            value = 0x25;
            flag  = flag < 0x4268;
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

/// The enemy's walk-out state: a reset request re-arms the block (the two
/// 0xEF4 counters, the 0x7B0 pose flag, the 0xEFA re-arm flag, pose 2 and the
/// 0xE96 yaw target), then the per-frame body runs and the animation frame the
/// mask leaves is tested against 0x12 and 0x18 -- each one-shot cue spawning a
/// script and a type-6 sound with the enemy's pan and half its depth, once per
/// arrival -- before being latched into `field_7D8`. Unless the game is frozen
/// the model is then stepped 0x19/0x1000 forward along its own facing, its
/// `flg` cleared, and once it has run out to x 0x1CCA in state 0 or 0x2882 in
/// state 1 the step advances and re-arms `field_0`.
void func_actor_403200_80134D40(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   model;
    s16              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;

    if (work->field_4 != 0) {
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_7B3 = 2;
        work->field_7B0 = 1;
        work->field_EFE = 0;
        work->field_E96 = 0xE74;
    }

    SCRATCH_SP -= 0xC;
    func_actor_403200_80133DD8(arg0);

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(gpGetObjDepth(((TmdObject*)arg0->extra)->coords) / 2));
    }

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(gpGetObjDepth(((TmdObject*)arg0->extra)->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].curRec & 0x3FF;

    model = ((TmdObject*)arg0->extra)->coords;
    if (D_80072729 != 1) {
        Actor403200_StepForward(model);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;

    switch (work->field_F08) {
        case 0:
            if (((TmdObject*)arg0->extra)->coords->coord.t[0] >= 0x1CCA) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
        case 1:
            if (((TmdObject*)arg0->extra)->coords->coord.t[0] >= 0x2882) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
    }

    if (work->field_6 > 0) {
        work->field_F06 = 8;
    }

    SCRATCH_SP += 0xC;
}

/// Link one of the work block's display nodes: it hangs off the model's own
/// coordinate, carries `rec` as its collision-record table and sits at `pos`
/// in that coordinate's space with `field1C` as its extent.
static __inline__ void Actor403200_LinkWorkObj(GsCOORDINATE2* coord, GpObj* obj, GpRec18* rec,
                                               SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->coord    = coord;
    obj->ctx.recs = rec;
    obj->pos.vx   = pos->vx;
    obj->pos.vy   = pos->vy;
    obj->pos.vz   = pos->vz;
    obj->radius   = field1C;
    obj->flags    = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->ctx.recs, kind, 0);
}

/// Rebuild `coord`'s rotation around the yaw it already faces and rescale it:
/// `ratan2` of the rotation's Z basis gives the yaw, `Gfx_RotMatrixY` rebuilds
/// the rotation from it and `ScaleMatrix` applies `xz` on both horizontal axes
/// and `y` on the vertical one. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, handed back once the rotation has been copied onto the
/// coordinate; as an inline the scratch-head accesses stay absolute.
static __inline__ void Actor403200_ScaleRotation(GsCOORDINATE2* coord, s16 xz, s32 y)
{
    Actor403200RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor403200RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor403200RotScratch));
    *(Actor403200RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = xz;
    sc->scale.vy = y;
    sc->scale.vz = xz;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(Actor403200RotScratch);
}

/// The same rebuild at a uniform half scale.
static __inline__ void Actor403200_ShrinkRotation(GsCOORDINATE2* coord)
{
    Actor403200RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor403200RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor403200RotScratch));
    *(Actor403200RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x800;
    sc->scale.vy = 0x800;
    sc->scale.vz = 0x800;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(Actor403200RotScratch);
}

/// Squared-distance test on a horizontal gap, run out of a `VECTOR3` carved
/// off `G_SCRATCH_HEAD`: the vector holds the gap's x and z beside a 1000-unit
/// reach, each component is squared in place, and the result says whether the
/// gap is longer than the reach.
static __inline__ s32 Actor403200_OutOfReach(SVECTOR* gap)
{
    VECTOR3* v;

    v                          = (VECTOR3*)(*(u8**)G_SCRATCH_HEAD - sizeof(VECTOR3));
    *(VECTOR3**)G_SCRATCH_HEAD = v;
    v->vx                      = gap->vx;
    v->vy                      = gap->vz;
    v->vz                      = 1000;
    v->vx                      = v->vx * v->vx;
    v->vy                      = v->vy * v->vy;
    v->vz                      = v->vz * v->vz;
    *(u8**)G_SCRATCH_HEAD      = *(u8**)G_SCRATCH_HEAD + sizeof(VECTOR3);

    return v->vx + v->vy >= v->vz;
}

/// Gap from `coord` to the camera target `D_80073B8C`, into `out`.
static __inline__ void Actor403200_GapToCamera(GsCOORDINATE2* coord, SVECTOR* out)
{
    out->vx = D_80073B8C->t[0] - coord->coord.t[0];
    out->vy = D_80073B8C->t[1] - coord->coord.t[1];
    out->vz = D_80073B8C->t[2] - coord->coord.t[2];
}

/// Spawn state of the enemy dispatched through `D_actor_403200_80131E90`:
/// allocate the work block and stand the model up where the host's first
/// escort's part 1 is, in view space.
///
/// The model is reparented to `gGfxViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `Actor403200_AccumulateRotation` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `Actor403200_LocalToView` carries its origin along the same chain for the
/// translation. The model is then spun by 0x80 of the 0x1000-unit circle, its
/// single display node is linked with a 0x394 extent, and that node is paired
/// with the owning enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void func_actor_403200_8013509C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor403200Work*     host;
    SVECTOR              pos;
    SVECTOR              vec;

    owner = task->parent->spawnArg2;
    host  = (Actor403200Work*)owner->task->work;

    if (D_actor_403200_80141C50 == 1 || host->field_0 == 0x10 || host->field_0 == 5 ||
        host->field_0 == 0xC || host->field_0 == 0x12 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work,
         work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->field_1AC                        = 0;
    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;

    Actor403200_AccumulateRotation(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1],
                                   &((TmdObject*)task->extra)->coords->coord);

    vec.vx = vec.vy = vec.vz = 0;
    Actor403200_LocalToView(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x80, 0);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);

    pos.vx = pos.vy = pos.vz = 0;
    Actor403200_LinkWorkObj(((TmdObject*)task->extra)->coords, &work->obj0, &work->rec0, &pos, 0x394, 3,
                            1);

    work->obj0.flags &= 0x7FFF;
    work->obj0.key    = Gp_PackObjPair((GpObj50*)owner, 2);
    work->field_1A8   = 1;
    task->state++;
}

/// Flight state of the enemy dispatched through `D_actor_403200_80131E90`:
/// carry the model along its own forward axis until it lands. `field_1A8` (the
/// dispatcher's state-changed flag) re-arms the step counter, the ground marker
/// and the first display node on the frame the state starts.
///
/// While the game is running (`D_801153F4` clear) the model falls 0xA a step,
/// column 2 of its coordinate is normalised into a scratchpad `SVECTOR` and
/// scaled by 0x89/0x1000 through the GTE's GPF, and that is the per-step
/// translation added to the coordinate; past step 0x29 the height is pinned to
/// -0x3E8 instead. The marker grows 0x60 a step and is drawn under the work
/// block's own coordinate, which is parented to `gGfxViewCoord` and tracks the
/// model. After 0x35 steps the display node is handed back and the task steps
/// on. Paused (`D_801153F4` set) only the coordinate is refreshed, and the
/// marker is skipped while the host's `field_F08` step is 6.
///
/// Bails out -- unlinking the display node and stepping the task on -- when the
/// overlay is shutting down or the host has moved to state 5, 0xC, 0x10 or
/// 0x12.
void func_actor_403200_801354A4(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Actor403200Work*     host;
    GpEnemy*             owner;
    u8*                  head;
    SVECTOR*             dir;
    /// Second live alias of `dir`: the GTE operand is kept in its own register
    /// for the whole function, which is what gives this function its seventh
    /// callee-saved slot.
    SVECTOR* gteDir;

    work  = (Actor403200GrabWork*)task->work;
    owner = task->parent->spawnArg2;
    host  = (Actor403200Work*)owner->task->work;

    if (D_actor_403200_80141C50 == 1 || host->field_0 == 0x10 || host->field_0 == 5 ||
        host->field_0 == 0xC || host->field_0 == 0x12) {
        task->state++;
        Gp_UnlinkObj(&work->obj0);
        return;
    }

    head                       = *(u8**)G_SCRATCH_HEAD;
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    *(SVECTOR**)G_SCRATCH_HEAD = dir;
    gteDir                     = dir;

    if (work->field_1A8 != 0) {
        work->field_1AC   = 0;
        work->field_1B0   = 0x400;
        work->field_1A8   = 0;
        work->rec0.key    = 0;
        work->obj0.flags |= 0x8000;
    }

    if (D_801153F4 == 0) {
        work->field_1AC++;
        ((TmdObject*)task->extra)->coords->coord.t[1] += 0xA;

        Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12();
        gte_stsv(gteDir);

        ((TmdObject*)task->extra)->coords->coord.t[0] += dir->vx;
        ((TmdObject*)task->extra)->coords->coord.t[1] += dir->vy;
        if (work->field_1AC >= 0x29) {
            ((TmdObject*)task->extra)->coords->coord.t[1] = -0x3E8;
        }
        ((TmdObject*)task->extra)->coords->coord.t[2] += dir->vz;
        ((TmdObject*)task->extra)->coords->flg         = 0;

        work->field_1B0 += 0x60;
        Gp_ClearRec18Occupied(&work->rec0);

        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                             Gp_State1C->groundShade);

        if (work->field_1AC >= 0x35) {
            Gp_UnlinkObj(&work->obj0);
            task->state++;
            work->field_1A8 = 1;
        }
    } else {
        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        if (host->field_F08 != 6) {
            Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                                 Gp_State1C->groundShade);
        }
    }

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

/// Entry state of the enemy dispatched through `D_actor_403200_80131E9C`:
/// allocate its work block and drop the model onto the floor of the view
/// coordinate, under escort 1 of the host actor.
///
/// The model is reparented to `gGfxViewCoord`, its texture page cleared and its
/// CLUT row set to 2, and -- once the stream buffers exist -- processed twice
/// before the spawn cue is enqueued at the model's own pan and half its depth
/// with the owner's id in its high half. The task's light and colour matrices
/// are pointed into the work block, the translation is replaced by the world
/// position of part 1 of escort 1's model, and `field_1AA` is a fifteenth of
/// that height. `vel` is the horizontal gap to the player, which the later
/// states spend a fifteenth at a time. The rotation is finally rebuilt at half
/// scale around the yaw the model already faces.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_403200_80135854(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor403200Work*     host;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    host   = (Actor403200Work*)owner->task->work;
    player = gameGetPtrSlot(3);

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(sizeof(Actor403200GrabWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;
    ((TmdObject*)task->extra)->tpage       = 0;
    ((TmdObject*)task->extra)->clut        = 2;

    if (((TmdObject*)task->extra)->buffer != NULL) {
        tmdProcessStream(task->extra);
        tmdProcessStream(task->extra);
        sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020001C;
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
    }

    ((TmdObject*)task->extra)->lightMtx = &work->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &work->colorMtx;

    vec.vx = vec.vy = vec.vz = 0;
    Actor403200_LocalToView(&((TmdObject*)host->field_ECC[1]->task->extra)->coords[1], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    work->field_1AA = ((TmdObject*)task->extra)->coords->coord.t[1] / 15;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
    work->field_1AC = 0;
    work->field_1B2 = 0;

    Actor403200_ShrinkRotation(((TmdObject*)task->extra)->coords);
    task->state++;
}

/// Bounce state of the enemy dispatched through `D_actor_403200_80131E9C`:
/// bounce the model on the floor until it settles. While the model is still below the floor plane (`coord.t[1] > 0`)
/// it is snapped back to -0x32, the step counter is cleared, the impact cue is
/// enqueued with the object's own pan and half its depth, and the task steps
/// on. Otherwise the body keeps falling by `field_1AA`'s magnitude, drifts a
/// fifteenth of `vel` in x and z, has its colour refreshed from the model's
/// world position, damps the two shake terms and has its rotation rebuilt at
/// half scale.
void func_actor_403200_80135CB8(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    GsCOORDINATE2*       coord;
    VECTOR               pos;
    s32                  sfx;
    s32                  pan;
    s32                  drop;
    s32                  bounce;

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    coord = ((TmdObject*)task->extra)->coords;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->field_1AC   = 0;
        sfx               = ((enemy->placeKey >> 0xC) << 8) | 0x4020000C;
        pan               = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->field_1AA);
    coord->coord.t[1] = drop + bounce;

    ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx / 15;
    ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz / 15;
    ((TmdObject*)task->extra)->coords->flg         = 0;

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    Actor403200_ShrinkRotation(((TmdObject*)task->extra)->coords);
}

/// Rise state of the enemy dispatched through `D_actor_403200_80131E9C`: for
/// the first nine steps the model is stretched taller and thinner each step --
/// horizontally `step * 400 + 0x800` and vertically `0x800 / step` -- around
/// the yaw it already faces. On step 7 it is squashed to 0x17A0 wide at normal
/// height, and if the player is within 1000 units horizontally, is not in mode
/// 2, still has HP and answers the 0x3F8 query, the overlay's own animation-set
/// table is sent as message 0x3FF and the take-over is latched in `field_1B2`.
/// The task steps on once the count passes ten with no animation installed,
/// once the latched animation has been released, or after 200 steps. Every
/// step refreshes the model's colour from its world position and damps the two
/// shake terms. Bails to `Gp_DestroyEnemy` when the overlay is shutting down,
/// cancelling a still-installed animation on the way out.
void func_actor_403200_80135F98(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    GameActor*           actor;
    PlayerStatus*        cfg;
    SVECTOR              gap;
    VECTOR               pos;
    s16                  step;
    s16                  scale;
    s32                  shrink;

    work   = (Actor403200GrabWork*)task->work;
    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    cfg    = &Player_Status;

    if (D_actor_403200_80141C50 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    step = ++work->field_1AC;
    if (step < 10) {
        scale  = step * 0x190 + 0x800;
        shrink = 0x800 / step;
        Actor403200_ScaleRotation(((TmdObject*)task->extra)->coords, scale, shrink);
    }

    if (work->field_1AC == 7) {
        Actor403200_ScaleRotation(((TmdObject*)task->extra)->coords, 0x17A0, 0x800);

        gap.vx = ((TmdObject*)task->extra)->coords->coord.t[0] -
                 ((TmdObject*)player->extra)->coords->coord.t[0];
        gap.vy = 0;
        gap.vz = ((TmdObject*)task->extra)->coords->coord.t[2] -
                 ((TmdObject*)player->extra)->coords->coord.t[2];

        if (Actor403200_OutOfReach(&gap) == 0 && actor->field_954 != 2 &&
            cfg->hp > 0) {
            D_actor_403200_8015F900.field_14 = 0x28;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015F900, 0) == 0) {
                D_actor_403200_80141C54 = 1;
                work->anim.field_0      = D_actor_403200_8015E710;
                work->anim.field_4      = 1;
                work->anim.field_8      = 0;
                work->anim.field_C      = 3;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_1B2 = 1;
            }
        }
    }

    if (work->field_1AC >= 11 && work->field_1B2 == 0) {
        task->state++;
    } else if (work->field_1B2 == 1 && D_actor_403200_80141C54 == 0) {
        task->state++;
    } else if (work->field_1AC >= 0xC9) {
        D_actor_403200_80141C54 = 0;
        task->state++;
    }

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}

/// Hold state of the enemy dispatched through `D_actor_403200_80131E9C`: once
/// `field_1A8` says the take-over is armed and `field_1B2` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void func_actor_403200_801364F4(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    s32                  armed;

    work   = (Actor403200GrabWork*)task->work;
    player = gameGetPtrSlot(3);
    if (D_actor_403200_80141C50 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        armed           = work->field_1B2;
        work->field_1AC = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        D_actor_403200_8015E710[2] =
            ((Actor403200AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->sets[9];
        work->anim.field_0 = D_actor_403200_8015E710;
        work->anim.field_4 = 2;
        work->anim.field_8 = armed;
        work->anim.field_C = 9;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        ((TmdObject*)task->extra)->flags = 0x80;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
}

/// Spawn state of this enemy: allocate the work block, drop the model onto the
/// floor of the view coordinate and hang the two display nodes off it.
///
/// The model is reparented to `gGfxViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height and `vel` the
/// horizontal gap to the player, which the later states spend a fifteenth at a
/// time. The landing cue is enqueued at the model's own pan and depth with the
/// owner's id in its high half, the model is spun to a random yaw, and the two
/// nodes are linked with their collision-record tables before the task's colour
/// and light matrices are pointed into the work block.
void func_actor_403200_8013669C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    player = gameGetPtrSlot(3);

    if (D_actor_403200_80141C50 == 1 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;

    vec.vx = vec.vy = vec.vz = 0;
    Actor403200_LocalToView(&((TmdObject*)owner->task->extra)->coords[3], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    work->field_1AA = ((TmdObject*)task->extra)->coords->coord.t[1] / 9;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    Actor403200_LinkWorkObj(((TmdObject*)task->extra)->coords, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.coord    = ((TmdObject*)task->extra)->coords;
    work->obj1.ctx.recs = &work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0x3000A;
    work->obj1.radius   = 0x100;
    work->obj1.flags    = 1;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= 0x8000;
    Gp_InitRec18Table(work->obj1.ctx.recs, 3, 0);
    work->obj1.flags |= 0x4000;
    work->obj0.key    = Gp_PackObjPair((GpObj50*)owner, 5);

    ((TmdObject*)task->extra)->lightMtx = &work->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &work->colorMtx;
}

/// Fall state of the enemy dispatched through `D_actor_403200_80131F04`, the
/// one after its spawn: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `field_1AA` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `rec1` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void func_actor_403200_80136ACC(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    VECTOR               pos;
    s32                  pan;

    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        Gp_SetLightMode((GpObj4C*)enemy, 0);
        ((TmdObject*)task->extra)->flags = 2;
    }

    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0) {
        work->obj0.flags                              |= 0x8000;
        work->obj1.flags                              |= 0x4000;
        ((TmdObject*)task->extra)->coords->coord.t[1] += ABS(work->field_1AA);
    }

    if (((TmdObject*)task->extra)->coords->coord.t[1] >= -0x31) {
        ((TmdObject*)task->extra)->coords->coord.t[1] = -0x32;
        work->field_1AC                               = 0;
        pan                                           = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        task->state++;
    }

    if (func_actor_403200_801324D0(((TmdObject*)task->extra)->coords, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        ((TmdObject*)task->extra)->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx / 9;
    ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz / 9;
    ((TmdObject*)task->extra)->coords->flg         = 0;

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// Settling state of the same enemy, after the fall: the step counter drives the whole
/// thing. When the dispatcher flags a state change the horizontal velocity is
/// cut to a ninth, both light modes are reset and the two display nodes drop
/// the draw flags the descent raised. Past x 0x4B65 in room 0x0427 the x
/// velocity is killed outright; for the first eight steps what is left of it
/// moves the model and is halved again each step. Steps 1, 2, 4, 8 and 20 puff
/// a `0x600A5` effect out of the model's coordinate, and 4 and 8 also switch
/// the light mode. After 0x51 steps both nodes are unlinked and the task steps
/// on; until then the two collision-record tables are wiped each step. The
/// model's own `workm` translation is handed to `Gp_UpdateActorColor`.
void func_actor_403200_80136D94(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    VECTOR               pos;
    s16                  step;

    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        work->field_1AC = 0;
        work->vel.vx   /= 9;
        work->vel.vz   /= 9;
        Gp_SetLightMode((GpObj4C*)enemy, 0);
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        work->obj1.flags                &= ~0x4000;
        work->obj0.flags                &= ~0x8000;
        ((TmdObject*)task->extra)->flags = 2;
    }

    work->field_1AC++;

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        ((TmdObject*)task->extra)->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    if (work->field_1AC < 8) {
        ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx;
        ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz;
        work->vel.vx                                 >>= 1;
        work->vel.vz                                 >>= 1;
        ((TmdObject*)task->extra)->coords->flg         = 0;
    }

    step = work->field_1AC - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(0x600A5, ((TmdObject*)task->extra)->coords, 1, NULL);
            Gp_SetLightMode((GpObj4C*)enemy, 2);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(0x600A5, ((TmdObject*)task->extra)->coords, 1, NULL);
            break;
    }

    if (work->field_1AC >= 0x51) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        task->state++;
    }

    if (work->field_1AC < 0x51) {
        Gp_ClearRec18Occupied(&work->rec1);
        Gp_ClearRec18Occupied(&work->rec0);
    }

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// Spawn state of the enemy dispatched through `D_actor_403200_80131F14`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `D_actor_403200_8015E70C`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->target` is then the host model's position pushed out
/// by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on, measured
/// against the camera target -- plus the `[group][spawnArg1]` entry of
/// `D_actor_403200_8015E7C0`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `D_actor_403200_8015E780` offset, its work coordinate is parented to
/// `gGfxViewCoord` with an identity rotation and carries the single display
/// node, and the spawn cue is enqueued at the model's own pan and depth with
/// the owner's id in its high half. The trailing `Gp_SpawnEff` effect becomes
/// this task's parent so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_403200_8013709C(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    GpEnemy*             owner;
    Task*                parent;
    Task*                player;
    Actor403200Matrix*   mtx;
    SVECTOR              vec;
    s32                  dist;
    s32                  rnd;
    s32                  snd;
    s32                  pan;

    player = gameGetPtrSlot(3);
    owner  = task->parent->spawnArg2;
    parent = task->parent;

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(Actor403200DropWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    work->field_1AA                        = 0;

    Actor403200_GapToCamera(((TmdObject*)task->extra)->coords, &vec);

    if ((u16)task->spawnArg1 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = ((u32)Gp_LcgState >> 16) & 3;
        switch (rnd) {
            case 0:
            case 1:
                D_actor_403200_8015E70C = 1;
                break;
            case 2:
                D_actor_403200_8015E70C = rnd;
                break;
            case 3:
                D_actor_403200_8015E70C = 0;
                break;
            default:
                D_actor_403200_8015E70C = 0;
                break;
        }
    }

    Actor403200_GapToCamera(((TmdObject*)parent->extra)->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x2710;
    } else {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x32C8;
    }

    work->target.vx +=
        D_actor_403200_8015E7C0[D_actor_403200_8015E840[D_actor_403200_8015E70C]
                                                       [(u16)task->spawnArg1]]
            .vz;
    work->target.vy = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->target.vz = D_actor_403200_8015E7C0[D_actor_403200_8015E840[D_actor_403200_8015E70C]
                                                                     [(u16)task->spawnArg1]]
                          .vx -
                      0x189C;
    work->target.vz = (((u32)Gp_LcgState >> 16) & 0x7F) + work->target.vz;

    if ((u16)task->spawnArg1 == 4) {
        work->target.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        work->target.vy = 0;
        work->target.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
    }

    work->timer     = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_1AE = ((u32)Gp_LcgState >> 16) & 8;

    vec.vx = D_actor_403200_8015E780[(u16)task->spawnArg1].vx;
    vec.vy = D_actor_403200_8015E780[(u16)task->spawnArg1].vy;
    vec.vz = D_actor_403200_8015E780[(u16)task->spawnArg1].vz;

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx + ((TmdObject*)parent->extra)->coords->coord.t[0];
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz + ((TmdObject*)parent->extra)->coords->coord.t[2];

    work->obj.key = Gp_PackObjPair((GpObj50*)owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    work->coord.sub    = &gGfxViewCoord;
    mtx                = (Actor403200Matrix*)&work->coord.coord;
    mtx->ident.m00_m01 = 0x1000;
    mtx->ident.m02_m10 = 0;
    mtx->ident.m11_m12 = 0x1000;
    mtx->ident.m20_m21 = 0;
    mtx->ident.m22     = 0x1000;
    Gfx_RotMatrixY(&mtx->mat, 0, 1);

    Actor403200_LinkWorkObj(&work->coord, &work->obj, &work->rec, &vec, 0x100, 3, 1);
    work->obj.flags &= 0x7FFF;

    snd = ((owner->placeKey >> 12) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));

    work->eff = Gp_SpawnEff(0x6019B, ((TmdObject*)task->extra)->coords, 0, NULL);
    if (work->eff != NULL) {
        Task_Reparent(task, work->eff->task);
    }
    task->state++;
}

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `field_1AE` a step until it passes -0x4E20, then clamp it there, snap its
/// horizontal position back onto `work->target`, restart the step counter, pick
/// a fresh 0..0x1F bias for the next leg, flag the list object and step the task
/// on. Either way the work block's own coordinate is left tracking the model.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_403200_80137600(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    s32                  y;

    work = (Actor403200DropWork*)task->work;
    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    y                                             = ((TmdObject*)task->extra)->coords->coord.t[1] - 0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[1] = y - work->field_1AE;
    if (((TmdObject*)task->extra)->coords->coord.t[1] < -0x4E20) {
        task->state++;
        ((TmdObject*)task->extra)->coords->coord.t[0] = work->target.vx;
        ((TmdObject*)task->extra)->coords->coord.t[2] = work->target.vz;
        Gp_LcgState                                   = Gp_LcgState * 5 + 0x71357911;
        ((TmdObject*)task->extra)->coords->coord.t[1] = -0x4E20;
        work->timer                                   = 0;
        work->field_1AE                               = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                              |= 0x8000;
    }

    ((TmdObject*)task->extra)->coords->flg = 0;
    work->coord.coord.t[0]                 = ((TmdObject*)task->extra)->coords->coord.t[0];
    work->coord.coord.t[1]                 = ((TmdObject*)task->extra)->coords->coord.t[1];
    work->coord.coord.t[2]                 = ((TmdObject*)task->extra)->coords->coord.t[2];
    work->coord.flg                        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Descent state of the enemy dispatched through `D_actor_403200_80131F14`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + field_1AE` a step. When
/// it reaches floor level, zero the height, restart the step counter, tell the
/// trailing `Gp_SpawnEff` effect to wind down, play the landing cue and step
/// the task on. Either way the work block's own coordinate is left tracking
/// the model. Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_403200_80137788(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    Actor403200DropCoord coord;
    MATRIX*              mtx;
    GpEnemy*             owner;
    s32                  snd;
    s32                  pan;

    work = (Actor403200DropWork*)task->work;
    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->timer++;
    coord.c.sub          = &gGfxViewCoord;
    mtx                  = &coord.c.coord;
    coord.ident.m00_m01  = 0x1000;
    coord.ident.m02_m10  = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    coord.ident.m20_m21  = 0;
    mtx->m[2][2]         = 0x1000;
    Gfx_RotMatrixY(mtx, 0, 1);

    coord.c.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
    coord.c.coord.t[1] = 0;
    coord.c.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
    coord.c.flg        = 0;
    Gp_UpdateCoord(&coord.c);

    Gp_DrawEffGroundQuad((VECTOR3*)coord.c.workm.t, (s16)((s16)work->timer * 8 + 0x80),
                         Gp_State1C->groundShade);

    if ((s16)work->timer >= 0x14) {
        ((TmdObject*)task->extra)->coords->coord.t[1] =
            ((TmdObject*)task->extra)->coords->coord.t[1] + (work->field_1AE + 0x258);
        if (((TmdObject*)task->extra)->coords->coord.t[1] > 0) {
            owner                                         = task->parent->spawnArg2;
            ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
            work->timer                                   = 0;
            if (work->eff != NULL) {
                work->eff->task->spawnArg1 = 2;
            }
            task->state++;
            snd = ((owner->placeKey >> 12) << 8) | 0x4020000C;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }
    }

    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
    work->coord.coord.t[1] = ((TmdObject*)task->extra)->coords->coord.t[1];
    work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}
