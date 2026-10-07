#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
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
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "overlay.h"

#include "rooms/neo_ark_submarine_gallery.h"
#include "../../shared/screen_wave.h"
#include "../../shared/coord_math.h"
#include "../../shared/diver.h"

extern TaskDesc D_actor_206100_80158B0C[];

/// The `worldCollisionLinkBody` record `func_actor_206100_8014FBE4` unlinks when it
/// retires the actor, plus the area-record list that handler applies.

/// The attack a shot's sphere carries at `WorldCollisionBody.key`.
/// `power` is 0x1A and `reaction` is 5.
extern DamageAttack D_actor_206100_80155194;

/// Enemy parameters `func_actor_206100_8014AF74` parks in `Enemy::param`.
/// `attacks` is `D_actor_206100_80155194` above and `hpMax` is the actor's
/// maximum hit points (2000), seeded into `field_40` / `field_42` at spawn.
extern EnemyParams D_actor_206100_80155198;

/// Animation bank handed to `animationInitContext` by `func_actor_206100_8014AF74`.
extern AnimationSet* D_actor_206100_80158B24[];

/// Placement records `func_actor_206100_8014EE2C` parks at `Enemy::place`
/// -- the same slot `Gp_SpawnArea` fills from a room's own place list, so this
/// is a local six-entry copy of one: `field_0` is 4 on the five live entries
/// and 0xFF on the sixth, the value `Gp_SpawnArea` stops its walk on.  The
/// overlay indexes it with the variant it was spawned for rather than walking
/// it, so the tail entry is reachable.
extern AreaPlacement D_actor_206100_80155134[];

/// The eight positions of the ring the actor circles, walked by the index at
/// `_Actor206100Work::waypointIndex`.
///
/// Each is a point in the root coordinate's translation units: radius 7600 in
/// the XZ plane, one 45-degree step per entry, at a constant height of 3000.
extern SVECTOR D_actor_206100_80158B68[8];

/// Frames a Bog Diver slot stays empty after the diver in it has died.
#define ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN 0xB4

/// One of the two Bog Divers the Sea Diver keeps summoned while it circles.
///
/// A slot holds one summoned diver at a time. While it is empty, and fewer
/// than five divers have been summoned in all, it summons the next one as soon
/// as its cooldown has run out. When the diver in it dies the slot empties
/// and waits `ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN` frames.
typedef struct {
    Enemy* enemy;          // the summoned Bog Diver's enemy record; NULL while the slot is empty
    s32    summonCooldown; // frames before the empty slot summons again, counted down a frame while it is empty
} _Actor206100BogDiverSlot;
STATIC_ASSERT_SIZEOF(_Actor206100BogDiverSlot, 0x8);

/// The two Bog Diver slots, emptied by the spawn state
/// `func_actor_206100_8014C274`.
extern _Actor206100BogDiverSlot D_actor_206100_80158CBC[2];

/// Farthest from the origin, in the XZ plane, that a step of the fight may
/// leave the root.
#define ACTOR_206100_FIGHT_AREA_RADIUS 0x190

/// Scratch-stack block of the fight's bounded step: where the root stands
/// from the origin once the step has been taken.
///
/// The step reserves one block and releases it before it returns.
typedef struct {
    SVECTOR toOrigin; // from the root to the origin in the XZ plane; `vy` and `pad` are never written
    s32     distance; // length of `toOrigin`; past `ACTOR_206100_FIGHT_AREA_RADIUS` the step's XZ is undone
} _Actor206100OriginDistanceScratch;
STATIC_ASSERT_SIZEOF(_Actor206100OriginDistanceScratch, 0xC);

/// Values of `_Actor206100Work::hitReaction`.
enum {
    ACTOR_206100_HIT_REACTION_NONE   = 0,
    ACTOR_206100_HIT_REACTION_LIGHT  = 1, // light recoil of the neck and head
    ACTOR_206100_HIT_REACTION_HEAVY  = 2, // heavy recoil of the neck and head
    ACTOR_206100_HIT_REACTION_STATUS = 3, // held until the status buildup runs out
    ACTOR_206100_HIT_REACTION_BLAST  = 4  // heavy recoil, then the recoil state
};

/// Values of `_Actor206100Work::neckPhase`.
enum {
    ACTOR_206100_NECK_FREE       = 0, // the animation poses the neck
    ACTOR_206100_NECK_STRAIGHTEN = 1, // the captured neck angles ease to zero
    ACTOR_206100_NECK_RETRACTED  = 2  // the neck is straight and `neckScale` shortens it
};

/// Values of `_Actor206100Work::state` during the fight (task state 2). 4, 5
/// and 6 have empty handlers and are never selected.
enum {
    ACTOR_206100_FIGHT_STATE_ENTRANCE    = 0, // scripted: whites the screen out, moves with the player to the fight's place and comes up
    ACTOR_206100_FIGHT_STATE_ATTACK      = 1, // discharges, then fires six shots at the target
    ACTOR_206100_FIGHT_STATE_DIVE        = 2, // goes under with a splash and waits
    ACTOR_206100_FIGHT_STATE_SURFACE     = 3, // comes back up; one time in four dives again, otherwise attacks
    ACTOR_206100_FIGHT_STATE_RECOIL      = 7, // plays the recoil animation out, then dives
    ACTOR_206100_FIGHT_STATE_STATUS_HOLD = 8  // floats at the water level until the status buildup runs out
};

/// Work block of the Sea Diver task.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation playback and its storage, the two collision spheres that take
/// the hits with their contact records, storage for the model's matrices, the
/// ring the diver circles before the fight, and the state machine.
///
/// The task state picks a table of states, `state` an entry of that table and
/// `subState` a step of that entry's own table. The task states are the spawn
/// (0), the circling of the waypoint ring while Bog Divers are summoned (1),
/// the fight (2), the death (3) and the despawn (4). While circling, `state`
/// is 0 for the placement on the ring and 1 from then on; the death counts it
/// up through its four steps.
///
/// Angles are 4096ths of a turn, and positions are in the root coordinate's
/// parent space, which is the view's, with Y growing downward. Model parts are
/// numbered as the skeleton's coordinates, which is the Bog Diver's: 0 the
/// root, 1 the trunk, 2 and 3 the neck, 4 the head and 5 the head's one child.
typedef struct {
    ActorAnimRig15        rig;                // animation playback of the model, one slot per part; 1..14 play `animClip`, and slot 1's status tells when it ended
    u16                   lookPitch;          // signed pitch of the head's look toward the target; an attack eases it to 0x400 while it charges
    u16                   lookYaw;            // signed yaw of the look toward the target, spread over parts 2, 3 and 4 a third each
    u16                   lookRoll;           // third angle of the look; only ever rewritten with its own value and never applied
    byte                  field_362[0x2];     // never accessed
    WorldCollisionBody    trunkBody;          // sphere on part 1 that other bodies touch; its contacts carry the hits
    WorldCollisionContact hitContacts[6];     // contacts of `trunkBody` and `headBody`; also the enemy's hit records
    WorldCollisionBody    headBody;           // smaller sphere on part 4; shares `hitContacts`
    SVECTOR               prevRootPos;        // root position at the start of the frame; its XZ is restored when a step ends more than 400 from the origin
    SVECTOR               rotation;           // root rotation: `vy` heading, `vz` roll; `vx` is only ever cleared and never applied
    byte                  field_444[0x1C];    // never accessed
    MATRIX                colorMtx;           // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;           // storage for the model's `TmdObject::lightMtx`
    byte                  field_4A0[0x20];    // never accessed
    EffectSpawnArg        effectArg;          // argument record of the hit and discharge effects, hung off part 1
    byte                  field_4C8[0x8];     // never accessed
    SVECTOR               targetPos;          // root position of the nearer of the player and the companion
    SVECTOR               lowerNeckAngles;    // Euler angles of part 2 as the neck retracted, eased to zero
    SVECTOR               upperNeckAngles;    // the same for part 3
    byte                  field_4E8[0xC];     // never accessed
    SVECTOR*              waypoints;          // ring of eight points circled while Bog Divers are summoned and into the entrance; `vy` is the height swum at
    Task*                 waveTask;           // screen-wave task covering the entrance's change of place, kept to be killed; NULL when its spawn failed
    byte                  field_4FC[0x8];     // never accessed
    s16                   hitCooldown;        // frames before another hit is taken; set from the hit's id parameter 2
    byte                  field_506[0x2];     // never accessed
    s16                   field_508;          // 0x1000 from the spawn; never read, role unproven
    s16                   field_50A;          // 0x1000 from the spawn; never read, role unproven
    s16                   animRequest;        // `DIVER_ANIM_REQUEST_*`
    s16                   animPlaying;        // animation last applied to the slots
    s16                   animClip;           // requested animation: index into the package's animation bank
    s16                   animFrames;         // frames since `animClip` was applied; rescaled to the new rate when a blend re-requests the playing animation
    u16                   animStatus;         // slot 1's ANIMATION_SLOT_* results from the latest frame's ticks; the states test this copy to learn their clip ended
    u16                   bobPhase;           // counts the frames run before the death, wrapping; never read here - the Bog Diver's bob counter sits at this place in its block
    u16                   frameCount;         // counts the same frames beside `bobPhase`; never read here either
    s16                   animStep;           // playback rate of slots 1..14; `ANIMATION_RATE_ONE` is normal speed
    s16                   playerBearing;      // heading from the root to the player relative to `rotation.vy`, 0..0xFFF; never read
    s16                   stateFrames;        // frames spent in the current state or step; the entrance's white-out counts it up by 6 as its level
    s16                   state;              // index into the state table of the current task state; `ACTOR_206100_FIGHT_STATE_*` during the fight
    s16                   subState;           // index into the step table of the current state
    s16                   animBlend;          // frames a blend request takes; cleared once a different animation has been blended into
    s16                   goalY;              // Y the root eases a sixteenth of the way to each frame from the fight on; follows the waypoint's while circling
    s16                   targetDistance;     // horizontal distance from the root to `targetPos`
    s16                   hitTaken;           // 1 when a hit or a status tick dealt damage this frame; lets `hitReaction` be consumed
    s16                   hitReaction;        // `ACTOR_206100_HIT_REACTION_*` awaiting the state machine
    s16                   attackFrames;       // frames left of the spark discharge that opens an attack, 24 from its start; paces its sound and sparks
    byte                  field_530[0x4];     // never accessed
    s16                   field_534;          // counted down a frame during the fight while nonzero, but never set; role unproven
    s16                   waterLevel;         // Y of the room's water surface: the float height of the status hold and the death, and 400 under it the diver cannot be locked onto
    byte                  field_538[0x2];     // never accessed
    s16                   neckPhase;          // `ACTOR_206100_NECK_*`
    s16                   neckScale;          // Z scale of part 2, 0x1000 = 1.0: eased to 0x2AA while retracted and back on release; the head takes the inverse
    s16                   modelScale;         // uniform scale of the root, 0x1000 = 1.0; 0x1EAA from the spawn
    s16                   part5Pitch;         // pitch added to part 5 by a recoil: swung to -0x300, then back to 0
    s16                   recoilPitch;        // pitch a recoil throws the neck and head back by: a third each on parts 2 and 3, all of it on the head
    s16                   recoilPeak;         // `recoilPitch` the running recoil rises to: 0x135 light, 0x3A0 heavy
    byte                  field_546[0x2];     // never accessed
    u8                    waypointIndex;      // entry of `waypoints` being swum to, 0..7
    byte                  field_549[0x2];     // never accessed
    u8                    wasHit;             // set by every hit that deals damage; never read
    byte                  field_54C[0x1];     // never accessed
    u8                    neckRetracted;      // 1 asks for the neck drawn in, as it is while circling; 0 releases it and lets the head look at the target
    byte                  field_54E[0x1];     // never accessed
    u8                    waypointsSinceRoll; // waypoints reached since the count last wrapped, 0..5; while it is 0 the diver rolls
    u8                    rolling;            // 1 while `rotation.vz` turns 0x20 a frame, until it completes a turn
    u8                    bogDiversSpawned;   // Bog Divers summoned so far, at most 5; also the index of the next one's placement
    u8                    bogDiversKilled;    // summoned Bog Divers that have died; the fifth starts the fight
    u8                    savedView;          // the session's view index as the entrance's white-out replaced it; never read
    u8                    recoilPhase;        // of `recoilPitch` (0 settles to zero, 1 voices the hit, 2 rises to `recoilPeak`, 3 falls back)
    u8                    shotRequested;      // 1 has the frame's tail spawn a shot from the head; set on each of an attack's six cue frames
    u8                    part5Phase;         // of `part5Pitch` (0 settles to zero, 1 starts, 2 swings to -0x300, 3 swings back)
    u8                    targetPart;         // part the enemy's target point and hit effects hang off: 4, or 1 during the status hold
} _Actor206100Work;
STATIC_ASSERT_SIZEOF(_Actor206100Work, 0x558);

/// The Diver library's name for this package's work block (see diver.h).
typedef _Actor206100Work DiverWork;

/// Distance ahead of the head, along its forward axis, at which a shot appears.
#define ACTOR_206100_SHOT_MUZZLE_DISTANCE 0x15E

/// Distance a shot moves along the head's forward axis every frame.
#define ACTOR_206100_SHOT_SPEED 0x5A

/// Added to a shot's vertical speed every frame.
#define ACTOR_206100_SHOT_GRAVITY 2

/// Frame count at which a shot still flying bursts by itself.
#define ACTOR_206100_SHOT_LIFETIME 0x5B

/// `_Actor206100ShotWork::burstSize` at launch, and what it grows by a frame.
#define ACTOR_206100_SHOT_BURST_SIZE_STEP 0x100

/// Value `_Actor206100ShotWork::burstSize` stops growing at.
#define ACTOR_206100_SHOT_BURST_SIZE_MAX 0x600

/// Added to `_Actor206100ShotWork::burstSize` to make the size word handed to
/// `_diverImpactBurst`: a room-particle size bias of 2 in bits 12..15. Bit 28
/// is set as well; the burst ignores bits 16..31.
#define ACTOR_206100_SHOT_BURST_VARIANT 0x10002000

/// Work block of a shot of the Sea Diver's attack.
///
/// A shot is a task of its own with a coordinate for a body, parented to the
/// view coordinate. The fight spawns one on each of an attack's six cue
/// frames, `ACTOR_206100_SHOT_MUZZLE_DISTANCE` ahead of the head and aimed
/// along it; from then on the shot moves by `velocity` every frame, falling
/// under `ACTOR_206100_SHOT_GRAVITY`, and throws sparks and spray that grow
/// with `burstSize`. It bursts when its sphere touches a body or the room, or
/// when it has flown `ACTOR_206100_SHOT_LIFETIME` frames, and the Diver
/// library's teardown then unlinks the sphere and ends the task.
typedef struct {
    DiverStrikeWork       strike;      // head the Diver library's teardown reads: the sphere carrying the attack, linked while the shot flies
    WorldCollisionContact contacts[2]; // contacts of the sphere: what the shot touched this frame
    SVECTOR               velocity;    // movement a frame in the view coordinate's space: the head's forward axis times `ACTOR_206100_SHOT_SPEED`; `pad` is never accessed
    s32                   burstPhase;  // phase handed to the burst, picking the spark's frame and pacing its puffs; cleared at launch and never advanced, where the Bog Diver's shot counts its frames here
    s32                   burstSize;   // size of the burst's spark and spray: `ACTOR_206100_SHOT_BURST_SIZE_STEP` at launch, growing by as much a frame up to `ACTOR_206100_SHOT_BURST_SIZE_MAX`
} _Actor206100ShotWork;
STATIC_ASSERT_SIZEOF(_Actor206100ShotWork, 0x68);

/// The wave `func_actor_206100_8014CB68` arms: pale cyan modulation with a
/// one-frame ramp.
extern ScreenWaveCtx D_actor_206100_80158CCC;

/// Child task `func_actor_206100_8014CB68` starts with the tint above as its
/// spawn arg.  Its callback is `screenWaveTask`.
extern TaskDesc D_actor_206100_80158AF0[];

/// Spawn-state body: hands the freshly spawned enemy its model, its part
/// coordinate and its state, then starts the animation.
///
/// `task->spawnArg2.pointer` is the `Enemy` `func_actor_206100_8014EC14` spawned, so
/// this is the writer of nearly every field that spawn leaves unset.  The
/// `TmdObject` at `task->extra` gets the two `MATRIX` slots the overlay's
/// light / colour hand-off uses (`lightMtx` the 0x480 `work->lightMtx`,
/// `colorMtx` the 0x460 `work->colorMtx`) and `otOffset` 0xA -- the
/// ordering-table offset the draw pass links the model's primitives at.
/// `coord` is the model's root coordinate,
/// which the effect argument at `effectArg` reuses for part 1 (`field_8[1]`),
/// so `enemy->field_4` and the effect share one coordinate.
///
/// `hp` is read once into a local because `D_actor_206100_80155198.hpMax` is
/// the pair's max HP and both `field_40` and `field_42` take it -- reading the
/// global twice instead costs a register and shifts the whole function's
/// allocation (see `DECOMPILATION_LEARNINGS.md`, "A repeated global load ...").
/// The two `task->extra` walks after `worldTargetLinkNode` are separate reloads in the
/// original, which is why `tmd` is not reused for `field_8[4]`.
static void func_actor_206100_8014AF74(Task* task);

