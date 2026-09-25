#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_521100.h"
#include "actors/actors_shared_80132074.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by the yaw-facing body
/// `func_actor_521100_80134C38`: only the `SVECTOR` at +0x10 is written, and it
/// is the pure-yaw rotation `RotMatrix` builds into the attach coordinate. The
/// rotation the function aims is the coordinate's own Z axis read back through
/// `ratan2`, and the scratch below it goes unused. Same shape as
/// `Actor02500RotScratch` / `Actor300700RotScratch`.
typedef struct Actor521100RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor521100RotScratch;
STATIC_ASSERT_SIZEOF(Actor521100RotScratch, 0x18);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by the aim body
/// `func_actor_521100_80134EDC`: `view` is the player-relative position
/// `Gp_WorldToLocal` produces for the head coordinate's `workm`, `delta` the
/// player position with the 0x600 head offset applied, and `local` the same
/// delta rotated into the body's frame by `ApplyTransposeMatrixLV` and then
/// clamped. Same shape as `Actor510900AimScratch`.
typedef struct Actor521100AimScratch {
    /* 0x00 */ MATRIX view;
    /* 0x20 */ VECTOR delta;
    /* 0x30 */ VECTOR local;
} Actor521100AimScratch;
STATIC_ASSERT_SIZEOF(Actor521100AimScratch, 0x40);

/// The scratch-pad stack head at 0x1F8003FC, named rather than written as a
/// plain `u32` dereference because the untwist body
/// `func_actor_521100_80135024` reads it through a member: a component access
/// is an in-struct memory reference (`MEM_IN_STRUCT_P`), which keeps the
/// scheduler from treating that load and store as aliases of every other
/// memory operation in the block. Written as a scalar, the block gains false
/// dependences, the `work` / `coord` chains lose a priority step and the
/// prologue is scheduled in the wrong order. Same shape as
/// `Actor02000ScratchStack` / `Actor510900ScratchStack`.
typedef struct {
    u32 sp;
} Actor521100ScratchStack;

typedef struct Actor521100HitScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[0x18];
} Actor521100HitScratch;
STATIC_ASSERT_SIZEOF(Actor521100HitScratch, 0x48);

/// Payload of the 0x3F8 query `func_actor_521100_80132C70` sends the player
/// before it takes the hold; `field_14` is the range it asks for. The same
/// shape as `Actor103700Msg3F8`, `Actor510900Msg3F8` and `Actor400600Msg3F8`.
typedef struct Actor521100Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor521100Msg3F8;
STATIC_ASSERT_SIZEOF(Actor521100Msg3F8, 0x18);

/// Argument block of the "start animation" script opcode 0x7D3, whose handler
/// is `func_actor_521100_80135C14`. `field_0` is the animation bank and picks
/// the clip the bank starts at (`0` maps to 0x14, every other bank to 0x1D);
/// `field_4` is the offset of the clip inside that bank; `field_8`, when
/// non-zero, blends to it over `field_C` frames instead of snapping. Same
/// four-word shape as `Actor361100AnimPreset` and `Actor503500AnimPreset`.
typedef struct Actor521100AnimPreset {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[2];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
} Actor521100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor521100AnimPreset, 0x10);

