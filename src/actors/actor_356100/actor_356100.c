#include "common.h"

#include "actors/actor_356100.h"
#include "main/mc.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "rooms/rooms_shared_80182078.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162258);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801625A0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162AEC);

/// Same body as `Actor01900_Fn00FA4` / `RoomsShared80182078`.
s32 func_actor_356100_80162C90(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                      scratch;
    void**                      tail;
    u8*                         head;
    RoomsShared80182078Scratch* st;
    u16                         vz;
    s16                         d;
    s16                         dz;
    s32                         t;
    s32                         hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(RoomsShared80182078Scratch);
        st  = (RoomsShared80182078Scratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    RoomsShared80182078ToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    RoomsShared80182078ToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12_real();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(RoomsShared80182078Scratch);
    return hit;
}

void func_actor_356100_801633DC(Actor356100* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor356100AnimWork* work;

    work   = (Actor356100AnimWork*)arg0->field_1C;
    weight = work->field_98C;
    anim   = &work->anim;
    for (i = 1; i < 0x15; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_98A;
            work->slots[i].field_9      = (u8)(work->field_982 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_982 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163508);

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the model's light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The `field_36`
/// sub-type picks the clip and the spawn argument the re-entry pair, and the
/// finished entry advances the state. Same body as `Actor01900_Fn02018`.
void func_actor_356100_8016382C(GpEnemy* enemy, Actor356100* actor)
{
    SVECTOR          dir;
    SVECTOR*         v;
    VECTOR           pos;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor356100Work* work;
    s32              kind;

    root            = actor->field_2C->coords;
    obj             = actor->field_2C;
    work            = Mem_Calloc(0xBC0, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    actor->field_18 = func_actor_356100_8016A158;
    Actor356100_BindMatrices(actor);
    enemy->field_4     = &actor->field_2C->coords->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &actor->field_2C->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4C     = 0;
    enemy->field_40     = (s16)D_actor_356100_8016A984.field_4;
    enemy->field_50     = &D_actor_356100_8016A984;
    enemy->field_54     = (s32)&work->field_9C0;
    func_800B3F84(&((Actor356100AnimWork*)work)->anim, D_actor_356100_801730B8, (GpAnimObj*)obj,
                  &((Actor356100AnimWork*)work)->slots[21], ((Actor356100AnimWork*)work)->slots);
    func_800B3F84(&((Actor356100AnimWork*)work)->blendAnim, D_actor_356100_801730B8, (GpAnimObj*)obj,
                  &((Actor356100AnimWork*)work)->blendSlots[21], ((Actor356100AnimWork*)work)->blendSlots);
    work->field_978 = 2;
    work->field_97E = 1;
    work->field_97A = 0;
    work->field_990 = 0;
    work->field_98E = 0;
    work->field_984 = 0x10;
    work->field_982 = 0x10;
    func_actor_356100_80163508(actor);
    work->field_14     = 0;
    work->field_C[0].x = actor->field_2C->coords->coord.t[0];
    work->field_C[0].z = actor->field_2C->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12_real();
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->coords->coord.t[2] + dir.vz;
    actor->field_24    = &D_actor_356100_80173258;
    root->sub          = &Gfx_ViewCoord;
    root->flg          = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_356100_801732A8.field_0 = actor->field_2C->coords;
    D_actor_356100_801732A8.field_4 = 0x100;
    D_actor_356100_801732A8.field_6 = 2;
    kind                            = actor->field_36;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            work->field_B54 = D_actor_356100_8016A994[0].field_0;
            work->field_B56 = D_actor_356100_8016A994[0].field_2;
            break;
        case 1:
            work->field_B54 = D_actor_356100_8016A994[2].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
        case 0:
        default:
            work->field_B54 = D_actor_356100_8016A994[1].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
    }
    Actor356100_RescaleYaw(actor->field_2C->coords, 0x1194);
    work->field_BBC = 0;
    actor->field_30++;
}

/// Runs the clip the work block's `field_978` halfword selects and holds this
/// state until it ends: while the actor is live, reset the model (`node.field_4`
/// / `obj->field_C`, `Tmd_AllocBuffers`), start clip 2 at speed 0x10, and tick
/// until clip 0xB has reached frame 6 or clip 0xC frame 9, then park `field_982`
/// at 0x20. Once the actor is no longer live the same slot is halved per frame as
/// a scale ramp that bounces between 0x10 and -0x10 — ending the state with
/// `field_0 = 0x11` when `Gp_TickObjFlag2` reports the flag has expired.
void func_actor_356100_80163CD4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              animA;
    s16              animB;
    s32              value;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        animA             = 0xB;
        animB             = 0xC;
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        do {
            func_actor_356100_80163508(arg0);
        } while (((work->field_97E != animA) || ((u32)(work->field_5A & 0x3FF) < 6U)) &&
                 ((work->field_97E != animB) || ((u32)(work->field_5A & 0x3FF) < 9U)));
        work->field_982 = 0x20;
        return;
    }
    arg0->field_2C->coords->flg = 0;
    value                       = (s16)work->field_982 / 2;
    work->field_982             = (u16)value;
    if (value == 1) {
        work->field_982 = -0x10U;
    }
    if ((s16)work->field_982 == -1) {
        work->field_982 = 0x10;
    }
    func_actor_356100_80163508(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)ctx) == 1) {
        ctx->field_4C &= 0xFD;
        work->field_0  = 0x11;
    }
}

