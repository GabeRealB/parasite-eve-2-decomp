#include "actor_207200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// 0x2B0-byte work block the enemy's spawn function allocates with `memCalloc`
/// and parks in the task's `Task::work` slot (that slot is not a `TaskIdMap`
/// here). It embeds three `WorldCollisionBody` list nodes; the first points its `context.contacts` at
/// the `WorldCollisionCapsule` that follows it, the other two point straight at their
/// own `WorldCollisionContact` table, and `Gp_InitRec18Table` zeroes each table.
/// `ActorsShared8014df20` hands all three nodes back to `Gp_UnlinkObj`.
typedef struct ActorShared8014df20Work {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[3];
    /* 0x08C */ byte                  field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX                field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX                field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ WorldCollisionBody    field_FC;
    /* 0x11C */ WorldCollisionCapsule field_11C;
    /* 0x134 */ WorldCollisionContact field_134[1];
    /* 0x14C */ WorldCollisionBody    field_14C;
    /* 0x16C */ WorldCollisionContact field_16C[1];
    /* 0x184 */ WorldCollisionBody    field_184;
    /* 0x1A4 */ WorldCollisionContact field_1A4[4];
    /* 0x204 */ byte                  pad_204[0x50];
    /* 0x254 */ s32                   field_254; // position restored when the push-back conflicts
    /* 0x258 */ s32                   field_258;
    /* 0x25C */ s32                   field_25C;
    /* 0x260 */ byte                  pad_260[0x2C];
    /* 0x28C */ s16                   field_28C;
    /* 0x28E */ s16                   field_28E;
    /* 0x290 */ s16                   field_290;
    /* 0x292 */ s16                   field_292;
    /* 0x294 */ byte                  pad_294[6];
    /* 0x29A */ s16                   field_29A;
    /* 0x29C */ byte                  pad_29C[4];
    /* 0x2A0 */ s16                   field_2A0;
    /* 0x2A2 */ byte                  pad_2A2[2];
    /* 0x2A4 */ s16                   field_2A4;
    /* 0x2A6 */ s16                   field_2A6;
    /* 0x2A8 */ s16                   field_2A8;
    /* 0x2AA */ s16                   field_2AA;
    /* 0x2AC */ s16                   field_2AC;
    /* 0x2AE */ byte                  pad_2AE[2];
} ActorShared8014df20Work;
STATIC_ASSERT_SIZEOF(ActorShared8014df20Work, 0x2B0);