typedef struct Actor521100FireRow {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100FireRow;

extern Actor521100FireRow D_actor_521100_8015F80C[][17];

typedef struct Actor521100FireMsg {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} Actor521100FireMsg;

typedef struct Actor521100FireAim {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor521100FireAim;

typedef struct Actor521100FireScratch {
    /* 0x00 */ VECTOR             pos;
    /* 0x10 */ VECTOR             delta;
    /* 0x20 */ SVECTOR            vec;
    /* 0x28 */ Actor521100FireMsg msg;
    /* 0x3C */ Actor521100FireAim aim;
} Actor521100FireScratch;
STATIC_ASSERT_SIZEOF(Actor521100FireScratch, 0x54);

extern MATRIX*  D_80073B8C;
extern u8       D_8011541B;
extern s16      D_actor_521100_8015F570[];
extern u16      D_actor_521100_8015F564;
extern u8       D_actor_521100_8015F7CC[];
extern GpEffArg D_actor_521100_8015F804;

/// Blend length in frames `func_actor_521100_80135964` seeds the slot walk
/// with when the clip changes, indexed by the incoming clip id; ids from 0x15
/// up keep the zero `val` starts at.
extern s16 D_actor_521100_8015F894[];
extern s16 D_actor_521100_8015F684[];

/// The three waypoints the state-6 body `func_actor_521100_80134774` walks the
/// actor to, one per phase `field_6A0` it switches on: `(-4000, 0, -2000)` for
/// phases 0 and 2, and `(-5250, 0, -1200)` for phase 1. Only `vx` and `vz` are
/// read, and only when the actor is too far from the player for that phase to
/// aim at it; the y of all three is zero, as the positions are on the floor.
extern VECTOR D_actor_521100_8015F654[];

/// Sixteen frames of the burn-out effect the state bodies at `field_6A0 == 1`
/// pick between on their last frame, indexed by the 4 bits under the top half
/// of an LCG draw (`(rng >> 16) & 0xF`). The sibling state body
/// `func_actor_521100_801357F0` reads the table one slot down at 0x8015F5F4.
extern u16 D_actor_521100_8015F634[];

/// The other sixteen-frame burn-out table, read by the state-5 body
/// `func_actor_521100_801357F0` off the same LCG draw bits the state-3 body
/// `func_actor_521100_8013570C` indexes `D_actor_521100_8015F634` with.
extern u16 D_actor_521100_8015F5F4[];

/// The burn-out effect table the sequence resets read, one 0x20-byte table
/// below `D_actor_521100_8015F5F4`: the state-1 body
/// `func_actor_521100_801335B4` draws from it both on the frame the actor
/// catches alight and on the frame the reset latch `field_6AA` has run out.
extern u16 D_actor_521100_8015F5D4[];

/// The 4-byte pair `func_actor_521100_801335B4` packs a type-2 record into and
/// copies onto both `obj57C` / `obj59C` at `field_18`, where the sibling
/// overlays' burn-out bodies put the same pair. Same shape as
/// `D_actor_510900_80167968` and `D_actor_400100_*`.
extern GpU16Pair D_actor_521100_8015F550;

/// One signed halfword choice in a three-row, two-choice transition table.
/// The selector combines the row and random-column byte offsets before
/// accessing this member. The member access also keeps GCC's structure-memory
/// annotation, allowing the independent RNG write to retain its schedule.
typedef struct Actor521100StateChoice {
    s16 state;
} Actor521100StateChoice;
STATIC_ASSERT_SIZEOF(Actor521100StateChoice, 2);

extern s8                     D_80114C12;
extern s16                    D_actor_521100_8015F57C[16];
extern Actor521100StateChoice D_actor_521100_8015F59C[6];
extern s16                    D_actor_521100_8015F5A8[16];
extern Actor521100StateChoice D_actor_521100_8015F5C8[6];

/// Frames between the burn-out effects `func_actor_521100_80135230` drops on
/// the attach coordinate, indexed by the sequence state `field_68C`: every
/// frame in state 0, then 7 / 0xE / 0x1C as the body burns out.
extern s16 D_actor_521100_8015F8CC[];

/// Coordinate slots the burn-out effects splash across when the sequence is in
/// state 1, chosen by the top 3 bits of an LCG draw.
extern s16 D_actor_521100_8015F8BC[];

extern u16 D_actor_521100_8015F614[];

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

extern GpPairSrcE D_actor_521100_8015F560;
extern TaskDesc   D_actor_521100_8015F6E4[];
extern u8         D_actor_521100_8015F6FC[];
extern u8         D_actor_521100_8015F73C[];

void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2);
void func_actor_521100_80132958(Task* arg0);
s32  func_actor_521100_80132C70(Task* arg0);
void func_actor_521100_80132DE8(Task* arg0);
void func_actor_521100_80133104(Task* arg0);
void func_actor_521100_8013334C(Task* arg0);
void func_actor_521100_801335B4(Task* arg0);
void func_actor_521100_801339B0(Task* arg0);
void func_actor_521100_80134658(Task* arg0);
void func_actor_521100_80134774(Task* arg0);
void func_actor_521100_80134C38(Task* arg0);
void func_actor_521100_80134D88(Task* arg0);
void func_actor_521100_80134EDC(Task* arg0);
void func_actor_521100_80135024(Task* arg0);
void func_actor_521100_80135230(Task* arg0);
void func_actor_521100_801353CC(GpEnemy* arg0, Task* arg1);
void func_actor_521100_80135414(GpEnemy* arg0, Task* arg1);
void func_actor_521100_80135478(GpEnemy* arg0, Task* arg1);
void func_actor_521100_801355C8(Task* arg0);
void func_actor_521100_80135680(Task* arg0);
void func_actor_521100_8013570C(Task* arg0);
void func_actor_521100_801357F0(Task* arg0);
void func_actor_521100_801358D4(Task* arg0);
void func_actor_521100_80135964(Task* arg0);
void func_actor_521100_80135A34(Task* arg0);
void func_actor_521100_80135A90(Task* arg0);
void func_actor_521100_80135B40(GpEnemy* enemy, Task* task);
void func_actor_521100_80135B80(GpEnemy* arg0, Task* task);

/// Declared locally with a signed `arg2`, as `include/gameplay/1BC.h` explains:
/// the definition takes `u16` so that its own body matches, but this caller
/// passes a sign-extended `s16` clip id.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Spawn state of the actor: allocates its 0x6C0 work block, registers the
/// enemy on the lock-on list with its parameter record, contact table and body
/// coordinate (the model's fourth part), and starts the animation on clip 0x15.
/// It then links the actor's collision bodies, spawns a second enemy from
/// `D_actor_521100_8015F6E4` with this one as its parent, dresses that enemy's
/// model with the texture page and CLUT of this enemy's placement, and links
/// two more pairs of bodies, one of them placed on the second enemy's model.
///
/// Every body's coordinate is assigned first in its block: the model pointer is
/// reloaded from the task each time, and that load has to precede the stores
/// into the work block, which it cannot be scheduled across.
void func_actor_521100_80131E8C(GpEnemy* enemy, Task* task)
{
    GpAreaKey        key;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor521100Work* work;
    GpEnemy*         spawned;
    TmdObject*       model;
    GpAreaKey*       sessionKey;
    GpAreaPlace*     place;
    s32              idx;
    u32              raw;
    s32              i;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(0x6C0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work      = work;
    obj->flags      = 0;
    coord->flg      = 0;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord         = &((TmdObject*)task->extra)->coords[3];
    enemy->bodyPos.vx    = 0;
    enemy->bodyPos.vy    = 0;
    enemy->bodyPos.vz    = 0;
    enemy->param         = &D_actor_521100_8015F560;
    enemy->recs          = work->rec534;
    enemy->hp            = D_actor_521100_8015F560.hpMax;
    work->eff.coord      = &((TmdObject*)task->extra)->coords[3];
    work->eff.spawnArgLo = 0x400;
    work->eff.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_521100_8015F73C, obj, work->poses, work->slots);
    work->field_686 = 0x15;
    work->field_688 = 0x15;
    i               = 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_686);
        i++;
    } while (i < 0x13);
    work->field_6B2 = 1;
    work->field_6B6 = -1;
    work->field_6B8 = -1;

    work->obj47C.coord    = ((TmdObject*)task->extra)->coords;
    work->obj47C.ctx.recs = work->rec49C;
    work->obj47C.pos.vx   = 0;
    work->obj47C.pos.vy   = -0x190;
    work->obj47C.pos.vz   = 0;
    work->obj47C.key      = 0x30022;
    work->obj47C.radius   = 0x190;
    work->obj47C.flags    = 1;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(work->rec49C, 5, 0);
    work->obj47C.flags |= 0x4200;

    work->obj514.coord    = &((TmdObject*)task->extra)->coords[3];
    work->obj514.ctx.recs = work->rec534;
    work->obj514.pos.vx   = 0;
    work->obj514.pos.vy   = 0;
    work->obj514.pos.vz   = 0;
    work->obj514.key      = 0x30022;
    work->obj514.radius   = 0x190;
    work->obj514.flags    = 1;
    Gp_LinkObj(2, &work->obj514);
    Gp_InitRec18Table(work->rec534, 3, 0);
    work->obj514.flags |= 0x8000;

    spawned    = Gp_SpawnEnemyFromTable(D_actor_521100_8015F6E4, 1, 0, enemy);
    model      = spawned->task->extra;
    raw        = enemy->placeKey;
    sessionKey = &gGameSession->at4.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    idx        = raw >> 12;
    key.view   = sessionKey->view;
    Gp_SyncAreaKeyIndex(&key);
    /* offset + base, as in the sibling spawn bodies: the ROM adds the scaled
       index onto the table. */
    place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    work->field_654 = spawned->task;

    work->obj57C.coord    = ((TmdObject*)spawned->task->extra)->coords;
    work->obj57C.pos.vx   = -0x226;
    work->obj57C.ctx.recs = work->rec5BC;
    work->obj57C.pos.vy   = 0x64;
    work->obj57C.pos.vz   = 0;
    work->obj57C.key      = 0;
    work->obj57C.radius   = 0x1C2;
    work->obj57C.flags    = 1;
    Gp_LinkObj(3, &work->obj57C);
    Gp_InitRec18Table(work->rec5BC, 1, 0);
    work->obj57C.flags &= 0x7FFF;

    work->obj59C.coord    = &((TmdObject*)task->extra)->coords[7];
    work->obj59C.ctx.recs = work->rec5BC;
    work->obj59C.pos.vx   = 0;
    work->obj59C.pos.vy   = 0;
    work->obj59C.pos.vz   = 0;
    work->obj59C.key      = 0;
    work->obj59C.radius   = 0x1C2;
    work->obj59C.flags    = 1;
    Gp_LinkObj(3, &work->obj59C);

    work->shape.end0.vz    = 0x5DC;
    work->shape.end0.vx    = 0;
    work->shape.end0.vy    = 0;
    work->shape.end1.vx    = 0;
    work->shape.end1.vy    = 0;
    work->shape.end1.vz    = 0;
    work->shape.end0Radius = 1;
    work->shape.end1Radius = 1;
    work->shape.recs       = work->rec62C;
    work->obj59C.flags    &= 0x7FFF;

    work->obj5D4.coord     = ((TmdObject*)task->extra)->coords;
    work->obj5D4.ctx.d4rec = &work->shape;
    work->obj5D4.pos.vy    = -0x1F4;
    work->obj5D4.pos.vx    = 0;
    work->obj5D4.pos.vz    = 0;
    work->obj5D4.key       = 0;
    work->obj5D4.radius    = 0;
    work->obj5D4.flags     = 3;
    Gp_LinkObj(3, &work->obj5D4);
    Gp_InitRec18Table(work->rec62C, 1, 0);
    work->obj5D4.flags |= 0x4000;

    work->obj5F4.coord    = ((TmdObject*)task->extra)->coords;
    work->obj5F4.pos.vy   = -0x320;
    work->obj5F4.ctx.recs = work->rec62C;
    work->obj5F4.pos.vx   = 0;
    work->obj5F4.pos.vz   = 0x4E2;
    work->obj5F4.key      = 0;
    work->obj5F4.radius   = 0x1C2;
    work->obj5F4.flags    = 1;
    Gp_LinkObj(3, &work->obj5F4);
    work->obj5F4.flags |= 0x4000;

    task->msgTable = D_actor_521100_8015F6FC;
    task->state    = 1;
}

static __inline__ s32 Actor521100_GetHitType(s32 key)
{
    if (key & 0x8000) {
        return 1;
    }
    return D_actor_521100_8015F684[key & 0x3F];
}