/// Turns the actor's facing onto the player in one step and rescales the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 9 state parked in `field_97E`, otherwise the aim scratch
/// takes the player offset, `Actor356100_PositionYaw` gives the wrapped turn,
/// `field_98E` snapshots it, it is clamped to [-0x10, 0x10] and the root yaw is
/// re-derived from it. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80163E2C(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 9;
        func_actor_356100_80163508(arg0);
        work->field_9BC = 0x180;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor356100_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x11) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Wrapped yaw from `coord`'s facing to an offset (`x`, `z`) already in hand.
/// Same body as `Actor401300_YawTo` / `Actor01900_YawTo`, the pair
/// `Actor356100_PositionYaw` above is spelled out as. The enter tick below and
/// the collapse tick further down both read its turn back out of the scratch
/// block they already hold, so it sits above them; `MoveForwardNonzero` is
/// only reached after the first `func_actor_356100_801653F4` caller.
static __inline__ s16 Actor356100_YawTo(GsCOORDINATE2* coord, s16 x, s16 z)
{
    s32 angle;

    angle = ratan2(x, z);
    return Actor356100_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Steps `coord` `amount` units along its own root colour-matrix column unless
/// movement is frozen, normalising the column with the GTE first and giving the
/// 8-byte `G_SCRATCH_HEAD` block back afterwards. The guardless sibling of
/// `Actor356100_MoveForwardNonzero`, reading the X component back through
/// `vec`; same body as `Actor01900_StepForward` / `Actor00100_MoveForward`.
static __inline__ void Actor356100_StepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        *(SVECTOR**)G_SCRATCH_HEAD = head - 1;
        vec                        = head - 1;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += vec->vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Pushes `coord` out of the `GpRec18` records `rec` by `func_800E0C10`'s
/// averaged 16.16 delta, then lifts it by `height`. `head` is read before the
/// 0x14-byte `Actor356100DeltaFlag` block is reserved off `G_SCRATCH_HEAD`, so
/// the two spellings of the block in the body reach it the same way the
/// original does — the negative offsets off `head` for the X component and the
/// flag, `s` for the rest. Same body as `Actor01900_Fn00E00`'s push without
/// its mask argument.
static __inline__ void Actor356100_PushRecords(GsCOORDINATE2* coord, GpRec18* rec, s32 count, s16 height)
{
    void**                scratch;
    u8*                   head;
    Actor356100DeltaFlag* s;
    s32                   val;

    if (D_80072729 != 1) {
        scratch                                  = (void**)G_SCRATCH_HEAD;
        head                                     = *scratch;
        *(Actor356100DeltaFlag**)G_SCRATCH_HEAD -= 1;
        s                                        = *(Actor356100DeltaFlag**)G_SCRATCH_HEAD;
        s->field_10                              = 0;
        if (func_800E0C10(rec, &s->delta, count, NULL) != 0) {
            coord->coord.t[0] += ((Actor356100DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
            coord->coord.t[1] += s->delta.vy.h.hi;
            coord->coord.t[2] += s->delta.vz.h.hi;
            val                = ((Actor356100DeltaFlag*)(head - 0x14))->delta.vx.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = s->delta.vz.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
            s->field_10 = 1;
        }
        *(Actor356100DeltaFlag**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_356100_80164158(Actor356100* arg0)
{
    Actor356100Work*       work;
    Actor356100AimScratch* aim;
    TmdObject*             obj;
    s16                    yaw;
    s32                    diff;
    s32                    range;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x12;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
        work->field_B64 = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6                             = (u16)work->field_6 + 1;
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    Actor356100_PushRecords(arg0->field_2C->coords, &work->field_A58, 3, 0x10);
    Actor356100_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    aim->target = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords->coord.m[2][0],
                         ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    Actor356100_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    yaw             = ratan2(aim->delta.vx, aim->delta.vz) + 0x800;
    aim->current    = yaw;
    aim->current    = Actor356100_NormalizeYaw(yaw);
    aim->angle      = Actor356100_YawTo(arg0->field_2C->coords, aim->delta.vx, aim->delta.vz);
    work->field_98E = aim->angle;
    diff            = aim->current - aim->target;
    if (ABS(diff) < 0x44 && (((s16)work->field_B66 / 2) + 3) < work->field_6 && ABS(aim->angle) < 0x80) {
        if (Actor356100_OutOfRange(&aim->delta, 0x708)) {
            work->field_0 = 0xA;
        }
    }
    range = Actor356100_NormalizeYaw((u16)aim->current - (u16)aim->target);
    if (ABS(range) >= 0x201 && (((s16)work->field_B66 / 2) + 3) < work->field_6 && work->field_8 == 0) {
        work->field_8   = 1;
        work->field_97E = 9;
        work->field_978 = 1;
    }
    if (aim->angle < 0x200) {
        if (!Actor356100_OutOfRange(&aim->delta, 0x44C)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x40) {
        aim->angle = 0x40;
    }
    if (aim->angle < -0x40) {
        aim->angle = -0x40;
    }
    aim->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_97E == 3) {
        if (work->field_97A == 0) {
            Actor356100_StepForward(arg0->field_2C->coords, 0x78);
        } else {
            Actor356100_StepForward(arg0->field_2C->coords, 0x1E);
        }
    } else if (work->field_68 & 1) {
        work->field_97E = 3;
        work->field_978 = 1;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164ACC);

/// Aim tick: going live resets the model and starts clip 1 at speed 0x10 with
/// the 3 state parked in `field_97E` and `field_98E` cleared; otherwise the
/// aim scratch takes the player offset, and the wrapped turn from it is paired
/// with the root's own facing yaw — snapshotted into `field_B48` and, plus
/// twice the turn, into the `field_B4A` the yaw is then slewed toward. Same
/// body as `Actor01900_Fn0551C`, whose aim tick this is the live-arm half of:
/// the settling yaw is re-derived from the player each entry while that one
/// only re-seeds the pair.
void func_actor_356100_801653F4(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         cur;
    GsCOORDINATE2*         facing;
    Actor356100AimScratch* head;
    Actor356100AimScratch* s;
    s32                    value;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                                     = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
        obj                                      = arg0->field_2C;
        *(Actor356100AimScratch**)G_SCRATCH_HEAD = head - 1;
        s                                        = head - 1;
        arg0->field_20->node.field_4             = 0;
        obj->flags                               = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 3;
        work->field_98E = 0;
        func_actor_356100_80163508(arg0);
        cur                                       = arg0->field_2C->coords;
        head[-1].delta.vx                         = Player_Status.coordMtx->t[0] - cur->coord.t[0];
        s->delta.vy                               = Player_Status.coordMtx->t[1] - cur->coord.t[1];
        s->delta.vz                               = Player_Status.coordMtx->t[2] - cur->coord.t[2];
        coord                                     = arg0->field_2C->coords;
        s->angle                                  = Actor356100_YawTo(coord, head[-1].delta.vx, s->delta.vz);
        facing                                    = arg0->field_2C->coords;
        s->facing                                 = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_B48                           = s->facing;
        work->field_B4A                           = s->facing + (u16)s->angle * 2;
        *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                     = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    *(Actor356100AimScratch**)G_SCRATCH_HEAD = head - 1;
    s                                        = head - 1;
    func_actor_356100_80163508(arg0);
    cur               = arg0->field_2C->coords;
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - cur->coord.t[0];
    s->delta.vy       = Player_Status.coordMtx->t[1] - cur->coord.t[1];
    s->delta.vz       = Player_Status.coordMtx->t[2] - cur->coord.t[2];
    if (work->field_B48 == work->field_B4A) {
        if (work->field_B64 < 2 || Actor356100_OutOfRange(&s->delta, 0x384)) {
            value = 8;
        } else {
            value = 0xB;
        }
        work->field_0 = value;
    }
    if (work->field_B48 > work->field_B4A) {
        work->field_B48 -= 0x89;
        if (work->field_B48 < work->field_B4A) {
            work->field_B48 = work->field_B4A;
        }
    }
    if (work->field_B48 < work->field_B4A) {
        work->field_B48 += 0x89;
        if (work->field_B48 > work->field_B4A) {
            work->field_B48 = work->field_B4A;
        }
    }
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, work->field_B48, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_97A == 0) {
        Actor356100_StepForward(arg0->field_2C->coords, 0x28);
    } else {
        Actor356100_StepForward(arg0->field_2C->coords, 0x14);
    }
    Actor356100_PushRecords(arg0->field_2C->coords, &work->field_A58, 3, 0x10);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-and-close tick: going live writes the 0x978..0x982 animation slots with
/// `field_9BC` forced to 0xC0 and the enemy's link node cleared, takes the
/// player offset into the aim scratch and turns the root onto it with
/// `ratan2`, then settles `field_B50` on the 12-bit side the `Gp_LcgState`
/// draw picks and leans the yaw by `field_B56` either way, before rebuilding
/// its Y rotation at the fixed 0xDE GPF scale and bumping `field_B66`. Each
/// frame then re-runs the animation and, while the clip sits in 0xC..0x15,
/// takes the aim and pushes the root out of the `field_A58` collision records
/// by 0x10. Past clip 0x1E the state moves to 7. Same body as
/// `Actor01900_Fn05B4C`, whose `head[-1]` / `aim` spelling of the 0x10-byte
/// scratch block this matches.
void func_actor_356100_80165B30(Actor356100* arg0)
{
    Actor356100Work*       work;
    Actor356100AimScratch* head;
    Actor356100AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR*               dir;
    MATRIX                 mat;
    u16                    angle;

    head                                     = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    work                                     = arg0->field_1C;
    *(Actor356100AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0xC0;
        work->field_6   = 0;
        Actor356100_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_B50 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_B50 = 1;
            } else {
                work->field_B50 = -1;
            }
        }
        if (work->field_B50 == 1) {
            work->field_97E = 0x15;
            if (work->field_B66 == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_B56 + angle;
            } else {
                aim->angle += work->field_B56;
            }
            work->field_B50 = -1;
        } else {
            work->field_97E = 0x14;
            if (work->field_B66 == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_B56;
            } else {
                aim->angle -= work->field_B56;
            }
            work->field_B50 = 1;
        }
        work->field_978 = 1;
        work->field_982 = 0xC;
        work->field_97A = 0;
        func_actor_356100_80163508(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_B40;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_B52 = 0xDE;
        work->field_B66++;
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    arg0->field_2C->coords->flg = 0;
    if (work->field_97A == 0) {
        gte_lddp(work->field_B52);
        gte_ldsv(&work->field_B40);
        gte_gpf12_real();
        gte_stsv(aim);
    } else {
        gte_lddp((s16)work->field_B52 >> 1);
        gte_ldsv(&work->field_B40);
        gte_gpf12_real();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->field_2C->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->field_2C->coords;
        coord->coord.t[2] += aim->delta.vz;
        Actor356100_PushRecords(arg0->field_2C->coords, &work->field_A58, 3, 0x10);
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-and-close tick, and the sibling of `func_actor_356100_801666B4` above
/// it. Going live writes the 0x978..0x982 animation slots with `field_9BC`
/// forced to 0x180 and the enemy's link node cleared, then turns the root
/// coordinate onto the player through `Actor356100_PositionYaw` and rebuilds
/// its Y rotation at a uniform 0x1194 scale, re-seeding the offset from the
/// player and clearing the two halfwords next to `field_B68`. Each frame then
/// re-runs the animation and, while the clip sits on 0x10 and the player is not
/// in mode 2, takes the player offset again through
/// `Actor356100_MatrixPositionYaw` and — if the turn is within 0x10 and the
/// player is closer than 0x44C — points `D_actor_356100_80173244.field_0` at
/// one of the two blocks `D_8007218A` selects, then queries message 0x3F8 and
/// on acceptance moves to state 0xC, sets `field_B68` and re-sends the handler
/// as message 0x3FF. Bit 0 of `field_68` forces `field_0` to 7 on clip 4, and
/// past clip 0x10 the actor is pushed one normalised unit away from the player
/// unless it is further than 0x578.
void func_actor_356100_80166018(Actor356100* arg0)
{
    SVECTOR          pos;
    SVECTOR*         p;
    Actor356100Work* work;
    GpEnemy*         enemy;
    GameActor*       player;
    PlayerStatus*    config;
    GsCOORDINATE2*   coord;
    s16              angle;

    enemy  = arg0->field_20;
    work   = arg0->field_1C;
    player = (GameActor*)((Task*)Game_GetPtrSlot(3))->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_978     = 1;
        work->field_982     = 0x10;
        work->field_97E     = 4;
        func_actor_356100_80163508(arg0);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, Actor356100_PositionYaw(arg0, &pos, config), 0);
        Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
        pos.vx                      = arg0->field_2C->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                      = 0;
        pos.vz                      = arg0->field_2C->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_98E             = 0;
        work->field_990             = 0;
        arg0->field_2C->coords->flg = 0;
        work->field_B66             = 0;
        work->field_B68             = 0;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && player->field_954 != 2) {
        angle = Actor356100_MatrixPositionYaw(arg0, &pos, D_80073B8C);
        if (abs(angle) < 0x10 && !Actor356100_OutOfRange(&pos, 0x44C)) {
            if (D_8007218A == 1) {
                D_actor_356100_80173244.field_0 = &D_actor_356100_80173230;
            } else {
                D_actor_356100_80173244.field_0 = &D_actor_356100_80173228;
            }
            D_actor_356100_801732D0.field_14 = 8;
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&D_actor_356100_801732D0, 0) == 0) {
                work->field_0                   = 0xC;
                work->field_B68                 = 1;
                D_actor_356100_80173244.field_4 = 1;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&D_actor_356100_80173244, 0);
            }
        }
    }
    if (work->field_97E == 4 && (work->field_68 & 1)) {
        work->field_0 = 7;
    }
    if ((work->field_5A & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = arg0->field_2C->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = arg0->field_2C->coords->coord.t[2] - config->coordMtx->t[2];
        if (!Actor356100_OutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12_real();
            gte_stsv(p);
            coord                       = arg0->field_2C->coords;
            coord->coord.t[0]          += pos.vx;
            coord                       = arg0->field_2C->coords;
            coord->coord.t[2]          += pos.vz;
            arg0->field_2C->coords->flg = 0;
        }
    }
}

void func_actor_356100_801666B4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         vecp;
    SVECTOR          vec;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        player                                   = Game_GetPtrSlot(3);
        work->field_9BC                          = 0x180;
        enemy->node.field_4                      = 0;
        work->field_978                          = 1;
        work->field_982                          = 0x10;
        work->field_97E                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        D_actor_356100_801732B0.x = ((TmdObject*)player->extra)->coords->coord.t[0];
        D_actor_356100_801732B0.y = ((TmdObject*)player->extra)->coords->coord.t[1];
        D_actor_356100_801732B0.z = ((TmdObject*)player->extra)->coords->coord.t[2];
        vecp                      = &vec;
        /* Order matters: the vy store must follow the vx loads in RTL, or
           sched1 fills its anti-dependency chain from the earlier stores and
           hoists it above the D.z store. */
        vec.vx = ((GpCoordXZ*)arg0->field_2C->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        vec.vy = 0;
        vec.vz = ((GpCoordXZ*)arg0->field_2C->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(vecp, vecp);
        gte_lddp(0x3E8);
        gte_ldsv(vecp);
        gte_gpf12_real();
        gte_stsv(vecp);
        arg0->field_2C->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + vec.vx;
        arg0->field_2C->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + vec.vz;
        arg0->field_2C->coords->flg        = 0;
        D_actor_356100_801732B0.field_10   = 0;
        D_actor_356100_801732B0.field_12   = ratan2(vec.vx, vec.vz);
        D_actor_356100_801732B0.field_14   = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_356100_801732B0, 0);
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 5 && (work->field_68 & 1)) {
        work->field_0 = 0xD;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801668FC);

/// Release tick, the sibling of `func_actor_356100_801684F0` below it and the
/// same body as `Actor01900_Fn06100`. Going live resets the model and starts
/// clip 3 at speed 8 with `field_97A` cleared and `field_B64` zeroed;
/// otherwise the aim scratch takes the player offset, the root is pushed out of
/// the `field_A58` collision records and `Actor356100_YawTo` gives the wrapped
/// turn, which `field_98E` snapshots. A turn under 0x200 while the player is
/// still within 0x384 moves the state to 0xB; the turn is then clamped to
/// [-0x40, 0x40], the root yaw is re-derived from it and the root rescaled to a
/// uniform 0x1194 before being stepped 0x78 along its own column, or 0x3C when
/// `field_97A` is set.
void func_actor_356100_80166CF0(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    Actor356100AimScratch* aim;
    s16                    ang;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 8;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
        work->field_B64 = 0;
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    Actor356100_PushRecords(arg0->field_2C->coords, &work->field_A58, 3, 0x10);
    Actor356100_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    ang             = Actor356100_YawTo(arg0->field_2C->coords, aim->delta.vx, aim->delta.vz);
    aim->angle      = ang;
    work->field_98E = ang;
    if (aim->angle < 0x200) {
        if (!Actor356100_OutOfRange(&aim->delta, 0x384)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x40) {
        aim->angle = 0x40;
    }
    if (aim->angle < -0x40) {
        aim->angle = -0x40;
    }
    aim->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_97A == 0) {
        Actor356100_StepForward(arg0->field_2C->coords, 0x78);
    } else {
        Actor356100_StepForward(arg0->field_2C->coords, 0x3C);
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Rotation-collapse tick: going live clears the model's `field_C`, flags the
/// enemy's link node and re-seeds `field_6`. Each frame then bumps `field_6`
/// and fires its milestone — 0x18 releases state F0 (arg 0xA), 0x1D switches
/// light mode 1 and spawns effect 0x600A5 at model coordinate 2, 0x29 sets
/// `field_C` to 2, 0x2F switches light mode 2 and 0x33 sets `field_C` to 0x80.
/// From 0x1A on, the root rotation is rebuilt in the 0x34-byte scratch block
/// as a uniform 0x1194 scale whose Y shrinks by 0xB per frame past 0x14, and
/// written back into the root coordinate with `flg` cleared. Same body as
/// `Actor01900_Fn06904`.
void func_actor_356100_80167358(Actor356100* arg0)
{
    Actor356100Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    s16                    temp_v0;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    temp_v1;
    u16                    m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->flags          = 0;
        enemy->node.field_4 = 1;
        work->field_6       = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->flags = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                        = 0x1194;
            head                                     = scratch_base;
            head                                     = *(u8**)(head + 0x3FC);
            coord                                    = arg0->field_2C->coords;
            blk                                      = (Actor356100RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor356100RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor356100RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor356100RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Range tick, and the sibling of `func_actor_356100_80167818` below it. Going
/// live clears the model's `field_C`, reallocates its buffers, clears the
/// enemy's link node, saves the `field_AF8` colour matrix into `field_B18` and
/// starts clip 0xE at speed 1 with `field_9BC` forced to 0x180. Each frame then
/// bumps `field_6` until it passes 0x960, after which a 4-bit `Gp_LcgState`
/// draw thins the tick to one frame in 16. A tick that runs drops to state 6
/// while the player is still within 3000 of the actor, then flips the clip
/// between 0xE and 0xF on a 50/50 draw gated by bits 2 and 1 of `field_68`.
/// Same shape as `func_actor_401300_80139520`.
void func_actor_356100_80167584(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj        = arg0->field_2C;
        enemy      = arg0->field_20;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_6       = 0;
        work->field_B18     = work->field_AF8;
        work->field_97E     = 0xE;
        work->field_978     = 1;
        work->field_982     = work->field_984;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)((u16)work->field_6 + 1);
    }
    coord = arg0->field_2C->coords;
    d     = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!Actor356100_OutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 0xE && (work->field_68 & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_97E = 0xF;
            work->field_978 = 1;
            func_actor_356100_80163508(arg0);
        }
    }
    if (work->field_97E == 0xF && (work->field_68 & 1)) {
        work->field_97E = 0xE;
        work->field_978 = 1;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_80167818(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_356100_801731B0 = 0;
        work->field_97E         = 0x10;
        work->field_978         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_990     = 0;
        work->field_982     = 0x10;
        work->field_98E     = 0;
        work->field_6       = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
        work->field_6 = 1;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_994 != (work->field_5A & 0x3FF)) {
        D_actor_356100_801732A8.field_0 = arg0->field_2C->coords;
        D_actor_356100_801732A8.field_4 = 0x100;
        D_actor_356100_801732A8.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->coords + 5, NULL,
                      &D_actor_356100_801732A8);
    }
    work->field_994 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->coords;
    d               = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!Actor356100_OutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167A7C);

/// Steps `coord` `amount` units along its own root colour-matrix column unless
/// movement is frozen (`D_80072729`) or `amount` is zero, the column
/// normalised by the GTE first. Same body as `Actor01900_MoveForward` /
/// `Actor401300_MoveForwardNonzero`.
static __inline__ void Actor356100_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Turn-and-push tick, the sibling of `func_actor_356100_80163E2C` above it and
/// the same body as `func_actor_401300_8013A208`. The live branch resets the
/// model and starts clip 1 at speed 0x10 with the 0x12 state parked in
/// `field_97E`; otherwise the turn scratch takes the player offset,
/// `Actor356100_PositionYaw` gives the wrapped turn, `field_98E` snapshots it,
/// it is clamped to [-0x40, 0x40] and the root yaw is re-derived from it. The
/// root is then pushed out of the `field_A58` collision records and one
/// normalised unit along its own Y column scaled by `field_B4C`, which decays
/// by 0xA per frame — once it reaches zero, or bit 0 of `field_68` is set, the
/// state moves to 9 and the turn scratch is given back.
void func_actor_356100_8016804C(Actor356100* arg0)
{
    Actor356100Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor356100TurnScratch* turn;
    u16                     next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_97E = 0x12;
        work->field_978 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_990     = 0;
        work->field_982     = 0x1E;
    }
    *(Actor356100TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor356100TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor356100_PositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_98E                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, turn->angle, 1);
    Actor356100_PushRecords(arg0->field_2C->coords, &work->field_A58, 3, 0x10);
    Actor356100_MoveForwardNonzero(arg0->field_2C->coords, work->field_B4C);
    if (work->field_B4C > 0) {
        next            = work->field_B4C - 0xA;
        work->field_B4C = next;
        if ((s16)next < 0) {
            work->field_B4C = 0;
        }
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_68 & 1) || work->field_B4C == 0) {
        work->field_0 = 9;
    }
    *(Actor356100TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// `Actor356100_MoveForwardNonzero` testing the freeze flag through a
/// `McSaveData*` rather than `D_80072729`, and without its zero-amount guard.
/// Reads the X component back through `vec`, as `Actor01900_StepForward` does —
/// the `head[-1]` spelling gives the scratch release value a register of its
/// own and costs three instructions here. Same body as
/// `Actor401300_MoveForwardSave`.
static __inline__ void Actor356100_StepForwardSave(McSaveData* save, GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += vec->vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor356100_PushRecords` testing the freeze flag through a `McSaveData*`,
/// and giving the 0x14 bytes back through `G_SCRATCH_HEAD` itself rather than a
/// saved `void**` — the saved pointer keeps the 0x1F8003FC constant live in a
/// register across the release.
static __inline__ void Actor356100_PushRecordsSave(McSaveData* save, GsCOORDINATE2* coord, GpRec18* rec, s32 count, s16 height)
{
    u8*                   head;
    Actor356100DeltaFlag* s;
    s32                   val;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                                     = *(u8**)G_SCRATCH_HEAD;
        *(Actor356100DeltaFlag**)G_SCRATCH_HEAD -= 1;
        s                                        = *(Actor356100DeltaFlag**)G_SCRATCH_HEAD;
        s->field_10                              = 0;
        if (func_800E0C10(rec, &s->delta, count, NULL) != 0) {
            coord->coord.t[0] += ((Actor356100DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
            coord->coord.t[1] += s->delta.vy.h.hi;
            coord->coord.t[2] += s->delta.vz.h.hi;
            val                = ((Actor356100DeltaFlag*)(head - 0x14))->delta.vx.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = s->delta.vz.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
            s->field_10 = 1;
        }
        *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x14;
    }
}

/// Turn-and-rescale tick, the sibling of `func_actor_356100_8016804C` above it
/// and the same body as `func_actor_401300_8013A5C0`. Going live resets the
/// model and starts clip 1 at speed 0x10 with the 0x13 state parked in
/// `field_97E`; otherwise the aim scratch takes the player offset,
/// `Actor356100_YawTo` gives the wrapped turn, `field_98E` snapshots it, it is
/// clamped to [-0x80, 0x80] and halved, the root yaw is re-derived from it and
/// the root coordinate rescaled to a uniform 0x1194. Once the state has settled
/// on 0x11 the collision step pushes the root out of the `field_A58` records
/// and one normalised unit back along its own Y column, both frozen while the
/// save flag is set, and past clip 0x13 the actor is leaned by ±0x4B0 into
/// state 7.
void func_actor_356100_801684F0(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         cur;
    GsCOORDINATE2*         root;
    SVECTOR**              scratch;
    Actor356100AimScratch* head;
    Actor356100AimScratch* aim;
    McSaveData*            save;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x16;
        work->field_97A = 0;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
        return;
    }
    func_actor_356100_80163508(arg0);
    scratch           = (SVECTOR**)G_SCRATCH_HEAD;
    cur               = arg0->field_2C->coords;
    head              = (Actor356100AimScratch*)*scratch;
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - cur->coord.t[0];
    aim               = (Actor356100AimScratch*)(*scratch = (SVECTOR*)(head - 1));
    aim->delta.vy     = Player_Status.coordMtx->t[1] - cur->coord.t[1];
    aim->delta.vz     = Player_Status.coordMtx->t[2] - cur->coord.t[2];
    aim->angle        = Actor356100_YawTo(arg0->field_2C->coords, head[-1].delta.vx, aim->delta.vz);
    work->field_98E   = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_97E == 2) {
        work->field_982 = 0x16;
        work->field_97E = 0x11;
        work->field_978 = 1;
        work->field_6   = 0;
        func_actor_356100_80163508(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    aim->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_97E == 0x11) {
        work->field_6++;
        save  = &Mc_SaveData;
        coord = arg0->field_2C->coords;
        if ((u8)save->unknown_5C0[1] != 1) {
            Actor356100_StepForwardSave(save, coord, -0x10);
        }
        root = arg0->field_2C->coords;
        if ((u8)save->unknown_5C0[1] != 1) {
            Actor356100_PushRecordsSave(save, root, &work->field_A58, 3, 0x10);
        }
        arg0->field_2C->coords->flg = 0;
        if ((s16)work->field_6 >= 0x13) {
            if (work->field_98E <= 0) {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor's facing onto the player in one step and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise `field_6`
/// ticks over for the 0xB-frame transition, the aim scratch takes the player
/// offset, `Actor356100_PositionYaw` gives the wrapped turn, `field_98E`
/// snapshots it, the turn is clamped to [-0x20, 0x20] and the root yaw is
/// re-derived from it before `field_0` moves to `state` once the count-down
/// expires. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80168AFC(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;
    int                    state;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6                             = (s16)((u16)work->field_6 + 1);
    state                                     = 0xB;
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if ((work->field_68 & 1) || ((s16)work->field_6 >= state)) {
        work->field_0 = state;
    }
    aim->angle      = Actor356100_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x21) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord      = arg0->field_2C->coords;
    aim->angle = (u16)aim->angle + ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise the aim
/// scratch takes the player offset, `Actor356100_PositionYaw` gives the wrapped
/// turn, `field_98E` walks toward it by at most 0x28 and the state flips to 0xB
/// once it has caught up. Same body as `func_actor_401300_8013AE48`.
void func_actor_356100_80168E44(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        func_actor_356100_80163508(arg0);
        work->field_6   = 0;
        work->field_990 = 0;
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor356100_PositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_98E < aim->angle) {
        if (aim->angle - work->field_98E > 0x28) {
            work->field_98E += 0x28;
        } else {
            work->field_98E = aim->angle;
        }
    } else if (work->field_98E - aim->angle > 0x28) {
        work->field_98E -= 0x28;
    } else {
        work->field_98E = aim->angle;
    }
    if (work->field_98E == aim->angle) {
        work->field_0 = 0xB;
    }
    coord      = arg0->field_2C->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->coords, 0x1194);
    work->field_978 = 2;
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// The overlay's death-throes tick, the sibling of `func_actor_356100_80168E44`:
/// going live re-seeds the model (the enemy's link node, `obj->field_C`, the
/// 0x978..0x982 animation slots) and queues sound 0x550B0007 against the root
/// part, whose coordinate the live arm clears outright. Each frame then bumps
/// `field_6`, runs the clip and walks part 1's coordinate by a fixed 0x1044 /
/// 0x4AA per frame. Four frames each fire their own sound (0x550B0008 with the
/// 6/0xFF/0x80 pad rumble, 0x400D0002 with 8/0x7F/0x30, 0x400D0001 with
/// 6/0x7F/0x30, 0x550B0009 bare), and `field_6` 0x29..0x2D drives a 16-effect
/// 0x600FB burst over the model's part coordinates — 0x2E..0x31 the same burst
/// with six effects, alternating on the frame's parity.
///
/// The parity test re-reads `field_6` from memory rather than reusing the range
/// test's value (the two reads are what the original emits), so the read is
/// spelled volatile.
void func_actor_356100_80169180(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        s32 pan;

        ctx                   = arg0->field_20;
        arg0->field_2C->flags = 0;
        Tmd_AllocBuffers(arg0->field_2C);
        ctx->node.field_4 = 1;
        work->field_97E   = 1;
        work->field_978   = 2;
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x800, 1);
        coord                       = arg0->field_2C->coords;
        coord->coord.t[2]           = 0;
        coord->coord.t[1]           = 0;
        coord->coord.t[0]           = 0;
        arg0->field_2C->coords->flg = 0;
        Gp_UpdateCoord(arg0->field_2C->coords);
        work->field_6 = 0;
        pan           = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->coords[1]);
        SndEvt_EnqueueType6(0x550B0007, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->coords[1]));
    }
    work->field_6 = (s16)((u16)work->field_6 + 1);
    func_actor_356100_80163508(arg0);
    arg0->field_2C->coords[1].coord.t[0] += 0x1044;
    arg0->field_2C->coords[1].coord.t[2] += 0x4AA;
    arg0->field_2C->coords[1].flg         = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[1]);
    if (work->field_6 == 0x31) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->coords[1]);
        SndEvt_EnqueueType6(0x550B0008, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->coords[1]));
        Gp_SpawnPadLerp(6, 0xFF, 0x80);
    }
    if (work->field_6 == 0x4D) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->coords[1]);
        SndEvt_EnqueueType6(0x400D0002, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->coords[1]));
        Gp_SpawnPadLerp(8, 0x7F, 0x30);
    }
    if (work->field_6 == 0x58) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->coords[1]);
        SndEvt_EnqueueType6(0x400D0001, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->coords[1]));
        Gp_SpawnPadLerp(6, 0x7F, 0x30);
    }
    if (work->field_6 == 0xCE) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->coords[1]);
        SndEvt_EnqueueType6(0x550B0009, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->coords[1]));
    }
    if ((u32)((u16)work->field_6 - 0x29) < 5U) {
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x12], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[2], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[4], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x13], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x12], 0, 0);
    }
    if ((u32)((u16)work->field_6 - 0x2E) < 4U) {
        if (!(*(volatile u16*)&work->field_6 & 1)) {
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[2], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[3], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[4], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x10], 0, 0);
        } else {
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[1], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x13], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x10], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->coords[0x12], 0, 0);
        }
    }
}

/// The 31 state handlers `func_actor_356100_80169854` dispatches through, in
/// state order; entry 0x1D has no handler and the `field_0` values the ticks
/// park (0, 6, 7, 8, 9, 0xB, 0xC, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x19,
/// 0x1E) are its live entries. Same role as `Actor01900_D1728C`.
const Actor356100StateTable D_actor_356100_80161EC4 = {
    {
        func_actor_356100_8016A1D8,
        func_actor_356100_8016A21C,
        func_actor_356100_8016A2AC,
        func_actor_356100_8016A340,
        func_actor_356100_80163CD4,
        func_actor_356100_8016A3D4,
        func_actor_356100_80163E2C,
        func_actor_356100_80164158,
        func_actor_356100_80164ACC,
        func_actor_356100_801653F4,
        func_actor_356100_80165B30,
        func_actor_356100_80166018,
        func_actor_356100_801666B4,
        func_actor_356100_8016A468,
        func_actor_356100_801668FC,
        func_actor_356100_8016A550,
        func_actor_356100_8016A5DC,
        func_actor_356100_8016A668,
        func_actor_356100_80166CF0,
        func_actor_356100_8016A710,
        func_actor_356100_8016A834,
        func_actor_356100_80167358,
        func_actor_356100_80167584,
        func_actor_356100_80167818,
        func_actor_356100_80167A7C,
        func_actor_356100_801684F0,
        func_actor_356100_8016804C,
        func_actor_356100_80168AFC,
        func_actor_356100_80168E44,
        NULL,
        func_actor_356100_80169180,
    }
};

void func_actor_356100_80169854(GpEnemy* arg0, Actor356100* arg1)
{
    VECTOR                  pos;
    Actor356100StateTable   tbl;
    Actor356100Work*        work;
    Actor356100GroundCoord* blk;
    s16                     next;

    work   = arg1->field_1C;
    tbl    = D_actor_356100_80161EC4;
    pos.vx = arg1->field_2C->coords[1].workm.t[0];
    pos.vy = arg1->field_2C->coords[1].workm.t[1];
    pos.vz = arg1->field_2C->coords[1].workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 0x15 && work->field_0 != 0x1E) {
                arg1->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->coords->workm.t, 0x180, Gp_State1C->field_8);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 0x15 && work->field_0 != 0x1E) {
                arg1->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->coords->workm.t, 0x180, Gp_State1C->field_8);
            }
            return;
        case 2:
            arg1->field_2C->flags = 0x80;
            return;
    }
    *(Actor356100GroundCoord**)G_SCRATCH_HEAD -= 1;
    blk                                        = *(Actor356100GroundCoord**)G_SCRATCH_HEAD;
    if (work->field_0 == 0x1E) {
        MATRIX* m;

        blk->v.vx = blk->v.vy = blk->v.vz = 0;
        Actor356100_TransformToView(&arg1->field_2C->coords[1], &blk->v);
        m                     = &blk->coord.coord;
        *(s32*)&m->m[0][0]    = 0x1000;
        *(s32*)&m->m[0][2]    = 0;
        *(s32*)&m->m[1][1]    = 0x1000;
        *(s32*)&m->m[2][0]    = 0;
        m->m[2][2]            = 0x1000;
        blk->coord.sub        = &Gfx_ViewCoord;
        blk->coord.coord.t[0] = blk->v.vx;
        blk->coord.coord.t[1] = 0;
        blk->coord.coord.t[2] = blk->v.vz;
        blk->coord.flg        = 0;
        Gp_UpdateCoord(&blk->coord);
        Gp_DrawEffGroundQuad((VECTOR3*)blk->coord.workm.t, 0x280, Gp_State1C->field_8);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    tbl.f[work->field_0](arg1);
    if (D_801153F2[1] == 1) {
        if (work->field_0 == 0x18) {
            work->field_0 = 6;
        }
    }
    blk->v.vx = 0;
    blk->v.vy = 0;
    blk->v.vz = 0;
    Actor356100_TransformToView(&arg1->field_2C->coords[2], &blk->v);
    work->field_B6C[work->field_BBC].vx        = blk->v.vx;
    work->field_B6C[work->field_BBC].vy        = blk->v.vy;
    work->field_B6C[work->field_BBC].vz        = blk->v.vz;
    *(Actor356100GroundCoord**)G_SCRATCH_HEAD += 1;
    next                                       = (u16)work->field_BBC + 1;
    work->field_BBC                            = next;
    if (next == 7) {
        work->field_BBC = 0;
    }
    if ((u32)((u16)work->field_97E - 0x14) < 2U) {
        arg0->field_1C.vx = work->field_B6C[work->field_BBC].vx;
        arg0->field_1C.vy = work->field_B6C[work->field_BBC].vy;
        arg0->field_1C.vz = work->field_B6C[work->field_BBC].vz;
    } else {
        arg0->field_1C.vx = blk->v.vx;
        arg0->field_1C.vy = blk->v.vy;
        arg0->field_1C.vz = blk->v.vz;
    }
    arg0->field_18 = &Gfx_ViewCoord;
}

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}
