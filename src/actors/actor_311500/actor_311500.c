#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/acropolis_fire_escape.h"
#include "../../shared/actor_contacts.h"

/// Psy-Q `RotMatrixY`.

typedef struct Actor311500Work {
    /// Animation context the block itself begins with: `func_actor_311500_80162F28`
    /// hands the block straight to `func_800B4114` / `Gp_AnimTickIndex`.
    /* 0x000 */ ActorAnimRig19 rig;
    /// List node `func_actor_311500_801630A4` unlinks on the first step of
    /// state 1. Sits directly in front of the collision table.
    /* 0x43C */ WorldCollisionBody field_43C;
    /// One-entry contact table the enemy record points at; the damage check
    /// looks here for a hit.
    /* 0x45C */ WorldCollisionContact rec18[1];
    /* 0x474 */ MATRIX                light;
    /* 0x494 */ MATRIX                color;
    /* 0x4B4 */ Task*                 field_4B4;
    /* 0x4B8 */ MATRIX*               field_4B8;
    /// The model's `flags` as they were when mode 2 began hiding it, put back
    /// by mode 0 while the mode it last recorded is nonzero.
    /* 0x4BC */ u32 field_4BC;
    /// Sub-state of the phase handler running this frame (idle, hit reaction
    /// or death); each phase change in `func_actor_311500_80163334` resets it
    /// to 0.
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ byte pad_4C2[0x2];
    /// Frame counter within a sub-state: the length of the idle pause, which
    /// ends after 0x1F frames, and the clock of the death sequence.
    /* 0x4C4 */ s16  field_4C4;
    /* 0x4C6 */ byte pad_4C6[0x2];
    /// Idle animation plays since the last pause. Each play that reaches its
    /// clip end raises it; from 2 on the idle always pauses instead of
    /// replaying, and the pause clears it.
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ byte pad_4CA[0x2];
    /* 0x4CC */ s32  field_4CC;
    /* 0x4D0 */ s32  field_4D0;
    /* 0x4D4 */ u16  field_4D4;
    /// `Gp_StateF0.field_4` as the previous frame saw it, so a mode change can be
    /// detected.
    /* 0x4D6 */ u16 field_4D6;
} Actor311500Work;
STATIC_ASSERT_SIZEOF(Actor311500Work, 0x4D8);

extern EnemyParams   D_actor_311500_801692C0;
extern AnimationSet* D_actor_311500_801692F4[2];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, s32, u32*);
    } handler;
} Actor311500MessageEntry;
STATIC_ASSERT_SIZEOF(Actor311500MessageEntry, 8);

extern Actor311500MessageEntry D_actor_311500_80169330[1];

extern s32 D_actor_311500_801692FC[2];
extern s32 D_actor_311500_80169304[8];
extern s32 D_actor_311500_80169324[3];

extern AnimationSet D_actor_311500_80168FA8;
extern AnimationSet D_actor_311500_80169290;
extern TmdSource    D_actor_311500_80168BF8;
void                func_actor_311500_80163334(Task*);
void                func_actor_311500_801636A0(Task*, s32, s32, u32*);

TmdBone D_actor_311500_801636B4[19] = {
#include "assets/actor_311500_model_06DD8_skeleton.inc"
};

u32 D_actor_311500_80163960[19] = {
#include "assets/actor_311500_model_06DD8_partVerts.inc"
};

SVECTOR D_actor_311500_801639AC[311] = {
#include "assets/actor_311500_model_06DD8_verts.inc"
};

SVECTOR D_actor_311500_80164364[309] = {
#include "assets/actor_311500_model_06DD8_normals.inc"
};

u32 D_actor_311500_80164D0C[4027] = {
#include "assets/actor_311500_model_06DD8_stream.inc"
};

TmdSource D_actor_311500_80168BF8 = {
    0,
    20180,
    7860,
    19,
    D_actor_311500_80163960,
    D_actor_311500_801639AC,
    D_actor_311500_80164364,
    D_actor_311500_801636B4,
    D_actor_311500_80164D0C,
};