void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2)
{
    Actor521100HitScratch* scratch;
    Actor521100Work*       work;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    u32                    lastId;
    u32                    sound;
    u32                    kind;
    u32                    damage;
    u32                    rng;
    u32                    rng2;
    u32                    r;
    s32                    result;
    s32                    dx;
    s32                    coordX;
    s32                    dz;
    s32                    absDiff;
    s32                    r2;
    s32                    angle;
    s32                    angle2;
    s32                    hitType;
    s32                    i;
    s16                    diff;
    s16                    wrap;
    s16                    cooldown;
    s32                    pan;
    s32                    pan1;
    s32                    pan2;
    s32                    depth;
    s16                    wait;

    lastId  = 0;
    work    = arg0->work;
    scratch = (Actor521100HitScratch*)(SCRATCH_SP -= 0x48);
    coord   = ((TmdObject*)arg0->extra)->coords;
    enemy   = arg0->spawnArg2;
    result  = func_800E0C10(work->rec49C, &scratch->delta, 5, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[1] += scratch->delta.vy.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_64C;
            coord->coord.t[1] = work->field_64E;
            coord->coord.t[2] = work->field_650;
            break;
    }
    Gp_ClearRec18Occupied(work->rec49C);
    if (work->field_684 != 0) {
        cooldown        = (u16)work->field_684 - 1;
        work->field_684 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_684 = 0;
        }
    }
    if (work->field_6AC != 0) {
        work->field_6AC = (u16)work->field_6AC - 1;
    }
    for (i = 0; i < 3; i++) {
        kind = (u16)(work->rec534[i].key >> 0x10);
        if (kind < 2) {
            continue;
        }
        if (kind != 2) {
            continue;
        }
        if (work->field_684 != 0) {
            continue;
        }
        coordX              = coord->coord.t[0];
        dx                  = Player_Status.coordMtx->t[0] - coordX;
        scratch->delta.vx.w = dx;
        scratch->delta.vy.w = 0;
        dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->delta.vz.w = dz;
        damage              = Gp_ComputeDamage(work->rec534[i].key, SquareRoot0(dx * dx + dz * dz), 0, 0);
        hitType             = Actor521100_GetHitType(work->rec534[i].key);
        if (hitType == 1) {
            if (work->field_69E == 2) {
                hitType = 0;
            } else {
                diff    = work->field_696 - (ratan2((s16)scratch->delta.vx.w, (s16)scratch->delta.vz.w) & 0xFFF);
                absDiff = abs(diff);
                if (absDiff < 0x800) {
                    wrap = absDiff;
                } else if (diff > 0) {
                    wrap = 0x1000 - diff;
                } else {
                    wrap = diff + 0x1000;
                }
                if ((wrap >= 0x301) || (work->field_6AE == 1)) {
                    hitType = 2;
                }
            }
        }
        switch (hitType) {
            case 0:
                rng         = Gp_LcgState * 5 + 0x71357911;
                r           = rng >> 0x10;
                angle       = (r & 0x7F) + 0x40;
                Gp_LcgState = rng;
                if (!(r & 1)) {
                    angle = -angle;
                }
                work->field_678.vx = angle;
                r2                 = (s16)r >> 8;
                angle2             = (r2 & 0x7F) + 0x40;
                if (!(r2 & 1)) {
                    angle2 = -angle2;
                }
                work->field_678.vy = angle2;
                work->field_680    = 1;
                damage           >>= 1;
                if ((Gp_GetIdParam0(work->rec534[i].key) & 0xFFFF) == 5) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 2, NULL);
                }
                if ((work->field_69E == 0) && (work->field_6AC <= 0)) {
                    work->field_69E = 5;
                    work->field_6A0 = 0;
                    rng2            = Gp_LcgState * 5 + 0x71357911;
                    work->field_6AC = ((rng2 >> 0x10) & 0xFF) + 0x96;
                    Gp_LcgState     = rng2;
                    sound           = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0006;
                    pan             = (s8)Gp_GetObjPan(coord);
                    depth           = (s8)gpGetObjDepth(coord);
                    SndEvt_EnqueueType6((s32)sound, pan, depth);
                    goto damage_done;
                }
                goto damage_done;
            case 1:
                work->field_69E = 4;
                work->field_6A0 = 0;
                if (work->rec534[i].key & 0x8000) {
                    damage >>= 2;
                } else {
                    damage >>= 3;
                }
                sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0003;
                pan1  = (s8)Gp_GetObjPan(coord);
                depth = (s8)gpGetObjDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan1, depth);
                goto damage_done;
            case 2:
                work->field_69E = 3;
                work->field_6A0 = 0;
                work->field_6AE = 0;
                if (work->rec534[i].key & 0x8000) {
                    damage *= 2;
                } else {
                    damage >>= 1;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((Gp_LcgState >> 16) & 1) {
                    sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0004;
                } else {
                    sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0005;
                }
                pan2  = (s8)Gp_GetObjPan(coord);
                depth = (s8)gpGetObjDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan2, depth);
        }
    damage_done:
        func_800E2C78(enemy, (s32)work->rec534[i].key, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        enemy->hp = (u16)enemy->hp - damage;
        if (lastId != work->rec534[i].key) {
            lastId = work->rec534[i].key;
            func_800FDB18(Gp_GetIdParam1(work->rec534[i].key) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[3], NULL, &work->eff);
        }
        wait = Gp_GetIdParam2(work->rec534[i].key);
        if (wait > 0) {
            work->field_684 = wait;
        }
    }
    Gp_ClearRec18Occupied(work->rec534);
    if (work->rec5BC[0].flags & 1) {
        work->obj57C.flags &= 0x7FFF;
        work->obj59C.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(work->rec5BC);
        work->field_6A6 = 1;
    }
    work->field_6BE = 0;
    if (work->rec62C[0].flags & 1) {
        work->field_6BE = 1;
        Gp_ClearRec18Occupied(work->rec62C);
    }
    if (enemy->hp <= 0) {
        if (D_80073BA0 > 0) {
            work->field_6B2 = 0;
        } else {
            enemy->hp = 1;
        }
    }
    SCRATCH_SP += 0x48;
}

void func_actor_521100_80132958(Task* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    u16*             tbl;
    u16*             tbl1;
    u16*             tbl2;
    u32              rng;
    u32              rng1;
    u32              rng2;
    s16              state;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;

    coord                     = ((TmdObject*)arg0->extra)->coords;
    work                      = arg0->work;
    scratchEnd                = *(VECTOR**)G_SCRATCH_HEAD;
    vec                       = scratchEnd - 1;
    *(VECTOR**)G_SCRATCH_HEAD = vec;
    scratchEnd[-1].vx         = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                   = 0;
    vec->vz                   = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    work->field_6AA           = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
    angle                     = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
    work->field_698           = angle;
    if (gGameSession->at4.loc.view == 2) {
        work->field_69E = 6;
        work->field_6A0 = 0;
    } else {
        state = work->field_6A0;
        switch (state) {
            case 0:
                work->field_69A = 0;
                work->field_69C = 0;
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer < 0) {
                    work->field_6A0 = 1;
                    work->field_686 = 0x12;
                    tbl             = D_actor_521100_8015F614;
                    rng             = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = rng;
                    work->field_68E = tbl[(rng >> 16) & 0xF];
                } else if (func_actor_521100_80132C70(arg0) == 0) {
                    func_actor_521100_80135680(arg0);
                }
                break;
            case 1:
                if ((work->field_6BE == state) && (work->field_6AA < 0x7D0)) {
                    delta     = (u16)angle - (u16)work->field_696;
                    magnitude = abs(delta);
                    if (magnitude < 0x800) {
                        wrapped = magnitude;
                    } else {
                        if (delta > 0) {
                            wrapped = 0x1000 - delta;
                        } else {
                            wrapped = delta + 0x1000;
                        }
                    }
                    if (wrapped < 0x100) {
                        work->field_68E = 0;
                    }
                }
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer <= 0) {
                    work->field_69C = 0x78;
                    work->field_69A = 0;
                    tbl1            = D_actor_521100_8015F5F4;
                    rng1            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = rng1;
                    timer           = tbl1[(rng1 >> 16) & 0xF];
                    work->field_68E = timer;
                    if (timer == 0) {
                        func_actor_521100_80135680(arg0);
                        if (work->field_69E == 0) {
                            tbl2            = D_actor_521100_8015F614;
                            rng2            = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState     = rng2;
                            work->field_68E = tbl2[(rng2 >> 16) & 0xF];
                        }
                    } else {
                        work->field_6A0 = 0;
                        work->field_686 = 1;
                    }
                } else {
                    work->field_69A = 0x14;
                    work->field_69C = 0x78;
                    func_actor_521100_80132C70(arg0);
                }
                break;
        }
    }
    work->field_6AE         = 0;
    *(void**)G_SCRATCH_HEAD = (void*)(*(void**)G_SCRATCH_HEAD + 0x10);
}

