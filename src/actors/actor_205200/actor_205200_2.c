#include "actor_205200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_training_room.h"

/// Values of `_Actor205200Work::anim`: indices into the package's
/// animation-set table.
enum {
    ACTOR_205200_ANIM_IDLE         = 1, // between the others; the slots are reset on it at spawn
    ACTOR_205200_ANIM_HEAL         = 2, // answer to the paired enemy's heal request: 84 ticks, the heal is signalled on tick 60
    ACTOR_205200_ANIM_HIT_REACTION = 3, // `ACTION_HIT_REACTION`: 35 ticks
    ACTOR_205200_ANIM_FIDGET       = 5  // drawn at random while idle: 64 ticks
};

/// Values of `_Actor205200Work::action`.
enum {
    ACTOR_205200_ACTION_IDLE         = 0, // waits, fidgets and answers the paired enemy's heal requests
    ACTOR_205200_ACTION_HIT_REACTION = 1  // reacts to a hit and has the paired enemy reset
};

/// Values of `_Actor205200Work::actionStep` under `ACTOR_205200_ACTION_IDLE`.
enum {
    ACTOR_205200_IDLE_READY      = 0, // a pending heal request is taken; the fidget draw runs
    ACTOR_205200_IDLE_HEAL_BEGIN = 1, // starts the heal animation
    ACTOR_205200_IDLE_HEAL       = 2, // the heal animation plays
    ACTOR_205200_IDLE_COOLDOWN   = 3, // `healCooldown` runs down and heal requests stay pending; the fidget draw runs
    ACTOR_205200_IDLE_FIDGET     = 4  // the fidget animation plays, then `COOLDOWN` or `READY` follows
};

/// Values of `_Actor205200Work::actionStep` under
/// `ACTOR_205200_ACTION_HIT_REACTION`. The action ends by returning to
/// `ACTOR_205200_ACTION_IDLE` at `ACTOR_205200_IDLE_COOLDOWN`.
enum {
    ACTOR_205200_HIT_REACTION_BEGIN = 0, // starts the animation and raises the paired enemy's reset request
    ACTOR_205200_HIT_REACTION_WAIT  = 1  // the animation plays
};

/// Values of `_Actor205200Work::knockbackStep`.
enum {
    ACTOR_205200_KNOCKBACK_BEGIN = 0, // starts the player's first animation with its rumble, sound and effect
    ACTOR_205200_KNOCKBACK_PUSH  = 1, // moves the player away for 16 ticks, then starts the second animation
    ACTOR_205200_KNOCKBACK_END   = 2  // waits for the second animation to end and releases the player
};

/// Values of `_Actor205200Work::room`: the room the actor was placed in.
enum {
    ACTOR_205200_ROOM_CORRIDOR      = 0, // Shelter B6 corridor
    ACTOR_205200_ROOM_TRAINING_ROOM = 1  // Shelter B6 training room
};