AnimationPackedPose D_actor_311500_80168C1C[6] = {
#include "assets/actor_311500_animation_07188_bank1.inc"
};

AnimationPackedRotation D_actor_311500_80168C64[81] = {
#include "assets/actor_311500_animation_07188_bank4.inc"
};

AnimationRecord D_actor_311500_80168DA8[118] = {
#include "assets/actor_311500_animation_07188_records.inc"
};

u16 D_actor_311500_80168F80[20] = {
#include "assets/actor_311500_animation_07188_indices.inc"
};

AnimationSet D_actor_311500_80168FA8 = {
    D_actor_311500_80168DA8,
    D_actor_311500_80168F80,
    { NULL, D_actor_311500_80168C1C, NULL, NULL, D_actor_311500_80168C64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_311500_80168FD0[5] = {
#include "assets/actor_311500_animation_07470_bank1.inc"
};

AnimationPackedRotation D_actor_311500_8016900C[52] = {
#include "assets/actor_311500_animation_07470_bank4.inc"
};

AnimationRecord D_actor_311500_801690DC[99] = {
#include "assets/actor_311500_animation_07470_records.inc"
};

u16 D_actor_311500_80169268[20] = {
#include "assets/actor_311500_animation_07470_indices.inc"
};

AnimationSet D_actor_311500_80169290 = {
    D_actor_311500_801690DC,
    D_actor_311500_80169268,
    { NULL, D_actor_311500_80168FD0, NULL, NULL, D_actor_311500_8016900C, NULL, NULL, NULL },
};

DamageAttack D_actor_311500_801692B8[2] = {
    { 18, 7 },
    { 18, 0 },
};

EnemyParams D_actor_311500_801692C0 = { D_actor_311500_801692B8, 30, 42, 82, 4, 250, 0, 100, 0 };

u16 D_actor_311500_801692D0[18] = {
    20,
    900,
    12,
    2000,
    0,
    0,
    10,
    800,
    12,
    2500,
    0,
    0,
    0,
    500,
    12,
    3000,
    0,
    0,
};

AnimationSet* D_actor_311500_801692F4[2] = {
    &D_actor_311500_80169290,
    &D_actor_311500_80168FA8,
};

s32 D_actor_311500_801692FC[2] = {
    0,
    4096,
};

s32 D_actor_311500_80169304[8] = {
    -0x12BF448,
    0xFC18,
    -0x12BFC18,
    0xFC18,
    3000,
    0xFC18,
    1000,
    0xFC18,
};

s32 D_actor_311500_80169324[3] = {
    0x10000,
    0x30002,
    0x60000,
};

Actor311500MessageEntry D_actor_311500_80169330[1] = {
    { 2006, { .call0 = func_actor_311500_801636A0 } },
};

TaskDesc D_actor_311500_80169338 = { (TASK_BODY_TMD | 0x100), 192, func_actor_311500_80163334, { .model = &D_actor_311500_80168BF8 } }; /// Walks the first `count` contact records (stopping at a zero key) and keeps,

static void        func_actor_311500_801629D8(Task* arg0);
static inline void _actor311500ResetAnim(Task* task, u8 rate);
static inline u16  _actor311500TickAnim(Task* task);
static void        func_actor_311500_80162C34(Task* arg0, TmdObject* arg1);
static s16         func_actor_311500_80162DDC(Task* arg0);
static inline void _actor311500BlendAnim(Task* task);
static inline void _actor311500SpawnEffect(Task* task);
static s32         func_actor_311500_80162F28(Task* arg0);
static s32         func_actor_311500_801630A4(Task* arg0);

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_turn_joint.inc.c"

static void func_actor_311500_801629D8(Task* arg0)
{
    Actor311500Work* work;
    Actor311500Work* work2;
    Actor311500Work* work3;
    Enemy*           enemy;
    GfxCoord*        coords;
    TmdObject*       tmd;
    AreaPlacement*   place;
    s32              i;
    u8               rate;

    coords     = arg0->extra.tmd->coords;
    enemy      = arg0->spawnArg2.pointer;
    tmd        = arg0->extra.tmd;
    work       = (Actor311500Work*)memCalloc(0x4D8, 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    work2 = (Actor311500Work*)arg0->work;
    Mem_Set(work2, 0, 0x4D8);
    coords->parent = &gGfxViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx = &work2->light;
    tmd->colorMtx = &work2->color;
    tmd->flags    = 0;
    func_800B3F84(&work2->rig.anim, D_actor_311500_801692F4, tmd, work2->rig.poses,
                  &work2->rig.slots[0]);
    work2->field_4B4 = gameGetPtrSlot(3);
    work2->field_4B8 = Player_Status.coordMtx;
    rate             = 0x10;
    i                = 1;
    work3            = (Actor311500Work*)arg0->work;
    do {
        work3->rig.slots[i & 0xFFFF].rate = rate;
        Gp_AnimResetSlot(&work3->rig.anim, i & 0xFFFF, 0);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->hp                         = 0x32;
    enemy->node.state.parts.flags     = 0;
    enemy->reactionFlags              = 0;
    enemy->param                      = &D_actor_311500_801692C0;
    work2->field_43C.coord            = &arg0->extra.tmd->coords[2];
    work2->field_43C.context.contacts = &work2->rec18[0];
    work2->field_43C.pos.vx           = 0;
    work2->field_43C.pos.vy           = 0;
    work2->field_43C.pos.vz           = 0;
    work2->field_43C.key              = 0x3000A;
    work2->field_43C.radius           = 0x190;
    work2->field_43C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work2->field_43C);
    work2->field_43C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(&work2->rec18[0], 1, 0);
    enemy->recs      = &work2->rec18[0];
    arg0->msgTable   = D_actor_311500_80169330;
    work2->field_4D4 = 1;
    place            = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != 0xA) {
        place++;
    }
    Gp_SetTmdBytes(tmd, place->texturePageOffset, place->clutRowOffset);
}

/// Sets animation slots 1 to 18 to play at `rate` and restarts each of them.
static inline void _actor311500ResetAnim(Task* task, u8 rate)
{
    Actor311500Work* work = task->work;
    s32              i;

    i = 1;
    do {
        work->rig.slots[i & 0xFFFF].rate = rate;
        Gp_AnimResetSlot(&work->rig.anim, i & 0xFFFF, 0);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
}

/// Advances animation slots 1 to 18 by one frame and returns 1 when slot 1
/// has bit 0 of its flags set, 0 otherwise.
static inline u16 _actor311500TickAnim(Task* task)
{
    Actor311500Work* work = task->work;
    s32              i;

    i = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i & 0xFFFF);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    if (work->rig.slots[1].flags & ANIMATION_SLOT_REACHED_END) {
        return 1;
    }
    return 0;
}

static void func_actor_311500_80162C34(Task* arg0, TmdObject* arg1)
{
    Actor311500Work* work;
    SVECTOR          probe;
    u32              rng;
    u16              count;

    work = arg0->work;

    switch (work->field_4C0) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rng         = (u32)Gp_LcgState >> 16;
            if (work->field_4C8 >= 2) {
                work->field_4C0 = (u16)work->field_4C0 + 1;
            } else if (rng & 1) {
                work->field_4C0 = (u16)work->field_4C0 + 1;
            } else {
                _actor311500ResetAnim(arg0, 0x20);
                work->field_4C0 = (u16)work->field_4C0 + 2;
            }
            work->field_4C4 = 0;
            break;

        case 1:
            count           = (u16)work->field_4C4;
            work->field_4C4 = count + 1;
            if ((s16)count >= 0x1F) {
                work->field_4C8 = 0;
                work->field_4C0 = 0;
            }
            break;

        case 2:
            if (_actor311500TickAnim(arg0)) {
                work->field_4C0 = 0;
                work->field_4C8 = (u16)work->field_4C8 + 1;
            }
            break;

        default:
            break;
    }
}

static s16 func_actor_311500_80162DDC(Task* arg0)
{
    Actor311500Work*       work = arg0->work;
    Enemy*                 enemy;
    WorldCollisionContact* recs;
    SVECTOR                pos;
    SVECTOR*               pp;
    s32                    v;
    s32                    damage;
    s16                    i;

    enemy = arg0->spawnArg2.pointer;
    pp    = &pos;
    recs  = work->rec18;
    for (i = 0; i < 1; i++) {
        if (recs[i].key.value == 0) {
            break;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pp->vx = recs[i].point.vx;
            pp->vy = recs[i].point.vy;
            pp->vz = recs[i].point.vz;
            v      = recs[i].key.value;
            goto done;
        }
    }
    v = 0;
done:
    work->field_4CC = v;
    if (work->field_4CC != 0) {
        work->field_4D0 = v;
        damage          = Gp_ComputeDamage(work->field_4CC, 0, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, work->field_4CC, 0) != 0) {
            damage *= 5;
            Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords, 0, 0);
        }
        enemy->hp -= damage;
        Gp_ClearRec18Occupied(work->rec18);
        func_800DA6E8(&enemy->node, damage, 0);
    }
    return work->field_4CC;
}