/// Asks the player for the hold (message 0x3F8, range 0x19) once the actor has
/// swung its heading to within 0x20 of the slot-3 task's own and is lined up
/// to latch on. The heading error is the 12-bit difference between the work
/// block's `field_698` and `field_696`, wrapped into [-0x800, 0x800] and then
/// narrowed by `field_69C` being armed with 0x50; the request goes out only
/// while fewer than 0x4E2 units of the actor's health are left, the latch
/// `field_6BE` is clear, `D_80073BA0` (the remaining-enemy count) is positive
/// and the player's own `GameActor::field_954` is not its mode 2. On acceptance
/// the body rearms the motion state (2 into `field_69E`, 0xA frames of blend
/// into `field_686`, the 0xA/0xFF/0x80 pad lerp) and returns 1; the 0x3F8
/// query buffer is the 0x18 bytes `SCRATCH_SP` is pushed by.
s32 func_actor_521100_80132C70(Task* arg0)
{
    Actor521100Work*   work;
    Task*              player;
    Actor521100Msg3F8* msg;
    s16                diff;
    s32                adiff;
    s16                wrap;
    s32                ret;

    work   = arg0->work;
    player = gameGetPtrSlot(3);
    msg    = (Actor521100Msg3F8*)(SCRATCH_SP -= 0x18);

    diff  = work->field_698 - work->field_696;
    adiff = diff >= 0 ? diff : -diff;
    ret   = 0;
    if (adiff < 0x800) {
        wrap = adiff;
    } else if (diff > 0) {
        wrap = 0x1000 - diff;
    } else {
        wrap = diff + 0x1000;
    }
    if ((wrap < 0x400) && (work->field_6AA < 0x4E2) && (work->field_6BE == 0) && (D_80073BA0 > 0) && (work->field_69C = 0x50, (wrap < 0x20)) && (((GameActor*)player->work)->field_954 != 2)) {
        msg->field_14 = 0x19;
        if (Gp_DispatchMsg(player, 0x3F8, (s32)msg, 0) == 0) {
            ret             = 1;
            work->field_6A8 = 0;
            work->field_69E = 2;
            work->field_6A0 = 0;
            work->field_6A2 = 0;
            work->field_686 = 0xA;
            work->field_69A = 0;
            work->field_69C = 0;
            Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_SP += 0x18;
    return ret;
}
void func_actor_521100_80132DE8(Task* arg0)
{
    Actor521100Work*        work;
    GsCOORDINATE2*          coord;
    VECTOR*                 head;
    VECTOR*                 vec;
    Actor521100StateChoice* pairNear;
    Actor521100StateChoice* pairFar;
    s16*                    flatNear;
    s16*                    flatFar;
    u16                     prev;
    u32                     rngPN;
    u32                     rngPF;
    u32                     rngFN;
    u32                     rngFF;
    s32                     packed;
    s32                     next;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    head  = *(VECTOR**)G_SCRATCH_HEAD;
    vec   = head - 1;

    *(VECTOR**)G_SCRATCH_HEAD = vec;
    head[-1].vx               = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                   = 0;
    vec->vz                   = Player_Status.coordMtx->t[2] - coord->coord.t[2];

    work->field_6AA = SquareRoot0(head[-1].vx * head[-1].vx + vec->vz * vec->vz);
    work->field_698 = ratan2((s16)head[-1].vx, (s16)vec->vz) & 0xFFF;

    switch (work->field_6A0) {
        case 0:
            if (((u32)((u8)D_80114C12 - 2) >= 2U) && (gGameSession->at4.loc.view != 2)) {
                if (work->field_6AA < 0x8FC) {
                    if (work->field_6B8 == work->field_6B6) {
                        pairNear    = D_actor_521100_8015F59C;
                        rngPN       = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rngPN;
                        next        = ((Actor521100StateChoice*)((u8*)pairNear + (work->field_6B6 * 4 + ((rngPN >> 16) & 1) * 2)))->state;
                    } else {
                        flatNear    = D_actor_521100_8015F57C;
                        rngFN       = Gp_LcgState * 5 + 0x71357911;
                        next        = flatNear[(rngFN >> 16) & 0xF];
                        Gp_LcgState = rngFN;
                    }
                } else {
                    if (work->field_6B8 == work->field_6B6) {
                        pairFar     = D_actor_521100_8015F5C8;
                        rngPF       = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rngPF;
                        next        = ((Actor521100StateChoice*)((u8*)pairFar + (work->field_6B6 * 4 + ((rngPF >> 16) & 1) * 2)))->state;
                    } else {
                        flatFar     = D_actor_521100_8015F5A8;
                        rngFF       = Gp_LcgState * 5 + 0x71357911;
                        next        = flatFar[(rngFF >> 16) & 0xF];
                        Gp_LcgState = rngFF;
                    }
                }
            } else {
                next = 2;
            }

            prev            = (u16)work->field_6B6;
            work->field_6B6 = next;
            work->field_6B8 = prev;

            switch (next) {
                case 0:
                    work->field_6A0  = 1;
                    work->field_686  = 5;
                    packed           = Gp_PackPair(&D_actor_521100_8015F550, 0);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 1:
                    work->field_6A0  = 2;
                    work->field_686  = 6;
                    packed           = Gp_PackPair(&D_actor_521100_8015F550, 1);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 2:
                    work->field_6A0  = 3;
                    work->field_6A2  = 0;
                    work->field_686  = 3;
                    packed           = Gp_PackPair(&D_actor_521100_8015F550, 2);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
            }
            break;
        case 1:
            func_actor_521100_80133104(arg0);
            break;
        case 2:
            func_actor_521100_8013334C(arg0);
            break;
        case 3:
            func_actor_521100_801335B4(arg0);
            break;
    }
    SCRATCH_SP += 0x10;
}

void func_actor_521100_80133104(Task* arg0)
{
    GsCOORDINATE2*   coord;
    SVECTOR*         vec;
    SVECTOR*         head;
    s16*             clipPtr;
    s16              frame2;
    s16              frame3;
    s16              frame;
    s16              clip;
    s16              speed;
    s32              snd;
    s32              pan;
    GsCOORDINATE2*   effectCoord;
    s32              effect;
    s32              kind;
    SVECTOR*         offset;
    u16*             tbl;
    u16              clipId;
    u16              part;
    u32              rng;
    Actor521100Work* work;

    head                                           = (SVECTOR*)((Actor521100ScratchStack*)G_SCRATCH_HEAD)->sp;
    vec                                            = head - 1;
    ((Actor521100ScratchStack*)G_SCRATCH_HEAD)->sp = (u32)vec;
    work                                           = arg0->work;
    frame                                          = (s16)work->field_68A;
    clipPtr                                        = &D_actor_521100_8015F894[work->field_686];
    clip                                           = *clipPtr;
    clipId                                         = (u16)*clipPtr;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    if (frame == (clip + 0x1A)) {
        effect      = 0x60188;
        kind        = 0xC;
        effectCoord = coord;
        SOFT_TOUCH_REG(effectCoord);
        offset = NULL;
        SOFT_TOUCH_REG4(effect, kind, effectCoord, offset);
        Gp_SpawnEff(effect, &effectCoord[8], kind, offset);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == (clip + 0x1E)) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_654->extra)->coords, 0, vec);
    }
    frame2 = (s16)work->field_68A;
    if (frame2 == ((s16)clipId + 0x1C)) {
        work->field_6AE    = 1;
        work->obj57C.flags = (u16)(work->obj57C.flags | 0x8000);
        work->obj59C.flags = (u16)(work->obj59C.flags | 0x8000);
        snd                = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401C0008;
        pan                = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == ((s16)clipId + 0x28)) {
            work->field_6A6    = 0;
            work->obj57C.flags = (u16)(work->obj57C.flags & 0x7FFF);
            work->obj59C.flags = (u16)(work->obj59C.flags & 0x7FFF);
        }
    }
    frame3 = (s16)work->field_68A;
    if (frame3 >= ((s16)clipId + 0x1C)) {
        if (((s16)clipId + 0x1E) >= frame3) {
            speed = 0x64;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= ((s16)clipId + 0x7A)) {
        work->field_686 = 1;
        tbl             = D_actor_521100_8015F5F4;
        rng             = (Gp_LcgState * 5) + 0x71357911;
        work->field_69E = 0;
        work->field_6A0 = 0;
        part            = tbl[(rng >> 16) & 0xF];
        Gp_LcgState     = rng;
        work->field_6AE = 0;
        work->field_68E = part;
    }
    ((Actor521100ScratchStack*)G_SCRATCH_HEAD)->sp += 8;
}

