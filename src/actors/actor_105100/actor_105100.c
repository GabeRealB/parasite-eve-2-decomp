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
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
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

/// Scratch-stack block of the enemy's hit intake, which runs every tick the
/// enemy is alive.
///
/// The intake reserves one block on entry and releases it before it returns;
/// nothing carries over from one tick to the next. Only the two vectors are
/// used, and each is written before it is read: what the rest of the block
/// was laid out for is unproven.
typedef struct {
    VECTOR  toPlayer;         // Player's position minus the root's, world units; its length is the range the damage is rolled for. `pad` is never written
    byte    unknown_10[0x10]; // Reserved with the block and never accessed; role unproven
    SVECTOR flashOffset;      // Offset of the shield's deflect flash from its parent coordinate, the model's part 3: 200 along Z. `pad` is never written
    byte    unknown_28[0x8];  // Reserved with the block and never accessed; role unproven
} _Actor105100HitScratch;
STATIC_ASSERT_SIZEOF(_Actor105100HitScratch, 0x30);

/// Values of `_Actor105100FireballWork::step`, in the order a fireball goes
/// through them.
enum {
    ACTOR_105100_FIREBALL_HOVER  = 0, // drifts about where it appeared until the summon launches it; ends at once if the summon is broken off
    ACTOR_105100_FIREBALL_LAUNCH = 1, // 16 ticks tipping about its own X while it picks up speed along its Z
    ACTOR_105100_FIREBALL_AIM    = 2, // holds for 3 ticks, then turns its Z onto the player
    ACTOR_105100_FIREBALL_FLY    = 3, // flies along its Z until it touches something or 26 ticks have passed
    ACTOR_105100_FIREBALL_BURST  = 4  // 30 ticks as the burst it ends in, which can hurt for the first 10; then the task is torn down
};

/// Work block of a fireball, the task `ACTION_FIREBALLS` spawns for each one
/// it gathers.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. The
/// fireball has no model: its task carries one coordinate, which starts above
/// the parent, is moved by the per-tick handler and is where the glow is drawn
/// and both collision bodies sit. The fireball reads the parent's
/// `summonPhase` to know when to fly. Timers count ticks.
typedef struct {
    WorldCollisionBody    body;         // sphere on the coordinate carrying the fireball's attack: radius 200 in flight, re-keyed with the burst's attack at radius 500. Its pair tests are on from the flight coming under 4000 units up until 10 ticks into the burst
    WorldCollisionContact contacts[1];  // contacts of `body` and of `sweepCapsule`; a key in it ends the flight
    WorldCollisionBody    sweepBody;    // keyless capsule body on the same coordinate that the room-grid pass tests, so that the room's surfaces end the flight; its grid tests are on with `body`'s pair tests and off from the burst
    WorldCollisionCapsule sweepCapsule; // shape of `sweepBody`: the segment from the coordinate's origin to 400 down its -Z, radius 1
    SVECTOR               hoverOffset;  // sum of the random steps `ACTOR_105100_FIREBALL_HOVER` has taken on each axis; a step that would bring an axis to 500 either way is not taken. `pad` is never written
    s16                   timer;        // ticks of the current step: the launch delay counted down in the hover, 0 to 22 drawn at spawn; counted up through the launch, the aim and the flight; counted down from 30 through the burst
    s16                   step;         // `ACTOR_105100_FIREBALL_*`
    s16                   speed;        // distance moved along the coordinate's Z each tick, world units: restarted at 1 by the launch and by the flight and doubled every tick, up to 50 in the launch and 300 in the flight
    s16                   glowSize;     // size the glow is drawn at; 400 from spawn
} _Actor105100FireballWork;
STATIC_ASSERT_SIZEOF(_Actor105100FireballWork, 0x80);

/// Scratch-stack block of a fireball's per-tick handler.
///
/// The handler reserves one block for a tick in which the fireball moves and
/// releases it before it returns; each step uses only the members it needs
/// and nothing carries over to the next tick.
typedef struct {
    MATRIX  rotation; // the launch's turn for one tick, built from `operand` and multiplied onto the coordinate's rotation a column at a time; its translation is never set or read
    VECTOR  toPlayer; // player's position minus the fireball's, world units: the direction the aim turns the coordinate's Z onto. `pad` is never written
    SVECTOR operand;  // short vector being worked on: in the hover the random step for each axis in turn, in the launch the Euler angles of the turn (4096 per turn), 32 about X. `pad` is never written
} _Actor105100FireballScratch;
STATIC_ASSERT_SIZEOF(_Actor105100FireballScratch, 0x38);

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

/// Work block of a beam, the task `ACTION_BEAMS` spawns for each beam of the
/// pattern it draws.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. The
/// beam has no model: its task carries one coordinate, the free end of the
/// beam the room draws from its own anchor. That end starts beside the parent
/// and is moved each tick by the pattern's motion, carrying the sphere that
/// does the damage. A beam ends when its time is up, when its sphere touches
/// something or when the parent's summon is broken off, and takes itself off
/// the parent's `childCount` as it does.
typedef struct {
    WorldCollisionBody    body;          // sphere of radius 200 on the coordinate carrying the pattern's attack; its pair tests are on from spawn
    WorldCollisionContact contacts[1];   // contacts of `body`; a key in it ends the beam
    SVECTOR               direction;     // heading of the leg being moved along, a unit vector across X and Z (4096 for 1); the seeker pattern turns the coordinate instead and leaves it unused
    s16                   pattern;       // `ACTOR_105100_BEAMS_*` the parent had drawn when the beam spawned
    s16                   index;         // the beam's place among the beams of its pattern, 0 for the first spawned
    s16                   route;         // row of the package's route tables for this pattern and `index`: where the end starts relative to the parent, and the room position it moves to; a pair beam's second leg ends at the position three rows on
    s16                   moveStep;      // stage of the pattern's motion (0 take the heading and work out `speed`, 1 moving, 2 a pair beam's second leg); unused by the seeker
    s16                   lifeTicks;     // ticks left before the beam ends: 60 at spawn, 120 for the seeker
    s16                   firstLegTicks; // ticks left of a pair beam's first leg; its second leg starts when they run out
    s16                   speed;         // distance moved along `direction` each tick, world units: the length of the whole path over `lifeTicks` as the motion started
    u16                   colorIndex;    // entry of the room's four-step beam colour ramp the beam is drawn in: 3, the brightest, stepping down to 0 over the last 15 ticks
} _Actor105100BeamWork;
STATIC_ASSERT_SIZEOF(_Actor105100BeamWork, 0x50);

/// Task states and descriptor indices used by this package's enemy children.
enum {
    ACTOR_105100_TASK_UPDATE    = 1,
    ACTOR_105100_TASK_TEARDOWN  = 2,
    ACTOR_105100_CHILD_FIREBALL = 1,
    ACTOR_105100_CHILD_BEAM     = 2
};

/// Beam route rows, motion stages, fixed-point direction units and brightness.
enum {
    ACTOR_105100_BEAM_NO_ROUTE           = -1,
    ACTOR_105100_BEAM_ROUTES_PER_PATTERN = 3,
    ACTOR_105100_BEAM_MOVE_START         = 0,
    ACTOR_105100_BEAM_MOVE_FIRST_LEG     = 1,
    ACTOR_105100_BEAM_MOVE_SECOND_LEG    = 2,
    ACTOR_105100_DIRECTION_SHIFT         = 12,
    ACTOR_105100_BEAM_COLOR_DIM          = 0,
    ACTOR_105100_BEAM_COLOR_LOW          = 1,
    ACTOR_105100_BEAM_COLOR_MEDIUM       = 2,
    ACTOR_105100_BEAM_COLOR_BRIGHT       = 3
};

/// Shield wait after a reaction interrupts an attack without breaking a charge.
enum { ACTOR_105100_SHIELD_RECOVERY_TICKS = 30 };

static void _actor105100Task(Task* task);
static void _actor105100FireballTask(Task* task);
static void _actor105100Spawn(Enemy* enemy, Task* task);
static void func_actor_105100_80132AA0(Enemy* arg0, Task* arg1);
static void func_actor_105100_80132C2C(Task* arg0);
static void func_actor_105100_80133134(Task* arg0);
static void _actor105100Idle(Task* task, Enemy* unusedEnemy);
static void _actor105100SummonFireballs(Task* task, Enemy* unusedEnemy);
static void _actor105100SummonBeams(Task* task, Enemy* unusedEnemy);
static void func_actor_105100_80133A14(Task* arg0, Enemy* arg1);
static void func_actor_105100_80133CE4(Task* arg0);
static void _actor105100PlayAnimationSounds(Task* task);
static void func_actor_105100_80134284(Enemy* arg0, Task* arg1);
static void _actor105100FireballSpawn(Enemy* enemy, Task* task);
static void func_actor_105100_80134B00(Enemy* arg0, Task* arg1);
static void _actor105100BeamSpawn(Enemy* enemy, Task* task);
static void _actor105100BeamTick(Enemy* unusedEnemy, Task* task);
static void _actor105100BeamMovePair(Task* task);
static void _actor105100BeamMoveTriple(Task* task);
static void _actor105100BeamSeekPlayer(Task* task);
static void _actor105100UpdateStatusReactions(Task* task);
static void _actor105100RaiseShield(Task* task);
static void _actor105100ApplyPartnerHealing(Task* task);
static void _actor105100Buildup(Task* task);
static void _actor105100Stagger(Task* task);
static void _actor105100StopLoopingSounds(Task* task);
static void _actor105100Defeated(Task* task);
static void _actor105100UpdateAnimation(Task* task);
static void _actor105100RefreshColor(Task* task);
static void _actor105100DrawShadow(Task* task);
static void _actor105100FireballDestroy(Enemy* enemy, Task* task);
static void _actor105100BeamDestroy(Enemy* enemy, Task* task);
static void _actor105100BeamTask(Task* task);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Main-executable globals with no module header yet. The death handler withholds
/// message 0x13F4 while the attachment wheel is open (`Gp_StateC08.mode`) or
/// `gDisplayState.pendingMode` is live.