/// Calls `func_800B4114` on animation slots 1 to 18 with a 10-frame count.
static inline void _actor311500BlendAnim(Task* task)
{
    Actor311500Work* work = task->work;
    s32              i;

    i = 1;
    do {
        func_800B4114(&work->rig.anim, i & 0xFFFF, 1, 0, 0xA);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
}

/// Spawns the actor's `func_800FDB18` effect on model coord 2, offset by
/// (0x3C, -0xC, 0x1E), for the id parameter of `field_4D0`.
static inline void _actor311500SpawnEffect(Task* task)
{
    Actor311500Work* work = task->work;
    SVECTOR          pos;
    EffectSpawnArg   eff;

    eff.coord      = &task->extra.tmd->coords[2];
    eff.spawnArgLo = 0x100;
    eff.spawnArgHi = 2;
    pos.vx         = 0x3C;
    pos.vy         = -0xC;
    pos.vz         = 0x1E;
    func_800FDB18(Gp_GetIdParam1(work->field_4D0) & 0xFFFF, &task->extra.tmd->coords[2], &pos, &eff);
}

static s32 func_actor_311500_80162F28(Task* arg0)
{
    Actor311500Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    switch (work->field_4C0) {
        case 0:
            _actor311500BlendAnim(arg0);
            _actor311500SpawnEffect(arg0);
            if (enemy->hp <= 0) {
                return -1;
            }
            work->field_4C0 = (u16)work->field_4C0 + 1;
            break;

        case 1:
            if (_actor311500TickAnim(arg0)) {
                return 1;
            }
            break;
    }
    return 0;
}

static s32 func_actor_311500_801630A4(Task* arg0)
{
    Actor311500Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    MATRIX           mtx;
    VECTOR           scale;
    u16              m22;
    s32              state;
    s16              cur;
    s32              sy;
    s16              ang;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->field_4C0;

    switch (state) {
        case 0:
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(0x400A0008, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            work->field_4C4 = 0;
            work->field_4C0 = ((u16)work->field_4C0) + 1;
            break;

        case 1:
            switch (work->field_4C4) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->field_43C);
                    enemy->node.state.parts.flags = state;
                    break;

                case 0xA:
                    Gp_SpawnEff(0x600A5, &arg0->extra.tmd->coords[2], 3, NULL);
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;

                case 0x16:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;

                case 0x1C:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;

                case 0x50:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    break;

                case 0x104:
                    return 1;
            }

            cur = work->field_4C4;
            if (cur >= 6) {
                coord = arg0->extra.tmd->coords;
                sy    = 0x1000 - (cur - 0x14) * 0xA;
                ang   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                Gfx_RotMatrixY(&mtx, ang, 1);
                scale.vx = 0x1000;
                scale.vy = (s16)sy;
                scale.vz = 0x1000;
                ScaleMatrix(&mtx, &scale);

                m22                  = (u16)mtx.m[0][0];
                coord->coord.m[0][0] = m22;
                m22                  = (u16)mtx.m[0][1];
                coord->coord.m[0][1] = m22;
                m22                  = (u16)mtx.m[0][2];
                coord->coord.m[0][2] = m22;
                m22                  = (u16)mtx.m[1][0];
                coord->coord.m[1][0] = m22;
                m22                  = (u16)mtx.m[1][1];
                coord->coord.m[1][1] = m22;
                m22                  = (u16)mtx.m[1][2];
                coord->coord.m[1][2] = m22;
                m22                  = (u16)mtx.m[2][0];
                coord->coord.m[2][0] = m22;
                m22                  = (u16)mtx.m[2][1];
                coord->coord.m[2][1] = m22;
                m22                  = (u16)mtx.m[2][2];
                coord->composeStamp  = GRAPHICS_COORD_DIRTY;
                coord->coord.m[2][2] = m22;
            }

            work->field_4C4 = ((u16)work->field_4C4) + 1;
            break;

        default:
            return 0;
    }
    return 0;
}