/// Runs one frame of the burn-out sequence timed off the clip the slots are
/// playing: `D_actor_521100_8015F894[field_686]` is the clip's own length, read
/// signed and again unsigned because the cue frames below need it both ways,
/// and `field_68A` is the frame counter the blend in
/// `func_actor_521100_80135964` ticks. The counter is re-read at each cue
/// rather than carried, so the effects spawned in between cannot leave a stale
/// copy behind.
///
/// The cues, all offsets from that length: under +0x28 the turn limit
/// `field_69C` is held at 0x50; at +0x23 effect 0x60188 drops onto the attach
/// coordinate eight slots along and the 0xA/0x40/0xFF pad lerp starts; at
/// +0x27 the 8-byte scratch `SVECTOR` is thrown to (-0x320, 0x64, 0) and handed
/// to effect 0x6009C on the coordinate `field_654`'s own display object
/// carries; and +0x23 again, this time against the unsigned length, arms
/// `field_6AE` and raises the two record flags at 0x59A / 0x5BA together, then
/// cues `SndEvt_EnqueueType6` with the actor's pan and depth narrowed to bytes.
/// +0x2D hands the flags back down and clears the parked animation `field_6A6`.
///
/// The two ends are the motion: `field_69A` is held at 0x88 of forward speed
/// while the counter is between +0x20 and +0x2A of the length, and is zero
/// everywhere else, and past +0x90 the sequence starts over - clip 1, a fresh
/// effect id out of `D_actor_521100_8015F5F4` (the top four bits of an LCG
/// draw) into `field_68E`, and the state latch `field_69E`, its phase
/// `field_6A0` and the armed flag all cleared.
void func_actor_521100_8013334C(Task* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    u32              rng;
    s16              clip;
    u16              clipId;
    s16              turn;
    s16              speed;
    s16              frame;
    s16              frame2;
    s32              frame3;
    s32              snd;
    s32              pan;

    work                       = arg0->work;
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    clip                       = D_actor_521100_8015F894[work->field_686];
    clipId                     = D_actor_521100_8015F894[work->field_686];
    coord                      = ((TmdObject*)arg0->extra)->coords;

    turn = 0;
    if ((s16)work->field_68A < clip + 0x28) {
        turn = 0x50;
    }
    work->field_69C = turn;

    frame = (s16)work->field_68A;
    if (frame == clip + 0x23) {
        Gp_SpawnEff(0x60188, ((TmdObject*)arg0->extra)->coords + 8, 0xC, NULL);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x27) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_654->extra)->coords, 0, vec);
    }

    frame2 = (s16)work->field_68A;
    if (frame2 == (s16)clipId + 0x23) {
        work->field_6AE     = 1;
        work->obj57C.flags |= 0x8000;
        work->obj59C.flags |= 0x8000;
        snd                 = (((u32)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C0008;
        pan                 = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == (s16)clipId + 0x2D) {
            work->field_6A6     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
        }
    }

    frame3 = (s16)work->field_68A;
    if ((s16)clipId + 0x20 < frame3) {
        if ((s16)clipId + 0x2A >= frame3) {
            speed = 0x88;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= (s16)clipId + 0x90) {
        work->field_686 = 1;
        work->field_69E = 0;
        work->field_6A0 = 0;
        tbl             = D_actor_521100_8015F5F4;
        rng             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rng;
        work->field_68E = tbl[(rng >> 16) & 0xF];
        work->field_6AE = 0;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}
/// Step-0 body of the burn-out sequence: the transition into it and the two
/// respawn draws. `field_6A2` is a four-phase latch. Phase 0 waits out the clip
/// long enough for the actor to commit (`D_actor_521100_8015F894[field_686]`
/// plus 0x38) and then latches clip 4 and hands phase 1 a fresh effect id out
/// of `D_actor_521100_8015F5D4`, cueing the 0x401C0007 sound with the actor's
/// own pan and depth. Phase 1 counts the effect id down (unsigned `field_68E`,
/// tested as a halfword) and, when it lands, either arms the two collision
/// nodes with the type-2 pair and asks for clip 8, or - once `field_6AA` has
/// run out - drops the actor back to idle with a draw out of
/// `D_actor_521100_8015F5F4`.
///
/// Phase 2 walks the turn limit `field_69C` 0x3C up while the clip is young,
/// fires the 0x401C0009 cue, the effect 0x60188 on the eighth coordinate and
/// the 0xA/0x40/0xFF pad lerp together on the clip's 0x20th frame, holds the
/// forward speed at 0x64 across the 0x22..0x26 window, and at 0x27 latches
/// phase 3 and hands both record flags back. Phase 3 waits 0x5E frames and then
/// picks the finish off `coord->coord.t[0]`: under -0xFA0 the actor stays
/// burning (phase 2 of the latch, or 1 in the session's mode 2), otherwise it
/// resets to idle with the clip-1 draw.
void func_actor_521100_801335B4(Task* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    u32              rng;
    u16              timer;
    s16              turn;
    s32              snd;
    s32              pair;

    SCRATCH_SP -= 0x18;
    work        = arg0->work;
    coord       = ((TmdObject*)arg0->extra)->coords;

    switch (work->field_6A2) {
        case 0:
            if ((s16)work->field_68A >= D_actor_521100_8015F894[work->field_686] + 0x38) {
                u16* tbl        = D_actor_521100_8015F5D4;
                work->field_686 = 4;
                work->field_6A2 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
                snd             = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C0007;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            break;
        case 1:
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer > 0) {
                break;
            }
            if (work->field_6AA >= 0xDAC) {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_6A2 = 0;
                work->field_686 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
            } else {
                work->field_6A2  = 2;
                work->field_686  = 8;
                pair             = Gp_PackPair(&D_actor_521100_8015F550, 2);
                work->obj57C.key = pair;
                work->obj59C.key = pair;
            }
            break;
        case 2:
            turn = 0;
            if ((s16)work->field_68A < 0x20) {
                turn = 0x3C;
            }
            work->field_69C = turn;
            if ((s16)work->field_68A == 0x20) {
                work->field_6AE     = 1;
                work->obj57C.flags |= 0x8000;
                snd                 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C0009;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                Gp_SpawnEff(0x60188, ((TmdObject*)arg0->extra)->coords + 8, 8, NULL);
                Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
            }
            if ((u32)(work->field_68A - 0x22) < 5) {
                work->field_69A = 0x64;
            } else {
                work->field_69A = 0;
            }
            if ((s16)work->field_68A >= 0x27) {
                work->field_6A2     = 3;
                work->field_686     = 7;
                work->field_6A6     = 0;
                work->obj57C.flags &= 0x7FFF;
                work->obj59C.flags &= 0x7FFF;
            }
            break;
        case 3:
            if ((s16)work->field_68A < 0x5E) {
                break;
            }
            if (gGameSession->at4.loc.view == 2) {
                work->field_69E = 6;
                if (coord->coord.t[0] < -0xFA0) {
                    work->field_6A0 = 1;
                } else {
                    work->field_6A0 = 0;
                }
            } else if (coord->coord.t[0] < -0xFA0) {
                work->field_69E = 6;
                work->field_6A0 = 2;
            } else {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_686 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
            }
            work->field_6AE = 0;
            break;
    }
    SCRATCH_SP += 0x18;
}

void func_actor_521100_801339B0(Task* arg0)
{
    Actor521100Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          pcoord;
    Task*                   player;
    Actor521100FireScratch* sc;
    s32                     flag;
    s32                     i;
    s32                     snd;
    s32                     absDiff;
    s32                     angle;
    s16                     state;
    s16                     turn;
    u16                     timer;
    u32                     rng;
    u16*                    tbl;

    work        = arg0->work;
    coord       = ((TmdObject*)arg0->extra)->coords;
    player      = gameGetPtrSlot(3);
    SCRATCH_SP -= 0x54;
    sc          = (Actor521100FireScratch*)SCRATCH_SP;

    switch (work->field_6A0) {
        case 0:
            if ((s16)work->field_68A == 0xA) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 6;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else if ((s16)work->field_68A == 0xC) {
                flag             = (D_80073B8C->m[0][2] * coord->coord.m[0][2] + D_80073B8C->m[1][2] * coord->coord.m[1][2] + D_80073B8C->m[2][2] * coord->coord.m[2][2]);
                work->field_6A4  = (u32)flag >> 31;
                sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                sc->msg.field_4  = work->field_6A4 ? 2 : 6;
                sc->msg.field_8  = 0;
                sc->msg.field_C  = 0;
                sc->msg.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
            } else if ((s16)work->field_68A >= 0x2B) {
                work->field_6A0  = 1;
                work->field_686  = 0xB;
                work->field_68E  = 0;
                work->field_690  = 0;
                sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                sc->msg.field_4  = work->field_6A4 ? 3 : 7;
                sc->msg.field_8  = 0;
                sc->msg.field_C  = 0;
                sc->msg.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
            }
            if ((u16)(work->field_68A - 4) < 9) {
                sc->vec.vz = 0x4E2;
                sc->vec.vx = 0;
                sc->vec.vy = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                sc->pos.vx   = coord->coord.t[0] + sc->pos.vx;
                sc->pos.vy   = coord->coord.t[1] + sc->pos.vy;
                sc->pos.vz   = coord->coord.t[2] + sc->pos.vz;
                pcoord       = ((TmdObject*)player->extra)->coords;
                sc->delta.vx = sc->pos.vx - pcoord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = sc->pos.vz - pcoord->coord.t[2];
                if ((SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x32) || ((s16)work->field_68A == 0xC)) {
                    sc->aim.pos.vx = sc->pos.vx;
                    sc->aim.pos.vy = sc->pos.vy;
                    sc->aim.pos.vz = sc->pos.vz;
                } else {
                    VectorNormal(&sc->delta, &sc->pos);
                    sc->aim.pos.vx = pcoord->coord.t[0] + ((sc->pos.vx * 0x32) >> 12);
                    sc->aim.pos.vy = pcoord->coord.t[1] + ((sc->pos.vy * 0x32) >> 12);
                    sc->aim.pos.vz = pcoord->coord.t[2] + ((sc->pos.vz * 0x32) >> 12);
                }
                angle          = ratan2(pcoord->coord.m[0][2], pcoord->coord.m[2][2]) & 0xFFF;
                flag           = (s16)work->field_696 - angle;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vz = 0;
                if ((s16)work->field_68A == 0xC) {
                    sc->aim.rot.vy = work->field_696;
                } else {
                    absDiff = flag >= 0 ? flag : -flag;
                    if ((u32)(absDiff - 0x400) >= 0x801U) {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = work->field_696;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle + 0x64;
                        } else {
                            sc->aim.rot.vy = angle - 0x64;
                        }
                    } else {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = (work->field_696 + 0x800) & 0xFFF;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle - 0x64;
                        } else {
                            sc->aim.rot.vy = angle + 0x64;
                        }
                    }
                }
                Gp_DispatchMsg(player, 0x3E9, (s32)&sc->aim, 0);
            }
            break;
        case 1:
            flag = 0;
            if (work->field_68E == 2) {
                Gp_SpawnPadLerp(5, 0xC0, 0x80);
            }
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer <= 0) {
                if (D_80073BA0 <= D_actor_521100_8015F570[D_8011541B]) {
                    work->field_686  = 0x14;
                    work->field_6A0  = 5;
                    sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                    sc->msg.field_4  = work->field_6A4 ? 0xC : 0xD;
                    sc->msg.field_8  = 0;
                    sc->msg.field_C  = 0;
                    sc->msg.field_10 = 1;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
                    flag = 1;
                } else {
                    work->field_68E = 0x20;
                    Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_521100_8015F550, 3), 0);
                }
            }
            if (flag != 1) {
                flag = 0;
                if ((work->field_6A8 == 1) && (D_80073BA0 < 0x3D) && (((D_actor_521100_8015F564 / 3) & 0xFFFF) >= ((GpEnemy*)arg0->spawnArg2)->hp)) {
                    flag = work->field_6A4 == 1;
                }
                if (flag != 0) {
                    work->field_6A0  = 3;
                    work->field_6A8  = 0;
                    work->field_68A  = 0;
                    sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                    sc->msg.field_4  = 5;
                    sc->msg.field_8  = 0;
                    sc->msg.field_C  = 0;
                    sc->msg.field_10 = 1;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
                } else {
                    if (work->field_6A8 == 0) {
                        timer           = work->field_690 + 1;
                        work->field_690 = timer;
                        if ((s16)timer < 0x97) {
                            break;
                        }
                    }
                    work->field_6A0  = 2;
                    work->field_686  = 0x13;
                    work->field_6A8  = 0;
                    sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                    sc->msg.field_4  = work->field_6A4 ? 9 : 0xA;
                    sc->msg.field_8  = 0;
                    sc->msg.field_C  = 0;
                    sc->msg.field_10 = 1;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
                }
            }
            break;
        case 2:
            if ((s16)work->field_68A == 0x22) {
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if ((s16)work->field_68A == 0x25) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C000F;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_68A < 0x45) {
                for (i = 0; i < 0x11; i++) {
                    if ((s16)work->field_68A < D_actor_521100_8015F80C[work->field_6A4][i].field_0) {
                        sc->vec.vx = D_actor_521100_8015F80C[work->field_6A4][i].field_2;
                        break;
                    }
                }
                sc->vec.vy = 0;
                sc->vec.vz = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                pcoord         = ((TmdObject*)player->extra)->coords;
                sc->aim.pos.vx = pcoord->coord.t[0] + sc->pos.vx;
                sc->aim.pos.vy = pcoord->coord.t[1] + sc->pos.vy;
                sc->aim.pos.vz = pcoord->coord.t[2] + sc->pos.vz;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = work->field_696;
                sc->aim.rot.vz = 0;
                Gp_DispatchMsg(player, 0x3E9, (s32)&sc->aim, 0);
            }
            if ((s16)work->field_68A == 0x23) {
                Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 3, 0x80003400, NULL);
            }
            if ((s16)work->field_68A == 0x45) {
                sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                sc->msg.field_4  = 0xB;
                sc->msg.field_8  = 0;
                sc->msg.field_C  = 0;
                sc->msg.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
            }
            turn = 0;
            if ((s16)work->field_68A < 0x5F) {
                turn = -0x10;
            }
            work->field_69A = turn;
            if ((s16)work->field_68A == 0x6F) {
                coord          = ((TmdObject*)player->extra)->coords;
                sc->aim.pos.vx = coord->coord.t[0];
                sc->aim.pos.vy = coord->coord.t[1];
                sc->aim.pos.vz = coord->coord.t[2];
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = (work->field_696 + 0x800) & 0xFFF;
                sc->aim.rot.vz = 0;
                Gp_DispatchMsg(player, 0x3E9, (s32)&sc->aim, 0);
            }
            if (((s16)work->field_68A >= 0x6F) && (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0)) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
            }
            if ((s16)work->field_68A >= 0xA4) {
                tbl             = D_actor_521100_8015F5F4;
                work->field_686 = 1;
                work->field_69E = 0;
                work->field_6A0 = 0;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            break;
        case 3:
            if ((s16)work->field_68A == 0x20) {
                Gp_SpawnEff(0x60273, ((TmdObject*)gameGetPtrSlot(3)->extra)->coords + 0xC, 0, NULL);
                snd = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                work->field_6A0  = 4;
                work->field_686  = 0xD;
                sc->msg.field_0  = (void*)&D_actor_521100_8015F7CC;
                sc->msg.field_4  = 4;
                sc->msg.field_8  = 0;
                sc->msg.field_C  = 0;
                sc->msg.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->msg, 0);
            }
            break;
        case 4:
            if ((s16)work->field_68A == 0xF) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C000C;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_68A == 0x64) {
                ((GpEnemy*)arg0->spawnArg2)->hp = 0;
            }
            break;
        case 5:
            if ((s16)work->field_68A == 0x1A) {
                ((GameActor*)player->work)->field_956 = 0xA;
                work->field_6A0                       = 6;
                work->field_68E                       = 0;
                gGameSession->deathRestartDelay       = 0x5A;
                gGameSession->areaBgmCountdown        = 0x7F;
                sc->vec.vx                            = 0;
                sc->vec.vy                            = -0x96;
                sc->vec.vz                            = 0xC8;
                func_800FDB18(1, ((TmdObject*)gameGetPtrSlot(3)->extra)->coords + 4, &sc->vec,
                              &D_actor_521100_8015F804);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                D_80073BA0 = 0;
            }
            break;
        case 6:
            state = (s16)work->field_68E;
            if (state != 1) {
                if (state < 2) {
                    if (state == 0) {
                        CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                        work->field_68E = 1;
                    }
                }
            } else if ((CdCmd_IsIdle() & 0xFFFF) == state) {
                coord = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
                SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                work->field_68E = 2;
            }
            break;
    }
    SCRATCH_SP += 0x54;
}