/// Ticks `_Actor205200Work::healCooldown` is loaded with when a heal or a hit
/// reaction ends.
enum { ACTOR_205200_HEAL_COOLDOWN = 600 };

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, two collision
/// spheres with their contact tables, and the state the per-frame tick runs
/// the actor with. Timers count ticks.
///
/// The actor takes no damage and does not move. It is the partner of actor
/// 105100 in `gSceneCombatState.pairedEnemySignals`: it answers that enemy's
/// heal requests, and a hit it reacts to makes that enemy reset. A player
/// character touching it is knocked back.
typedef struct {
    ActorAnimRig19        rig;                 // playback of the model's parts; slots 1 to 18 are driven
    MATRIX                colorMtx;            // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;            // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    hitBody;             // sphere of radius 300 on the model's root, 300 up, that takes the hits
    WorldCollisionContact hitContacts[3];      // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    touchBody;           // sphere on the model's root that senses what touches the actor; `room` picks its centre and radius
    WorldCollisionContact touchContacts[1];    // contacts of `touchBody`; a player character's body in it starts the knockback
    byte                  unknown_51C[0x38];   // No access found; role unproven
    EffectSpawnArg        hitEffectArg;        // argument record of the effect a landed hit spawns, hung off the model's part 3
    byte                  unknown_55C[0x20];   // No access found; role unproven
    s16                   hitCooldown;         // ticks further hits are ignored for, set by a hit whose id asks for it
    s16                   anim;                // `ACTOR_205200_ANIM_*` the actions ask for; 0 from spawn until the first request
    s16                   playingAnim;         // `anim` the slots were last started on
    s16                   animFrame;           // ticks since `playingAnim` was started
    s16                   action;              // `ACTOR_205200_ACTION_*`
    s16                   actionStep;          // stage of the running action: `ACTOR_205200_IDLE_*` or `ACTOR_205200_HIT_REACTION_*`
    s16                   knockbackActive;     // 1 from the player touching `touchBody` until the knockback has ended (0 otherwise)
    s16                   knockbackStep;       // `ACTOR_205200_KNOCKBACK_*`
    s16                   knockbackFrame;      // ticks in the knockback's current stage
    s16                   knockbackFromBehind; // 1 when the player touched the actor while facing away from it, which picks the knockback's animations and facing (0 otherwise)
    s16                   healCooldown;        // ticks before a heal request is answered again: 600 from the end of a heal or of a hit reaction
    s16                   fidgetTimer;         // ticks until the next draw for the fidget animation, 15 to 46 between draws
    s16                   destroyRequested;    // 1 once an actor command has asked for the actor's teardown (0 otherwise)
    s16                   room;                // `ACTOR_205200_ROOM_*`, copied from the placement's `mode`; picks the touch sphere, the room's own tick and the knockback's effect and sound
} _Actor205200Work;
STATIC_ASSERT_SIZEOF(_Actor205200Work, 0x598);

/// Animation block the attack body hands the player with message 0x3F4.
extern AnimationSet* D_actor_205200_80156800[5];
extern AnimationSet* D_actor_205200_801567E8[6];
extern s16           D_actor_205200_801567B0[];
// One collision centre for each of the two placement modes.
extern SVECTOR D_actor_205200_801567B4[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_205200_801567D0[3];

static void func_actor_205200_8014BAE8(Enemy* enemy, Task* task);
static void func_actor_205200_8014BD4C(Task* arg0);
static void func_actor_205200_8014BF28(Task* arg0);
static void func_actor_205200_8014C0C0(Task* arg0);
static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1);
static void func_actor_205200_8014C67C(Task* arg0);
static void func_actor_205200_8014C748(Task* arg0);
static void func_actor_205200_8014C7CC(Task* arg0);
static void func_actor_205200_8014C87C(Task* arg0);
static void func_actor_205200_8014C8D4(Task* arg0);
static void func_actor_205200_8014C924(Enemy* arg0, Task* arg1);

/// The actor's own state handlers - spawn, per-frame tick and teardown - that
/// `func_actor_205200_8014C540` dispatches through by state.
static const EnemyTaskFuncTable3 D_actor_205200_80149E30 = {
    func_actor_205200_8014BAE8,
    func_actor_205200_8014C59C,
    func_actor_205200_8014C924,
};

static AnimationSet _gActor205200Animation08B50;
static AnimationSet _gActor205200Animation0943C;
static AnimationSet _gActor205200Animation09C1C;
static AnimationSet _gActor205200Animation0A964;
static AnimationSet _gActor205200Animation0B1AC;
static AnimationSet _gActor205200Animation0B948;
static AnimationSet _gActor205200Animation0C154;
static AnimationSet _gActor205200Animation0C968;