/// Builds the enemy's two collision objects.  Each is bound to a part
/// coordinate of the actor's `TmdObject` -- `trunkBody` to `coords[1]` with
/// `field_1C` 0x400, `headBody` to `field_8[4]` with 0x200 -- and both point
/// their `field_C` at the shared `WorldCollisionContact` pair table zeroed at `hitContacts`,
/// which is why there is a single `worldCollisionInitContacts` for the pair.  Each
/// block ends by clearing `flags` bit 0x8000 after its `worldCollisionLinkBody`, the same
/// tail shape `func_actor_403100_80132320` has (`|= 0x8000` there).
static void func_actor_206100_8014F18C(Task* task);

/// First state of a shot's task: parents its coordinate to the view, links
/// the sphere that carries the attack and throws the launch burst. `task` is
/// the shot the fight spawned, so its `Task::work` is a `_Actor206100ShotWork`.
static void func_actor_206100_8014EEC0(Task* task);

/// Steps the actor's model coordinate `arg1` along the heading `arg2`, in the
/// XZ plane, and marks it dirty.
///
/// `task->extra` is the actor's `TmdObject`, so `coords` is the root
/// `GfxCoord` of its part array: `coord.t[0]` gains `rsin(arg2) * arg1`
/// and `coord.t[2]` `rcos(arg2) * arg1`. The `<< 4` on the `rsin` / `rcos`
/// result and the `>> 16` after the multiply are one `>> 12` split in two, the
/// unit circle the rest of the overlay's rotation code uses. Clearing `composeStamp` is
/// what makes the composition pass rebuild the matrix from `coord`, so the caller never
/// writes `workm` itself. The `task->extra` chain is walked again for each of
/// the three statements because `rsin` / `rcos` sit between them.
///
/// Every call site in this overlay takes `arg2` from the actor's heading and
/// `arg1` from a step distance, either a constant (`0x30`, `0x40`) or an
/// `s16` the caller narrows itself.

/// State-1 body: ticks the actor's per-state frame counter and, once it
/// reaches 0x22, walks the actor out of the scene -- parks its model coordinate
/// at y 0x1B58, clears the counter, hands the area record to the light mode,
/// tells slot 3 (message 0x3E9) to place the player, and advances the sub-state
/// `subState`.  Before that, the counter passing 3 fires the overlay's sound
/// event.  Every other frame ramps `goalY` toward 0x1D4C by a quarter of the
/// remaining distance and steps the actor `0x30` along its heading.
///
/// The second argument of `worldCoordSetActorColorMode` is read through a cast rather than
/// as `task->spawnArg2.pointer` directly: the cast makes the load a *scalar* `MEM`,
/// which is what keeps its dependence on the fixed-address
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store, so the store is scheduled ahead of it - the same
/// `MEM_IN_STRUCT_P` mechanism `dryfield_water_tank_4.c` and
/// `DECOMPILATION_LEARNINGS.md`, "Scalar memory references", describe.
static void func_actor_206100_8014CD08(Task* task);

/// State handler 4 of `D_actor_206100_80149E94`, and the one that hands the
/// actor to `func_actor_206100_8014CD08` above.  It clears the fixed-address
/// `D_neo_ark_submarine_gallery_801818B8` flag, ticks the per-state counter `stateFrames` and seeds
/// `D_actor_206100_80158CCC.state` to `SCREEN_WAVE_RAMP_FINISHED` on the first frame; frame 3 retires the
/// child task `func_actor_206100_8014CB68` spawned into `waveTask`, and if the
/// kill left the counter alone, fires sound event 0x551E0003; frame 0xC splats
/// the 0x01202148 particle ring `func_actor_206100_8014D574` fires, at a radius
/// of 0x1000 and a constant y of -0x294; and frame 0x46 clears the model
/// coordinate's x and z, plays the weapon and 0x3F3 messages under light mode
/// 2, and moves the actor to state 1 at sub-state 0.
///
/// That last block reads `task->work` again instead of reusing the `work`
/// pointer, the same fresh load `set_state` makes, so the two stores stay a
/// block-local quantity.
static void func_actor_206100_8014CE60(Task* task);

/// Sub-state 0 of `func_actor_206100_8014CFF4`'s table: clears the per-state
/// frame counter and the actor's animation request, then advances the
/// sub-state.
static void func_actor_206100_8014F6F8(Task* task);

/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks `stateFrames`,
/// arms `attackFrames` with 0x18 on the first frame, eases `lookPitch` toward
/// 0x400 over frames 0x29..0x4D, fires the overlay's sound events at 0x54
/// (type 6, panned) and 0x77, sets `shotRequested` on the six cue frames of the
/// counter, and folds the heading to the actor's target --
/// `VectorNormalSS` then `ratan2` -- into `rotation.vy` in steps of 0xC.
static void func_actor_206100_8014D14C(Task* task);

/// Sub-state 1 of `D_actor_206100_8014D6F4`'s table
/// (`D_actor_206100_80149EB4`, whose first entry `func_actor_206100_8014F7B4`
/// and third `func_actor_206100_8014F878` bracket it).  On the seventh frame of
/// the sub-state it splats 0x20 effect particles around the actor's root
/// coordinate -- the same `effectSpawn` id 0x01202148 ring
/// `func_actor_206100_8014D574` fires, at a radius of 0x1000 and a constant
/// y of -0x3E8 -- and from frame 0x1F it draws from `gRandomLcgState`: a one-in-four
/// `(state >> 16) & 3 == 0` hands state 2 (the teleport
/// `func_actor_206100_8014CB68`) to the actor at sub-state 0, and every other
/// draw restarts the counter and advances the sub-state.
static void func_actor_206100_8014D8E8(Task* task);

/// Sub-state 1 of the state-2 dispatcher `func_actor_206100_8014DA28`'s
/// two-entry local table, which picks it with `funcs[state]` and is
/// entered from that dispatcher's `gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING` arm -- entry 0 is the ring
/// stepper `func_actor_206100_8014FAE4`.  It keeps the Bog Divers summoned.
///
/// `stateFrames` is held at 0x1E -- the frame `func_actor_206100_8014D574` fires
/// the explosion on -- by arming the global `gSceneCombatState` flag again through
/// `sceneEngageBattle` instead of advancing it, so the sub-state never leaves it;
/// every earlier frame just advances the counter.  The shared companion tick
/// `func_actor_206100_8014DEAC` then runs, and the two slots
/// `D_actor_206100_80158CBC` are walked by index, each handled on its own:
///
/// - an empty slot whose `summonCooldown` has run out summons a Bog Diver with
///   `func_actor_206100_8014EE2C`, stores it, arms its `hp`
///   and advances `bogDiversSpawned` -- but only while `bogDiversSpawned` is still
///   below 5, because that index picks the diver's place record;
/// - an empty slot whose `summonCooldown` is still running counts it down by one;
/// - a filled slot whose diver has lost its `hp` is emptied and armed with
///   `ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN`, and the fifth such release moves
///   the actor to state 2 with the state and sub-state indices cleared.
///
/// The state change reads `task->work` again rather than reusing `work`, the
/// same fresh load `set_state` makes.
static void func_actor_206100_8014DD3C(Task* task);

/// Transforms `pos` from `coord`'s space up the parent chain into the view
/// coordinate's space.  Returns 1 with `pos` rewritten once the walk reaches
/// `gGfxViewCoord`, or 0 with `pos` untouched if the chain ends first.

/// Last of the actor's five top-level states (`D_actor_206100_80149E5C`):
/// hands the task's `Enemy`, parked in `Task::spawnArg2`, back to
/// `enemyDestroy`.
static void func_actor_206100_8014F490(Task* task);

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone.
static void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst);

static void func_actor_206100_8014B0AC(Task* task, u8 arg1);
static void func_actor_206100_8014E0C0(Task* task);
static void func_actor_206100_8014EB60(Task* task);
static void func_actor_206100_8014EC54(Task* task);
static void func_actor_206100_8014DEAC(Task* task);
static void func_actor_206100_8014FAE4(Task* task);

static Enemy* func_actor_206100_8014EE2C(s32 arg0);

static void func_actor_206100_8014D574(Task* task);
static void func_actor_206100_8014EB48(Task* task, s16 arg1);
static void func_actor_206100_8014ED3C(Task* task, s16 arg1);
static void func_actor_206100_8014F2F0(Task* task);
static s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
static void func_actor_206100_8014F738(Task* task);
static void func_actor_206100_8014F770(Task* task);
static void func_actor_206100_8014F7B4(Task* task);
static void func_actor_206100_8014F878(Task* task);
static void func_actor_206100_8014E964(Task* task);
static void func_actor_206100_8014FBE4(Task* task);
static void func_actor_206100_8014FCD4(Task* task);
static void func_actor_206100_8014FDE8(Task* task);
static void func_actor_206100_8014E228(Task* task);

/// Distortion amplitude of the screen wave: `frame * scale / span` of the
/// running spawn argument, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The spawn argument of the running wave task, parked at spawn so the tick
/// reads the ramp through it.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

/// Task table `func_actor_206100_8014FDE8` spawns the shockwave from.

static void func_actor_206100_8014DEAC(Task* task);
static void func_actor_206100_8014F2F0(Task* task);
static s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
static void func_actor_206100_8014F970(Task* task);
static void func_actor_206100_8014F9C4(Task* task);
static void func_actor_206100_8014FA08(Task* task);

static void func_actor_206100_8014B8B4(Task* task);

static const TaskFuncTable3 D_actor_206100_80149E24 = {
    {
        func_actor_206100_8014EEC0,
        func_actor_206100_8014B8B4,
        _diverStrikeTeardown,
    },
};

static u32     _gActor206100DiverEnergyBallPartVerts[1];
static SVECTOR _gActor206100DiverEnergyBallVerts[24];
static SVECTOR _gActor206100DiverEnergyBallNormals[25];
static TmdBone _gActor206100DiverEnergyBallSkeleton[1];
static u32     _gActor206100DiverEnergyBallStream[270];

static TmdSource _gActor206100DiverBody;
void             func_actor_206100_8014F134(Task*);
void             func_actor_206100_8014F428(Task*);

static TmdBone _gActor206100DiverBodySkeleton[15] = {
#include "assets/diver_body_skeleton.inc"
};

static u32 _gActor206100DiverBodyPartVerts[15] = {
#include "assets/diver_body_partVerts.inc"
};

static SVECTOR _gActor206100DiverBodyVerts[145] = {
#include "assets/diver_body_verts.inc"
};

static SVECTOR _gActor206100DiverBodyNormals[142] = {
#include "assets/diver_body_normals.inc"
};

static u32 _gActor206100DiverBodyStream[2494] = {
#include "assets/diver_body_stream.inc"
};

static TmdSource _gActor206100DiverBody = {
    0,
    11652,
    5104,
    15,
    _gActor206100DiverBodyPartVerts,
    _gActor206100DiverBodyVerts,
    _gActor206100DiverBodyNormals,
    _gActor206100DiverBodySkeleton,
    _gActor206100DiverBodyStream,
};

static TmdBone _gActor206100DiverBurstHeadSkeleton[1] = {
#include "assets/diver_burst_head_skeleton.inc"
};

static u32 _gActor206100DiverBurstHeadPartVerts[1] = {
#include "assets/diver_burst_head_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadVerts[35] = {
#include "assets/diver_burst_head_verts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadNormals[44] = {
#include "assets/diver_burst_head_normals.inc"
};

static u32 _gActor206100DiverBurstHeadStream[360] = {
#include "assets/diver_burst_head_stream.inc"
};

static TmdSource _gActor206100DiverBurstHead = {
    0,
    2388,
    0,
    1,
    _gActor206100DiverBurstHeadPartVerts,
    _gActor206100DiverBurstHeadVerts,
    _gActor206100DiverBurstHeadNormals,
    _gActor206100DiverBurstHeadSkeleton,
    _gActor206100DiverBurstHeadStream,
};

static TmdBone _gActor206100DiverBurstArmRightSkeleton[1] = {
#include "assets/diver_burst_arm_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmRightPartVerts[1] = {
#include "assets/diver_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightVerts[14] = {
#include "assets/diver_burst_arm_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightNormals[24] = {
#include "assets/diver_burst_arm_right_normals.inc"
};