/// Step-4 body of the burn-out sequence, the fourth of the ones the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0xE, phase 1 waits out 5 blended frames and
/// asks for clip 0xF, and phase 2 waits out 0x26 of them and then either drops
/// the actor to the idle state or, when `field_6BA` asks for it, on to state 6
/// at sub-state `field_6BC`. Phase 2 latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F634`, the same 4-bit draw
/// `func_actor_521100_8013570C` makes.
void func_actor_521100_80134658(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0xE;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
            return;
        case 1:
            if ((s16)work->field_68A >= 5) {
                work->field_686 = 0xF;
                work->field_6A0 = 2;
            }
            return;
        case 2:
            if ((s16)work->field_68A >= 0x26) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// State-6 body of the burn-out sequence, the last one the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch again and `D_actor_521100_8015F654` holds one waypoint per
/// phase; every phase seeds the slot blend the same way (`field_686` 0x12,
/// forward speed `field_69A` 0x14, turn limit `field_69C` 0x78) and builds the
/// vector from the attach coordinate's translation to its target into the
/// 0x18-byte scratch, of which only the `vec` half is written.
///
/// Phase 0 aims at the player (`Player_Status.coordMtx->t`) and hands the actor
/// back to state 1, speed zeroed, once it is within 0x7D0 of it and the
/// player's own Z is past -0x5DC; otherwise it aims at waypoint 0 and steps the
/// phase to 1 on arrival within 0x3C. Those two paths leave the switch
/// directly, while the ones that reach neither clear `field_69E` and
/// `field_6A0` - or raise `field_6BA` only, in a session whose `field_4` is 2 -
/// and then clear `field_6BC`. Phase 1 aims at the player and falls back to
/// waypoint 1 past 0x7D0, re-aiming at the player from 0x3C of that waypoint;
/// the within-0x7D0 path and the re-aim one share the epilogue that stops the
/// actor (`field_698` re-aimed, `field_69A` 0, `field_69C` 0x78, `field_69E` 1,
/// `field_6A0` 0), while the far one goes to `game`, where the phase steps to 2
/// and both `field_6BA` / `field_6BC` are cleared, or both raised when
/// `field_4` is 2. Phase 2 aims at waypoint 2 and, from the coordinate's X past
/// -0xFA0, clears `field_69E` and `field_6A0` (the session check there only
/// clears `field_6A0`), then drops both flags.
///
/// `sc2` is a second view of the same scratch that only phase 0's else branch
/// reads: CSE folds its initialisation into a copy of `sc`, and the
/// `do { ... } while (0)` around phase 1's `ratan2` is what keeps `head` ahead
/// of that copy in the register allocator's order - see
/// `DECOMPILATION_LEARNINGS.md`, "loop_depth as an allocation weight".
void func_actor_521100_80134774(Task* arg0)
{
    Actor521100Work*       work;
    GsCOORDINATE2*         coord;
    Actor521100RotScratch* sc;
    Actor521100RotScratch* sc2;
    u8*                    head;
    s16                    state;

    head                  = *(u8**)G_SCRATCH_HEAD;
    sc                    = (Actor521100RotScratch*)(head - 0x18);
    sc2                   = (Actor521100RotScratch*)sc;
    *(u8**)G_SCRATCH_HEAD = (u8*)sc;
    work                  = arg0->work;
    coord                 = ((TmdObject*)arg0->extra)->coords;
    state                 = work->field_6A0;
    switch (state) {
        case 0:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->vec.vx      = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->vec.vy      = 0;
            sc->vec.vz      = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            if ((SquareRoot0((sc->vec.vx * sc->vec.vx) + (sc->vec.vz * sc->vec.vz)) < 0x7D0) && (Player_Status.coordMtx->t[2] < -0x5DC)) {
                work->field_698 = (u16)(ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF);
                work->field_69A = 0;
                work->field_69C = 0x78;
                work->field_69E = 1;
                work->field_6A0 = 0;
            } else {
                sc2->vec.vx     = D_actor_521100_8015F654[0].vx - coord->coord.t[0];
                sc2->vec.vy     = 0;
                sc2->vec.vz     = D_actor_521100_8015F654[0].vz - coord->coord.t[2];
                work->field_698 = (u16)(ratan2((s16)sc2->vec.vx, (s16)sc2->vec.vz) & 0xFFF);
                if (SquareRoot0((sc2->vec.vx * sc2->vec.vx) + (sc2->vec.vz * sc2->vec.vz)) < 0x3C) {
                    work->field_6A0 = 1;
                } else {
                    if (gGameSession->at4.loc.view != 2) {
                        work->field_69E = 0;
                        work->field_6A0 = 0;
                        work->field_6BA = 0;
                    } else {
                        work->field_6BA = 1;
                    }
                    work->field_6BC = 0;
                }
            }
            break;
        case 1:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->vec.vx      = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->vec.vy      = 0;
            sc->vec.vz      = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            if (SquareRoot0((sc->vec.vx * sc->vec.vx) + (sc->vec.vz * sc->vec.vz)) >= 0x7D0) {
                sc->vec.vx = D_actor_521100_8015F654[1].vx - coord->coord.t[0];
                sc->vec.vy = 0;
                sc->vec.vz = D_actor_521100_8015F654[1].vz - coord->coord.t[2];
                do {
                    work->field_698 = (u16)(ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF);
                    if (SquareRoot0((sc->vec.vx * sc->vec.vx) + (sc->vec.vz * sc->vec.vz)) >= 0x3C) {
                        goto game;
                    }
                } while (0);
                sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->vec.vy = 0;
                sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            }
            work->field_698 = (u16)(ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF);
            work->field_69A = 0;
            work->field_69C = 0x78;
            work->field_69E = 1;
            work->field_6A0 = 0;
            break;
        game:
            if (gGameSession->at4.loc.view != 2) {
                work->field_6A0 = 2;
                work->field_6BA = 0;
                work->field_6BC = 0;
            } else {
                work->field_6BA = 1;
                work->field_6BC = 1;
            }
            break;
        case 2:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->vec.vx      = D_actor_521100_8015F654[2].vx - coord->coord.t[0];
            sc->vec.vy      = 0;
            sc->vec.vz      = D_actor_521100_8015F654[2].vz - coord->coord.t[2];
            work->field_698 = (u16)(ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF);
            if (coord->coord.t[0] < -0xFA0) {
                if (SquareRoot0((sc->vec.vx * sc->vec.vx) + (sc->vec.vz * sc->vec.vz)) < 0x3C) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else if (gGameSession->at4.loc.view == state) {
                    work->field_6A0 = 0;
                }
            } else {
                if (gGameSession->at4.loc.view != state) {
                    work->field_69E = 0;
                }
                work->field_6A0 = 0;
            }
            work->field_6BA = 0;
            work->field_6BC = 0;
            break;
    }
    *(u32*)G_SCRATCH_HEAD += 0x18;
}
/// Steers the actor's heading towards the work block's `field_698` at up to
/// `field_69C` of turn per frame, then builds the result into the attach
/// coordinate as a pure-yaw rotation. The heading error is `field_698` minus
/// the coordinate's own Z-axis yaw (`ratan2` of `m[0][2]` over `m[2][2]`,
/// masked to the 12 bits the rotation is measured in), taken signed; the new
/// `field_696` is the target when the error is within the turn limit, and the
/// current yaw stepped by that limit otherwise. Errors past half a turn take
/// the short way round the wrap: the limit only has to beat `0x1000` minus the
/// error (or the error plus `0x1000`) to snap, so the turn never crosses into
/// the far half. `field_696` is read back as a signed half, the form the
/// sibling overlays' work blocks declare their yaw in; this body is the same
/// one `Actor02500_Fn016FC` and `func_actor_300700_80164794` carry.
void func_actor_521100_80134C38(Task* arg0)
{
    Actor521100Work*       work;
    GsCOORDINATE2*         coord;
    Actor521100RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor521100RotScratch*)(SCRATCH_SP -= 0x18);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_698;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_696 = ang;
    if (adiff < 0x800) {
        step = work->field_69C;
        if (step >= adiff) {
            work->field_696 = want;
        } else {
            next = (s16)work->field_696;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_696 = next;
        }
    } else {
        step = work->field_69C;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_696 = work->field_698;
        goto done;
    turn:
        wrapStep = work->field_69C;
        cur      = (s16)work->field_696;
        if (diff > 0) {
            work->field_696 = cur - wrapStep;
        } else {
            work->field_696 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_696;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}
/// Plays the actor's footstep cues: while the animation record the cue body
/// reads carries `flags` bit 0x20 (or 0x10), a sound is queued on the frame
/// that bit has just dropped from `Actor521100Work::field_6B4`, panned and
/// depth-attenuated from the actor's display coordinate. The record is the one
/// `Gp_AnimGetRec` returns for the slot at 0x3C - the second of the 0x28-byte
/// slots the actor work blocks lay out from 0x14, the same one the other actor
/// overlays' cue bodies play from. The cue id is the `GpEnemy` work id's bits
/// 12+ placed in bits 8-11 with the overlay's 0x401C tag, 1 for the 0x20 foot
/// and 2 for the 0x10 one, and the record's two bits are latched for the next
/// frame at the end.
void func_actor_521100_80134D88(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    GpAnimRec*       rec;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    rec   = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & 0x20) && (work->field_6B4 & 0x20)) {
            snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C0001;
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        }
        if (!(rec->flags & 0x10) && (work->field_6B4 & 0x10)) {
            snd  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x401C0002;
            pan2 = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
        }
        work->field_6B4 = (u16)(rec->flags & 0x30);
    }
}

