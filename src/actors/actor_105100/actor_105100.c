#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b6_training_room.h"
#include "../../shared/fireball.h"
#include "../../shared/model_placement.h"

/// Main-executable counter whose lowest bit the flicker alternates on.

/// 0x30-byte scratch `func_actor_105100_80132C2C` takes from the scratch stack:
/// `delta` is the player offset whose length feeds `Gp_ComputeDamage`, and
/// `ofs` is the spark offset handed to `Gp_SpawnEff`.
typedef struct Actor105100HitScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ byte    pad_10[0x10];
    /* 0x20 */ SVECTOR ofs;
    /* 0x28 */ byte    pad_28[8];
} Actor105100HitScratch;
STATIC_ASSERT_SIZEOF(Actor105100HitScratch, 0x30);

/// The work block of the glowing projectile this overlay spawns as a second
/// enemy task, the 0x80 bytes its spawn handler asks `memCalloc` for. `obj0`
/// is the body the collision lists carry and `rec20` the contact record whose
/// key ends the flight; `obj38` is the second body, unlinked beside the first
/// when the task is destroyed. This child allocation is distinct from the
/// parent's animation work block and the smaller reaction task's work.
///
/// `field_70` accumulates the per-axis jitter the hover step applies to the
/// coordinate, and the step holds that accumulation inside a fixed bound.
/// `field_78` counts frames within the current step and `field_7A` selects
/// it, `field_7C` is the speed the flight doubles each frame up to a cap, and
/// `field_7E` is the size the billboard is drawn at.
typedef struct Actor105100ProjWork {
    /* 0x00 */ WorldCollisionBody    obj0;
    /* 0x20 */ WorldCollisionContact rec20;
    /* 0x38 */ WorldCollisionBody    obj38;
    /* 0x58 */ WorldCollisionCapsule pose;
    /* 0x70 */ SVECTOR               field_70;
    /* 0x78 */ u16                   field_78;
    /* 0x7A */ s16                   field_7A;
    /* 0x7C */ u16                   field_7C;
    /* 0x7E */ s16                   field_7E;
} Actor105100ProjWork;
STATIC_ASSERT_SIZEOF(Actor105100ProjWork, 0x80);

/// 0x38-byte scratch the projectile's per-frame handler takes from
/// the scratch stack: `rot` is the jitter offset it adds to the coordinate and,
/// in the launch step, the rotation `RotMatrix` turns into `mat` before the
/// GTE multiplies it into the coordinate; `vec` is the offset to the player
/// the aiming step orients along. The size is pinned by the handler, which
/// claims and releases the block by decrementing and incrementing the scratch
/// head a whole element at a time.
typedef struct Actor105100ProjScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ VECTOR  vec;
    /* 0x30 */ SVECTOR rot;
} Actor105100ProjScratch;
STATIC_ASSERT_SIZEOF(Actor105100ProjScratch, 0x38);

/// Values of `_Actor105100Work::action`: the handler the per-frame tick runs.
///
/// A handler numbers its own stages in `actionStep`, from 0 on entry.
enum {
    ACTOR_105100_ACTION_IDLE         = 0, // waits out `timer`, then picks a summon, or the charge after three of them; turns to `RAISE_SHIELD` first whenever the shield is down and may go up
    ACTOR_105100_ACTION_FIREBALLS    = 1, // gathers up to four fireballs above itself and releases them at the player
    ACTOR_105100_ACTION_BEAMS        = 2, // sends out the beams of the pattern it draws and holds until they are gone or their time is up
    ACTOR_105100_ACTION_CHARGE       = 3, // charges for 188 ticks with the shield down, then strikes the player wherever they stand; a stagger or a buildup breaks it
    ACTOR_105100_ACTION_RAISE_SHIELD = 4, // 60 ticks of the raise animation, then the shield is up
    ACTOR_105100_ACTION_BUILDUP      = 5, // held by a buildup reaction until the enemy's buildup has run its length
    ACTOR_105100_ACTION_STAGGER      = 6, // flinches for 29 ticks: 420 damage inside the stagger window, a staggering hit, damage over time or the paired enemy's reset request
    ACTOR_105100_ACTION_DEFEATED     = 7  // out of hit points: ends its sounds and effect and puts the task in its death state, or staggers with 1 hit point when the player has already died
};

/// Values of `_Actor105100Work::anim`: indices into the package's
/// animation-set table.
enum {
    ACTOR_105100_ANIM_IDLE         = 1, // between actions; also the first 31 ticks of the death state
    ACTOR_105100_ANIM_RAISE_SHIELD = 2, // `ACTION_RAISE_SHIELD`
    ACTOR_105100_ANIM_CAST         = 3, // held through both summons and the charge
    ACTOR_105100_ANIM_STRIKE       = 4, // release of the charge; the strike lands on its tick 12
    ACTOR_105100_ANIM_STRIKE_END   = 5, // 28 ticks back to idle after the strike
    ACTOR_105100_ANIM_STAGGER      = 6, // `ACTION_STAGGER`
    ACTOR_105100_ANIM_DIE          = 7, // death state
    ACTOR_105100_ANIM_BUILDUP      = 8, // held while `ACTION_BUILDUP` waits
    ACTOR_105100_ANIM_BUILDUP_END  = 9, // 11 ticks back to idle after the buildup
    ACTOR_105100_ANIM_CAST_END     = 10 // 26 ticks back to idle after a summon
};

/// Values of `_Actor105100Work::summonPhase`: what the running summon asks of
/// the child tasks it spawned, which read it through their parent task.
enum {
    ACTOR_105100_SUMMON_NONE             = 0, // no summon: hovering fireballs and beams end themselves
    ACTOR_105100_SUMMON_FIREBALLS_GATHER = 1, // fireballs are being sent out and hover where they appeared
    ACTOR_105100_SUMMON_FIREBALLS_LAUNCH = 2, // 15 ticks before the summon ends: each hovering fireball counts down its own delay and flies
    ACTOR_105100_SUMMON_BEAMS            = 3  // the beams are out
};

/// Values of `_Actor105100Work::beamPattern`. Each beam copies the pattern
/// when it spawns; it selects the beam's motion, its attack and how long it
/// lasts.
enum {
    ACTOR_105100_BEAMS_PAIR   = 0, // two beams, each moved through two legs of a path, for 60 ticks
    ACTOR_105100_BEAMS_TRIPLE = 1, // three beams, each moved along a straight line, for 60 ticks
    ACTOR_105100_BEAMS_SEEKER = 2  // one beam that keeps turning toward the player, for 120 ticks
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, three collision
/// spheres with their contact tables, and the state the per-frame tick runs
/// the fight with. The fireball and beam tasks the summons spawn reach it
/// through their parent task.
///
/// The actor stays where it was placed: each tick its root is rebuilt from
/// `placementMtx` and `scale`. Timers count ticks.
typedef struct {
    ActorAnimRig19        rig;                 // playback of the model's parts; slots 1 to 18 are driven
    MATRIX                colorMtx;            // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;            // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    hitBody;             // sphere of radius 800 on the model's part 3 that takes the hits
    WorldCollisionContact hitContacts[3];      // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    strikeBody;          // sphere of radius 500 on the player's root carrying the charge's attack; its pair tests are on for the one tick the strike lands
    WorldCollisionContact strikeContacts[1];   // contacts of `strikeBody`; initialised and never read
    WorldCollisionBody    touchBody;           // sphere of radius 1200 on the model's root, 300 down its -Z, that senses what touches the actor
    WorldCollisionContact touchContacts[1];    // contacts of `touchBody`; a player character's body in it starts the knockback
    EffectSpawnArg        hitEffectArg;        // argument record of the effect a landed hit spawns, hung off the model's part 3
    EffectWork*           ringEffect;          // ring effect of `ACTION_BEAMS` or `ACTION_CHARGE` while it plays, NULL otherwise; whatever interrupts the action ends it
    MATRIX                placementMtx;        // the root's matrix before `scale`: the placement's from spawn, the root's own as the death state found it
    s32                   fireballSound;       // request id of the looping sound of `ACTION_FIREBALLS`, 0 while it is not playing
    s32                   beamSound;           // request id of the looping sound of `ACTION_BEAMS`, 0 while it is not playing
    s32                   chargeSound;         // request id of the looping sound of `ACTION_CHARGE`, 0 while it is not playing
    s16                   hitCooldown;         // ticks further hits are ignored for, set by a hit whose id asks for it
    s16                   anim;                // `ACTOR_105100_ANIM_*` the actions ask for
    s16                   playingAnim;         // `anim` the slots were last started on
    s16                   animFrame;           // ticks since `playingAnim` was started
    s16                   scale;               // scale of the root over `placementMtx`, 4096 for 1: 2.5 on every axis while alive; the death state restarts it at 1 and shrinks the height to an eighth
    s16                   action;              // `ACTOR_105100_ACTION_*`
    s16                   actionStep;          // stage of the running action, 0 on entry; the death state of the task counts its own stages in it (0 begin, 1 fade delay, 2 death animation and room event, 3 shrink, 4 destroy)
    s16                   timer;               // countdown of the running action's wait; the death state counts its 31-tick delay and its 60 ticks of shrinking up in it
    s16                   fireballTimer;       // ticks until `ACTION_FIREBALLS` sends out its next fireball
    s16                   engageDelay;         // ticks of idle battle phase left before the actor engages the battle and selects scene music entry 10; 15 at spawn
    s16                   knockbackFromBehind; // 1 when the player touched the actor while facing away from it, which picks the knockback's animations and facing (0 otherwise)
    s16                   knockbackActive;     // 1 from the player touching `touchBody` until the knockback has ended (0 otherwise)
    s16                   knockbackStep;       // stage of the knockback (0 start the player's first animation, 1 push the player away and start the second, 2 wait for it to end)
    s16                   knockbackFrame;      // ticks in the knockback's current stage
    union {
        struct {
            s16 active;    // 1 while the shield is up: hits do half damage or none, are refused their buildup and damage over time, and arcs play over the model (0 otherwise)
            s16 cooldown;  // ticks before `ACTION_IDLE` may raise the shield again
        } fields;
        s32 word;          // both halves at once: 0 when the shield is down and may go up
    } shield;              // the halves are stored apart and tested together
    s16 summonPhase;       // `ACTOR_105100_SUMMON_*`
    s16 childCount;        // children of the running summon: fireballs sent out so far, or beams still alive
    s16 beamPattern;       // `ACTOR_105100_BEAMS_*` drawn for the running `ACTION_BEAMS`
    s16 summonCount;       // summons picked since the last charge; `ACTION_IDLE` follows the third with `ACTION_CHARGE`
    s16 charging;          // 1 from the start of `ACTION_CHARGE` until its strike has been thrown (0 otherwise)
    s16 chargeBroken;      // 1 once a stagger or a buildup has interrupted a charge; the next pick is then a summon `summonCount` does not count, and clears it
    u16 animCues;          // cue bits of the animation record slot 1 showed last tick; a bit going off plays its sound
    s16 deathEventPending; // 1 from the start of the death animation until the room has been sent its actor event (0 otherwise)
    s16 soundsMuted;       // actor-control mode, 1 or 2, the character sounds were muted under; 0 while they play
    s16 staggerDamage;     // damage taken inside the stagger window; 420 of it staggers the actor
    s16 staggerTimer;      // ticks left of the stagger window, 188 from every damaging hit; `staggerDamage` is cleared once it has run out
    s16 buildupPending;    // 1 from a buildup reaction taking hold until its wait is over; a stagger in between returns to `ACTION_BUILDUP` (0 otherwise)
} _Actor105100Work;
STATIC_ASSERT_SIZEOF(_Actor105100Work, 0x5C4);

/// The child spawner allocates this 0x50-byte collision and reaction block:
/// a WorldCollisionBody, one contact record, and the state the reaction handlers drive.
/// `func_actor_105100_801354E8` dispatches on `field_40`, decrements the
/// `field_48` countdown and chooses the displayed pose in `field_4E`.
/// The movement handlers aim `direction` at approach point `field_44`;
/// `field_46` selects their phase, `travelTicks` counts down the first leg,
/// and `step` is the distance advanced per frame. Only the leading `obj`
/// participates in collision; the tail is movement state, not another body.
typedef struct Actor105100Rec {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[1];
    /* 0x38 */ SVECTOR               direction;
    /* 0x40 */ s16                   field_40;
    /* 0x42 */ s16                   field_42;
    /* 0x44 */ s16                   field_44;
    /* 0x46 */ s16                   field_46;
    /* 0x48 */ s16                   field_48;
    /* 0x4A */ s16                   travelTicks;
    /* 0x4C */ s16                   step;
    /* 0x4E */ u16                   field_4E;
} Actor105100Rec;
STATIC_ASSERT_SIZEOF(Actor105100Rec, 0x50);

static void func_actor_105100_801327B4(Enemy* arg0, Task* arg1);
static void func_actor_105100_80132AA0(Enemy* arg0, Task* arg1);
static void func_actor_105100_80132C2C(Task* arg0);
static void func_actor_105100_80133134(Task* arg0);
static void func_actor_105100_8013329C(Task* arg0, Enemy* arg1);
static void func_actor_105100_8013345C(Task* arg0, Enemy* arg1);
static void func_actor_105100_801336B8(Task* arg0, Enemy* arg1);
static void func_actor_105100_80133A14(Task* arg0, Enemy* arg1);
static void func_actor_105100_80133CE4(Task* arg0);
static void func_actor_105100_80134130(Task* arg0);
static void func_actor_105100_80134284(Enemy* arg0, Task* arg1);
static void func_actor_105100_801347D4(Enemy* arg0, Task* arg1);
static void func_actor_105100_80134B00(Enemy* arg0, Task* arg1);
static void func_actor_105100_80135278(Enemy* arg0, Task* arg1);
static void func_actor_105100_801354E8(Enemy* arg0, Task* arg1);
static void func_actor_105100_80135674(Task* arg0);
static void func_actor_105100_801359B4(Task* arg0);
static void func_actor_105100_80135B40(Task* arg0);
static void func_actor_105100_80135E54(Task* arg0);
static void func_actor_105100_80135F50(Task* arg0);
static void func_actor_105100_80135FCC(Task* arg0);
static void func_actor_105100_801360AC(Task* arg0);
static void func_actor_105100_801361C4(Task* arg0);
static void func_actor_105100_801362A0(Task* arg0);
static void func_actor_105100_80136318(Task* arg0);
static void func_actor_105100_80136408(Task* arg0);
static void func_actor_105100_801364CC(Task* arg0);
static void func_actor_105100_80136524(Task* arg0);
static void func_actor_105100_801366D8(Enemy* arg0, Task* arg1);
static void func_actor_105100_80136788(Enemy* arg0, Task* arg1);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Main-executable globals with no module header yet. The death handler withholds
/// message 0x13F4 while the attachment wheel is open (`Gp_StateC08.mode`) or
/// `gDisplayState.pendingMode` is live.

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `Enemy::hp` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.

/// The s16 run at 0x801414C8, one entry per animation in
/// `_Actor105100Work::anim`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `Gp_PackPair` packs the `DamageAttack` at 0x80141380
/// into the work's third list node (`_Actor105100Work::strikeBody.key`), and the
/// `EnemyParams` at 0x80141398 is the parameter record the context points at
/// with `Enemy::param`. Its `hpMax` seeds the enemy's hit points.
extern DamageAttack D_actor_105100_80141380[6];
extern EnemyParams  D_actor_105100_80141398;

/// The animation data `animationInitContext` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

extern SVECTOR D_actor_105100_801413E8[];
extern s16     D_actor_105100_80141448[];
extern s16     D_actor_105100_80141450[];

/// Where each spawned projectile starts relative to the parent's coordinate,
/// indexed by `_Actor105100Work::childCount`.
extern SVECTOR D_actor_105100_801414E0[];

/// The enemy task's state handlers, indexed by `Task::state`: spawn/setup,
/// per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_105100_80131E24 = {
    {
        func_actor_105100_801327B4,
        func_actor_105100_80132AA0,
        func_actor_105100_80134284,
    },
};