extern EnemyParams D_actor_207200_8014DBBC;
extern u8          D_actor_207200_8014E7B0[];
extern SVECTOR     D_actor_207200_8014E7BC;
extern SVECTOR     D_actor_207200_8014E7C4;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_207200_8014ACF8(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014A1C4(Task* arg0);
static void func_actor_207200_8014AE08(Task* arg0);
static void func_actor_207200_8014AE70(Task* task);
static void func_actor_207200_8014AF2C(Task* arg0);
static void func_actor_207200_8014B04C(Task* task);
static void func_actor_207200_8014B128(Task* arg0);
static void func_actor_207200_8014B21C(Task* task);
static void func_actor_207200_8014AFDC(Enemy* arg0, Task* task);

extern TmdSource D_actor_207200_8014E4C8;
void             func_actor_207200_8014AC9C(Task*);

DamageAttack D_actor_207200_8014DBB8[1] = { 0 };

EnemyParams D_actor_207200_8014DBBC = { D_actor_207200_8014DBB8, 1, 2, 32, 1, 100, 20, 100, 99 };

TmdBone D_actor_207200_8014DBCC[3] = {
#include "assets/actor_207200_model_046A8_skeleton.inc"
};

u32 D_actor_207200_8014DC38[3] = {
#include "assets/actor_207200_model_046A8_partVerts.inc"
};

SVECTOR D_actor_207200_8014DC44[37] = {
#include "assets/actor_207200_model_046A8_verts.inc"
};

SVECTOR D_actor_207200_8014DD6C[47] = {
#include "assets/actor_207200_model_046A8_normals.inc"
};

u32 D_actor_207200_8014DEE4[377] = {
#include "assets/actor_207200_model_046A8_stream.inc"
};

TmdSource D_actor_207200_8014E4C8 = {
    0,
    2184,
    332,
    3,
    D_actor_207200_8014DC38,
    D_actor_207200_8014DC44,
    D_actor_207200_8014DD6C,
    D_actor_207200_8014DBCC,
    D_actor_207200_8014DEE4,
};

AnimationPackedPose D_actor_207200_8014E4EC[2] = {
#include "assets/actor_207200_animation_04708_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E504[1] = {
#include "assets/actor_207200_animation_04708_bank4.inc"
};

AnimationRecord D_actor_207200_8014E508[6] = {
#include "assets/actor_207200_animation_04708_records.inc"
};

u16 D_actor_207200_8014E520[4] = {
#include "assets/actor_207200_animation_04708_indices.inc"
};

AnimationSet D_actor_207200_8014E528 = {
    D_actor_207200_8014E508,
    D_actor_207200_8014E520,
    { NULL, D_actor_207200_8014E4EC, NULL, NULL, D_actor_207200_8014E504, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_8014E550[24] = {
#include "assets/actor_207200_animation_0495C_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E670[10] = {
#include "assets/actor_207200_animation_0495C_bank4.inc"
};

AnimationRecord D_actor_207200_8014E698[55] = {
#include "assets/actor_207200_animation_0495C_records.inc"
};

u16 D_actor_207200_8014E774[4] = {
#include "assets/actor_207200_animation_0495C_indices.inc"
};

AnimationSet D_actor_207200_8014E77C = {
    D_actor_207200_8014E698,
    D_actor_207200_8014E774,
    { NULL, D_actor_207200_8014E550, NULL, NULL, D_actor_207200_8014E670, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_8014E7A4 = { TASK_BODY_TMD, 96, func_actor_207200_8014AC9C, { .model = &D_actor_207200_8014E4C8 } };

u8 D_actor_207200_8014E7B0[12] = {
    0,
    0,
    0,
    0,
    40,
    229,
    20,
    128,
    124,
    231,
    20,
    128,
};

SVECTOR D_actor_207200_8014E7BC = { 0, -100, 0, 0 };

SVECTOR D_actor_207200_8014E7C4 = { 0, 0, 100, 0 };

DamageAttack D_actor_207200_8014E7CC[2] = { { 25, 11 }, { 10, 0 } };

static void            func_actor_207200_80149E84(Enemy* arg0, Task* arg1);
static void            func_actor_207200_8014A588(Task* arg0);
static __inline__ void _actor207200TickAnim(Task* task);
static void            func_actor_207200_8014AA74(Enemy* arg0, Task* arg1);

static void func_actor_207200_80149E84(Enemy* arg0, Task* arg1)
{
    ActorShared8014df20Work* work;
    TmdObject*               obj;
    GfxCoord*                coord;
    GfxCoord*                part;
    u32                      seed;
    WorldCollisionContact*   records1;
    WorldCollisionContact*   records2;
    WorldCollisionContact*   records3;
    s32                      i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_DC;
    obj->colorMtx       = &work->field_BC;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &D_actor_207200_8014DBBC;
    arg0->recs                   = work->field_1A4;
    arg0->hp                     = D_actor_207200_8014DBBC.hpMax;
    func_800B3F84(&work->context, D_actor_207200_8014E7B0, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    (Gp_IncStateF0Ref)(0);
    work->field_28C              = 1;
    work->field_28E              = 1;
    work->field_2A6              = 1;
    work->field_2A4              = 0x12;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    seed                         = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8              = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState                  = seed;
    Gp_SetLightMode(arg1->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->field_11C.ends[0].vz     = 0x1388;
    work->field_11C.end0Radius     = 0xFA0;
    work->field_11C.end1Radius     = 0x7D0;
    records1                       = work->field_134;
    work->field_11C.contacts       = records1;
    work->field_FC.context.capsule = &work->field_11C;
    work->field_FC.coord           = coord;
    work->field_FC.pos.vx          = 0;
    work->field_FC.pos.vy          = 0;
    work->field_FC.pos.vz          = 0;
    work->field_FC.key             = 0;
    work->field_FC.radius          = 0;
    work->field_FC.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord            = coord;
    records2                         = work->field_16C;
    work->field_14C.context.contacts = records2;
    work->field_14C.pos.vx           = 0;
    work->field_14C.pos.vy           = 0;
    work->field_14C.pos.vz           = 0;
    work->field_14C.key              = 0;
    work->field_14C.radius           = 0x7D0;
    work->field_14C.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_FC.flags             = work->field_FC.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                         = work->field_1A4;
    work->field_184.coord            = coord;
    work->field_184.context.contacts = records3;
    work->field_184.pos.vx           = 0;
    work->field_184.pos.vy           = -0xC8;
    work->field_184.pos.vz           = 0;
    work->field_184.key              = 0x3002F;
    work->field_184.radius           = 0xC8;
    work->field_184.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_14C.flags            = work->field_14C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->bodyKind == work->field_2AC) {
        obj->texturePageOffset++;
        obj->clutRowOffset++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = func_actor_207200_8014B21C;
    arg1->state++;
}

/// State-0 tick of the small enemy. A 0x10000-class hit on either of its two
/// single-record tables sets `Gp_StateF0.prefix.bytes.field_3`, latches `field_2AA` and selects
/// animation 2; if the light blend is fully up, one sound plays, the blend is
/// turned to fall and a new 0x12..0x31 frame wait is rolled. A latched hit
/// plays a second sound, clears the 0x8000 bit of both nodes and arms state
/// 0xF0. Under animation 1 the frame counter reaching `field_2A8` turns the
/// blend down (with the first sound) when it is fully up, or back up after a
/// new 0x64..0xA3 frame wait when it has bottomed out; under animation 2 the
/// second sound repeats every 0x28 frames. `field_2AC` picks between two sets
/// of sound ids.
static void func_actor_207200_8014A1C4(Task* arg0)
{
    ActorShared8014df20Work* work;
    GfxCoord*                obj;
    s32                      snd;
    s16                      mode;
    s32                      id;
    Enemy*                   ctx;

    work = (ActorShared8014df20Work*)arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(8);
    obj = arg0->extra.tmd->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        Gp_StateF0.prefix.bytes.field_3 = 1;
        work->field_2AA                 = 1;
        work->field_28C                 = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_FC.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
static void func_actor_207200_8014A588(Task* arg0)
{
    ActorShared8014df20Work* work;
    ActorDeltaFrame38*       sc;
    ActorDeltaFrame38*       head;
    TmdObject*               obj;
    GfxCoord*                coord;
    Enemy*                   enemy;
    s32                      i;
    s32                      sndHit;
    s32                      sndHit2;
    u32                      damage;
    s32                      snd;

    work                                    = (ActorShared8014df20Work*)arg0->work;
    head                                    = SCRATCH_STACK_CURSOR(ActorDeltaFrame38);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = head - 1;
    sc                                      = head - 1;
    obj                                     = arg0->extra.tmd;
    coord                                   = obj->coords;
    enemy                                   = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &D_actor_207200_8014E7C4);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].key.value,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->field_1A4[i].key.value, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78(enemy, work->field_1A4[i].key.value, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &D_actor_207200_8014E7C4);
                    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key.value)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2(enemy, work->field_1A4[i].key.value, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = SCRATCH_STACK_CURSOR(ActorDeltaFrame38) + 1;
}

/// Rebinds the small enemy's animation id `field_28C` to its two helper
/// slots: a changed id is remembered in `field_28E`, its frame count restarts
/// and both slots switch to it with a blend of 8; otherwise the count ticks and
/// the slots advance.
static __inline__ void _actor207200TickAnim(Task* task)
{
    Actor207200Work* work = task->work;
    s32              i;

    if (work->field_28C != work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        for (i = 1; i < 3; i++) {
            func_800B4114((AnimationContext*)work, i, work->field_28C, 0, 8);
        }
    } else {
        work->field_290++;
        for (i = 1; i < 3; i++) {
            Gp_AnimTickIndex((AnimationContext*)work, i);
        }
    }
}

/// Dying-state tick of the small enemy, under the shared `Gp_StateF0.field_4` mode
/// byte: 1 does nothing and 2 hides the model. Otherwise the root part's
/// matrix is saved into `field_264` and refolded with the decaying Y scale.
/// Once `field_288` is set the enemy is destroyed after 0x3D frames; before
/// that, the kill countdown running out releases the state-0xF0 reference,
/// sets `field_288` and unlinks the enemy's node and its three objects, and
/// the two animation slots are rebound or advanced.
static void func_actor_207200_8014AA74(Enemy* arg0, Task* arg1)
{
    Actor207200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        func_actor_207200_8014B128(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    func_actor_207200_8014B128(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add(arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C.obj);
        Gp_UnlinkObj(&work->field_FC.obj);
        Gp_UnlinkObj(&work->field_184.obj);
    }
    _actor207200TickAnim(arg1);
}

/// The small enemy's state handlers - spawn, live tick and dying tick - which
/// `func_actor_207200_8014AC9C` dispatches through by task state.
static const GpEnemyTaskFuncTable3 D_actor_207200_80149E24 = {
    { func_actor_207200_80149E84, func_actor_207200_8014ACF8, func_actor_207200_8014AA74 }
};

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Global mode byte in the main executable shared by the enemy actors: 1 runs
/// only the tail below, 2 puts the model in its hidden pose, 0 clears the
/// node flag before falling into the update, and any other value updates
/// directly.
///
/// The update raises the root coordinate's Y translation by 0x80 and ticks the
/// five model helpers, clears the display flags of the first two parts and
/// recomputes the second part's world matrix; the tail then colours the actor
/// from that part.
static void func_actor_207200_8014ACF8(Enemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
    one   = 1;
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
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    arg1->extra.tmd->coords[0].coord.t[1] += 0x80;
    func_actor_207200_8014AE70(arg1);
    func_actor_207200_8014B04C(arg1);
    func_actor_207200_8014AE08(arg1);
    func_actor_207200_8014A588(arg1);
    func_actor_207200_8014AF2C(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
case1:
    func_actor_207200_8014AFDC(arg0, arg1);
}

/// Consumes the pending bits of the enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame counter cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
static void func_actor_207200_8014AE08(Task* arg0)
{
    Enemy*           enemy;
    Actor207200Work* work;
    u8               flags;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            enemy->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Per-frame dispatch on the small enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1
/// and the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
static void func_actor_207200_8014AE70(Task* task)
{
    Actor207200Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            func_actor_207200_8014A1C4(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2(task->spawnArg2.pointer) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

/// Drives animation slots 1 and 2 from the work's animation id `field_28C`.
/// When it differs from the remembered `field_28E` it is remembered, the
/// frame counter restarts and both slots switch to it with a blend of 8;
/// otherwise the counter ticks and both slots advance.
static void func_actor_207200_8014AF2C(Task* arg0)
{
    _actor207200TickAnim(arg0);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
static void func_actor_207200_8014AFDC(Enemy* arg0, Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Ramps the small enemy's light blend up or down depending on the flag at
/// `field_2A6`. Rising, the first frame switches the display object to light
/// mode 2 and the counter saturates at 0x12, where it sets the enemy node's
/// flag and the model's 0x80 bit. Falling, leaving 0x12 clears the node flag
/// and returns the object to light mode 0, and the counter bottoms out at 0
/// with the model bits cleared.
static void func_actor_207200_8014B04C(Task* task)
{
    ActorShared8014df20Work* work;
    Enemy*                   enemy;
    TmdObject*               obj;

    work  = (ActorShared8014df20Work*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    obj   = task->extra.tmd;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4               = 0x12;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.state.parts.flags = 0;
            obj->flags                    = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}

/// Rebuilds the model's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `composeStamp` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
static void func_actor_207200_8014B128(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor207200Work*   work;

    head                       = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                       = arg0->work;
    scratch                    = head - 1;
    SCRATCH_STACK_CURSOR(void) = scratch;
    coord                      = arg0->extra.tmd->coords;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Exit callback of the small enemy: detaches the enemy's hit records,
/// unlinks its node and the work's three objects, then runs the common enemy
/// exit.
static void func_actor_207200_8014B21C(Task* task)
{
    ActorShared8014df20Work* work;
    Enemy*                   enemy;

    enemy = task->spawnArg2.pointer;
    work  = (ActorShared8014df20Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