/// Aims the actor's head coordinate (`field_8[4]`) at the player. Takes that
/// coordinate's `workm` into view space, offsets the player position by 0x600
/// in Y, rotates the delta into the body's frame, clamps it to +/-0x400 yaw,
/// +/-0x300 pitch and a minimum 0x200 forward, then builds the head rotation
/// from it. Same body as `func_actor_510900_80138BF0`.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void func_actor_521100_80134EDC(Task* arg0)
{
    Actor521100AimScratch* scratch;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         head;
    s32                    offsetY;

    coord                   = ((TmdObject*)arg0->extra)->coords;
    head                    = &coord[4];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor521100AimScratch);
    scratch                 = (Actor521100AimScratch*)*(void**)G_SCRATCH_HEAD;

    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &head->workm, &scratch->view);
    scratch->delta.vx = Player_Status.coordMtx->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = Player_Status.coordMtx->t[1] - offsetY;
    scratch->delta.vz = Player_Status.coordMtx->t[2] - scratch->view.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->delta, &scratch->local);

    if (scratch->local.vx < -0x400) {
        scratch->local.vx = -0x400;
    } else if (scratch->local.vx > 0x400) {
        scratch->local.vx = 0x400;
    }
    if (scratch->local.vy < -0x300) {
        scratch->local.vy = -0x300;
    } else if (scratch->local.vy > 0x300) {
        scratch->local.vy = 0x300;
    }
    if (scratch->local.vz < 0x200) {
        scratch->local.vz = 0x200;
    }
    Gp_OrientAlong(&scratch->local, &head->coord, 0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor521100AimScratch);
}
/// Untwists the coordinate at `field_8[3]`, which `func_actor_521100_801322F8`
/// left rotated by the random residual in `Actor521100Work::field_678` on the
/// frame the actor took a hit. The residual is turned into a matrix and
/// multiplied into that coordinate's own by `Gp_MulMatrix0`'s three `rtir`
/// passes - `rtir` multiplies the GTE rotation matrix by the vector in
/// `IR1..IR3`, so the body loads the coordinate's matrix, then each row of the
/// scratch matrix in turn, storing each result back over the coordinate. The
/// two angles are then stepped 0x20 towards zero; `field_680`, the flag the hit
/// body armed, survives while either is still moving and is cleared on the
/// frame both arrive, which is what the update body tests before calling this.
///
/// The scratch stack head is read through `Actor521100ScratchStack` rather than
/// as a `u32` - see that type for why the shape matters.
///
/// Same body as `Actor02000_Fn01698` and `func_actor_510900_80138D38`.
void func_actor_521100_80135024(Task* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                     = (MATRIX*)(((Actor521100ScratchStack*)0x1F8003FC)->sp - 0x20);
    ((Actor521100ScratchStack*)0x1F8003FC)->sp = (u32)matrix;
    active                                     = 0;
    work                                       = arg0->work;
    coord                                      = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->field_678, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    angleX = work->field_678.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_678.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_678.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_678.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_678.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_678.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_680 = 0;
    }
    SCRATCH_SP += 0x20;
}
/// The burn-out tick `func_actor_521100_80135414` runs while the sequence state
/// `field_68C` is non-zero. `field_68E` counts the frames since the last effect
/// and fires one once it reaches `D_actor_521100_8015F8CC[field_68C]` — every 7
/// frames while the body is alight in state 1, then 0xE and 0x1C as it burns
/// down. Every effect splashes part 3 of the model's coordinate array; in state
/// 1 a second one lands on a random other part, picked out of
/// `D_actor_521100_8015F8BC` by the top three bits of an LCG draw. `field_690`
/// is the sequence's own clock, walking the state 1 -> 2 at 0xF0 frames, 2 -> 3
/// at 0x14A and 3 -> 0 at 0x1A4, where the tick stops.
void func_actor_521100_80135230(Task* arg0)
{
    Actor521100Work* work;
    u16              timer;
    s16*             tbl;
    s16              part;

    work            = arg0->work;
    timer           = work->field_68E + 1;
    work->field_68E = timer;
    if ((s16)timer >= D_actor_521100_8015F8CC[work->field_68C]) {
        work->field_68E = 0U;
        func_800FDB18(3, &((TmdObject*)arg0->extra)->coords[3], NULL, &work->eff);
        if (work->field_68C == 1) {
            tbl         = D_actor_521100_8015F8BC;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            part        = tbl[(Gp_LcgState >> 16) & 7];
            func_800FDB18(3, &((TmdObject*)arg0->extra)->coords[part], NULL, &work->eff);
        }
    }
    timer           = work->field_690 + 1;
    work->field_690 = timer;
    if ((s16)timer == 0xF0) {
        work->field_68C = 2;
    }
    if ((s16)work->field_690 == 0x14A) {
        work->field_68C = 3;
    }
    if ((s16)work->field_690 >= 0x1A4) {
        work->field_68C = 0;
    }
}

/// State handlers of the actor's second part, which `func_actor_521100_80135AE4`
/// dispatches through: the setup `func_actor_521100_80135B40`, the per-frame
/// tick `func_actor_521100_80135B80` and `Gp_DestroyEnemy`.
const GpEnemyTaskFuncTable3 D_actor_521100_80131E40 = { {
    func_actor_521100_80135B40,
    func_actor_521100_80135B80,
    Gp_DestroyEnemy,
} };