static TmdSource _gActor105100StingerBody;
void             func_actor_105100_80135DF8(Task*);

static AnimationSet _gActor105100Animation0DD7C;
static AnimationSet _gActor105100Animation0E518;
static AnimationSet _gActor105100Animation0ED24;
static AnimationSet _gActor105100Animation0F538;

static TmdBone _gActor105100StingerBodySkeleton[19] = {
#include "assets/stinger_body_skeleton.inc"
};

static u32 _gActor105100StingerBodyPartVerts[19] = {
#include "assets/stinger_body_partVerts.inc"
};

static SVECTOR _gActor105100StingerBodyVerts[250] = {
#include "assets/stinger_body_verts.inc"
};

static SVECTOR _gActor105100StingerBodyNormals[257] = {
#include "assets/stinger_body_normals.inc"
};

static u32 _gActor105100StingerBodyStream[3520] = {
#include "assets/stinger_body_stream.inc"
};

static TmdSource _gActor105100StingerBody = {
    0,
    17784,
    6072,
    19,
    _gActor105100StingerBodyPartVerts,
    _gActor105100StingerBodyVerts,
    _gActor105100StingerBodyNormals,
    _gActor105100StingerBodySkeleton,
    _gActor105100StingerBodyStream,
};

static AnimationPackedPose _gActor105100Animation09B8CBank1[10] = {
#include "assets/actor_105100_animation_09B8C_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation09B8CBank4[167] = {
#include "assets/actor_105100_animation_09B8C_bank4.inc"
};

static AnimationRecord _gActor105100Animation09B8CRecords[300] = {
#include "assets/actor_105100_animation_09B8C_records.inc"
};

static u16 _gActor105100Animation09B8CIndices[20] = {
#include "assets/actor_105100_animation_09B8C_indices.inc"
};