/// Per-frame update. `Task::state` is the actor's phase: 0 sets the actor up,
/// 1 idles until it is hit, 2 plays the hit reaction and returns to 1 or, once
/// the hit points are gone, goes on to 3, which runs the death sequence; 4
/// does nothing.
void func_actor_311500_80163334(Task* arg0)
{
    Task*            actor = arg0;
    Actor311500Work* work;
    Actor311500Work* anim;
    Enemy*           enemy;
    TmdObject*       obj;
    VECTOR           pos;
    s32              state;
    s32              i;
    s32              pan;

    work  = actor->work;
    obj   = actor->extra.tmd;
    state = Gp_StateF0.field_4;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto case1;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto case1;

case0:
    if (work->field_4D6 != 0) {
        obj->flags = work->field_4BC;
    }
    switch (actor->state) {
        case 0:
            Mem_CopyUnaligned(&D_actor_311500_80169304, D_acropolis_fire_escape_80181EC4, 0x20);
            Mem_CopyUnaligned(&D_actor_311500_801692FC, D_acropolis_fire_escape_80181E74, 8);
            Mem_CopyUnaligned(&D_actor_311500_80169324, D_acropolis_fire_escape_8018207C, 0xC);
            func_actor_311500_801629D8(actor);
            work = actor->work;
            anim = work;
            i    = 1;
            do {
                Gp_AnimTickIndex(&anim->rig.anim, i & 0xFFFF);
                i += 1;
            } while (((u32)(i & 0xFFFF)) < 0x13U);
            actor->state += 1;
            goto case1;

        case 1:
            func_actor_311500_80162C34(actor, obj);
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                pan = (s8)Gp_GetObjPan(actor->extra.tmd->coords);
                SndEvt_EnqueueType6(0x400A0007, pan,
                                    (s8)gpGetObjDepth(actor->extra.tmd->coords));
                work->field_4C0 = 0;
                actor->state   += 1;
            }
            Gp_ClearRec18Occupied(work->rec18);
            goto case1;

        case 2:
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                work->field_4C0 = 0;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) > 0) {
                work->field_4C0 = 0;
                actor->state   -= 1;
                goto case1;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) < 0) {
                Mem_Set(D_acropolis_fire_escape_80181EC4, 0, 0x20);
                Mem_Set(D_acropolis_fire_escape_80181E74, 0, 8);
                Mem_Set(D_acropolis_fire_escape_8018207C, 0, 0xC);
                work->field_4D4 = 0;
                work->field_4C0 = 0;
                actor->state   += 1;
            }
            goto case1;

        case 3:
            if ((func_actor_311500_801630A4(actor) << 0x10) != 0) {
                actor->state += 1;
                return;
            }
            goto tail;

        case 4:
            return;
    }
    goto case1;

case2:
    if (work->field_4D6 != state) {
        work->field_4BC = obj->flags;
    }
    actor->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    goto case1;

case1:
    work->field_4D6 = Gp_StateF0.field_4;
tail:
    enemy = actor->spawnArg2.pointer;
    Gp_UpdateCoord(&actor->extra.tmd->coords[1]);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

void func_actor_311500_801636A0(Task* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = ((Actor311500Work*)arg0->work)->field_4D4;
}