/// The run of HP caps at 0x8014139C; `_actor105100ApplyPartnerHealing` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `Enemy::hp` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.

/// The s16 run at 0x801414C8, one entry per animation in
/// `_Actor105100Work::anim`; `_actor105100UpdateAnimation` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `damagePackAttackKey` packs the `DamageAttack` at 0x80141380
/// into the work's third list node (`_Actor105100Work::strikeBody.key`), and the
/// `EnemyParams` at 0x80141398 is the parameter record the context points at
/// with `Enemy::param`. Its `hpMax` seeds the enemy's hit points.
extern DamageAttack D_actor_105100_80141380[6];
extern EnemyParams  D_actor_105100_80141398;

/// The animation data `animationInitContext` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// The room positions the pair and triple beams move to, indexed by
/// `_Actor105100BeamWork::route`; a pair beam's second leg ends at the entry
/// three rows on. Only the x and z halves are read: the motion subtracts the
/// beam's current position and moves along the resulting planar offset.
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
        _actor105100Spawn,
        func_actor_105100_80132AA0,
        func_actor_105100_80134284,
    },
};

static TmdSource _gActor105100StingerBody;

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

TaskDesc D_actor_105100_80141464[3] = {
    { { { TASK_BODY_TMD, 96 } }, _actor105100Task, { .model = &_gActor105100StingerBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor105100FireballTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, _actor105100BeamTask, { .value = 0 } },
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
/// so the two words `enemySpawnFromTable` splits it into are the function's
/// addend, not a load this function has to model.
extern TaskDesc D_actor_105100_80141464[];

/// The 16-entry run at 0x801413C8 the beam summon's LCG draw picks
/// `_Actor105100Work::beamPattern` from. Four entries are 0 (two children), five are 1 (three), seven are 2
/// (one).
extern u16 D_actor_105100_801413C8[16];

static inline void _actor105100AnimUpdate(Task* task);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

/// Initializes the enemy's model, animation rig and combat bodies.
///
/// The task owns the zeroed work block; its model borrows the block's lighting
/// matrices and animation storage until teardown. Hit and touch spheres start
/// enabled, while the charge's strike sphere follows the player's root and
/// stays disabled. The model starts at 2.5 scale with its shield raised.
/// Success acquires a battle hold and selects the update state; allocation
/// failure destroys the enemy and task.
static void _actor105100Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_105100_HIT_PART           = 3,
        ACTOR_105100_CONTACT_KEY        = 0x30033,
        ACTOR_105100_STRIKE_ATTACK      = 5,
        ACTOR_105100_SCALE_ONE          = 4096,
        ACTOR_105100_INITIAL_SCALE      = ACTOR_105100_SCALE_ONE * 5 / 2,
        ACTOR_105100_ENGAGE_DELAY_TICKS = 15,
        ACTOR_105100_INITIAL_IDLE_TICKS = 150,
    };

    _Actor105100Work*      work;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* touchContacts;
    WorldCollisionContact* strikeContacts;
    s32                    slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Bind model lighting and animation storage before linking combat bodies.
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[ACTOR_105100_HIT_PART];
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0x64;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_105100_80141398;
    enemy->recs                   = work->hitContacts;
    enemy->hp                     = D_actor_105100_80141398.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[ACTOR_105100_HIT_PART];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 3;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_105100_80141488, model, work->rig.poses,
                         work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, 1);
    }
    sceneAcquireBattleRef(0);
    work->placementMtx             = rootCoord->coord;
    work->scale                    = ACTOR_105100_INITIAL_SCALE;
    work->shield.fields.active     = 1;
    work->engageDelay              = ACTOR_105100_ENGAGE_DELAY_TICKS;
    work->timer                    = ACTOR_105100_INITIAL_IDLE_TICKS;
    work->hitBody.coord            = &task->extra.tmd->coords[ACTOR_105100_HIT_PART];
    hitContacts                    = work->hitContacts;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0x1F4;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = ACTOR_105100_CONTACT_KEY;
    work->hitBody.radius           = 0x320;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.flags              = work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->touchBody.coord            = task->extra.tmd->coords;
    touchContacts                    = work->touchContacts;
    work->touchBody.context.contacts = touchContacts;
    work->touchBody.pos.vx           = 0;
    work->touchBody.pos.vy           = 0;
    work->touchBody.pos.vz           = -0x12C;
    work->touchBody.key              = 0;
    work->touchBody.radius           = 0x4B0;
    work->touchBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->touchBody);
    worldCollisionInitContacts(touchContacts, ARRAY_SIZE(work->touchContacts), 0);
    work->touchBody.flags             = work->touchBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->strikeBody.coord            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    strikeContacts                    = work->strikeContacts;
    work->strikeBody.context.contacts = strikeContacts;
    work->strikeBody.pos.vx           = 0;
    work->strikeBody.pos.vy           = 0;
    work->strikeBody.pos.vz           = 0;
    work->strikeBody.key              = damagePackAttackKey(D_actor_105100_80141380, ACTOR_105100_STRIKE_ATTACK);
    work->strikeBody.radius           = 0x1F4;
    work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
    worldCollisionInitContacts(strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state            = ACTOR_105100_TASK_UPDATE;
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
            _actor105100RefreshColor(arg1);
            _actor105100DrawShadow(arg1);
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
        _actor105100UpdateStatusReactions(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->knockbackActive != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    _actor105100UpdateAnimation(arg1);
    _actor105100PlayAnimationSounds(arg1);
    _modelPlacementSetScaled(arg1, &work->placementMtx, work->scale, MODEL_PLACEMENT_SCALE_UNIFORM);
    if (work->shield.fields.active != 0) {
        shelterB6TrainingRoomSpawnShieldArcs(arg1);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _actor105100RefreshColor(arg1);
    _actor105100DrawShadow(arg1);
}

/// Per-frame hit handler: walks the three `hitContacts` records. A category-2
/// contact lands only while `hitCooldown` is clear. Damage is the player
/// distance through `Gp_ComputeDamage`, quadrupled on a successful
/// `damageRollCriticalHit`, and halved (or zeroed for 0x8000 ids) while the
/// shield is up, which also plays the deflect flash and sound. The id
/// parameter may ask for a stagger or, with the shield down, for
/// `damageStartEnemyBuildup` or `damageTryStartEnemyDamageOverTime`. The hit credits Life Drain healing
/// through `damageAccumulateLifeDrainHp`, updates the readout through
/// `worldTargetAddReadoutAmount`, and reduces HP; at 0 the actor goes to
/// `ACTION_DEFEATED`, and `staggerDamage` reaching 0x1A4 (or the stagger
/// request) sends it to `ACTION_STAGGER`, breaking a charge in progress.
/// Either ends `ringEffect`. A new id sparks `effectSpawnHit` once, and
/// `damageGetPlayerAttackHitCooldown` arms the cooldown. The tail releases the hit table, steps
/// `staggerTimer`, and starts the knockback when `touchContacts` holds a
/// player character's body.
static void func_actor_105100_80132C2C(Task* arg0)
{
    s32                     flag;
    s32                     lastId;
    _Actor105100HitScratch* scratch;
    _Actor105100Work*       work;
    Enemy*                  ctx;
    GfxCoord*               coord;
    s32                     i;
    s16                     damage;
    s32                     snd;
    s32                     wait;

    flag    = 0;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor105100HitScratch);
    lastId  = 0;
    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((u16)(work->hitContacts[i].key.value >> 16) == 2 && work->hitCooldown == 0) {
            scratch->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            scratch->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            scratch->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage               = Gp_ComputeDamage(work->hitContacts[i].key.value,
                                                    SquareRoot0(scratch->toPlayer.vx * scratch->toPlayer.vx + scratch->toPlayer.vy * scratch->toPlayer.vy +
                                                                scratch->toPlayer.vz * scratch->toPlayer.vz),
                                                    0, 0);
            if (damageRollCriticalHit(ctx, work->hitContacts[i].key.value, 0) != 0) {
                damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
            }
            if (work->shield.fields.active == 1) {
                if (work->hitContacts[i].key.value & 0x8000) {
                    damage = 0;
                } else {
                    damage /= 2;
                }
                scratch->flashOffset.vx = 0;
                scratch->flashOffset.vy = 0;
                scratch->flashOffset.vz = 0xC8;
                effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, &arg0->extra.tmd->coords[3], 0, &scratch->flashOffset);
                snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000D;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            worldTargetAddReadoutAmount(&ctx->node, damage, 0);
            if (damage != 0) {
                switch (damageGetPlayerAttackReaction(work->hitContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if ((work->hitContacts[i].key.value & 0x3F) != 0x1C) {
                            flag = 1;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->shield.fields.active == 0) {
                            damageStartEnemyBuildup(ctx, work->hitContacts[i].key.value, 0);
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        if (work->shield.fields.active == 0) {
                            damageTryStartEnemyDamageOverTime(ctx, work->hitContacts[i].key.value, 0);
                        }
                        break;
                    case 4:
                        flag = 1;
                        break;
                    case 5:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                    case 9:
                        break;
                }
                damageAccumulateLifeDrainHp(ctx, work->hitContacts[i].key.value, damage, 0);
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
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastId), &arg0->extra.tmd->coords[3], NULL,
                                   &work->hitEffectArg);
                }
                wait = damageGetPlayerAttackHitCooldown(work->hitContacts[i].key.value);
                if (wait > 0) {
                    work->hitCooldown = wait;
                }
            }
        }
    }
    worldCollisionClearContacts(work->hitContacts);
    work->staggerTimer--;
    if (work->staggerTimer <= 0) {
        work->staggerDamage = 0;
    }
    if (work->touchContacts[0].flags & 1) {
        if ((work->touchContacts[0].key.value & 0xFFFF0000) == 0x10000 && gPlayerStatus.hp > 0) {
            work->knockbackActive = 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_EVENT_LOCK;
        }
        worldCollisionClearContacts(work->touchContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor105100HitScratch);
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
            _actor105100Idle(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_FIREBALLS:
            _actor105100SummonFireballs(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_BEAMS:
            _actor105100SummonBeams(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_CHARGE:
            func_actor_105100_80133A14(arg0, ctx);
            break;
        case ACTOR_105100_ACTION_RAISE_SHIELD:
            _actor105100RaiseShield(arg0);
            break;
        case ACTOR_105100_ACTION_BUILDUP:
            _actor105100Buildup(arg0);
            break;
        case ACTOR_105100_ACTION_STAGGER:
            _actor105100Stagger(arg0);
            break;
        case ACTOR_105100_ACTION_DEFEATED:
            _actor105100Defeated(arg0);
        default:
            break;
    }
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_HEAL_READY) {
        _actor105100ApplyPartnerHealing(arg0);
    }
    if (work->shield.fields.cooldown > 0) {
        work->shield.fields.cooldown--;
    }
}

/// Waits between attacks, raises the shield and chooses the next action.
///
/// After the engage delay it starts the idle battle and selects scene music
/// entry 10. A ready shield takes priority over attack selection. Three counted
/// summons precede a charge; an interrupted charge instead schedules one
/// uncounted summon. The task must own the enemy work block; `unusedEnemy`
/// is ignored.
static void _actor105100Idle(Task* task, Enemy* unusedEnemy)
{
    enum {
        ACTOR_105100_IDLE_WAIT             = 0,
        ACTOR_105100_IDLE_SELECT_SUMMON    = 1,
        ACTOR_105100_IDLE_SELECT_CHARGE    = 2,
        ACTOR_105100_IDLE_REPLACE_CHARGE   = 3,
        ACTOR_105100_SUMMONS_BEFORE_CHARGE = 3,
        ACTOR_105100_BATTLE_MUSIC          = 10,
    };

    _Actor105100Work* work;
    s16               step;
    s16               nextAction;

    work = task->work;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        if (--work->engageDelay <= 0) {
            sceneEngageBattle(1);
            gStageSceneMusicEntry = ACTOR_105100_BATTLE_MUSIC;
        }
    }
    if (work->shield.word == 0) {
        work->action     = ACTOR_105100_ACTION_RAISE_SHIELD;
        work->actionStep = 0;
        return;
    }
    step = work->actionStep;
    switch (step) {
        case ACTOR_105100_IDLE_WAIT:
            if (--work->timer <= 0) {
                work->timer = 0;
                if (work->chargeBroken == 0) {
                    step = ACTOR_105100_IDLE_SELECT_CHARGE;
                    if (work->summonCount < ACTOR_105100_SUMMONS_BEFORE_CHARGE) {
                        step = ACTOR_105100_IDLE_SELECT_SUMMON;
                    }
                    work->actionStep = step;
                    return;
                }
                work->actionStep   = ACTOR_105100_IDLE_REPLACE_CHARGE;
                work->chargeBroken = 0;
                return;
            }
            return;
        case ACTOR_105100_IDLE_SELECT_SUMMON: {
            u16* actions     = D_actor_105100_801413A8;
            u32  randomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;

            nextAction       = (s16)actions[(randomState >> 16) & (ARRAY_SIZE(D_actor_105100_801413A8) - 1)];
            gRandomLcgState  = randomState;
            work->actionStep = 0;
            work->summonCount++;
            work->action = nextAction;
            return;
        }
        case ACTOR_105100_IDLE_SELECT_CHARGE:
            work->action      = ACTOR_105100_ACTION_CHARGE;
            work->actionStep  = 0;
            work->summonCount = 0;
            return;
        case ACTOR_105100_IDLE_REPLACE_CHARGE: {
            u16* actions     = D_actor_105100_801413A8;
            u32  randomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;

            gRandomLcgState  = randomState;
            work->action     = (s16)actions[(randomState >> 16) & (ARRAY_SIZE(D_actor_105100_801413A8) - 1)];
            work->actionStep = 0;
            break;
        }
    }
}

