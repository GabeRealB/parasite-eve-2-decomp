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
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
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

/// Values of `_Actor311500Work::step`, the stage of the phase `Task::state`
/// selects.
///
/// Every phase starts at step 0 and gives the steps its own meaning.
enum {
    ACTOR_311500_IDLE_STEP_CHOOSE = 0, // draws between a pause and a replay of animation 0; two replays in a row force the pause
    ACTOR_311500_IDLE_STEP_PAUSE  = 1, // holds the pose for 32 ticks
    ACTOR_311500_IDLE_STEP_REPLAY = 2, // plays animation 0 through from its start
    ACTOR_311500_HIT_STEP_START   = 0, // blends into animation 1 and spawns the hit effect; ends the phase at once when the hit points are gone
    ACTOR_311500_HIT_STEP_PLAY    = 1, // plays animation 1 through
    ACTOR_311500_DEATH_STEP_CRY   = 0, // queues the death sound
    ACTOR_311500_DEATH_STEP_BURN  = 1  // runs the burn-away sequence that `stepFrame` times
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, the body that takes the hits with its one contact
/// record, storage for the model's matrices and the state of the idle, hit and
/// death phases.
///
/// Animation numbers are indices into the package's animation-set table, and
/// only slots 1 to 18 of the rig are driven. A contact key is a
/// `WorldCollisionContact` key: its category in the high half and the
/// identity of what made the contact in the low half.
typedef struct {
    ActorAnimRig19        rig;              // playback of the model's parts; slot 1 reaching a boundary ends an idle replay and the hit reaction
    WorldCollisionBody    hitBody;          // sphere of radius 400 at model part 2 that takes the hits; unlinked when the death sequence begins
    WorldCollisionContact hitContacts[1];   // the one contact of `hitBody`; also the enemy's hit records until the death sequence begins
    MATRIX                lightMtx;         // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;         // storage for the model's `TmdObject::colorMtx`
    Task*                 playerTask;       // the player's task as the spawn found it; never read
    MATRIX*               playerCoordMtx;   // the player's root coordinate matrix as the spawn found it; never read
    u32                   savedModelFlags;  // the model's `TmdObject::flags` as actor control 2 found them before hiding it; written back when the control returns to 0 from any other value
    s16                   step;             // `ACTOR_311500_*_STEP_*` of the running phase; every phase change restarts it at 0
    byte                  pad_4C2[2];       // never accessed
    s16                   stepFrame;        // ticks counted by the idle pause and by the burn-away sequence, each of which restarts it at 0
    byte                  pad_4C6[2];       // never accessed
    s16                   idlePlayCount;    // idle replays finished since the last pause
    byte                  pad_4CA[2];       // never accessed
    s32                   hitKey;           // contact key of the category-2 contact found in `hitContacts` this tick; 0 when there is none
    s32                   lastHitKey;       // `hitKey` of the last hit taken; its id parameter 1 selects the hit effect
    u16                   present;          // answer to `ACTOR_MESSAGE_IS_PRESENT` (1 from spawn, 0 once the hit points are gone)
    u16                   prevActorControl; // `gSceneCombatState.actorControl` as the tick last recorded it (0 update, 1 pause, 2 hide); not recorded once the death phase runs
} _Actor311500Work;
STATIC_ASSERT_SIZEOF(_Actor311500Work, 0x4D8);

extern EnemyParams   D_actor_311500_801692C0;
extern AnimationSet* D_actor_311500_801692F4[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_311500_80169330[1];

extern s32 D_actor_311500_801692FC[2];
extern s32 D_actor_311500_80169304[8];
extern s32 D_actor_311500_80169324[3];

static AnimationSet _gActor311500Animation07188;
static AnimationSet _gActor311500Animation07470;
static TmdSource    _gActor311500StrangerBody;
void                func_actor_311500_80163334(Task*);
s32                 func_actor_311500_801636A0(Task*, s32, s32, u32*);

static TmdBone _gActor311500StrangerBodySkeleton[19] = {
#include "assets/stranger_body_skeleton.inc"
};

static u32 _gActor311500StrangerBodyPartVerts[19] = {
#include "assets/stranger_body_partVerts.inc"
};

static SVECTOR _gActor311500StrangerBodyVerts[311] = {
#include "assets/stranger_body_verts.inc"
};

static SVECTOR _gActor311500StrangerBodyNormals[309] = {
#include "assets/stranger_body_normals.inc"
};

static u32 _gActor311500StrangerBodyStream[4027] = {
#include "assets/stranger_body_stream.inc"
};

static TmdSource _gActor311500StrangerBody = {
    0,
    20180,
    7860,
    19,
    _gActor311500StrangerBodyPartVerts,
    _gActor311500StrangerBodyVerts,
    _gActor311500StrangerBodyNormals,
    _gActor311500StrangerBodySkeleton,
    _gActor311500StrangerBodyStream,
};

static AnimationPackedPose _gActor311500Animation07188Bank1[6] = {
#include "assets/actor_311500_animation_07188_bank1.inc"
};

static AnimationPackedRotation _gActor311500Animation07188Bank4[81] = {
#include "assets/actor_311500_animation_07188_bank4.inc"
};

static AnimationRecord _gActor311500Animation07188Records[118] = {
#include "assets/actor_311500_animation_07188_records.inc"
};

static u16 _gActor311500Animation07188Indices[20] = {
#include "assets/actor_311500_animation_07188_indices.inc"
};

static AnimationSet _gActor311500Animation07188 = {
    _gActor311500Animation07188Records,
    _gActor311500Animation07188Indices,
    { NULL, _gActor311500Animation07188Bank1, NULL, NULL, _gActor311500Animation07188Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor311500Animation07470Bank1[5] = {
#include "assets/actor_311500_animation_07470_bank1.inc"
};

static AnimationPackedRotation _gActor311500Animation07470Bank4[52] = {
#include "assets/actor_311500_animation_07470_bank4.inc"
};

static AnimationRecord _gActor311500Animation07470Records[99] = {
#include "assets/actor_311500_animation_07470_records.inc"
};

static u16 _gActor311500Animation07470Indices[20] = {
#include "assets/actor_311500_animation_07470_indices.inc"
};

static AnimationSet _gActor311500Animation07470 = {
    _gActor311500Animation07470Records,
    _gActor311500Animation07470Indices,
    { NULL, _gActor311500Animation07470Bank1, NULL, NULL, _gActor311500Animation07470Bank4, NULL, NULL, NULL },
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
    &_gActor311500Animation07470,
    &_gActor311500Animation07188,
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

TaskMessageEntry D_actor_311500_80169330[1] = {
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_311500_801636A0 },
};

TaskDesc D_actor_311500_80169338 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_311500_80163334, { .model = &_gActor311500StrangerBody } }; /// Walks the first `count` contact records (stopping at a zero key) and keeps,

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
    _Actor311500Work* work;
    _Actor311500Work* work2;
    _Actor311500Work* work3;
    Enemy*            enemy;
    GfxCoord*         coords;
    TmdObject*        tmd;
    AreaPlacement*    place;
    s32               i;
    u8                rate;

    coords     = arg0->extra.tmd->coords;
    enemy      = arg0->spawnArg2.pointer;
    tmd        = arg0->extra.tmd;
    work       = memCalloc(sizeof(_Actor311500Work), 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    work2 = arg0->work;
    memFillBytes(work2, 0, sizeof(*work2));
    coords->parent = &gGfxViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx = &work2->lightMtx;
    tmd->colorMtx = &work2->colorMtx;
    tmd->flags    = 0;
    animationInitContext(&work2->rig.anim, D_actor_311500_801692F4, tmd, work2->rig.poses,
                         &work2->rig.slots[0]);
    work2->playerTask     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work2->playerCoordMtx = gPlayerStatus.coordMtx;
    rate                  = ANIMATION_RATE_ONE;
    i                     = 1;
    work3                 = arg0->work;
    do {
        work3->rig.slots[i & 0xFFFF].rate = rate;
        animationResetSlot(&work3->rig.anim, i & 0xFFFF, 0);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->hp                       = 0x32;
    enemy->node.state.parts.flags   = 0;
    enemy->reactionFlags            = 0;
    enemy->param                    = &D_actor_311500_801692C0;
    work2->hitBody.coord            = &arg0->extra.tmd->coords[2];
    work2->hitBody.context.contacts = &work2->hitContacts[0];
    work2->hitBody.pos.vx           = 0;
    work2->hitBody.pos.vy           = 0;
    work2->hitBody.pos.vz           = 0;
    work2->hitBody.key              = 0x3000A;
    work2->hitBody.radius           = 0x190;
    work2->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work2->hitBody);
    work2->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(&work2->hitContacts[0], 1, 0);
    enemy->recs    = &work2->hitContacts[0];
    arg0->msgTable = D_actor_311500_80169330;
    work2->present = 1;
    place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != 0xA) {
        place++;
    }
    Gp_SetTmdBytes(tmd, place->texturePageOffset, place->clutRowOffset);
}

/// Sets animation slots 1 to 18 to play at `rate` and restarts each of them.
static inline void _actor311500ResetAnim(Task* task, u8 rate)
{
    _Actor311500Work* work = task->work;
    s32               i;

    i = 1;
    do {
        work->rig.slots[i & 0xFFFF].rate = rate;
        animationResetSlot(&work->rig.anim, i & 0xFFFF, 0);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
}

/// Advances animation slots 1 to 18 by one frame and returns 1 when slot 1
/// has bit 0 of its flags set, 0 otherwise.
static inline u16 _actor311500TickAnim(Task* task)
{
    _Actor311500Work* work = task->work;
    s32               i;

    i = 1;
    do {
        animationTickSlot(&work->rig.anim, i & 0xFFFF);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        return 1;
    }
    return 0;
}

static void func_actor_311500_80162C34(Task* arg0, TmdObject* arg1)
{
    _Actor311500Work* work;
    SVECTOR           probe;
    u32               rng;

    work = arg0->work;

    switch (work->step) {
        case ACTOR_311500_IDLE_STEP_CHOOSE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rng             = gRandomLcgState >> 16;
            if (work->idlePlayCount >= 2) {
                work->step++;
            } else if (rng & 1) {
                work->step++;
            } else {
                _actor311500ResetAnim(arg0, 0x20);
                work->step += 2;
            }
            work->stepFrame = 0;
            break;

        case ACTOR_311500_IDLE_STEP_PAUSE:
            if (work->stepFrame++ >= 0x1F) {
                work->idlePlayCount = 0;
                work->step          = ACTOR_311500_IDLE_STEP_CHOOSE;
            }
            break;

        case ACTOR_311500_IDLE_STEP_REPLAY:
            if (_actor311500TickAnim(arg0)) {
                work->step = ACTOR_311500_IDLE_STEP_CHOOSE;
                work->idlePlayCount++;
            }
            break;

        default:
            break;
    }
}

static s16 func_actor_311500_80162DDC(Task* arg0)
{
    _Actor311500Work*      work = arg0->work;
    Enemy*                 enemy;
    WorldCollisionContact* recs;
    SVECTOR                pos;
    SVECTOR*               pp;
    s32                    v;
    s32                    damage;
    s16                    i;

    enemy = arg0->spawnArg2.pointer;
    pp    = &pos;
    recs  = work->hitContacts;
    for (i = 0; i < 1; i++) {
        if (recs[i].key.value == 0) {
            break;
        }
        if ((recs[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x20000) {
            pp->vx = recs[i].point.vx;
            pp->vy = recs[i].point.vy;
            pp->vz = recs[i].point.vz;
            v      = recs[i].key.value;
            goto done;
        }
    }
    v = 0;
done:
    work->hitKey = v;
    if (work->hitKey != 0) {
        work->lastHitKey = v;
        damage           = Gp_ComputeDamage(work->hitKey, 0, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, work->hitKey, 0) != 0) {
            damage *= 5;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 0, 0);
        }
        enemy->hp -= damage;
        Gp_ClearRec18Occupied(work->hitContacts);
        func_800DA6E8(&enemy->node, damage, 0);
    }
    return work->hitKey;
}

/// Calls `animationSeekSlotWithBlend` on animation slots 1 to 18 with a 10-frame count.
static inline void _actor311500BlendAnim(Task* task)
{
    _Actor311500Work* work = task->work;
    s32               i;

    i = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, i & 0xFFFF, 1, 0, 0xA);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
}

/// Spawns the actor's `func_800FDB18` effect on model coord 2, offset by
/// (0x3C, -0xC, 0x1E), for id parameter 1 of `lastHitKey`.
static inline void _actor311500SpawnEffect(Task* task)
{
    _Actor311500Work* work = task->work;
    SVECTOR           pos;
    EffectSpawnArg    eff;

    eff.coord      = &task->extra.tmd->coords[2];
    eff.spawnArgLo = 0x100;
    eff.spawnArgHi = 2;
    pos.vx         = 0x3C;
    pos.vy         = -0xC;
    pos.vz         = 0x1E;
    func_800FDB18(Gp_GetIdParam1(work->lastHitKey) & 0xFFFF, &task->extra.tmd->coords[2], &pos, &eff);
}

static s32 func_actor_311500_80162F28(Task* arg0)
{
    _Actor311500Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    switch (work->step) {
        case ACTOR_311500_HIT_STEP_START:
            _actor311500BlendAnim(arg0);
            _actor311500SpawnEffect(arg0);
            if (enemy->hp <= 0) {
                return -1;
            }
            work->step++;
            break;

        case ACTOR_311500_HIT_STEP_PLAY:
            if (_actor311500TickAnim(arg0)) {
                return 1;
            }
            break;
    }
    return 0;
}

static s32 func_actor_311500_801630A4(Task* arg0)
{
    _Actor311500Work* work;
    Enemy*            enemy;
    GfxCoord*         coord;
    MATRIX            mtx;
    VECTOR            scale;
    u16               m22;
    s32               state;
    s16               cur;
    s32               sy;
    s16               ang;
    s32               pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->step;

    switch (state) {
        case ACTOR_311500_DEATH_STEP_CRY:
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(SOUND_ACTOR_311500_DEATH, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->stepFrame = 0;
            work->step++;
            break;

        case ACTOR_311500_DEATH_STEP_BURN:
            switch (work->stepFrame) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->hitBody);
                    enemy->node.state.parts.flags = state;
                    break;

                case 0xA:
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[2], 3, NULL);
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

            cur = work->stepFrame;
            if (cur >= 6) {
                coord = arg0->extra.tmd->coords;
                sy    = 0x1000 - (cur - 0x14) * 0xA;
                ang   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                gfxRotMatrixY(&mtx, ang, 1);
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

            work->stepFrame++;
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
    Task*             actor = arg0;
    _Actor311500Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    VECTOR            pos;
    s32               state;
    s32               pan;

    work  = actor->work;
    obj   = actor->extra.tmd;
    state = gSceneCombatState.actorControl;
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
    if (work->prevActorControl != 0) {
        obj->flags = work->savedModelFlags;
    }
    switch (actor->state) {
        case 0:
            Mem_CopyUnaligned(&D_actor_311500_80169304, gAcropolisFireEscapeCollision04CE8Verts, 0x20);
            Mem_CopyUnaligned(&D_actor_311500_801692FC, gAcropolisFireEscapeCollision04CE8Normals, 8);
            Mem_CopyUnaligned(&D_actor_311500_80169324, gAcropolisFireEscapeCollision04CE8Faces, sizeof(*gAcropolisFireEscapeCollision04CE8Faces));
            func_actor_311500_801629D8(actor);
            work = actor->work;
            _actor311500TickAnim(actor);
            actor->state += 1;
            goto case1;

        case 1:
            func_actor_311500_80162C34(actor, obj);
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                pan = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                SndEvt_EnqueueType6(SOUND_ACTOR_311500_HURT, pan,
                                    (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
                work->step    = 0;
                actor->state += 1;
            }
            Gp_ClearRec18Occupied(work->hitContacts);
            goto case1;

        case 2:
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                work->step = 0;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) > 0) {
                work->step    = 0;
                actor->state -= 1;
                goto case1;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) < 0) {
                memFillBytes(gAcropolisFireEscapeCollision04CE8Verts, 0, 0x20);
                memFillBytes(gAcropolisFireEscapeCollision04CE8Normals, 0, 8);
                memFillBytes(gAcropolisFireEscapeCollision04CE8Faces, 0, sizeof(*gAcropolisFireEscapeCollision04CE8Faces));
                work->present = 0;
                work->step    = 0;
                actor->state += 1;
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
    if (work->prevActorControl != state) {
        work->savedModelFlags = obj->flags;
    }
    actor->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    goto case1;

case1:
    work->prevActorControl = gSceneCombatState.actorControl;
tail:
    enemy = actor->spawnArg2.pointer;
    actorRenderComposeCoord(&actor->extra.tmd->coords[1]);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

s32 func_actor_311500_801636A0(Task* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = ((_Actor311500Work*)arg0->work)->present;
}