s32         func_actor_205200_8014C980(Task*, s32, s32, s32);
s32         func_actor_205200_8014C9A0(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
static void func_actor_205200_8014C540(Task*);

static AnimationPackedPose _gActor205200Animation08B50Bank1[29] = {
#include "assets/actor_205200_animation_08B50_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation08B50Bank4[472] = {
#include "assets/actor_205200_animation_08B50_bank4.inc"
};

static AnimationRecord _gActor205200Animation08B50Records[543] = {
#include "assets/actor_205200_animation_08B50_records.inc"
};

static u16 _gActor205200Animation08B50Indices[20] = {
#include "assets/actor_205200_animation_08B50_indices.inc"
};

static AnimationSet _gActor205200Animation08B50 = {
    _gActor205200Animation08B50Records,
    _gActor205200Animation08B50Indices,
    { NULL, _gActor205200Animation08B50Bank1, NULL, NULL, _gActor205200Animation08B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0943CBank1[16] = {
#include "assets/actor_205200_animation_0943C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0943CBank4[223] = {
#include "assets/actor_205200_animation_0943C_bank4.inc"
};

static AnimationRecord _gActor205200Animation0943CRecords[280] = {
#include "assets/actor_205200_animation_0943C_records.inc"
};

static u16 _gActor205200Animation0943CIndices[20] = {
#include "assets/actor_205200_animation_0943C_indices.inc"
};

static AnimationSet _gActor205200Animation0943C = {
    _gActor205200Animation0943CRecords,
    _gActor205200Animation0943CIndices,
    { NULL, _gActor205200Animation0943CBank1, NULL, NULL, _gActor205200Animation0943CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation09C1CBank1[11] = {
#include "assets/actor_205200_animation_09C1C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation09C1CBank4[167] = {
#include "assets/actor_205200_animation_09C1C_bank4.inc"
};

static AnimationRecord _gActor205200Animation09C1CRecords[284] = {
#include "assets/actor_205200_animation_09C1C_records.inc"
};

static u16 _gActor205200Animation09C1CIndices[20] = {
#include "assets/actor_205200_animation_09C1C_indices.inc"
};

static AnimationSet _gActor205200Animation09C1C = {
    _gActor205200Animation09C1CRecords,
    _gActor205200Animation09C1CIndices,
    { NULL, _gActor205200Animation09C1CBank1, NULL, NULL, _gActor205200Animation09C1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0A964Bank1[23] = {
#include "assets/actor_205200_animation_0A964_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0A964Bank4[347] = {
#include "assets/actor_205200_animation_0A964_bank4.inc"
};

static AnimationRecord _gActor205200Animation0A964Records[414] = {
#include "assets/actor_205200_animation_0A964_records.inc"
};

static u16 _gActor205200Animation0A964Indices[20] = {
#include "assets/actor_205200_animation_0A964_indices.inc"
};

static AnimationSet _gActor205200Animation0A964 = {
    _gActor205200Animation0A964Records,
    _gActor205200Animation0A964Indices,
    { NULL, _gActor205200Animation0A964Bank1, NULL, NULL, _gActor205200Animation0A964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B1ACBank1[21] = {
#include "assets/actor_205200_animation_0B1AC_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B1ACBank4[193] = {
#include "assets/actor_205200_animation_0B1AC_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B1ACRecords[254] = {
#include "assets/actor_205200_animation_0B1AC_records.inc"
};

static u16 _gActor205200Animation0B1ACIndices[20] = {
#include "assets/actor_205200_animation_0B1AC_indices.inc"
};

static AnimationSet _gActor205200Animation0B1AC = {
    _gActor205200Animation0B1ACRecords,
    _gActor205200Animation0B1ACIndices,
    { NULL, _gActor205200Animation0B1ACBank1, NULL, NULL, _gActor205200Animation0B1ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B948Bank1[20] = {
#include "assets/actor_205200_animation_0B948_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B948Bank4[172] = {
#include "assets/actor_205200_animation_0B948_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B948Records[235] = {
#include "assets/actor_205200_animation_0B948_records.inc"
};

static u16 _gActor205200Animation0B948Indices[20] = {
#include "assets/actor_205200_animation_0B948_indices.inc"
};

static AnimationSet _gActor205200Animation0B948 = {
    _gActor205200Animation0B948Records,
    _gActor205200Animation0B948Indices,
    { NULL, _gActor205200Animation0B948Bank1, NULL, NULL, _gActor205200Animation0B948Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C154Bank1[15] = {
#include "assets/actor_205200_animation_0C154_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C154Bank4[206] = {
#include "assets/actor_205200_animation_0C154_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C154Records[244] = {
#include "assets/actor_205200_animation_0C154_records.inc"
};

static u16 _gActor205200Animation0C154Indices[20] = {
#include "assets/actor_205200_animation_0C154_indices.inc"
};

static AnimationSet _gActor205200Animation0C154 = {
    _gActor205200Animation0C154Records,
    _gActor205200Animation0C154Indices,
    { NULL, _gActor205200Animation0C154Bank1, NULL, NULL, _gActor205200Animation0C154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C968Bank1[14] = {
#include "assets/actor_205200_animation_0C968_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C968Bank4[207] = {
#include "assets/actor_205200_animation_0C968_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C968Records[248] = {
#include "assets/actor_205200_animation_0C968_records.inc"
};

static u16 _gActor205200Animation0C968Indices[20] = {
#include "assets/actor_205200_animation_0C968_indices.inc"
};

static AnimationSet _gActor205200Animation0C968 = {
    _gActor205200Animation0C968Records,
    _gActor205200Animation0C968Indices,
    { NULL, _gActor205200Animation0C968Bank1, NULL, NULL, _gActor205200Animation0C968Bank4, NULL, NULL, NULL },
};

s16 D_actor_205200_801567B0[2] = {
    4000,
    700,
};

SVECTOR D_actor_205200_801567B4[2] = {
    { 0, -700, -3500, 0 },
    { 0, -700, 0, 0 },
};

TaskDesc D_actor_205200_801567C4 = { { { TASK_BODY_TMD, 96 } }, func_actor_205200_8014C540, { .model = &gActor205200EveBreaMaskedBody } };

TaskMessageEntry D_actor_205200_801567D0[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_205200_8014C980 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_205200_8014C9A0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_205200_801567E8[6] = {
    NULL,
    &_gActor205200Animation09C1C,
    &_gActor205200Animation08B50,
    &_gActor205200Animation0943C,
    NULL,
    &_gActor205200Animation0A964,
};

AnimationSet* D_actor_205200_80156800[5] = {
    NULL,
    &_gActor205200Animation0B1AC,
    &_gActor205200Animation0B948,
    &_gActor205200Animation0C154,
    &_gActor205200Animation0C968,
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveGridOscillator gScreenWaveColumns[10];

ScreenWaveGridOscillator gScreenWaveRows[30];

POLY_FT4 gScreenWaveGrid[2][30][8];

ScreenWaveCtx D_actor_205200_8015B458;

/// Spawn handler: allocates the work block, binds the model's matrices to it,
/// resets animation slots 1..18 on the idle animation and links the hit and
/// touch spheres, the second of which takes its centre and radius from the
/// placement's `mode`, kept as `_Actor205200Work::room`.
static void func_actor_205200_8014BAE8(Enemy* enemy, Task* task)
{
    TmdObject*        tmd;
    GfxCoord*         coords;
    _Actor205200Work* work;
    s32               i;

    tmd    = task->extra.tmd;
    coords = tmd->coords;
    work   = memCalloc(sizeof(_Actor205200Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work           = work;
    tmd->flags           = 0;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    tmd->lightMtx        = &work->lightMtx;
    tmd->colorMtx        = &work->colorMtx;
    enemy->field_4       = &coords->coord;
    enemy->field_48      = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->hitContacts;
    enemy->param                  = NULL;
    enemy->hp                     = 0;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x200;
    work->hitEffectArg.spawnArgHi = 1;
    animationInitContext(&work->rig.anim, D_actor_205200_801567E8, tmd, work->rig.poses, work->rig.slots);
    i = 1;
    do {
        animationResetSlot(&work->rig.anim, i, ACTOR_205200_ANIM_IDLE);
        i++;
    } while (i < ARRAY_SIZE(work->rig.slots));
    work->room                     = enemy->place->mode;
    work->hitBody.pos.vy           = -300;
    work->hitBody.coord            = coords;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x3003C;
    work->hitBody.radius           = 300;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->hitBody);
    Gp_InitRec18Table(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->touchBody.coord            = coords;
    work->touchBody.context.contacts = work->touchContacts;
    work->touchBody.pos.vx           = D_actor_205200_801567B4[work->room].vx;
    work->touchBody.pos.vy           = D_actor_205200_801567B4[work->room].vy;
    work->touchBody.pos.vz           = D_actor_205200_801567B4[work->room].vz;
    work->touchBody.key              = 0;
    work->touchBody.radius           = D_actor_205200_801567B0[work->room];
    work->touchBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->touchBody);
    Gp_InitRec18Table(work->touchContacts, ARRAY_SIZE(work->touchContacts), 0);
    work->touchBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->msgTable         = D_actor_205200_801567D0;
    task->state            = 1;
}

static void func_actor_205200_8014BD4C(Task* arg0)
{
    _Actor205200Work* work;
    s32               i;
    s32               found;
    s32               last;
    s32               n;

    found = 0;
    work  = arg0->work;
    last  = 0;
    SCRATCH_STACK_RESERVE_BYTES(0x10);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
        if (work->hitCooldown != 0) {
            goto end;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        if ((work->hitContacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x20000) {
            func_800DA6E8(&((Enemy*)arg0->spawnArg2.pointer)->node, 0, 0);
            switch (Gp_GetIdParam0(work->hitContacts[i].key.value) & 0xFFFF) {
                case 1:
                    found = 1;
                    break;
                case 2:
                    break;
            }
            if (found == 0) {
                break;
            }
            work->action     = ACTOR_205200_ACTION_HIT_REACTION;
            work->actionStep = ACTOR_205200_HIT_REACTION_BEGIN;
            if (last != work->hitContacts[i].key.value) {
                last = work->hitContacts[i].key.value;
                func_800FDB18(Gp_GetIdParam1(last) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL,
                              &work->hitEffectArg);
            }
            if ((n = Gp_GetIdParam2(work->hitContacts[i].key.value)) > 0) {
                work->hitCooldown = n;
            }
        }
    }
end:
    Gp_ClearRec18Occupied(work->hitContacts);
    if (work->touchContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        if ((work->touchContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000 && gPlayerStatus.hp > 0) {
            work->knockbackActive = 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_EVENT_LOCK;
        }
        Gp_ClearRec18Occupied(work->touchContacts);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// `ACTOR_205200_ACTION_IDLE`, stepped by `actionStep`. `IDLE_READY` takes a
/// pending heal request and `IDLE_COOLDOWN` runs `healCooldown` down first;
/// both share the fidget draw: every 15-46 ticks a 1-in-8 draw starts
/// `IDLE_FIDGET`, which also replaces a heal request taken on the same tick.
/// `IDLE_HEAL` plays the heal animation, raising
/// `SCENE_COMBAT_PAIRED_HEAL_READY` on tick 60 and starting the cooldown on
/// tick 84. `IDLE_FIDGET` returns to `IDLE_COOLDOWN` while `healCooldown` is
/// still running and to `IDLE_READY` otherwise.
static void func_actor_205200_8014BF28(Task* arg0)
{
    _Actor205200Work* work;
    s16               next;

    work = arg0->work;
    switch (work->actionStep) {
        case ACTOR_205200_IDLE_READY:
            if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_HEAL_REQUEST) {
                gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_HEAL_REQUEST);
                work->actionStep                      = ACTOR_205200_IDLE_HEAL_BEGIN;
            }
        tick:
            if (--work->fidgetTimer <= 0) {
                work->fidgetTimer = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F) + 0xF;
                if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 7)) {
                    work->actionStep = ACTOR_205200_IDLE_FIDGET;
                    work->anim       = ACTOR_205200_ANIM_FIDGET;
                }
            }
            break;
        case ACTOR_205200_IDLE_HEAL_BEGIN:
            work->anim       = ACTOR_205200_ANIM_HEAL;
            work->actionStep = ACTOR_205200_IDLE_HEAL;
            break;
        case ACTOR_205200_IDLE_HEAL:
            if (work->animFrame == 0x3C) {
                gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_HEAL_READY;
            }
            if (work->animFrame >= 0x54) {
                work->anim         = ACTOR_205200_ANIM_IDLE;
                work->actionStep   = ACTOR_205200_IDLE_COOLDOWN;
                work->healCooldown = ACTOR_205200_HEAL_COOLDOWN;
            }
            break;
        case ACTOR_205200_IDLE_COOLDOWN:
            if (--work->healCooldown <= 0) {
                work->actionStep = ACTOR_205200_IDLE_READY;
            }
            goto tick;
        case ACTOR_205200_IDLE_FIDGET:
            if (work->animFrame >= 0x40) {
                next = ACTOR_205200_IDLE_READY;
                if (work->healCooldown > 0) {
                    next = ACTOR_205200_IDLE_COOLDOWN;
                }
                work->actionStep = next;
                work->anim       = ACTOR_205200_ANIM_IDLE;
            }
            break;
    }
}

/// The knockback, run while `knockbackActive` is set. It carves an
/// `ActorPlayerKnockbackScratch` from the scratch stack and steps `knockbackStep`.
/// `KNOCKBACK_BEGIN` records in `knockbackFromBehind` whether the player
/// faces away from the actor, starts the player's first animation and spawns
/// the room's effect on the player; a player already in scripted mode ends
/// the knockback instead. `KNOCKBACK_PUSH` moves the player 100 units a tick
/// directly away from the actor for 16 ticks, facing the actor or away from
/// it as recorded, and starts the second animation after 30 ticks from
/// behind or 32 from the front. `KNOCKBACK_END` waits for that animation to
/// finish, releases the player and clears `knockbackActive`. The duplicated
/// calls in the `room` arms are what the target's shared tails need: jump2's
/// cross-jumping merges them, where a variable or ternary is hoisted instead.
static void func_actor_205200_8014C0C0(Task* arg0)
{
    _Actor205200Work*            work;
    GfxCoord*                    coord;
    Task*                        player;
    GfxCoord*                    target;
    ActorPlayerKnockbackScratch* scratch;
    s32                          sound;
    s32                          count;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerKnockbackScratch);
    coord   = arg0->extra.tmd->coords;
    target  = player->extra.tmd->coords;

    switch (work->knockbackStep) {
        case ACTOR_205200_KNOCKBACK_BEGIN:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->toPlayer.vx                     = target->coord.t[0] - coord->coord.t[0];
                scratch->toPlayer.vy                     = 0;
                scratch->toPlayer.vz                     = target->coord.t[2] - coord->coord.t[2];
                work->knockbackFromBehind                = (scratch->toPlayer.vx * target->coord.m[0][2] + scratch->toPlayer.vz * target->coord.m[2][2]) > 0;
                scratch->playerAnim.source.sets          = D_actor_205200_80156800;
                scratch->playerAnim.animationId          = work->knockbackFromBehind + 1;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &scratch->playerAnim, 0);
                work->knockbackStep  = ACTOR_205200_KNOCKBACK_PUSH;
                work->knockbackFrame = 0;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                scratch->pushDirection.vx = 0;
                scratch->pushDirection.vy = -1000;
                scratch->pushDirection.vz = 0;
                if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
                    Gp_SpawnEff(EFFECT_SHELTER_B6_CORRIDOR_PLAYER_HIT_RING, player->extra.tmd->coords, 0, &scratch->pushDirection);
                } else {
                    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, player->extra.tmd->coords, 0, &scratch->pushDirection);
                }
            } else {
                work->knockbackActive = 0;
            }
            break;
        case ACTOR_205200_KNOCKBACK_PUSH:
            if (work->knockbackFrame < 0x10) {
                scratch->toPlayer.vx = target->coord.t[0] - coord->coord.t[0];
                scratch->toPlayer.vy = target->coord.t[1] - coord->coord.t[1];
                scratch->toPlayer.vz = target->coord.t[2] - coord->coord.t[2];
                VectorNormalS(&scratch->toPlayer, &scratch->pushDirection);
                scratch->playerPlacement.pos.vx = target->coord.t[0] + ((scratch->pushDirection.vx * 25) >> 10);
                scratch->playerPlacement.pos.vy = 0;
                scratch->playerPlacement.pos.vz = target->coord.t[2] + ((scratch->pushDirection.vz * 25) >> 10);
                scratch->playerPlacement.rot.vx = 0;
                if (work->knockbackFromBehind == 0) {
                    scratch->playerPlacement.rot.vy = (ratan2((s16)scratch->toPlayer.vx, (s16)scratch->toPlayer.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    scratch->playerPlacement.rot.vy = ratan2((s16)scratch->toPlayer.vx, (s16)scratch->toPlayer.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                }
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if (work->knockbackFrame == 0x10) {
                if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55180002;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55190003;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            count = ++work->knockbackFrame;
            if ((work->knockbackFromBehind != 0 && count >= 0x1E) || (work->knockbackFromBehind == 0 && count >= 0x20)) {
                scratch->playerAnim.source.sets          = D_actor_205200_80156800;
                scratch->playerAnim.animationId          = work->knockbackFromBehind + 3;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &scratch->playerAnim, 0);
                work->knockbackStep  = ACTOR_205200_KNOCKBACK_END;
                work->knockbackFrame = 0;
            }
            break;
        case ACTOR_205200_KNOCKBACK_END:
            if (++work->knockbackFrame >= 0x25) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->knockbackStep   = ACTOR_205200_KNOCKBACK_BEGIN;
                    work->knockbackFrame  = 0;
                    work->knockbackActive = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerKnockbackScratch);
}

/// Update of the actor's own task: runs the handler of
/// `D_actor_205200_80149E30` that `Task::state` selects, through a stack copy
/// of the table.
static void func_actor_205200_8014C540(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1)
{
    GfxCoord*         coord;
    TmdObject*        obj;
    _Actor205200Work* work;
    s32               state;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    if (gGameSession->eventState != 0) {
        return;
    }
    if (work->destroyRequested != 0) {
        arg1->state = 2;
        return;
    }
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
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags = 0;
    goto default_body;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    func_actor_205200_8014BD4C(arg1);
    func_actor_205200_8014C67C(arg1);
    func_actor_205200_8014C7CC(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    func_actor_205200_8014C87C(arg1);
    func_actor_205200_8014C8D4(arg1);
}

/// Per-frame tick of the live state, run from `func_actor_205200_8014C59C`'s
/// shared body. Bit 0 of `gSceneCombatState.pairedEnemySignals` is a one-shot
/// request: it is cleared here and starts `ACTOR_205200_ACTION_HIT_REACTION`
/// from its first step, as a hit does. `action` then picks the idle or the
/// hit-reaction handler, `room` which of the two rooms' own ticks follows, and
/// `knockbackActive` keeps the knockback running until that body clears it
/// itself.
static void func_actor_205200_8014C67C(Task* arg0)
{
    _Actor205200Work* work;

    work = arg0->work;
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_CHARGE_REQUEST) {
        gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_CHARGE_REQUEST);
        work->action                          = ACTOR_205200_ACTION_HIT_REACTION;
        work->actionStep                      = ACTOR_205200_HIT_REACTION_BEGIN;
    }
    switch (work->action) {
        case ACTOR_205200_ACTION_IDLE:
            func_actor_205200_8014BF28(arg0);
            break;
        case ACTOR_205200_ACTION_HIT_REACTION:
            func_actor_205200_8014C748(arg0);
            break;
    }
    if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
        func_shelter_b6_corridor_8017EBA4(arg0);
    } else {
        func_shelter_b6_training_room_80181930(arg0);
    }
    if (work->knockbackActive != 0) {
        func_actor_205200_8014C0C0(arg0);
    }
}

/// `ACTOR_205200_ACTION_HIT_REACTION`, stepped by `actionStep`. On entry it
/// asks for the hit-reaction animation and raises
/// `SCENE_COMBAT_PAIRED_RESET_REQUEST`; once that animation has played 35
/// ticks it asks for the idle one and returns to `ACTOR_205200_ACTION_IDLE`
/// at `ACTOR_205200_IDLE_COOLDOWN` with `healCooldown` restarted.
static void func_actor_205200_8014C748(Task* arg0)
{
    _Actor205200Work* work;
    s16               state;

    work  = arg0->work;
    state = work->actionStep;
    switch (state) {
        case ACTOR_205200_HIT_REACTION_BEGIN:
            work->anim                            = ACTOR_205200_ANIM_HIT_REACTION;
            work->actionStep                      = ACTOR_205200_HIT_REACTION_WAIT;
            gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_RESET_REQUEST;
            return;
        case ACTOR_205200_HIT_REACTION_WAIT:
            if (work->animFrame >= 0x23) {
                work->actionStep   = ACTOR_205200_IDLE_COOLDOWN;
                work->anim         = ACTOR_205200_ANIM_IDLE;
                work->action       = ACTOR_205200_ACTION_IDLE;
                work->healCooldown = ACTOR_205200_HEAL_COOLDOWN;
            }
            return;
    }
}

/// Keeps the rig's slots on the animation the actions ask for. When `anim`
/// differs from `playingAnim` the latter follows it, `animFrame` restarts and
/// every slot 1..18 is pointed at the new animation with an 8-frame blend;
/// otherwise `animFrame` counts and the slots are simply advanced.
static void func_actor_205200_8014C7CC(Task* arg0)
{
    _Actor205200Work* work;
    s32               i;

    work = arg0->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, 8);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Feeds the actor's world position - the translation of its attach
/// coordinate - to `Gp_UpdateActorColor` for its enemy record, with no blend
/// parameters.
static void func_actor_205200_8014C87C(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the ground quad under the actor at its attach coordinate's world
/// position.
static void func_actor_205200_8014C8D4(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x180, 0x80);
}

/// Teardown state: unlinks the enemy's lock-on node and the work's two
/// collision objects, then destroys the enemy.
static void func_actor_205200_8014C924(Enemy* arg0, Task* arg1)
{
    _Actor205200Work* work;

    work = arg1->work;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->hitBody);
    Gp_UnlinkObj(&work->touchBody);
    enemyDestroy(arg0, arg1);
}

/// Message 0x7D5 handler, listed in `D_actor_205200_801567D0` beside the 0x7DB
/// one: `arg2` zero sets the 0x80 flag of the task's model and any other value
/// clears its flags. The opcode itself (`msgId`) is unused.
s32 func_actor_205200_8014C980(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if (arg2 == 0) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags = 0;
    }
    return 0;
}

/// Message 0x7DB handler, listed in `D_actor_205200_801567D0` next to the
/// 0x7D5 one. A non-zero command sets `_Actor205200Work::destroyRequested`, the
/// flag `func_actor_205200_8014C59C` tests to push the actor to state 2.
/// Nothing reads the opcode itself, hence `arg1`.
s32 func_actor_205200_8014C9A0(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor205200Work* work;

    work = arg0->work;
    if (request->command != 0) {
        work->destroyRequested = 1;
    }
    return 0;
}