/// Gathers up to four fireballs and releases them toward the player.
///
/// The cast waits 88 animation ticks before spawning children at random
/// 15..30 tick intervals. The 158..189 tick summon changes from gather to
/// launch with 15 ticks left, then waits 26 cast-end ticks before returning
/// to idle. Children borrow the parent's summon phase until teardown.
/// `unusedEnemy` is ignored; the task owns the enemy work and model.
static void _actor105100SummonFireballs(Task* task, Enemy* unusedEnemy)
{
    enum {
        ACTOR_105100_FIREBALL_CAST_BEGIN      = 0,
        ACTOR_105100_FIREBALL_CAST_HOLD       = 1,
        ACTOR_105100_FIREBALL_CAST_RECOVER    = 2,
        ACTOR_105100_FIREBALL_SPAWN_FRAME     = 88,
        ACTOR_105100_FIREBALL_LIMIT           = 4,
        ACTOR_105100_FIREBALL_LAUNCH_TICKS    = 15,
        ACTOR_105100_FIREBALL_CAST_MIN_TICKS  = 158,
        ACTOR_105100_FIREBALL_SPAWN_MIN_TICKS = 15,
        ACTOR_105100_FIREBALL_CAST_END_FRAME  = 26,
        ACTOR_105100_FIREBALL_LOOP_SOUND      = 0x40330004,
    };

    _Actor105100Work* work;
    GfxCoord*         coord;
    s16               step;
    s32               audioPan;
    u32               durationRandomState;
    u32               intervalRandomState;
    u32               idleRandomState;

    work  = task->work;
    step  = work->actionStep;
    coord = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_105100_FIREBALL_CAST_BEGIN:
            work->anim          = ACTOR_105100_ANIM_CAST;
            work->fireballTimer = 0;
            work->childCount    = 0;
            work->actionStep    = ACTOR_105100_FIREBALL_CAST_HOLD;
            work->summonPhase   = ACTOR_105100_SUMMON_FIREBALLS_GATHER;
            durationRandomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState     = durationRandomState;
            work->timer         = ((durationRandomState >> 16) & 0x1F) + ACTOR_105100_FIREBALL_CAST_MIN_TICKS;
            return;
        case ACTOR_105100_FIREBALL_CAST_HOLD:
            if (work->animFrame >= ACTOR_105100_FIREBALL_SPAWN_FRAME) {
                if (--work->fireballTimer <= 0 && work->childCount < ACTOR_105100_FIREBALL_LIMIT && work->summonPhase == ACTOR_105100_SUMMON_FIREBALLS_GATHER) {
                    enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_FIREBALL, 0,
                                        task->spawnArg2.pointer);
                    work->childCount   += 1;
                    intervalRandomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = intervalRandomState;
                    work->fireballTimer = ((intervalRandomState >> 16) & 0xF) + ACTOR_105100_FIREBALL_SPAWN_MIN_TICKS;
                }
            }
            if (work->timer == ACTOR_105100_FIREBALL_LAUNCH_TICKS) {
                work->summonPhase = ACTOR_105100_SUMMON_FIREBALLS_LAUNCH;
            }
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_105100_FIREBALL_CAST_RECOVER;
                work->anim       = ACTOR_105100_ANIM_CAST_END;
            }
            if (work->animFrame == ACTOR_105100_FIREBALL_SPAWN_FRAME) {
                work->fireballSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_FIREBALL_LOOP_SOUND;
                audioPan            = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->fireballSound, audioPan,
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case ACTOR_105100_FIREBALL_CAST_RECOVER:
            if (work->animFrame >= ACTOR_105100_FIREBALL_CAST_END_FRAME) {
                work->anim        = ACTOR_105100_ANIM_IDLE;
                work->action      = ACTOR_105100_ACTION_IDLE;
                work->actionStep  = 0;
                work->summonPhase = ACTOR_105100_SUMMON_NONE;
                idleRandomState   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = idleRandomState;
                work->timer       = (idleRandomState >> 16) & 0x3F;
                sndEvtRequestScriptStop(work->fireballSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->fireballSound = 0;
            }
            break;
    }
}