static u32 _gActor206100DiverBurstArmRightStream[143] = {
#include "assets/diver_burst_arm_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmRight = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmRightPartVerts,
    _gActor206100DiverBurstArmRightVerts,
    _gActor206100DiverBurstArmRightNormals,
    _gActor206100DiverBurstArmRightSkeleton,
    _gActor206100DiverBurstArmRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft1Skeleton[1] = {
#include "assets/diver_burst_arm_left_1_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft1PartVerts[1] = {
#include "assets/diver_burst_arm_left_1_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Verts[14] = {
#include "assets/diver_burst_arm_left_1_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Normals[24] = {
#include "assets/diver_burst_arm_left_1_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft1Stream[143] = {
#include "assets/diver_burst_arm_left_1_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft1 = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmLeft1PartVerts,
    _gActor206100DiverBurstArmLeft1Verts,
    _gActor206100DiverBurstArmLeft1Normals,
    _gActor206100DiverBurstArmLeft1Skeleton,
    _gActor206100DiverBurstArmLeft1Stream,
};

static TmdBone _gActor206100DiverBurstLegRightSkeleton[1] = {
#include "assets/diver_burst_leg_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstLegRightPartVerts[1] = {
#include "assets/diver_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightVerts[19] = {
#include "assets/diver_burst_leg_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightNormals[33] = {
#include "assets/diver_burst_leg_right_normals.inc"
};

static u32 _gActor206100DiverBurstLegRightStream[210] = {
#include "assets/diver_burst_leg_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstLegRight = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstLegRightPartVerts,
    _gActor206100DiverBurstLegRightVerts,
    _gActor206100DiverBurstLegRightNormals,
    _gActor206100DiverBurstLegRightSkeleton,
    _gActor206100DiverBurstLegRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft2Skeleton[1] = {
#include "assets/diver_burst_arm_left_2_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft2PartVerts[1] = {
#include "assets/diver_burst_arm_left_2_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Verts[19] = {
#include "assets/diver_burst_arm_left_2_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Normals[33] = {
#include "assets/diver_burst_arm_left_2_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft2Stream[210] = {
#include "assets/diver_burst_arm_left_2_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft2 = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstArmLeft2PartVerts,
    _gActor206100DiverBurstArmLeft2Verts,
    _gActor206100DiverBurstArmLeft2Normals,
    _gActor206100DiverBurstArmLeft2Skeleton,
    _gActor206100DiverBurstArmLeft2Stream,
};

static TmdBone _gActor206100DiverEnergyBallSkeleton[1] = {
#include "assets/diver_energy_ball_skeleton.inc"
};

static u32 _gActor206100DiverEnergyBallPartVerts[1] = {
#include "assets/diver_energy_ball_partVerts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallVerts[24] = {
#include "assets/diver_energy_ball_verts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallNormals[25] = {
#include "assets/diver_energy_ball_normals.inc"
};

static u32 _gActor206100DiverEnergyBallStream[270] = {
#include "assets/diver_energy_ball_stream.inc"
};

static TmdSource _gActor206100DiverEnergyBall = {
    0,
    1760,
    0,
    1,
    _gActor206100DiverEnergyBallPartVerts,
    _gActor206100DiverEnergyBallVerts,
    _gActor206100DiverEnergyBallNormals,
    _gActor206100DiverEnergyBallSkeleton,
    _gActor206100DiverEnergyBallStream,
};

AreaPlacement D_actor_206100_80155134[6] = {
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

DamageAttack D_actor_206100_80155194 = { 26, 5 };

EnemyParams D_actor_206100_80155198 = { &D_actor_206100_80155194, 2000, 400, 1000, 15, 200, 3, 100, 5 };

static AnimationPackedPose _gActor206100Animation0B8F8Bank1[8] = {
#include "assets/actor_206100_animation_0B8F8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0B8F8Bank4[133] = {
#include "assets/actor_206100_animation_0B8F8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0B8F8Records[183] = {
#include "assets/actor_206100_animation_0B8F8_records.inc"
};

static u16 _gActor206100Animation0B8F8Indices[16] = {
#include "assets/actor_206100_animation_0B8F8_indices.inc"
};

static AnimationSet _gActor206100Animation0B8F8 = {
    _gActor206100Animation0B8F8Records,
    _gActor206100Animation0B8F8Indices,
    { NULL, _gActor206100Animation0B8F8Bank1, NULL, NULL, _gActor206100Animation0B8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0BC80Bank1[3] = {
#include "assets/actor_206100_animation_0BC80_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0BC80Bank4[78] = {
#include "assets/actor_206100_animation_0BC80_bank4.inc"
};

static AnimationRecord _gActor206100Animation0BC80Records[121] = {
#include "assets/actor_206100_animation_0BC80_records.inc"
};

static u16 _gActor206100Animation0BC80Indices[16] = {
#include "assets/actor_206100_animation_0BC80_indices.inc"
};

static AnimationSet _gActor206100Animation0BC80 = {
    _gActor206100Animation0BC80Records,
    _gActor206100Animation0BC80Indices,
    { NULL, _gActor206100Animation0BC80Bank1, NULL, NULL, _gActor206100Animation0BC80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0C544Bank1[8] = {
#include "assets/actor_206100_animation_0C544_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0C544Bank4[174] = {
#include "assets/actor_206100_animation_0C544_bank4.inc"
};

static AnimationRecord _gActor206100Animation0C544Records[345] = {
#include "assets/actor_206100_animation_0C544_records.inc"
};

static u16 _gActor206100Animation0C544Indices[16] = {
#include "assets/actor_206100_animation_0C544_indices.inc"
};

static AnimationSet _gActor206100Animation0C544 = {
    _gActor206100Animation0C544Records,
    _gActor206100Animation0C544Indices,
    { NULL, _gActor206100Animation0C544Bank1, NULL, NULL, _gActor206100Animation0C544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0CC4CBank1[10] = {
#include "assets/actor_206100_animation_0CC4C_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0CC4CBank4[167] = {
#include "assets/actor_206100_animation_0CC4C_bank4.inc"
};

static AnimationRecord _gActor206100Animation0CC4CRecords[235] = {
#include "assets/actor_206100_animation_0CC4C_records.inc"
};

static u16 _gActor206100Animation0CC4CIndices[16] = {
#include "assets/actor_206100_animation_0CC4C_indices.inc"
};

static AnimationSet _gActor206100Animation0CC4C = {
    _gActor206100Animation0CC4CRecords,
    _gActor206100Animation0CC4CIndices,
    { NULL, _gActor206100Animation0CC4CBank1, NULL, NULL, _gActor206100Animation0CC4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D530Bank1[10] = {
#include "assets/actor_206100_animation_0D530_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D530Bank4[213] = {
#include "assets/actor_206100_animation_0D530_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D530Records[308] = {
#include "assets/actor_206100_animation_0D530_records.inc"
};

static u16 _gActor206100Animation0D530Indices[16] = {
#include "assets/actor_206100_animation_0D530_indices.inc"
};

static AnimationSet _gActor206100Animation0D530 = {
    _gActor206100Animation0D530Records,
    _gActor206100Animation0D530Indices,
    { NULL, _gActor206100Animation0D530Bank1, NULL, NULL, _gActor206100Animation0D530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D968Bank1[6] = {
#include "assets/actor_206100_animation_0D968_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D968Bank4[103] = {
#include "assets/actor_206100_animation_0D968_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D968Records[131] = {
#include "assets/actor_206100_animation_0D968_records.inc"
};

static u16 _gActor206100Animation0D968Indices[16] = {
#include "assets/actor_206100_animation_0D968_indices.inc"
};

static AnimationSet _gActor206100Animation0D968 = {
    _gActor206100Animation0D968Records,
    _gActor206100Animation0D968Indices,
    { NULL, _gActor206100Animation0D968Bank1, NULL, NULL, _gActor206100Animation0D968Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0DEA8Bank1[15] = {
#include "assets/actor_206100_animation_0DEA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0DEA8Bank4[109] = {
#include "assets/actor_206100_animation_0DEA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0DEA8Records[164] = {
#include "assets/actor_206100_animation_0DEA8_records.inc"
};

static u16 _gActor206100Animation0DEA8Indices[16] = {
#include "assets/actor_206100_animation_0DEA8_indices.inc"
};

static AnimationSet _gActor206100Animation0DEA8 = {
    _gActor206100Animation0DEA8Records,
    _gActor206100Animation0DEA8Indices,
    { NULL, _gActor206100Animation0DEA8Bank1, NULL, NULL, _gActor206100Animation0DEA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E4A8Bank1[20] = {
#include "assets/actor_206100_animation_0E4A8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E4A8Bank4[126] = {
#include "assets/actor_206100_animation_0E4A8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E4A8Records[180] = {
#include "assets/actor_206100_animation_0E4A8_records.inc"
};

static u16 _gActor206100Animation0E4A8Indices[16] = {
#include "assets/actor_206100_animation_0E4A8_indices.inc"
};

static AnimationSet _gActor206100Animation0E4A8 = {
    _gActor206100Animation0E4A8Records,
    _gActor206100Animation0E4A8Indices,
    { NULL, _gActor206100Animation0E4A8Bank1, NULL, NULL, _gActor206100Animation0E4A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E798Bank1[5] = {
#include "assets/actor_206100_animation_0E798_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E798Bank4[43] = {
#include "assets/actor_206100_animation_0E798_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E798Records[112] = {
#include "assets/actor_206100_animation_0E798_records.inc"
};

static u16 _gActor206100Animation0E798Indices[16] = {
#include "assets/actor_206100_animation_0E798_indices.inc"
};

static AnimationSet _gActor206100Animation0E798 = {
    _gActor206100Animation0E798Records,
    _gActor206100Animation0E798Indices,
    { NULL, _gActor206100Animation0E798Bank1, NULL, NULL, _gActor206100Animation0E798Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0ECA8Bank1[11] = {
#include "assets/actor_206100_animation_0ECA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0ECA8Bank4[108] = {
#include "assets/actor_206100_animation_0ECA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0ECA8Records[165] = {
#include "assets/actor_206100_animation_0ECA8_records.inc"
};

static u16 _gActor206100Animation0ECA8Indices[16] = {
#include "assets/actor_206100_animation_0ECA8_indices.inc"
};

static AnimationSet _gActor206100Animation0ECA8 = {
    _gActor206100Animation0ECA8Records,
    _gActor206100Animation0ECA8Indices,
    { NULL, _gActor206100Animation0ECA8Bank1, NULL, NULL, _gActor206100Animation0ECA8Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_206100_80158AF0[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_actor_206100_80158B0C[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_206100_8014F428, { .model = &_gActor206100DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_206100_8014F134, { .value = 0 } },
};

AnimationSet* D_actor_206100_80158B24[17] = {
    NULL,
    &_gActor206100Animation0B8F8,
    NULL,
    &_gActor206100Animation0BC80,
    NULL,
    &_gActor206100Animation0C544,
    NULL,
    &_gActor206100Animation0CC4C,
    &_gActor206100Animation0D530,
    NULL,
    &_gActor206100Animation0D968,
    &_gActor206100Animation0DEA8,
    NULL,
    NULL,
    &_gActor206100Animation0E4A8,
    &_gActor206100Animation0E798,
    &_gActor206100Animation0ECA8,
};

SVECTOR D_actor_206100_80158B68[8] = {
    { 0, 3000, 7600, 0 },
    { 5373, 3000, 5373, 0 },
    { 7600, 3000, 0, 0 },
    { 5373, 3000, -5373, 0 },
    { 0, 3000, -7600, 0 },
    { -5373, 3000, -5373, 0 },
    { -7600, 3000, 0, 0 },
    { -5373, 3000, 5373, 0 },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

_Actor206100BogDiverSlot D_actor_206100_80158CBC[2] = { 0 };

ScreenWaveCtx D_actor_206100_80158CCC = { 0 };

static void func_actor_206100_8014DA28(Task* task);

static void func_actor_206100_8014C458(Task* task);

static void func_actor_206100_8014E7D4(Task* task);

static void func_actor_206100_8014F524(Task* task);

static void func_actor_206100_8014CFF4(Task* task);

static void func_actor_206100_8014D380(Task* task);

static void func_actor_206100_8014D6F4(Task* task);

static void func_actor_206100_8014F59C(Task* task);

static void func_actor_206100_8014F5A4(Task* task);

static void func_actor_206100_8014F5AC(Task* task);

static void func_actor_206100_8014F5B4(Task* task);

static void func_actor_206100_8014F608(Task* task);

static void func_actor_206100_8014F65C(Task* task);

static void func_actor_206100_8014F69C(Task* task);

extern TaskDesc D_actor_100400_80147E48;

static void            func_actor_206100_8014B698(Task* task);
static void            func_actor_206100_8014BAA8(Task* task);
static inline void     _actor206100AnimUpdate(Task* task);
static void            func_actor_206100_8014C274(Task* task);
static __inline__ void Actor206100_UpdateColor(Task* task);
static void            func_actor_206100_8014CB68(Task* task);
static __inline__ void set_state(Task* task, s32 state);
static __inline__ s16  take_request(Task* task);

#include "../../shared/screen_wave.inc.c"

#include "../../shared/diver_impact_burst.inc.c"
#include "../../shared/diver_draw_spark.inc.c"

static void func_actor_206100_8014AF74(Task* task)
{
    _Actor206100Work* work;
    TmdObject*        tmd;
    Enemy*            enemy;
    GfxCoord*         coord;
    u16               hp;

    tmd                        = task->extra.tmd;
    work                       = task->work;
    enemy                      = (Enemy*)task->spawnArg2.pointer;
    tmd->otOffset              = 0xA;
    tmd->lightMtx              = &work->lightMtx;
    tmd->flags                 = 0;
    tmd->colorMtx              = &work->colorMtx;
    coord                      = tmd->coords;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x580;
    work->effectArg.spawnArgHi = 3;
    enemy->field_4             = &coord->coord;
    enemy->field_48            = 0;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vy          = 0;
    enemy->bodyPos.vz          = 0;
    enemy->coord               = &task->extra.tmd->coords[4];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->hitContacts;
    enemy->param                  = &D_actor_206100_80155198;
    hp                            = D_actor_206100_80155198.hpMax;
    enemy->hpMax                  = hp;
    enemy->hp                     = hp;
    coord->parent                 = &gGfxViewCoord;
    animationInitContext(&work->rig.anim, D_actor_206100_80158B24, tmd, work->rig.poses, work->rig.slots);
    func_actor_206100_8014F18C(task);
    work->rotation.vy = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->targetPart  = 4;
}

/// Draws the neck in and lets it out again as `neckRetracted` asks: parts 2, 3
/// and 4 are rebuilt from scratch each frame, the neck's two from an Euler
/// angle triple each and the head from its angles plus a scale.
///
/// `neckRetracted` picks the direction of travel.  While it reads 1 the neck is
/// drawn in: `neckPhase` 0 latches `lowerNeckAngles` and `upperNeckAngles` out
/// of the parts' matrices and falls straight into 1, which folds all six
/// components toward zero by an eighth and arms 2 once every one of them is
/// within 0x30; 2 then ramps `neckScale` down toward the 0x2AA the retracted
/// neck shortens to.  Once `neckRetracted` reads 0 the else arm ramps `neckScale`
/// back up toward 0x1000, and while it is still under 0xF80 it rebuilds the
/// parts the same way; at 0xF80 the neck is out again and the head is handed to
/// `func_actor_206100_8014E228`, which aims it at the target.
///
/// The head is the one that grows rather than shrinks: `neckScale` shortens
/// part 2, so `mc` gets its reciprocal, `0x1000000 /
/// neckScale`.  Note also that the two arms clear `neckPhase` rather than the
/// join after them: written once after the `if`, the else arm's tail becomes
/// instruction-for-instruction the case-2 tail and the jump optimiser merges
/// the two, costing the case-2 copy of the last fifty instructions.
static void func_actor_206100_8014B0AC(Task* task, u8 arg1)
{
    VECTOR            scale;
    MATRIX            rot;
    MATRIX            ma;
    MATRIX            mb;
    SVECTOR           euler;
    MATRIX            mc;
    _Actor206100Work* work;
    GfxCoord*         base;
    GfxCoord*         c2;
    GfxCoord*         c3;
    GfxCoord*         c4;
    s32               invScale;

    base = task->extra.tmd->coords;
    work = task->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];

    switch (work->neckRetracted) {
        case 1:
            switch (work->neckPhase) {
                case ACTOR_206100_NECK_FREE:
                    base[0].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[1].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(c4);
                    gfxExtractEulerAngles(&c2->coord, &work->lowerNeckAngles);
                    gfxExtractEulerAngles(&c3->coord, &work->upperNeckAngles);
                    work->neckPhase = ACTOR_206100_NECK_STRAIGHTEN;
                    work->neckScale = 0x1000;
                    /* fallthrough */
                case ACTOR_206100_NECK_STRAIGHTEN: {
                    work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((s32) - (work->lowerNeckAngles.vx * 0x10) >> 7);
                    work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((s32) - (work->lowerNeckAngles.vy * 0x10) >> 7);
                    work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((s32) - (work->lowerNeckAngles.vz * 0x10) >> 7);
                    work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((s32) - (work->upperNeckAngles.vx * 0x10) >> 7);
                    work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((s32) - (work->upperNeckAngles.vy * 0x10) >> 7);
                    work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((s32) - (work->upperNeckAngles.vz * 0x10) >> 7);
                    gfxSetRotIdentity(&rot);
                    RotMatrix(&work->lowerNeckAngles, &rot);
                    func_actor_206100_8014F4B8(&rot, &c2->coord);
                    gfxSetRotIdentity(&rot);
                    RotMatrix(&work->upperNeckAngles, &rot);
                    func_actor_206100_8014F4B8(&rot, &c3->coord);
                    if ((abs(work->lowerNeckAngles.vx) < 0x30) && (abs(work->lowerNeckAngles.vy) < 0x30) && (abs(work->lowerNeckAngles.vz) < 0x30) &&
                        (abs(work->upperNeckAngles.vx) < 0x30) && (abs(work->upperNeckAngles.vy) < 0x30) && (abs(work->upperNeckAngles.vz) < 0x30)) {
                        work->neckPhase = ACTOR_206100_NECK_RETRACTED;
                    }
                    c2->composeStamp = GRAPHICS_COORD_DIRTY;
                    c3->composeStamp = GRAPHICS_COORD_DIRTY;
                    c4->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(c4);
                    break;
                }
                case ACTOR_206100_NECK_RETRACTED: {
                    gfxExtractEulerAngles(&c4->coord, &euler);
                    work->neckScale = (u16)work->neckScale + ((0x2AA - work->neckScale) >> 3);
                    gfxSetRotIdentity(&ma);
                    scale.vx = 0x1000;
                    scale.vy = 0x1000;
                    scale.vz = work->neckScale;
                    ScaleMatrix(&ma, &scale);
                    func_actor_206100_8014F4B8(&ma, &c2->coord);
                    gfxSetRotIdentity(&mb);
                    scale.vx = 0x1000;
                    scale.vy = 0x1000;
                    scale.vz = 0x1000;
                    ScaleMatrix(&mb, &scale);
                    func_actor_206100_8014F4B8(&mb, &c3->coord);
                    gfxSetRotIdentity(&mc);
                    scale.vx = 0x1000;
                    scale.vy = 0x1000;
                    invScale = 0x1000000 / work->neckScale;
                    scale.vz = invScale;
                    ScaleMatrix(&mc, &scale);
                    gfxSetRotIdentity(&rot);
                    RotMatrix(&euler, &rot);
                    MulMatrix(&mc, &rot);
                    func_actor_206100_8014F4B8(&mc, &c4->coord);
                    base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(c4);
                    break;
                }
            }
            break;
        case 0:
            base[0].composeStamp = GRAPHICS_COORD_DIRTY;
            base[1].composeStamp = GRAPHICS_COORD_DIRTY;
            base[2].composeStamp = GRAPHICS_COORD_DIRTY;
            base[3].composeStamp = GRAPHICS_COORD_DIRTY;
            base[4].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(c4);
            if (work->neckScale < 0xF80) {
                gfxExtractEulerAngles(&c4->coord, &euler);
                work->neckScale = (u16)work->neckScale + ((0x1000 - work->neckScale) >> 2);
                gfxSetRotIdentity(&ma);
                scale.vx = 0x1000;
                scale.vy = 0x1000;
                scale.vz = work->neckScale;
                ScaleMatrix(&ma, &scale);
                func_actor_206100_8014F4B8(&ma, &c2->coord);
                gfxSetRotIdentity(&mb);
                scale.vx = 0x1000;
                scale.vy = 0x1000;
                scale.vz = 0x1000;
                ScaleMatrix(&mb, &scale);
                func_actor_206100_8014F4B8(&mb, &c3->coord);
                gfxSetRotIdentity(&mc);
                scale.vx = 0x1000;
                scale.vy = 0x1000;
                invScale = 0x1000000 / work->neckScale;
                scale.vz = invScale;
                ScaleMatrix(&mc, &scale);
                gfxSetRotIdentity(&rot);
                RotMatrix(&euler, &rot);
                MulMatrix(&mc, &rot);
                func_actor_206100_8014F4B8(&mc, &c4->coord);
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(c4);
                work->neckPhase = ACTOR_206100_NECK_FREE;
            } else {
                func_actor_206100_8014E228(task);
                work->neckPhase = ACTOR_206100_NECK_FREE;
            }
            break;
    }
}
/// Latches the actor's position and picks the nearer of the two `gPlayerActorTasks`
/// actors as its target: it stores the root position in `prevRootPos`, then
/// measures the XZ distance to each slot from the root coordinate, keeping the
/// closer one's position in `targetPos` and its distance in `targetDistance`.
///
/// Two details are load-bearing.  `gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]` is read *before* the three
/// `prevRootPos` stores: written after them the scheduler moves the whole `lui` / `lw`
/// group below the stores, which costs twelve bytes of schedule and shifts
/// every later branch target.  And the player delta `d0` is normalised and fed
/// to `ratan2` even when slot 1 was the closer one, so `playerBearing` follows the
/// player's bearing rather than the target's.
static void func_actor_206100_8014B698(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         coord;
    GfxCoord*         c0;
    GfxCoord*         c1;
    Task*             player;
    SVECTOR           d0;
    SVECTOR           d1;
    s32               dist0;
    s32               dist1;

    work                 = task->work;
    coord                = task->extra.tmd->coords;
    player               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    if (player != NULL) {
        c0    = player->extra.tmd->coords;
        d0.vx = (u16)c0->coord.t[0] - (u16)coord->coord.t[0];
        d0.vy = (u16)c0->coord.t[1] - (u16)coord->coord.t[1];
        d0.vz = (u16)c0->coord.t[2] - (u16)coord->coord.t[2];
        dist0 = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->targetPos.vx   = c0->coord.t[0];
            work->targetPos.vy   = c0->coord.t[1];
            work->targetPos.vz   = c0->coord.t[2];
            work->targetDistance = dist0;
        } else {
            c1    = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            d1.vx = (u16)c1->coord.t[0] - (u16)coord->coord.t[0];
            d1.vy = (u16)c1->coord.t[1] - (u16)coord->coord.t[1];
            d1.vz = (u16)c1->coord.t[2] - (u16)coord->coord.t[2];
            dist1 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist1 < dist0) {
                work->targetPos.vx = c1->coord.t[0];
                work->targetPos.vy = c1->coord.t[1];
                work->targetPos.vz = c1->coord.t[2];
                dist0              = dist1;
            } else {
                work->targetPos.vx = c0->coord.t[0];
                work->targetPos.vy = c0->coord.t[1];
                work->targetPos.vz = c0->coord.t[2];
            }
            work->targetDistance = dist0;
        }
        VectorNormalSS(&d0, &d0);
        work->playerBearing = (ratan2(d0.vx, d0.vz) - work->rotation.vy) & 0xFFF;
    }
}
/// Flight state of a shot, entered once `func_actor_206100_8014EEC0` has
/// launched it. While the actors run it adds `ACTOR_206100_SHOT_GRAVITY` to the
/// shot's vertical speed, moves the coordinate by
/// `_Actor206100ShotWork::velocity`, then decides whether the shot bursts.
///
/// `hit` is raised when either of the shot's two contacts reports one of the
/// three kinds 1/3/5, or when `worldCollisionResolvePushback` finds the sphere against the
/// room's grid without bit 8 in its mask. Once it is raised - or at
/// `ACTOR_206100_SHOT_LIFETIME` frames - the sphere's grid and pair tests are
/// switched off, the task's state is bumped and the burst is thrown with kind
/// 2 instead of 1. `burstSize` grows before every burst.
static void func_actor_206100_8014B8B4(Task* task)
{
    _Actor206100ShotWork* shot;
    GfxCoord*             coord;
    WorldCollisionDelta   delta;
    s32                   mask;
    s32                   hit;
    s32                   burstKind;
    s32                   i;
    s32                   n;
    s32                   v;

    hit       = 0;
    shot      = task->work;
    coord     = task->extra.tmd->coords;
    burstKind = DIVER_BURST_TRAIL;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        shot->velocity.vy    += ACTOR_206100_SHOT_GRAVITY;
        *&coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[0]    += shot->velocity.vx;
        coord->coord.t[1]    += shot->velocity.vy;
        coord->coord.t[2]    += shot->velocity.vz;
        if (worldCollisionFindContactIndex(shot->contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            for (i = 0; i < ARRAY_SIZE(shot->contacts); i++) {
                switch (shot->contacts[i].key.value & 0xFFFF0000) {
                    case 0x10000:
                    case 0x30000:
                    case 0x50000:
                        hit = 1;
                        break;
                }
            }
        }
        n = worldCollisionResolvePushback(shot->contacts, &delta, ARRAY_SIZE(shot->contacts), &mask);
        if (n < 3) {
            if (n > 0) {
                if ((mask & 8) == 0) {
                    hit = 1;
                }
            }
        }
        worldCollisionClearContacts(shot->contacts);
        if ((++task->killCountdown >= ACTOR_206100_SHOT_LIFETIME) || (hit != 0)) {
            task->killCountdown            = 0;
            shot->strike.attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            burstKind                      = DIVER_BURST_IMPACT;
            task->state                   += 1;
        }
        v = shot->burstSize;
        if (v < ACTOR_206100_SHOT_BURST_SIZE_MAX) {
            shot->burstSize = v + ACTOR_206100_SHOT_BURST_SIZE_STEP;
        } else {
            shot->burstSize = ACTOR_206100_SHOT_BURST_SIZE_MAX;
        }
        _diverImpactBurst(coord, shot->burstPhase, burstKind, shot->burstSize + ACTOR_206100_SHOT_BURST_VARIANT);
    }
}
/// Damage / knock-back tick: walks the six contact records of the actor's
/// `hitContacts` table and turns the first occupied one into a hit.  `hitCooldown` is
/// the cooldown that gates it -- `damageGetPlayerAttackHitCooldown` of the record arms it, and
/// `hit` / `heavy` are the two reactions it leaves in `hitReaction`, the light
/// recoil (1) and the heavy one (2) that the consumer `take_request` maps to the
/// 0x135 and 0x3A0 peaks of `recoilPitch`.  Both are initialised before the loop
/// rather than written as literals at each site: the arms that write
/// `hitReaction` land in different basic blocks, and a literal in each of them is
/// reloaded per block, so only a value that is live from the entry block keeps
/// one register for all of them.  `hit` doubles as the value of the two hit
/// flags `hitTaken` / `wasHit`, which the actor raises on the frame it
/// takes the hit; `hitTaken` also ends the walk of the records.
///
/// While the cooldown reads 0 the record's packed id is rolled through
/// `damageComputePlayerAttack` and `damageRollCriticalHit` -- a successful roll scales the
/// damage and selects the effect kind. The result credits any Life Drain
/// healing through `damageAccumulateLifeDrainHp`, updates the damage readout
/// through `worldTargetAddReadoutAmount`, and is subtracted from the enemy's `hp`.
/// The id's low parameter then picks one of the three flag
/// setters, one of the hit reaction sizes, or clears the hit flag again, and
/// the `0x7F`/`0x8000` pair on an id ending 0x1C forces the light reaction and
/// clears bit 0 of the object's draw flags.  The `else` arm is the same record
/// arriving with the cooldown still up: id parameter 0xD sounds
/// `effectSpawnHit` on the root coordinate's second part alone.
///
/// The tail turns `reactionFlags` into requests the same way -- stagger clears
/// and asks for the heavy reaction, buildup asks for the consumer's
/// sound-and-state pair, and the damage-over-time countdown applies
/// its knock-back and asks for the light one -- and every frame ends by
/// releasing the record table and counting the cooldown down, or clamping it to
/// 0 so it never goes negative.
static void func_actor_206100_8014BAA8(Task* task)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    s32               kind;
    s16               amount;
    s32               dmg;
    s32               tmp;
    s32               tick;
    s32               i;
    s32               hit;
    s32               heavy;

    kind           = 0;
    hit            = 1;
    heavy          = ACTOR_206100_HIT_REACTION_HEAVY;
    work           = task->work;
    enemy          = (Enemy*)task->spawnArg2.pointer;
    work->hitTaken = 0;
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        if ((work->hitContacts[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = hit;
                work->wasHit      = hit;
                dmg               = damageComputePlayerAttack(work->hitContacts[i].key.value, work->targetDistance, 0, 0);
                amount            = dmg;
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[i].key.value);
                if (damageRollCriticalHit(enemy, work->hitContacts[i].key.value, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[i].key.value),
                               &task->extra.tmd->coords[work->targetPart], 0, &work->effectArg);
                if (amount >= 0xB4) {
                    work->hitReaction = heavy;
                } else {
                    work->hitReaction = hit;
                }
                switch (damageGetPlayerAttackReaction(work->hitContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        damageStartEnemyStagger(enemy);
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case 4:
                        work->hitReaction = ACTOR_206100_HIT_REACTION_BLAST;
                        break;
                    case 5:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        work->hitReaction = heavy;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        kind              = 2;
                        work->hitReaction = ACTOR_206100_HIT_REACTION_HEAVY;
                        amount           += amount;
                        break;
                    case 8:
                        work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                        work->hitTaken    = 0;
                        break;
                    case 9:
                        work->hitReaction = hit;
                        break;
                }
                if ((work->hitContacts[i].key.value & 0x7F) == 0x1C && (work->hitContacts[i].key.value & 0x8000) == 0) {
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->hitReaction     = hit;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], 0, 0);
                        break;
                    case 2:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], 2, 0);
                        break;
                }
                damageAccumulateLifeDrainHp(enemy, work->hitContacts[i].key.value, amount, 0);
                worldTargetAddReadoutAmount(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if ((s16)enemy->hp < 0) {
                    enemy->hp = 0;
                }
            } else if ((damageGetPlayerAttackEffectId(work->hitContacts[i].key.value)) == 0xD) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], 0, &work->effectArg);
            }
        }
        if (work->hitTaken != 0) {
            break;
        }
    }
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = ACTOR_206100_HIT_REACTION_HEAVY;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = ACTOR_206100_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tmp  = damageTickEnemyDamageOverTime(enemy);
        tick = (s16)tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            worldTargetAddReadoutAmount(&enemy->node, tick, 0);
            if ((s16)enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = ACTOR_206100_HIT_REACTION_LIGHT;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
    worldCollisionClearContacts(work->hitContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
}
#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_turn_joint.inc.c"

/// Services the animation request in the work block and steps animation slots
/// 1 through 0xE once.  `animRequest` holds the request kind: kind 1 zeroes the
/// clip phase `animFrames` when the clip playing (`animPlaying`) is not the
/// requested one (`animClip`), otherwise hands it to
/// `func_actor_206100_8014F3C8`, and then calls `func_actor_206100_8014F2F0`;
/// kind 2 calls `_diverRestartClip` and zeroes the phase.  Both leave
/// kind 3, which advances the phase by one each call.
static inline void _actor206100AnimUpdate(Task* task)
{
    _Actor206100Work* work;
    s16               kind;
    s32               i;

    work = task->work;
    kind = work->animRequest;
    if (kind == DIVER_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrames = 0;
        } else {
            work->animFrames = func_actor_206100_8014F3C8(task, work->animFrames);
        }
        func_actor_206100_8014F2F0(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (kind == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (kind == DIVER_ANIM_REQUEST_PLAYING) {
        work->animFrames = work->animFrames + 1;
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
}

/// Spawn state of `D_actor_206100_80149E94`: builds the actor's work block --
/// a zeroed `_Actor206100Work` parked straight in `Task::work`, the actor destroyed
/// if that fails -- empties both Bog Diver slots of `D_actor_206100_80158CBC`
/// through their index (the walked-pointer form gives the cooldown field an
/// induction variable of its own) and calls the setup `func_actor_206100_8014AF74`
/// with the block in place.
///
/// It then requests the first clip -- kind 2 in `animRequest`, `animClip` as the
/// clip and `animStep` the step scale -- and services the request at once.
///
/// The tail seeds `field_508`, `field_50A`, `modelScale` and `goalY`, asks for
/// the neck drawn in, places the root coordinate at the goal height, takes the state-0
/// reference `sceneAcquireBattleRef` and re-arms the actor in state 1 with the state
/// and sub-state indices cleared -- the two index pairs written through the two
/// fresh `Task::work` loads, the block-local store shape `func_actor_206100_8014CE60`
/// uses.
static void func_actor_206100_8014C274(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* req;
    _Actor206100Work* state;
    _Actor206100Work* tail;
    Enemy*            enemy;
    GfxCoord*         coord;
    s32               i;

    enemy      = task->spawnArg2.pointer;
    coord      = task->extra.tmd->coords;
    task->work = memCalloc(sizeof(_Actor206100Work), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    D_neo_ark_submarine_gallery_801818B8 = 1;
    work->waterLevel                     = D_neo_ark_submarine_gallery_80181A48;
    for (i = 0; i < ARRAY_SIZE(D_actor_206100_80158CBC); i++) {
        D_actor_206100_80158CBC[i].enemy          = NULL;
        D_actor_206100_80158CBC[i].summonCooldown = 0;
    }
    func_actor_206100_8014AF74(task);
    req              = task->work;
    req->animStep    = 0x10;
    req->animClip    = 3;
    req->animRequest = DIVER_ANIM_REQUEST_RESET;
    _actor206100AnimUpdate(task);
    work->field_508     = 0x1000;
    work->field_50A     = 0x1000;
    work->modelScale    = 0x1EAA;
    work->neckRetracted = 1;
    coord->coord.t[0]   = 0;
    work->goalY         = 0x2710;
    coord->coord.t[1]   = 0x2710;
    coord->coord.t[2]   = 0;
    (sceneAcquireBattleRef)(0);
    state           = task->work;
    task->state     = 1;
    state->state    = 0;
    state->subState = 0;
    tail            = task->work;
    tail->state     = 0;
    tail->subState  = 0;
}

/// The actor's five top-level states, dispatched on `Task::state` by its task
/// callback `func_actor_206100_8014F428`: `func_actor_206100_8014C274` (which
/// builds the work block), `func_actor_206100_8014DA28`,
/// `func_actor_206100_8014C458`, `func_actor_206100_8014E7D4` and the exit
/// `func_actor_206100_8014F490`.
static const TaskFuncTable5 D_actor_206100_80149E5C = {
    {
        func_actor_206100_8014C274,
        func_actor_206100_8014DA28,
        func_actor_206100_8014C458,
        func_actor_206100_8014E7D4,
        func_actor_206100_8014F490,
    },
};

static const TaskFuncTable9 D_actor_206100_80149E70 = {
    {
        func_actor_206100_8014F524,
        func_actor_206100_8014CFF4,
        func_actor_206100_8014D380,
        func_actor_206100_8014D6F4,
        func_actor_206100_8014F59C,
        func_actor_206100_8014F5A4,
        func_actor_206100_8014F5AC,
        func_actor_206100_8014F5B4,
        func_actor_206100_8014F608,
    },
};

/// Push the model's second coordinate's world position onto the scratch stack
/// and hand it to `worldCoordUpdateActorColor`.  The body is `ActorsShared8013a2c0`'s,
/// inlined the way `actorUpdateModelColor` and `actorUpdateModelColor`
/// inline it -- and it has to stay an inlined copy.  Only while expanding an
/// inline body does cc1 keep the scratch head's absolute address folded into
/// the memory operand (`lw $a1,0x1F8003FC` / `sw $a1,0x1F8003FC`, which the
/// assembler expands to the `lui` + `%lo` pair); written out at the call site
/// the same statements materialise the address in a register instead, and the
/// three instructions that costs are the whole difference.
static __inline__ void Actor206100_UpdateColor(Task* task)
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
    worldCoordUpdateActorColor(task->spawnArg2.pointer, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Multiplies `coord`'s rotation by the scale `factors` holds for each axis,
/// `ONE` = 1.0.
static inline void _actor206100ScaleCoord(GfxCoord* coord, VECTOR* factors)
{
    MATRIX    scaling;

    gfxSetRotIdentity(&scaling);
    ScaleMatrix(&scaling, factors);
    MulMatrix(&coord->coord, &scaling);
}

/// Multiplies `coord`'s rotation by the uniform `scale`, `ONE` = 1.0.
static inline void _actor206100ScaleCoordUniform(GfxCoord* coord, s16 scale)
{
    VECTOR factors;

    // The matrix has to be the callee's local: as a second local here its
    // identity words would be stored through this helper's frame base.
    factors.vx = scale;
    factors.vy = factors.vx;
    factors.vz = factors.vx;
    _actor206100ScaleCoord(coord, &factors);
}

/// Rebuilds the root coordinate's rotation from `_Actor206100Work::rotation`:
/// the roll about Z, then the heading about Y.
static inline void _actor206100ApplyRootRotation(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         coord;
    MATRIX            m;
    MATRIX*           dest;

    coord = task->extra.tmd->coords;
    work  = task->work;
    gfxSetRotIdentity(&m);
    RotMatrixZ(work->rotation.vz, &m);
    RotMatrixY(work->rotation.vy, &m);
    dest                = &coord->coord;
    dest->m[0][0]       = m.m[0][0];
    dest->m[0][1]       = m.m[0][1];
    dest->m[0][2]       = m.m[0][2];
    dest->m[1][0]       = m.m[1][0];
    dest->m[1][1]       = m.m[1][1];
    dest->m[1][2]       = m.m[1][2];
    dest->m[2][0]       = m.m[2][0];
    dest->m[2][1]       = m.m[2][1];
    dest->m[2][2]       = m.m[2][2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns a shot's task and hands it a zeroed `_Actor206100ShotWork`: the
/// shot starts `ACTOR_206100_SHOT_MUZZLE_DISTANCE` along the head's Z axis,
/// and the span from the head's origin to the point
/// `ACTOR_206100_SHOT_SPEED` along that axis becomes its `velocity`. A shot
/// whose task or block cannot be had is dropped.
static inline void _actor206100SpawnShot(Task* task)
{
    _Actor206100ShotWork* shot;
    GfxCoord*             head;
    GfxCoord*             shotCoord;
    Task*                 shotTask;
    SVECTOR               pos;
    SVECTOR               base;
    SVECTOR               tip;

    head     = &task->extra.tmd->coords[4];
    shotTask = taskSpawnFromTable(D_actor_206100_80158B0C, 1, 0, 0);
    if (shotTask != NULL) {
        shot = memCalloc(sizeof(_Actor206100ShotWork), 0);
        if (shot == NULL) {
            taskKill(shotTask);
        } else {
            base.vx = 0;
            base.vy = 0;
            base.vz = 0;
            tip.vx  = 0;
            tip.vy  = 0;
            tip.vz  = ACTOR_206100_SHOT_SPEED;
            _actorRenderTransformPointToWorld(head, &base);
            _actorRenderTransformPointToWorld(head, &tip);
            shotTask->work = shot;
            shotCoord      = shotTask->extra.tmd->coords;
            pos.vx         = 0;
            pos.vy         = 0;
            pos.vz         = ACTOR_206100_SHOT_MUZZLE_DISTANCE;
            _actorRenderTransformPointToWorld(head, &pos);
            shotCoord->coord.t[0] = pos.vx;
            shotCoord->coord.t[1] = pos.vy;
            shotCoord->coord.t[2] = pos.vz;
            shot->velocity.vx     = tip.vx - base.vx;
            shot->velocity.vy     = tip.vy - base.vy;
            shot->velocity.vz     = tip.vz - base.vz;
        }
    }
}

/// Depth under the water surface past which the Sea Diver cannot be locked
/// onto.
#define ACTOR_206100_LOCKABLE_DEPTH 0x190

/// Takes the enemy off the lock-on list while model part `part` is more than
/// `ACTOR_206100_LOCKABLE_DEPTH` below `_Actor206100Work::waterLevel`, and
/// puts it back above that.
///
/// The part's origin is carried into world space by the same parent walk as
/// `_actorRenderTransformPointToWorld`. A chain that ends before the view
/// leaves the position at zero, which counts as above the water.
static inline void _actor206100UpdateLockable(Task* task, u8 part)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    GfxCoord*         coord;
    SVECTOR           pos;
    SVECTOR           local;
    VECTOR            result;
    s32               flag;
    SVECTOR*          in;
    SVECTOR*          out;

    in       = &local;
    out      = &pos;
    work     = task->work;
    coord    = &task->extra.tmd->coords[part];
    enemy    = task->spawnArg2.pointer;
    pos.vx   = 0;
    pos.vy   = 0;
    pos.vz   = 0;
    local.vx = 0;
    local.vy = 0;
    local.vz = 0;
    while (1) {
        if (coord->parent == NULL) {
            break;
        }
        if (coord != &gGfxViewCoord) {
            gte_SetTransMatrix(&coord->coord);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(in);
            gte_rtv0tr();
            gte_stlvnl(&result);
            gte_stflg(&flag);
            local.vx = result.vx;
            local.vy = result.vy;
            local.vz = result.vz;
            coord    = coord->parent;
            continue;
        }
        out->vx = local.vx;
        out->vy = local.vy;
        out->vz = local.vz;
        break;
    }
    if (work->waterLevel + ACTOR_206100_LOCKABLE_DEPTH < pos.vy) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        enemy->node.state.parts.flags = 0;
    }
}

/// Fight tick: runs the current fight state, drives the animation, re-poses
/// the root, spawns a requested shot, and gates lock-on by the height of the
/// target part.
static void func_actor_206100_8014C458(Task* task)
{
    _Actor206100Work* work   = task->work;
    GfxCoord*         coord  = task->extra.tmd->coords;
    TmdObject*        obj    = task->extra.tmd;
    Enemy*            enemy  = (Enemy*)task->spawnArg2.pointer;
    TaskFuncTable9    states = D_actor_206100_80149E70;
    _Actor206100Work* next;
    _Actor206100Work* dying;
    _Actor206100Work* sub;
    _Actor206100Work* anim;
    s32               i;
    s32               sound;
    s32               pan;
    s16               state;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase   = work->bobPhase + 1;
            work->frameCount = work->frameCount + 1;
            func_actor_206100_8014B698(task);
            states.funcs[work->state](task);
            sub = task->work;
            if (sub->attackFrames != 0) {
                if ((sub->attackFrames & 7) == 0) {
                    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000B;
                    pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                    sndEvtRequestScriptStart(
                        sound, pan,
                        (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                }
                if (sub->attackFrames == 0x18 || sub->attackFrames == 0x30) {
                    effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, task->extra.tmd->coords + 1, NULL, &sub->effectArg);
                }
                sub->attackFrames--;
            }
            next = task->work;
            if (next->field_534 != 0) {
                next->field_534--;
            }
            anim  = task->work;
            state = anim->animRequest;
            if (state == DIVER_ANIM_REQUEST_BLEND) {
                if (anim->animPlaying != anim->animClip) {
                    anim->animFrames = 0;
                } else {
                    anim->animFrames = func_actor_206100_8014F3C8(task, anim->animFrames);
                }
                func_actor_206100_8014F2F0(task);
                anim->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (state == DIVER_ANIM_REQUEST_RESET) {
                _diverRestartClip(task);
                anim->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                anim->animFrames  = 0;
            } else if (state == DIVER_ANIM_REQUEST_PLAYING) {
                anim->animFrames = anim->animFrames + 1;
            }
            for (i = 1; i < ARRAY_SIZE(anim->rig.slots); i++) {
                animationTickSlot(&anim->rig.anim, i);
            }
            work->animStatus = work->rig.slots[1].status.fields.flags;
            func_actor_206100_8014B0AC(task, work->neckRetracted);
            func_actor_206100_8014E0C0(task);
            func_actor_206100_8014EC54(task);
            func_actor_206100_8014EB60(task);
            _actor206100ApplyRootRotation(task);
            _actor206100ScaleCoordUniform(task->extra.tmd->coords, work->modelScale);
            func_actor_206100_8014BAA8(task);
            if (work->shotRequested != 0) {
                _actor206100SpawnShot(task);
                work->shotRequested = 0;
            }
            if (enemy->hp <= 0) {
                dying           = task->work;
                task->state     = 3;
                dying->state    = 0;
                dying->subState = 0;
            }
            coord->coord.t[1] =
                coord->coord.t[1] + ((work->goalY - coord->coord.t[1]) >> 4);
            enemy->coord = &task->extra.tmd->coords[work->targetPart];
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor206100_UpdateColor(task);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    _actor206100UpdateLockable(task, work->targetPart);
}

/// Teleport: ramps the white-out `func_actor_206100_8014DEAC` fades with, and
/// once it is fully up, hands slot 3 the actor's new position and re-arms the
/// actor on the far side, spawning the screen tint `D_actor_206100_80158CCC`
/// describes as it goes.
///
/// `modulateTexture` is written between `r` and `g`, not in declaration
/// order, and that is load-bearing: it shares the constant 1 with `span`, and
/// that constant and the `%hi` of the global's own address tie in
/// `local-alloc`'s `QTY_CMP_PRI`
/// (`floor_log2 (n_refs) * n_refs * size / span`). The address only wins
/// that tie while the constant's live range runs the whole store run. Cutting
/// it short is what puts the constant in `$v1` and the `%hi` in `$t0`; with
/// `modulateTexture` written last the two swap and the tail no longer
/// schedules the same way.
static void func_actor_206100_8014CB68(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* work2;
    TmdObject*        tmd;
    GfxCoord*         coord;
    ActorTransform    msg;

    work  = task->work;
    tmd   = task->extra.tmd;
    coord = tmd->coords;
    func_actor_206100_8014DEAC(task);
    work->stateFrames = work->stateFrames + 6;
    if (work->stateFrames >= 0x100) {
        work->stateFrames = 0xFF;
    }
    fadeDrawOverlay(work->stateFrames, work->stateFrames, work->stateFrames, GPU_BLEND_SUBTRACT);
    if (work->stateFrames == 0xFF) {
        work->trunkBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->headBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        msg.pos.vx             = 0x690;
        msg.pos.vy             = 0x1388;
        msg.pos.vz             = 0x898;
        msg.rot.vx             = 0;
        msg.rot.vy             = 0x200;
        msg.rot.vz             = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &msg, 0);
        work2                                                      = task->work;
        work2->animStep                                            = 0x10;
        work2->animClip                                            = 3;
        work2->animRequest                                         = DIVER_ANIM_REQUEST_RESET;
        coord->coord.t[1]                                          = 0xDAC;
        work->goalY                                                = 0xDAC;
        coord->coord.t[0]                                          = 0x157C;
        coord->coord.t[2]                                          = 0x157C;
        work->rotation.vx                                          = 0;
        work->rotation.vy                                          = 0xA00;
        work->rotation.vz                                          = 0;
        work->savedView                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
        work->stateFrames                                          = 0;
        work->neckRetracted                                        = 0;
        work->subState                                             = work->subState + 1;
        D_actor_206100_80158CCC.span                               = 1;
        D_actor_206100_80158CCC.scale                              = 0x60;
        D_actor_206100_80158CCC.r                                  = 0x40;
        D_actor_206100_80158CCC.modulateTexture                    = SCREEN_WAVE_MODULATE_TEXTURE;
        D_actor_206100_80158CCC.g                                  = 0x80;
        D_actor_206100_80158CCC.b                                  = 0x80;
        work->waveTask                                             = taskSpawnFromTable(D_actor_206100_80158AF0, 0, 0, &D_actor_206100_80158CCC);
    }
}
static void func_actor_206100_8014CD08(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* work2;
    TmdObject*        tmd;
    GfxCoord*         coord;
    ActorTransform    msg;

    work              = task->work;
    tmd               = task->extra.tmd;
    coord             = tmd->coords;
    work->stateFrames = work->stateFrames + 1;
    if (work->stateFrames == 3) {
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_DEPART, 0, 0);
    }
    if (work->stateFrames == 0x22) {
        coord->coord.t[0]                                          = 0;
        coord->coord.t[2]                                          = 0;
        work->goalY                                                = 0x1388;
        work->stateFrames                                          = 0U;
        coord->coord.t[1]                                          = 0x1B58;
        work->rotation.vy                                          = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        msg.pos.vx = 0x690;
        msg.pos.vy = 0x1388;
        msg.pos.vz = 0x898;
        msg.rot.vx = 0;
        msg.rot.vy = 0xA00;
        msg.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &msg, 0);
        work2              = task->work;
        work2->animStep    = 0x10;
        work2->animClip    = 1;
        work2->animRequest = DIVER_ANIM_REQUEST_RESET;
        work->subState     = work->subState + 1;
        return;
    }
    work->goalY = work->goalY + ((0x1D4C - work->goalY) >> 2);
    _diverStepForward(task, 0x30, work->rotation.vy);
}
/// State handler 4 of `D_actor_206100_80149E94`: clears the fixed-address
/// `D_neo_ark_submarine_gallery_801818B8` flag, ticks the per-state counter `stateFrames` and seeds
/// `D_actor_206100_80158CCC.state` to `SCREEN_WAVE_RAMP_FINISHED` on its first frame.
///
/// Frame 3 retires the child task `func_actor_206100_8014CB68` spawned into
/// `waveTask` and, if the kill left the counter where it was, fires the
/// overlay's sound event 0x551E0003.  Frame 0xC splats the 0x01202148 particle
/// ring -- the same one `func_actor_206100_8014D574` fires, at a radius of
/// 0x1000 and a constant y of -0x294 -- and frame 0x46 hands the actor to state
/// 1 (`func_actor_206100_8014CD08`): it clears the model coordinate's x and z,
/// plays the weapon and 0x3F3 messages under light mode 2, and zeroes the
/// sub-state index along with the new state.
///
/// The frame-0x46 block reads `task->work` again rather than reusing `work`,
/// the fresh load that keeps the pair of stores a block-local quantity -- the
/// same reload `set_state` below makes.
static void func_actor_206100_8014CE60(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    GfxCoord*         coord;
    GfxCoord*         ring;
    SVECTOR           vec;
    s32               i;
    s16               y;
    s16               frame;

    work                                 = task->work;
    D_neo_ark_submarine_gallery_801818B8 = 0;
    coord                                = task->extra.tmd->coords;
    work->stateFrames                    = work->stateFrames + 1;
    if (work->stateFrames == 1) {
        D_actor_206100_80158CCC.state = SCREEN_WAVE_RAMP_FINISHED;
    }
    frame = work->stateFrames;
    if (frame == 3) {
        if (work->waveTask != NULL) {
            taskKill(work->waveTask);
        }
        if (work->stateFrames == frame) {
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_REAPPEAR, 0, 0);
        }
    }
    if (work->stateFrames == 0xC) {
        y    = -0x294;
        i    = 0;
        ring = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            effectSpawn(gRoomEffectWaterSprayId, ring, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if (work->stateFrames == 0x46) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        coord->coord.t[0]                                          = 0;
        coord->coord.t[2]                                          = 0;
        next                                                       = task->work;
        next->state                                                = ACTOR_206100_FIGHT_STATE_ATTACK;
        next->subState                                             = 0;
    }
}
/// Clears `subState` and hands `state` the new state, reloading the work
/// block through the task rather than taking the caller's pointer: the fresh
/// load is what makes `state` a block-local quantity, which is what lets
/// local-alloc hand it `$v0` (see `take_request` below).
static __inline__ void set_state(Task* task, s32 state)
{
    _Actor206100Work* next = task->work;

    next->state    = state;
    next->subState = 0;
}

/// Consumes the pending sub-state request in `hitReaction`, which is only
/// honoured while `hitTaken` reads 1, and returns 1 when it did, so the caller
/// skips this frame's sub-state handler.  Requests 1 and 2 run an animation
/// through `func_actor_206100_8014EB48` (0x135, then the 0x3A0 recovery);
/// 3 and 4 stop the attack loop while keeping its ADSR release settings and move the actor to state 8 and 7 at
/// sub-state 0.  Every arm clears `hitReaction`.
///
/// The arms that only run an animation `break` to the single `return 0` after
/// the switch: that leaves a `return 0` block between the last arm and the
/// join, which dbr later steals into the branch delay slots, while the arms
/// that set a state `return 1` straight past it.  The `s16` return is what
/// makes the flag a halfword value: promoting it in the caller is the
/// `addu $v0,$a0,$zero` at the join, where an `s32` return leaves the flag in
/// `$a0` and tests it there, one instruction short of the target.
static __inline__ s16 take_request(Task* task)
{
    _Actor206100Work* work = task->work;

    if (work->hitTaken == 1) {
        switch (work->hitReaction) {
            case ACTOR_206100_HIT_REACTION_LIGHT:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                func_actor_206100_8014EB48(task, 0x135);
                break;
            case ACTOR_206100_HIT_REACTION_HEAVY:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                func_actor_206100_8014EB48(task, 0x3A0);
                break;
            case ACTOR_206100_HIT_REACTION_STATUS:
                sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                set_state(task, ACTOR_206100_FIGHT_STATE_STATUS_HOLD);
                return 1;
            case ACTOR_206100_HIT_REACTION_BLAST:
                sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                func_actor_206100_8014EB48(task, 0x3A0);
                set_state(task, ACTOR_206100_FIGHT_STATE_RECOIL);
                return 1;
            default:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                break;
        }
    }
    return 0;
}

/// State handler: consumes a pending sub-state request and, when there was
/// none, runs the current sub-state handler.  The request is handled through
/// the inlined `take_request`, so a request that moved the actor to another
/// state skips this frame's handler entirely.
static void func_actor_206100_8014CFF4(Task* task)
{
    _Actor206100Work* sub                 = task->work;
    void              (*states[2])(Task*) = {
        func_actor_206100_8014F6F8,
        func_actor_206100_8014D14C,
    };

    if (take_request(task) == 0) {
        states[sub->subState](task);
    }
}
/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks the per-state
/// counter `stateFrames` and arms `attackFrames` with 0x18 on the first frame, eases
/// `lookPitch` toward 0x400 by a quarter of the remaining distance over frames
/// 0x29..0x4D, fires the overlay's sound events -- 0x551E0002 panned through
/// `worldCoordGetOriginAudioPan` / `worldCoordGetOriginAudioDepth` at 0x54 and plain at 0x77 -- flags the six
/// cue frames, hands state 2 to the actor at sub-state 0 when its `animStatus`
/// says the clip ended, and folds the heading onto the vector from the root coordinate to
/// the target `targetPos`, exactly as
/// `func_actor_206100_8014D380` does but with a step of 0xC and a deadband of
/// 0x18, before handing the actor to `func_actor_206100_8014ED3C` with step
/// 0x10.  The heading fold is that function's, instruction for instruction
/// once the constants are substituted -- including the `yaw` load after the
/// `jal` and the deadband as a variable rather than a literal.
///
/// Three details are load-bearing:
///
/// - the six cue frames are an `||` chain, not a `switch`: GCC 2.8.1 expands a
///   six-case switch over the 0x24-wide range 0x54..0x77 into a jump table
///   (`sltiu` + `jr` off a `.rodata` table, the shape the m2c seed produced),
///   where the chain stays the five `beq` the target has.  The 0x54 and 0x77
///   constants of the two sound blocks above are `CSE`'d into registers the
///   chain's own comparisons then reuse, which is why they live in callee-saved
///   `$s3` / `$s0` across the calls in between.
/// - the reads of `task->work` after the first are separate loads: the
///   clip-ended test and the state change each reload it inside their inlined
///   helper (`_diverClipHasBoundaryOrJump`, `set_state`), and the tail reloads it into
///   `work`.  A single variable assigned in all three places is one pseudo
///   with three definitions, and `global_alloc` homes the whole of it in one
///   callee-saved register -- `$s0` for the flags test and the state change as
///   well as the tail, which is the register only the tail's load crosses
///   calls for.
static void func_actor_206100_8014D14C(Task* task)
{
    _Actor206100Work* sub = task->work;
    _Actor206100Work* work;
    GfxCoord*         coord;
    SVECTOR           vec;
    u16               ease;
    s32               pan;
    s32               angle;
    s32               yaw;
    s32               limit;
    s32               diff;

    sub->stateFrames = sub->stateFrames + 1;
    if (sub->stateFrames == 1) {
        sub->attackFrames = 0x18;
    }
    if ((u32)((u16)sub->stateFrames - 0x29) < 0x25U) {
        ease           = sub->lookPitch;
        sub->lookPitch = ease + ((s32)((0x4000 - (ease * 0x10)) << 0x10) >> 0x16);
    }
    if (sub->stateFrames == 0x54) {
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, pan,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (sub->stateFrames == 0x77) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (sub->stateFrames == 0x54 || sub->stateFrames == 0x5B || sub->stateFrames == 0x62 ||
        sub->stateFrames == 0x69 || sub->stateFrames == 0x70 || sub->stateFrames == 0x77) {
        sub->shotRequested = 1;
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        set_state(task, ACTOR_206100_FIGHT_STATE_DIVE);
    }
    work                = task->work;
    coord               = task->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    vec.vx              = sub->targetPos.vx - (u16)coord->coord.t[0];
    vec.vy              = 0;
    vec.vz              = sub->targetPos.vz - (u16)coord->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    limit = 0x18;
    angle = (u16)work->rotation.vy;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > limit) {
        work->rotation.vy = angle - 0xC;
    } else if (diff < -0x18) {
        work->rotation.vy = angle + 0xC;
    }
    func_actor_206100_8014ED3C(task, 0x10);
}

/// The five sub-state handlers `func_actor_206100_8014F524` picks between: it
/// copies the table onto its stack and calls `funcs[subState]`.
static const TaskFuncTable5 D_actor_206100_80149E94 = {
    {
        func_actor_206100_8014F65C,
        func_actor_206100_8014F69C,
        func_actor_206100_8014CB68,
        func_actor_206100_8014CD08,
        func_actor_206100_8014CE60,
    },
};

/// The state-0 dispatcher's sub-state table, a table in its own right rather
/// than the local array `func_actor_206100_8014CFF4` builds -- the dispatcher
/// copies it whole, which is why the copy is a three-word block move out of
/// `.rodata`.  `func_actor_206100_8014D6F4` has the same body over the sibling
/// table `D_actor_206100_80149EB4`.
static const TaskFuncTable3 D_actor_206100_80149EA8 = {
    {
        func_actor_206100_8014F738,
        func_actor_206100_8014D574,
        func_actor_206100_8014F770,
    },
};

/// The sibling table, immediately after `D_actor_206100_80149EA8` in
/// `.rodata`: the three sub-state handlers `func_actor_206100_8014D6F4`
/// dispatches between, the first `func_actor_206100_8014F7B4` and the third
/// `func_actor_206100_8014F878` bracketing the ring of debris
/// `func_actor_206100_8014D8E8` throws.
static const TaskFuncTable3 D_actor_206100_80149EB4 = {
    {
        func_actor_206100_8014F7B4,
        func_actor_206100_8014D8E8,
        func_actor_206100_8014F878,
    },
};

/// The last object in this unit's `.rodata`, one object after
/// `D_actor_206100_80149EB4` and flush against the unit's first code address:
/// the four steps of the death `func_actor_206100_8014E7D4` dispatches on
/// `_Actor206100Work::state`.
static const TaskFuncTable4 D_actor_206100_80149EC0 = {
    {
        func_actor_206100_8014FBE4,
        func_actor_206100_8014FCD4,
        func_actor_206100_8014E964,
        func_actor_206100_8014FDE8,
    },
};

/// State handler of `D_actor_206100_80149E94`: consumes a pending sub-state
/// request through the inlined `take_request`, and when there was none runs the
/// current sub-state handler from `D_actor_206100_80149EA8` and then steers the
/// actor along its heading.
///
/// The steering half is the same fold `ActorsShared80139c00` makes: clear the
/// root coordinate's `composeStamp`, take the XZ vector from the coordinate to the
/// target `targetPos`, normalise it, and turn `rotation.vy` toward
/// `ratan2` of it by 0x18 a frame while the heading is more than a deadband off.
/// Three details are load-bearing:
///
/// - `yaw` is read from the call before `angle` is loaded, so the angle load
///   sits after the `jal` and never conflicts with the call-clobbered `$a0`;
///   with the load written first it is pushed into a callee-saved register and
///   every later value moves up one.
/// - the deadband is a variable, not the literal 0x28 -- which is what makes
///   `diff`'s comparison a register-register `slt` against a `li`'d `$v1`, the
///   same `li` + `slt` shape `_actor00400TurnTowardPoint`'s deadband parameter forces.
///   Written as a literal the test becomes `slti $a0,0x29` + `bnez` instead:
///   the literal is folded away into `!(diff < 0x29)`, where a register operand
///   reaches `gen_int_relational`'s `reverse_regs` arm and its constant is
///   `force_reg`'d.  Only the first test is affected; the second one, written
///   against the same literal, stays an `slti`.
/// - `rotation.vy` is read through the `u16` view, so the load is `lhu` and its
///   zero-extension folds into the load; the field's own `s16` view is the `lh`
///   `func_actor_206100_8014ED3C` makes.
///
/// It ends by handing the actor to `func_actor_206100_8014ED3C` with step 0x14,
/// the walk that puts the actor back on `prevRootPos`'s XZ when the step ends
/// more than 400 from the origin.
static void func_actor_206100_8014D380(Task* task)
{
    _Actor206100Work* sub    = task->work;
    TaskFuncTable3    states = D_actor_206100_80149EA8;
    _Actor206100Work* work;
    GfxCoord*         coord;
    SVECTOR           vec;
    s32               angle;
    s32               yaw;
    s32               limit;
    s32               diff;

    if (take_request(task) == 0) {
        states.funcs[sub->subState](task);
        work                = task->work;
        coord               = task->extra.tmd->coords;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx              = sub->targetPos.vx - (u16)coord->coord.t[0];
        vec.vy              = 0;
        vec.vz              = sub->targetPos.vz - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->rotation.vy;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->rotation.vy = angle - 0x18;
        } else if (diff < -0x28) {
            work->rotation.vy = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
static void func_actor_206100_8014D574(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         coord;
    SVECTOR           vec;
    s32               sound;
    s32               pan;
    s32               i;
    s16               y;

    work              = task->work;
    work->stateFrames = work->stateFrames + 1;
    if (work->stateFrames < 0x1E) {
        work->lookPitch = work->lookPitch + ((s32) - (work->lookPitch << 0x14) >> 0x15);
        work->lookYaw   = work->lookYaw + ((s32) - (work->lookYaw << 0x14) >> 0x15);
    }
    y = -0x64;
    if (work->stateFrames == 0x1E) {
        i     = 0;
        coord = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            effectSpawn(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
        sound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x551E0006;
        pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateFrames = 0;
        work->goalY       = 0x1E78;
        work->subState    = work->subState + 1;
    }
}
/// State handler of the second table, `D_actor_206100_80149EB4`: consumes a
/// pending sub-state request through the inlined `take_request`, and when there
/// was none runs the current sub-state handler from that table and then steers
/// the actor along its heading, exactly as `func_actor_206100_8014D380` does
/// over `D_actor_206100_80149EA8`.
///
/// The body is that function's instruction for instruction -- the two are the
/// same source shape over different tables, so the prologue's `lui` / `addiu`
/// pair is the only thing that differs between them, and an edit to one belongs
/// in the other.  See `func_actor_206100_8014D380` for what the steering fold
/// does and for why the `yaw` load sits after the `jal` and the deadband is a
/// variable.
static void func_actor_206100_8014D6F4(Task* task)
{
    _Actor206100Work* sub    = task->work;
    TaskFuncTable3    states = D_actor_206100_80149EB4;
    _Actor206100Work* work;
    GfxCoord*         coord;
    SVECTOR           vec;
    s32               angle;
    s32               yaw;
    s32               limit;
    s32               diff;

    if (take_request(task) == 0) {
        states.funcs[sub->subState](task);
        work                = task->work;
        coord               = task->extra.tmd->coords;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx              = sub->targetPos.vx - (u16)coord->coord.t[0];
        vec.vy              = 0;
        vec.vz              = sub->targetPos.vz - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->rotation.vy;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->rotation.vy = angle - 0x18;
        } else if (diff < -0x28) {
            work->rotation.vy = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
/// Sub-state 1 of `func_actor_206100_8014D6F4`'s table: the ring of debris the
/// death throes throw off, and the draw that decides whether the actor
/// teleports out of them.
///
/// On the seventh frame of the sub-state it splats 0x20 effect particles
/// (`effectSpawn` id 0x01202148, the same pair `func_actor_206100_8014D574`
/// fires at frame 0x1E) around the actor's root coordinate -- `rsin` / `rcos`
/// of `i << 7` shifted down by 3, so a ring of radius 0x1000 in 0x20 steps,
/// held at a constant y of -0x3E8.  `y` is a local rather than a literal in
/// the store because the whole ring shares the height, the same local
/// `func_actor_206100_8014D574` hoists.
///
/// From frame 0x1F on it draws from `gRandomLcgState`: the one-in-four that lands
/// on `(state >> 16) & 3 == 0` hands state 2 to the teleport
/// `func_actor_206100_8014CB68` at sub-state 0 -- so the actor leaves the
/// scene it is exploding in -- and the rest restart the counter and advance
/// to the next sub-state.  The state change goes through the inlined
/// `set_state`, which reloads `task->work` instead of reusing `work`: that
/// fresh load is what keeps the pointer a block-local quantity, exactly as in
/// `take_request`.
static void func_actor_206100_8014D8E8(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         coord;
    SVECTOR           vec;
    s32               i;
    s16               y;

    work              = task->work;
    work->stateFrames = work->stateFrames + 1;
    if (work->stateFrames == 7) {
        y     = -0x3E8;
        i     = 0;
        coord = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            effectSpawn(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if (work->stateFrames >= 0x1F) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 0x10) & 3) == 0) {
            set_state(task, ACTOR_206100_FIGHT_STATE_DIVE);
            return;
        }
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}
/// State-2 tick: the `gSceneCombatState.actorControl` effect mode 0 arm bumps the actor's two frame
/// counters and runs the handler `funcs[state]` picks out of a
/// two-entry local table, then drives the animation request and re-poses the
/// actor; mode 1 is that tail alone and mode 2 excludes the model from active
/// drawing. The table's entries are the ring stepper
/// `func_actor_206100_8014FAE4` and the summon tick
/// `func_actor_206100_8014DD3C`, which is the `state` index the spawn state
/// `func_actor_206100_8014C274` leaves at 0.
///
/// The table is two addresses materialised into `$v0`, not a block move out of
/// `.rodata` -- the head of the function's four `lui` / `addiu` / `sw` pairs
/// are the declaration initialiser.  They follow the two `Task` walks because
/// both of those are locals with initialisers as well, and a declaration's
/// initialiser is emitted where the declaration is.
///
/// From the request kind down the body is `func_actor_206100_8014E964`'s word
/// for word: the same `animRequest` re-arm / ramp / reset chain over a second
/// `task->work` load, and the same `for (i = 1; i < 0xF; i++)` slot tick whose
/// initialiser sits *after* the chain for the reason
/// `func_actor_206100_8014FCD4` documents.  The chain is the only reader of
/// `next`; everything else stays on `work`, which is why the two loads exist.
///
/// The nine stores onto the coordinate go through a pointer:
/// through `dest` their address is a register plus a displacement, so the copy
/// is `4($s3)` followed by eight off `$v1`; naming `coord->coord.m[i][j]`
/// instead gives nine distinct sums and no register at all, and every store
/// comes out frame-relative.  `scale` is declared between the two matrices
/// because the frame slots are handed out in declaration order -- matrix /
/// scale / scaling is what puts them at 0x18, 0x38 and 0x48.
static void func_actor_206100_8014DA28(Task* task)
{
    _Actor206100Work* work               = task->work;
    TmdObject*        obj                = task->extra.tmd;
    void              (*funcs[2])(Task*) = {
        func_actor_206100_8014FAE4,
        func_actor_206100_8014DD3C,
    };
    _Actor206100Work* next;
    _Actor206100Work* sub;
    GfxCoord*         coord;
    GfxCoord*         scaled;
    MATRIX*           dest;
    MATRIX            matrix;
    VECTOR            scale;
    MATRIX            scaling;
    s32               i;
    s16               state;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase   = work->bobPhase + 1;
            work->frameCount = work->frameCount + 1;
            funcs[work->state](task);
            next  = task->work;
            state = next->animRequest;
            if (state == DIVER_ANIM_REQUEST_BLEND) {
                if (next->animPlaying != next->animClip) {
                    next->animFrames = 0;
                } else {
                    next->animFrames = func_actor_206100_8014F3C8(task, next->animFrames);
                }
                func_actor_206100_8014F2F0(task);
                next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (state == DIVER_ANIM_REQUEST_RESET) {
                _diverRestartClip(task);
                next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                next->animFrames  = 0;
            } else if (state == DIVER_ANIM_REQUEST_PLAYING) {
                next->animFrames = next->animFrames + 1;
            }
            for (i = 1; i < ARRAY_SIZE(next->rig.slots); i++) {
                animationTickSlot(&next->rig.anim, i);
            }
            work->animStatus = work->rig.slots[1].status.fields.flags;
            func_actor_206100_8014B0AC(task, work->neckRetracted);
            coord = task->extra.tmd->coords;
            sub   = task->work;
            gfxSetRotIdentity(&matrix);
            RotMatrixZ(sub->rotation.vz, &matrix);
            RotMatrixY(sub->rotation.vy, &matrix);
            dest                = &coord->coord;
            dest->m[0][0]       = matrix.m[0][0];
            dest->m[0][1]       = matrix.m[0][1];
            dest->m[0][2]       = matrix.m[0][2];
            dest->m[1][0]       = matrix.m[1][0];
            dest->m[1][1]       = matrix.m[1][1];
            dest->m[1][2]       = matrix.m[1][2];
            dest->m[2][0]       = matrix.m[2][0];
            dest->m[2][1]       = matrix.m[2][1];
            dest->m[2][2]       = matrix.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            scaled              = task->extra.tmd->coords;
            scale.vx            = work->modelScale;
            scale.vy            = scale.vx;
            scale.vz            = scale.vx;
            gfxSetRotIdentity(&scaling);
            ScaleMatrix(&scaling, &scale);
            MulMatrix(&scaled->coord, &scaling);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor206100_UpdateColor(task);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
/// Summon tick: holds the per-state counter at the explosion frame and then
/// fills and retires the actor's two Bog Diver slots (see the header for the
/// full walk -- each slot is handled on its own, and the fifth release moves
/// the actor to state 2).
///
/// Both slots are reached by *index* rather than through a walking pointer, and
/// that is what the target's register file depends on: a walked pointer makes
/// `timer`'s read and write two identical `DEST_ADDR` givs on the same biv,
/// which `combine_givs` merges into one that survives `strength_reduce`'s
/// "worth while" test -- so the second field is given an induction variable of
/// its own, `$s1` goes to it instead of to `work`, and the frame grows by a
/// slot.  Indexed, the two `timer` accesses are displacements off the address
/// register strength reduction builds for `enemy`, and neither is reduced.
/// See `DECOMPILATION_LEARNINGS.md`, "A walked pointer's second field becomes a
/// second induction variable".
///
/// The two loops carry their own counters for the same reason: one variable
/// used by both is a single pseudo whose live range spans both loops, so
/// local-alloc has to home it in a callee-saved register for the whole
/// function, where the target's second loop counts in `$a0`.
static void func_actor_206100_8014DD3C(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    Enemy*            enemy;
    s32               i;
    s32               j;
    u8                count;

    work = task->work;
    if (work->stateFrames == 0x1E) {
        sceneEngageBattle(1);
    } else {
        work->stateFrames = work->stateFrames + 1;
    }
    func_actor_206100_8014DEAC(task);
    i = 0;
    do {
        if (work->bogDiversSpawned < 5 && D_actor_206100_80158CBC[i].enemy == NULL) {
            if (D_actor_206100_80158CBC[i].summonCooldown == 0) {
                enemy = func_actor_206100_8014EE2C(work->bogDiversSpawned);
                if (enemy != NULL) {
                    D_actor_206100_80158CBC[i].enemy = enemy;
                    enemy->hp                        = 1;
                    work->bogDiversSpawned           = work->bogDiversSpawned + 1;
                }
            } else {
                D_actor_206100_80158CBC[i].summonCooldown = D_actor_206100_80158CBC[i].summonCooldown - 1;
            }
        }
        i++;
    } while (i < ARRAY_SIZE(D_actor_206100_80158CBC));
    j = 0;
    do {
        if (D_actor_206100_80158CBC[j].enemy != NULL &&
            D_actor_206100_80158CBC[j].enemy->hp <= 0) {
            D_actor_206100_80158CBC[j].enemy          = NULL;
            D_actor_206100_80158CBC[j].summonCooldown = ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN;
            count                                     = work->bogDiversKilled + 1;
            work->bogDiversKilled                     = count;
            if (count >= 5) {
                next           = task->work;
                task->state    = 2;
                next->state    = ACTOR_206100_FIGHT_STATE_ENTRANCE;
                next->subState = 0;
            }
        }
        j++;
    } while (j < ARRAY_SIZE(D_actor_206100_80158CBC));
}
/// Companion tick every state runs: while the flag `rolling` is up it ramps
/// the roll `rotation.vz` by 0x20 a frame and clears the flag once the ramp lands
/// on a 0x1000 boundary, and it walks the actor along the eight-vertex ring
/// `waypoints` the index `waypointIndex` points into.  Reaching the vertex (the
/// planar distance below 0x3E8) advances `waypointIndex` modulo 8 and the ring-step
/// counter `waypointsSinceRoll`, which resets after its sixth step and re-arms the roll;
/// otherwise it steers the yaw `rotation.vy` toward the vertex by 0x2C a frame
/// and hands the actor to `_diverStepForward` for a 0x40 step.
///
/// The three diffs are written into the `delta` `SVECTOR` although only `vx`
/// and `vz` are read back -- the distance is planar, so `vy` is dead.  That
/// dead member is load-bearing: as scalar locals the three stay in registers
/// and their stores disappear, and it is the store boundaries of the *stack*
/// form that the target's four recomputed ring addresses hang off.
///
/// The else arm reads the ring through the `ring` / `index` pair instead of a
/// pointer to the entry.  With both operands in registers the address is a
/// register `plus` that survives the two `vec` stores, so the ring index and
/// base are loaded once; a pointer to the entry leaves `lbu waypointIndex` /
/// `lw waypoints` inside the address expression, which any store invalidates,
/// and it also puts the base in the `addu`'s first operand.  See
/// `DECOMPILATION_LEARNINGS.md`, "Array index vs intermediate pointer for
/// `addu` operand order".
static void func_actor_206100_8014DEAC(Task* task)
{
    _Actor206100Work* sub = task->work;
    _Actor206100Work* work;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    SVECTOR*          ring;
    u32               index;
    SVECTOR           delta;
    SVECTOR           vec;
    u16               roll;
    u8                count;
    s32               angle;
    s32               yaw;
    s32               diff;
    s32               limit;

    coord = task->extra.tmd->coords;
    if (sub->waypointsSinceRoll == 0) {
        sub->rolling = 1;
    }
    if (sub->rolling == 1) {
        roll             = sub->rotation.vz + 0x20;
        sub->rotation.vz = roll;
        if ((roll & 0xFFF) == 0) {
            sub->rolling = 0;
        }
    }
    delta.vx   = sub->waypoints[sub->waypointIndex].vx - (u16)coord->coord.t[0];
    delta.vy   = sub->waypoints[sub->waypointIndex].vy - (u16)coord->coord.t[1];
    delta.vz   = sub->waypoints[sub->waypointIndex].vz - (u16)coord->coord.t[2];
    sub->goalY = sub->waypoints[sub->waypointIndex].vy;
    if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) < 0x3E8) {
        sub->waypointIndex      = (sub->waypointIndex + 1) & 7;
        count                   = sub->waypointsSinceRoll + 1;
        sub->waypointsSinceRoll = count;
        if (count >= 6) {
            sub->waypointsSinceRoll = 0;
        }
    } else {
        ring                 = sub->waypoints;
        index                = sub->waypointIndex;
        work                 = task->work;
        coord2               = task->extra.tmd->coords;
        coord2->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx               = ring[index].vx - (u16)coord2->coord.t[0];
        vec.vy               = 0;
        vec.vz               = ring[index].vz - (u16)coord2->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x100;
        angle = (u16)work->rotation.vy;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->rotation.vy = angle - 0x2C;
        } else if (diff < -0x100) {
            work->rotation.vy = angle + 0x2C;
        }
        _diverStepForward(task, 0x40, sub->rotation.vy);
    }
}
/// Recoil tick: `func_actor_206100_8014EB48` arms `recoilPhase` to 1 with the
/// angle it wants in `recoilPeak`, and this walks the three phases it takes to
/// get there -- voice the hit, ramp `recoilPitch` half the remaining distance
/// each frame and advance once it is within 0x20 of the peak, then step the
/// pitch down by 0x10 a frame until it is back at zero, where the pitch snaps
/// to 0 and the phase to the idle below.
///
/// Phase 0 is that idle, and what a freshly spawned (zeroed) block sits in: the
/// leftover pitch decays by an eighth of itself each frame.
///
/// The decay reads `recoilPitch` twice and lands as an `lh` and an `lhu` pair,
/// with nothing in the C saying so: the multiplicand's sign is needed (it feeds
/// the shift) while the addend's high bits are dead, the sum going straight back
/// through `sh` into the same halfword.  The ramp in case 2, whose operands do
/// need full width, is where the `(u16)` casts are load-bearing.
static void func_actor_206100_8014E0C0(Task* task)
{
    _Actor206100Work* work;
    s32               sound;
    s32               pan;
    s16               phase;

    work = task->work;
    switch (work->recoilPhase) {
        case 0:
            work->recoilPitch = work->recoilPitch + ((s32) - (work->recoilPitch * 0x10) >> 7);
            break;
        case 1:
            sound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
            pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->recoilPhase = work->recoilPhase + 1;
            break;
        case 2:
            phase             = (u16)work->recoilPitch + ((s32)(((u16)work->recoilPeak - (u16)work->recoilPitch) << 0x14) >> 0x15);
            work->recoilPitch = phase;
            if (phase < work->recoilPeak - 0x20) {
                break;
            }
            work->recoilPhase = work->recoilPhase + 1;
            break;
        case 3:
            phase             = (u16)work->recoilPitch - 0x10;
            work->recoilPitch = phase;
            if ((phase << 0x10) <= 0) {
                work->recoilPhase = 0;
                work->recoilPitch = 0;
            }
            break;
    }
}
static void func_actor_206100_8014E228(Task* task)
{
    SVECTOR           ang;
    SVECTOR*          aim;
    SVECTOR           rot1;
    SVECTOR           rot2;
    MATRIX            t1;
    MATRIX            t2;
    MATRIX            t3;
    MATRIX            ma;
    MATRIX            mb;
    MATRIX            mc;
    MATRIX            view;
    VECTOR            delta;
    VECTOR            local;
    GfxCoord*         c1;
    GfxCoord*         c3;
    GfxCoord*         c4;
    _Actor206100Work* work;
    GfxCoord*         base;
    GfxCoord*         c2;
    MATRIX*           m2;
    MATRIX*           m3;
    MATRIX*           dest;
    s32               hx;
    s32               hy;
    s32               hz;
    s32               total;
    u16               yaw;
    u16               pitch;
    u32               pitchDiff;
    s16               limit;

    base = task->extra.tmd->coords;
    work = task->work;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &c4->workm, &view);
    delta.vx = work->targetPos.vx - view.t[0];
    delta.vy = work->targetPos.vy - (view.t[1] + 0x100);
    delta.vz = work->targetPos.vz - view.t[2];
    ApplyTransposeMatrixLV(&base->coord, &delta, &local);
    aim             = &ang;
    aim->vx         = (ratan2(-local.vy, local.vz) << 20) >> 20;
    aim->vy         = (ratan2(local.vx, local.vz) << 20) >> 20;
    aim->vz         = 0;
    hx              = (s16)work->lookPitch;
    hy              = (s16)work->lookYaw;
    hz              = (s16)work->lookRoll;
    work->lookPitch = hx;
    work->lookYaw   = hy;
    work->lookRoll  = hz;
    if ((u16)(ang.vy + 0x3FF) < 0x7FF) {
        if ((u32)(((s16)ang.vy - (s16)work->lookYaw) + 0x20) >= 0x41) {
            work->lookYaw = ((s16)work->lookYaw < ang.vy) ? work->lookYaw + 0x18
                                                          : work->lookYaw - 0x18;
        }
        yaw = ang.vx;
        if ((u16)(yaw + 0x3FF) < 0x7FF) {
            pitchDiff = ((s16)yaw - (s16)work->lookPitch) + 0x20;
            pitch     = work->lookPitch;
            if (pitchDiff >= 0x41) {
                work->lookPitch = pitch + ((s32)(((u16)yaw - pitch) << 20) >> 23);
            }
        }
    } else {
        work->lookYaw   = work->lookYaw + ((s32) - (s16)(work->lookYaw * 0x10) >> 8);
        work->lookPitch = work->lookPitch + ((s32) - (s16)(work->lookPitch * 0x10) >> 8);
    }
    m2 = &c2->coord;

    gfxSetRotIdentity(&ma);
    gfxSetRotIdentity(&mb);
    gfxSetRotIdentity(&mc);

    gfxExtractEulerAngles(m2, &rot1);
    m3 = &c3->coord;
    gfxExtractEulerAngles(&c3->coord, &rot2);
    RotMatrixX(rot1.vx + (s16)(work->recoilPitch / 3), &ma);
    RotMatrixX(rot2.vx + (s16)(work->recoilPitch / 3), &mb);
    c2->coord.m[0][0] = ma.m[0][0];
    m2->m[0][1]       = ma.m[0][1];
    m2->m[0][2]       = ma.m[0][2];
    m2->m[1][0]       = ma.m[1][0];
    m2->m[1][1]       = ma.m[1][1];
    m2->m[1][2]       = ma.m[1][2];
    m2->m[2][0]       = ma.m[2][0];
    m2->m[2][1]       = ma.m[2][1];
    m2->m[2][2]       = ma.m[2][2];
    c3->coord.m[0][0] = mb.m[0][0];
    m3->m[0][1]       = mb.m[0][1];
    m3->m[0][2]       = mb.m[0][2];
    m3->m[1][0]       = mb.m[1][0];
    m3->m[1][1]       = mb.m[1][1];
    m3->m[1][2]       = mb.m[1][2];
    m3->m[2][0]       = mb.m[2][0];
    m3->m[2][1]       = mb.m[2][1];
    m3->m[2][2]       = mb.m[2][2];
    actorRenderComposeCoord(c1);
    actorRenderComposeCoord(c2);
    actorRenderComposeCoord(c3);
    _diverTurnJoint(c2, (s16)work->lookYaw / 3);
    _diverTurnJoint(c3, (s16)work->lookYaw / 3);

    gfxSetRotIdentity(&mc);

    total = (s16)work->lookPitch + work->recoilPitch;
    limit = total;
    if (limit >= 0x300) {
        limit = 0x300;
    } else if (limit < -0x200) {
        limit = -0x200;
    }
    RotMatrixX((s32)limit, &mc);
    RotMatrixY((s16)((s16)work->lookYaw / 3), &mc);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(&c2->coord, &t2);
    TransposeMatrix(&c3->coord, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc);
    dest             = &c4->coord;
    dest->m[0][0]    = t1.m[0][0];
    dest->m[0][1]    = t1.m[0][1];
    dest->m[0][2]    = t1.m[0][2];
    dest->m[1][0]    = t1.m[1][0];
    dest->m[1][1]    = t1.m[1][1];
    dest->m[1][2]    = t1.m[1][2];
    dest->m[2][0]    = t1.m[2][0];
    dest->m[2][1]    = t1.m[2][1];
    dest->m[2][2]    = t1.m[2][2];
    c4->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(c4);
}

/// Effect-mode tick of the `state` state table `D_actor_206100_80149EC0`,
/// keyed on `gSceneCombatState.actorControl`. Mode 2 only excludes the model from active drawing and
/// leaves; mode 0 runs the handler `state` selects, latches the animation
/// slot's flags into `animStatus` and eases the root coordinate -- x and z to a
/// sixteenth of their distance to zero, y the same fraction of the way to the
/// height `goalY` -- before falling into the shared tail; mode 1 is that
/// tail alone.
///
/// The table is copied onto the stack first, the same local jump table
/// `func_actor_206100_8014F524` builds, which is what the prologue's four-word
/// block move out of `.rodata` is.  The tail is `Actor206100_UpdateColor`; see
/// there for why it stays inline.
static void func_actor_206100_8014E7D4(Task* task)
{
    _Actor206100Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    TaskFuncTable4    states;

    work   = task->work;
    obj    = task->extra.tmd;
    coord  = obj->coords;
    states = D_actor_206100_80149EC0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            states.funcs[work->state](task);
            work->animStatus  = work->rig.slots[1].status.fields.flags;
            coord->coord.t[0] = coord->coord.t[0] + (-coord->coord.t[0] >> 4);
            coord->coord.t[2] = coord->coord.t[2] + (-coord->coord.t[2] >> 4);
            coord->coord.t[1] =
                coord->coord.t[1] + ((work->goalY - coord->coord.t[1]) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor206100_UpdateColor(task);
            return;
    }
}
/// Teleport-state tick: the same animation request chain
/// `func_actor_206100_8014FCD4` runs -- re-arm, ramp or reset the clip phase
/// `animFrames` and tick every slot -- but on its own frame counter, and with the
/// tail this actor needs instead of that one's: `stateFrames` reaching 0x32
/// rewinds it and steps the state index `state`, the frame the sub-state
/// table walks to pick the next handler.
///
/// `coord` is the actor's root coordinate, cleared so the composition pass rebuilds it --
/// the same dereference-store local `func_actor_206100_8014FDE8` binds.
///
/// The loop initialiser sits after the sub-state chain for the reason
/// `func_actor_206100_8014FCD4` documents: ahead of it the store that
/// materialises `i` shares a block with the case-3 increment, post-reload CSE
/// folds that increment's `+ 1` into `+ $s0`, and the phase is written with
/// `addu`.  Here the branch targets the initialiser instead.
static void func_actor_206100_8014E964(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    GfxCoord*         coord;
    s32               i;
    s16               state;

    work              = task->work;
    coord             = task->extra.tmd->coords;
    work->stateFrames = work->stateFrames + 1;
    next              = task->work;
    state             = next->animRequest;
    if (state == DIVER_ANIM_REQUEST_BLEND) {
        if (next->animPlaying != next->animClip) {
            next->animFrames = 0;
        } else {
            next->animFrames = func_actor_206100_8014F3C8(task, next->animFrames);
        }
        func_actor_206100_8014F2F0(task);
        next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (state == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(task);
        next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        next->animFrames  = 0;
    } else if (state == DIVER_ANIM_REQUEST_PLAYING) {
        next->animFrames = next->animFrames + 1;
    }
    for (i = 1; i < ARRAY_SIZE(next->rig.slots); i++) {
        animationTickSlot(&next->rig.anim, i);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateFrames >= 0x32) {
        work->stateFrames = 0;
        work->state       = work->state + 1;
    }
}
#include "../../shared/diver_step_forward.inc.c"

static void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    _Actor206100Work* work = task->work;

    work->part5Phase  = 1;
    work->recoilPhase = 1;
    work->recoilPeak  = arg1;
}

static void func_actor_206100_8014EB60(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         coords;
    SVECTOR           rot;
    MATRIX            matrix;
    MATRIX*           dest;

    work   = task->work;
    coords = task->extra.tmd->coords;
    dest   = &coords[5].coord;

    gfxSetRotIdentity(&matrix);

    gfxExtractEulerAngles(dest, &rot);
    rot.vx += work->part5Pitch;
    RotMatrix(&rot, &matrix);
    dest->m[0][0] = matrix.m[0][0];
    dest->m[0][1] = matrix.m[0][1];
    dest->m[0][2] = matrix.m[0][2];
    dest->m[1][0] = matrix.m[1][0];
    dest->m[1][1] = matrix.m[1][1];
    dest->m[1][2] = matrix.m[1][2];
    dest->m[2][0] = matrix.m[2][0];
    dest->m[2][1] = matrix.m[2][1];
    dest->m[2][2] = matrix.m[2][2];
}

static void func_actor_206100_8014EC54(Task* task)
{
    _Actor206100Work* work = task->work;
    s16               value;
    s16               angle;

    switch (work->part5Phase) {
        case 0:
            work->part5Pitch = (u16)work->part5Pitch + ((-(work->part5Pitch * 0x10)) >> 7);
            return;
        case 1:
            work->part5Phase = 2;
            return;
        case 2:
            value            = (u16)work->part5Pitch + ((-0x3000 - work->part5Pitch * 0x10) >> 6);
            work->part5Pitch = value;
            if (value < -0x2DF) {
                work->part5Phase++;
                return;
            }
            return;
        case 3:
            angle            = (u16)work->part5Pitch + 0x1C;
            work->part5Pitch = angle;
            if (angle >= 0) {
                work->part5Phase = 0;
            }
            break;
    }
}
static void func_actor_206100_8014ED3C(Task* task, s16 arg1)
{
    _Actor206100Work*                  work;
    GfxCoord*                          coord;
    _Actor206100OriginDistanceScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor206100OriginDistanceScratch);
    work    = task->work;
    coord   = task->extra.tmd->coords;
    _diverStepForward(task, arg1, work->rotation.vy);
    scratch->toOrigin.vx = -(u16)coord->coord.t[0];
    scratch->toOrigin.vz = -(u16)coord->coord.t[2];
    scratch->distance    = SquareRoot0(scratch->toOrigin.vx * scratch->toOrigin.vx +
                                       scratch->toOrigin.vz * scratch->toOrigin.vz);
    if (scratch->distance > ACTOR_206100_FIGHT_AREA_RADIUS) {
        coord->coord.t[0] = work->prevRootPos.vx;
        coord->coord.t[2] = work->prevRootPos.vz;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor206100OriginDistanceScratch);
}

static Enemy* func_actor_206100_8014EE2C(s32 arg0)
{
    Enemy*     enemy;
    TmdObject* obj;

    enemy = enemySpawnFromTable(&D_actor_100400_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->placeKey        = arg0 << ENEMY_PLACE_INDEX_SHIFT;
        enemy->place           = &D_actor_206100_80155134[(s16)arg0];
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdBuildBufferHalf(obj);
        tmdBuildBufferHalf(obj);
        return enemy;
    }
    return NULL;
}

static void func_actor_206100_8014EEC0(Task* task)
{
    _Actor206100ShotWork*  shot;
    WorldCollisionContact* contacts;
    GfxCoord*              coord;

    shot                                     = task->work;
    coord                                    = task->extra.tmd->coords;
    task->killCountdown                      = 0;
    shot->burstSize                          = ACTOR_206100_SHOT_BURST_SIZE_STEP;
    shot->burstPhase                         = 0;
    coord->parent                            = &gGfxViewCoord;
    coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
    shot->strike.attackBody.key              = damagePackAttackKey(&D_actor_206100_80155194, 0);
    shot->strike.attackBody.coord            = task->extra.tmd->coords;
    contacts                                 = shot->contacts;
    shot->strike.attackBody.context.contacts = contacts;
    shot->strike.attackBody.pos.vx           = 0;
    shot->strike.attackBody.pos.vy           = 0;
    shot->strike.attackBody.pos.vz           = 0;
    shot->strike.attackBody.radius           = 0x140;
    shot->strike.attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &shot->strike.attackBody);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(shot->contacts), 0);
    shot->strike.attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actorRenderComposeCoord(coord);
    _diverImpactBurst(coord, (u16)shot->burstPhase, DIVER_BURST_LAUNCH, shot->burstSize + ACTOR_206100_SHOT_BURST_VARIANT);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

#include "../../shared/coord_math_local_to_world.inc.c"

/// A shot's task callback: runs its current state handler out of
/// `D_actor_206100_80149E24`, copying the table onto the stack first.
void func_actor_206100_8014F134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_206100_80149E24;
    sp.funcs[task->state](task);
}

static void func_actor_206100_8014F18C(Task* task)
{
    _Actor206100Work* work;

    work = task->work;

    work->trunkBody.coord            = &task->extra.tmd->coords[1];
    work->trunkBody.context.contacts = work->hitContacts;
    work->trunkBody.pos.vx           = 0;
    work->trunkBody.pos.vy           = 0;
    work->trunkBody.pos.vz           = 0;
    work->trunkBody.key              = 0x3003D;
    work->trunkBody.radius           = 0x400;
    work->trunkBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->trunkBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->trunkBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->headBody.coord            = &task->extra.tmd->coords[4];
    work->headBody.context.contacts = work->hitContacts;
    work->headBody.pos.vx           = 0;
    work->headBody.pos.vy           = 0;
    work->headBody.pos.vz           = 0;
    work->headBody.key              = 0x3003D;
    work->headBody.radius           = 0x200;
    work->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headBody);
    work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Re-arms every animation slot for the pending request: writes the request's
/// step scale (`animStep`) into each slot's `rate` and re-seeks the slot to
/// the requested clip with `animationSeekSlotWithBlend`, whose fifth argument is the request's
/// own value at `animBlend`.  When the clip already playing (`animPlaying`) is not
/// the one requested, `animBlend` is cleared as well.  `animPlaying` latches the
/// clip either way, which is what lets the next frame tell the two cases apart.
/// The call sits *inside* the loop and takes a fresh `work` in `$a0` each
/// iteration, the same shape `func_actor_405800_80138294` has.
static void func_actor_206100_8014F2F0(Task* arg0)
{
    _Actor206100Work* work;
    s32               i;

    work = arg0->work;
    if (work->animPlaying == work->animClip) {
        i = 1;
        do {
            work->rig.slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < ARRAY_SIZE(work->rig.slots));
    } else {
        i = 1;
        do {
            work->rig.slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < ARRAY_SIZE(work->rig.slots));
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}
static s16 func_actor_206100_8014F3C8(Task* arg0, s16 arg1)
{
    _Actor206100Work* work = arg0->work;

    if (work->animStep == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->animStep << 12) >> 16;
}

/// The actor's task callback: runs its current top-level state out of
/// `D_actor_206100_80149E5C`, copying the table onto the stack first.
void func_actor_206100_8014F428(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_actor_206100_80149E5C;
    sp.funcs[task->state](task);
}

static void func_actor_206100_8014F490(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

static void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

/// Runs the actor's sub-state handler for the current `subState`, after
/// marking the enemy's list node so the exit path tears the actor down.
static void func_actor_206100_8014F524(Task* task)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    TaskFuncTable5    sp;

    work                          = task->work;
    enemy                         = (Enemy*)task->spawnArg2.pointer;
    sp                            = D_actor_206100_80149E94;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    sp.funcs[work->subState](task);
}

static void func_actor_206100_8014F59C(Task* task)
{
}

static void func_actor_206100_8014F5A4(Task* task)
{
}

static void func_actor_206100_8014F5AC(Task* task)
{
}

static void func_actor_206100_8014F5B4(Task* task)
{
    _Actor206100Work* work                = task->work;
    void              (*states[2])(Task*) = {
        _diverEnterRecoil,
        func_actor_206100_8014F970,
    };

    states[work->subState](task);
}

static void func_actor_206100_8014F608(Task* task)
{
    _Actor206100Work* work                = task->work;
    void              (*states[2])(Task*) = {
        func_actor_206100_8014F9C4,
        func_actor_206100_8014FA08,
    };

    states[work->subState](task);
}

static void func_actor_206100_8014F65C(Task* task)
{
    _Actor206100Work* work = task->work;

    func_actor_206100_8014DEAC(task);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F69C(Task* task)
{
    u16               timer;
    _Actor206100Work* work = task->work;

    func_actor_206100_8014DEAC(task);
    timer             = work->stateFrames + 1;
    work->stateFrames = timer;
    if ((s16)timer >= 0x5A) {
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}

static void func_actor_206100_8014F6F8(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* anim;

    work              = task->work;
    work->stateFrames = 0;
    anim              = task->work;
    anim->animBlend   = 8;
    anim->animStep    = 8;
    anim->animClip    = 7;
    anim->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F738(Task* task)
{
    _Actor206100Work* work = task->work;

    work->animBlend   = 0xA;
    work->animStep    = 0x10;
    work->animClip    = 1;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F770(Task* task)
{
    u16               timer;
    _Actor206100Work* work;
    _Actor206100Work* next;

    work              = task->work;
    timer             = work->stateFrames + 1;
    work->stateFrames = timer;
    if ((s16)timer >= 0x5B) {
        next           = task->work;
        next->state    = ACTOR_206100_FIGHT_STATE_SURFACE;
        next->subState = 0;
    }
}

static void func_actor_206100_8014F7B4(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    s32               soundId;
    s32               pan;

    work    = task->work;
    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x551E0005;
    pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    next              = task->work;
    next->animBlend   = 0xA;
    next->animStep    = 0x10;
    next->animClip    = 1;
    next->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->goalY       = 0x1388;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F878(Task* task)
{
    u16               timer;
    _Actor206100Work* work;
    _Actor206100Work* next;

    work              = task->work;
    timer             = work->stateFrames + 1;
    work->stateFrames = timer;
    if ((s16)timer >= 0x3D) {
        next           = task->work;
        next->state    = ACTOR_206100_FIGHT_STATE_ATTACK;
        next->subState = 0;
    }
}

#include "../../shared/diver_state7_enter.inc.c"

static void func_actor_206100_8014F970(Task* task)
{
    _Actor206100Work* work;

    if (_diverClipHasBoundaryOrJump(task)) {
        work           = task->work;
        work->state    = ACTOR_206100_FIGHT_STATE_DIVE;
        work->subState = 0;
    }
}

static void func_actor_206100_8014F9C4(Task* task)
{
    _Actor206100Work* work = task->work;

    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 0xE;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->goalY       = work->waterLevel;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014FA08(Task* task)
{
    u16               timer;
    _Actor206100Work* work;
    _Actor206100Work* next;

    work              = task->work;
    timer             = work->stateFrames + 1;
    work->stateFrames = timer;
    if ((s16)timer >= 0x1F) {
        work->targetPart = 1;
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        next              = task->work;
        next->animBlend   = 8;
        next->animStep    = 8;
        next->animClip    = 0x10;
        next->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        work->targetPart = 4;
        next             = task->work;
        next->state      = ACTOR_206100_FIGHT_STATE_ATTACK;
        next->subState   = 0;
    }
}
/// Ring-spawn state: seeds `waypoints` and `waypointIndex` from the eight-point ring
/// `D_actor_206100_80158B68`, copies the current vertex into the root part
/// coordinate, advances the index modulo 8, and hands the actor the state-1
/// animation request.  `coord` is the coordinate the effect argument at
/// `effectArg` shares, so moving it moves the actor.
///
/// `enemy` is a local rather than the inline
/// `((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;` because the fused form
/// transposes the `spawnArg2` and `task->extra` loads; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store, so give the pointer its own local".
static void func_actor_206100_8014FAE4(Task* task)
{
    GfxCoord*         coord;
    _Actor206100Work* work;
    _Actor206100Work* next;
    _Actor206100Work* last;
    Enemy*            enemy;

    work                          = task->work;
    enemy                         = (Enemy*)task->spawnArg2.pointer;
    coord                         = task->extra.tmd->coords;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    work->neckRetracted           = 1;
    work->waypointIndex           = 0;
    work->waypoints               = D_actor_206100_80158B68;
    next                          = task->work;
    next->animStep                = 0x10;
    next->animClip                = 3;
    next->animRequest             = DIVER_ANIM_REQUEST_RESET;
    work->rotation.vy             = 0x400;
    coord->coord.t[0]             = work->waypoints[work->waypointIndex].vx;
    coord->coord.t[1]             = work->waypoints[work->waypointIndex].vy;
    coord->coord.t[2]             = work->waypoints[work->waypointIndex].vz;
    work->waypointIndex           = (work->waypointIndex + 1) & 7;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->stateFrames = 0;
    last              = task->work;
    last->state       = 1;
    last->subState    = 0;
}

static void func_actor_206100_8014FBE4(Task* task)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    s32               soundId;
    s32               pan;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    Gp_ApplyAreaRecs(D_neo_ark_submarine_gallery_8018590C);
    work->goalY = work->waterLevel;
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    gameFlagSetNibble(GAME_FLAG_0F3, 1);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->trunkBody);
    worldCollisionUnlinkBody(&work->headBody);
    work->stateFrames = 0;
    work->state       = work->state + 1;
    soundId           = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan               = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Idle-state tick: re-arms the animation request, then advances the clip
/// phase `animFrames` -- reset when the requested clip is not the one playing,
/// otherwise ramped by `func_actor_206100_8014F3C8` -- ticks every animation
/// slot and bumps the frame counter.
///
/// `next` is the same block as `work` loaded a second time: the first four
/// stores reach it through one local and everything after the request-kind
/// read through the other, which is what the two loads of `Task::work` are.
///
/// The loop is written `for (i = 1; i < 0xF; i++)` rather than as the
/// `do`/`while` its test-at-the-bottom shape suggests, and its initialiser sits
/// *after* the sub-state chain rather than before it.  Placed before the chain,
/// the store that materialises `i` is the one the case-3 branch jumps over, and
/// post-reload CSE then rewrites the phase's `+ 1` into `+ $s0`; after the chain
/// that store is the branch's own target, reorg copies it into the delay slot
/// and threads the branch past it.  See `DECOMPILATION_LEARNINGS.md`, "A
/// constant store in a delay slot decides whether post-reload CSE folds it into
/// a later increment".
static void func_actor_206100_8014FCD4(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    s32               i;
    s16               state;

    work              = task->work;
    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0xE;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    next              = task->work;
    state             = next->animRequest;
    if (state == DIVER_ANIM_REQUEST_BLEND) {
        if (next->animPlaying != next->animClip) {
            next->animFrames = 0;
        } else {
            next->animFrames = func_actor_206100_8014F3C8(task, next->animFrames);
        }
        func_actor_206100_8014F2F0(task);
        next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (state == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(task);
        next->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        next->animFrames  = 0;
    } else if (state == DIVER_ANIM_REQUEST_PLAYING) {
        next->animFrames = next->animFrames + 1;
    }
    for (i = 1; i < ARRAY_SIZE(next->rig.slots); i++) {
        animationTickSlot(&next->rig.anim, i);
    }
    work->state = work->state + 1;
}
/// Idle-state tick: advances the actor's two frame counters, keeps the root
/// coordinate dirty so the composition pass rebuilds it, spawns the shockwave task once the
/// counter reaches 0x5A and retires the actor four frames later.
///
/// `coord` is a local rather than the inline
/// `task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;` because the fused form loads
/// `task->extra` *after* the two counter stores, and sched1 will not lift a load
/// above an earlier store; its address load stays with the stores and both pick
/// up load-delay nops.  Binding the pointer above the counters frees the two
/// loads to be scheduled first, which is the target's order; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store".
static void func_actor_206100_8014FDE8(Task* task)
{
    _Actor206100Work* work;
    _Actor206100Work* next;
    GfxCoord*         coord;

    coord               = task->extra.tmd->coords;
    work                = task->work;
    work->stateFrames   = work->stateFrames + 1;
    work->goalY         = work->goalY + 0x10;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateFrames == 0x5A) {
        taskSpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    if (work->stateFrames >= 0x10E) {
        task->state    = 4;
        next           = task->work;
        next->state    = 0;
        next->subState = 0;
    }
}