/// The actor's task body: runs the handler for `Task::state` out of a two-entry
/// table built on the stack - the spawn state `func_actor_521100_80131E8C`,
/// then the per-frame state `func_actor_521100_801353CC` - passing the task's
/// `GpEnemy` along with the task.
void func_actor_521100_80135378(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_521100_80131E8C,
        func_actor_521100_801353CC,
    };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_521100_801353CC(GpEnemy* arg0, Task* arg1)
{
    Actor521100Work* work;

    work = arg1->work;
    if (gGameSession->eventState != 0) {
        work->field_682 = 1;
        func_actor_521100_80135414(arg0, arg1);
        return;
    }
    work->field_682 = 0;
    func_actor_521100_80135478(arg0, arg1);
}

void func_actor_521100_80135414(GpEnemy* arg0, Task* arg1)
{
    Actor521100Work* temp_s0;

    temp_s0          = arg1->work;
    arg0->node.flags = 1;
    func_actor_521100_80135964(arg1);
    func_actor_521100_80135A34(arg1);
    func_actor_521100_80135A90(arg1);
    if (temp_s0->field_68C != 0) {
        func_actor_521100_80135230(arg1);
    }
}

void func_actor_521100_80135478(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   temp_s2;
    TmdObject*       temp_a1;
    Actor521100Work* temp_s1;
    s32              state;
    s32              one;

    temp_a1 = arg1->extra;
    state   = Gp_StateF0.field_4;
    temp_s1 = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->flags                                 = 0;
    ((TmdObject*)temp_s1->field_654->extra)->flags = 0;
    arg0->node.flags                               = 8;
    goto default_body;
case2:
    temp_a1->flags                                 = 0x80;
    ((TmdObject*)temp_s1->field_654->extra)->flags = 0x80;
    arg0->node.flags                               = one;
    return;
default_body:
    if (temp_s1->field_6B0 == 0) {
        arg0->node.flags = 1;
        return;
    }
    func_actor_521100_801322F8(arg1, temp_a1, one);
    func_actor_521100_801355C8(arg1);
    func_actor_521100_80134C38(arg1);
    func_actor_521100_801358D4(arg1);
    func_actor_521100_80134D88(arg1);
    func_actor_521100_80135964(arg1);
    func_actor_521100_80134EDC(arg1);
    if (temp_s1->field_680 != 0) {
        func_actor_521100_80135024(arg1);
    }
    temp_s2->flg                             = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(temp_s2);
case1:
    func_actor_521100_80135A34(arg1);
    func_actor_521100_80135A90(arg1);
}

void func_actor_521100_801355C8(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor521100Work*)arg0->work)->field_69E;
    switch (temp_v1) {
        case 0:
            func_actor_521100_80132958(arg0);
            return;
        case 1:
            func_actor_521100_80132DE8(arg0);
            return;
        case 2:
            func_actor_521100_801339B0(arg0);
            return;
        case 3:
            func_actor_521100_8013570C(arg0);
            return;
        case 4:
            func_actor_521100_80134658(arg0);
            return;
        case 5:
            func_actor_521100_801357F0(arg0);
            return;
        case 6:
            func_actor_521100_80134774(arg0);
        default:
            return;
    }
}

/// Steps the actor into state 1 once its facing has come within 45 degrees of
/// the angle at `field_696`, then stops it: both speeds are zeroed.
void func_actor_521100_80135680(Task* arg0)
{
    Actor521100Work* work;
    s16              delta;
    s16              angle;
    s16              wrapped;
    s32              magnitude;

    work      = arg0->work;
    delta     = work->field_698 - work->field_696;
    magnitude = abs(delta);
    if (magnitude < 0x800) {
        angle = magnitude;
    } else {
        if (delta > 0) {
            wrapped = 0x1000 - delta;
        } else {
            wrapped = delta + 0x1000;
        }
        angle = wrapped;
    }
    if ((angle < 0x200) && (work->field_6AA < 0xDAC)) {
        work->field_69E = 1;
        work->field_6A0 = 0;
        work->field_69A = 0;
        work->field_69C = 0;
    }
}

/// Step-3 body of the burn-out sequence, the third of the three the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is its own
/// two-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0x10, phase 1 waits out 0x37 blended frames and
/// then either drops the actor to the idle state or, when `field_6BA` asks for
/// it, on to state 6 at sub-state `field_6BC`. Either way it latches clip 1 for
/// the blend and picks this frame's effect out of `D_actor_521100_8015F634`.
void func_actor_521100_8013570C(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x10;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
            return;
        case 1:
            if ((s16)work->field_68A >= 0x37) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Step-5 body of the burn-out sequence, the same two-phase `field_6A0` latch
/// `func_actor_521100_8013570C` runs with the longer timing: phase 0 hands the
/// record flags at 0x59A / 0x5BA back and asks the slot blend for clip 0x11,
/// phase 1 waits out 0x48 blended frames and then either drops the actor to the
/// idle state or, when `field_6BA` asks for it, on to state 6 at sub-state
/// `field_6BC`. Either way it latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F5F4`.
void func_actor_521100_801357F0(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x11;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
            return;
        case 1:
            if ((s16)work->field_68A >= 0x48) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F5F4;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Snapshots the attach coordinate's translation into the work block, then
/// walks the coordinate forward: 0x80 up, and along its own facing axis
/// (`m[0][2]` / `m[2][2]`) scaled by the work block's speed in 12-bit fixed
/// point.
void func_actor_521100_801358D4(Task* arg0)
{
    GsCOORDINATE2*   coord;
    Actor521100Work* work;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;

    work->field_64C    = coord->coord.t[0];
    work->field_64E    = coord->coord.t[1];
    work->field_650    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_69A) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_69A) >> 12;
}

/// Blends every animation slot towards the clip latched in `field_686` while
/// it differs from the clip the slots carry, then ticks them once they agree:
/// the blend runs the nineteen slots through `func_800B4114` with the length
/// `D_actor_521100_8015F894` gives the incoming clip, and the tick counts the
/// agreeing frames in `field_68A`.
void func_actor_521100_80135964(Task* arg0)
{
    Actor521100Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    val  = 0;
    if (work->field_686 != work->field_688) {
        work->field_688 = work->field_686;
        work->field_68A = 0;
        if (work->field_686 < 0x15) {
            val = D_actor_521100_8015F894[work->field_686];
        }
        i = 1;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_686, 0, val);
            i++;
        } while (i < 0x13);
        return;
    }
    i                = 1;
    work->field_68A += i;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 0x13);
}

/// Colours the actor from the world position of its second model coordinate,
/// handing it to `Gp_UpdateActorColor` with no blend parameters.
void func_actor_521100_80135A34(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = &((TmdObject*)arg0->extra)->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// Draws the actor's ground shadow: a quad at the second model coordinate's
/// x/z and the first's y, so it lies on the ground under the actor.
void func_actor_521100_80135A90(Task* arg0)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* sub;
    VECTOR3        vec;

    coord  = &((TmdObject*)arg0->extra)->coords[0];
    sub    = &((TmdObject*)arg0->extra)->coords[1];
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}

/// Task body of the actor's second part: copies `D_actor_521100_80131E40`
/// onto the stack and runs the handler for `Task::state` on the task's
/// `GpEnemy`.
void func_actor_521100_80135AE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_521100_80131E40;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// Setup state of the actor's second part: hangs its model coordinate under
/// the parent model's ninth coordinate, draws it under the parent work block's
/// light and colour matrices, shows it and moves the task on to its tick.
void func_actor_521100_80135B40(GpEnemy* enemy, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor521100Work*)parent->work;

    coord->sub    = &parentCoords[8];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

void func_actor_521100_80135B80(GpEnemy* arg0, Task* task)
{
    TmdObject*       obj;
    Actor521100Work* work;
    s16              mode;

    work = (Actor521100Work*)task->parent->work;
    obj  = task->extra;
    if (work->field_682 != 0) {
        mode       = ((work->field_692 & 1) == 0) << 7;
        obj->flags = mode;
        if (work->field_692 & 2) {
            obj->flags = mode | 4;
        }
        if (work->field_694 != 0) {
            obj->flags = 0x80;
        }
    }
}

s32 func_actor_521100_80135BEC(Task* arg0)
{
    if (D_80073BA0 > 0) {
        ((Actor521100Work*)arg0->work)->field_6A8 = 1;
    }
    return 0;
}

s32 func_actor_521100_80135C14(Task* arg0, s32 arg1, Actor521100AnimPreset* args)
{
    Actor521100Work* work;
    s32              i;
    s32              frames;
    s16              clip;
    s16              base;

    frames = 0;
    work   = arg0->work;
    base   = 0x1D;
    if (args->field_0 == 0) {
        base = 0x14;
    }
    clip            = args->field_4 + base;
    work->field_686 = clip;
    work->field_688 = clip;
    if (args->field_8 != 0) {
        frames = args->field_C;
    }
    for (i = 1; i < 0x13; i++) {
        func_800B4114((GpAnimCtx*)work, i, work->field_686, 0, frames);
    }
    return 0;
}

/// Message 0x7D4 handler in `D_actor_521100_8015F6FC`, placing the actor: builds the root coordinate's
/// matrix from the argument block's angles, stores its translation and clears
/// `flg` so the world matrix is recomputed.
s32 func_actor_521100_80135CAC(Task* task, s32 arg1, ActorsShared80132074Args* args)
{
    TmdObject*     ext   = task->extra;
    GsCOORDINATE2* coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}