/// Spawns a pair, triple or seeker beam pattern and waits for it to end.
///
/// The cast opens its ring at animation tick 30 and spawns the selected
/// pattern at tick 90. Its lifetime or the last child's completion starts
/// cast recovery; after 26 ticks the ring ends and the summon phase clears.
/// The task owns the enemy work and model; `unusedEnemy` is ignored.
static void _actor105100SummonBeams(Task* task, Enemy* unusedEnemy)
{
    enum {
        ACTOR_105100_BEAM_CAST_BEGIN       = 0,
        ACTOR_105100_BEAM_CAST_HOLD        = 1,
        ACTOR_105100_BEAM_CAST_WAIT        = 2,
        ACTOR_105100_BEAM_CAST_RECOVER     = 3,
        ACTOR_105100_BEAM_RING_FRAME       = 30,
        ACTOR_105100_BEAM_SPAWN_FRAME      = 90,
        ACTOR_105100_BEAM_RING_TICKS       = 60,
        ACTOR_105100_BEAM_CAST_END_FRAME   = 26,
        ACTOR_105100_SUMMON_RING_END_STATE = 4,
        ACTOR_105100_BEAM_RING_SOUND       = 0x40330007,
        ACTOR_105100_BEAM_LOOP_SOUND       = 0x40330008,
    };

    _Actor105100Work* work;
    GfxCoord*         coord;
    SVECTOR           ringOffset;
    s16               step;
    s32               ringAudioPan;
    s32               beamAudioPan;
    s32               ringSound;

    work  = task->work;
    step  = work->actionStep;
    coord = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_105100_BEAM_CAST_BEGIN: {
            u16* patterns;
            u32  randomState;
            s16  pattern;

            work->anim        = ACTOR_105100_ANIM_CAST;
            work->actionStep  = ACTOR_105100_BEAM_CAST_HOLD;
            patterns          = D_actor_105100_801413C8;
            randomState       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            pattern           = patterns[(randomState >> 16) & (ARRAY_SIZE(D_actor_105100_801413C8) - 1)];
            gRandomLcgState   = randomState;
            work->childCount  = 0;
            work->summonPhase = ACTOR_105100_SUMMON_BEAMS;
            work->beamPattern = pattern;
            return;
        }
        case ACTOR_105100_BEAM_CAST_HOLD:
            if (work->animFrame == ACTOR_105100_BEAM_RING_FRAME) {
                ringOffset.vx    = 0;
                ringOffset.vy    = -0x6D6;
                ringOffset.vz    = 0x320;
                work->ringEffect = effectSpawn((EFFECT_SHELTER_B6_TRAINING_SUMMON_RING | EFFECT_SPAWN_UNLIMITED), task->extra.tmd->coords, ACTOR_105100_BEAM_RING_TICKS, &ringOffset);
                ringSound        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_BEAM_RING_SOUND;
                ringAudioPan     = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(ringSound, ringAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= ACTOR_105100_BEAM_SPAWN_FRAME) {
                switch (work->beamPattern) {
                    case ACTOR_105100_BEAMS_PAIR:
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        break;
                    case ACTOR_105100_BEAMS_TRIPLE:
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        break;
                    case ACTOR_105100_BEAMS_SEEKER:
                        enemySpawnFromTable(D_actor_105100_80141464, ACTOR_105100_CHILD_BEAM, 0,
                                            task->spawnArg2.pointer);
                        break;
                }
                work->timer      = D_actor_105100_80141448[work->beamPattern];
                work->actionStep = ACTOR_105100_BEAM_CAST_WAIT;
                work->beamSound  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_BEAM_LOOP_SOUND;
                beamAudioPan     = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->beamSound, beamAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case ACTOR_105100_BEAM_CAST_WAIT:
            if (--work->timer <= 0 || work->childCount == 0) {
                work->actionStep = ACTOR_105100_BEAM_CAST_RECOVER;
                work->anim       = ACTOR_105100_ANIM_CAST_END;
                sndEvtRequestScriptStop(work->beamSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->beamSound = 0;
            }
            break;
        case ACTOR_105100_BEAM_CAST_RECOVER: {
            u32         randomState;
            EffectWork* ringEffect;

            if (work->animFrame >= ACTOR_105100_BEAM_CAST_END_FRAME) {
                work->anim       = ACTOR_105100_ANIM_IDLE;
                work->action     = ACTOR_105100_ACTION_IDLE;
                work->actionStep = 0;
                randomState      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomState;
                ringEffect       = work->ringEffect;
                work->timer      = (randomState >> 16) & 0x3F;
                if (ringEffect != NULL) {
                    ringEffect->task->state = ACTOR_105100_SUMMON_RING_END_STATE;
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
            work->ringEffect             = effectSpawn((EFFECT_SHELTER_B6_TRAINING_CHARGE_RING | EFFECT_SPAWN_UNLIMITED), arg0->extra.tmd->coords, work->timer + 0xA, &pos);
            work->chargeSound            = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330009;
            sndEvtRequestScriptStart(work->chargeSound, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            break;
        case 1:
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->anim       = ACTOR_105100_ANIM_STRIKE;
                sndEvtRequestScriptStop(work->chargeSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->chargeSound = 0;
                snd               = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000B;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
                padScriptSpawnVariableMotorRamp(0xF, 8, 0xFF);
            }
            if (work->timer == 0x5A) {
                snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000A;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            }
            break;
        case 2:
            if (work->animFrame == 0xC) {
                work->ringEffect        = NULL;
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                padScriptSpawnVariableMotorRamp(0xF, 0xFF, 0x80);
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
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                scratch->pushDirection.vx = 0;
                scratch->pushDirection.vy = -1000;
                scratch->pushDirection.vz = 0;
                effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, player->extra.tmd->coords, 0, &scratch->pushDirection);
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
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
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

/// Plays spatial sounds when the animated part's cue bits turn off.
///
/// Requires a live enemy, model and animation rig. Slot 1 supplies the cues;
/// a missing record leaves the previous cue mask intact. The placement index
/// supplies the sound instance tag. Pan and depth narrow to signed bytes.
static void _actor105100PlayAnimationSounds(Task* task)
{
    enum {
        ACTOR_105100_CUE_2_END_SOUND = 0x40330001,
        ACTOR_105100_CUE_1_END_SOUND = 0x40330002,
    };

    s32                    soundId;
    s32                    cue2Pan;
    s32                    cue1Pan;
    _Actor105100Work*      work;
    GfxCoord*              coord;
    Enemy*                 enemy;
    const AnimationRecord* rec;

    work  = task->work;
    coord = task->extra.tmd->coords;
    rec   = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->animCues & ANIMATION_RECORD_CUE_2)) {
            enemy   = task->spawnArg2.pointer;
            soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_CUE_2_END_SOUND;
            cue2Pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundId, cue2Pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->animCues & ANIMATION_RECORD_CUE_1)) {
            enemy   = task->spawnArg2.pointer;
            soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_CUE_1_END_SOUND;
            cue1Pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundId, cue1Pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->animCues = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

/// Applies animation requests or advances the enemy's nineteen-slot rig.
///
/// Slots 1..18 animate the model parts; slot 0 remains the placed root.
/// A changed request restarts those slots at frame zero with the animation's
/// blend duration in whole normal-rate frames and clears the tick counter. Otherwise each slot and the
/// counter advance once. The task must own a live `_Actor105100Work` and model.
static inline void _actor105100AnimUpdate(Task* task)
{
    _Actor105100Work* work;
    s32               slotIndex;
    s32               blendFrames;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        blendFrames       = D_actor_105100_801414C8[work->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Updates the enemy's model lighting and colour at its composed root position.
///
/// Requires a live enemy in `spawnArg2.pointer` and a composed model root.
/// Only the three world-coordinate components are sampled; colour modes and
/// blends are applied by the world-lighting service.
static inline void _actor105100UpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR3   worldPosition;

    coord            = task->extra.tmd->coords;
    worldPosition.vx = coord->workm.t[0];
    worldPosition.vy = coord->workm.t[1];
    worldPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
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
    Task*             actor;
    TmdObject*        obj;
    _Actor105100Work* work;
    GfxCoord*         coord;
    Task*             player;
    s32               snd;
    s16               flag;

    actor  = arg1;
    obj    = actor->extra.tmd;
    work   = actor->work;
    coord  = obj->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (gSceneCombatState.actorControl) {
        case 1:
            _actor105100UpdateColor(actor);
            return;
        case 2:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
        default:
            break;
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
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->touchBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            work->timer      = 0;
            work->actionStep = 1;
            _actor105100UpdateColor(actor);
            return;
        case 1:
            if (++work->timer == 0xA) {
                obj->flags = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer >= 0x1F) {
                work->anim       = ACTOR_105100_ANIM_DIE;
                work->actionStep = 2;
                sceneReleaseBattleRefWithRewards(actor, 0x33);
                work->deathEventPending = 1;
            }
            _actor105100AnimUpdate(actor);
            _actor105100UpdateColor(actor);
            return;
        case 2:
            flag = work->deathEventPending;
            if ((flag == 1) && (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) && (Gp_StateC08.mode != flag) &&
                (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
                work->deathEventPending = 0;
            }
            if (work->animFrame == 0xB) {
                snd = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4033000E;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x40);
            }
            if (work->animFrame == 0x28) {
                snd = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330003;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                padScriptSpawnVariableMotorRamp(0xF, 0xFF, 0x80);
            }
            if (work->animFrame == 0x36) {
                dir.vx = 0;
                dir.vy = 0;
                dir.vz = 0x96;
                effectSpawn(EFFECT_CORPSE_BURN, &actor->extra.tmd->coords[3], 5, &dir);
                work->timer = 0;
            }
            if ((work->animFrame >= 0x36) && (work->deathEventPending == 0)) {
                work->actionStep = 3;
            }
            _actor105100AnimUpdate(actor);
            _actor105100UpdateColor(actor);
            return;
        case 3:
            if (work->scale >= 0x201) {
                work->scale -= 0x50;
            }
            _modelPlacementSetScaled(actor, &work->placementMtx, work->scale, MODEL_PLACEMENT_SCALE_Y_ONLY);
            if (++work->timer >= 0x3C) {
                work->actionStep = 4;
            }
            _actor105100UpdateColor(actor);
            return;
        case 4:
            enemyDestroy(arg0, actor);
            return;
    }
}

/// Initializes a fireball's coordinate, glow and disabled collision bodies.
///
/// The task has a single-coordinate body and a live enemy/model parent.
/// The parent's fireball count selects a start offset, jittered independently
/// by up to 127 world units on each axis. A key-bearing sphere and keyless
/// room-sweep capsule share one contact record. The task owns the zeroed
/// work block; allocation failure destroys the enemy and task.
static void _actor105100FireballSpawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_105100_FIREBALL_ATTACK = 0,
    };

    Task*                     parent;
    _Actor105100Work*         parentWork;
    _Actor105100FireballWork* work;
    GfxCoord*                 coord;
    GfxCoord*                 parentCoord;
    long*                     translationY;
    void*                     startOffsetOrSweepCoord;
    s32                       childIndexOrFlags;
    s32                       offsetY;
    u32                       randomState;
    u32                       jitterRollX;
    u32                       jitterRollY;
    u32                       jitterRollZ;
    u32                       delayRollA;
    u32                       delayRollB;
    s32                       jitterX;
    s32                       jitterY;
    s32                       jitterZ;
    s32                       addJitterX;
    s32                       addJitterY;
    s32                       addJitterZ;
    s32                       positionX;
    s32                       positionY;
    s32                       positionZ;

    parent      = task->parent;
    coord       = task->extra.coordBody->coord;
    parentCoord = parent->extra.tmd->coords;
    parentWork  = parent->work;
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->work              = work;
    coord->parent           = &gGfxViewCoord;
    coord->coord            = parentCoord->coord;
    coord->coord.t[0]       = parentCoord->coord.t[0] + D_actor_105100_801414E0[parentWork->childCount].vx;
    offsetY                 = D_actor_105100_801414E0[parentWork->childCount].vy;
    randomState             = gRandomLcgState;
    translationY            = &coord->coord.t[1];
    *translationY           = parentCoord->coord.t[1] + offsetY;
    jitterRollX             = (gRandomLcgState = randomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    childIndexOrFlags       = parentWork->childCount;
    startOffsetOrSweepCoord = &D_actor_105100_801414E0[childIndexOrFlags];
    coord->coord.t[2]       = parentCoord->coord.t[2] + (jitterX = ((SVECTOR*)startOffsetOrSweepCoord)->vz);
    jitterX                 = jitterRollX & 0x7F;
    addJitterX              = jitterRollX & 0x80;
    positionX               = coord->coord.t[0];
    coord->coord.t[0]       = !addJitterX ? positionX - jitterX : positionX + jitterX;

    jitterRollY       = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    jitterY           = jitterRollY & 0x7F;
    addJitterY        = jitterRollY & 0x80;
    positionY         = coord->coord.t[1];
    coord->coord.t[1] = !addJitterY ? positionY - jitterY : positionY + jitterY;

    jitterRollZ       = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
    jitterZ           = jitterRollZ & 0x7F;
    addJitterZ        = jitterRollZ & 0x80;
    positionZ         = coord->coord.t[2];
    coord->coord.t[2] = !addJitterZ ? positionZ - jitterZ : positionZ + jitterZ;

    // Both shapes start disarmed; the flight handler enables their tests.
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->body.coord            = task->extra.coordBody->coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = damagePackAttackKey(D_actor_105100_80141380, ACTOR_105100_FIREBALL_ATTACK);
    work->body.radius           = 0xC8;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    work->sweepCapsule.ends[0].vx   = 0;
    work->sweepCapsule.ends[0].vy   = 0;
    work->sweepCapsule.ends[0].vz   = 0;
    work->sweepCapsule.ends[1].vx   = 0;
    work->sweepCapsule.ends[1].vy   = 0;
    work->sweepCapsule.ends[1].vz   = -0x190;
    work->sweepCapsule.end0Radius   = 1;
    work->sweepCapsule.end1Radius   = 1;
    work->sweepCapsule.contacts     = work->contacts;
    childIndexOrFlags               = work->body.flags;
    childIndexOrFlags              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->body.flags                = childIndexOrFlags;
    startOffsetOrSweepCoord         = task->extra.coordBody->coord;
    work->sweepBody.context.capsule = &work->sweepCapsule;
    work->sweepBody.pos.vx          = 0;
    work->sweepBody.pos.vy          = 0;
    work->sweepBody.pos.vz          = 0;
    work->sweepBody.key             = 0;
    work->sweepBody.radius          = 0;
    work->sweepBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->sweepBody.coord           = startOffsetOrSweepCoord;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sweepBody);
    work->sweepBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->glowSize  = 0x190;
    delayRollA      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    delayRollB      = (delayRollA * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = delayRollB;
    work->timer     = ((delayRollA >> 16) & 0xF) + ((delayRollB >> 16) & 7);
    task->state     = ACTOR_105100_TASK_UPDATE;
}

/// The projectile task's state handlers, indexed by `Task::state`: setup,
/// per-frame flight and teardown.
static const EnemyTaskFuncTable3 D_actor_105100_80131E90 = {
    {
        _actor105100FireballSpawn,
        func_actor_105100_80134B00,
        _actor105100FireballDestroy,
    },
};

/// Per-frame handler of the glowing projectile this overlay spawns as its
/// second enemy task, the middle entry of `D_actor_105100_80131E90`. Mode 1 of
/// `gSceneCombatState.actorControl` only redraws the billboard and mode 2 skips the frame.
///
/// `_Actor105100FireballWork::step` takes the fireball through its life. In
/// the hover it drifts by a random step on each axis, held near where it
/// appeared, and watches the parent's `summonPhase`: with the summon broken
/// off the task ends, and once the summon launches its fireballs each counts
/// down its own delay. The launch tips the fireball a little further every
/// tick while it picks up speed, the aim turns it onto the player, and the
/// flight carries it straight on, with its two collision bodies armed once it
/// is under 4000 units up. A contact or the end of the flight's time turns it
/// into the burst: the sphere is re-keyed and widened, the burst's effect and
/// sound are started, and the task ends when the burst's time is up.
static void func_actor_105100_80134B00(Enemy* arg0, Task* arg1)
{
    _Actor105100FireballWork*    work;
    _Actor105100Work*            parentWork;
    GfxCoord*                    coord;
    _Actor105100FireballScratch* scratch;
    u32                          rng;
    u32                          hi;
    s32                          val;
    s32                          snd;

    work       = arg1->work;
    coord      = arg1->extra.tmd->coords;
    parentWork = (arg1->parent)->work;
    switch (gSceneCombatState.actorControl) {
        case 1:
            fireballDrawGlow(coord, work->glowSize);
            return;
        case 2:
            return;
        case 0:
        default:
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(_Actor105100FireballScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor105100FireballScratch);
    switch (work->step) {
        case ACTOR_105100_FIREBALL_HOVER:
            // Take a step of up to 63 units either way on each axis, unless it
            // would carry the fireball 500 units from where it appeared.
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->operand.vx = val;
            if (ABS(work->hoverOffset.vx + (s16)val) < 0x1F4) {
                work->hoverOffset.vx += val;
                coord->coord.t[0]    += scratch->operand.vx;
            }
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->operand.vy = val;
            if (ABS(work->hoverOffset.vy + (s16)val) < 0x1F4) {
                work->hoverOffset.vy += val;
                coord->coord.t[1]    += scratch->operand.vy;
            }
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi              = rng >> 16;
            val             = hi & 0x3F;
            gRandomLcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->operand.vz = val;
            if (ABS(work->hoverOffset.vz + (s16)val) < 0x1F4) {
                work->hoverOffset.vz += val;
                coord->coord.t[2]    += scratch->operand.vz;
            }
            if (parentWork->summonPhase == ACTOR_105100_SUMMON_NONE) {
                arg1->state = 2;
            }
            if (parentWork->summonPhase == ACTOR_105100_SUMMON_FIREBALLS_LAUNCH) {
                if (--work->timer <= 0) {
                    work->step  = ACTOR_105100_FIREBALL_LAUNCH;
                    work->speed = 1;
                    work->timer = 0;
                }
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, work->glowSize);
            break;
        case ACTOR_105100_FIREBALL_LAUNCH:
            // Turn the coordinate about its own X: its rotation times the
            // tick's turn, a column at a time.
            scratch->operand.vx = 0x20;
            scratch->operand.vy = 0;
            scratch->operand.vz = 0;
            RotMatrix(&scratch->operand, &scratch->rotation);
            gte_SetRotMatrix(&coord->coord);
            gte_ldclmv(&scratch->rotation);
            gte_rtir();
            gte_stclmv(&coord->coord);
            gte_ldclmv(&scratch->rotation.m[0][1]);
            gte_rtir();
            gte_stclmv(&coord->coord.m[0][1]);
            gte_ldclmv(&scratch->rotation.m[0][2]);
            gte_rtir();
            gte_stclmv(&coord->coord.m[0][2]);
            work->speed += work->speed;
            if (work->speed >= 0x33) {
                work->speed = 0x32;
            }
            coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> 12;
            coord->coord.t[1] += (coord->coord.m[1][2] * work->speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> 12;
            if (++work->timer >= 0x10) {
                work->step  = ACTOR_105100_FIREBALL_AIM;
                work->timer = 0;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, work->glowSize);
            break;
        case ACTOR_105100_FIREBALL_AIM:
            if (++work->timer >= 3) {
                scratch->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                scratch->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                scratch->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                gfxBuildDirectionRotation(&scratch->toPlayer, &coord->coord, 0);
                work->step  = ACTOR_105100_FIREBALL_FLY;
                work->timer = 0;
                work->speed = 1;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, work->glowSize);
            break;
        case ACTOR_105100_FIREBALL_FLY:
            if (++work->timer == 4) {
                snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330005;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (coord->coord.t[1] >= -0xF9F) {
                work->body.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->sweepBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            work->speed += work->speed;
            if (work->speed >= 0x12D) {
                work->speed = 0x12C;
            }
            coord->coord.t[0]  += (coord->coord.m[0][2] * work->speed) >> 12;
            coord->coord.t[1]  += (coord->coord.m[1][2] * work->speed) >> 12;
            coord->coord.t[2]  += (coord->coord.m[2][2] * work->speed) >> 12;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, work->glowSize);
            if (work->contacts[0].key.value != 0 || work->timer >= 0x1A) {
                // Become the burst: stop sweeping the room and carry the burst's attack.
                work->sweepBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                worldCollisionClearContacts(work->contacts);
                work->body.key    = damagePackAttackKey(D_actor_105100_80141380, 1);
                work->body.radius = 0x1F4;
                work->timer       = 0x1E;
                work->step        = ACTOR_105100_FIREBALL_BURST;
                effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_ORANGE_BURST, coord, 0, NULL);
                snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40330006;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case ACTOR_105100_FIREBALL_BURST:
            if (work->timer == 0x14) {
                work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (--work->timer <= 0) {
                arg1->state = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor105100FireballScratch);
}

/// Initializes one beam endpoint and registers it with the parent's summon.
///
/// Requires a single-coordinate task body and a live model parent whose
/// pattern is pair, triple or seeker. The parent's live child count selects
/// the next route; a missing route or allocation failure destroys this child.
/// Success increments that count, copies the pattern and route, arms the
/// endpoint's attack sphere and starts its 60- or 120-tick lifetime.
static void _actor105100BeamSpawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_105100_BEAM_FIRST_ATTACK = 2,
    };

    Task*                 parent;
    _Actor105100Work*     parentWork;
    _Actor105100BeamWork* beam;
    GfxCoord*             coord;
    GfxCoord*             parentCoord;

    parent      = task->parent;
    parentWork  = parent->work;
    coord       = task->extra.coordBody->coord;
    parentCoord = parent->extra.tmd->coords;

    // Missing routes reject the child before it changes the live beam count.
    if (D_actor_105100_80141450[parentWork->beamPattern * ACTOR_105100_BEAM_ROUTES_PER_PATTERN + parentWork->childCount] == ACTOR_105100_BEAM_NO_ROUTE) {
        enemyDestroy(enemy, task);
        return;
    }

    beam = memCalloc(sizeof(*beam), false);
    if (beam == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->work    = beam;
    beam->pattern = parentWork->beamPattern;
    beam->index   = parentWork->childCount;
    parentWork->childCount++;
    beam->route                 = D_actor_105100_80141450[parentWork->beamPattern * ACTOR_105100_BEAM_ROUTES_PER_PATTERN + beam->index];
    beam->lifeTicks             = D_actor_105100_80141448[beam->pattern];
    beam->colorIndex            = ACTOR_105100_BEAM_COLOR_BRIGHT;
    coord->parent               = &gGfxViewCoord;
    coord->coord                = parentCoord->coord;
    coord->coord.t[0]           = parentCoord->coord.t[0] + D_actor_105100_801413E8[beam->route].vx;
    coord->coord.t[1]           = parentCoord->coord.t[1] + D_actor_105100_801413E8[beam->route].vy;
    coord->coord.t[2]           = parentCoord->coord.t[2] + D_actor_105100_801413E8[beam->route].vz;
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    beam->body.coord            = task->extra.coordBody->coord;
    beam->body.context.contacts = beam->contacts;
    beam->body.pos.vx           = 0;
    beam->body.pos.vy           = 0;
    beam->body.pos.vz           = 0;
    beam->body.key              = damagePackAttackKey(D_actor_105100_80141380, beam->pattern + ACTOR_105100_BEAM_FIRST_ATTACK);
    beam->body.radius           = 0xC8;
    beam->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &beam->body);
    worldCollisionInitContacts(beam->contacts, ARRAY_SIZE(beam->contacts), 0);
    beam->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state       = ACTOR_105100_TASK_UPDATE;
}

/// Moves and draws a summoned beam endpoint until its attack ends.
///
/// Paused actors redraw without moving or aging; hidden actors do neither.
/// Running actors move by the selected pattern, fade over the last 15 ticks,
/// and retire on lifetime expiry, a contact key or summon cancellation.
/// Retirement decrements the parent's live beam count exactly once and
/// selects teardown. `unusedEnemy` is retained for `EnemyTaskFunc`.
static void _actor105100BeamTick(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_105100_BEAM_RADIUS_SCALE    = 128,
        ACTOR_105100_BEAM_FADE_STEP_TICKS = 5,
    };

    _Actor105100BeamWork* beam;
    _Actor105100Work*     parentWork;
    GfxCoord*             coord;

    beam       = task->work;
    parentWork = (task->parent)->work;
    coord      = task->extra.coordBody->coord;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            shelterB6TrainingRoomDrawSummonBeam(coord, ACTOR_105100_BEAM_RADIUS_SCALE, beam->colorIndex);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (beam->pattern) {
        case ACTOR_105100_BEAMS_PAIR:
            _actor105100BeamMovePair(task);
            break;
        case ACTOR_105100_BEAMS_TRIPLE:
            _actor105100BeamMoveTriple(task);
            break;
        case ACTOR_105100_BEAMS_SEEKER:
            _actor105100BeamSeekPlayer(task);
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    // Dim the beam through the last 15 ticks of its life.
    if (beam->lifeTicks < ACTOR_105100_BEAM_FADE_STEP_TICKS + 1) {
        beam->colorIndex = ACTOR_105100_BEAM_COLOR_DIM;
    } else if (beam->lifeTicks < ACTOR_105100_BEAM_FADE_STEP_TICKS * 2 + 1) {
        beam->colorIndex = ACTOR_105100_BEAM_COLOR_LOW;
    } else if (beam->lifeTicks < ACTOR_105100_BEAM_FADE_STEP_TICKS * 3 + 1) {
        beam->colorIndex = ACTOR_105100_BEAM_COLOR_MEDIUM;
    }
    shelterB6TrainingRoomDrawSummonBeam(coord, ACTOR_105100_BEAM_RADIUS_SCALE, beam->colorIndex);
    if (--beam->lifeTicks <= 0 || beam->contacts[0].key.value != 0 ||
        parentWork->summonPhase == ACTOR_105100_SUMMON_NONE) {
        parentWork->childCount = parentWork->childCount - 1;
        task->state            = ACTOR_105100_TASK_TEARDOWN;
    }
}

/// Advances a beam endpoint by its signed speed along its Q12 planar heading.
///
/// Translation is in world units per tick, with an arithmetic shift after each
/// signed product. Only local X and Z change. The caller invalidates and
/// composes the coordinate after moving it; neither pointer is retained.
static inline void _actor105100MoveBeamEndpoint(GfxCoord* coord, const _Actor105100BeamWork* beam)
{
    coord->coord.t[0] += (beam->direction.vx * beam->speed) >> ACTOR_105100_DIRECTION_SHIFT;
    coord->coord.t[2] += (beam->direction.vz * beam->speed) >> ACTOR_105100_DIRECTION_SHIFT;
}

/// Moves a pair beam through two successive room waypoints.
///
/// Routes 0 and 1 use their own waypoint, then the waypoint three rows later.
/// The initial total planar length divided by the positive remaining lifetime
/// sets the signed 16-bit speed in world units per tick. The first leg's
/// integer tick count switches direction; neither leg snaps to its waypoint.
/// Squared distances must fit signed 32 bits and the narrowed speed must be
/// nonzero. Reserves one scratch `VECTOR` and releases it before returning.
static void _actor105100BeamMovePair(Task* task)
{
    _Actor105100BeamWork* beam;
    GfxCoord*             coord;
    VECTOR*               toWaypoint;
    s16                   moveStep;
    s32                   firstLegX;
    s32                   firstLegZ;
    s32                   secondLegX;
    s32                   secondLegZ;
    s32                   firstLegLength;
    s16                   speed;

    toWaypoint = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    beam       = task->work;
    moveStep   = beam->moveStep;
    coord      = task->extra.coordBody->coord;
    switch (moveStep) {
        case ACTOR_105100_BEAM_MOVE_START:
            // Divide the complete two-leg distance across the remaining lifetime.
            toWaypoint->vx = D_actor_105100_80141418[beam->route].vx - coord->coord.t[0];
            toWaypoint->vy = 0;
            toWaypoint->vz = D_actor_105100_80141418[beam->route].vz - coord->coord.t[2];
            VectorNormalS(toWaypoint, &beam->direction);
            firstLegX      = toWaypoint->vx;
            firstLegZ      = toWaypoint->vz;
            firstLegLength = SquareRoot0(firstLegX * firstLegX + firstLegZ * firstLegZ);
            toWaypoint->vx = D_actor_105100_80141418[beam->route + ACTOR_105100_BEAM_ROUTES_PER_PATTERN].vx -
                             D_actor_105100_80141418[beam->route].vx;
            toWaypoint->vy = 0;
            secondLegZ     = D_actor_105100_80141418[beam->route + ACTOR_105100_BEAM_ROUTES_PER_PATTERN].vz -
                         D_actor_105100_80141418[beam->route].vz;
            toWaypoint->vz      = secondLegZ;
            secondLegX          = toWaypoint->vx;
            speed               = (firstLegLength + SquareRoot0(secondLegX * secondLegX + secondLegZ * secondLegZ)) / beam->lifeTicks;
            beam->moveStep      = ACTOR_105100_BEAM_MOVE_FIRST_LEG;
            beam->speed         = speed;
            beam->firstLegTicks = firstLegLength / speed;
            break;
        case ACTOR_105100_BEAM_MOVE_FIRST_LEG:
            _actor105100MoveBeamEndpoint(coord, beam);
            if (--beam->firstLegTicks <= 0) {
                toWaypoint->vx = D_actor_105100_80141418[beam->route + ACTOR_105100_BEAM_ROUTES_PER_PATTERN].vx - coord->coord.t[0];
                toWaypoint->vy = 0;
                toWaypoint->vz = D_actor_105100_80141418[beam->route + ACTOR_105100_BEAM_ROUTES_PER_PATTERN].vz - coord->coord.t[2];
                VectorNormalS(toWaypoint, &beam->direction);
                beam->moveStep = ACTOR_105100_BEAM_MOVE_SECOND_LEG;
            }
            break;
        case ACTOR_105100_BEAM_MOVE_SECOND_LEG:
            _actor105100MoveBeamEndpoint(coord, beam);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Moves a triple beam along a fixed heading toward its room waypoint.
///
/// Routes 2..4 select the waypoint. Initialization normalizes its planar
/// offset with 12 fractional bits and divides length by the positive remaining
/// lifetime for signed 16-bit world units per tick. Later ticks translate
/// along that heading without snapping to the waypoint. Reserves and releases
/// one scratch `VECTOR`.
static void _actor105100BeamMoveTriple(Task* task)
{
    _Actor105100BeamWork* beam;
    GfxCoord*             coord;
    VECTOR*               toWaypoint;
    s16                   moveStep;
    s32                   offsetX;
    s32                   offsetZ;

    toWaypoint = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    beam       = task->work;
    moveStep   = beam->moveStep;
    coord      = task->extra.coordBody->coord;
    switch (moveStep) {
        case ACTOR_105100_BEAM_MOVE_START:
            toWaypoint->vx = D_actor_105100_80141418[beam->route].vx - coord->coord.t[0];
            toWaypoint->vy = 0;
            toWaypoint->vz = D_actor_105100_80141418[beam->route].vz - coord->coord.t[2];
            VectorNormalS(toWaypoint, &beam->direction);
            offsetX        = toWaypoint->vx;
            offsetZ        = toWaypoint->vz;
            beam->speed    = SquareRoot0(offsetX * offsetX + offsetZ * offsetZ) / beam->lifeTicks;
            beam->moveStep = ACTOR_105100_BEAM_MOVE_FIRST_LEG;
            break;
        case ACTOR_105100_BEAM_MOVE_FIRST_LEG:
            _actor105100MoveBeamEndpoint(coord, beam);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Turns a seeker beam toward the player and advances its endpoint.
///
/// Headings use 4096 units per turn. The turn follows the shorter arc, limited
/// to 80 angle units per tick, and translation advances 60 world units along
/// the resulting local Z axis. Player deltas narrow to signed 16 bits before
/// heading calculation. Reserves and releases one `ActorFaceScratch` block;
/// the caller invalidates and composes the updated coordinate.
static void _actor105100BeamSeekPlayer(Task* task)
{
    enum {
        ACTOR_105100_ANGLE_TURN                 = 4096,
        ACTOR_105100_ANGLE_MASK                 = 4095,
        ACTOR_105100_SEEK_TURN_STEP             = 80,
        ACTOR_105100_SEEK_TRANSLATION_SHIFT     = 10,
        ACTOR_105100_SEEK_TRANSLATION_NUMERATOR = 15,
    };

    ActorFaceScratch* scratch;
    GfxCoord*         coord;
    s32               playerHeading;
    s32               currentHeading;
    s16               nextHeading;
    s16               headingDelta;
    s32               headingDistance;
    s16               snapHeading;
    s32               directHeading;
    s32               wrappedHeading;

    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord             = task->extra.coordBody->coord;
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy = 0;
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    playerHeading     = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_105100_ANGLE_MASK;
    snapHeading       = playerHeading;
    currentHeading    = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_105100_ANGLE_MASK;
    nextHeading       = currentHeading;
    headingDelta      = playerHeading - currentHeading;
    headingDistance   = headingDelta >= 0 ? headingDelta : -headingDelta;
    // Follow the shorter angular arc, including the wrap through zero.
    if (headingDistance < (ACTOR_105100_ANGLE_TURN / 2)) {
        nextHeading = playerHeading;
        if (headingDistance >= (ACTOR_105100_SEEK_TURN_STEP + 1)) {
            directHeading = (s16)currentHeading;
            if (headingDelta > 0) {
                nextHeading = directHeading + ACTOR_105100_SEEK_TURN_STEP;
            } else {
                nextHeading = directHeading - ACTOR_105100_SEEK_TURN_STEP;
            }
        }
    } else if (headingDelta > 0 ? ACTOR_105100_ANGLE_TURN - headingDelta < (ACTOR_105100_SEEK_TURN_STEP + 1) : ACTOR_105100_ANGLE_TURN + headingDelta < (ACTOR_105100_SEEK_TURN_STEP + 1)) {
        nextHeading = snapHeading;
    } else {
        wrappedHeading = (s16)nextHeading;
        if (headingDelta > 0) {
            nextHeading = wrappedHeading - ACTOR_105100_SEEK_TURN_STEP;
        } else {
            nextHeading = wrappedHeading + ACTOR_105100_SEEK_TURN_STEP;
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = nextHeading;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &coord->coord);
    coord->coord.t[0] += (coord->coord.m[0][2] * ACTOR_105100_SEEK_TRANSLATION_NUMERATOR) >> ACTOR_105100_SEEK_TRANSLATION_SHIFT;
    coord->coord.t[2] += (coord->coord.m[2][2] * ACTOR_105100_SEEK_TRANSLATION_NUMERATOR) >> ACTOR_105100_SEEK_TRANSLATION_SHIFT;
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

#include "../../shared/fireball_ember.inc.c"

/// Dispatches the enemy's spawn, per-frame update or teardown handler.
///
/// `task->state` must be 0..2; `spawnArg2.pointer` holds its live enemy.
/// The descriptor supplies the nineteen-part model. A teardown handler may
/// destroy the enemy, model, work block and task before returning.
static void _actor105100Task(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_105100_80131E24;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Consumes reaction flags and applies quarter-strength damage-over-time pulses.
///
/// Requires the live enemy and its work block. A buildup request selects the
/// buildup action; its counters advance in that action. Each nonzero signed
/// low-halfword pulse selects stagger or defeat from the resulting signed
/// low-halfword HP, even when the reduced damage is zero. Expiry clears the
/// damage-over-time flags. A pending stagger flag is only acknowledged here.
static void _actor105100UpdateStatusReactions(Task* task)
{
    enum { ACTOR_105100_STATUS_DAMAGE_SHIFT = 2 };

    Enemy*            enemy;
    _Actor105100Work* work;
    s32               remainingHp;
    s32               appliedDamage;
    s32               pulseHighHalf;
    u8                reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->action != ACTOR_105100_ACTION_BUILDUP)) {
        work->action         = ACTOR_105100_ACTION_BUILDUP;
        work->actionStep     = 0;
        work->buildupPending = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        // Sign-extend the pulse's low halfword while dividing it by four.
        pulseHighHalf = damageTickEnemyDamageOverTime(enemy) << 16;
        if (pulseHighHalf != 0) {
            appliedDamage = pulseHighHalf >> (16 + ACTOR_105100_STATUS_DAMAGE_SHIFT);
            worldTargetAddReadoutAmount(&enemy->node, appliedDamage, 0);
            remainingHp      = (u16)enemy->hp - appliedDamage;
            enemy->hp        = remainingHp;
            remainingHp    <<= 16;
            work->action     = remainingHp <= 0 ? ACTOR_105100_ACTION_DEFEATED : ACTOR_105100_ACTION_STAGGER;
            work->actionStep = 0;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Raises the shield after a 60-tick animation wait and returns to idle.
///
/// The task must own the enemy work block. The saved step value also supplies
/// the shield-enabled value and idle animation index when the wait expires.
static void _actor105100RaiseShield(Task* task)
{
    enum {
        ACTOR_105100_SHIELD_BEGIN       = 0,
        ACTOR_105100_SHIELD_WAIT        = 1,
        ACTOR_105100_SHIELD_RAISE_TICKS = 60,
    };

    _Actor105100Work* work;
    s32               step;

    work = task->work;
    step = work->actionStep;
    switch (step) {
        case ACTOR_105100_SHIELD_BEGIN:
            work->anim       = ACTOR_105100_ANIM_RAISE_SHIELD;
            work->timer      = ACTOR_105100_SHIELD_RAISE_TICKS;
            work->actionStep = ACTOR_105100_SHIELD_WAIT;
            break;
        case ACTOR_105100_SHIELD_WAIT:
            if (--work->timer <= 0) {
                work->shield.fields.active = step;
                work->anim                 = step;
                work->action               = ACTOR_105100_ACTION_IDLE;
                work->actionStep           = 0;
                work->timer                = 0;
            }
            break;
    }
}

/// Consumes the partner's healing-ready signal and restores 80 hit points.
///
/// Requires the live enemy in `task->spawnArg2.pointer` and its model body.
/// The addition narrows to an unsigned halfword before a signed-halfword
/// comparison with the HP cap. The readout always reports the full 80 HP,
/// including when clamped, and the room healing effect and sound are emitted.
static void _actor105100ApplyPartnerHealing(Task* task)
{
    enum {
        ACTOR_105100_PARTNER_HEAL_HP = 80,
        ACTOR_105100_HEAL_SOUND      = 0x4033000C,
    };

    Enemy*    enemy;
    GfxCoord* coord;
    s32       healSound;
    s32       audioPan;
    u16       healedHp;

    enemy                                 = task->spawnArg2.pointer;
    coord                                 = task->extra.tmd->coords;
    gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_HEAL_READY);
    healedHp                              = enemy->hp + ACTOR_105100_PARTNER_HEAL_HP;
    enemy->hp                             = healedHp;
    if (D_actor_105100_80141398.hpMax < (s16)healedHp) {
        enemy->hp = D_actor_105100_80141398.hpMax;
    }
    worldTargetAddReadoutAmount(&enemy->node, -ACTOR_105100_PARTNER_HEAL_HP, 0);
    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL, NULL, 0, NULL);
    healSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_105100_HEAL_SOUND;
    audioPan  = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(healSound, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
}

/// Disables the charge's strike sphere and stops drawing the active ring.
///
/// Drops the actor's ring reference after requesting state 4. Summon rings
/// release themselves in that state; charge rings remain dormant until room
/// cancellation. The effect task continues to own its work in either case.
static inline void _actor105100DisableStrikeAndRing(_Actor105100Work* work)
{
    enum { ACTOR_105100_RING_STOP_REQUEST = 4 };
    EffectWork* ring;

    ring                    = work->ringEffect;
    work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (ring != NULL) {
        ring->task->state = ACTOR_105100_RING_STOP_REQUEST;
        work->ringEffect  = NULL;
    }
}

/// Holds the buildup reaction, then recovers to idle after eleven animation ticks.
///
/// Requires the enemy's task and live work. Entry interrupts summons and
/// sounds, disables the strike and ring, and records a broken charge.
/// Otherwise the shield waits thirty ticks before it can rise again.
/// Completion clears the reaction flag and the pending-buildup latch.
static void _actor105100Buildup(Task* task)
{
    enum {
        ACTOR_105100_BUILDUP_BEGIN          = 0,
        ACTOR_105100_BUILDUP_HOLD           = 1,
        ACTOR_105100_BUILDUP_RECOVER        = 2,
        ACTOR_105100_BUILDUP_RECOVERY_FRAME = 11,
    };

    _Actor105100Work* work;
    Enemy*            enemy;
    s32               step;

    work  = task->work;
    step  = work->actionStep;
    enemy = task->spawnArg2.pointer;
    switch (step) {
        case ACTOR_105100_BUILDUP_BEGIN:
            // Interrupt the attack before holding the buildup animation.
            work->anim       = ACTOR_105100_ANIM_BUILDUP;
            work->actionStep = ACTOR_105100_BUILDUP_HOLD;
            if (work->charging != 0) {
                work->charging               = 0;
                work->chargeBroken           = 1;
                work->shield.fields.cooldown = 0;
            }
            work->summonPhase = ACTOR_105100_SUMMON_NONE;
            _actor105100StopLoopingSounds(task);
            _actor105100DisableStrikeAndRing(work);
            if (work->chargeBroken == 0) {
                work->shield.fields.cooldown = ACTOR_105100_SHIELD_RECOVERY_TICKS;
            }
            break;
        case ACTOR_105100_BUILDUP_HOLD:
            if (damageTickEnemyBuildup(enemy) != 0) {
                work->anim            = ACTOR_105100_ANIM_BUILDUP_END;
                work->actionStep      = ACTOR_105100_BUILDUP_RECOVER;
                work->buildupPending  = 0;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            }
            break;
        case ACTOR_105100_BUILDUP_RECOVER:
            if (work->animFrame >= ACTOR_105100_BUILDUP_RECOVERY_FRAME) {
                work->action     = ACTOR_105100_ACTION_IDLE;
                work->actionStep = ACTOR_105100_BUILDUP_BEGIN;
                work->anim       = ACTOR_105100_ANIM_IDLE;
            }
            break;
    }
}

/// Interrupts the attack and plays twenty-nine ticks of stagger recovery.
///
/// Requires the enemy's live work. Entry clears charging and summons, stops
/// their sounds, and disables the strike and ring. Unless a charge was already
/// broken, it arms the thirty-tick shield wait. A pending buildup resumes after
/// the stagger; otherwise the actor returns to idle.
static void _actor105100Stagger(Task* task)
{
    enum {
        ACTOR_105100_STAGGER_BEGIN          = 0,
        ACTOR_105100_STAGGER_WAIT           = 1,
        ACTOR_105100_STAGGER_RECOVERY_FRAME = 29,
    };

    _Actor105100Work* work;
    s32               step;

    work = task->work;
    step = work->actionStep;
    switch (step) {
        case ACTOR_105100_STAGGER_BEGIN:
            work->anim        = ACTOR_105100_ANIM_STAGGER;
            work->actionStep  = ACTOR_105100_STAGGER_WAIT;
            work->charging    = 0;
            work->summonPhase = ACTOR_105100_SUMMON_NONE;
            _actor105100StopLoopingSounds(task);
            _actor105100DisableStrikeAndRing(work);
            if (work->chargeBroken == 0) {
                work->shield.fields.cooldown = ACTOR_105100_SHIELD_RECOVERY_TICKS;
            }
            break;
        case ACTOR_105100_STAGGER_WAIT:
            if (work->animFrame >= ACTOR_105100_STAGGER_RECOVERY_FRAME) {
                if (work->buildupPending == 0) {
                    work->action = ACTOR_105100_ACTION_IDLE;
                    work->anim   = ACTOR_105100_ANIM_IDLE;
                } else {
                    work->action = ACTOR_105100_ACTION_BUILDUP;
                    work->anim   = ACTOR_105100_ANIM_BUILDUP;
                }
                work->actionStep = ACTOR_105100_STAGGER_BEGIN;
            }
            break;
    }
}

/// Stops an active sound request with its release tail and clears its owner slot.
///
/// Borrows a writable request slot. Zero means no request; other values select
/// the bank, instance and entry to stop. The slot is cleared after queuing the
/// stop even if the event queue cannot admit it, so repeated cleanup is harmless.
static inline void _actor105100StopLoopingSound(s32* requestSlot)
{
    s32 requestId;

    requestId = *requestSlot;
    if (requestId != 0) {
        sndEvtRequestScriptStop(requestId, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        *requestSlot = 0;
    }
}

/// Stops and clears the enemy's active summon and charge sound requests.
///
/// Zero request IDs are inactive. Each active request is stopped with its
/// release tail retained, then cleared so repeated cleanup is harmless.
/// The task must own a live enemy work block.
static void _actor105100StopLoopingSounds(Task* task)
{
    _Actor105100Work* work;

    work = task->work;

    _actor105100StopLoopingSound(&work->fireballSound);
    _actor105100StopLoopingSound(&work->beamSound);
    _actor105100StopLoopingSound(&work->chargeSound);
}

/// Ends attacks and enters teardown, unless the player has already died.
///
/// Requires the live enemy and work block. Player death instead restores one
/// enemy HP and selects stagger without changing the task state. Otherwise it
/// clears charging and summons, stops their sounds, disables the strike and
/// ring, and selects task state 2; destruction happens in that state.
static void _actor105100Defeated(Task* task)
{
    _Actor105100Work* work;
    _Actor105100Work* soundWork;

    work = task->work;
    if (gPlayerStatus.hp <= 0) {
        Enemy* enemy;

        enemy            = task->spawnArg2.pointer;
        enemy->hp        = 1;
        work->action     = ACTOR_105100_ACTION_STAGGER;
        work->actionStep = 0;
        return;
    }

    work->charging    = 0;
    work->summonPhase = ACTOR_105100_SUMMON_NONE;

    soundWork = task->work;
    _actor105100StopLoopingSound(&soundWork->fireballSound);
    _actor105100StopLoopingSound(&soundWork->beamSound);
    _actor105100StopLoopingSound(&soundWork->chargeSound);

    _actor105100DisableStrikeAndRing(work);

    task->state = ACTOR_105100_TASK_TEARDOWN;
}

/// Applies the enemy's animation request or advances its nineteen-part rig.
///
/// Requires live enemy work and model. A changed animation resets the tick
/// counter and starts slots 1..18 with the table's blend duration in frames;
/// otherwise those slots and the counter advance once. Slot 0 stays placed.
static void _actor105100UpdateAnimation(Task* task)
{
    _actor105100AnimUpdate(task);
}

/// Updates the enemy's model lighting and colour at its composed root position.
///
/// Requires the task's live enemy and composed model root, including during
/// a frozen update. The world-lighting service applies colour modes and blends.
static void _actor105100RefreshColor(Task* task)
{
    _actor105100UpdateColor(task);
}

/// The child collision task's state handlers, indexed by `Task::state`:
/// setup, per-frame reaction and teardown.
static const EnemyTaskFuncTable3 D_actor_105100_80131EB0 = {
    {
        _actor105100BeamSpawn,
        _actor105100BeamTick,
        _actor105100BeamDestroy,
    },
};

/// Draws the enemy's ground shadow at its composed root position.
///
/// Requires a live model root. The square has a half-side of 2500 world units
/// and GPU shade 128. The drawing service borrows the three-component position
/// for this call and applies the room-effect visibility policy.
static void _actor105100DrawShadow(Task* task)
{
    enum {
        ACTOR_105100_SHADOW_HALF_SIZE = 2500,
        ACTOR_105100_SHADOW_SHADE     = 128,
    };
    GfxCoord* coord;
    VECTOR3   worldPosition;

    coord            = task->extra.tmd->coords;
    worldPosition.vx = coord->workm.t[0];
    worldPosition.vy = coord->workm.t[1];
    worldPosition.vz = coord->workm.t[2];
    effectDrawGroundShadow(&worldPosition, ACTOR_105100_SHADOW_HALF_SIZE, ACTOR_105100_SHADOW_SHADE);
}

#include "../../shared/model_placement_scale.inc.c"

/// Dispatches a summoned fireball's spawn, movement or teardown handler.
///
/// `task->state` must be 0..2; `spawnArg2.pointer` holds its live enemy.
/// The descriptor supplies a single-coordinate body. Movement borrows a live
/// model-bearing parent; teardown may destroy the fireball enemy and task.
static void _actor105100FireballTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_105100_80131E90;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Unlinks a fireball's attack sphere and sweep capsule, then destroys it.
///
/// Requires live fireball work with both bodies linked. Enemy destruction
/// releases the work and task; none of their pointers remain valid afterwards.
static void _actor105100FireballDestroy(Enemy* enemy, Task* task)
{
    _Actor105100FireballWork* fireball;

    fireball = task->work;
    worldCollisionUnlinkBody(&fireball->body);
    worldCollisionUnlinkBody(&fireball->sweepBody);
    enemyDestroy(enemy, task);
}

/// Dispatches one summoned beam's spawn, update or teardown state.
///
/// `task->state` must be 0..2 and `spawnArg2.pointer` must hold its live enemy.
/// The selected handler may destroy both objects. The task uses one coordinate
/// body and borrows its live parent throughout movement and retirement.
static void _actor105100BeamTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_105100_80131EB0;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Unlinks a beam's attack sphere and destroys its enemy and task.
///
/// Requires the live beam work block with its sphere linked. The update
/// handler has already decremented the parent's beam count. No work, enemy
/// or task pointer remains valid after destruction.
static void _actor105100BeamDestroy(Enemy* enemy, Task* task)
{
    _Actor105100BeamWork* beam;

    beam = task->work;
    worldCollisionUnlinkBody(&beam->body);
    enemyDestroy(enemy, task);
}