static AnimationSet _gActor105100Animation09B8C = {
    _gActor105100Animation09B8CRecords,
    _gActor105100Animation09B8CIndices,
    { NULL, _gActor105100Animation09B8CBank1, NULL, NULL, _gActor105100Animation09B8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0A30CBank1[14] = {
#include "assets/actor_105100_animation_0A30C_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0A30CBank4[170] = {
#include "assets/actor_105100_animation_0A30C_bank4.inc"
};

static AnimationRecord _gActor105100Animation0A30CRecords[248] = {
#include "assets/actor_105100_animation_0A30C_records.inc"
};

static u16 _gActor105100Animation0A30CIndices[20] = {
#include "assets/actor_105100_animation_0A30C_indices.inc"
};

static AnimationSet _gActor105100Animation0A30C = {
    _gActor105100Animation0A30CRecords,
    _gActor105100Animation0A30CIndices,
    { NULL, _gActor105100Animation0A30CBank1, NULL, NULL, _gActor105100Animation0A30CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0AFDCBank1[16] = {
#include "assets/actor_105100_animation_0AFDC_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0AFDCBank4[298] = {
#include "assets/actor_105100_animation_0AFDC_bank4.inc"
};

static AnimationRecord _gActor105100Animation0AFDCRecords[454] = {
#include "assets/actor_105100_animation_0AFDC_records.inc"
};

static u16 _gActor105100Animation0AFDCIndices[20] = {
#include "assets/actor_105100_animation_0AFDC_indices.inc"
};

static AnimationSet _gActor105100Animation0AFDC = {
    _gActor105100Animation0AFDCRecords,
    _gActor105100Animation0AFDCIndices,
    { NULL, _gActor105100Animation0AFDCBank1, NULL, NULL, _gActor105100Animation0AFDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0B48CBank1[7] = {
#include "assets/actor_105100_animation_0B48C_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0B48CBank4[114] = {
#include "assets/actor_105100_animation_0B48C_bank4.inc"
};

static AnimationRecord _gActor105100Animation0B48CRecords[145] = {
#include "assets/actor_105100_animation_0B48C_records.inc"
};

static u16 _gActor105100Animation0B48CIndices[20] = {
#include "assets/actor_105100_animation_0B48C_indices.inc"
};

static AnimationSet _gActor105100Animation0B48C = {
    _gActor105100Animation0B48CRecords,
    _gActor105100Animation0B48CIndices,
    { NULL, _gActor105100Animation0B48CBank1, NULL, NULL, _gActor105100Animation0B48CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0B8C8Bank1[8] = {
#include "assets/actor_105100_animation_0B8C8_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0B8C8Bank4[97] = {
#include "assets/actor_105100_animation_0B8C8_bank4.inc"
};

static AnimationRecord _gActor105100Animation0B8C8Records[130] = {
#include "assets/actor_105100_animation_0B8C8_records.inc"
};

static u16 _gActor105100Animation0B8C8Indices[20] = {
#include "assets/actor_105100_animation_0B8C8_indices.inc"
};

static AnimationSet _gActor105100Animation0B8C8 = {
    _gActor105100Animation0B8C8Records,
    _gActor105100Animation0B8C8Indices,
    { NULL, _gActor105100Animation0B8C8Bank1, NULL, NULL, _gActor105100Animation0B8C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0BFD8Bank1[12] = {
#include "assets/actor_105100_animation_0BFD8_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0BFD8Bank4[171] = {
#include "assets/actor_105100_animation_0BFD8_bank4.inc"
};

static AnimationRecord _gActor105100Animation0BFD8Records[225] = {
#include "assets/actor_105100_animation_0BFD8_records.inc"
};

static u16 _gActor105100Animation0BFD8Indices[20] = {
#include "assets/actor_105100_animation_0BFD8_indices.inc"
};

static AnimationSet _gActor105100Animation0BFD8 = {
    _gActor105100Animation0BFD8Records,
    _gActor105100Animation0BFD8Indices,
    { NULL, _gActor105100Animation0BFD8Bank1, NULL, NULL, _gActor105100Animation0BFD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0C53CBank1[7] = {
#include "assets/actor_105100_animation_0C53C_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0C53CBank4[108] = {
#include "assets/actor_105100_animation_0C53C_bank4.inc"
};

static AnimationRecord _gActor105100Animation0C53CRecords[196] = {
#include "assets/actor_105100_animation_0C53C_records.inc"
};

static u16 _gActor105100Animation0C53CIndices[20] = {
#include "assets/actor_105100_animation_0C53C_indices.inc"
};

static AnimationSet _gActor105100Animation0C53C = {
    _gActor105100Animation0C53CRecords,
    _gActor105100Animation0C53CIndices,
    { NULL, _gActor105100Animation0C53CBank1, NULL, NULL, _gActor105100Animation0C53CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0C7E4Bank1[4] = {
#include "assets/actor_105100_animation_0C7E4_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0C7E4Bank4[55] = {
#include "assets/actor_105100_animation_0C7E4_bank4.inc"
};

static AnimationRecord _gActor105100Animation0C7E4Records[83] = {
#include "assets/actor_105100_animation_0C7E4_records.inc"
};

static u16 _gActor105100Animation0C7E4Indices[20] = {
#include "assets/actor_105100_animation_0C7E4_indices.inc"
};

static AnimationSet _gActor105100Animation0C7E4 = {
    _gActor105100Animation0C7E4Records,
    _gActor105100Animation0C7E4Indices,
    { NULL, _gActor105100Animation0C7E4Bank1, NULL, NULL, _gActor105100Animation0C7E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0CBFCBank1[7] = {
#include "assets/actor_105100_animation_0CBFC_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0CBFCBank4[95] = {
#include "assets/actor_105100_animation_0CBFC_bank4.inc"
};

static AnimationRecord _gActor105100Animation0CBFCRecords[126] = {
#include "assets/actor_105100_animation_0CBFC_records.inc"
};

static u16 _gActor105100Animation0CBFCIndices[20] = {
#include "assets/actor_105100_animation_0CBFC_indices.inc"
};

static AnimationSet _gActor105100Animation0CBFC = {
    _gActor105100Animation0CBFCRecords,
    _gActor105100Animation0CBFCIndices,
    { NULL, _gActor105100Animation0CBFCBank1, NULL, NULL, _gActor105100Animation0CBFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0D534Bank1[17] = {
#include "assets/actor_105100_animation_0D534_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0D534Bank4[230] = {
#include "assets/actor_105100_animation_0D534_bank4.inc"
};

static AnimationRecord _gActor105100Animation0D534Records[289] = {
#include "assets/actor_105100_animation_0D534_records.inc"
};

static u16 _gActor105100Animation0D534Indices[20] = {
#include "assets/actor_105100_animation_0D534_indices.inc"
};

static AnimationSet _gActor105100Animation0D534 = {
    _gActor105100Animation0D534Records,
    _gActor105100Animation0D534Indices,
    { NULL, _gActor105100Animation0D534Bank1, NULL, NULL, _gActor105100Animation0D534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0DD7CBank1[21] = {
#include "assets/actor_105100_animation_0DD7C_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0DD7CBank4[193] = {
#include "assets/actor_105100_animation_0DD7C_bank4.inc"
};

static AnimationRecord _gActor105100Animation0DD7CRecords[254] = {
#include "assets/actor_105100_animation_0DD7C_records.inc"
};

static u16 _gActor105100Animation0DD7CIndices[20] = {
#include "assets/actor_105100_animation_0DD7C_indices.inc"
};

static AnimationSet _gActor105100Animation0DD7C = {
    _gActor105100Animation0DD7CRecords,
    _gActor105100Animation0DD7CIndices,
    { NULL, _gActor105100Animation0DD7CBank1, NULL, NULL, _gActor105100Animation0DD7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0E518Bank1[20] = {
#include "assets/actor_105100_animation_0E518_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0E518Bank4[172] = {
#include "assets/actor_105100_animation_0E518_bank4.inc"
};

static AnimationRecord _gActor105100Animation0E518Records[235] = {
#include "assets/actor_105100_animation_0E518_records.inc"
};

static u16 _gActor105100Animation0E518Indices[20] = {
#include "assets/actor_105100_animation_0E518_indices.inc"
};

static AnimationSet _gActor105100Animation0E518 = {
    _gActor105100Animation0E518Records,
    _gActor105100Animation0E518Indices,
    { NULL, _gActor105100Animation0E518Bank1, NULL, NULL, _gActor105100Animation0E518Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0ED24Bank1[15] = {
#include "assets/actor_105100_animation_0ED24_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0ED24Bank4[206] = {
#include "assets/actor_105100_animation_0ED24_bank4.inc"
};

static AnimationRecord _gActor105100Animation0ED24Records[244] = {
#include "assets/actor_105100_animation_0ED24_records.inc"
};

static u16 _gActor105100Animation0ED24Indices[20] = {
#include "assets/actor_105100_animation_0ED24_indices.inc"
};

static AnimationSet _gActor105100Animation0ED24 = {
    _gActor105100Animation0ED24Records,
    _gActor105100Animation0ED24Indices,
    { NULL, _gActor105100Animation0ED24Bank1, NULL, NULL, _gActor105100Animation0ED24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105100Animation0F538Bank1[14] = {
#include "assets/actor_105100_animation_0F538_bank1.inc"
};

static AnimationPackedRotation _gActor105100Animation0F538Bank4[207] = {
#include "assets/actor_105100_animation_0F538_bank4.inc"
};

static AnimationRecord _gActor105100Animation0F538Records[248] = {
#include "assets/actor_105100_animation_0F538_records.inc"
};

static u16 _gActor105100Animation0F538Indices[20] = {
#include "assets/actor_105100_animation_0F538_indices.inc"
};

static AnimationSet _gActor105100Animation0F538 = {
    _gActor105100Animation0F538Records,
    _gActor105100Animation0F538Indices,
    { NULL, _gActor105100Animation0F538Bank1, NULL, NULL, _gActor105100Animation0F538Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_105100_80141380[6] = {
    { 32, 6 },
    { 32, 6 },
    { 25, 10 },
    { 25, 1 },
    { 25, 2 },
    { 40, 6 },
};

EnemyParams D_actor_105100_80141398 = { D_actor_105100_80141380, 4000, 1000, 500, 100, 200, 5, 100, 4 };

u16 D_actor_105100_801413A8[16] = {
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u16 D_actor_105100_801413C8[16] = {
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

SVECTOR D_actor_105100_801413E8[6] = {
    { -1000, -100, -1200, 0 },
    { 1000, -100, -1200, 0 },
    { 0, -100, -1200, 0 },
    { 1000, -100, -1200, 0 },
    { -1000, -100, -1200, 0 },
    { 0, -100, -1200, 0 },
};

SVECTOR D_actor_105100_80141418[6] = {
    { 0, 0, 6000, 0 },
    { 5000, 0, 6000, 0 },
    { 2500, 0, 0, 0 },
    { 5000, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2500, 0, 0, 0 },
};

s16 D_actor_105100_80141448[4] = {
    60,
    60,
    120,
    0,
};

s16 D_actor_105100_80141450[10] = {
    0,
    1,
    -1,
    2,
    3,
    4,
    5,
    -1,
    -1,
    0,
};

void func_actor_105100_80135DF8(Task*);
void func_actor_105100_8013667C(Task*);
void func_actor_105100_8013672C(Task*);

TaskDesc D_actor_105100_80141464[3] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_105100_80135DF8, { .model = &_gActor105100StingerBody } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_105100_8013667C, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_105100_8013672C, { .value = 0 } },
};

u8 D_actor_105100_80141488[44] = {
    0,
    0,
    0,
    0,
    172,
    185,
    19,
    128,
    44,
    193,
    19,
    128,
    252,
    205,
    19,
    128,
    172,
    210,
    19,
    128,
    232,
    214,
    19,
    128,
    248,
    221,
    19,
    128,
    84,
    243,
    19,
    128,
    92,
    227,
    19,
    128,
    4,
    230,
    19,
    128,
    28,
    234,
    19,
    128,
};

/// Borrowed player clips for the scripted attack; entry zero is unused.
static AnimationSet* _gActor105100PlayerAnimationSets[5] = {
    NULL,
    &_gActor105100Animation0DD7C,
    &_gActor105100Animation0E518,
    &_gActor105100Animation0ED24,
    &_gActor105100Animation0F538,
};

s16 D_actor_105100_801414C8[12] = {
    0,
    8,
    4,
    4,
    0,
    0,
    2,
    2,
    2,
    0,
    0,
    0,
};

SVECTOR D_actor_105100_801414E0[7] = {
    { 0, 0, 0, 0 },
    { 0, -4000, -2000, 0 },
    { -500, -4000, -2000, 0 },
    { 500, -4000, -2000, 0 },
    { 0, -3500, -2000, 0 },
    { -500, -3500, -2000, 0 },
    { 500, -3500, -2000, 0 },
};

/// The run of actions at 0x801413A8 the idle action's draw picks from, one
/// `s16` entry per draw. Declared as an aggregate on purpose: a bare `extern u16`
/// makes `true_dependence` (`sched.c:846`) drop the dependence between the
/// entry load and the `sh` to `_Actor105100Work::actionStep`, and sched2 then
/// sinks that store past the `sw` of the LCG state instead of leaving the
/// lookup and the action store adjacent at the end of the block.
extern u16 D_actor_105100_801413A8[16];

/// The enemy descriptor run at 0x80141464 the spawn below draws from. Declared
/// as a scalar rather than an aggregate on purpose: only its address is taken,
/// so the two words `Gp_SpawnEnemyFromTable` splits it into are the function's
/// addend, not a load this function has to model.
extern TaskDesc D_actor_105100_80141464[];

/// The 16-entry run at 0x801413C8 the beam summon's LCG draw picks
/// `_Actor105100Work::beamPattern` from. Four entries are 0 (two children), five are 1 (three), seven are 2
/// (one).
extern u16 D_actor_105100_801413C8[16];

static inline void _actor105100AnimUpdate(Task* task);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

/// Spawn/setup handler. It allocates the work block and hangs it off the
/// task, points the model at the block's `lightMtx` and `colorMtx`, and fills
/// the enemy's coordinate, parameters and hit points (`hp`, seeded from the
/// record's `hpMax`).
///
/// `animationInitContext` binds the rig to the package's set table; slots 1..18
/// are then reset. The placement is kept in `placementMtx`, the model starts
/// at 2.5 times its size with the shield up, and the first idle wait is 150
/// ticks. The three bodies are linked with their contact tables: `hitBody` and
/// `touchBody` with pair tests on, `strikeBody` - which rides the player's
/// root - with them off until the charge lands. The model's part 3 is what
/// `hitBody`, `hitEffectArg` and the enemy's own coordinate hang off.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
static void func_actor_105100_801327B4(Enemy* arg0, Task* arg1)
{
    _Actor105100Work*      work;
    TmdObject*             obj;
    GfxCoord*              coord;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor105100Work), 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                   = &arg1->extra.tmd->coords[3];
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0x64;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &D_actor_105100_80141398;
    arg0->recs                    = work->hitContacts;
    arg0->hp                      = D_actor_105100_80141398.hpMax;
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 3;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_105100_80141488, obj, work->rig.poses,
                         work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->placementMtx             = coord->coord;
    work->scale                    = 0x2800;
    work->shield.fields.active     = 1;
    work->engageDelay              = 0xF;
    work->timer                    = 0x96;
    work->hitBody.coord            = &arg1->extra.tmd->coords[3];
    records1                       = work->hitContacts;
    work->hitBody.context.contacts = records1;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0x1F4;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30033;
    work->hitBody.radius           = 0x320;
    work->hitBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->hitBody);
    Gp_InitRec18Table(records1, 3, 0);
    work->hitBody.flags              = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->touchBody.coord            = arg1->extra.tmd->coords;
    records2                         = work->touchContacts;
    work->touchBody.context.contacts = records2;
    work->touchBody.pos.vx           = 0;
    work->touchBody.pos.vy           = 0;
    work->touchBody.pos.vz           = -0x12C;
    work->touchBody.key              = 0;
    work->touchBody.radius           = 0x4B0;
    work->touchBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->touchBody);
    Gp_InitRec18Table(records2, 1, 0);
    work->touchBody.flags             = (u16)(work->touchBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->strikeBody.coord            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    records3                          = work->strikeContacts;
    work->strikeBody.context.contacts = records3;
    work->strikeBody.pos.vx           = 0;
    work->strikeBody.pos.vy           = 0;
    work->strikeBody.pos.vz           = 0;
    work->strikeBody.key              = Gp_PackPair(D_actor_105100_80141380, 5);
    work->strikeBody.radius           = 0x1F4;
    work->strikeBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->strikeBody);
    Gp_InitRec18Table(records3, 1, 0);
    work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->state            = 1;
}

static void func_actor_105100_80132AA0(Enemy* arg0, Task* arg1)
{
    GfxCoord*         coord;
    TmdObject*        obj;
    _Actor105100Work* work;
    s32               state;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    switch (state) {
        case 0:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            if (work->soundsMuted != 0) {
                SndEvt_EnqueueType9(SOUND_BANK_TYPE_CHARACTER_ALL);
                work->soundsMuted = 0;
            }
            break;
        case 1:
            func_actor_105100_801364CC(arg1);
            func_actor_105100_80136524(arg1);
            if (work->soundsMuted == 0) {
                SndEvt_EnqueueType8(SOUND_BANK_TYPE_CHARACTER_ALL);
            }
            work->soundsMuted = state;
            return;
        case 2:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (work->soundsMuted == 0) {
                SndEvt_EnqueueType8(SOUND_BANK_TYPE_CHARACTER_ALL);
            }
            work->soundsMuted = state;
            return;
    }
    if (arg0->reactionFlags != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->knockbackActive != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    modelPlacementSetScaled(arg1, &work->placementMtx, work->scale, 1);
    if (work->shield.fields.active != 0) {
        func_shelter_b6_training_room_8018294C(arg1);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
}

/// Per-frame hit handler: walks the three `hitContacts` records. A category-2
/// contact lands only while `hitCooldown` is clear. Damage is the player
/// distance through `Gp_ComputeDamage`, quadrupled on a successful
/// `Gp_RollEnemyChance`, and halved (or zeroed for 0x8000 ids) while the
/// shield is up, which also plays the deflect flash and sound. The id
/// parameter may ask for a stagger or, with the shield down, for
/// `Gp_SetObjFlag2` or `Gp_SetObjFlag4`. HP is applied through
/// `func_800E2C78` / `func_800DA6E8`; at 0 the actor goes to
/// `ACTION_DEFEATED`, and `staggerDamage` reaching 0x1A4 (or the stagger
/// request) sends it to `ACTION_STAGGER`, breaking a charge in progress.
/// Either ends `ringEffect`. A new id sparks `func_800FDB18` once, and
/// `Gp_GetIdParam2` arms the cooldown. The tail releases the hit table, steps
/// `staggerTimer`, and starts the knockback when `touchContacts` holds a
/// player character's body.
static void func_actor_105100_80132C2C(Task* arg0)
{
    s32                    flag;
    s32                    lastId;
    Actor105100HitScratch* sc;
    _Actor105100Work*      work;
    Enemy*                 ctx;
    GfxCoord*              coord;
    s32                    i;
    s16                    damage;
    s32                    snd;
    s32                    wait;

    flag   = 0;
    sc     = SCRATCH_STACK_RESERVE_BLOCK(Actor105100HitScratch);
    lastId = 0;
    coord  = arg0->extra.tmd->coords;
    work   = arg0->work;
    ctx    = arg0->spawnArg2.pointer;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((u16)(work->hitContacts[i].key.value >> 16) == 2 && work->hitCooldown == 0) {
            sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage       = Gp_ComputeDamage(work->hitContacts[i].key.value,
                                            SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                                                        sc->delta.vz * sc->delta.vz),
                                            0, 0);
            if (Gp_RollEnemyChance(ctx, work->hitContacts[i].key.value, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
            }
            if (work->shield.fields.active == 1) {
                if (work->hitContacts[i].key.value & 0x8000) {
                    damage = 0;
                } else {
                    damage /= 2;
                }
                sc->ofs.vx = 0;
                sc->ofs.vy = 0;
                sc->ofs.vz = 0xC8;
                Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, &arg0->extra.tmd->coords[3], 0, &sc->ofs);
                snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000D;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            func_800DA6E8(&ctx->node, damage, 0);
            if (damage != 0) {
                switch (Gp_GetIdParam0(work->hitContacts[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        if ((work->hitContacts[i].key.value & 0x3F) != 0x1C) {
                            flag = 1;
                        }
                        break;
                    case 2:
                        if (work->shield.fields.active == 0) {
                            Gp_SetObjFlag2(ctx, work->hitContacts[i].key.value, 0);
                        }
                        break;
                    case 3:
                        if (work->shield.fields.active == 0) {
                            Gp_SetObjFlag4(ctx, work->hitContacts[i].key.value, 0);
                        }
                        break;
                    case 4:
                        flag = 1;
                        break;
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                func_800E2C78(ctx, work->hitContacts[i].key.value, damage, 0);
                ctx->hp -= damage;
                if (ctx->hp <= 0) {
                    work->action     = ACTOR_105100_ACTION_DEFEATED;
                    work->actionStep = 0;
                    if (work->ringEffect != NULL) {
                        work->ringEffect->task->state = 4;
                        work->ringEffect              = NULL;
                    }
                } else {
                    work->staggerDamage += damage;
                    work->staggerTimer   = 0xBC;
                    if (work->staggerDamage >= 0x1A4 || flag == 1) {
                        work->staggerTimer  = 0;
                        work->staggerDamage = 0;
                        work->action        = ACTOR_105100_ACTION_STAGGER;
                        work->actionStep    = 0;
                        if (work->ringEffect != NULL) {
                            work->ringEffect->task->state = 4;
                            work->ringEffect              = NULL;
                        }
                        if (work->charging != 0) {
                            work->charging               = 0;
                            work->chargeBroken           = 1;
                            work->shield.fields.cooldown = 0;
                        }
                        work->summonPhase = ACTOR_105100_SUMMON_NONE;
                    }
                }
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (lastId != work->hitContacts[i].key.value) {
                    lastId = work->hitContacts[i].key.value;
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL,
                                  &work->hitEffectArg);
                }
                wait = Gp_GetIdParam2(work->hitContacts[i].key.value);
                if (wait > 0) {
                    work->hitCooldown = wait;
                }
            }
        }
    }
    Gp_ClearRec18Occupied(work->hitContacts);
    work->staggerTimer--;
    if (work->staggerTimer <= 0) {
        work->staggerDamage = 0;
    }
    if (work->touchContacts[0].flags & 1) {
        if ((work->touchContacts[0].key.value & 0xFFFF0000) == 0x10000 && gPlayerStatus.hp > 0) {
            work->knockbackActive = 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_EVENT_LOCK;
        }
        Gp_ClearRec18Occupied(work->touchContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor105100HitScratch);
}

/// The enemy's action dispatcher, run every frame. Bit 3 of
/// `gSceneCombatState.pairedEnemySignals` is a reset request: it is cleared
/// here and the actor is put in `ACTION_STAGGER` with its shield down.
///
/// `ACTION_DEFEATED` hands the task to its death state, so it falls straight
/// through to the tail, as does an action outside the enumeration. The tail
/// answers the paired enemy's heal (`SCENE_COMBAT_PAIRED_HEAL_READY`) and
/// steps `shield.fields.cooldown` down while it is positive. `ACTION_IDLE`
/// also raises the heal request once the HP drops under the cap in
/// `D_actor_105100_80141398.hpMax`.
static void func_actor_105100_80133134(Task* arg0)
{
    _Actor105100Work* work;
    Enemy*            ctx;
    s16               state;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_RESET_REQUEST) {
        gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_RESET_REQUEST);
        work->action                          = ACTOR_105100_ACTION_STAGGER;
        work->actionStep                      = 0;
        work->shield.fields.active            = 0;
    }
    state = work->action;
    switch (state) {
        case ACTOR_105100_ACTION_IDLE:
            if (ctx->hp < (s32)D_actor_105100_80141398.hpMax) {
                gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_HEAL_REQUEST;
            }
            func_actor_105100_8013329C(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_FIREBALLS:
            func_actor_105100_8013345C(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_BEAMS:
            func_actor_105100_801336B8(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_CHARGE:
            func_actor_105100_80133A14(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_RAISE_SHIELD:
            func_actor_105100_80135F50(arg0);
            break;
        case ACTOR_105100_ACTION_BUILDUP:
            func_actor_105100_801360AC(arg0);
            break;
        case ACTOR_105100_ACTION_STAGGER:
            func_actor_105100_801361C4(arg0);
            break;
        case ACTOR_105100_ACTION_DEFEATED:
            func_actor_105100_80136318(arg0);
        default:
            break;
    }
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_HEAL_READY) {
        func_actor_105100_80135FCC(arg0);
    }
    if (work->shield.fields.cooldown > 0) {
        work->shield.fields.cooldown--;
    }
}

/// `ACTION_IDLE`: picks what the actor does next.
///
/// `engageDelay` is stepped down at the top while the battle phase is idle;
/// once it has run out the actor engages the battle and scene music entry 0xA
/// is selected in `gStageSceneMusicEntry`. With the shield down and its
/// cooldown spent - both halves of `shield` tested as one word - the action
/// becomes `ACTION_RAISE_SHIELD` at once.
///
/// Otherwise step 0 waits `timer` out and chooses the step that picks: 1 while
/// fewer than three summons have run since the last charge, 2 after the third,
/// and 3 when a charge was broken. Steps 1 and 3 draw the next summon from
/// `D_actor_105100_801413A8` with the LCG, step 1 also counting it in
/// `summonCount`; step 2 starts `ACTION_CHARGE` and clears the count.
static void func_actor_105100_8013329C(Task* arg0, Enemy* arg1)
{
    _Actor105100Work* work;
    s16               state;
    s16               summon;

    work = arg0->work;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        if (--work->engageDelay <= 0) {
            Gp_ArmStateF0(1);
            gStageSceneMusicEntry = 0xA;
        }
    }
    if (work->shield.word == 0) {
        work->action     = ACTOR_105100_ACTION_RAISE_SHIELD;
        work->actionStep = 0;
        return;
    }
    state = work->actionStep;
    switch (state) {
        case 0:
            if (--work->timer <= 0) {
                work->timer = 0;
                if (work->chargeBroken == 0) {
                    state = 2;
                    if (work->summonCount < 3) {
                        state = 1;
                    }
                    work->actionStep = state;
                    return;
                }
                work->actionStep   = 3;
                work->chargeBroken = 0;
                return;
            }
            return;
        case 1: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;

            summon           = (s16)tbl[(rnd >> 16) & 0xF];
            gRandomLcgState  = rnd;
            work->actionStep = 0;
            work->summonCount++;
            work->action = summon;
            return;
        }
        case 2:
            work->action      = ACTOR_105100_ACTION_CHARGE;
            work->actionStep  = 0;
            work->summonCount = 0;
            return;
        case 3: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;

            gRandomLcgState  = rnd;
            work->action     = (s16)tbl[(rnd >> 16) & 0xF];
            work->actionStep = 0;
            break;
        }
    }
}

/// `ACTION_FIREBALLS`: gathers fireballs above the actor and releases them.
///
/// Step 0 starts the cast animation, clears `fireballTimer` and `childCount`,
/// sets the summon phase to gathering and draws `timer`, the length of the
/// summon (`0x9E` .. `0xBD`), from the LCG.
///
/// Step 1 holds until the animation reaches frame `0x58`: from then on every
/// expiry of `fireballTimer` sends one fireball out through
/// `Gp_SpawnEnemyFromTable` - up to four, and only while the phase is still
/// gathering - and redraws the interval (`0xF` .. `0x1E`). `timer` steps down
/// in parallel: at `0xF` the phase becomes launch, and at zero the step moves
/// on to 2 with the cast-end animation. The single frame `animFrame == 0x58`
/// also starts the summon's looping sound, kept in `fireballSound`, with the
/// model coordinate's pan and depth.
///
/// Step 2 waits for frame `0x1A` of the cast-end animation, then returns to
/// `ACTION_IDLE` with the idle animation and no summon phase, draws the idle
/// wait (`0` .. `0x3F`) into `timer` and stops the looping sound.
static void func_actor_105100_8013345C(Task* arg0, Enemy* arg1)
{
    _Actor105100Work* work;
    GfxCoord*         coord;
    s16               step;
    s32               pan;
    u32               rnd;
    u32               spawnRnd;
    u32               resetRnd;

    work  = arg0->work;
    step  = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (step) {
        case 0:
            work->anim          = ACTOR_105100_ANIM_CAST;
            work->fireballTimer = 0;
            work->childCount    = 0;
            work->actionStep    = 1;
            work->summonPhase   = ACTOR_105100_SUMMON_FIREBALLS_GATHER;
            rnd                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState     = rnd;
            work->timer         = ((rnd >> 16) & 0x1F) + 0x9E;
            return;
        case 1:
            if (work->animFrame >= 0x58) {
                if (--work->fireballTimer <= 0 && work->childCount < 4 && work->summonPhase == ACTOR_105100_SUMMON_FIREBALLS_GATHER) {
                    Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 1, 0,
                                           (Enemy*)arg0->spawnArg2.pointer);
                    work->childCount   += 1;
                    spawnRnd            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = spawnRnd;
                    work->fireballTimer = ((spawnRnd >> 16) & 0xF) + 0xF;
                }
            }
            if (work->timer == 0xF) {
                work->summonPhase = ACTOR_105100_SUMMON_FIREBALLS_LAUNCH;
            }
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->anim       = ACTOR_105100_ANIM_CAST_END;
            }
            if (work->animFrame == 0x58) {
                work->fireballSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330004;
                pan                 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->fireballSound, pan,
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case 2:
            if (work->animFrame >= 0x1A) {
                work->anim        = ACTOR_105100_ANIM_IDLE;
                work->action      = ACTOR_105100_ACTION_IDLE;
                work->actionStep  = 0;
                work->summonPhase = ACTOR_105100_SUMMON_NONE;
                resetRnd          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = resetRnd;
                work->timer       = (resetRnd >> 16) & 0x3F;
                SndEvt_EnqueueType7(work->fireballSound, 1);
                work->fireballSound = 0;
            }
            break;
    }
}

/// `ACTION_BEAMS`: sends out the beams of one pattern and holds while they
/// run.
///
/// Step 0 starts the cast animation, sets the summon phase to beams, clears
/// `childCount` and draws `beamPattern` from `D_actor_105100_801413C8` with
/// the LCG.
///
/// Step 1 holds on `animFrame`: the single frame `0x1E` spawns the summon
/// ring at the model's coordinate - offset 0/-0x6D6/0x320, life 0x3C - kept
/// in `ringEffect`, and plays `...0007`. Once the frame reaches `0x5A` it
/// spawns the pattern's two, three or one beams through
/// `Gp_SpawnEnemyFromTable`, loads `timer` from `D_actor_105100_80141448`,
/// and starts the looping `...0008`, kept in `beamSound`.
///
/// Step 2 waits `timer` out, or leaves as soon as `childCount` is 0, then
/// moves on with the cast-end animation and stops the looping sound.
///
/// Step 3 waits for frame `0x1A`, ends the ring, clears the summon phase and
/// returns to `ACTION_IDLE` with an idle wait of `0` .. `0x3F`.
static void func_actor_105100_801336B8(Task* arg0, Enemy* arg1)
{
    _Actor105100Work* work;
    GfxCoord*         coord;
    SVECTOR           pos;
    s16               step;
    s32               pan;
    s32               pan2;
    s32               snd;

    work  = arg0->work;
    step  = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (step) {
        case 0: {
            u16* tbl;
            u32  rnd;
            s16  kind;

            work->anim        = ACTOR_105100_ANIM_CAST;
            work->actionStep  = 1;
            tbl               = D_actor_105100_801413C8;
            rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            kind              = tbl[(rnd >> 16) & 0xF];
            gRandomLcgState   = rnd;
            work->childCount  = 0;
            work->summonPhase = ACTOR_105100_SUMMON_BEAMS;
            work->beamPattern = kind;
            return;
        }
        case 1:
            if (work->animFrame == 0x1E) {
                pos.vx           = 0;
                pos.vy           = -0x6D6;
                pos.vz           = 0x320;
                work->ringEffect = Gp_SpawnEff((EFFECT_SHELTER_B6_TRAINING_SUMMON_RING | EFFECT_SPAWN_UNLIMITED), arg0->extra.tmd->coords, 0x3C, &pos);
                snd              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330007;
                pan              = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x5A) {
                switch (work->beamPattern) {
                    case ACTOR_105100_BEAMS_PAIR:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        break;
                    case ACTOR_105100_BEAMS_TRIPLE:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        break;
                    case ACTOR_105100_BEAMS_SEEKER:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (Enemy*)arg0->spawnArg2.pointer);
                        break;
                }
                work->timer      = D_actor_105100_80141448[work->beamPattern];
                work->actionStep = 2;
                work->beamSound  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330008;
                pan2             = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->beamSound, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case 2:
            if (--work->timer <= 0 || work->childCount == 0) {
                work->actionStep = 3;
                work->anim       = ACTOR_105100_ANIM_CAST_END;
                SndEvt_EnqueueType7(work->beamSound, 1);
                work->beamSound = 0;
            }
            break;
        case 3: {
            u32         rnd;
            EffectWork* eff;

            if (work->animFrame >= 0x1A) {
                work->anim       = ACTOR_105100_ANIM_IDLE;
                work->action     = ACTOR_105100_ACTION_IDLE;
                work->actionStep = 0;
                rnd              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = rnd;
                eff              = work->ringEffect;
                work->timer      = (rnd >> 16) & 0x3F;
                if (eff != NULL) {
                    eff->task->state = 4;
                }
                work->ringEffect  = NULL;
                work->summonPhase = ACTOR_105100_SUMMON_NONE;
            }
            break;
        }
    }
}

/// `ACTION_CHARGE`: the charged strike.
///
/// Step 0 starts the cast animation, loads `timer` with the 0xBC ticks of the
/// charge, drops the shield with a 0x1E-tick cooldown and marks the charge in
/// progress, then spawns the charge ring at the model's coordinate - offset
/// 0/-0x6D6/0x1F4, living ten ticks longer than the charge and kept in
/// `ringEffect` - and starts the looping `...0009`, kept in `chargeSound`.
/// Step 1 waits the timer out, playing `...000A` at its 0x5A mark; on expiry
/// it starts the strike animation, stops the looping sound, plays `...000B`
/// and runs the pad lerp in. Step 2 turns `strikeBody`'s pair tests on for
/// the single frame `animFrame` is 0xC and off on every other, and moves on
/// to the strike-end animation once the frame passes 0x1B. Step 3 clears
/// `charging` and, past frame 0x1C, returns to `ACTION_IDLE` with an idle
/// wait drawn from the gameplay LCG.
///
/// The pan and depth are cast at the call rather than through locals: the
/// sign extension then occupies the argument's own temporary (`$s0`) instead
/// of `work`'s register, which is what the original allocation needs.
static void func_actor_105100_80133A14(Task* arg0, Enemy* arg1)
{
    _Actor105100Work* work;
    GfxCoord*         self;
    SVECTOR           pos;
    s32               snd;
    u32               rnd;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    switch (work->actionStep) {
        case 0:
            work->anim                   = ACTOR_105100_ANIM_CAST;
            work->timer                  = 0xBC;
            work->shield.fields.cooldown = 0x1E;
            work->shield.fields.active   = 0;
            work->charging               = 1;
            work->actionStep             = 1;
            pos.vx                       = 0;
            pos.vy                       = -0x6D6;
            pos.vz                       = 0x1F4;
            work->ringEffect             = Gp_SpawnEff((EFFECT_SHELTER_B6_TRAINING_CHARGE_RING | EFFECT_SPAWN_UNLIMITED), arg0->extra.tmd->coords, work->timer + 0xA, &pos);
            work->chargeSound            = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330009;
            SndEvt_EnqueueType6(work->chargeSound, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            break;
        case 1:
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->anim       = ACTOR_105100_ANIM_STRIKE;
                SndEvt_EnqueueType7(work->chargeSound, 1);
                work->chargeSound = 0;
                snd               = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000B;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
                Gp_SpawnPadLerp(0xF, 8, 0xFF);
            }
            if (work->timer == 0x5A) {
                snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000A;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            }
            break;
        case 2:
            if (work->animFrame == 0xC) {
                work->ringEffect        = NULL;
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            } else {
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame >= 0x1B) {
                work->actionStep = 3;
                work->anim       = ACTOR_105100_ANIM_STRIKE_END;
            }
            break;
        case 3:
            work->charging = 0;
            if (work->animFrame >= 0x1C) {
                work->anim       = ACTOR_105100_ANIM_IDLE;
                work->action     = ACTOR_105100_ACTION_IDLE;
                work->actionStep = 0;
                rnd              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = rnd;
                work->timer      = (rnd >> 16) & 0x3F;
            }
            break;
    }
}

/// The knockback, run while `knockbackActive` is set. It carves an
/// `ActorPlayerKnockbackScratch` from the scratch stack and steps `knockbackStep`:
/// stage 0 records whether the player faces away from the actor
/// (`knockbackFromBehind`), starts the player's first animation for that side
/// and spawns the flash; stage 1 pushes the player away from the actor for
/// 0x10 frames and starts the second animation after 0x1E/0x20; stage 2 waits
/// for that animation to finish, ends the player's scripted mode and clears
/// `knockbackActive`. A player already in scripted mode cancels it at stage 0.
static void func_actor_105100_80133CE4(Task* arg0)
{
    _Actor105100Work*            work;
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
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->toPlayer.vx                     = target->coord.t[0] - coord->coord.t[0];
                scratch->toPlayer.vy                     = 0;
                scratch->toPlayer.vz                     = target->coord.t[2] - coord->coord.t[2];
                work->knockbackFromBehind                = (scratch->toPlayer.vx * target->coord.m[0][2] + scratch->toPlayer.vz * target->coord.m[2][2]) > 0;
                scratch->playerAnim.source.sets          = _gActor105100PlayerAnimationSets;
                scratch->playerAnim.animationId          = work->knockbackFromBehind + 1;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &scratch->playerAnim, 0);
                work->knockbackStep  = 1;
                work->knockbackFrame = 0;
                Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                scratch->pushDirection.vx = 0;
                scratch->pushDirection.vy = -1000;
                scratch->pushDirection.vz = 0;
                Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, player->extra.tmd->coords, 0, &scratch->pushDirection);
            } else {
                work->knockbackActive = 0;
            }
            break;
        case 1:
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
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55190003;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            count = ++work->knockbackFrame;
            if ((work->knockbackFromBehind != 0 && count >= 0x1E) || (work->knockbackFromBehind == 0 && count >= 0x20)) {
                scratch->playerAnim.source.sets          = _gActor105100PlayerAnimationSets;
                scratch->playerAnim.animationId          = work->knockbackFromBehind + 3;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &scratch->playerAnim, 0);
                work->knockbackStep  = 2;
                work->knockbackFrame = 0;
            }
            break;
        case 2:
            if (++work->knockbackFrame >= 0x25) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->knockbackStep   = 0;
                    work->knockbackFrame  = 0;
                    work->knockbackActive = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerKnockbackScratch);
}

static void func_actor_105100_80134130(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    _Actor105100Work*      work;
    GfxCoord*              self;
    const AnimationRecord* rec;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->animCues & ANIMATION_RECORD_CUE_2)) {
            snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330001;
            pan = (s8)worldCoordGetOriginAudioPan(self);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(self));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->animCues & ANIMATION_RECORD_CUE_1)) {
            snd  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330002;
            pan2 = (s8)worldCoordGetOriginAudioPan(self);
            SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(self));
        }
        work->animCues = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

/// Advances the work block's animation by a frame. A new animation in `anim`
/// reseeds slots 1..18 from it, with the `D_actor_105100_801414C8` entry that
/// id selects, and restarts the frame count in `animFrame`; otherwise the
/// count steps on and every slot is ticked.
static inline void _actor105100AnimUpdate(Task* task)
{
    _Actor105100Work* work;
    s32               i;
    s32               val;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        val               = D_actor_105100_801414C8[work->anim];
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, val);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Teardown handler in `D_actor_105100_80131E24`. Mode 1 of `gSceneCombatState.actorControl` only
/// refreshes the actor colour; mode 2 hides the model and returns. Otherwise it
/// walks `actionStep`: unlink the collision bodies, wait out the fade delay,
/// play the death animation and send the room its actor event, shrink the
/// model's height, then destroy the enemy. A knockback in progress keeps
/// running throughout.
static void func_actor_105100_80134284(Enemy* arg0, Task* arg1)
{
    SVECTOR           dir;
    VECTOR            pos;
    Task*             actor;
    TmdObject*        obj;
    _Actor105100Work* work;
    GfxCoord*         coord;
    Task*             player;
    s32               state;
    s32               snd;
    s16               flag;
    GfxCoord*         colorCoord;

    actor  = arg1;
    obj    = actor->extra.tmd;
    work   = actor->work;
    coord  = obj->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    state  = gSceneCombatState.actorControl;
    if (state == 1) {
        goto color_update;
    }
    if (state >= 2) {
        if (state == 2) {
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        }
    }
    if (work->knockbackActive != 0) {
        func_actor_105100_80133CE4(actor);
    }
    switch (work->actionStep) {
        case 0:
            work->anim         = ACTOR_105100_ANIM_IDLE;
            work->scale        = 0x1000;
            work->placementMtx = coord->coord;
            arg0->recs         = 0;
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->hitBody);
            Gp_UnlinkObj(&work->touchBody);
            Gp_UnlinkObj(&work->strikeBody);
            Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            work->timer      = 0;
            work->actionStep = 1;
            goto color_update;
        case 1:
            if (++work->timer == 0xA) {
                obj->flags = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer >= 0x1F) {
                work->anim       = ACTOR_105100_ANIM_DIE;
                work->actionStep = 2;
                Gp_ReleaseStateF0Add(actor, 0x33);
                work->deathEventPending = 1;
            }
            _actor105100AnimUpdate(actor);
            goto color_update;
        case 2:
            flag = work->deathEventPending;
            if ((flag == 1) && (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) && (Gp_StateC08.mode != flag) &&
                (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
                work->deathEventPending = 0;
            }
            if (work->animFrame == 0xB) {
                snd = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000E;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                Gp_SpawnPadLerp(0xA, 0xFF, 0x40);
            }
            if (work->animFrame == 0x28) {
                snd = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330003;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if (work->animFrame == 0x36) {
                dir.vx = 0;
                dir.vy = 0;
                dir.vz = 0x96;
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &actor->extra.tmd->coords[3], 5, &dir);
                work->timer = 0;
            }
            if ((work->animFrame >= 0x36) && (work->deathEventPending == 0)) {
                work->actionStep = 3;
            }
            _actor105100AnimUpdate(actor);
            goto color_update;
        case 3:
            if (work->scale >= 0x201) {
                work->scale -= 0x50;
            }
            modelPlacementSetScaled(actor, &work->placementMtx, work->scale, 0);
            if (++work->timer >= 0x3C) {
                work->actionStep = 4;
            }
        color_update:
            colorCoord = actor->extra.tmd->coords;
            pos.vx     = colorCoord->workm.t[0];
            pos.vy     = colorCoord->workm.t[1];
            pos.vz     = colorCoord->workm.t[2];
            Gp_UpdateActorColor(actor->spawnArg2.pointer, &pos, 0, 0);
            return;
        case 4:
            enemyDestroy(arg0, actor);
            return;
    }
}

/// Setup handler of the projectile task. It allocates the task's
/// `Actor105100ProjWork` and, if that fails, tears the enemy down and stays on
/// this handler.
///
/// The model's coordinate starts as a copy of the parent's, moved by the
/// `D_actor_105100_801414E0` entry the parent's `childCount` selects and then
/// jittered on each axis by up to 127 units either way from the gameplay LCG.
/// The two list nodes are linked into list 3 - `obj0` on the model coordinate,
/// `obj38` on the pose segment - the collision table is initialised, the
/// billboard size and a random frame count are seeded, and the task moves to
/// `state` 1.
///
/// `seed`, `transY`, `index` and `temp` are shared or split the way they are
/// because the original's register allocation and scheduling depend on it:
/// the state is read before the Y store, which goes through a plain `long*`,
/// and the third table index and address live in temporaries reused later.
static void func_actor_105100_801347D4(Enemy* arg0, Task* arg1)
{
    Task*                parent;
    _Actor105100Work*    parentWork;
    Actor105100ProjWork* work;
    GfxCoord*            coord;
    GfxCoord*            parentCoord;
    long*                transY;
    void*                temp;
    s32                  index;
    s32                  offsetY;
    u32                  seed;
    u32                  rollX;
    u32                  rollY;
    u32                  rollZ;
    u32                  rollA;
    u32                  rollB;
    s32                  amountX;
    s32                  amountY;
    s32                  amountZ;
    s32                  signX;
    s32                  signY;
    s32                  signZ;
    s32                  posX;
    s32                  posY;
    s32                  posZ;

    parent      = arg1->parent;
    coord       = arg1->extra.tmd->coords;
    parentCoord = parent->extra.tmd->coords;
    parentWork  = parent->work;
    work        = memCalloc(0x80, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }

    arg1->work        = work;
    coord->parent     = &gGfxViewCoord;
    coord->coord      = parentCoord->coord;
    coord->coord.t[0] = parentCoord->coord.t[0] + D_actor_105100_801414E0[parentWork->childCount].vx;
    offsetY           = D_actor_105100_801414E0[parentWork->childCount].vy;
    seed              = gRandomLcgState;
    transY            = &coord->coord.t[1];
    *transY           = parentCoord->coord.t[1] + offsetY;
    rollX             = (gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    index             = parentWork->childCount;
    temp              = &D_actor_105100_801414E0[index];
    coord->coord.t[2] = parentCoord->coord.t[2] + (amountX = ((SVECTOR*)temp)->vz);
    amountX           = rollX & 0x7F;
    signX             = rollX & 0x80;
    posX              = coord->coord.t[0];
    coord->coord.t[0] = !signX ? posX - amountX : posX + amountX;

    rollY             = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    amountY           = rollY & 0x7F;
    signY             = rollY & 0x80;
    posY              = coord->coord.t[1];
    coord->coord.t[1] = !signY ? posY - amountY : posY + amountY;

    rollZ             = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    amountZ           = rollZ & 0x7F;
    signZ             = rollZ & 0x80;
    posZ              = coord->coord.t[2];
    coord->coord.t[2] = !signZ ? posZ - amountZ : posZ + amountZ;

    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->obj0.coord            = arg1->extra.tmd->coords;
    work->obj0.context.contacts = &work->rec20;
    work->obj0.pos.vx           = 0;
    work->obj0.pos.vy           = 0;
    work->obj0.pos.vz           = 0;
    work->obj0.key              = Gp_PackPair(D_actor_105100_80141380, 0);
    work->obj0.radius           = 0xC8;
    work->obj0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj0);
    work->pose.ends[0].vx       = 0;
    work->pose.ends[0].vy       = 0;
    work->pose.ends[0].vz       = 0;
    work->pose.ends[1].vx       = 0;
    work->pose.ends[1].vy       = 0;
    work->pose.ends[1].vz       = -0x190;
    work->pose.end0Radius       = 1;
    work->pose.end1Radius       = 1;
    work->pose.contacts         = &work->rec20;
    index                       = work->obj0.flags;
    index                      &= 0x7FFF;
    work->obj0.flags            = index;
    temp                        = arg1->extra.tmd->coords;
    work->obj38.context.capsule = &work->pose;
    work->obj38.pos.vx          = 0;
    work->obj38.pos.vy          = 0;
    work->obj38.pos.vz          = 0;
    work->obj38.key             = 0;
    work->obj38.radius          = 0;
    work->obj38.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->obj38.coord           = temp;
    Gp_LinkObj(3, &work->obj38);
    work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->field_7E  = 0x190;
    rollA           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    rollB           = (rollA * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rollB;
    work->field_78  = ((rollA >> 16) & 0xF) + ((rollB >> 16) & 7);
    arg1->state     = 1;
}

/// The projectile task's state handlers, indexed by `Task::state`: setup,
/// per-frame flight and teardown.
static const EnemyTaskFuncTable3 D_actor_105100_80131E90 = {
    {
        func_actor_105100_801347D4,
        func_actor_105100_80134B00,
        func_actor_105100_801366D8,
    },
};

/// Per-frame handler of the glowing projectile this overlay spawns as its
/// second enemy task, the middle entry of `D_actor_105100_80131E90`. Mode 1 of
/// `gSceneCombatState.actorControl` only redraws the billboard and mode 2 skips the frame.
///
/// `field_7A` steps the projectile through its life. It first hovers, jittering
/// its coordinate by a per-axis offset the gameplay LCG draws and taking each
/// offset only while the accumulated jitter stays inside its bound, and waits
/// there for the parent's state: gone, and the task ends; ready, and a
/// countdown launches it. It then turns onto its heading and starts
/// accelerating, aims itself at the player, and flies, arming its two
/// collision bodies once it is clear of the ground. The flight ends when the
/// contact record reports a hit or the time runs out: the projectile is
/// re-keyed and widened to the burst, which is spawned with its own effect and
/// sound, and a last step fades the body out and destroys the task.
static void func_actor_105100_80134B00(Enemy* arg0, Task* arg1)
{
    Actor105100ProjWork*    work;
    _Actor105100Work*       parentWork;
    GfxCoord*               coord;
    Actor105100ProjScratch* scratch;
    s32                     state;
    u32                     rng;
    u32                     hi;
    s32                     val;
    u16                     speed;
    u16                     timer;
    s32                     snd;
    s32                     n;

    work       = arg1->work;
    coord      = arg1->extra.tmd->coords;
    parentWork = (arg1->parent)->work;
    state      = gSceneCombatState.actorControl;
    if (state == 1) {
        fireballDrawGlow(coord, work->field_7E);
        return;
    }
    if (state < 2) {
        goto body;
    }
    if (state == 2) {
        return;
    }
body:
    SCRATCH_STACK_RESERVE_BLOCK(Actor105100ProjScratch);
    scratch = SCRATCH_STACK_CURSOR(Actor105100ProjScratch);
    switch (work->field_7A) {
        case 0:
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vx = val;
            if (ABS(work->field_70.vx + (s16)val) < 0x1F4) {
                work->field_70.vx += val;
                coord->coord.t[0] += scratch->rot.vx;
            }
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vy = val;
            if (ABS(work->field_70.vy + (s16)val) < 0x1F4) {
                work->field_70.vy += val;
                coord->coord.t[1] += scratch->rot.vy;
            }
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vz = val;
            if (ABS(work->field_70.vz + (s16)val) < 0x1F4) {
                work->field_70.vz += val;
                coord->coord.t[2] += scratch->rot.vz;
            }
            if (parentWork->summonPhase == ACTOR_105100_SUMMON_NONE) {
                arg1->state = 2;
            }
            if (parentWork->summonPhase == ACTOR_105100_SUMMON_FIREBALLS_LAUNCH) {
                timer          = work->field_78 - 1;
                work->field_78 = timer;
                if ((timer << 16) <= 0) {
                    work->field_7A = 1;
                    work->field_7C = 1;
                    work->field_78 = 0;
                }
            }
            goto update;
        case 1:
            scratch->rot.vx = 0x20;
            scratch->rot.vy = 0;
            scratch->rot.vz = 0;
            RotMatrix(&scratch->rot, &scratch->mat);
            gte_SetRotMatrix(&coord->coord);
            gte_ldclmv(&scratch->mat);
            gte_rtir();
            gte_stclmv(&coord->coord);
            gte_ldclmv((char*)&scratch->mat + 2);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 2);
            gte_ldclmv((char*)&scratch->mat + 4);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 4);
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x33) {
                work->field_7C = 0x32;
            }
            coord->coord.t[0] += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1] += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            timer              = work->field_78 + 1;
            work->field_78     = timer;
            if ((s16)timer >= 0x10) {
                work->field_7A = 2;
                work->field_78 = 0;
            }
            goto update;
        case 2:
            timer          = work->field_78 + 1;
            work->field_78 = timer;
            if ((s16)timer >= 3) {
                scratch->vec.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                scratch->vec.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                scratch->vec.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                Gp_OrientAlong(&scratch->vec, &coord->coord, 0);
                work->field_7A = 3;
                work->field_78 = 0;
                work->field_7C = 1;
            }
            goto update;
        update:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            fireballDrawGlow(coord, work->field_7E);
            break;
        case 3:
            n = 4;
            if ((s16)++work->field_78 == n) {
                snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330005;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (coord->coord.t[1] >= -0xF9F) {
                work->obj0.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj38.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x12D) {
                work->field_7C = 0x12C;
            }
            coord->coord.t[0]  += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1]  += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2]  += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            fireballDrawGlow(coord, work->field_7E);
            if (work->rec20.key.value != 0 || (s16)work->field_78 >= 0x1A) {
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                Gp_ClearRec18Occupied(&work->rec20);
                work->obj0.key    = Gp_PackPair(D_actor_105100_80141380, 1);
                work->obj0.radius = 0x1F4;
                work->field_78    = 0x1E;
                work->field_7A    = n;
                Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_ORANGE_BURST, coord, 0, NULL);
                snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330006;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 4:
            if ((s16)work->field_78 == 0x14) {
                work->obj0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            timer          = work->field_78 - 1;
            work->field_78 = timer;
            if ((timer << 16) <= 0) {
                arg1->state = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor105100ProjScratch);
}

static void func_actor_105100_80135278(Enemy* arg0, Task* arg1)
{
    Task*             parent;
    _Actor105100Work* work;
    Actor105100Rec*   obj;
    GfxCoord*         dst;
    GfxCoord*         src;

    parent = arg1->parent;
    work   = parent->work;
    dst    = arg1->extra.tmd->coords;
    src    = parent->extra.tmd->coords;

    if (D_actor_105100_80141450[work->beamPattern * 3 + work->childCount] == -1) {
        enemyDestroy(arg0, arg1);
        return;
    }

    obj = memCalloc(0x50, 0);
    if (obj == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }

    arg1->work    = obj;
    obj->field_40 = work->beamPattern;
    obj->field_42 = work->childCount;
    work->childCount++;
    obj->field_44             = D_actor_105100_80141450[work->beamPattern * 3 + obj->field_42];
    obj->field_48             = D_actor_105100_80141448[obj->field_40];
    obj->field_4E             = 3;
    dst->parent               = &gGfxViewCoord;
    dst->coord                = src->coord;
    dst->coord.t[0]           = src->coord.t[0] + D_actor_105100_801413E8[obj->field_44].vx;
    dst->coord.t[1]           = src->coord.t[1] + D_actor_105100_801413E8[obj->field_44].vy;
    dst->coord.t[2]           = src->coord.t[2] + D_actor_105100_801413E8[obj->field_44].vz;
    dst->composeStamp         = GRAPHICS_COORD_DIRTY;
    obj->obj.coord            = arg1->extra.tmd->coords;
    obj->obj.context.contacts = obj->rec;
    obj->obj.pos.vx           = 0;
    obj->obj.pos.vy           = 0;
    obj->obj.pos.vz           = 0;
    obj->obj.key              = Gp_PackPair(D_actor_105100_80141380, obj->field_40 + 2);
    obj->obj.radius           = 0xC8;
    obj->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &obj->obj);
    Gp_InitRec18Table(obj->rec, 1, 0);
    obj->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    arg1->state     = 1;
}

/// The per-frame handler the `state == 1` dispatch runs: it hands the reaction
/// `field_40` selects to one of the `80135674` / `801359B4` / `80135B40`
/// sub-handlers, retimes the pose every 6/0xB/0x10 frames of the countdown in
/// `field_48`, and ends the fight (`state = 2`) once that countdown, the work's
/// `field_24` and the parent's `summonPhase` all say so.
static void func_actor_105100_801354E8(Enemy* arg0, Task* arg1)
{
    Actor105100Rec*   rec;
    _Actor105100Work* parentWork;
    GfxCoord*         coord;
    s32               state;
    s32               one;
    s16               timer;
    u16               count;

    rec        = arg1->work;
    parentWork = (arg1->parent)->work;
    state      = gSceneCombatState.actorControl;
    coord      = arg1->extra.tmd->coords;
    one        = 1;

    if (state == one) {
        func_shelter_b6_training_room_8017FC40(coord, 0x80, rec->field_4E);
        return;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto done;
    }
default_body:
    if (rec->field_40 == one) {
        goto rec1;
    }
    if (rec->field_40 >= 2) {
        goto ge2;
    }
    if (rec->field_40 == 0) {
        goto rec0;
    }
    goto join;
ge2:
    if (rec->field_40 == 2) {
        goto rec2;
    }
    goto join;
rec0:
    func_actor_105100_80135674(arg1);
    goto join;
rec1:
    func_actor_105100_801359B4(arg1);
    goto join;
rec2:
    func_actor_105100_80135B40(arg1);
join:
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    timer = rec->field_48;
    if (timer < 6) {
        rec->field_4E = 0;
    } else if (timer < 0xB) {
        rec->field_4E = 1;
    } else if (timer < 0x10) {
        rec->field_4E = 2;
    }
    func_shelter_b6_training_room_8017FC40(coord, 0x80, rec->field_4E);
    count         = (u16)rec->field_48 - 1;
    rec->field_48 = count;
    if ((count << 16) <= 0 || rec->rec[0].key.value != 0 ||
        parentWork->summonPhase == ACTOR_105100_SUMMON_NONE) {
        parentWork->childCount = parentWork->childCount - 1;
        arg1->state            = 2;
    }
done:
    return;
}

/// Reaction 0's handler (`field_40 == 0`), which walks the model along a
/// two-leg path through `D_actor_105100_80141418`: `field_44`, then
/// `field_44 + 3`. Pass 0 builds the first-leg aim, measures both legs and
/// stores the per-frame step (total length over `field_48`) plus how many
/// frames the first leg takes; pass 1 walks that step and re-aims at the
/// second point when the countdown hits 0; pass 2 keeps walking.
static void func_actor_105100_80135674(Task* arg0)
{
    Actor105100Rec* rec;
    GfxCoord*       coord;
    VECTOR*         head;
    VECTOR*         vec;
    s16             state;
    s32             dx;
    s32             dz;
    s32             dx2;
    s32             dz2;
    s32             dist;
    s32             speed;
    s16             timer;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = head - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    rec                          = arg0->work;
    state                        = rec->field_46;
    coord                        = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, &rec->direction);
            dx      = vec->vx;
            dz      = vec->vz;
            dist    = SquareRoot0(dx * dx + dz * dz);
            vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx -
                      D_actor_105100_80141418[rec->field_44].vx;
            vec->vy = 0;
            dz2     = D_actor_105100_80141418[rec->field_44 + 3].vz -
                  D_actor_105100_80141418[rec->field_44].vz;
            vec->vz          = dz2;
            dx2              = vec->vx;
            speed            = (dist + SquareRoot0(dx2 * dx2 + dz2 * dz2)) / rec->field_48;
            rec->field_46    = 1;
            rec->step        = speed;
            rec->travelTicks = dist / (s16)speed;
            break;
        case 1:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            timer              = (u16)rec->travelTicks - 1;
            rec->travelTicks   = timer;
            if ((timer << 16) <= 0) {
                vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = D_actor_105100_80141418[rec->field_44 + 3].vz - coord->coord.t[2];
                VectorNormalS(vec, &rec->direction);
                rec->field_46 = 2;
            }
            break;
        case 2:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Reaction 1's handler (`field_40 == 1`), which walks the model towards the
/// approach point `field_44` selects from `D_actor_105100_80141418`. The first
/// pass (`field_46 == 0`) builds the planar delta in 16 bytes of scratch,
/// normalises it into the record's own 0x38 vector and stores the step it then
/// travels per frame -- the delta's length over `field_48`; the second
/// (`field_46 == 1`) applies that step to the coordinate every frame.
static void func_actor_105100_801359B4(Task* arg0)
{
    Actor105100Rec* rec;
    GfxCoord*       coord;
    VECTOR*         head;
    VECTOR*         vec;
    s16             state;
    s32             dx;
    s32             dz;
    s32             speed;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = head - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    rec                          = arg0->work;
    state                        = rec->field_46;
    coord                        = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, &rec->direction);
            dx            = vec->vx;
            dz            = vec->vz;
            speed         = SquareRoot0(dx * dx + dz * dz) / rec->field_48;
            rec->field_46 = 1;
            rec->step     = speed;
            break;
        case 1:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_105100_80135B40(Task* arg0)
{
    ActorFaceScratch* sc;
    GfxCoord*         coord;
    s32               ang;
    s32               cur;
    s16               target;
    s16               diff;
    s32               adiff;
    s16               snap;
    s32               next;
    s32               step;

    sc           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord        = arg0->extra.tmd->coords;
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    ang          = ratan2((s32)(s16)sc->delta.vx, (s32)(s16)sc->delta.vz) & 0xFFF;
    snap         = ang;
    cur          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    target       = cur;
    diff         = ang - cur;
    adiff        = diff >= 0 ? diff : -diff;
    if (adiff < 0x800) {
        target = ang;
        if (adiff >= 0x51) {
            next = (s16)cur;
            if (diff > 0) {
                target = next + 0x50;
            } else {
                target = next - 0x50;
            }
        }
    } else {
        if (diff > 0) {
            if (0x1000 - diff < 0x51) {
                goto snapTurn;
            } else {
                goto turn;
            }
        } else if (0x1000 + diff < 0x51) {
            goto snapTurn;
        } else {
            goto turn;
        }
    snapTurn:
        target = snap;
        goto done;
    turn:
        step = (s16)target;
        if (diff > 0) {
            target = step - 0x50;
        } else {
            target = step + 0x50;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = target;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    coord->coord.t[0] += (coord->coord.m[0][2] * 0xF) >> 0xA;
    coord->coord.t[2] += (coord->coord.m[2][2] * 0xF) >> 0xA;
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

#include "../../shared/fireball_ember.inc.c"

void func_actor_105100_80135DF8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_80135E54(Task* arg0)
{
    Enemy*            enemy;
    _Actor105100Work* work;
    s32               state;
    s32               damage;
    s32               tick;
    u8                flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->action != ACTOR_105100_ACTION_BUILDUP)) {
        work->action         = ACTOR_105100_ACTION_BUILDUP;
        work->actionStep     = 0;
        work->buildupPending = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tick = Gp_TickObjFlag4(enemy) << 0x10;
        if (tick != 0) {
            damage = tick >> 0x12;
            func_800DA6E8(&enemy->node, damage, 0);
            state     = (u16)enemy->hp - damage;
            enemy->hp = state;
            state   <<= 0x10;
            if (state <= 0) {
                state = ACTOR_105100_ACTION_DEFEATED;
            } else {
                state = ACTOR_105100_ACTION_STAGGER;
            }
            work->action     = state;
            work->actionStep = 0;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// `ACTION_RAISE_SHIELD`: starts the raise animation with `timer` at 0x3C
/// frames, then, when the timer runs out, puts the shield up and returns to
/// `ACTION_IDLE` with the idle animation.
static void func_actor_105100_80135F50(Task* arg0)
{
    _Actor105100Work* work;
    s32               state;

    work  = arg0->work;
    state = work->actionStep;
    switch (state) {
        case 0:
            work->anim       = ACTOR_105100_ANIM_RAISE_SHIELD;
            work->timer      = 0x3C;
            work->actionStep = 1;
            break;
        case 1:
            if (--work->timer <= 0) {
                work->shield.fields.active = state;
                work->anim                 = state;
                work->action               = ACTOR_105100_ACTION_IDLE;
                work->actionStep           = 0;
                work->timer                = 0;
            }
            break;
    }
}

static void func_actor_105100_80135FCC(Task* arg0)
{
    Enemy*    enemy;
    GfxCoord* coord;
    s32       snd;
    s32       pan;
    u16       hp;

    enemy                                 = arg0->spawnArg2.pointer;
    coord                                 = arg0->extra.tmd->coords;
    gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_HEAL_READY);
    hp                                    = enemy->hp + 0x50;
    enemy->hp                             = hp;
    if (D_actor_105100_80141398.hpMax < (s16)hp) {
        enemy->hp = D_actor_105100_80141398.hpMax;
    }
    func_800DA6E8(&enemy->node, -0x50, 0);
    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL, NULL, 0, NULL);
    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000C;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
}

/// `ACTION_BUILDUP`: step 0 starts the held animation, breaks a charge in
/// progress, clears the summon phase, stops the looping sounds, turns
/// `strikeBody`'s pair tests off and ends `ringEffect`; unless a charge was
/// broken it also arms the shield cooldown. Step 1 waits on `Gp_TickObjFlag2`
/// before clearing the enemy's buildup bit and `buildupPending`. Step 2 waits
/// for frame 0xB of the recovery animation and returns to `ACTION_IDLE`.
static void func_actor_105100_801360AC(Task* arg0)
{
    _Actor105100Work* work;
    Enemy*            enemy;
    EffectWork*       eff;
    s32               state;

    work  = arg0->work;
    state = work->actionStep;
    enemy = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            work->anim       = ACTOR_105100_ANIM_BUILDUP;
            work->actionStep = 1;
            if (work->charging != 0) {
                work->charging               = 0;
                work->chargeBroken           = 1;
                work->shield.fields.cooldown = 0;
            }
            work->summonPhase = ACTOR_105100_SUMMON_NONE;
            func_actor_105100_801362A0(arg0);
            eff                    = work->ringEffect;
            work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (eff != NULL) {
                eff->task->state = 4;
                work->ringEffect = NULL;
            }
            if (work->chargeBroken == 0) {
                work->shield.fields.cooldown = 0x1E;
            }
            break;
        case 1:
            if (Gp_TickObjFlag2(enemy) != 0) {
                work->anim            = ACTOR_105100_ANIM_BUILDUP_END;
                work->actionStep      = 2;
                work->buildupPending  = 0;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            }
            break;
        case 2:
            if (work->animFrame >= 0xB) {
                work->action     = ACTOR_105100_ACTION_IDLE;
                work->actionStep = 0;
                work->anim       = ACTOR_105100_ANIM_IDLE;
            }
            break;
    }
}

/// `ACTION_STAGGER`: step 0 starts the stagger animation and makes the same
/// cleanup as `ACTION_BUILDUP`'s; step 1 waits for frame 0x1D, then returns to
/// `ACTION_IDLE`, or to `ACTION_BUILDUP` when `buildupPending` says a buildup
/// was still being served.
static void func_actor_105100_801361C4(Task* arg0)
{
    _Actor105100Work* work;
    EffectWork*       eff;
    s32               state;

    work  = arg0->work;
    state = work->actionStep;
    switch (state) {
        case 0:
            work->anim        = ACTOR_105100_ANIM_STAGGER;
            work->actionStep  = 1;
            work->charging    = 0;
            work->summonPhase = ACTOR_105100_SUMMON_NONE;
            func_actor_105100_801362A0(arg0);
            eff                    = work->ringEffect;
            work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (eff != NULL) {
                eff->task->state = 4;
                work->ringEffect = NULL;
            }
            if (work->chargeBroken == 0) {
                work->shield.fields.cooldown = 0x1E;
            }
            break;
        case 1:
            if (work->animFrame >= 0x1D) {
                if (work->buildupPending == 0) {
                    work->action = ACTOR_105100_ACTION_IDLE;
                    work->anim   = state;
                } else {
                    work->action = ACTOR_105100_ACTION_BUILDUP;
                    work->anim   = ACTOR_105100_ANIM_BUILDUP;
                }
                work->actionStep = 0;
            }
            break;
    }
}

static void func_actor_105100_801362A0(Task* arg0)
{
    _Actor105100Work* work;
    s32               snd;

    work = arg0->work;

    snd = work->fireballSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->fireballSound = 0;
    }
    snd = work->beamSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->beamSound = 0;
    }
    snd = work->chargeSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->chargeSound = 0;
    }
}

/// `ACTION_DEFEATED`. With the player still alive it clears the charge and
/// summon state, stops the looping sounds, turns `strikeBody`'s pair tests
/// off, ends `ringEffect` and moves the task to its death state (`state` 2).
/// With the player already dead it instead leaves the enemy at 1 HP
/// (`Enemy::hp`) in `ACTION_STAGGER`, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `charging` / `summonPhase` stores sit between them), and the first pointer is
/// still live at the tail for `strikeBody.flags` and `ringEffect`, so the second one
/// needs a register of its own.
static void func_actor_105100_80136318(Task* arg0)
{
    _Actor105100Work* work;
    _Actor105100Work* sndWork;
    EffectWork*       eff;
    s32               snd;

    work = arg0->work;
    if (gPlayerStatus.hp <= 0) {
        ((Enemy*)arg0->spawnArg2.pointer)->hp = 1;
        work->action                          = ACTOR_105100_ACTION_STAGGER;
        work->actionStep                      = 0;
        return;
    }

    work->charging    = 0;
    work->summonPhase = ACTOR_105100_SUMMON_NONE;

    sndWork = arg0->work;

    snd = sndWork->fireballSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->fireballSound = 0;
    }
    snd = sndWork->beamSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->beamSound = 0;
    }
    snd = sndWork->chargeSound;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->chargeSound = 0;
    }

    eff                    = work->ringEffect;
    work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (eff != NULL) {
        eff->task->state = 4;
        work->ringEffect = NULL;
    }

    arg0->state = 2;
}

static void func_actor_105100_80136408(Task* arg0)
{
    _actor105100AnimUpdate(arg0);
}

/// Relights the actor at its model's world position: copies the model
/// coordinate's translation into a `VECTOR` and hands it with the context to
/// `Gp_UpdateActorColor`, with no blend parameters.
static void func_actor_105100_801364CC(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// The child collision task's state handlers, indexed by `Task::state`:
/// setup, per-frame reaction and teardown.
static const EnemyTaskFuncTable3 D_actor_105100_80131EB0 = {
    {
        func_actor_105100_80135278,
        func_actor_105100_801354E8,
        func_actor_105100_80136788,
    },
};

static void func_actor_105100_80136524(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x9C4, 0x80);
}

#include "../../shared/model_placement_scale.inc.c"

void func_actor_105100_8013667C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_801366D8(Enemy* arg0, Task* arg1)
{
    Actor105100ProjWork* work;

    work = arg1->work;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    enemyDestroy(arg0, arg1);
}

void func_actor_105100_8013672C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131EB0;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_80136788(Enemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->work);
    enemyDestroy(arg0, arg1);
}
