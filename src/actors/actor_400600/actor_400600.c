#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "rooms/dryfield_night_junk_yard.h"
#define STALKER_ZEBRA_IVORY_KIND STALKER_ZEBRA
#include "../../shared/stalker_zebra_ivory.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

/// Work block of the Zebra Stalker task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It holds
/// the model's matrices, the animation context and its storage, the collision
/// spheres and the probe capsule with their contact records, the cloak fade
/// and the state machine: the task state picks a table of states, `state` an
/// entry of that table, and `subState` a step of that entry's own table.
/// Entering a task state clears both and selecting a state clears `subState`.
///
/// While the task runs (task state 1) `state` is one of eighteen:
///
/// | | |
/// |---|---|
/// | 0 | go to 1 |
/// | 1 | wait hidden for the target within 3000 or for another Zebra Stalker's death, then show and walk |
/// | 2 | walk at the target; picks an attack once `timer` has run out, and hides when the player faces it from nearby |
/// | 3, 4 | light and heavy recoil |
/// | 5 | status hold, until the enemy's status buildup runs out |
/// | 6, 7 | left-arm and right-arm strike |
/// | 8 | grab: probe the line to the target, then hold and bite the player |
/// | 9 | leap back, as far as the probe behind it allows |
/// | 0xA | crawl on its back until `countdown` runs out |
/// | 0xB | right itself |
/// | 0xC, 0xD | leap to the ceiling, drop from it |
/// | 0xE | knocked off the ceiling onto its back |
/// | 0xF | knockdown |
/// | 0x10 | leave the ceiling: drop when the target is beyond 2000, else grab |
/// | 0x11 | idle hidden for `idleFrames`, then show and walk |
///
/// While it dies (task state 2) `state` walks twelve steps: 0 picks the death
/// (1 on the floor, 7 after a blast, 9 on the ceiling); 1..6 play the death
/// clip, unlink the bodies and burn the corpse away; 7 and 8 burst the body
/// into chunks; 9..0xB drop it from the ceiling onto its back and rejoin 1.
/// Task state 3 kills the arm tasks, raises the scene's Zebra Stalker death
/// alert and destroys the enemy 0x97 frames later.
///
/// Task states 4..8 are the scripted entrances, chosen by the high nibble
/// (1..5) of the task's first spawn argument. Each hides the Stalker, waits
/// for `roomCommand` or for the scene's Zebra Stalker group phase, brings it
/// in and joins task state 1.
typedef struct {
    MATRIX                savedRootMtx;           // root matrix when the corpse began to burn; each frame rescales a copy of it
    MATRIX                colorMtx;               // storage for the `TmdObject::colorMtx` of the body and both arm models
    MATRIX                lightMtx;               // storage for their `TmdObject::lightMtx`
    byte                  field_60[0x10];         // never accessed
    VECTOR                prevRootPos;            // root position at the start of the frame; X and Z are restored when the collision step reports a conflict
    s16                   pitch;                  // root pitch in 4096ths of a turn; swept through half a turn by a leap to the ceiling or a drop from it
    s16                   yaw;                    // root heading in 4096ths of a turn
    s16                   roll;                   // root roll in 4096ths of a turn; half a turn apart on its feet and on the ceiling or its back, a quarter turn on the wall of the water entrance
    byte                  field_86[2];            // never accessed
    SVECTOR3              anchorPos;              // view-space spot a part is pinned to while the root moves around it: X and Z on the floor (`vy` unused), X and Y on the wall of the water entrance (`vz` then holds the root's Z)
    byte                  field_8E[2];            // never accessed
    s16                   spawnX;                 // root X at spawn; never read
    s16                   floorY;                 // root Y at spawn: the height every leap, drop and fall lands on
    s16                   spawnZ;                 // root Z at spawn; never read
    byte                  field_96[2];            // never accessed
    s16                   leapX;                  // X step per frame of the leap back; for a drop from the ceiling, the X the root lands at
    s16                   leapY;                  // height the leap to the ceiling rises to, or the hop off the player lands on (`floorY`)
    s16                   leapZ;                  // Z counterpart of `leapX`
    byte                  field_9E[0xA];          // never accessed
    SVECTOR               targetPos;              // point it hunts: the player's root, or a waypoint of the zone route while `routesByZone` is set
    ActorAnimRig18        rig;                    // playback of the model's parts: slots 1 to 17 play `animClip`, and slot 1's status tells when it ended
    WorldCollisionBody    body;                   // sphere on part 3 that takes hits and is tested against the room grid; out of both while the hold has the player, out of the grid during a leap to the ceiling
    WorldCollisionContact bodyContacts[8];        // contacts of `body`: the enemy's hit records and the push-out of each frame
    WorldCollisionBody    rightArmBody;           // attack sphere on part 7; enabled only on frames 0x15..0x1B of the right-arm strike
    WorldCollisionContact rightArmContacts[1];    // contacts of `rightArmBody`
    WorldCollisionBody    leftArmBody;            // attack sphere on part 10; enabled only on frames 0x15..0x1B of the left-arm strike
    WorldCollisionContact leftArmContacts[1];     // contacts of `leftArmBody`
    WorldCollisionBody    capsuleBody;            // probe on the root, tested against the room grid only while a grab, a leap back or a leap to the ceiling looks for a wall
    WorldCollisionCapsule capsule;                // its shape: from the root to the target for a grab, 3000 behind the root for a leap back, 3000 above it for a leap to the ceiling
    WorldCollisionContact capsuleContacts[8];     // contacts of `capsuleBody`; the first grid face among them is the wall
    EffectSpawnArg        effectArg;              // argument record of the effects its hits spawn, hung off part 3
    Task*                 armTasks[2];            // tasks of the arm models (0 left on part 10, 1 right on part 7); they take the body's draw flags and swing out while that arm strikes
    byte                  field_70C[4];           // never accessed
    s16                   ceilingCooldown;        // frames left before it may next jump to the ceiling or start a move from it; set to 210..241 as each jump up and drop ends
    u16                   previousAnimationFlags; // slot 1's ANIMATION_SLOT_* results as the last running update's tick left them
    s16                   corpseScaleY;           // Y-axis scale of the burning corpse's root matrix (4.12, ONE = unscaled); lowered every frame of the burn
    u16                   frameCount;             // frames the enemy has run, counted from a random start; never read
    s16                   stateFrames;            // frames spent in the current state or sub-state; in the group entrance, the frames left before it joins
    s16                   holdFrames;             // frames since the hold caught the player
    s16                   state;                  // index into the state table of the current task state
    s16                   subState;               // index into the step table of the current state
    s16                   animBlend;              // frames a blend request takes
    s16                   moveAccel;              // added to `moveSpeed` each frame; itself grows each frame
    s16                   moveSpeed;              // vertical speed of a leap, a drop or a fall
    s16                   animStep;               // playback rate of slots 1..17; `ANIMATION_RATE_ONE` is normal speed
    s16                   playerDistance;         // horizontal distance from the root to `targetPos`
    u16                   bearingFromPlayer;      // heading from `targetPos` to the root relative to the player's heading, 0..0xFFF; 0 when the player faces it
    u16                   targetBearing;          // heading of `targetPos` relative to `yaw`, 0..0xFFF
    s16                   pendingArmed;           // 1 when a hit or a status tick dealt damage this frame; lets `pendingAction` be consumed
    s16                   pendingAction;          // `STALKER_ZEBRA_IVORY_PENDING_*` awaiting the state machine
    s16                   timer;                  // frames before the walk may pick its next attack
    s16                   nextAnchorPart;         // the walking part (8 or 0xB) the walk is not pinning; never read
    byte                  field_736[4];           // never accessed
    s16                   shadowShade;            // brightness of the limb shadows, 0 to 0xFF; eased with the cloak fade
    s16                   shadowWallZ;            // view-space Z of the wall the limb shadows are laid on during the water entrance
    s16                   shadowHeight;           // height the limb shadows are laid at: `floorY`, or `leapY` once a leap nears the ceiling
    s16                   cloakFadeFrames;        // frames the running cloak fade has lasted; it ends at 0x20 when showing, 0x12 when hiding
    s16                   animRequest;            // `STALKER_ZEBRA_IVORY_ANIM_REQUEST_*`
    s16                   animPlaying;            // clip last applied to the slots
    s16                   animClip;               // requested clip: index into the animation set table
    s16                   animFrame;              // frames since `animClip` was applied; rescaled when a blend repeats it at a new step
    s16                   hitCooldown;            // frames before another hit is taken; set from the hit's id parameter 2
    s16                   armSwingAngles[2];      // yaw each arm model has swung out by (0 left, 1 right): eased to 0x380 while the arm is out, back to 0 after
    u16                   countdown;              // frames left on its back before it rights itself, 0x1E..0x9D
    s16                   walkStep;               // `animStep` the walk clip takes from its next loop; raised for a distant target
    s16                   turnStep;               // heading change per frame of the walk
    u16                   idleFrames;             // frames left of the idle, 0x5A..0x99
    s16                   hideCooldown;           // frames before the walk may next hide, 0x1E..0x5D
    s16                   markedFrames;           // frames left of the mark a hit by row 0xE of the weapon attack table leaves (600): part 3 gives off a puff every eighth frame, and a finished hide leaves the enemy lockable
    s16                   holdLoops;              // bite clips the hold has completed; the hold ends at 3
    u8                    cloaked;                // cloak target (0 shown, 1 hidden)
    u8                    cloakFading;            // 1 while the fade toward `cloaked` runs
    s8                    damageOverTimeSeen;     // set once the enemy has carried a damage-over-time status; never read
    byte                  field_761[1];           // never accessed
    u8                    roomCommand;            // kind of the last room command (1..4); the scripted entrances wait for it
    u8                    playerDied;             // set by message 2014, broadcast when damage takes the player's last health; ends a hold
    u8                    holdKilledPlayer;       // 1 once a bite has taken the player's last health; the release then leaves the player's animation and scripted mode alone
    u8                    rightArmOut;            // 1 while the right-arm strike keeps its arm model swung out
    u8                    leftArmOut;             // 1 while the left-arm strike keeps its arm model swung out
    u8                    holding;                // 1 while the current move must finish before the death sequence starts
    u8                    onCeiling;              // 1 while it hangs from the ceiling
    u8                    onBack;                 // 1 once it has landed on its back; cleared when it rights itself
    u8                    distanceMode;           // how the probe's wall contact is measured (0 hit or miss, 1 X/Z distance, 2 X/Y distance)
    u8                    walkHurried;            // 1 once the walk has raised its steps for a target beyond 3000
    u8                    inWater;                // 1 for the water entrance (spawn kind 1): its steps and landings raise ripples and spray
    u8                    ceilingProbePending;    // 1 while the walk waits a frame for the capsule's probe of the ceiling above it
    u8                    routesByZone;           // 1 when spawned in the Dryfield water tower area: `targetPos` then leads it zone by zone to the player's zone
    byte                  field_76F[1];           // never accessed
} _Actor400600ZebraStalkerWork;
STATIC_ASSERT_SIZEOF(_Actor400600ZebraStalkerWork, 0x770);

/// The work block the `stalkerZebraIvory` fragments included below operate on:
/// this package's own. `stalker_zebra_ivory.h` lists the members they reach.
typedef _Actor400600ZebraStalkerWork StalkerZebraIvoryWork;

/// Scratch-stack block of one floor limb shadow quad: the corners of the
/// subtractive textured quad laid under the segment between two points, and
/// their projection.
///
/// `corners` are in world space, the frame under the view coordinate, in GPU
/// quad strip order: 0 and 1 either side of the first point, 2 and 3 either
/// side of the second, each pair at its point's height and pushed outwards
/// along the segment by half its length, so the quad is twice as long as the
/// segment. `screenCorners`, `depthCue` and `flag` are `RotTransPers4`'s
/// outputs for those four corners.
///
/// It is `ActorLimbShadowScratch` without the part transforms and positions:
/// the floor shadow stages every part's position once, outside the block, and
/// hands the drawer the two ends of each segment.
///
/// Reserve one block for a segment and release it once the quad is queued;
/// nothing in it outlives the call.
typedef struct {
    SVECTOR corners[4];       // The quad's corners; `pad` is never written
    long    screenCorners[4]; // Projected corners: screen X in bits 0..15, Y in bits 16..31, copied whole into the primitive
    long    depthCue;         // Depth-cueing interpolation value of the projection; never read
    long    flag;             // GTE FLAG word of the projection; a set bit 31 drops the quad
    s32     depth;            // Last corner's screen Z / 4, which picks the ordering-table entry
} _Actor400600LimbShadowQuadScratch;
STATIC_ASSERT_SIZEOF(_Actor400600LimbShadowQuadScratch, 0x3C);

extern ActorZone D_actor_400600_80151B40[];

/* `D_800678F0` selects the model stream a following `effectSpawn` uses as the
 * source for the effect's own `TmdObject`; `gSceneCombatState.zebraStalkerGroupPhase` and `gSceneCombatState.zebraStalkerDeathAlert` are
 * bytes of the run of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `_Actor400600ZebraStalkerWork` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `gSceneCombatState.zebraStalkerDeathAlert` matched with an empty-asm barrier after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `gSceneCombatState.zebraStalkerGroupPhase` is the aggregate case too: one of its stores sits between
 * struct stores on both sides, and the barrier trades the sink for a hoist
 * above the preceding flag updates. */
extern void* D_800678F0[1];

extern DamageAttack D_actor_400600_80144EA8[2];
extern EnemyParams  D_actor_400600_80144EB0;      // the enemy's parameter record

extern AnimationSet* D_actor_400600_80151A54[35]; // animation bank handed to animationInitContext
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_400600_80151AE0[3];

extern AnimationSet* D_actor_400600_80151A48[3];

extern TaskDesc D_actor_400600_80151AF8[];

/// The records closing four of the overlay's model streams, selected through
/// `D_800678F0`.
static TmdSource _gActor400600ZebraStalkerBurstHead;
static TmdSource _gActor400600StalkerEffect;
static TmdSource _gActor400600StalkerBurstHandLeft;
static TmdSource _gActor400600StalkerBurstFootRight;

extern u8 gStalkerZebraIvoryResumeClips[];

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

/// Cloak requests and the arm-lighting sentinel accepted by this package.
enum {
    ACTOR_400600_REVEAL         = 0,
    ACTOR_400600_CLOAK          = 1,
    ACTOR_400600_KEEP_ARM_COLOR = -1
};

/// Water-tower route regions; coordinate signs identify their sides of the tower.
enum {
    ACTOR_400600_ZONE_CENTER     = 1,
    ACTOR_400600_ZONE_POSITIVE_X = 2,
    ACTOR_400600_ZONE_NEGATIVE_Z = 3,
    ACTOR_400600_ZONE_NEGATIVE_X = 4,
    ACTOR_400600_ZONE_POSITIVE_Z = 5,
    ACTOR_400600_ZONE_PASSAGE    = 6
};

/// Running behavior indices used by this package's attack and reaction selectors.
enum {
    ACTOR_400600_STATE_WALK              = 2,
    ACTOR_400600_STATE_LEFT_STRIKE       = 6,
    ACTOR_400600_STATE_RIGHT_STRIKE      = 7,
    ACTOR_400600_STATE_GRAB              = 8,
    ACTOR_400600_STATE_ON_BACK           = 0xA,
    ACTOR_400600_STATE_LEAP_TO_CEILING   = 0xC,
    ACTOR_400600_STATE_DROP_FROM_CEILING = 0xD,
    ACTOR_400600_STATE_LEAVE_CEILING     = 0x10
};

/// Probe geometry and the distance interpretation of its returned contact.
enum {
    ACTOR_400600_PROBE_TARGET             = 0,
    ACTOR_400600_PROBE_BACK               = 1,
    ACTOR_400600_PROBE_CEILING            = 2,
    ACTOR_400600_FLOOR_TARGET_Y_OFFSET    = 900,
    ACTOR_400600_CEILING_TARGET_Y_OFFSET  = 1600,
    ACTOR_400600_BACK_PROBE_Y             = 400,
    ACTOR_400600_PROBE_LENGTH             = 3000,
    ACTOR_400600_THIN_PROBE_RADIUS        = 10,
    ACTOR_400600_BACK_PROBE_RADIUS        = 80,
    ACTOR_400600_PROBE_ROOT_Y             = 100,
    ACTOR_400600_SECRET_PASSAGE_CEILING_Y = -2500
};

/// Cloak durations in update frames; shade is an eight-bit grey level.
enum {
    ACTOR_400600_REVEAL_FADE_FRAMES = 32,
    ACTOR_400600_HIDE_FADE_FRAMES   = 18,
    ACTOR_400600_SHADOW_SHADE_FULL  = 255
};

/// Floor limb-shadow geometry and the subtractive four-bit texture packet.
enum {
    ACTOR_400600_FLOOR_SHADOW_POINT_COUNT  = 11,
    ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH   = 128,
    ACTOR_400600_SHADOW_TRIG_FRACTION_BITS = 12,
    ACTOR_400600_SHADOW_PACKET_WORDS       = 9,
    ACTOR_400600_SHADOW_QUAD_CODE          = 0x2E,
    ACTOR_400600_SHADOW_TPAGE              = 0x48,
    ACTOR_400600_SHADOW_CLUT               = 0x4283,
    ACTOR_400600_SHADOW_U0                 = 0xC0,
    ACTOR_400600_SHADOW_V0                 = 0x98,
    ACTOR_400600_SHADOW_U1                 = 0xF7,
    ACTOR_400600_SHADOW_V1                 = 0xCF
};

/// Animation-set indices for the knockdown's recoil and resting poses.
enum {
    ACTOR_400600_CLIP_STANDING_REST            = 0x10,
    ACTOR_400600_CLIP_ON_BACK_REST             = 0x14,
    ACTOR_400600_CLIP_KNOCKDOWN_UPRIGHT        = 0x1A,
    ACTOR_400600_CLIP_KNOCKDOWN_ON_BACK        = 0x1B,
    ACTOR_400600_KNOCKDOWN_RECOIL_BLEND_FRAMES = 3,
    ACTOR_400600_KNOCKDOWN_REST_BLEND_FRAMES   = 30,
    ACTOR_400600_ARM_LEFT                      = 0,
    ACTOR_400600_ARM_RIGHT                     = 1
};

/// Clip, hand-anchor, frame-scale and sound values shared by the gait and ceiling transitions.
enum {
    ACTOR_400600_CLIP_WALK               = 2,
    ACTOR_400600_CLIP_LANDING            = 25,
    ACTOR_400600_PART_RIGHT_HAND         = 8,
    ACTOR_400600_PART_LEFT_HAND          = 11,
    ACTOR_400600_LANDING_BLEND_FRAMES    = 2,
    ACTOR_400600_WALK_FIRST_END_FRAME    = 11,
    ACTOR_400600_WALK_SECOND_START_FRAME = 12,
    ACTOR_400600_WALK_SECOND_END_FRAME   = 21,
    ACTOR_400600_FRAME_FRACTION_BITS     = 8,
    ACTOR_400600_RATE_FRACTION_BITS      = 4,
    ACTOR_400600_SPAWN_ENTRANCE_MASK     = 0xF0,
    ACTOR_400600_SPAWN_WATER_ENTRANCE    = 0x10,
    ACTOR_400600_SOUND_LANDING           = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 3,
    ACTOR_400600_SOUND_WATER_LANDING     = 0x404A0003,
};

/// Clip and timing values of the arm strikes and ceiling entries.
enum {
    ACTOR_400600_CLIP_UPRIGHT_LIGHT_RECOIL = 9,
    ACTOR_400600_STRIKE_FIRST_HIT_FRAME    = 21,
    ACTOR_400600_STRIKE_END_HIT_FRAME      = 28,
    ACTOR_400600_STRIKE_TIMER_BASE         = 45,
    ACTOR_400600_STRIKE_RETREAT_DISTANCE   = 1400,
    ACTOR_400600_STRIKE_FACING_MARGIN      = 0x200,
    ACTOR_400600_STRIKE_REAR_ARC_END       = 0xC00,
    ACTOR_400600_STATE_LEAP_BACK           = 9,
    ACTOR_400600_SOUND_STRIKE              = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 5,
    ACTOR_400600_SOUND_WATER_STRIKE        = 0x404A0005
};

/// Death-table entries accepted by the out-of-line state selector's callers.
enum {
    ACTOR_400600_DEATH_START_CLIP   = 1,
    ACTOR_400600_DEATH_BURST        = 7,
    ACTOR_400600_DEATH_CEILING_FALL = 9
};

/// Body/water-bank cue 4 and the leap clip shared by these entrance and movement phases.
enum {
    ACTOR_400600_SOUND_BODY_CUE4   = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 4,
    ACTOR_400600_SOUND_WATER_CUE4  = 0x404A0004,
    ACTOR_400600_CLIP_LEAP         = 21,
    ACTOR_400600_LEAP_BLEND_FRAMES = 4
};

/// Water-entry timing in updates, and the Stalker's water-surface effect recipe.
enum {
    ACTOR_400600_WATER_LEAP_MOVE_FRAME       = 17,
    ACTOR_400600_WATER_LEAP_SPLASH_FRAME     = 34,
    ACTOR_400600_WATER_FIRST_RIPPLE_FRAME    = 35,
    ACTOR_400600_WATER_SECOND_RIPPLE_FRAME   = 37,
    ACTOR_400600_WATER_LEAP_RIPPLE_HALF_SIZE = 56,
    ACTOR_400600_SPLASH_POINT_COUNT          = 16,
    ACTOR_400600_SPLASH_ANGLE_STEP           = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    ACTOR_400600_SPLASH_RADIUS_SHIFT         = 3,
    ACTOR_400600_SPLASH_RIPPLE_HALF_SIZE     = 64,
    ACTOR_400600_SPLASH_SPRAY_ARGUMENT       = 0x01202148, // Size 328, two updates/cell, speed 32, random upward spray
    ACTOR_400600_SPLASH_LANDING_Y_OFFSET     = -420
};

static void _actor400600InitCollisionBodies(Task* task);
static void _actor400600DrawWallLimbShadow(Task* task, s16 firstPartIndex, s16 secondPartIndex, s16 halfWidth, s16 wallZ, u8 shade);
static void _actor400600DrawWallShadows(Task* task, s16 wallZ, u8 shade);
static void func_actor_400600_801328A8(Task* arg0);
static void _actor400600FinishJunkYardLanding(Task* task);
static void _actor400600TickFloorDropEntrance(Task* task);
static void _actor400600WaitWaterEntranceCommand(Task* task);
static void _actor400600TickFirstWaterWallWalk(Task* task);
static void _actor400600TickFirstWaterEntranceLeap(Task* task);
static void _actor400600TickSecondWaterEntranceLeap(Task* task);
static void _actor400600WaitGroupDropEntranceCommand(Task* task);
static void func_actor_400600_80133434(Task* arg0);
static void func_actor_400600_801337A8(Task* arg0);
static void _actor400600WaitHiddenWakeup(Task* task);
static void _actor400600TickLeftStrike(Task* task);
static void _actor400600TickRightStrike(Task* task);
static void _actor400600StartGrabHold(Task* task);
static void _actor400600TickGrabHold(Task* task);
static void _actor400600TickGrabReleaseFall(Task* task);
static void _actor400600ResolveBackwardLeapProbe(Task* task);
static void _actor400600TickBackwardLeap(Task* task);
static void _actor400600TickCeilingLeap(Task* task);
static void _actor400600TickCeilingDrop(Task* task);
static void _actor400600FinishCeilingDropLanding(Task* task);
static void _actor400600FinishCeilingFallLanding(Task* task);
static void func_actor_400600_801356E0(Task* arg0);
static void _actor400600TickHorizontalWalk(Task* task, s16 nextLoopRate);
static void _actor400600TickWallWalk(Task* task);
static void _actor400600TickCloakFade(Task* task);
static void _actor400600UpdateTarget(Task* task);
static void func_actor_400600_80136968(Task* arg0);
static s32  _actor400600TakeArmedHitReaction(Task* task);
static void func_actor_400600_80137240(Task* arg0);
static void _actor400600StartWallProbe(Task* task, s16 probeMode);
static void _actor400600UpdateArmSwing(Task* task);
static s32  _actor400600TryEnterHiddenIdle(Task* task);
static s32  _actor400600TrySelectAttack(Task* task);
static void _actor400600UpdateGroupEntrance(Task* task);
static void _actor400600DrawFloorShadows(Task* task, s16 height, u8 shade);
static void _actor400600DrawFloorLimbShadow(const SVECTOR* firstPoint, const SVECTOR* secondPoint, s16 halfWidth, u8 shade);
static void _actor400600SyncArmModelFlags(Task* task, s32 colorMode);
static s32  _actor400600FindRouteZone(Task* task);
static s32  _actor400600ClipWasDone(Task* task);
static void _actor400600SpawnWaterSplash(Task* task, s16 verticalOffset);
static void _actor400600SnapCloaked(Task* task, s16 cloakRequest);
static void _actor400600TickAttackCooldowns(Task* task);
static void _actor400600StartCloakFade(Task* task, u8 cloak);
static void _actor400600RunWaterEntrance(Task* task);
static void _actor400600RunGroupDropEntrance(Task* task);
static void _actor400600RunJunkYardEntrance(Task* task);
static void _actor400600RunFloorDropEntrance(Task* task);
static void _actor400600EnterHiddenWait(Task* task);
static void func_actor_400600_80139110(Task* arg0);
static void _actor400600RunLightRecoil(Task* task);
static void _actor400600RunHeavyRecoil(Task* task);
static void _actor400600RunStatusHold(Task* task);
static void _actor400600RunLeftStrike(Task* task);
static void _actor400600RunRightStrike(Task* task);
static void _actor400600RunGrab(Task* task);
static void _actor400600RunBackwardLeap(Task* task);
static void _actor400600TickOnBackBehavior(Task* task);
static void _actor400600RunRighting(Task* task);
static void _actor400600RunCeilingLeap(Task* task);
static void _actor400600RunCeilingDrop(Task* task);
static void _actor400600RunCeilingFall(Task* task);
static void _actor400600RunCeilingExit(Task* task);
static void _actor400600RunHiddenIdle(Task* task);
static void _actor400600RequestClipBlend(Task* task, s16 clipIndex, s16 rate, s16 blendFrames);
static void _actor400600ReadPartWorldXY(Task* task, s16 partIndex, SVECTOR3* anchorPosition);
static void _actor400600PinPartXY(Task* task, s16 partIndex, const SVECTOR3* anchorPosition);
static void _actor400600Task(Task* task);
static void _actor400600RunDeathSequence(Task* task);
static void _actor400600RunCorpseRelease(Task* task);
static void _actor400600HandleHoldReleaseMessage(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
static void _actor400600InitLeftArmTask(Task* task);
static void _actor400600InitRightArmTask(Task* task);
static void _actor400600SelectDeathSequence(Task* task);
static void _actor400600StartCorpseBurn(Task* task);
static void _actor400600WaitCorpseBurn(Task* task);
static void _actor400600TickCorpseBurn(Task* task);
static void _actor400600EnterCorpseRelease(Task* task);
static void _actor400600WaitDeathBurst(Task* task);
static void func_actor_400600_8013A864(Task* arg0);
static void _actor400600StartDeathCeilingFall(Task* task);
static void _actor400600TickDeathCeilingFall(Task* task);
static void _actor400600FinishDeathCeilingFallLanding(Task* task);
static void _actor400600StartCorpseRelease(Task* task);
static void _actor400600WaitCorpseRelease(Task* task);
static void _actor400600HideForJunkYardEntrance(Task* task);
static void func_actor_400600_8013AC14(Task* arg0);
static void _actor400600StartJunkYardFloorDrop(Task* task);
static void _actor400600TickJunkYardFloorDrop(Task* task);
static void _actor400600HideForFloorDropEntrance(Task* task);
static void _actor400600WaitFloorDropEntranceCommand(Task* task);
static void _actor400600FinishFloorDropLanding(Task* task);
static void _actor400600HideForWaterEntrance(Task* task);
static void _actor400600FinishFirstWaterLanding(Task* task);
static void _actor400600TickSecondWaterWallWalk(Task* task);
static void _actor400600FinishSecondWaterLanding(Task* task);
static void _actor400600HideForGroupDropEntrance(Task* task);
static void _actor400600TickGroupEntranceDrop(Task* task);
static void _actor400600FinishGroupDropLanding(Task* task);
static void _actor400600QueueSoundBank(void);
static void func_actor_400600_8013B6F4(Task* arg0);
static void func_actor_400600_8013B740(Task* arg0);
static void _actor400600StartLightRecoil(Task* task);
static void _actor400600TickLightRecoil(Task* task);
static void _actor400600StartHeavyRecoil(Task* task);
static void _actor400600FinishHeavyRecoil(Task* task);
static void _actor400600StartStatusHold(Task* task);
static void _actor400600WaitStatusHold(Task* task);
static void _actor400600FinishStatusRecovery(Task* task);
static void _actor400600StartLeftStrike(Task* task);
static void _actor400600StartRightStrike(Task* task);
static void _actor400600StartGrabWindup(Task* task);
static void _actor400600StartGrabLeapWindup(Task* task);
static void _actor400600WaitGrabLeapWindup(Task* task);
static void _actor400600StartGrabTargetProbe(Task* task);
static void _actor400600StartBackwardLeapProbe(Task* task);
static void _actor400600StartRighting(Task* task);
static void _actor400600StartCeilingLeap(Task* task);
static void _actor400600FinishCeilingLeapLanding(Task* task);
static void _actor400600CommitCeilingDrop(Task* task);
static void _actor400600StartCeilingDrop(Task* task);
static void _actor400600StartCeilingFall(Task* task);
static void _actor400600TickCeilingFall(Task* task);
static void _actor400600StartKnockdownRecoil(Task* task);
static void _actor400600BlendKnockdownRest(Task* task);
static void _actor400600FinishKnockdownRest(Task* task);
static void _actor400600StartCeilingExit(Task* task);
static void _actor400600StartHiddenIdle(Task* task);
static void _actor400600TickHiddenIdle(Task* task);
static void _actorContactCalcHorizontalPushback(const SVECTOR* position, const WorldCollisionContact* contact, SVECTOR* pushDelta);
static s16  _actor400600SelectCollisionStep(s16 gridStep, s16 bodyPush);
static void _actor400600HideForGroupEntrance(Task* task);
static void _actor400600WaitGroupEntrance(Task* task);
static void _actor400600TickGroupEntranceDelay(Task* task);
static void _actor400600ExtendStrikeArm(Task* task, u8 armIndex);
static void _actor400600RequestPositionalSound(Task* task, s32 cueId);
static void _actor400600SelectState(Task* task, s16 state);

static TmdSource _gActor400600ZebraStalkerBody;

static TmdSource _gActor400600ZebraStalkerBurstArmLeft;
static TmdSource _gActor400600ZebraStalkerBurstArmLeft13064;

static TmdBone _gActor400600ZebraStalkerBodySkeleton[18] = {
#include "assets/zebra_stalker_body_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBodyPartVerts[18] = {
#include "assets/zebra_stalker_body_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBodyVerts[258] = {
#include "assets/zebra_stalker_body_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBodyNormals[290] = {
#include "assets/zebra_stalker_body_normals.inc"
};

static u32 _gActor400600ZebraStalkerBodyStream[3683] = {
#include "assets/zebra_stalker_body_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBody = {
    0,
    18184,
    6868,
    18,
    _gActor400600ZebraStalkerBodyPartVerts,
    _gActor400600ZebraStalkerBodyVerts,
    _gActor400600ZebraStalkerBodyNormals,
    _gActor400600ZebraStalkerBodySkeleton,
    _gActor400600ZebraStalkerBodyStream,
};

static TmdBone _gActor400600ZebraStalkerBurstHeadSkeleton[1] = {
#include "assets/zebra_stalker_burst_head_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstHeadPartVerts[1] = {
#include "assets/zebra_stalker_burst_head_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHeadVerts[37] = {
#include "assets/zebra_stalker_burst_head_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHeadNormals[45] = {
#include "assets/zebra_stalker_burst_head_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstHeadStream[359] = {
#include "assets/zebra_stalker_burst_head_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstHead = {
    0,
    2408,
    0,
    1,
    _gActor400600ZebraStalkerBurstHeadPartVerts,
    _gActor400600ZebraStalkerBurstHeadVerts,
    _gActor400600ZebraStalkerBurstHeadNormals,
    _gActor400600ZebraStalkerBurstHeadSkeleton,
    _gActor400600ZebraStalkerBurstHeadStream,
};

static TmdBone _gActor400600StalkerBurstTorsoSkeleton[1] = {
#include "assets/stalker_burst_torso_skeleton.inc"
};

static u32 _gActor400600StalkerBurstTorsoPartVerts[1] = {
#include "assets/stalker_burst_torso_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstTorsoVerts[55] = {
#include "assets/stalker_burst_torso_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstTorsoNormals[62] = {
#include "assets/stalker_burst_torso_normals.inc"
};

static u32 _gActor400600StalkerBurstTorsoStream[600] = {
#include "assets/stalker_burst_torso_stream.inc"
};

static TmdSource _gActor400600StalkerBurstTorso = {
    0,
    3988,
    0,
    1,
    _gActor400600StalkerBurstTorsoPartVerts,
    _gActor400600StalkerBurstTorsoVerts,
    _gActor400600StalkerBurstTorsoNormals,
    _gActor400600StalkerBurstTorsoSkeleton,
    _gActor400600StalkerBurstTorsoStream,
};

static TmdBone _gActor400600StalkerEffectSkeleton[1] = {
#include "assets/stalker_effect_skeleton.inc"
};

static u32 _gActor400600StalkerEffectPartVerts[1] = {
#include "assets/stalker_effect_partVerts.inc"
};

static SVECTOR _gActor400600StalkerEffectVerts[27] = {
#include "assets/stalker_effect_verts.inc"
};

static SVECTOR _gActor400600StalkerEffectNormals[34] = {
#include "assets/stalker_effect_normals.inc"
};

static u32 _gActor400600StalkerEffectStream[284] = {
#include "assets/stalker_effect_stream.inc"
};

static TmdSource _gActor400600StalkerEffect = {
    0,
    1860,
    0,
    1,
    _gActor400600StalkerEffectPartVerts,
    _gActor400600StalkerEffectVerts,
    _gActor400600StalkerEffectNormals,
    _gActor400600StalkerEffectSkeleton,
    _gActor400600StalkerEffectStream,
};

static TmdBone _gActor400600StalkerBurstHandLeftSkeleton[1] = {
#include "assets/stalker_burst_hand_left_skeleton.inc"
};

static u32 _gActor400600StalkerBurstHandLeftPartVerts[1] = {
#include "assets/stalker_burst_hand_left_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstHandLeftVerts[23] = {
#include "assets/stalker_burst_hand_left_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstHandLeftNormals[26] = {
#include "assets/stalker_burst_hand_left_normals.inc"
};

static u32 _gActor400600StalkerBurstHandLeftStream[211] = {
#include "assets/stalker_burst_hand_left_stream.inc"
};

static TmdSource _gActor400600StalkerBurstHandLeft = {
    0,
    1400,
    0,
    1,
    _gActor400600StalkerBurstHandLeftPartVerts,
    _gActor400600StalkerBurstHandLeftVerts,
    _gActor400600StalkerBurstHandLeftNormals,
    _gActor400600StalkerBurstHandLeftSkeleton,
    _gActor400600StalkerBurstHandLeftStream,
};

static TmdBone _gActor400600ZebraStalkerBurstHandRightSkeleton[1] = {
#include "assets/zebra_stalker_burst_hand_right_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstHandRightPartVerts[1] = {
#include "assets/zebra_stalker_burst_hand_right_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHandRightVerts[23] = {
#include "assets/zebra_stalker_burst_hand_right_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHandRightNormals[29] = {
#include "assets/zebra_stalker_burst_hand_right_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstHandRightStream[211] = {
#include "assets/zebra_stalker_burst_hand_right_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstHandRight = {
    0,
    1400,
    0,
    1,
    _gActor400600ZebraStalkerBurstHandRightPartVerts,
    _gActor400600ZebraStalkerBurstHandRightVerts,
    _gActor400600ZebraStalkerBurstHandRightNormals,
    _gActor400600ZebraStalkerBurstHandRightSkeleton,
    _gActor400600ZebraStalkerBurstHandRightStream,
};

static TmdBone _gActor400600ZebraStalkerBurstFootLeftSkeleton[1] = {
#include "assets/zebra_stalker_burst_foot_left_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstFootLeftPartVerts[1] = {
#include "assets/zebra_stalker_burst_foot_left_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstFootLeftVerts[19] = {
#include "assets/zebra_stalker_burst_foot_left_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstFootLeftNormals[25] = {
#include "assets/zebra_stalker_burst_foot_left_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstFootLeftStream[188] = {
#include "assets/zebra_stalker_burst_foot_left_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstFootLeft = {
    0,
    1220,
    0,
    1,
    _gActor400600ZebraStalkerBurstFootLeftPartVerts,
    _gActor400600ZebraStalkerBurstFootLeftVerts,
    _gActor400600ZebraStalkerBurstFootLeftNormals,
    _gActor400600ZebraStalkerBurstFootLeftSkeleton,
    _gActor400600ZebraStalkerBurstFootLeftStream,
};

static TmdBone _gActor400600StalkerBurstFootRightSkeleton[1] = {
#include "assets/stalker_burst_foot_right_skeleton.inc"
};

static u32 _gActor400600StalkerBurstFootRightPartVerts[1] = {
#include "assets/stalker_burst_foot_right_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstFootRightVerts[19] = {
#include "assets/stalker_burst_foot_right_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstFootRightNormals[25] = {
#include "assets/stalker_burst_foot_right_normals.inc"
};

static u32 _gActor400600StalkerBurstFootRightStream[188] = {
#include "assets/stalker_burst_foot_right_stream.inc"
};

static TmdSource _gActor400600StalkerBurstFootRight = {
    0,
    1220,
    0,
    1,
    _gActor400600StalkerBurstFootRightPartVerts,
    _gActor400600StalkerBurstFootRightVerts,
    _gActor400600StalkerBurstFootRightNormals,
    _gActor400600StalkerBurstFootRightSkeleton,
    _gActor400600StalkerBurstFootRightStream,
};

static TmdBone _gActor400600ZebraStalkerBurstArmLeftSkeleton[1] = {
#include "assets/zebra_stalker_burst_arm_left_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeftPartVerts[1] = {
#include "assets/zebra_stalker_burst_arm_left_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeftVerts[10] = {
#include "assets/zebra_stalker_burst_arm_left_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeftNormals[17] = {
#include "assets/zebra_stalker_burst_arm_left_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeftStream[85] = {
#include "assets/zebra_stalker_burst_arm_left_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstArmLeft = {
    0,
    528,
    0,
    1,
    _gActor400600ZebraStalkerBurstArmLeftPartVerts,
    _gActor400600ZebraStalkerBurstArmLeftVerts,
    _gActor400600ZebraStalkerBurstArmLeftNormals,
    _gActor400600ZebraStalkerBurstArmLeftSkeleton,
    _gActor400600ZebraStalkerBurstArmLeftStream,
};

static TmdBone _gActor400600ZebraStalkerBurstArmLeft13064Skeleton[1] = {
#include "assets/zebra_stalker_burst_arm_left_13064_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeft13064PartVerts[1] = {
#include "assets/zebra_stalker_burst_arm_left_13064_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeft13064Verts[10] = {
#include "assets/zebra_stalker_burst_arm_left_13064_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeft13064Normals[17] = {
#include "assets/zebra_stalker_burst_arm_left_13064_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeft13064Stream[85] = {
#include "assets/zebra_stalker_burst_arm_left_13064_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstArmLeft13064 = {
    0,
    528,
    0,
    1,
    _gActor400600ZebraStalkerBurstArmLeft13064PartVerts,
    _gActor400600ZebraStalkerBurstArmLeft13064Verts,
    _gActor400600ZebraStalkerBurstArmLeft13064Normals,
    _gActor400600ZebraStalkerBurstArmLeft13064Skeleton,
    _gActor400600ZebraStalkerBurstArmLeft13064Stream,
};

DamageAttack D_actor_400600_80144EA8[2] = {
    { 26, 7 },
    { 10, 7 },
};

EnemyParams D_actor_400600_80144EB0 = { D_actor_400600_80144EA8, 180, 106, 36, 5, 100, 10, 100, 10 };

static AnimationPackedPose _gActor400600Animation136D4Bank1[14] = {
#include "assets/actor_400600_animation_136D4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation136D4Bank4[144] = {
#include "assets/actor_400600_animation_136D4_bank4.inc"
};

static AnimationRecord _gActor400600Animation136D4Records[202] = {
#include "assets/actor_400600_animation_136D4_records.inc"
};

static u16 _gActor400600Animation136D4Indices[18] = {
#include "assets/actor_400600_animation_136D4_indices.inc"
};

static AnimationSet _gActor400600Animation136D4 = {
    _gActor400600Animation136D4Records,
    _gActor400600Animation136D4Indices,
    { NULL, _gActor400600Animation136D4Bank1, NULL, NULL, _gActor400600Animation136D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation13DA0Bank1[22] = {
#include "assets/actor_400600_animation_13DA0_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation13DA0Bank4[144] = {
#include "assets/actor_400600_animation_13DA0_bank4.inc"
};

static AnimationRecord _gActor400600Animation13DA0Records[206] = {
#include "assets/actor_400600_animation_13DA0_records.inc"
};

static u16 _gActor400600Animation13DA0Indices[18] = {
#include "assets/actor_400600_animation_13DA0_indices.inc"
};

static AnimationSet _gActor400600Animation13DA0 = {
    _gActor400600Animation13DA0Records,
    _gActor400600Animation13DA0Indices,
    { NULL, _gActor400600Animation13DA0Bank1, NULL, NULL, _gActor400600Animation13DA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation14210Bank1[8] = {
#include "assets/actor_400600_animation_14210_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation14210Bank4[95] = {
#include "assets/actor_400600_animation_14210_bank4.inc"
};

static AnimationRecord _gActor400600Animation14210Records[146] = {
#include "assets/actor_400600_animation_14210_records.inc"
};

static u16 _gActor400600Animation14210Indices[18] = {
#include "assets/actor_400600_animation_14210_indices.inc"
};

static AnimationSet _gActor400600Animation14210 = {
    _gActor400600Animation14210Records,
    _gActor400600Animation14210Indices,
    { NULL, _gActor400600Animation14210Bank1, NULL, NULL, _gActor400600Animation14210Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation14980Bank1[21] = {
#include "assets/actor_400600_animation_14980_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation14980Bank4[164] = {
#include "assets/actor_400600_animation_14980_bank4.inc"
};

static AnimationRecord _gActor400600Animation14980Records[230] = {
#include "assets/actor_400600_animation_14980_records.inc"
};

static u16 _gActor400600Animation14980Indices[18] = {
#include "assets/actor_400600_animation_14980_indices.inc"
};

static AnimationSet _gActor400600Animation14980 = {
    _gActor400600Animation14980Records,
    _gActor400600Animation14980Indices,
    { NULL, _gActor400600Animation14980Bank1, NULL, NULL, _gActor400600Animation14980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation151E4Bank1[21] = {
#include "assets/actor_400600_animation_151E4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation151E4Bank4[186] = {
#include "assets/actor_400600_animation_151E4_bank4.inc"
};

static AnimationRecord _gActor400600Animation151E4Records[269] = {
#include "assets/actor_400600_animation_151E4_records.inc"
};

static u16 _gActor400600Animation151E4Indices[18] = {
#include "assets/actor_400600_animation_151E4_indices.inc"
};

static AnimationSet _gActor400600Animation151E4 = {
    _gActor400600Animation151E4Records,
    _gActor400600Animation151E4Indices,
    { NULL, _gActor400600Animation151E4Bank1, NULL, NULL, _gActor400600Animation151E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation15954Bank1[20] = {
#include "assets/actor_400600_animation_15954_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation15954Bank4[174] = {
#include "assets/actor_400600_animation_15954_bank4.inc"
};

static AnimationRecord _gActor400600Animation15954Records[223] = {
#include "assets/actor_400600_animation_15954_records.inc"
};

static u16 _gActor400600Animation15954Indices[18] = {
#include "assets/actor_400600_animation_15954_indices.inc"
};

static AnimationSet _gActor400600Animation15954 = {
    _gActor400600Animation15954Records,
    _gActor400600Animation15954Indices,
    { NULL, _gActor400600Animation15954Bank1, NULL, NULL, _gActor400600Animation15954Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation16118Bank1[16] = {
#include "assets/actor_400600_animation_16118_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation16118Bank4[188] = {
#include "assets/actor_400600_animation_16118_bank4.inc"
};

static AnimationRecord _gActor400600Animation16118Records[242] = {
#include "assets/actor_400600_animation_16118_records.inc"
};

static u16 _gActor400600Animation16118Indices[18] = {
#include "assets/actor_400600_animation_16118_indices.inc"
};

static AnimationSet _gActor400600Animation16118 = {
    _gActor400600Animation16118Records,
    _gActor400600Animation16118Indices,
    { NULL, _gActor400600Animation16118Bank1, NULL, NULL, _gActor400600Animation16118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation169A8Bank1[16] = {
#include "assets/actor_400600_animation_169A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation169A8Bank4[212] = {
#include "assets/actor_400600_animation_169A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation169A8Records[269] = {
#include "assets/actor_400600_animation_169A8_records.inc"
};

static u16 _gActor400600Animation169A8Indices[18] = {
#include "assets/actor_400600_animation_169A8_indices.inc"
};

static AnimationSet _gActor400600Animation169A8 = {
    _gActor400600Animation169A8Records,
    _gActor400600Animation169A8Indices,
    { NULL, _gActor400600Animation169A8Bank1, NULL, NULL, _gActor400600Animation169A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation16E3CBank1[9] = {
#include "assets/actor_400600_animation_16E3C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation16E3CBank4[106] = {
#include "assets/actor_400600_animation_16E3C_bank4.inc"
};

static AnimationRecord _gActor400600Animation16E3CRecords[141] = {
#include "assets/actor_400600_animation_16E3C_records.inc"
};

static u16 _gActor400600Animation16E3CIndices[18] = {
#include "assets/actor_400600_animation_16E3C_indices.inc"
};

static AnimationSet _gActor400600Animation16E3C = {
    _gActor400600Animation16E3CRecords,
    _gActor400600Animation16E3CIndices,
    { NULL, _gActor400600Animation16E3CBank1, NULL, NULL, _gActor400600Animation16E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation17820Bank1[19] = {
#include "assets/actor_400600_animation_17820_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation17820Bank4[245] = {
#include "assets/actor_400600_animation_17820_bank4.inc"
};

static AnimationRecord _gActor400600Animation17820Records[312] = {
#include "assets/actor_400600_animation_17820_records.inc"
};

static u16 _gActor400600Animation17820Indices[18] = {
#include "assets/actor_400600_animation_17820_indices.inc"
};

static AnimationSet _gActor400600Animation17820 = {
    _gActor400600Animation17820Records,
    _gActor400600Animation17820Indices,
    { NULL, _gActor400600Animation17820Bank1, NULL, NULL, _gActor400600Animation17820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation17D50Bank1[9] = {
#include "assets/actor_400600_animation_17D50_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation17D50Bank4[120] = {
#include "assets/actor_400600_animation_17D50_bank4.inc"
};

static AnimationRecord _gActor400600Animation17D50Records[166] = {
#include "assets/actor_400600_animation_17D50_records.inc"
};

static u16 _gActor400600Animation17D50Indices[18] = {
#include "assets/actor_400600_animation_17D50_indices.inc"
};

static AnimationSet _gActor400600Animation17D50 = {
    _gActor400600Animation17D50Records,
    _gActor400600Animation17D50Indices,
    { NULL, _gActor400600Animation17D50Bank1, NULL, NULL, _gActor400600Animation17D50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation18584Bank1[15] = {
#include "assets/actor_400600_animation_18584_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation18584Bank4[202] = {
#include "assets/actor_400600_animation_18584_bank4.inc"
};

static AnimationRecord _gActor400600Animation18584Records[259] = {
#include "assets/actor_400600_animation_18584_records.inc"
};

static u16 _gActor400600Animation18584Indices[18] = {
#include "assets/actor_400600_animation_18584_indices.inc"
};

static AnimationSet _gActor400600Animation18584 = {
    _gActor400600Animation18584Records,
    _gActor400600Animation18584Indices,
    { NULL, _gActor400600Animation18584Bank1, NULL, NULL, _gActor400600Animation18584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1892CBank1[7] = {
#include "assets/actor_400600_animation_1892C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1892CBank4[83] = {
#include "assets/actor_400600_animation_1892C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1892CRecords[111] = {
#include "assets/actor_400600_animation_1892C_records.inc"
};

static u16 _gActor400600Animation1892CIndices[18] = {
#include "assets/actor_400600_animation_1892C_indices.inc"
};

static AnimationSet _gActor400600Animation1892C = {
    _gActor400600Animation1892CRecords,
    _gActor400600Animation1892CIndices,
    { NULL, _gActor400600Animation1892CBank1, NULL, NULL, _gActor400600Animation1892CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation18D74Bank1[9] = {
#include "assets/actor_400600_animation_18D74_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation18D74Bank4[100] = {
#include "assets/actor_400600_animation_18D74_bank4.inc"
};

static AnimationRecord _gActor400600Animation18D74Records[128] = {
#include "assets/actor_400600_animation_18D74_records.inc"
};

static u16 _gActor400600Animation18D74Indices[18] = {
#include "assets/actor_400600_animation_18D74_indices.inc"
};

static AnimationSet _gActor400600Animation18D74 = {
    _gActor400600Animation18D74Records,
    _gActor400600Animation18D74Indices,
    { NULL, _gActor400600Animation18D74Bank1, NULL, NULL, _gActor400600Animation18D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation195A8Bank1[12] = {
#include "assets/actor_400600_animation_195A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation195A8Bank4[191] = {
#include "assets/actor_400600_animation_195A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation195A8Records[279] = {
#include "assets/actor_400600_animation_195A8_records.inc"
};

static u16 _gActor400600Animation195A8Indices[18] = {
#include "assets/actor_400600_animation_195A8_indices.inc"
};

static AnimationSet _gActor400600Animation195A8 = {
    _gActor400600Animation195A8Records,
    _gActor400600Animation195A8Indices,
    { NULL, _gActor400600Animation195A8Bank1, NULL, NULL, _gActor400600Animation195A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation199E0Bank1[6] = {
#include "assets/actor_400600_animation_199E0_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation199E0Bank4[100] = {
#include "assets/actor_400600_animation_199E0_bank4.inc"
};

static AnimationRecord _gActor400600Animation199E0Records[133] = {
#include "assets/actor_400600_animation_199E0_records.inc"
};

static u16 _gActor400600Animation199E0Indices[18] = {
#include "assets/actor_400600_animation_199E0_indices.inc"
};

static AnimationSet _gActor400600Animation199E0 = {
    _gActor400600Animation199E0Records,
    _gActor400600Animation199E0Indices,
    { NULL, _gActor400600Animation199E0Bank1, NULL, NULL, _gActor400600Animation199E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1A5D8Bank1[15] = {
#include "assets/actor_400600_animation_1A5D8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1A5D8Bank4[315] = {
#include "assets/actor_400600_animation_1A5D8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1A5D8Records[387] = {
#include "assets/actor_400600_animation_1A5D8_records.inc"
};

static u16 _gActor400600Animation1A5D8Indices[18] = {
#include "assets/actor_400600_animation_1A5D8_indices.inc"
};

static AnimationSet _gActor400600Animation1A5D8 = {
    _gActor400600Animation1A5D8Records,
    _gActor400600Animation1A5D8Indices,
    { NULL, _gActor400600Animation1A5D8Bank1, NULL, NULL, _gActor400600Animation1A5D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1ABE8Bank1[11] = {
#include "assets/actor_400600_animation_1ABE8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1ABE8Bank4[153] = {
#include "assets/actor_400600_animation_1ABE8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1ABE8Records[183] = {
#include "assets/actor_400600_animation_1ABE8_records.inc"
};

static u16 _gActor400600Animation1ABE8Indices[18] = {
#include "assets/actor_400600_animation_1ABE8_indices.inc"
};

static AnimationSet _gActor400600Animation1ABE8 = {
    _gActor400600Animation1ABE8Records,
    _gActor400600Animation1ABE8Indices,
    { NULL, _gActor400600Animation1ABE8Bank1, NULL, NULL, _gActor400600Animation1ABE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1AF6CBank1[5] = {
#include "assets/actor_400600_animation_1AF6C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1AF6CBank4[79] = {
#include "assets/actor_400600_animation_1AF6C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1AF6CRecords[112] = {
#include "assets/actor_400600_animation_1AF6C_records.inc"
};

static u16 _gActor400600Animation1AF6CIndices[18] = {
#include "assets/actor_400600_animation_1AF6C_indices.inc"
};

static AnimationSet _gActor400600Animation1AF6C = {
    _gActor400600Animation1AF6CRecords,
    _gActor400600Animation1AF6CIndices,
    { NULL, _gActor400600Animation1AF6CBank1, NULL, NULL, _gActor400600Animation1AF6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1B474Bank1[10] = {
#include "assets/actor_400600_animation_1B474_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1B474Bank4[122] = {
#include "assets/actor_400600_animation_1B474_bank4.inc"
};

static AnimationRecord _gActor400600Animation1B474Records[151] = {
#include "assets/actor_400600_animation_1B474_records.inc"
};

static u16 _gActor400600Animation1B474Indices[18] = {
#include "assets/actor_400600_animation_1B474_indices.inc"
};

static AnimationSet _gActor400600Animation1B474 = {
    _gActor400600Animation1B474Records,
    _gActor400600Animation1B474Indices,
    { NULL, _gActor400600Animation1B474Bank1, NULL, NULL, _gActor400600Animation1B474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1B89CBank1[10] = {
#include "assets/actor_400600_animation_1B89C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1B89CBank4[93] = {
#include "assets/actor_400600_animation_1B89C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1B89CRecords[124] = {
#include "assets/actor_400600_animation_1B89C_records.inc"
};

static u16 _gActor400600Animation1B89CIndices[18] = {
#include "assets/actor_400600_animation_1B89C_indices.inc"
};

static AnimationSet _gActor400600Animation1B89C = {
    _gActor400600Animation1B89CRecords,
    _gActor400600Animation1B89CIndices,
    { NULL, _gActor400600Animation1B89CBank1, NULL, NULL, _gActor400600Animation1B89CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1C300Bank1[40] = {
#include "assets/actor_400600_animation_1C300_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1C300Bank4[233] = {
#include "assets/actor_400600_animation_1C300_bank4.inc"
};

static AnimationRecord _gActor400600Animation1C300Records[293] = {
#include "assets/actor_400600_animation_1C300_records.inc"
};

static u16 _gActor400600Animation1C300Indices[18] = {
#include "assets/actor_400600_animation_1C300_indices.inc"
};

static AnimationSet _gActor400600Animation1C300 = {
    _gActor400600Animation1C300Records,
    _gActor400600Animation1C300Indices,
    { NULL, _gActor400600Animation1C300Bank1, NULL, NULL, _gActor400600Animation1C300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1C654Bank1[6] = {
#include "assets/actor_400600_animation_1C654_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1C654Bank4[75] = {
#include "assets/actor_400600_animation_1C654_bank4.inc"
};

static AnimationRecord _gActor400600Animation1C654Records[101] = {
#include "assets/actor_400600_animation_1C654_records.inc"
};

static u16 _gActor400600Animation1C654Indices[18] = {
#include "assets/actor_400600_animation_1C654_indices.inc"
};

static AnimationSet _gActor400600Animation1C654 = {
    _gActor400600Animation1C654Records,
    _gActor400600Animation1C654Indices,
    { NULL, _gActor400600Animation1C654Bank1, NULL, NULL, _gActor400600Animation1C654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1CB40Bank1[11] = {
#include "assets/actor_400600_animation_1CB40_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1CB40Bank4[111] = {
#include "assets/actor_400600_animation_1CB40_bank4.inc"
};

static AnimationRecord _gActor400600Animation1CB40Records[152] = {
#include "assets/actor_400600_animation_1CB40_records.inc"
};

static u16 _gActor400600Animation1CB40Indices[18] = {
#include "assets/actor_400600_animation_1CB40_indices.inc"
};

static AnimationSet _gActor400600Animation1CB40 = {
    _gActor400600Animation1CB40Records,
    _gActor400600Animation1CB40Indices,
    { NULL, _gActor400600Animation1CB40Bank1, NULL, NULL, _gActor400600Animation1CB40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D0E4Bank1[10] = {
#include "assets/actor_400600_animation_1D0E4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D0E4Bank4[139] = {
#include "assets/actor_400600_animation_1D0E4_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D0E4Records[173] = {
#include "assets/actor_400600_animation_1D0E4_records.inc"
};

static u16 _gActor400600Animation1D0E4Indices[18] = {
#include "assets/actor_400600_animation_1D0E4_indices.inc"
};

static AnimationSet _gActor400600Animation1D0E4 = {
    _gActor400600Animation1D0E4Records,
    _gActor400600Animation1D0E4Indices,
    { NULL, _gActor400600Animation1D0E4Bank1, NULL, NULL, _gActor400600Animation1D0E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D2A8Bank1[2] = {
#include "assets/actor_400600_animation_1D2A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D2A8Bank4[16] = {
#include "assets/actor_400600_animation_1D2A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D2A8Records[72] = {
#include "assets/actor_400600_animation_1D2A8_records.inc"
};

static u16 _gActor400600Animation1D2A8Indices[18] = {
#include "assets/actor_400600_animation_1D2A8_indices.inc"
};

static AnimationSet _gActor400600Animation1D2A8 = {
    _gActor400600Animation1D2A8Records,
    _gActor400600Animation1D2A8Indices,
    { NULL, _gActor400600Animation1D2A8Bank1, NULL, NULL, _gActor400600Animation1D2A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D46CBank1[2] = {
#include "assets/actor_400600_animation_1D46C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D46CBank4[16] = {
#include "assets/actor_400600_animation_1D46C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D46CRecords[72] = {
#include "assets/actor_400600_animation_1D46C_records.inc"
};

static u16 _gActor400600Animation1D46CIndices[18] = {
#include "assets/actor_400600_animation_1D46C_indices.inc"
};

static AnimationSet _gActor400600Animation1D46C = {
    _gActor400600Animation1D46CRecords,
    _gActor400600Animation1D46CIndices,
    { NULL, _gActor400600Animation1D46CBank1, NULL, NULL, _gActor400600Animation1D46CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D630Bank1[2] = {
#include "assets/actor_400600_animation_1D630_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D630Bank4[16] = {
#include "assets/actor_400600_animation_1D630_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D630Records[72] = {
#include "assets/actor_400600_animation_1D630_records.inc"
};

static u16 _gActor400600Animation1D630Indices[18] = {
#include "assets/actor_400600_animation_1D630_indices.inc"
};

static AnimationSet _gActor400600Animation1D630 = {
    _gActor400600Animation1D630Records,
    _gActor400600Animation1D630Indices,
    { NULL, _gActor400600Animation1D630Bank1, NULL, NULL, _gActor400600Animation1D630Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D7F4Bank1[2] = {
#include "assets/actor_400600_animation_1D7F4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D7F4Bank4[16] = {
#include "assets/actor_400600_animation_1D7F4_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D7F4Records[72] = {
#include "assets/actor_400600_animation_1D7F4_records.inc"
};

static u16 _gActor400600Animation1D7F4Indices[18] = {
#include "assets/actor_400600_animation_1D7F4_indices.inc"
};

static AnimationSet _gActor400600Animation1D7F4 = {
    _gActor400600Animation1D7F4Records,
    _gActor400600Animation1D7F4Indices,
    { NULL, _gActor400600Animation1D7F4Bank1, NULL, NULL, _gActor400600Animation1D7F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1DC70Bank1[8] = {
#include "assets/actor_400600_animation_1DC70_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1DC70Bank4[100] = {
#include "assets/actor_400600_animation_1DC70_bank4.inc"
};

static AnimationRecord _gActor400600Animation1DC70Records[144] = {
#include "assets/actor_400600_animation_1DC70_records.inc"
};

static u16 _gActor400600Animation1DC70Indices[18] = {
#include "assets/actor_400600_animation_1DC70_indices.inc"
};

static AnimationSet _gActor400600Animation1DC70 = {
    _gActor400600Animation1DC70Records,
    _gActor400600Animation1DC70Indices,
    { NULL, _gActor400600Animation1DC70Bank1, NULL, NULL, _gActor400600Animation1DC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1E468Bank1[19] = {
#include "assets/actor_400600_animation_1E468_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1E468Bank4[188] = {
#include "assets/actor_400600_animation_1E468_bank4.inc"
};

static AnimationRecord _gActor400600Animation1E468Records[246] = {
#include "assets/actor_400600_animation_1E468_records.inc"
};

static u16 _gActor400600Animation1E468Indices[18] = {
#include "assets/actor_400600_animation_1E468_indices.inc"
};

static AnimationSet _gActor400600Animation1E468 = {
    _gActor400600Animation1E468Records,
    _gActor400600Animation1E468Indices,
    { NULL, _gActor400600Animation1E468Bank1, NULL, NULL, _gActor400600Animation1E468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1ECE8Bank1[22] = {
#include "assets/actor_400600_animation_1ECE8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1ECE8Bank4[199] = {
#include "assets/actor_400600_animation_1ECE8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1ECE8Records[260] = {
#include "assets/actor_400600_animation_1ECE8_records.inc"
};

static u16 _gActor400600Animation1ECE8Indices[18] = {
#include "assets/actor_400600_animation_1ECE8_indices.inc"
};

static AnimationSet _gActor400600Animation1ECE8 = {
    _gActor400600Animation1ECE8Records,
    _gActor400600Animation1ECE8Indices,
    { NULL, _gActor400600Animation1ECE8Bank1, NULL, NULL, _gActor400600Animation1ECE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1F4ECBank1[11] = {
#include "assets/actor_400600_animation_1F4EC_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1F4ECBank4[183] = {
#include "assets/actor_400600_animation_1F4EC_bank4.inc"
};

static AnimationRecord _gActor400600Animation1F4ECRecords[277] = {
#include "assets/actor_400600_animation_1F4EC_records.inc"
};

static u16 _gActor400600Animation1F4ECIndices[20] = {
#include "assets/actor_400600_animation_1F4EC_indices.inc"
};

static AnimationSet _gActor400600Animation1F4EC = {
    _gActor400600Animation1F4ECRecords,
    _gActor400600Animation1F4ECIndices,
    { NULL, _gActor400600Animation1F4ECBank1, NULL, NULL, _gActor400600Animation1F4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1FC00Bank1[14] = {
#include "assets/actor_400600_animation_1FC00_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1FC00Bank4[169] = {
#include "assets/actor_400600_animation_1FC00_bank4.inc"
};

static AnimationRecord _gActor400600Animation1FC00Records[222] = {
#include "assets/actor_400600_animation_1FC00_records.inc"
};

static u16 _gActor400600Animation1FC00Indices[20] = {
#include "assets/actor_400600_animation_1FC00_indices.inc"
};

static AnimationSet _gActor400600Animation1FC00 = {
    _gActor400600Animation1FC00Records,
    _gActor400600Animation1FC00Indices,
    { NULL, _gActor400600Animation1FC00Bank1, NULL, NULL, _gActor400600Animation1FC00Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_400600_80151A48[3] = {
    NULL,
    &_gActor400600Animation1F4EC,
    &_gActor400600Animation1FC00,
};

AnimationSet* D_actor_400600_80151A54[35] = {
    NULL,
    &_gActor400600Animation136D4,
    &_gActor400600Animation13DA0,
    &_gActor400600Animation14210,
    &_gActor400600Animation14980,
    &_gActor400600Animation151E4,
    &_gActor400600Animation15954,
    &_gActor400600Animation16118,
    &_gActor400600Animation169A8,
    &_gActor400600Animation16E3C,
    &_gActor400600Animation17820,
    &_gActor400600Animation17D50,
    &_gActor400600Animation18584,
    &_gActor400600Animation1892C,
    &_gActor400600Animation18D74,
    &_gActor400600Animation195A8,
    &_gActor400600Animation199E0,
    &_gActor400600Animation1A5D8,
    &_gActor400600Animation1ABE8,
    &_gActor400600Animation1AF6C,
    &_gActor400600Animation1B474,
    &_gActor400600Animation1B89C,
    &_gActor400600Animation1C300,
    NULL,
    NULL,
    &_gActor400600Animation1C654,
    &_gActor400600Animation1CB40,
    &_gActor400600Animation1D0E4,
    &_gActor400600Animation1D2A8,
    &_gActor400600Animation1D46C,
    &_gActor400600Animation1D630,
    &_gActor400600Animation1D7F4,
    &_gActor400600Animation1DC70,
    &_gActor400600Animation1E468,
    &_gActor400600Animation1ECE8,
};

TaskMessageEntry D_actor_400600_80151AE0[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _stalkerZebraIvoryApplyRoomCommand },
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor400600HandleHoldReleaseMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_400600_80151AF8[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor400600InitLeftArmTask, { .model = &_gActor400600ZebraStalkerBurstArmLeft13064 } },
    { { { TASK_BODY_TMD, 96 } }, _actor400600InitRightArmTask, { .model = &_gActor400600ZebraStalkerBurstArmLeft } },
};

TaskDesc D_actor_400600_80151B10 = { { { TASK_BODY_TMD, 96 } }, _actor400600Task, { .model = &_gActor400600ZebraStalkerBody } };

u8 gStalkerZebraIvoryResumeClips[36] = {
    26,
    26,
    26,
    27,
    27,
    15,
    15,
    26,
    26,
    26,
    26,
    27,
    27,
    15,
    15,
    26,
    26,
    27,
    27,
    30,
    27,
    26,
    26,
    26,
    26,
    26,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    0,
};

ActorZone D_actor_400600_80151B40[7] = {
    { 3000, -1500, 3000, 3000, 6 },
    { -3000, -3000, 6000, 6000, 1 },
    { 3000, -6000, 3000, 12000, 2 },
    { -6000, -6000, 9000, 3000, 3 },
    { -6000, -3000, 3000, 9000, 4 },
    { -3000, 3000, 5400, 3000, 5 },
    { 0, 0, 0, 0, ACTOR_ZONE_END },
};

s16 D_actor_400600_80151B88[12] = {
    1,
    3,
    5,
    6,
    8,
    9,
    11,
    12,
    14,
    15,
    17,
    -1,
};

static inline void _actor400600SetCoordYaw(GfxCoord* coord, s16 yaw);

#include "../../shared/stalker_zebra_ivory_inlines.inc.c"

/// Links the hit sphere, wall-probe capsule and two arm attack spheres.
///
/// Requires zeroed live work, all eighteen model coordinates and the attack
/// table. Initializes the embedded contact arrays (8 body, 8 probe, 1 per arm).
/// Only the receiving sphere starts pair-enabled; grid tests start disabled.
/// Its radius is 608 in the night motel loft/balcony, otherwise 512 world units.
/// Capsule endpoint Y values rely on the zeroed work. The task owns every shape
/// and contact array until unlinking during death; nothing is allocated here.
static void _actor400600InitCollisionBodies(Task* task)
{
    enum { ACTOR_400600_BODY_ID = 6 };
    _Actor400600ZebraStalkerWork* work;

/// Links and initializes one inactive arm attack sphere and its contacts.
///
/// Captures this function's live `task` and `work`. `body` and `contactArray`
/// must be member tokens selecting an unlinked sphere and its whole contact
/// array. Part and local-X expressions are evaluated once. Uses attack entry 0
/// and radius 400 in part-local game units. Expands to a compound statement;
/// use only as a standalone statement here. Undefined after the two calls.
#define ACTOR_400600_INIT_ARM_ATTACK_SPHERE(body, contactArray, partIndex, centerX)                \
    {                                                                                              \
        work->body.key              = damagePackAttackKey(D_actor_400600_80144EA8, 0);             \
        work->body.coord            = &task->extra.tmd->coords[(partIndex)];                       \
        work->body.context.contacts = work->contactArray;                                          \
        work->body.pos.vx           = (centerX);                                                   \
        work->body.pos.vy           = 0;                                                           \
        work->body.pos.vz           = 0;                                                           \
        work->body.radius           = 400;                                                         \
        work->body.flags            = WORLD_COLLISION_BODY_SPHERE;                                 \
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);                   \
        worldCollisionInitContacts(work->contactArray, ARRAY_SIZE(work->contactArray), 0);         \
        work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED); \
    }

    work                        = task->work;
    work->body.coord            = &task->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0x96;
    work->body.pos.vz           = 0x110;
    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_400600_BODY_ID;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT && (gGameSession->location.loc.area == GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT || gGameSession->location.loc.area == GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY)) {
        work->body.radius = 0x260;
    } else {
        work->body.radius = 0x200;
    }
    work->body.flags = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    // Keep the root probe linked but inactive until a move asks for a wall test.
    work->capsule.ends[0].vz          = ACTOR_400600_PROBE_LENGTH;
    work->capsule.end0Radius          = ACTOR_400600_THIN_PROBE_RADIUS;
    work->capsule.end1Radius          = ACTOR_400600_THIN_PROBE_RADIUS;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.contacts            = work->capsuleContacts;
    work->body.flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->capsuleBody.coord           = task->extra.tmd->coords;
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = -0x190;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_400600_BODY_ID;
    work->capsuleBody.radius          = 0;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->capsuleBody);
    worldCollisionInitContacts(work->capsuleContacts, ARRAY_SIZE(work->capsuleContacts), 0);
    work->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    // Both arm spheres use the first attack entry and stay inactive until a strike.
    ACTOR_400600_INIT_ARM_ATTACK_SPHERE(rightArmBody, rightArmContacts, 7, -512);
    ACTOR_400600_INIT_ARM_ATTACK_SPHERE(leftArmBody, leftArmContacts, 10, 512);
#undef ACTOR_400600_INIT_ARM_ATTACK_SPHERE
}

/// Copies a projected wall limb shadow into one subtractive textured GPU quad.
///
/// Borrows four packed screen X/Y words in quad-strip order and the projection's
/// screen Z / 4. `shade` is grey modulation, 0..255. Requires word-aligned
/// primitive-buffer space for one POLY_FT4 and a current ordering table covering
/// tags 0..1023: scaled depth wraps through bits 2..11 of the byte offset.
/// The caller rejects failed projections. Only the GPU packet outlives this call;
/// the scratch block is neither retained nor released here.
static inline void _actor400600QueueWallShadowQuad(const ActorLimbShadowScratch* scratch, u8 shade)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
    quad->code                     = ACTOR_400600_SHADOW_QUAD_CODE;
    GPU_PRIMITIVE_XY_WORD(quad, 0) = scratch->screenCorners[0];
    GPU_PRIMITIVE_XY_WORD(quad, 1) = scratch->screenCorners[1];
    GPU_PRIMITIVE_XY_WORD(quad, 2) = scratch->screenCorners[2];
    GPU_PRIMITIVE_XY_WORD(quad, 3) = scratch->screenCorners[3];
    setUV4(quad, ACTOR_400600_SHADOW_U0, ACTOR_400600_SHADOW_V0, ACTOR_400600_SHADOW_U1, ACTOR_400600_SHADOW_V0,
           ACTOR_400600_SHADOW_U0, ACTOR_400600_SHADOW_V1, ACTOR_400600_SHADOW_U1, ACTOR_400600_SHADOW_V1);
    quad->tpage = ACTOR_400600_SHADOW_TPAGE;
    quad->clut  = ACTOR_400600_SHADOW_CLUT;
    setRGB0(quad, shade, shade, shade);
    addPrim((&gGpuCurrentOt[((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), quad);
}

/// Draws a subtractive limb-shadow segment on the wall at world Z `wallZ`.
///
/// Part indices must be in 0..17 of the live Zebra Stalker model; equal indices
/// draw nothing. Composes both parts and uses their world X/Y, narrowed to s16.
/// `halfWidth` is the perpendicular half-width in world units; both ends
/// overhang by half the segment length. `shade` is grey brightness (0..255).
/// Requires current ancestor/view matrices, an initialized scratch stack and
/// space for one GPU quad. Releases scratch after projection; a negative GTE
/// FLAG drops the quad. The cached view matrix is recomposed for projection.
static void _actor400600DrawWallLimbShadow(Task* task, s16 firstPartIndex, s16 secondPartIndex, s16 halfWidth, s16 wallZ, u8 shade)
{
    ActorLimbShadowScratch* scratch;
    s16                     segmentAngle;
    GfxCoord*               secondCoord;
    GfxCoord*               firstCoord;
    s32                     lateralY0;
    s32                     lateralY1;
    s32                     lateralY2;
    s32                     lateralY3;
    s32                     overhangX;
    s32                     overhangY;
    GfxCoord*               coords;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstPartIndex;
    secondCoord = coords + secondPartIndex;
    if (firstPartIndex != secondPartIndex) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorLimbShadowScratch);
        // Flatten both part origins onto the wall in the frame below the view.
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &scratch->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &scratch->secondMatrix);
        scratch->firstPos.vx  = scratch->firstMatrix.t[0];
        scratch->firstPos.vy  = scratch->firstMatrix.t[1];
        scratch->secondPos.vx = scratch->secondMatrix.t[0];
        scratch->secondPos.vy = scratch->secondMatrix.t[1];
        scratch->firstPos.vz  = wallZ;
        scratch->secondPos.vz = wallZ;
        segmentAngle          = ratan2(scratch->secondPos.vx - scratch->firstPos.vx, scratch->secondPos.vy - scratch->firstPos.vy);
        // Widen in X/Y and extend each end by half the segment's length.
        overhangX                  = (scratch->firstPos.vx - scratch->secondPos.vx) / 2;
        overhangY                  = (scratch->firstPos.vy - scratch->secondPos.vy) / 2;
        scratch->corners[0].vx     = overhangX + (scratch->firstPos.vx - ((rcos(segmentAngle) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
        lateralY0                  = rsin(segmentAngle) * halfWidth;
        scratch->corners[0].vz     = wallZ;
        scratch->corners[0].vy     = overhangY + (scratch->firstPos.vy + (lateralY0 >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[1].vx     = overhangX + (scratch->firstPos.vx + ((rcos(segmentAngle) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
        lateralY1                  = rsin(segmentAngle) * halfWidth;
        scratch->corners[1].vz     = wallZ;
        scratch->corners[1].vy     = overhangY + (scratch->firstPos.vy - (lateralY1 >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[2].vx     = (scratch->secondPos.vx - ((rcos(segmentAngle) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - overhangX;
        lateralY2                  = rsin(segmentAngle) * halfWidth;
        scratch->corners[2].vz     = wallZ;
        scratch->corners[2].vy     = (scratch->secondPos.vy + (lateralY2 >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - overhangY;
        scratch->corners[3].vx     = (scratch->secondPos.vx + ((rcos(segmentAngle) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - overhangX;
        lateralY3                  = rsin(segmentAngle) * halfWidth;
        scratch->corners[3].vz     = wallZ;
        scratch->corners[3].vy     = (scratch->secondPos.vy - (lateralY3 >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - overhangY;
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        scratch->depth = RotTransPers4(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2], &scratch->corners[3], &scratch->screenCorners[0], &scratch->screenCorners[1],
                                       &scratch->screenCorners[2], &scratch->screenCorners[3], &scratch->depthCue, &scratch->flag);
        if (scratch->flag >= 0) {
            _actor400600QueueWallShadowQuad(scratch, shade);
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorLimbShadowScratch);
    }
}

/// Draws the Zebra Stalker's thirteen limb-shadow segments on a world-Z wall.
///
/// Requires the live eighteen-part model, current ancestor/view matrices,
/// initialized scratch stack and GPU space for up to thirteen textured quads.
/// `wallZ` is a signed-halfword world coordinate and `shade` an eight-bit grey
/// modulation level. Segment endpoints lie in parts 1, 3 and 5..17; every
/// segment has a perpendicular half-width of 128 world units. Failed projections
/// are skipped by the segment drawer. No work or scratch pointer is retained.
static void _actor400600DrawWallShadows(Task* task, s16 wallZ, u8 shade)
{
    enum { ACTOR_400600_WALL_SHADOW_HALF_WIDTH = 128 };

    _actor400600DrawWallLimbShadow(task, 3, 9, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 9, 0xA, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 0xA, 0xB, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 3, 6, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 6, 7, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 7, 8, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 1, 5, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 1, 0xC, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 0xC, 0xD, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 0xD, 0xE, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 1, 0xF, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 0xF, 0x10, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
    _actor400600DrawWallLimbShadow(task, 0x10, 0x11, ACTOR_400600_WALL_SHADOW_HALF_WIDTH, wallZ, shade);
}

static void func_actor_400600_801328A8(Task* arg0)
{
    GfxCoord*                     coords;
    _Actor400600ZebraStalkerWork* work;
    s32                           sound;
    s32                           pan;

    coords              = arg0->extra.tmd->coords;
    work                = (_Actor400600ZebraStalkerWork*)arg0->work;
    coords->coord.t[0] += (0x4364 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0x760 - coords->coord.t[2]) >> 2;
    work->moveAccel    += 2;
    work->moveSpeed    += work->moveAccel;
    coords->coord.t[1] += work->moveSpeed;
    if (coords->coord.t[1] >= -0x508) {
        padScriptSpawnVariableMotorRamp(0xA, 0xC0, 0x80);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531A0009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        func_dryfield_night_junk_yard_8017D9B8(1);
        _stalkerZebraIvoryRequestClipRestart(arg0, 0x19, (3 * ANIMATION_RATE_ONE));
        coords->coord.t[1] = -0x508;
        work->state++;
    }
}

/// Queues an entrance sound with the Stalker's placement instance and spatial offsets.
///
/// Used by the Junk Yard landing for room-bank and body-bank cues. `cueId`
/// supplies the bank and entry with bits 8..15 clear; the Enemy placement index
/// (0..15) supplies the instance tag. Requires a live Enemy/model, composed root
/// cache and initialized audio-query scratch stack. Pan and attenuation use the
/// sound event's signed-byte units. Retains no pointer; bank scripts and samples
/// must remain loaded through deferred playback.
static inline void _actor400600PlayJunkYardEntranceCue(Task* task, u32 cueId)
{
    enum { ACTOR_400600_SOUND_INSTANCE_SHIFT = 8 };
    Enemy* enemy;
    u32    soundId;
    s32    soundPan;

    enemy      = task->spawnArg2.pointer;
    soundId    = enemy->placeKey;
    soundId  >>= ENEMY_PLACE_INDEX_SHIFT;
    soundId  <<= ACTOR_400600_SOUND_INSTANCE_SHIFT;
    soundId   |= cueId;
    soundPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
    soundPan >>= 24;
    sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Enables the receiving body sphere's pair tests and disables both arm attack spheres.
///
/// Requires the live Zebra Stalker work. Changes only the three pair-enable
/// bits; shape kinds, grid enables, contacts and collision links are preserved.
/// The probe capsule is unaffected.
static inline void _actor400600EnableBodyPairOnly(_Actor400600ZebraStalkerWork* work)
{
    work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Waits out the Junk Yard landing clip, then enables body hits and starts walking.
///
/// Final entry (5) of scripted task state 6. Requires the initialized body rig,
/// model root and Enemy spawn record. Sounds the landing once at counter zero,
/// increments the signed-halfword counter, and tests slot 1's low-halfword
/// completion result. Completion plays body-bank cue 4, enables body pair
/// testing and disables both arm attack spheres before entering running state 2.
static void _actor400600FinishJunkYardLanding(Task* task)
{
    enum { ACTOR_400600_SOUND_JUNK_YARD_LANDING = 0x531A000A,
           ACTOR_400600_TASK_RUNNING            = 1,
           ACTOR_400600_STATE_INITIAL           = 0 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if (work->stateFrames == 0) {
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_JUNK_YARD_LANDING);
    }
    work->stateFrames++;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_BODY_CUE4);
        _actor400600EnableBodyPairOnly(work);
        // Enter the running task, then select walking from its behavior table.
        task->state = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
    }
}

/// Steps the Stalker's increasing downward acceleration and moves the model root.
///
/// Requires live work and root coordinates in their parent frame. Acceleration
/// gains two game-coordinate units per update squared; each acceleration and
/// speed assignment narrows to s16 before the next addition. Root Y is s32.
static inline void _actor400600StepAcceleratedFall(_Actor400600ZebraStalkerWork* work, GfxCoord* rootCoord)
{
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
}

/// Emits the root-local splash ring using the loaded room's copied-offset water effects.
///
/// Requires current root matrices. `verticalOffset` is signed local Y; X/Z radius
/// is 512. Offsets live only through spawn; selected water callbacks read copies.
static inline void _actor400600SpawnSplashRing(GfxCoord* rootCoord, s16 verticalOffset)
{
    SVECTOR splashOffset;
    s32     splashIndex;

    effectSpawn(gRoomEffectWaterRippleId, rootCoord, ACTOR_400600_SPLASH_RIPPLE_HALF_SIZE, NULL);
    for (splashIndex = 0; splashIndex < ACTOR_400600_SPLASH_POINT_COUNT; splashIndex++) {
        splashOffset.vx = (u32)rsin(splashIndex * ACTOR_400600_SPLASH_ANGLE_STEP) >> ACTOR_400600_SPLASH_RADIUS_SHIFT;
        splashOffset.vy = verticalOffset;
        splashOffset.vz = (u32)rcos(splashIndex * ACTOR_400600_SPLASH_ANGLE_STEP) >> ACTOR_400600_SPLASH_RADIUS_SHIFT;
        effectSpawn(gRoomEffectWaterSprayId, rootCoord, ACTOR_400600_SPLASH_SPRAY_ARGUMENT, &splashOffset);
    }
}

/// Accelerates the scripted floor drop into its landing clip.
///
/// Entry 2 of task state 5. Eases root X/Z toward the entrance's floor point,
/// adds two to signed-halfword acceleration, then integrates speed and root Y.
/// Reaching Y zero triggers vibration and the room landing cue, restarts
/// loaded clip 25 at normal rate and advances to entry 3. Requires live work,
/// model/Enemy, a loaded sound bank and the audio-query scratch stack.
static void _actor400600TickFloorDropEntrance(Task* task)
{
    enum { ACTOR_400600_SOUND_JUNK_YARD_LANDING = 0x531A000A };
    GfxCoord*                     rootCoord;
    _Actor400600ZebraStalkerWork* work;

    rootCoord              = task->extra.tmd->coords;
    work                   = task->work;
    rootCoord->coord.t[0] += (0x1C54 - rootCoord->coord.t[0]) >> 2;
    rootCoord->coord.t[2] += (0xED5 - rootCoord->coord.t[2]) >> 2;
    _actor400600StepAcceleratedFall(work, rootCoord);
    if (rootCoord->coord.t[1] >= 0) {
        padScriptSpawnVariableMotorRamp(0x10, 0x80, 0x40);
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_JUNK_YARD_LANDING);
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE);
        rootCoord->coord.t[1] = 0;
        work->state++;
    }
}

/// Places the water entrance on either wall-walk path or directly on the floor.
///
/// Entry 1 of scripted task state 4. Room commands 0/1 latch requests 1/2:
/// restart the walk clip at normal rate, quarter-roll the model against the
/// wall and select entries 2/5, each with its own fixed root/shadow-wall position.
/// Commands 2/3 latch requests 3/4: place the root at either floor start, enable
/// body pair testing, disable arm pair testing and enter running behavior 0.
/// Other requests wait. Requires a live model and work; positions use the root's
/// parent frame, angles use 4096 units per turn, and the request remains latched.
static void _actor400600WaitWaterEntranceCommand(Task* task)
{
    enum {
        ACTOR_400600_WATER_REQUEST_FIRST_WALL_WALK    = 1,
        ACTOR_400600_WATER_REQUEST_SECOND_WALL_WALK   = 2,
        ACTOR_400600_WATER_REQUEST_FIRST_FLOOR_START  = 3,
        ACTOR_400600_WATER_REQUEST_SECOND_FLOOR_START = 4,
        ACTOR_400600_WATER_SECOND_WALL_WALK           = 5,
        ACTOR_400600_TASK_RUNNING                     = 1,
        ACTOR_400600_STATE_INITIAL                    = 0,
        ACTOR_400600_WALL_ROLL                        = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    u8                            entranceRequest;

    work            = task->work;
    entranceRequest = work->roomCommand;
    rootCoord       = task->extra.tmd->coords;
    // Requests latch room commands 0..3 as values 1..4; zero keeps waiting.
    if (entranceRequest == ACTOR_400600_WATER_REQUEST_FIRST_WALL_WALK) {
        rootCoord->coord.t[0] = 0x36B0;
        rootCoord->coord.t[1] = -0x320;
        rootCoord->coord.t[2] = -0x7D0;
        work->pitch           = 0;
        work->yaw             = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        work->roll            = ACTOR_400600_WALL_ROLL;
        work->stateFrames     = 0;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_WALK, ANIMATION_RATE_ONE);
        work->shadowWallZ = -0x7D0;
        work->state++;
    } else if (entranceRequest == ACTOR_400600_WATER_REQUEST_SECOND_WALL_WALK) {
        rootCoord->coord.t[0] = 0x4A38;
        rootCoord->coord.t[1] = -0x320;
        rootCoord->coord.t[2] = -0xFA0;
        work->yaw             = -ACTOR_400600_WALL_ROLL;
        work->pitch           = 0;
        work->roll            = ACTOR_400600_WALL_ROLL;
        work->stateFrames     = 0;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_WALK, ANIMATION_RATE_ONE);
        work->shadowWallZ = -0xFA0;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_WATER_SECOND_WALL_WALK);
    } else if (entranceRequest == ACTOR_400600_WATER_REQUEST_FIRST_FLOOR_START) {
        _actor400600EnableBodyPairOnly(work);
        rootCoord->coord.t[0] = 0x2AF8;
        rootCoord->coord.t[2] = -0x3E8;
        rootCoord->coord.t[1] = 0;
        work->pitch           = 0;
        work->yaw             = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4;
        task->state           = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
    } else if (entranceRequest == ACTOR_400600_WATER_REQUEST_SECOND_FLOOR_START) {
        _actor400600EnableBodyPairOnly(work);
        rootCoord->coord.t[0] = 0x3A98;
        rootCoord->coord.t[2] = -0xBB8;
        rootCoord->coord.t[1] = 0;
        work->pitch           = 0;
        work->yaw             = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        task->state           = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
    }
}

/// Walks the first water-entry wall path for 90 updates, then starts its leap.
///
/// Entry 2 of scripted task state 4, selected by room command 0. Requires a
/// zeroed state counter, the walk clip and wall placement set by entry 1, plus
/// current part transforms and the initialized anchor/sound scratch stack.
/// Reveals at update 38 and eases shadow shade toward 255 from update 39.
/// At 90, sounds water-bank cue 4, resets vertical motion with acceleration -10
/// root units per update squared, blends to leap clip 21 and selects entry 3.
static void _actor400600TickFirstWaterWallWalk(Task* task)
{
    enum { ACTOR_400600_WATER_WALL_REVEAL_FRAME       = 38,
           ACTOR_400600_WATER_WALL_SHADOW_FIRST_FRAME = 39,
           ACTOR_400600_FIRST_WATER_WALL_WALK_FRAMES  = 90,
           ACTOR_400600_WATER_LEAP_INITIAL_ACCEL      = -10 };
    _Actor400600ZebraStalkerWork* work = task->work;
    u32                           soundId;
    s32                           soundPan;

    work->stateFrames++;
    _actor400600TickWallWalk(task);
    if (work->stateFrames == ACTOR_400600_WATER_WALL_REVEAL_FRAME) {
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    }
    if (work->stateFrames >= ACTOR_400600_WATER_WALL_SHADOW_FIRST_FRAME) {
        work->shadowShade = (u16)work->shadowShade + ((ACTOR_400600_SHADOW_SHADE_FULL - work->shadowShade) >> 4);
    }
    // Leave the wall walk for the following water-entry leap.
    if (work->stateFrames == ACTOR_400600_FIRST_WATER_WALL_WALK_FRAMES) {
        soundId    = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId  >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId  <<= 8;
        soundId   |= ACTOR_400600_SOUND_WATER_CUE4;
        soundPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        soundPan >>= 24;
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateFrames = 0;
        work->moveAccel   = ACTOR_400600_WATER_LEAP_INITIAL_ACCEL;
        work->moveSpeed   = 0;
        _actor400600RequestClipBlend(task, ACTOR_400600_CLIP_LEAP, ANIMATION_RATE_ONE, ACTOR_400600_LEAP_BLEND_FRAMES);
        work->state++;
    }
}

/// Leaps from the first water-entry wall path and starts its floor landing.
///
/// Entry 3 of task state 4, with the counter and motion seeded by entry 2.
/// From update 17, fades wall shadows, integrates the fall and levels the root
/// toward X 17800, Z -3000, yaw 3072 (4096 units/turn). Update 34 spawns a
/// splash; 35 and 37 spawn ripples. Y zero clamps the root, sounds the water
/// landing, restarts loaded clip 25 and advances. Requires live work/model,
/// current root matrices, room water effects and the audio-query scratch stack.
static void _actor400600TickFirstWaterEntranceLeap(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    s32                           soundId;
    s32                           soundPan;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    // Keep the wall pose through the windup, then level it during the fall.
    if (work->stateFrames >= ACTOR_400600_WATER_LEAP_MOVE_FRAME) {
        work->shadowShade += -work->shadowShade >> 3;
        _actor400600StepAcceleratedFall(work, rootCoord);
        rootCoord->coord.t[2] += (-0xBB8 - rootCoord->coord.t[2]) >> 3;
        rootCoord->coord.t[0] += (0x4588 - rootCoord->coord.t[0]) >> 2;
        work->yaw             += (3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 - work->yaw) >> 2;
        work->roll            += -work->roll >> 3;
        if (work->stateFrames == ACTOR_400600_WATER_LEAP_SPLASH_FRAME) {
            _actor400600SpawnWaterSplash(task, 0);
        }
        if (work->stateFrames == ACTOR_400600_WATER_FIRST_RIPPLE_FRAME || work->stateFrames == ACTOR_400600_WATER_SECOND_RIPPLE_FRAME) {
            effectSpawn(gRoomEffectWaterRippleId, rootCoord, ACTOR_400600_WATER_LEAP_RIPPLE_HALF_SIZE, NULL);
        }
        if (rootCoord->coord.t[1] >= 0) {
            soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400600_SOUND_WATER_LANDING;
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE);
            rootCoord->coord.t[1] = 0;
            work->state++;
        }
    }
}

/// Leaps from the second water-entry wall path and starts its floor landing.
///
/// Entry 6 of task state 4, with the counter and motion seeded by entry 5.
/// From update 17, fades wall shadows, integrates the fall and levels the root
/// toward X 15000, Z -3000, yaw 1024 (4096 units/turn). Update 34 spawns a
/// splash; 35 and 37 spawn ripples. Y zero clamps the root, sounds the water
/// landing, restarts loaded clip 25 and advances. Requires live work/model,
/// current root matrices, room water effects and the audio-query scratch stack.
static void _actor400600TickSecondWaterEntranceLeap(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    s32                           soundId;
    s32                           soundPan;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    // Keep the wall pose through the windup, then level it during the fall.
    if (work->stateFrames >= ACTOR_400600_WATER_LEAP_MOVE_FRAME) {
        work->shadowShade += -work->shadowShade >> 3;
        _actor400600StepAcceleratedFall(work, rootCoord);
        rootCoord->coord.t[2] += (-0xBB8 - rootCoord->coord.t[2]) >> 3;
        rootCoord->coord.t[0] += (0x3A98 - rootCoord->coord.t[0]) >> 2;
        work->yaw             += (ACTOR_TRANSFORM_ANGLE_TURN / 4 - work->yaw) >> 2;
        work->roll            += -work->roll >> 3;
        if (work->stateFrames == ACTOR_400600_WATER_LEAP_SPLASH_FRAME) {
            _actor400600SpawnWaterSplash(task, 0);
        }
        if (work->stateFrames == ACTOR_400600_WATER_FIRST_RIPPLE_FRAME || work->stateFrames == ACTOR_400600_WATER_SECOND_RIPPLE_FRAME) {
            effectSpawn(gRoomEffectWaterRippleId, rootCoord, ACTOR_400600_WATER_LEAP_RIPPLE_HALF_SIZE, NULL);
        }
        if (rootCoord->coord.t[1] >= 0) {
            soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400600_SOUND_WATER_LANDING;
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE);
            rootCoord->coord.t[1] = 0;
            work->state++;
        }
    }
}

/// Selects an entry of the current task state's handler table and resets its sub-state.
///
/// Requires live Zebra Stalker work and an entry valid for the current task
/// state. Only the two signed-halfword cursors change; animation requests,
/// counters and `Task::state` remain intact.
static inline void _actor400600SelectStateInline(Task* task, s16 state)
{
    _Actor400600ZebraStalkerWork* stateWork = task->work;

    stateWork->state    = state;
    stateWork->subState = 0;
}

/// Starts the group entrance's drop or places its first actor directly on the floor.
///
/// Entry 1 of task state 7. Latched request 1 shows and reveals the body,
/// resets vertical motion and restarts loaded leap clip 21 at Y -3000.
/// Request 2 enables only body pair tests, places the floor start and
/// publishes active-group phase 2 before entering running behavior 0.
/// Other requests wait; the latch remains intact. Requires live work/model and
/// the initialized collision rig; coordinates use the root's parent frame.
static void _actor400600WaitGroupDropEntranceCommand(Task* task)
{
    enum { ACTOR_400600_GROUP_REQUEST_DROP        = 1,
           ACTOR_400600_GROUP_REQUEST_FLOOR_START = 2,
           ACTOR_400600_TASK_RUNNING              = 1,
           ACTOR_400600_STATE_INITIAL             = 0 };
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    GfxCoord*                     rootCoord;
    s32                           entranceRequest;

    work            = task->work;
    model           = task->extra.tmd;
    entranceRequest = work->roomCommand;
    rootCoord       = model->coords;
    if (entranceRequest == ACTOR_400600_GROUP_REQUEST_DROP) {
        padScriptSpawnVariableMotorRamp(0x14, 0xFF, 0x80);
        rootCoord->coord.t[0] = 0xCE4;
        rootCoord->coord.t[1] = -0xBB8;
        rootCoord->coord.t[2] = 0;
        work->pitch           = 0;
        work->yaw             = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4;
        work->roll            = 0;
        model->flags         &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->stateFrames     = 0;
        work->moveAccel       = 0;
        work->moveSpeed       = 0;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LEAP, ANIMATION_RATE_ONE);
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
        work->state++;
    } else if (entranceRequest == ACTOR_400600_GROUP_REQUEST_FLOOR_START) {
        _actor400600EnableBodyPairOnly(work);
        rootCoord->coord.t[0] = -0x6A4;
        rootCoord->coord.t[2] = -0x514;
        rootCoord->coord.t[1] = 0;
        work->yaw             = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        work->shadowShade     = ACTOR_400600_SHADOW_SHADE_FULL;
        work->pitch           = 0;
        work->roll            = 0;
        // Publish the active-group phase before handing this actor to normal behavior.
        gSceneCombatState.zebraStalkerGroupPhase = entranceRequest;
        task->state                              = ACTOR_400600_TASK_RUNNING;
        _actor400600SelectStateInline(task, ACTOR_400600_STATE_INITIAL);
    }
}

static const TaskFuncTable12 D_actor_400600_80131E24 = { {
    _actor400600SelectDeathSequence,
    _stalkerZebraIvoryStartDeathClip,
    _stalkerZebraIvoryTickDeathClip,
    _actor400600StartCorpseBurn,
    _actor400600WaitCorpseBurn,
    _actor400600TickCorpseBurn,
    _actor400600EnterCorpseRelease,
    _actor400600WaitDeathBurst,
    func_actor_400600_8013A864,
    _actor400600StartDeathCeilingFall,
    _actor400600TickDeathCeilingFall,
    _actor400600FinishDeathCeilingFallLanding,
} };

static const TaskFuncTable6 D_actor_400600_80131E54 = { {
    _actor400600HideForJunkYardEntrance,
    func_actor_400600_8013AC14,
    func_actor_400600_801328A8,
    _actor400600StartJunkYardFloorDrop,
    _actor400600TickJunkYardFloorDrop,
    _actor400600FinishJunkYardLanding,
} };

static const TaskFuncTable4 D_actor_400600_80131E6C = { {
    _actor400600HideForFloorDropEntrance,
    _actor400600WaitFloorDropEntranceCommand,
    _actor400600TickFloorDropEntrance,
    _actor400600FinishFloorDropLanding,
} };

static const TaskFuncTable8 D_actor_400600_80131E7C = { {
    _actor400600HideForWaterEntrance,
    _actor400600WaitWaterEntranceCommand,
    _actor400600TickFirstWaterWallWalk,
    _actor400600TickFirstWaterEntranceLeap,
    _actor400600FinishFirstWaterLanding,
    _actor400600TickSecondWaterWallWalk,
    _actor400600TickSecondWaterEntranceLeap,
    _actor400600FinishSecondWaterLanding,
} };

static const TaskFuncTable4 D_actor_400600_80131E9C = { {
    _actor400600HideForGroupDropEntrance,
    _actor400600WaitGroupDropEntranceCommand,
    _actor400600TickGroupEntranceDrop,
    _actor400600FinishGroupDropLanding,
} };

static const TaskFuncTable9 D_actor_400600_80131EAC = { {
    func_actor_400600_80133434,
    func_actor_400600_801337A8,
    _actor400600RunDeathSequence,
    _actor400600RunCorpseRelease,
    _actor400600RunWaterEntrance,
    _actor400600RunGroupDropEntrance,
    _actor400600RunJunkYardEntrance,
    _actor400600RunFloorDropEntrance,
    _actor400600UpdateGroupEntrance,
} };

static void func_actor_400600_80133434(Task* arg0)
{
    TmdObject*                    model;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* w2;
    _Actor400600ZebraStalkerWork* w3;
    _Actor400600ZebraStalkerWork* w4;
    u32                           rnd;

    model      = arg0->extra.tmd;
    enemy      = (Enemy*)arg0->spawnArg2.pointer;
    coord      = model->coords;
    arg0->work = memCalloc(0x770U, false);
    work       = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work == NULL) {
        enemyDestroy(enemy, arg0);
        return;
    }
    _actor400600QueueSoundBank();
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 20, 0, 0)) {
        work->routesByZone = 1;
    }
    model->lightMtx   = &work->lightMtx;
    model->colorMtx   = &work->colorMtx;
    model->flags      = 0;
    arg0->msgTable    = D_actor_400600_80151AE0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[3];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->param                  = &D_actor_400600_80144EB0;
    enemy->recs                   = work->bodyContacts;
    work->effectArg.coord         = &arg0->extra.tmd->coords[3];
    work->effectArg.spawnArgLo    = 0x300;
    work->effectArg.spawnArgHi    = 2;
    enemy->hp = enemy->hpMax = D_actor_400600_80144EB0.hpMax;
    animationInitContext(&work->rig.anim, D_actor_400600_80151A54, model, work->rig.poses, work->rig.slots);

    w2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    w2->animStep    = ANIMATION_RATE_ONE;
    w2->animClip    = 1;
    w2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;

    _stalkerZebraIvoryTickAnimInline(arg0);

    coord->parent = &gGfxViewCoord;
    work->yaw     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    _actor400600InitCollisionBodies(arg0);
    work->body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    func_actor_400600_801356E0(arg0);
    (sceneAcquireBattleRef)(0);
    _actor400600SnapCloaked(arg0, ACTOR_400600_CLOAK);
    w3                 = (_Actor400600ZebraStalkerWork*)arg0->work;
    w3->state          = 0;
    w3->subState       = 0;
    work->spawnX       = coord->coord.t[0];
    work->floorY       = coord->coord.t[1];
    work->spawnZ       = coord->coord.t[2];
    rnd                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = rnd;
    work->frameCount   = rnd >> 0x10;
    work->shadowHeight = work->floorY;
    switch ((u8)arg0->spawnArg1.value >> 4) {
        case 0:
            w4           = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state  = 1;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 1:
            work->inWater = 1;
            w4            = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state   = 4;
            w4->state     = 0;
            w4->subState  = 0;
            break;
        case 2:
            w4           = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state  = 5;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 3:
            w4           = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state  = 6;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 4:
            w4           = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state  = 7;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 5:
            w4           = (_Actor400600ZebraStalkerWork*)arg0->work;
            arg0->state  = 8;
            w4->state    = 0;
            w4->subState = 0;
            break;
    }
}

static const TaskFuncTable18 D_actor_400600_80131EEC = { {
    _actor400600EnterHiddenWait,
    _actor400600WaitHiddenWakeup,
    func_actor_400600_80139110,
    _actor400600RunLightRecoil,
    _actor400600RunHeavyRecoil,
    _actor400600RunStatusHold,
    _actor400600RunLeftStrike,
    _actor400600RunRightStrike,
    _actor400600RunGrab,
    _actor400600RunBackwardLeap,
    _actor400600TickOnBackBehavior,
    _actor400600RunRighting,
    _actor400600RunCeilingLeap,
    _actor400600RunCeilingDrop,
    _actor400600RunCeilingFall,
    stalkerZebraIvoryRunSubStates,
    _actor400600RunCeilingExit,
    _actor400600RunHiddenIdle,
} };

static void func_actor_400600_801337A8(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    Enemy*                        enemy = (Enemy*)arg0->spawnArg2.pointer;
    TaskFuncTable18               fns   = D_actor_400600_80131EEC;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor400600SyncArmModelFlags(arg0, ACTOR_400600_KEEP_ARM_COLOR);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(arg0);
            fns.funcs[work->state](arg0);
            _actor400600TickAttackCooldowns(arg0);
            _actor400600UpdateArmSwing(arg0);
            _actor400600TickCloakFade(arg0);
            _stalkerZebraIvoryTickAnimInline(arg0);
            work->previousAnimationFlags = work->rig.slots[1].status.fields.flags;
            _stalkerZebraIvoryApplyRotationInline(arg0);
            func_actor_400600_80136968(arg0);
            if (enemy->hp <= 0 && work->holding == 0) {
                _Actor400600ZebraStalkerWork* w = (_Actor400600ZebraStalkerWork*)arg0->work;
                arg0->state                     = 2;
                w->state                        = 0;
                w->subState                     = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            _actorRenderUpdateModelColor(arg0);
            _actor400600DrawFloorShadows(arg0, work->shadowHeight, work->shadowShade);
            if (work->cloaked == 0) {
                model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                _actor400600SyncArmModelFlags(arg0, ACTOR_400600_KEEP_ARM_COLOR);
            }
            break;
    }
}

/// Reveals and starts walking when the target comes near or another Stalker dies.
///
/// Running behavior 1. A horizontal target distance below 3000 root-parent
/// units or the scene's death alert sounds body-bank cue 4, begins revealing,
/// engages battle and seeds 30..93 updates before walking can hide again.
/// Otherwise an armed hit reaction can engage battle and replace this behavior.
/// Requires live work, model coordinates and the Enemy spawn record. The target
/// may be the current route waypoint rather than the player's root.
static void _actor400600WaitHiddenWakeup(Task* task)
{
    enum { ACTOR_400600_WAKEUP_TARGET_DISTANCE = 3000,
           ACTOR_400600_HIDE_COOLDOWN_BASE     = 30,
           ACTOR_400600_HIDE_COOLDOWN_MASK     = 63 };
    _Actor400600ZebraStalkerWork* work;
    s32                           soundId;
    s32                           soundPan;
    u32                           randomValue;

    work = task->work;
    if (work->playerDistance < ACTOR_400600_WAKEUP_TARGET_DISTANCE || gSceneCombatState.zebraStalkerDeathAlert != 0) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400600_SOUND_BODY_CUE4;
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
        sceneEngageBattle(1);
        randomValue        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = randomValue;
        work->hideCooldown = ((randomValue >> 0x10) & ACTOR_400600_HIDE_COOLDOWN_MASK) + ACTOR_400600_HIDE_COOLDOWN_BASE;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
        return;
    }
    if ((s16)_actor400600TakeArmedHitReaction(task) != 0) {
        sceneEngageBattle(1);
    }
}

/// Advances the left-arm strike and selects walking or a retreat when its clip ends.
///
/// Requires running behavior 6, sub-state 1, with its frame counter
/// initialized to zero. Enables the left attack sphere on updates 21..27 and
/// sounds the strike at 21. Completion folds both arms and reseeds the attack
/// timer to 45..123 updates. On the floor, facing a nearby player who faces the
/// actor selects the backward leap; otherwise walking resumes.
static void _actor400600TickLeftStrike(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    s32                           baseSoundId;
    u32                           soundId;
    s32                           soundPan;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames == ACTOR_400600_STRIKE_FIRST_HIT_FRAME) {
        baseSoundId = ACTOR_400600_SOUND_STRIKE;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_STRIKE;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->leftArmBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == ACTOR_400600_STRIKE_END_HIT_FRAME) {
        work->leftArmBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    // Fold the arms before choosing the next movement from the current bearings.
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _stalkerZebraIvorySeedTimer(task, ACTOR_400600_STRIKE_TIMER_BASE);
        _stalkerZebraIvoryFoldArms(task);
        if (work->onCeiling == 0 && work->playerDistance < ACTOR_400600_STRIKE_RETREAT_DISTANCE &&
            (u16)(work->targetBearing - ACTOR_400600_STRIKE_FACING_MARGIN) > ACTOR_400600_STRIKE_REAR_ARC_END &&
            (u16)(work->bearingFromPlayer - ACTOR_400600_STRIKE_FACING_MARGIN) > ACTOR_400600_STRIKE_REAR_ARC_END) {
            _actor400600SelectStateInline(task, ACTOR_400600_STATE_LEAP_BACK);
        } else {
            _actor400600SelectStateInline(task, ACTOR_400600_STATE_WALK);
        }
    }
}

/// Advances the right-arm strike and selects walking or a retreat when its clip ends.
///
/// Requires running behavior 7, sub-state 1, with its frame counter
/// initialized to zero. Enables the right attack sphere on updates 21..27 and
/// sounds the strike at 21. Completion folds both arms and reseeds the attack
/// timer to 45..123 updates. On the floor, facing a nearby player who faces the
/// actor selects the backward leap; otherwise walking resumes.
static void _actor400600TickRightStrike(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    s32                           baseSoundId;
    u32                           soundId;
    s32                           soundPan;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames == ACTOR_400600_STRIKE_FIRST_HIT_FRAME) {
        baseSoundId = ACTOR_400600_SOUND_STRIKE;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_STRIKE;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->rightArmBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == ACTOR_400600_STRIKE_END_HIT_FRAME) {
        work->rightArmBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    // Fold the arms before choosing the next movement from the current bearings.
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _stalkerZebraIvorySeedTimer(task, ACTOR_400600_STRIKE_TIMER_BASE);
        _stalkerZebraIvoryFoldArms(task);
        if (work->onCeiling == 0 && work->playerDistance < ACTOR_400600_STRIKE_RETREAT_DISTANCE &&
            (u16)(work->targetBearing - ACTOR_400600_STRIKE_FACING_MARGIN) > ACTOR_400600_STRIKE_REAR_ARC_END &&
            (u16)(work->bearingFromPlayer - ACTOR_400600_STRIKE_FACING_MARGIN) > ACTOR_400600_STRIKE_REAR_ARC_END) {
            _actor400600SelectStateInline(task, ACTOR_400600_STATE_LEAP_BACK);
        } else {
            _actor400600SelectStateInline(task, ACTOR_400600_STATE_WALK);
        }
    }
}

/// Queues a body-animation blend for the next slot update.
///
/// Requires the initialized live rig and a loaded clip (bank entries 1..22 or
/// 25..34). `rate` is a signed sixteenth-frame step, narrowed to each slot's
/// signed byte; `ANIMATION_RATE_ONE` is normal speed. `blendFrames` counts
/// normal-rate frames. Repeating the current clip changes its rate without
/// restarting the blend. This operation advances no animation slots.
static inline void _actor400600SetBlendRequest(Task* task, s16 clipIndex, s16 rate, s16 blendFrames)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    work->animBlend   = blendFrames;
    work->animStep    = rate;
    work->animClip    = clipIndex;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
}

/// Accepts the grab's target probe and starts the player's eight-button struggle hold.
///
/// Sub-state 4 of running behavior 8. Rejects scripted players, probe hits,
/// targets at least 2000 units away and bearings outside the forward arc.
/// A rejected hold request resumes walking or drops from the ceiling.
/// Acceptance locks attachments, disables receiving-body collisions, installs
/// player clip 1 and blends body clip 5, clearing both hold counters.
/// Requires the live player/Enemy, current probe contacts and loaded paired
/// animation banks. Message payloads are borrowed only through dispatch.
static void _actor400600StartGrabHold(Task* task)
{
    enum { ACTOR_400600_CLIP_GRAB_HOLD         = 5,
           ACTOR_400600_GRAB_HOLD_BLEND_FRAMES = 4,
           ACTOR_400600_GRAB_PRESS_COUNT       = 8,
           ACTOR_400600_GRAB_MAX_DISTANCE      = 2000,
           ACTOR_400600_GRAB_FACING_MARGIN     = 0x200,
           ACTOR_400600_GRAB_REJECT_ARC_COUNT  = 0xC01U,
           ACTOR_400600_PLAYER_CLIP_HELD       = 1 };
    AnimationPlayRequest          playerAnimation;
    GameActorButtonPressHold      pressHoldRequest;
    _Actor400600ZebraStalkerWork* work;
    GameActor*                    playerActor;
    s32                           baseSoundId;
    s32                           soundId;
    s32                           soundPan;

    work        = task->work;
    playerActor = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work;
    if (playerActor->mode == GAME_ACTOR_MODE_SCRIPTED || (s16)_stalkerZebraIvoryWallDistance(task) != 0 || work->playerDistance >= ACTOR_400600_GRAB_MAX_DISTANCE || (u32)(work->targetBearing - ACTOR_400600_GRAB_FACING_MARGIN) < ACTOR_400600_GRAB_REJECT_ARC_COUNT) {
        _stalkerZebraIvoryDisableCapsuleGrid(task);
        _actor400600SelectStateInline(task, ACTOR_400600_STATE_WALK);
        _actor400600TickHorizontalWalk(task, work->walkStep);
        return;
    }
    // The player hold handler reads pressCount only, so animation stays unwritten.
    pressHoldRequest.pressCount = ACTOR_400600_GRAB_PRESS_COUNT;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &pressHoldRequest, 0) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        if (work->onCeiling == 0) {
            _actor400600SelectStateInline(task, ACTOR_400600_STATE_WALK);
            return;
        }
        _actor400600SelectStateInline(task, ACTOR_400600_STATE_DROP_FROM_CEILING);
        return;
    }
    // Commit the hold only after the player accepts scripted struggle control.
    work->shadowHeight = work->floorY;
    _stalkerZebraIvoryDisableCapsuleGrid(task);
    work->onCeiling                      = 0;
    Gp_StateC08.flags                   |= ATTACHMENT_FLAG_EVENT_LOCK;
    work->holding                        = 1;
    work->leapY                          = work->floorY;
    playerAnimation.source.sets          = D_actor_400600_80151A48;
    playerAnimation.blend                = ANIMATION_BLEND_RESET;
    playerAnimation.blendFrames          = 0;
    playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    playerAnimation.animationId          = ACTOR_400600_PLAYER_CLIP_HELD;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &playerAnimation, 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_GRAB_HOLD, ANIMATION_RATE_ONE, ACTOR_400600_GRAB_HOLD_BLEND_FRAMES);
    work->stateFrames = 0;
    work->holdFrames  = 0;
    baseSoundId       = ACTOR_400600_SOUND_BODY_CUE4;
    if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
        baseSoundId = ACTOR_400600_SOUND_WATER_CUE4;
    }
    soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->holdLoops = 0;
    work->subState++;
}

/// Holds the Stalker against the player, biting until struggle release or the repeat limit.
///
/// Sub-state 5 of running behavior 8. Eases the root beneath the player in
/// their common parent frame and damages with enemy attack 1 at updates 13/26
/// of each clip repeat. A release-message latch, fatal bite, enemy death or
/// three completed repeats starts body clip 6 and the fall. Unless a bite killed
/// the player, paired release clip 2 is installed too. Requires live player, Enemy,
/// model, loaded paired banks and current root matrices. Payloads/offsets are copied during
/// synchronous dispatch/spawn; player part 4 must remain live through the burst.
/// The burst reads the copied offset and lasts four running updates.
static void _actor400600TickGrabHold(Task* task)
{
    enum { ACTOR_400600_CLIP_GRAB_RELEASE         = 6,
           ACTOR_400600_GRAB_RELEASE_BLEND_FRAMES = 8,
           ACTOR_400600_HOLD_REPEAT_LIMIT         = 3,
           ACTOR_400600_HOLD_CUE_FRAME            = 8,
           ACTOR_400600_HOLD_FIRST_BITE_FRAME     = 13,
           ACTOR_400600_HOLD_SECOND_BITE_FRAME    = 26,
           ACTOR_400600_HOLD_PLAYER_Y_OFFSET      = 900,
           ACTOR_400600_SOUND_HOLD                = 6,
           ACTOR_400600_SOUND_BITE                = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 9,
           ACTOR_400600_SOUND_WATER_BITE          = 0x404A0009,
           ACTOR_400600_PLAYER_CLIP_RELEASED      = 2,
           ACTOR_400600_PLAYER_BITE_PART          = 4,
           ACTOR_400600_BITE_ATTACK_INDEX         = 1,
           ACTOR_400600_BITE_SPLATTER_ARGUMENT    = 0x10100, // Initial puff speed 48, four running updates
           ACTOR_400600_BITE_SPLATTER_Y_OFFSET    = -200 };
    AnimationPlayRequest          playerAnimation;
    SVECTOR                       bloodOffset;
    _Actor400600ZebraStalkerWork* work;
    Enemy*                        enemy;
    GfxCoord*                     rootCoord;
    GfxCoord*                     playerCoord;
    GfxCoord*                     biteCoord;
    s32                           biteCueId;
    s32                           holdSoundId;
    s32                           holdSoundPan;
    s32                           biteSoundId;
    s32                           biteSoundPan;
    s32                           rootY;
    s32                           holdAnchorY;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    enemy                  = task->spawnArg2.pointer;
    playerCoord            = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll            += -work->roll >> 2;
    rootCoord->coord.t[0] += (playerCoord->coord.t[0] - rootCoord->coord.t[0]) >> 2;
    rootCoord->coord.t[2] += (playerCoord->coord.t[2] - rootCoord->coord.t[2]) >> 2;
    rootY                  = rootCoord->coord.t[1];
    holdAnchorY            = rootY + ACTOR_400600_HOLD_PLAYER_Y_OFFSET;
    rootCoord->coord.t[1]  = rootY + ((playerCoord->coord.t[1] - holdAnchorY) >> 2);
    work->stateFrames++;
    if (++work->holdFrames == ACTOR_400600_HOLD_CUE_FRAME) {
        holdSoundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400600_SOUND_HOLD;
        holdSoundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(holdSoundId, holdSoundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // A fatal bite leaves the player's death animation and control intact.
    if (work->playerDied == 1 || work->holdKilledPlayer == 1 || enemy->hp <= 0 || work->holdLoops >= ACTOR_400600_HOLD_REPEAT_LIMIT) {
        work->playerDied = 0;
        if (work->holdKilledPlayer == 0) {
            playerAnimation.source.sets          = D_actor_400600_80151A48;
            playerAnimation.blend                = ANIMATION_BLEND_INTERPOLATE;
            playerAnimation.blendFrames          = ACTOR_400600_GRAB_RELEASE_BLEND_FRAMES;
            playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            playerAnimation.animationId          = ACTOR_400600_PLAYER_CLIP_RELEASED;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &playerAnimation, 0);
        }
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_GRAB_RELEASE, ANIMATION_RATE_ONE, ACTOR_400600_GRAB_RELEASE_BLEND_FRAMES);
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        work->stateFrames = 0;
        work->body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->subState++;
        return;
    }
    // Apply both bite cues per repeat; the clip boundary resets the clip-relative update count.
    if (work->stateFrames == ACTOR_400600_HOLD_FIRST_BITE_FRAME || work->stateFrames == ACTOR_400600_HOLD_SECOND_BITE_FRAME) {
        biteCoord = &gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords[ACTOR_400600_PLAYER_BITE_PART];
        padScriptSpawnVariableMotorRamp(0xA, 0xC0, 8);
        biteCueId = ACTOR_400600_SOUND_BITE;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            biteCueId = ACTOR_400600_SOUND_WATER_BITE;
        }
        biteSoundId  = biteCueId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        biteSoundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(biteSoundId, biteSoundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_400600_BITE_ATTACK_INDEX), 0) != 0) {
            work->holdKilledPlayer = 1;
        }
        bloodOffset.vx = 0;
        bloodOffset.vy = ACTOR_400600_BITE_SPLATTER_Y_OFFSET;
        bloodOffset.vz = 0;
        effectSpawn(EFFECT_HIT_SPLATTER_SPRAY, biteCoord, ACTOR_400600_BITE_SPLATTER_ARGUMENT, &bloodOffset);
    }
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->stateFrames = 0;
        work->holdLoops++;
    }
}

/// Drops off the player and starts the grab's landing animation.
///
/// Sub-state 6 of running behavior 8. Levels roll, enables body pair tests
/// from update 8 and integrates increasing downward acceleration until leapY,
/// the captured floor height. Landing refreshes the player's root cache,
/// emits water effects when needed, sounds cue 3, enables body grid tests and
/// requests a two-frame blend to loaded clip 25 before advancing. Requires
/// live player/work/model, initialized collision and loaded room effects.
static void _actor400600TickGrabReleaseFall(Task* task)
{
    enum { ACTOR_400600_GRAB_RELEASE_FALL_FRAME = 8 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    GfxCoord*                     playerCoord;
    GfxCoord*                     splashCoord;
    s32                           nextY;
    s32                           baseSoundId;
    s32                           soundId;
    s32                           soundPan;

    work        = task->work;
    rootCoord   = task->extra.tmd->coords;
    playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll += -work->roll >> 2;
    work->stateFrames++;
    // Receiving collisions resume while the release falls to the captured floor.
    if (work->stateFrames >= ACTOR_400600_GRAB_RELEASE_FALL_FRAME) {
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        _actor400600StepAcceleratedFall(work, rootCoord);
        nextY = rootCoord->coord.t[1];
        if (nextY >= work->leapY) {
            rootCoord->coord.t[1]     = work->leapY;
            playerCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(playerCoord);
            if (work->inWater != 0) {
                splashCoord = task->extra.tmd->coords;
                _actor400600SpawnSplashRing(splashCoord, ACTOR_400600_SPLASH_LANDING_Y_OFFSET);
            }
            baseSoundId = ACTOR_400600_SOUND_LANDING;
            if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
                baseSoundId = ACTOR_400600_SOUND_WATER_LANDING;
            }
            soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE, ACTOR_400600_LANDING_BLEND_FRAMES);
            work->subState++;
        }
    }
}

/// Consumes the rear wall probe and prepares a backward leap or resumes walking.
///
/// Sub-state 1 of running behavior 9; sub-state 0 enabled an X/Z capsule probe.
/// Reads and clears the previous collision contacts, narrowing the distance to
/// s16. A hit must be 1257..3000 units away; zero means no hit and uses 3000.
/// Reserves 256 units before a hit, computes signed-halfword X/Z steps as one
/// twentieth of that span using 4.12 sine/cosine, then disables the probe in
/// every path. Actual travel ends when the motion phase reaches the floor.
/// An accepted leap blends clip 21 at normal rate, resets its counter/speed,
/// sets vertical acceleration -42 and advances to sub-state 2. Root cache and
/// probe contacts must describe the same current collision frame.
static void _actor400600ResolveBackwardLeapProbe(Task* task)
{
    enum { ACTOR_400600_BACKWARD_LEAP_FIRST_DISTANCE = 1257,
           ACTOR_400600_BACKWARD_LEAP_DISTANCE_COUNT = 1744,
           ACTOR_400600_BACKWARD_LEAP_WALL_MARGIN    = 256,
           ACTOR_400600_BACKWARD_LEAP_STEP_DIVISOR   = 20,
           ACTOR_400600_LEAP_TRIG_FRACTION_BITS      = 12,
           ACTOR_400600_BACKWARD_LEAP_INITIAL_ACCEL  = -42 };
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* animationWork;
    s16                           wallDistance;

    work         = task->work;
    wallDistance = _stalkerZebraIvoryWallDistance(task);
    // Consume the previous probe; a wall hit outside the range cancels the retreat.
    if (wallDistance != 0) {
        if ((u16)(wallDistance - ACTOR_400600_BACKWARD_LEAP_FIRST_DISTANCE) >= (u32)ACTOR_400600_BACKWARD_LEAP_DISTANCE_COUNT) {
            _stalkerZebraIvoryDisableCapsuleGrid(task);
            _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
            return;
        }
        work->leapX = ((rsin(work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) * (wallDistance - ACTOR_400600_BACKWARD_LEAP_WALL_MARGIN)) >> ACTOR_400600_LEAP_TRIG_FRACTION_BITS) / ACTOR_400600_BACKWARD_LEAP_STEP_DIVISOR;
        work->leapZ = ((rcos(work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) * (wallDistance - ACTOR_400600_BACKWARD_LEAP_WALL_MARGIN)) >> ACTOR_400600_LEAP_TRIG_FRACTION_BITS) / ACTOR_400600_BACKWARD_LEAP_STEP_DIVISOR;
    } else {
        work->leapX = ((rsin(work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) * ACTOR_400600_PROBE_LENGTH) >> ACTOR_400600_LEAP_TRIG_FRACTION_BITS) / ACTOR_400600_BACKWARD_LEAP_STEP_DIVISOR;
        work->leapZ = ((rcos(work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) * ACTOR_400600_PROBE_LENGTH) >> ACTOR_400600_LEAP_TRIG_FRACTION_BITS) / ACTOR_400600_BACKWARD_LEAP_STEP_DIVISOR;
    }
    _stalkerZebraIvoryDisableCapsuleGrid(task);
    animationWork              = task->work;
    animationWork->animBlend   = ACTOR_400600_LEAP_BLEND_FRAMES;
    animationWork->animStep    = ANIMATION_RATE_ONE;
    animationWork->animClip    = ACTOR_400600_CLIP_LEAP;
    animationWork->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->moveAccel            = ACTOR_400600_BACKWARD_LEAP_INITIAL_ACCEL;
    work->moveSpeed            = 0;
    work->stateFrames          = 0;
    work->subState++;
}

/// Commits the backward leap after its windup and starts landing on floor contact.
///
/// Sub-state 2 of running behavior 9. The first sixteen updates can take an
/// armed reaction; update 17 clears pending action and defers death. Adds the
/// captured X/Z steps each update, increases s16 acceleration by six and
/// integrates speed until root Y reaches floorY. Landing emits optional water
/// effects and cue 3, requests a two-frame blend to loaded clip 25, permits
/// death again and advances. Requires live work/model and current room effects.
static void _actor400600TickBackwardLeap(Task* task)
{
    enum { ACTOR_400600_BACKWARD_LEAP_COMMIT_FRAME = 17 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    GfxCoord*                     splashCoord;
    s32                           nextY;
    s32                           baseSoundId;
    s32                           soundId;
    s32                           soundPan;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < ACTOR_400600_BACKWARD_LEAP_COMMIT_FRAME) {
        _actor400600TakeArmedHitReaction(task);
        return;
    }
    // Death waits until the committed leap reaches its landing.
    if (work->stateFrames == ACTOR_400600_BACKWARD_LEAP_COMMIT_FRAME) {
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding       = 1;
    }
    rootCoord->coord.t[0] += work->leapX;
    rootCoord->coord.t[2] += work->leapZ;
    work->moveAccel       += 6;
    work->moveSpeed       += work->moveAccel;
    nextY                  = rootCoord->coord.t[1] + work->moveSpeed;
    rootCoord->coord.t[1]  = nextY;
    if (nextY >= work->floorY) {
        rootCoord->coord.t[1] = work->floorY;
        if (work->inWater != 0) {
            splashCoord = task->extra.tmd->coords;
            _actor400600SpawnSplashRing(splashCoord, ACTOR_400600_SPLASH_LANDING_Y_OFFSET);
        }
        baseSoundId = ACTOR_400600_SOUND_LANDING;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_LANDING;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE, ACTOR_400600_LANDING_BLEND_FRAMES);
        work->holding = 0;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_right_itself.inc.c"

/// Selects a running behavior and starts it at its first sub-state.
///
/// Requires this actor's live work in running task state 1. `state` indexes
/// its eighteen behavior handlers (0..17). Leaves frame counters and animation
/// requests for the selected behavior's entry step to initialize.
static inline void _actor400600SelectBehavior(Task* task, s16 state)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    work->state    = state;
    work->subState = 0;
}

/// Commits the ceiling leap after its windup, then attaches the actor overhead.
///
/// The first sixteen updates can take an armed hit reaction. Update seventeen
/// clears the pending action, defers death and disables grid collision. Eases
/// root Y and pitch toward the probed ceiling; arrival restores collision,
/// turns the actor upside down and requests a two-frame landing blend.
static void _actor400600TickCeilingLeap(Task* task)
{
    enum {
        ACTOR_400600_LEAP_COMMIT_FRAME = 17,
        ACTOR_400600_LEAP_CLEARANCE_Y  = 400,
    };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    s32                           baseSoundId;
    s32                           soundId;
    s32                           soundPan;
    s32                           clearanceY;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < ACTOR_400600_LEAP_COMMIT_FRAME) {
        _actor400600TakeArmedHitReaction(task);
        return;
    }
    if (work->stateFrames == ACTOR_400600_LEAP_COMMIT_FRAME) {
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding       = 1;
        work->shadowHeight  = work->leapY;
        work->body.flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    // Keep the clearance sum separate before easing toward the ceiling.
    clearanceY             = rootCoord->coord.t[1] + ACTOR_400600_LEAP_CLEARANCE_Y;
    rootCoord->coord.t[1] += (work->leapY - clearanceY) >> 3;
    work->pitch           += (ACTOR_TRANSFORM_ANGLE_HALF_TURN - work->pitch) >> 3;
    if (work->leapY >= rootCoord->coord.t[1]) {
        baseSoundId = ACTOR_400600_SOUND_LANDING;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_LANDING;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->body.flags     |= WORLD_COLLISION_BODY_GRID_ENABLED;
        rootCoord->coord.t[1] = work->leapY;
        work->pitch           = 0;
        work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        work->yaw            += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        _stalkerZebraIvoryApplyRotationInline(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE, ACTOR_400600_LANDING_BLEND_FRAMES);
        work->holding   = 0;
        work->onCeiling = 1;
        work->subState++;
    }
}

/// Falls from the ceiling while moving the body anchor toward the landing point.
///
/// Samples part 3 for seven updates, then pins its world X/Z while increasing
/// vertical acceleration and turning pitch by 128/4096 turn per update.
/// Crossing the spawn floor height snaps to the landing point, restores the
/// upright orientation, applies the landing blend and advances the sub-state.
static void _actor400600TickCeilingDrop(Task* task)
{
    enum {
        ACTOR_400600_PART_BODY          = 3,
        ACTOR_400600_DROP_ANCHOR_FRAMES = 8,
        ACTOR_400600_DROP_PITCH_STEP    = 128,
    };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < ACTOR_400600_DROP_ANCHOR_FRAMES) {
        _stalkerZebraIvoryReadPartWorldXZ(task, ACTOR_400600_PART_BODY, &work->anchorPos);
        return;
    }
    work->anchorPos.vx += (work->leapX - work->anchorPos.vx) >> 2;
    work->anchorPos.vz += (work->leapZ - work->anchorPos.vz) >> 2;
    _stalkerZebraIvoryPinPartXZ(task, ACTOR_400600_PART_BODY, &work->anchorPos);
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if ((work->pitch & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        work->pitch -= ACTOR_400600_DROP_PITCH_STEP;
    }
    if (work->floorY < rootCoord->coord.t[1]) {
        work->onCeiling       = 0;
        rootCoord->coord.t[0] = work->leapX;
        rootCoord->coord.t[1] = work->floorY;
        rootCoord->coord.t[2] = work->leapZ;
        work->pitch           = 0;
        work->roll            = 0;
        work->yaw            += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        _stalkerZebraIvoryApplyRotationInline(task);
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE, ACTOR_400600_LANDING_BLEND_FRAMES);
        _stalkerZebraIvoryTickAnimInline(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        work->stateFrames = 0;
        work->subState++;
    }
}

/// Sounds the upright landing and returns a completed ceiling drop to walking.
///
/// Clears the death deferral on its first update. Pending reactions take
/// precedence over clip completion; an uninterrupted completion seeds a
/// ceiling cooldown of 210..241 updates and starts the walking behavior.
static void _actor400600FinishCeilingDropLanding(Task* task)
{
    enum {
        ACTOR_400600_CEILING_COOLDOWN_BASE = 210,
        ACTOR_400600_CEILING_COOLDOWN_MASK = 31,
    };
    _Actor400600ZebraStalkerWork* work;
    u32                           soundId;
    s32                           baseSoundId;
    s32                           soundPan;
    u32                           randomValue;

    work = task->work;
    if (work->stateFrames == 0) {
        baseSoundId = ACTOR_400600_SOUND_LANDING;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_LANDING;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->holding = 0;
        work->stateFrames++;
    }
    if ((s16)_stalkerZebraIvoryApplyPendingReaction(task) == 0 && (s16)_stalkerZebraIvoryClipDone(task) != 0) {
        randomValue           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = randomValue;
        work->ceilingCooldown = ((randomValue >> 0x10) & ACTOR_400600_CEILING_COOLDOWN_MASK) + ACTOR_400600_CEILING_COOLDOWN_BASE;
        _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
    }
}

/// Finishes an on-back ceiling-fall landing, then rests or enters status hold.
///
/// The first update sounds the landing and clears death deferral. Once the
/// clip completes, a pending status action is consumed and starts status hold
/// with a 30..157-update on-back countdown. Otherwise blends into the on-back
/// rest clip and advances to the waiting sub-state.
static void _actor400600FinishCeilingFallLanding(Task* task)
{
    enum {
        ACTOR_400600_STATE_STATUS_HOLD           = 5,
        ACTOR_400600_ON_BACK_COUNTDOWN_BASE      = 30,
        ACTOR_400600_ON_BACK_COUNTDOWN_MASK      = 127,
        ACTOR_400600_SOUND_ON_BACK_LANDING       = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 6,
        ACTOR_400600_SOUND_WATER_ON_BACK_LANDING = 0x404A0006,
    };
    _Actor400600ZebraStalkerWork* work;
    u32                           soundId;
    s32                           baseSoundId;
    s32                           soundPan;
    u32                           randomValue;

    work = task->work;
    if (work->stateFrames == 0) {
        work->holding = 0;
        baseSoundId   = ACTOR_400600_SOUND_ON_BACK_LANDING;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_ON_BACK_LANDING;
        }
        soundId  = baseSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateFrames++;
    }
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->pendingAction != STALKER_ZEBRA_IVORY_PENDING_STATUS) {
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_REST, ANIMATION_RATE_ONE, ACTOR_400600_LANDING_BLEND_FRAMES);
            work->subState++;
            return;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        randomValue         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = randomValue;
        work->countdown     = ((randomValue >> 0x10) & ACTOR_400600_ON_BACK_COUNTDOWN_MASK) + ACTOR_400600_ON_BACK_COUNTDOWN_BASE;
        _actor400600SelectBehavior(task, ACTOR_400600_STATE_STATUS_HOLD);
    }
}

/// Spawn the two child models from `D_actor_400600_80151AF8`, parent them to
/// root parts 10 and 7 at +/-0x200 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
static void func_actor_400600_801356E0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;
    GfxCoord*                     root;
    GfxCoord*                     parent;
    GfxCoord*                     parent2;
    Task*                         task;
    TmdObject*                    obj;
    TmdObject*                    dst;
    TmdObject*                    src;
    MATRIX*                       mdst;
    MATRIX*                       pm;
    MATRIX*                       pm2;
    MATRIX                        m;

    root              = arg0->extra.tmd->coords;
    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    parent            = &root[7];
    parent2           = &root[10];
    task              = taskSpawnFromTable(D_actor_400600_80151AF8, 0, 0, 0);
    work->armTasks[0] = task;
    if (task != NULL) {
        obj               = task->extra.tmd;
        coord             = obj->coords;
        obj->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        coord->coord.t[0] = 0x200;
        coord->parent     = parent2;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0;
        pm                = &m;
        gfxSetRotIdentity(pm);
        RotMatrixY(-0x180, pm);
        mdst                   = &coord->coord;
        mdst->m[0][0]          = pm->m[0][0];
        mdst->m[0][1]          = pm->m[0][1];
        mdst->m[0][2]          = pm->m[0][2];
        mdst->m[1][0]          = pm->m[1][0];
        mdst->m[1][1]          = pm->m[1][1];
        mdst->m[1][2]          = pm->m[1][2];
        mdst->m[2][0]          = pm->m[2][0];
        mdst->m[2][1]          = pm->m[2][1];
        mdst->m[2][2]          = pm->m[2][2];
        src                    = arg0->extra.tmd;
        dst                    = task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
        obj->lightMtx = &work->lightMtx;
        obj->colorMtx = &work->colorMtx;
    }
    task = work->armTasks[1] = taskSpawnFromTable(D_actor_400600_80151AF8, 1, 0, 0);
    if (task != NULL) {
        obj                    = task->extra.tmd;
        coord                  = obj->coords;
        obj->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        coord->parent          = parent;
        coord->coord.t[0]      = -0x200;
        coord->coord.t[1]      = 0;
        coord->coord.t[2]      = 0;
        src                    = arg0->extra.tmd;
        dst                    = task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
        pm2 = &m;
        gfxSetRotIdentity(pm2);
        RotMatrixY(0x180, pm2);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm2->m[0][0];
        mdst->m[0][1] = pm2->m[0][1];
        mdst->m[0][2] = pm2->m[0][2];
        mdst->m[1][0] = pm2->m[1][0];
        mdst->m[1][1] = pm2->m[1][1];
        mdst->m[1][2] = pm2->m[1][2];
        mdst->m[2][0] = pm2->m[2][0];
        mdst->m[2][1] = pm2->m[2][1];
        mdst->m[2][2] = pm2->m[2][2];
        obj->lightMtx = &work->lightMtx;
        obj->colorMtx = &work->colorMtx;
    }
}

/// Advances the hand-anchored walking cycle on the floor or ceiling.
///
/// Starts clip 2 at normal rate when needed. Each completed loop resets the
/// frame counter and adopts `nextLoopRate`, a signed sixteenth-frame rate.
/// Left/right hand windows are normal-rate frames 0..11 and 12..21 inclusive;
/// bounds use unsigned-shift conversion and narrow to bytes, with zero rate
/// producing zero bounds. Bounds are computed before adopting the next rate.
/// Each window starts a positional step sound and optionally a water ripple;
/// world X/Z anchoring leaves root Y unchanged.
static void _actor400600TickHorizontalWalk(Task* task, s16 nextLoopRate)
{
    enum {
        ACTOR_400600_WALK_RIPPLE_Y          = -420,
        ACTOR_400600_WALK_RIPPLE_SIZE       = 64, // Initial local half-side, in coordinate units
        ACTOR_400600_SOUND_LEFT_STEP        = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 1,
        ACTOR_400600_SOUND_RIGHT_STEP       = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 2,
        ACTOR_400600_SOUND_WATER_LEFT_STEP  = 0x404A0001,
        ACTOR_400600_SOUND_WATER_RIGHT_STEP = 0x404A0002,
    };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    // Convert clip-frame windows before changing the rate at a loop boundary.
    u32     firstEndTicks;
    u8      secondStartTicks;
    u8      secondEndTicks;
    u8      firstEnd;
    u8      secondStart;
    u8      secondEnd;
    s32     baseSoundId;
    u32     soundId;
    u32     placementVoice;
    s32     soundPan;
    SVECTOR rippleOffset;

    /// Converts a normal-rate walking frame bound to update ticks.
    ///
    /// `taskArg` is a side-effect-free live Task pointer, read once on the zero
    /// path and twice otherwise. `normalFrame` is a nonnegative s32 frame count
    /// whose eight-bit left shift fits s32; it is evaluated once only at nonzero
    /// rate. `result` is a writable u8/u32 local, assigned once in either arm.
    /// Keeps signed division, unsigned shifting and destination-width narrowing.
    /// Expands one complete if/else statement; invoke in explicit braces.
#define ACTOR_400600_CONVERT_WALK_BOUND(taskArg, result, normalFrame)                                                                                                          \
    if (((_Actor400600ZebraStalkerWork*)(taskArg)->work)->animStep == 0) {                                                                                                     \
        (result) = 0;                                                                                                                                                          \
    } else {                                                                                                                                                                   \
        (result) = (u32)(((normalFrame) << ACTOR_400600_FRAME_FRACTION_BITS) / ((_Actor400600ZebraStalkerWork*)(taskArg)->work)->animStep) >> ACTOR_400600_RATE_FRACTION_BITS; \
    }

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animClip != ACTOR_400600_CLIP_WALK) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = ACTOR_400600_CLIP_WALK;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        _stalkerZebraIvoryTickAnimInline(task);
    }
    ACTOR_400600_CONVERT_WALK_BOUND(task, firstEndTicks, ACTOR_400600_WALK_FIRST_END_FRAME);
    firstEnd = firstEndTicks;
    ACTOR_400600_CONVERT_WALK_BOUND(task, secondStartTicks, ACTOR_400600_WALK_SECOND_START_FRAME);
    secondStart = secondStartTicks;
    ACTOR_400600_CONVERT_WALK_BOUND(task, secondEndTicks, ACTOR_400600_WALK_SECOND_END_FRAME);
    secondEnd = secondEndTicks;
#undef ACTOR_400600_CONVERT_WALK_BOUND
    if ((s16)_actor400600ClipWasDone(task) != 0) {
        work->animFrame = 0;
        work->animStep  = nextLoopRate;
    }
    if (work->animFrame == 0) {
        _stalkerZebraIvoryReadPartWorldXZ(task, ACTOR_400600_PART_LEFT_HAND, &work->anchorPos);
        baseSoundId = ACTOR_400600_SOUND_LEFT_STEP;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_LEFT_STEP;
        }
        // Combine the placement voice with the selected step script.
        soundId        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId      >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId      <<= 8;
        placementVoice = soundId;
        soundId        = baseSoundId | placementVoice;
        soundPan       = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        soundPan     >>= 24;
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        if (work->inWater != 0) {
            rippleOffset.vx = 0;
            rippleOffset.vy = ACTOR_400600_WALK_RIPPLE_Y;
            rippleOffset.vz = 0;
            effectSpawn(gRoomEffectWaterRippleId, rootCoord, ACTOR_400600_WALK_RIPPLE_SIZE, &rippleOffset);
        }
    }
    if (work->animFrame == secondStart) {
        _stalkerZebraIvoryReadPartWorldXZ(task, ACTOR_400600_PART_RIGHT_HAND, &work->anchorPos);
        baseSoundId = ACTOR_400600_SOUND_RIGHT_STEP;
        if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
            baseSoundId = ACTOR_400600_SOUND_WATER_RIGHT_STEP;
        }
        soundId        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId      >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId      <<= 8;
        placementVoice = soundId;
        soundId        = baseSoundId | placementVoice;
        soundPan       = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        soundPan     >>= 24;
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        if (work->inWater != 0) {
            rippleOffset.vx = 0;
            rippleOffset.vy = ACTOR_400600_WALK_RIPPLE_Y;
            rippleOffset.vz = 0;
            effectSpawn(gRoomEffectWaterRippleId, rootCoord, ACTOR_400600_WALK_RIPPLE_SIZE, &rippleOffset);
        }
    }
    if (work->animFrame >= 0 && work->animFrame <= firstEnd) {
        _stalkerZebraIvoryPinPartXZ(task, ACTOR_400600_PART_LEFT_HAND, &work->anchorPos);
        work->nextAnchorPart = ACTOR_400600_PART_RIGHT_HAND;
    }
    if (work->animFrame >= secondStart && work->animFrame <= secondEnd) {
        _stalkerZebraIvoryPinPartXZ(task, ACTOR_400600_PART_RIGHT_HAND, &work->anchorPos);
        work->nextAnchorPart = ACTOR_400600_PART_LEFT_HAND;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/stalker_zebra_ivory_tick_on_back_crawl.inc.c"

/// Advances the hand-anchored wall walk used by the water entrance.
///
/// Starts clip 2 at normal rate when needed and loops on completion. Anchors
/// left/right hands in world X/Y during normal-rate frames 0..11 and 12..21
/// inclusive, sounding each window's first tick. Bounds narrow to bytes after
/// unsigned-shift rate conversion; zero rate produces zero bounds. Root Z
/// stays fixed. Requires a live initialized model beneath the composed view.
static void _actor400600TickWallWalk(Task* task)
{
    enum {
        ACTOR_400600_SOUND_WALL_LEFT_STEP  = 0x404A000A,
        ACTOR_400600_SOUND_WALL_RIGHT_STEP = 0x404A000B
    };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    // Preserve byte bounds through the rate conversion and both window tests.
    u8 firstEndTicks;
    u8 secondStartTicks;
    u8 secondEndTicks;
    u8 firstEnd;
    u8 secondStart;
    u8 secondEnd;

    u8  firstStart;
    u32 soundId;
    s32 soundPan;

    /// Converts a normal-rate walking frame bound to update ticks.
    ///
    /// `taskArg` is a side-effect-free live Task pointer, read once on the zero
    /// path and twice otherwise. `normalFrame` is a nonnegative s32 frame count
    /// whose eight-bit left shift fits s32; it is evaluated once only at nonzero
    /// rate. `result` is a writable u8/u32 local, assigned once in either arm.
    /// Keeps signed division, unsigned shifting and destination-width narrowing.
    /// Expands one complete if/else statement; invoke in explicit braces.
#define ACTOR_400600_CONVERT_WALK_BOUND(taskArg, result, normalFrame)                                                                                                          \
    if (((_Actor400600ZebraStalkerWork*)(taskArg)->work)->animStep == 0) {                                                                                                     \
        (result) = 0;                                                                                                                                                          \
    } else {                                                                                                                                                                   \
        (result) = (u32)(((normalFrame) << ACTOR_400600_FRAME_FRACTION_BITS) / ((_Actor400600ZebraStalkerWork*)(taskArg)->work)->animStep) >> ACTOR_400600_RATE_FRACTION_BITS; \
    }

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animClip != ACTOR_400600_CLIP_WALK) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = ACTOR_400600_CLIP_WALK;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        _stalkerZebraIvoryTickAnimInline(task);
    }
    firstStart = 0;
    ACTOR_400600_CONVERT_WALK_BOUND(task, firstEndTicks, ACTOR_400600_WALK_FIRST_END_FRAME);
    firstEnd = firstEndTicks;
    ACTOR_400600_CONVERT_WALK_BOUND(task, secondStartTicks, ACTOR_400600_WALK_SECOND_START_FRAME);
    secondStart = secondStartTicks;
    ACTOR_400600_CONVERT_WALK_BOUND(task, secondEndTicks, ACTOR_400600_WALK_SECOND_END_FRAME);
    secondEnd = secondEndTicks;
#undef ACTOR_400600_CONVERT_WALK_BOUND
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->animFrame = 0;
    }
    if (work->animFrame == firstStart) {
        _actor400600ReadPartWorldXY(task, ACTOR_400600_PART_LEFT_HAND, &work->anchorPos);
        soundId    = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId  >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId  <<= 8;
        soundId   |= ACTOR_400600_SOUND_WALL_LEFT_STEP;
        soundPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        soundPan >>= 24;
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->animFrame == secondStart) {
        _actor400600ReadPartWorldXY(task, ACTOR_400600_PART_RIGHT_HAND, &work->anchorPos);
        soundId    = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId  >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId  <<= 8;
        soundId   |= ACTOR_400600_SOUND_WALL_RIGHT_STEP;
        soundPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        soundPan >>= 24;
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->animFrame >= firstStart && work->animFrame <= firstEnd) {
        _actor400600PinPartXY(task, ACTOR_400600_PART_LEFT_HAND, &work->anchorPos);
        work->nextAnchorPart = ACTOR_400600_PART_RIGHT_HAND;
    }
    if (work->animFrame >= secondStart && work->animFrame <= secondEnd) {
        _actor400600PinPartXY(task, ACTOR_400600_PART_RIGHT_HAND, &work->anchorPos);
        work->nextAnchorPart = ACTOR_400600_PART_LEFT_HAND;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Advances the cloak transition and eases the limb-shadow shade toward its target.
///
/// Reveal finishes after 32 running updates by disabling model semitransparency;
/// hide finishes after 18 by suppressing model drawing and the shadows. A marked
/// hidden enemy remains lockable. Both arm models inherit the body's draw flags.
static void _actor400600TickCloakFade(Task* task)
{
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)task->work;
    TmdObject*                    model = task->extra.tmd;
    Enemy*                        enemy = (Enemy*)task->spawnArg2.pointer;

    if (work->cloaked == 0 && work->cloakFading == 1) {
        work->shadowShade = (u16)work->shadowShade + ((ACTOR_400600_SHADOW_SHADE_FULL - work->shadowShade) >> 5);
        work->cloakFadeFrames++;
        if (work->cloakFadeFrames >= ACTOR_400600_REVEAL_FADE_FRAMES) {
            model->flags &= ~TMD_OBJECT_SEMI_TRANS;
            _actor400600SyncArmModelFlags(task, ACTOR_400600_KEEP_ARM_COLOR);
            work->cloakFadeFrames = 0;
            work->cloakFading     = 0;
        }
    } else if (work->cloaked == 1 && work->cloakFading == 1) {
        work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 3);
        work->cloakFadeFrames++;
        if (work->cloakFadeFrames >= ACTOR_400600_HIDE_FADE_FRAMES) {
            if (work->markedFrames != 0) {
                enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            } else {
                enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
            }
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor400600SyncArmModelFlags(task, ACTOR_400600_KEEP_ARM_COLOR);
            work->cloakFadeFrames = 0;
            work->cloakFading     = 0;
            work->shadowShade     = 0;
        }
    }
}

/// Snapshots the root position and updates the pursuit point and player-relative bearings.
///
/// The target is the player's root, or the next water-tower route waypoint when
/// zone routing is enabled. With no player task, only the root snapshot changes.
/// Position differences narrow to signed 16-bit world units before normalization;
/// `playerDistance` is their horizontal length, and both bearings wrap to 0..4095.
/// The squared X/Z sum must fit s32.
static void _actor400600UpdateTarget(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    GfxCoord*                     playerCoord;
    GameActor*                    playerActor;
    Task*                         playerTask;
    SVECTOR                       targetDirection;
    s16                           actorZone;
    s16                           playerZone;

    work                 = (_Actor400600ZebraStalkerWork*)task->work;
    rootCoord            = task->extra.tmd->coords;
    playerTask           = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = rootCoord->coord.t[0];
    work->prevRootPos.vy = rootCoord->coord.t[1];
    work->prevRootPos.vz = rootCoord->coord.t[2];
    if (playerTask == NULL) {
        return;
    }
    playerCoord = playerTask->extra.tmd->coords;
    playerActor = playerTask->work;
    // Route around the water tower before measuring the target direction.
    if (work->routesByZone != 0) {
        actorZone  = _actor400600FindRouteZone(task);
        playerZone = _actor400600FindRouteZone(playerTask);
        if (actorZone == ACTOR_400600_ZONE_POSITIVE_X && (playerZone == ACTOR_400600_ZONE_CENTER || playerZone == ACTOR_400600_ZONE_PASSAGE)) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else if (actorZone == ACTOR_400600_ZONE_NEGATIVE_Z && ((playerZone >= ACTOR_400600_ZONE_CENTER && playerZone <= ACTOR_400600_ZONE_POSITIVE_X) || playerZone == ACTOR_400600_ZONE_PASSAGE)) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = -0x1194;
        } else if (actorZone == ACTOR_400600_ZONE_NEGATIVE_X && ((playerZone >= ACTOR_400600_ZONE_CENTER && playerZone <= ACTOR_400600_ZONE_NEGATIVE_Z) || playerZone == ACTOR_400600_ZONE_PASSAGE)) {
            work->targetPos.vx = -0x125C;
            work->targetPos.vy = 0;
            work->targetPos.vz = -0x1194;
        } else if (actorZone == ACTOR_400600_ZONE_POSITIVE_Z && ((playerZone >= ACTOR_400600_ZONE_CENTER && playerZone <= ACTOR_400600_ZONE_NEGATIVE_X) || playerZone == ACTOR_400600_ZONE_PASSAGE)) {
            work->targetPos.vx = -0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0x1194;
        } else if (playerZone == ACTOR_400600_ZONE_CENTER && actorZone == ACTOR_400600_ZONE_PASSAGE) {
            work->targetPos.vx = 0;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else if (playerZone != ACTOR_400600_ZONE_CENTER && actorZone == ACTOR_400600_ZONE_CENTER) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else {
            work->targetPos.vx = playerCoord->coord.t[0];
            work->targetPos.vy = playerCoord->coord.t[1];
            work->targetPos.vz = playerCoord->coord.t[2];
        }
    } else {
        work->targetPos.vx = playerCoord->coord.t[0];
        work->targetPos.vy = playerCoord->coord.t[1];
        work->targetPos.vz = playerCoord->coord.t[2];
    }
    targetDirection.vx   = work->targetPos.vx - rootCoord->coord.t[0];
    targetDirection.vy   = work->targetPos.vy - rootCoord->coord.t[1];
    targetDirection.vz   = work->targetPos.vz - rootCoord->coord.t[2];
    work->playerDistance = SquareRoot0(targetDirection.vx * targetDirection.vx + targetDirection.vz * targetDirection.vz);
    VectorNormalSS(&targetDirection, &targetDirection);
    work->targetBearing     = (ratan2(targetDirection.vx, targetDirection.vz) - work->yaw) & ACTOR_TRANSFORM_ANGLE_MASK;
    work->bearingFromPlayer = (ratan2(-targetDirection.vx, -targetDirection.vz) - playerActor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
}

static const TaskFuncTable3 D_actor_400600_80131F34 = { {
    _actor400600StartStatusHold,
    _actor400600WaitStatusHold,
    _actor400600FinishStatusRecovery,
} };

static const TaskFuncTable8 D_actor_400600_80131F40 = { {
    _actor400600StartGrabWindup,
    _actor400600StartGrabLeapWindup,
    _actor400600WaitGrabLeapWindup,
    _actor400600StartGrabTargetProbe,
    _actor400600StartGrabHold,
    _actor400600TickGrabHold,
    _actor400600TickGrabReleaseFall,
    _stalkerZebraIvoryReleaseHold,
} };

static const TaskFuncTable4 D_actor_400600_80131F60 = { {
    _actor400600StartBackwardLeapProbe,
    _actor400600ResolveBackwardLeapProbe,
    _actor400600TickBackwardLeap,
    _stalkerZebraIvoryWaitClip,
} };

static const TaskFuncTable3 D_actor_400600_80131F70 = { {
    _actor400600StartCeilingLeap,
    _actor400600TickCeilingLeap,
    _actor400600FinishCeilingLeapLanding,
} };

static const TaskFuncTable4 D_actor_400600_80131F7C = { {
    _actor400600CommitCeilingDrop,
    _actor400600StartCeilingDrop,
    _actor400600TickCeilingDrop,
    _actor400600FinishCeilingDropLanding,
} };

static const TaskFuncTable4 D_actor_400600_80131F8C = { {
    _actor400600StartCeilingFall,
    _actor400600TickCeilingFall,
    _actor400600FinishCeilingFallLanding,
    _stalkerZebraIvoryFinishCeilingFall,
} };

static const TaskFuncTable3 gStalkerZebraIvorySubStates = { {
    _actor400600StartKnockdownRecoil,
    _actor400600BlendKnockdownRest,
    _actor400600FinishKnockdownRest,
} };

static void func_actor_400600_80136968(Task* arg0)
{
    SVECTOR                       push;
    SVECTOR                       pos;
    WorldCollisionDelta           delta;
    GfxCoord*                     eff;
    s16                           maxX;
    s16                           maxZ;
    s16                           stepX;
    s16                           stepZ;
    u8                            blocked;
    _Actor400600ZebraStalkerWork* work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    s16                           amount;
    s32                           dmg;
    s32                           tmp;
    s16                           tick;
    s32                           i;

    maxX               = 0;
    maxZ               = 0;
    stepX              = 0;
    stepZ              = 0;
    blocked            = 0;
    coord              = arg0->extra.tmd->coords;
    work               = (_Actor400600ZebraStalkerWork*)arg0->work;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    work->pendingArmed = 0;
    eff                = &coord[3];

    for (i = 0; i < 8; i++) {
        switch (work->bodyContacts[i].key.value & 0xFFFF0000) {
            case 0x10000:
            case 0x30000:
                pos.vx = coord->workm.t[0];
                pos.vy = coord->workm.t[1];
                pos.vz = coord->workm.t[2];
                _actorContactCalcHorizontalPushback(&pos, &work->bodyContacts[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->hitCooldown == 0) {
                    work->pendingArmed = 1;
                    dmg                = damageComputePlayerAttack(work->bodyContacts[i].key.value, work->playerDistance, 0, 0);
                    amount             = dmg;
                    work->hitCooldown  = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
                    if (damageRollCriticalHit(enemy, work->bodyContacts[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->bodyContacts[i].key.value, amount, 0);
                    worldTargetAddReadoutAmount(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    if ((work->bodyContacts[i].key.value & 0x7F) == 0xE) {
                        if (!(work->bodyContacts[i].key.value & 0x8000)) {
                            work->markedFrames = 0x258;
                        }
                    } else {
                        effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value),
                                       &arg0->extra.tmd->coords[4], NULL, &work->effectArg);
                    }
                    if (amount >= 0x64) {
                        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_HEAVY;
                    } else {
                        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_LIGHT;
                    }
                    switch (damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(enemy);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->bodyContacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[i].key.value, 0);
                            break;
                        case 4:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_BLAST;
                            break;
                        case 5:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_HEAVY;
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_BLAST;
                            break;
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_HEAVY;
                            break;
                        case 8:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_STATUS;
                            break;
                        case 9:
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_STATUS;
                            break;
                    }
                } else if ((damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value)) == 0xD) {
                    effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
                }
                break;
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->pendingAction   = STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->pendingAction   = STALKER_ZEBRA_IVORY_PENDING_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->damageOverTimeSeen = 1;
        tmp                      = damageTickEnemyDamageOverTime(enemy);
        tick                     = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            worldTargetAddReadoutAmount(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->pendingArmed  = 1;
            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_HEAVY;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    switch (worldCollisionResolvePushback(work->bodyContacts, &delta, 8, NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            stepZ = delta.fixed.vz.halves.integer;
            stepX = delta.fixed.vx.word >> 16;
            if (delta.fixed.vx.word & 0xFFFF) {
                if (delta.fixed.vx.word > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.fixed.vz.word & 0xFFFF) {
                if (delta.fixed.vz.word > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            coord->coord.t[0]   = work->prevRootPos.vx;
            coord->coord.t[2]   = work->prevRootPos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    worldCollisionClearContacts(work->bodyContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
    if (blocked == 0) {
        work->anchorPos.vx += _actor400600SelectCollisionStep(stepX, maxX);
        work->anchorPos.vz += _actor400600SelectCollisionStep(stepZ, maxZ);
        coord->coord.t[0]  += _actor400600SelectCollisionStep(stepX, maxX);
        coord->coord.t[2]  += _actor400600SelectCollisionStep(stepZ, maxZ);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->markedFrames != 0) {
        work->markedFrames--;
        if ((work->markedFrames & 7) == 1) {
            effectSpawn(EFFECT_ADDITIVE_PUFF, eff, 0x10200, NULL);
        }
    }
}

/// Applies this frame's armed hit reaction and cancels any outstanding wall probe.
///
/// Returns 0 unless `pendingArmed` is exactly 1, otherwise 1 even for an unknown
/// pending action. Floor status holds in place; ceiling status/knockdown falls.
/// Ceiling status remains pending for the fall to handle. Every selected behavior
/// starts at sub-state 0; this routine leaves `pendingArmed` for the next hit scan.
static s32 _actor400600TakeArmedHitReaction(Task* task)
{
    enum { ACTOR_400600_STATE_LIGHT_RECOIL = 3,
           ACTOR_400600_STATE_HEAVY_RECOIL = 4,
           ACTOR_400600_STATE_STATUS_HOLD  = 5,
           ACTOR_400600_STATE_CEILING_FALL = 0xE,
           ACTOR_400600_STATE_KNOCKDOWN    = 0xF };
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)task->work;
    if (work->pendingArmed != 1) {
        return 0;
    }
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_LIGHT_RECOIL);
                break;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_HEAVY_RECOIL);
                break;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_STATUS_HOLD);
                break;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_HEAVY_RECOIL);
                break;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_KNOCKDOWN);
                break;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
    } else {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_LIGHT_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_CEILING_FALL);
                break;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_CEILING_FALL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
        }
    }
    // Reactions abandon an attack probe even if the request was unrecognized.
    work->ceilingProbePending = 0;
    _stalkerZebraIvoryDisableCapsuleGrid(task);
    return 1;
}

#include "../../shared/stalker_zebra_ivory_apply_pending_reaction.inc.c"

static void func_actor_400600_80137240(Task* arg0)
{
    EffectWork* eff;
    EffectWork* eff2;
    EffectWork* eff3;
    EffectWork* eff4;
    TmdObject*  dst;
    TmdObject*  dst2;
    TmdObject*  dst3;
    TmdObject*  dst4;
    TmdObject*  src;
    TmdObject*  src2;
    TmdObject*  src3;
    TmdObject*  src4;

    D_800678F0[0] = &_gActor400600ZebraStalkerBurstHead;
    eff           = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[4], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerEffect;
    eff2          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[2], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdBuildBufferHalf(dst2);
            tmdBuildBufferHalf(dst2);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerBurstHandLeft;
    eff3          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = arg0->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdBuildBufferHalf(dst3);
            tmdBuildBufferHalf(dst3);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerBurstFootRight;
    eff4          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4                    = arg0->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdBuildBufferHalf(dst4);
            tmdBuildBufferHalf(dst4);
        }
    }
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

/// Starts a grid collision probe toward the target, behind the body or toward the ceiling.
///
/// `probeMode` is 0 for a target hit test, 1 for backward X/Z distance, or 2 for
/// ceiling X/Y distance. Geometry is in body-local world units; target mode
/// inverts yaw and, on the ceiling, roll. The target delta narrows to s16 before
/// rotation. Clears old contacts and enables the capsule's grid test; a later
/// collision update supplies contacts for `_stalkerZebraIvoryWallDistance`.
static void _actor400600StartWallProbe(Task* task, s16 probeMode)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)task->work;
    SVECTOR                       targetDelta;
    SVECTOR                       localTarget;
    MATRIX                        inverseRotation;
    s16                           endZOrRadius;

    // Put the probe in body-local coordinates; contacts arrive on a later update.
    work->distanceMode = probeMode;
    switch (probeMode) {
        case ACTOR_400600_PROBE_TARGET:
            if (work->onCeiling == 0) {

                targetDelta.vx = work->targetPos.vx - task->extra.tmd->coords->coord.t[0];
                targetDelta.vy = work->targetPos.vy - task->extra.tmd->coords->coord.t[1] - ACTOR_400600_FLOOR_TARGET_Y_OFFSET;
                targetDelta.vz = work->targetPos.vz - task->extra.tmd->coords->coord.t[2];
                gfxSetRotIdentity(&inverseRotation);
                inverseRotation.t[0] = 0;
                inverseRotation.t[1] = 0;
                inverseRotation.t[2] = 0;
                RotMatrixY(-work->yaw, &inverseRotation);
                ApplyMatrixSV(&inverseRotation, &targetDelta, &localTarget);
            } else {

                targetDelta.vx = work->targetPos.vx - task->extra.tmd->coords->coord.t[0];
                targetDelta.vy = work->targetPos.vy - task->extra.tmd->coords->coord.t[1] - ACTOR_400600_CEILING_TARGET_Y_OFFSET;
                targetDelta.vz = work->targetPos.vz - task->extra.tmd->coords->coord.t[2];
                gfxSetRotIdentity(&inverseRotation);
                inverseRotation.t[0] = 0;
                inverseRotation.t[1] = 0;
                inverseRotation.t[2] = 0;
                RotMatrixY(-work->yaw, &inverseRotation);
                RotMatrixZ(-work->roll, &inverseRotation);
                ApplyMatrixSV(&inverseRotation, &targetDelta, &localTarget);
            }
            work->capsule.ends[0].vx = localTarget.vx;
            work->capsule.ends[0].vy = localTarget.vy;
            // The reused temporary preserves the endpoint/radius store order.
            endZOrRadius             = localTarget.vz;
            work->capsule.ends[0].vz = endZOrRadius;
            endZOrRadius             = ACTOR_400600_THIN_PROBE_RADIUS;
            work->capsule.end0Radius = endZOrRadius;
            work->capsule.end1Radius = endZOrRadius;
            break;
        case ACTOR_400600_PROBE_BACK:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = ACTOR_400600_BACK_PROBE_Y;
            work->capsule.ends[0].vz = -ACTOR_400600_PROBE_LENGTH;
            work->capsule.end0Radius = ACTOR_400600_BACK_PROBE_RADIUS;
            work->capsule.end1Radius = ACTOR_400600_BACK_PROBE_RADIUS;
            break;
        case ACTOR_400600_PROBE_CEILING:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = -ACTOR_400600_PROBE_LENGTH;
            work->capsule.ends[0].vz = 0;
            work->capsule.end0Radius = ACTOR_400600_THIN_PROBE_RADIUS;
            work->capsule.end1Radius = ACTOR_400600_THIN_PROBE_RADIUS;
            break;
    }
    work->capsule.ends[1].vx = 0;
    work->capsule.ends[1].vy = ACTOR_400600_PROBE_ROOT_Y;
    work->capsule.ends[1].vz = 0;
    worldCollisionClearContacts(work->capsuleContacts);
    work->capsuleBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#include "../../shared/stalker_zebra_ivory_wall_distance.inc.c"

/// Copies the nine Q12 rotation coefficients between two live matrices.
///
/// Requires a readable source and writable destination, aligned for s16; they
/// may be the same matrix. Translation and alignment bytes are outside this copy.
static inline void _actor400600CopyMatrixRotation(MATRIX* destination, const MATRIX* source)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Replaces a coordinate's local rotation with unit-scale yaw in 4096ths of a turn.
///
/// Writes only the nine rotation coefficients; translation, stored angles and
/// composition stamps remain intact. The caller must arrange recomposition
/// before reading the cached rotation. The coordinate must be live.
static inline void _actor400600SetCoordYaw(GfxCoord* coord, s16 yaw)
{
    MATRIX yawRotation;

    gfxSetRotIdentity(&yawRotation);
    RotMatrixY(yaw, &yawRotation);
    _actor400600CopyMatrixRotation(&coord->coord, &yawRotation);
}

/// Sets the root rotation of the child task held in work field `child`, if it
/// has been spawned.
#define _ACTOR400600_ROTATE_CHILD(task, child, angle)                        \
    do {                                                                     \
        Task* _child = ((_Actor400600ZebraStalkerWork*)(task)->work)->child; \
                                                                             \
        if (_child != NULL) {                                                \
            _actor400600SetCoordYaw(_child->extra.tmd->coords, (angle));     \
        }                                                                    \
    } while (0)

/// Eases both arm models outward during strikes and back toward their rest yaw.
///
/// Requires live work and initialized attack spheres; missing arm tasks are
/// allowed. Swing angles use 4096 units per turn, approach 896 when extended
/// and approach zero when folded. Left-arm yaw has the opposite sign. Folding
/// disables that arm's pair tests, and every behavior except left/right strike
/// disables both attacks. Only child rotation coefficients are rewritten.
static void _actor400600UpdateArmSwing(Task* task)
{
    enum { ACTOR_400600_ARM_EXTENDED_YAW = 896 };
    _Actor400600ZebraStalkerWork* work;
    s32                           rightArmYaw;

    work = task->work;
    if (work->rightArmOut != 0) {
        work->armSwingAngles[1] += (ACTOR_400600_ARM_EXTENDED_YAW - work->armSwingAngles[1]) >> 2;
        _ACTOR400600_ROTATE_CHILD(task, armTasks[1], work->armSwingAngles[1]);
    } else {
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->armSwingAngles[1]  += -work->armSwingAngles[1] >> 3;
        rightArmYaw               = work->armSwingAngles[1];
        _ACTOR400600_ROTATE_CHILD(task, armTasks[1], rightArmYaw);
    }
    if (work->leftArmOut != 0) {
        work->armSwingAngles[0] += (ACTOR_400600_ARM_EXTENDED_YAW - work->armSwingAngles[0]) >> 2;
        _ACTOR400600_ROTATE_CHILD(task, armTasks[0], -work->armSwingAngles[0]);
    } else {
        work->leftArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->armSwingAngles[0] += -work->armSwingAngles[0] >> 3;
        _ACTOR400600_ROTATE_CHILD(task, armTasks[0], -work->armSwingAngles[0]);
    }
    if (work->state != ACTOR_400600_STATE_LEFT_STRIKE && work->state != ACTOR_400600_STATE_RIGHT_STRIKE) {
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

/// Starts the hide branch of the cloak fade for a live actor and its arm models.
///
/// Leaves an existing hide request alone, including a fade in progress.
/// Otherwise resets fade progress and selects semitransparent rendering with
/// black lighting for the body and existing arms. Drawing and target-scanning
/// flags are preserved here; the fade tick applies the final hidden state.
/// Requires the live work, body model and its enemy record.
static inline void _actor400600StartHideFadeInline(Task* task)
{
    _Actor400600ZebraStalkerWork* work  = task->work;
    TmdObject*                    model = task->extra.tmd;

    if (work->cloaked != ACTOR_400600_CLOAK) {
        work->cloaked         = ACTOR_400600_CLOAK;
        work->cloakFading     = 1;
        work->cloakFadeFrames = 0;
        model->flags         |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_BLACK);
    }
}

/// Starts revealing the body and existing arms, making the target visible and lockable.
///
/// Requires live model/Enemy and work equal to `task->work`, borrowed for this call.
/// An existing reveal target leaves any fade progress intact. A new request resets
/// the fade counter, enables semitransparent body drawing, restores default body
/// and arm lighting, and replaces target flags with `WORLD_TARGET_KEEP_SCANNED`.
/// The fade tick later restores opaque drawing; collision is unaffected.
static inline void _actor400600StartRevealFadeInline(Task* task, _Actor400600ZebraStalkerWork* work)
{
    TmdObject* model = task->extra.tmd;
    Enemy*     enemy = task->spawnArg2.pointer;

    if (work->cloaked != ACTOR_400600_REVEAL) {
        work->cloaked         = ACTOR_400600_REVEAL;
        work->cloakFading     = 1;
        work->cloakFadeFrames = 0;
        model->flags          = (model->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_DEFAULT);
    }
}

/// Starts hidden idle when actor and player face one another at close range.
///
/// Returns 1 after selecting running behavior 17, otherwise 0. A positive hide
/// cooldown is decremented and leaves the cloak target alone. Once it expires,
/// hiding requires target distance below 3000 root-parent units, target bearing
/// within 1023 of forward and player-relative bearing within 767 of forward,
/// both modulo 4096. Otherwise starts revealing. The target may be a route
/// waypoint. Requires live work, body model, Enemy and any existing arm models.
static s32 _actor400600TryEnterHiddenIdle(Task* task)
{
    enum { ACTOR_400600_HIDE_DISTANCE         = 3000,
           ACTOR_400600_TARGET_HIDE_ARC_START = 0x400,
           ACTOR_400600_TARGET_HIDE_ARC_COUNT = 0x801U,
           ACTOR_400600_PLAYER_HIDE_ARC_START = 0x300,
           ACTOR_400600_PLAYER_HIDE_ARC_COUNT = 0xA01U,
           ACTOR_400600_STATE_HIDDEN_IDLE     = 17 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if (work->hideCooldown > 0) {
        work->hideCooldown--;
        return 0;
    }
    if ((u32)(work->targetBearing - ACTOR_400600_TARGET_HIDE_ARC_START) >= (u32)ACTOR_400600_TARGET_HIDE_ARC_COUNT && (u32)(work->bearingFromPlayer - ACTOR_400600_PLAYER_HIDE_ARC_START) >= (u32)ACTOR_400600_PLAYER_HIDE_ARC_COUNT) {
        if (work->playerDistance < ACTOR_400600_HIDE_DISTANCE) {
            _actor400600StartHideFadeInline(task);
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_HIDDEN_IDLE);
            return 1;
        }
    } else {
        work = task->work;
    }
    _actor400600StartRevealFadeInline(task, work);
    return 0;
}

/// Chooses a strike, grab or ceiling move when the walking attack timer permits it.
///
/// Returns 1 when a behavior is selected, otherwise 0. A ceiling leap first
/// starts a probe and waits until the next call to consume its contacts; only a
/// ceiling 2001..3000 units away is accepted. The secret passage uses a fixed
/// world Y of -2500. Every call advances the random stream, including timer waits;
/// an idle ceiling choice draws two more samples to seed a 60..138-frame timer.
static s32 _actor400600TrySelectAttack(Task* task)
{
    enum { ACTOR_400600_SPAWN_NO_CEILING       = 1,
           ACTOR_400600_STRIKE_REACH           = 1500,
           ACTOR_400600_CEILING_DISTANCE_FIRST = 2001,
           ACTOR_400600_CEILING_DISTANCE_COUNT = 1000 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    u32                           timerRandom1;
    u32                           timerRandom2;
    u32                           attackRandom;
    s32                           ceilingDistance;
    s16                           ceilingY;

    work            = (_Actor400600ZebraStalkerWork*)task->work;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rootCoord       = task->extra.tmd->coords;
    attackRandom    = gRandomLcgState >> 0x10;
    switch (work->ceilingProbePending) {
        case 0:
            if (work->timer != 0) {
                return 0;
            }
            if (work->onCeiling == 0) {
                if ((attackRandom & 0xF) == 0) {
                    if (!(task->spawnArg1.value & ACTOR_400600_SPAWN_NO_CEILING) && work->ceilingCooldown == 0) {
                        _actor400600StartWallProbe(task, ACTOR_400600_PROBE_CEILING);
                        work->ceilingProbePending = 1;
                    }
                    return 0;
                }
                if ((attackRandom & 7) == 1 || (attackRandom & 7) == 2) {
                    if (work->playerDistance < ACTOR_400600_STRIKE_REACH && (u32)(work->targetBearing - 0x200) >= 0xC01U) {
                        _actor400600SelectBehavior(task, ACTOR_400600_STATE_GRAB);
                        return 1;
                    }
                } else if (work->playerDistance < ACTOR_400600_STRIKE_REACH) {
                    if ((s16)work->targetBearing < 0x400) {
                        _actor400600SelectBehavior(task, ACTOR_400600_STATE_LEFT_STRIKE);
                        return 1;
                    }
                    if ((s16)work->targetBearing > 0xC00) {
                        _actor400600SelectBehavior(task, ACTOR_400600_STATE_RIGHT_STRIKE);
                        return 1;
                    }
                }
            } else if ((attackRandom & 7) == 0) {
                if (work->ceilingCooldown == 0) {
                    _actor400600SelectBehavior(task, ACTOR_400600_STATE_LEAVE_CEILING);
                    return 1;
                }
            } else if ((attackRandom & 0xF) == 1) {
                if (work->ceilingCooldown == 0) {
                    _actor400600SelectBehavior(task, ACTOR_400600_STATE_DROP_FROM_CEILING);
                    return 1;
                }
            } else {
                timerRandom1                                       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                timerRandom2                                       = (timerRandom1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState                                    = timerRandom2;
                ((_Actor400600ZebraStalkerWork*)task->work)->timer = 0x3C + ((timerRandom1 >> 0x10) & 0x3F) + ((timerRandom2 >> 0x10) & 0xF);
                return 0;
            }
            return 0;
        case 1:
            // Consume the previous frame's ceiling probe, then disable it.
            work->ceilingProbePending = 0;
            ceilingDistance           = _stalkerZebraIvoryWallDistance(task);
            if ((u16)(ceilingDistance - ACTOR_400600_CEILING_DISTANCE_FIRST) < ACTOR_400600_CEILING_DISTANCE_COUNT) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, 0, 0)) {
                    ceilingY = ACTOR_400600_SECRET_PASSAGE_CEILING_Y;
                } else {
                    ceilingY = rootCoord->coord.t[1] - ceilingDistance;
                }
                work->leapY = ceilingY;
                _actor400600SelectBehavior(task, ACTOR_400600_STATE_LEAP_TO_CEILING);
                ((_Actor400600ZebraStalkerWork*)task->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                return 1;
            }
            ((_Actor400600ZebraStalkerWork*)task->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            return 0;
    }
    return 0;
}

static const TaskFuncTable3 D_actor_400600_80132030 = { {
    _actor400600HideForGroupEntrance,
    _actor400600WaitGroupEntrance,
    _actor400600TickGroupEntranceDelay,
} };

/// Updates the staggered group entrance and draws its floor shadows.
///
/// Task state 8, with entry index 0..2 and live work, rig, model and Enemy.
/// Hidden control suppresses drawing. Running control advances the target,
/// animation, entrance phase, cloak and root rotation in that order; paused
/// control shares only contact clearing, cached-coordinate lighting and nine
/// floor shadow segments at world height zero. Lighting samples coordinate 1's
/// existing translation without composing it; its frame and age are unproven.
/// Requires the initialized part-query, lighting and shadow scratch stack and
/// primitive space for the visible shadows.
static void _actor400600UpdateGroupEntrance(Task* task)
{
    TmdObject*                    model            = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work             = task->work;
    TaskFuncTable3                entranceHandlers = D_actor_400600_80132030;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(task);
            _stalkerZebraIvoryTickAnimInline(task);
            entranceHandlers.funcs[work->state](task);
            _actor400600TickCloakFade(task);
            _stalkerZebraIvoryApplyRotationInline(task);
            // Running updates also clear contacts and refresh the presentation.
            // Fall through.
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            _actorRenderUpdateModelColor(task);
            _actor400600DrawFloorShadows(task, 0, work->shadowShade);
            break;
    }
}

/// Draws the Zebra Stalker's nine floor limb-shadow segments at a world-space height.
///
/// Stages eleven model-part world X/Z positions, narrowing to s16, with `height`
/// as each point's world Y. Each segment has a half-width of 128 world units and
/// grey `shade` (0..255). The part list has eleven entries followed by -1, all
/// within the eighteen-part model. Points live only for this pass.
static void _actor400600DrawFloorShadows(Task* task, s16 height, u8 shade)
{
    MATRIX    worldTransform;
    SVECTOR   worldPoints[ACTOR_400600_FLOOR_SHADOW_POINT_COUNT];
    GfxCoord* partCoord;
    GfxCoord* rootCoord;
    s32       pointIndex;

    rootCoord                  = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    rootCoord->composeStamp    = GRAPHICS_COORD_DIRTY;
    // Cache each part in world space once for all segments.
    for (pointIndex = 0; D_actor_400600_80151B88[pointIndex] != -1; pointIndex++) {
        partCoord               = &task->extra.tmd->coords[D_actor_400600_80151B88[pointIndex]];
        partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(partCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &worldTransform);
        worldPoints[pointIndex].vx = worldTransform.t[0];
        worldPoints[pointIndex].vy = height;
        worldPoints[pointIndex].vz = worldTransform.t[2];
    }
    _actor400600DrawFloorLimbShadow(&worldPoints[1], &worldPoints[5], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[5], &worldPoints[6], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[1], &worldPoints[3], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[3], &worldPoints[4], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[0], &worldPoints[2], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[0], &worldPoints[7], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[7], &worldPoints[8], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[0], &worldPoints[9], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
    _actor400600DrawFloorLimbShadow(&worldPoints[9], &worldPoints[10], ACTOR_400600_FLOOR_SHADOW_HALF_WIDTH, shade);
}

/// Queues a subtractive textured limb-shadow quad between two world-space points.
///
/// `halfWidth` is the half-width in world units, perpendicular to the X/Z segment;
/// each end overhangs by half the segment length, making the quad twice as long.
/// Each corner retains its endpoint's Y and narrows to s16. `shade` is grey
/// brightness (0..255). Points are borrowed for this call; scratch storage is
/// released after projection, and a negative GTE FLAG discards the quad.
static void _actor400600DrawFloorLimbShadow(const SVECTOR* firstPoint, const SVECTOR* secondPoint, s16 halfWidth, u8 shade)
{
    _Actor400600LimbShadowQuadScratch* scratch;
    s16 // Widen in X/Z and overhang both ends before projecting the world corners.
              segmentYaw;
    s32       halfX;
    s32       halfZ;
    POLY_FT4* quad;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratch                    = SCRATCH_STACK_RESERVE_BLOCK(_Actor400600LimbShadowQuadScratch);
    actorRenderComposeCoord(&gGfxViewCoord);
    segmentYaw             = ratan2(secondPoint->vx - firstPoint->vx, secondPoint->vz - firstPoint->vz);
    halfX                  = (firstPoint->vx - secondPoint->vx) / 2;
    halfZ                  = (firstPoint->vz - secondPoint->vz) / 2;
    scratch->corners[0].vx = halfX + (firstPoint->vx - ((rcos(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
    scratch->corners[0].vy = firstPoint->vy;
    scratch->corners[0].vz = halfZ + (firstPoint->vz + ((rsin(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
    scratch->corners[1].vx = halfX + (firstPoint->vx + ((rcos(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
    scratch->corners[1].vy = firstPoint->vy;
    scratch->corners[1].vz = halfZ + (firstPoint->vz - ((rsin(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS));
    scratch->corners[2].vx = (secondPoint->vx - ((rcos(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - halfX;
    scratch->corners[2].vy = secondPoint->vy;
    scratch->corners[2].vz = (secondPoint->vz + ((rsin(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - halfZ;
    scratch->corners[3].vx = (secondPoint->vx + ((rcos(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - halfX;
    scratch->corners[3].vy = secondPoint->vy;
    scratch->corners[3].vz = (secondPoint->vz - ((rsin(segmentYaw) * halfWidth) >> ACTOR_400600_SHADOW_TRIG_FRACTION_BITS)) - halfZ;
    // Reject failed projections before reserving a GPU packet.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->depth = RotTransPers4(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2], &scratch->corners[3], &scratch->screenCorners[0], &scratch->screenCorners[1],
                                   &scratch->screenCorners[2], &scratch->screenCorners[3], &scratch->depthCue, &scratch->flag);
    if (scratch->flag >= 0) {
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, ACTOR_400600_SHADOW_PACKET_WORDS);
        quad->code                     = ACTOR_400600_SHADOW_QUAD_CODE;
        GPU_PRIMITIVE_XY_WORD(quad, 0) = scratch->screenCorners[0];
        GPU_PRIMITIVE_XY_WORD(quad, 1) = scratch->screenCorners[1];
        GPU_PRIMITIVE_XY_WORD(quad, 2) = scratch->screenCorners[2];
        GPU_PRIMITIVE_XY_WORD(quad, 3) = scratch->screenCorners[3];
        setUV4(quad, ACTOR_400600_SHADOW_U0, ACTOR_400600_SHADOW_V0, ACTOR_400600_SHADOW_U1, ACTOR_400600_SHADOW_V0, ACTOR_400600_SHADOW_U0, ACTOR_400600_SHADOW_V1, ACTOR_400600_SHADOW_U1, ACTOR_400600_SHADOW_V1);
        quad->tpage = ACTOR_400600_SHADOW_TPAGE;
        quad->clut  = ACTOR_400600_SHADOW_CLUT;
        setRGB0(quad, shade, shade, shade);
        addPrim((&gGpuCurrentOt[((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor400600LimbShadowQuadScratch);
}

/// Copies the body's draw flags to the two existing arm models.
///
/// `colorMode` selects their enemy lighting mode when nonnegative; a negative
/// value leaves lighting unchanged. Each arm task borrows its enemy through the
/// second spawn argument and remains owned by the body task.
static void _actor400600SyncArmModelFlags(Task* task, s32 colorMode)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Task*                         armTask;

    work  = (_Actor400600ZebraStalkerWork*)task->work;
    model = task->extra.tmd;
    if (work->armTasks[0] != NULL) {
        armTask                   = work->armTasks[0];
        armTask->extra.tmd->flags = model->flags;
        if (colorMode >= 0) {
            worldCoordSetActorColorMode(armTask->spawnArg2.pointer, colorMode);
        }
    }
    if (work->armTasks[1] != NULL) {
        armTask                   = work->armTasks[1];
        armTask->extra.tmd->flags = model->flags;
        if (colorMode >= 0) {
            worldCoordSetActorColorMode(armTask->spawnArg2.pointer, colorMode);
        }
    }
}

/// Returns the first route zone containing a model's root world X/Z, or 0 outside all zones.
///
/// Coordinates narrow to signed 16-bit world units; both rectangle edges are
/// included. Table order resolves overlap, so the narrow positive-X passage
/// (zone 6) precedes the surrounding zone 2. Nonzero results are signed zone IDs
/// 1..6, returned as s32 for the caller's explicit s16 snapshots.
static s32 _actor400600FindRouteZone(Task* task)
{
    GfxCoord*  rootCoord;
    ActorZone* zone;
    s16        x;
    s16        z;

    rootCoord = task->extra.tmd->coords;
    x         = rootCoord->coord.t[0];
    z         = rootCoord->coord.t[2];
    for (zone = D_actor_400600_80151B40; zone->id != ACTOR_ZONE_END; zone++) {
        if (zone->x <= x && x <= zone->x + zone->width && zone->z <= z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return 0;
}

/// Returns 1 when body animation slot 1 completed a boundary or jump on the previous running tick.
///
/// Tests the saved slot status for reached-boundary, followed-jump or settled.
/// Returns 0 otherwise. The snapshot survives a clip restart in the current
/// handler, so landing cues can restart their frame counter and playback rate.
static s32 _actor400600ClipWasDone(Task* task)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)task->work;

    if ((work->previousAnimationFlags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

/// Spawns a water ripple and sixteen spray particles around the model root.
///
/// The ring has radius 512 in root-local X/Z; `verticalOffset` is signed root-local
/// Y in game-coordinate units. Trig values use Q12 and narrow to s16 after a
/// logical right shift. Spray arguments select size 328, two updates per cell,
/// speed 32 and randomized upward velocity; the ring vector is a placement offset.
/// Requires current root matrices and loaded room ripple/spray callbacks, which
/// consume copied offsets rather than the retained pointer to the temporary.
static void _actor400600SpawnWaterSplash(Task* task, s16 verticalOffset)
{
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    _actor400600SpawnSplashRing(rootCoord, verticalOffset);
}

/// Immediately hides the body and arms without running a cloak fade.
///
/// A nonzero `cloakRequest` suppresses drawing, selects black lighting, leaves
/// the target scanned but un-lockable, and clears fade progress and shadow shade.
/// Zero does nothing. Requires live work, body model and Enemy; missing arm
/// tasks are allowed. Collision links and enables are preserved.
static void _actor400600SnapCloaked(Task* task, s16 cloakRequest)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Enemy*                        enemy;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (cloakRequest != 0) {
        enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_BLACK);
        work->cloaked         = ACTOR_400600_CLOAK;
        work->cloakFadeFrames = 0;
        work->cloakFading     = 0;
        work->shadowShade     = 0;
    }
}

#include "../../shared/stalker_zebra_ivory_fold_arms.inc.c"

/// Counts down positive attack-selection and ceiling-move cooldowns by one update.
///
/// Requires live work. Zero and negative signed-halfword timers stay unchanged;
/// hide and hit cooldowns are advanced by their own behavior/contact updates.
static void _actor400600TickAttackCooldowns(Task* task)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    if (work->timer > 0) {
        work->timer--;
    }
    if (work->ceilingCooldown > 0) {
        work->ceilingCooldown--;
    }
}

#include "../../shared/stalker_zebra_ivory_seed_timer.inc.c"

#include "../../shared/stalker_zebra_ivory_disable_capsule_grid.inc.c"

/// Starts a cloak or reveal transition when it differs from the current cloak target.
///
/// A zero `cloak` reveals; any nonzero byte cloaks. Resets the fade frame counter,
/// turns on semitransparency and selects default or black lighting for the body
/// and existing arms. Reveal immediately restores drawing and lock-on scanning;
/// hide suppresses drawing only when `_actor400600TickCloakFade` finishes.
static void _actor400600StartCloakFade(Task* task, u8 cloak)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Enemy*                        enemy;

    model = task->extra.tmd;
    enemy = (Enemy*)task->spawnArg2.pointer;
    work  = (_Actor400600ZebraStalkerWork*)task->work;
    if (cloak == 0) {
        if (work->cloaked != 0) {
            work->cloaked         = 0;
            work->cloakFading     = 1;
            work->cloakFadeFrames = 0;
            model->flags          = (model->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            _actor400600SyncArmModelFlags(task, ENEMY_COLOR_DEFAULT);
        }
    } else if (work->cloaked != 1) {
        work->cloaked         = 1;
        work->cloakFading     = 1;
        work->cloakFadeFrames = 0;
        model->flags         |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_BLACK);
    }
}

/// Clears the entrance's receiving-body and probe contacts after dispatch or while paused.
///
/// Requires live work with initialized, terminated contact tables. Collision
/// links and enables remain intact; each table's LAST terminator is preserved.
static inline void _actor400600ClearEntranceContacts(_Actor400600ZebraStalkerWork* work)
{
    worldCollisionClearContacts(work->bodyContacts);
    worldCollisionClearContacts(work->capsuleContacts);
}

/// Advances the water entrance and redraws its wall shadows while paused.
///
/// Task state 4, requiring work state 0..7 and a live rig/model. Running
/// updates advance animation before dispatching the entrance phase, then
/// update cloak and rotation. Running and paused updates clear body/probe
/// contacts, refresh lighting and draw shadows at shadowWallZ. Hidden control
/// suppresses body drawing; other control values do nothing.
static void _actor400600RunWaterEntrance(Task* task)
{
    TmdObject*                    model  = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable8                phases = D_actor_400600_80131E7C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(task);
            _stalkerZebraIvoryTickAnim(task);
            phases.funcs[work->state](task);
            _actor400600TickCloakFade(task);
            _stalkerZebraIvoryApplyRotation(task);
            // Fall through to presentation after the running update.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400600ClearEntranceContacts(work);
            _stalkerZebraIvoryUpdateColor(task);
            _actor400600DrawWallShadows(task, work->shadowWallZ, work->shadowShade);
            break;
    }
}

/// Advances the group drop entrance and redraws its floor shadows while paused.
///
/// Task state 7, requiring work state 0..3 and a live rig/model. Running
/// updates advance animation before the phase, then cloak and rotation.
/// Running and paused updates clear body/probe contacts, refresh lighting
/// and draw floor shadows at Y zero. Hidden control suppresses body drawing;
/// the phase may switch to running task state 1.
static void _actor400600RunGroupDropEntrance(Task* task)
{
    TmdObject*                    model  = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable4                phases = D_actor_400600_80131E9C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(task);
            _stalkerZebraIvoryTickAnim(task);
            phases.funcs[work->state](task);
            _actor400600TickCloakFade(task);
            _stalkerZebraIvoryApplyRotation(task);
            // Fall through to presentation after the running update.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400600ClearEntranceContacts(work);
            _stalkerZebraIvoryUpdateColor(task);
            _actor400600DrawFloorShadows(task, 0, work->shadowShade);
            break;
    }
}

/// Advances the Junk Yard entrance and redraws its floor shadows while paused.
///
/// Task state 6, requiring work state 0..5 and a live rig/model. Running
/// updates dispatch the phase before advancing animation, then cloak and
/// rotation. Running and paused updates clear body/probe contacts, refresh
/// lighting and draw floor shadows at Y zero. Hidden control suppresses body
/// drawing; the phase may switch to running task state 1.
static void _actor400600RunJunkYardEntrance(Task* task)
{
    TmdObject*                    model  = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable6                phases = D_actor_400600_80131E54;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(task);
            phases.funcs[work->state](task);
            _stalkerZebraIvoryTickAnim(task);
            _actor400600TickCloakFade(task);
            _stalkerZebraIvoryApplyRotation(task);
            // Fall through to presentation after the running update.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400600ClearEntranceContacts(work);
            _stalkerZebraIvoryUpdateColor(task);
            _actor400600DrawFloorShadows(task, 0, work->shadowShade);
            break;
    }
}

/// Advances the floor-drop entrance and redraws its floor shadows while paused.
///
/// Task state 5, requiring work state 0..3 and a live rig/model. Running
/// updates dispatch the phase before advancing animation, then cloak and
/// rotation. Running and paused updates clear body/probe contacts, refresh
/// lighting and draw floor shadows at Y zero. Hidden control suppresses body
/// drawing; the phase may switch to running task state 1.
static void _actor400600RunFloorDropEntrance(Task* task)
{
    TmdObject*                    model  = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable4                phases = D_actor_400600_80131E6C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _actor400600UpdateTarget(task);
            phases.funcs[work->state](task);
            _stalkerZebraIvoryTickAnim(task);
            _actor400600TickCloakFade(task);
            _stalkerZebraIvoryApplyRotation(task);
            // Fall through to presentation after the running update.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400600ClearEntranceContacts(work);
            _stalkerZebraIvoryUpdateColor(task);
            _actor400600DrawFloorShadows(task, 0, work->shadowShade);
            break;
    }
}

/// Starts the running behavior table at its hidden waiting entry.
///
/// Entry 0 of running task state 1. Selects behavior 1 and resets its sub-state;
/// the spawn or scripted entrance has already initialized the cloak and rig.
static void _actor400600EnterHiddenWait(Task* task)
{
    enum { ACTOR_400600_STATE_HIDDEN_WAIT = 1 };

    _actor400600SelectBehavior(task, ACTOR_400600_STATE_HIDDEN_WAIT);
}

static void func_actor_400600_80139110(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    _stalkerZebraIvoryFoldArms(arg0);
    if ((s16)_actor400600TakeArmedHitReaction(arg0) == 0) {
        fns[work->subState](arg0);
        if ((s16)_actor400600TrySelectAttack(arg0) == 0 && (s16)_actor400600TryEnterHiddenIdle(arg0) == 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 8, 0, 0) && work->onCeiling != 0 && arg0->extra.tmd->coords->coord.t[0] > 10000) {
            _Actor400600ZebraStalkerWork* cur = (_Actor400600ZebraStalkerWork*)arg0->work;

            cur->state    = 0xD;
            cur->subState = 0;
        }
    }
}

/// Folds the arms and dispatches the light-recoil entry or wait phase.
///
/// Running behavior 3, requiring sub-state 0..1 and a live rig/work/model.
/// Its wait phase accepts further armed reactions before testing completion;
/// completed recoil resumes walking upright or crawling on the back.
static void _actor400600RunLightRecoil(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartLightRecoil, _actor400600TickLightRecoil };

    _stalkerZebraIvoryFoldArms(task);
    phases[work->subState](task);
}

/// Folds the arms and dispatches the heavy-recoil entry or completion phase.
///
/// Running behavior 4, requiring sub-state 0..1 and a live rig/work/model.
/// The completion phase waits uninterrupted, then resumes walking upright or
/// crawling on the back.
static void _actor400600RunHeavyRecoil(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartHeavyRecoil, _actor400600FinishHeavyRecoil };

    _stalkerZebraIvoryFoldArms(task);
    phases[work->subState](task);
}

/// Folds the arms and dispatches status-hold setup, buildup expiry or recovery.
///
/// Running behavior 5, requiring sub-state 0..2, an initialized rig and a live
/// Enemy spawn record. Recovery returns to walking upright or crawling on the
/// back; this dispatcher takes no additional armed reaction.
static void _actor400600RunStatusHold(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable3                phases = D_actor_400600_80131F34;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

/// Dispatches the left-arm strike unless an armed hit reaction takes over.
///
/// Running behavior 6, requiring sub-state 0..1, the initialized body rig,
/// left attack sphere, arm task and Enemy. Tests the reaction's low halfword
/// before selecting the phase; the strike controls its own arm-extension flag.
static void _actor400600RunLeftStrike(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartLeftStrike, _actor400600TickLeftStrike };

    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        phases[work->subState](task);
    }
}

/// Dispatches the right-arm strike unless an armed hit reaction takes over.
///
/// Running behavior 7, requiring sub-state 0..1, the initialized body rig,
/// right attack sphere, arm task and Enemy. Tests the reaction's low halfword
/// before selecting the phase; the strike controls its own arm-extension flag.
static void _actor400600RunRightStrike(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartRightStrike, _actor400600TickRightStrike };

    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        phases[work->subState](task);
    }
}

/// Folds both arms and dispatches the current phase of the player grab.
///
/// Running behavior 8, requiring sub-state 0..7 and live work/model/Enemy.
/// Phases wind up, probe the target, approach, hold/bite and release the player.
/// The selected phase owns hit-reaction handling and transitions; this wrapper
/// always dispatches one phase and allocates nothing.
static void _actor400600RunGrab(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable8                phases = D_actor_400600_80131F40;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

/// Folds both arms and dispatches the current phase of the backward leap.
///
/// Running behavior 9, requiring sub-state 0..3 and live work/model/Enemy.
/// The four phases start a rear wall probe, choose the available leap distance,
/// move backward and wait for clip completion. Collision must populate probe
/// contacts between its start and resolution; this wrapper changes no cursor.
static void _actor400600RunBackwardLeap(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable4                phases = D_actor_400600_80131F60;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

/// Crawls toward the target on the back until recovery time expires or a hit intervenes.
///
/// Running behavior 10, with a positive countdown normally seeded to 30..157
/// updates. Folds both arms and gives armed reactions priority; uninterrupted
/// updates decrement the u16 countdown through an s16 local. Zero selects
/// righting behavior 11, otherwise turns by 24/4096 of a revolution outside
/// the shared turn dead zone and advances the hand-anchored crawl. Requires the
/// initialized rig, hands 8/11, Enemy and part-query/sound scratch stack; target
/// and root must share their parent frame. A zero input wraps rather than rights.
static void _actor400600TickOnBackBehavior(Task* task)
{
    enum { ACTOR_400600_STATE_RIGHTING    = 11,
           ACTOR_400600_ON_BACK_TURN_STEP = 24 };
    _Actor400600ZebraStalkerWork* work;
    SVECTOR                       targetPosition;
    s16                           remainingFrames;

    work = task->work;
    _stalkerZebraIvoryFoldArms(task);
    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        remainingFrames = work->countdown - 1;
        work->countdown = remainingFrames;
        if (remainingFrames == 0) {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_RIGHTING);
            return;
        }
        targetPosition.vx = work->targetPos.vx;
        targetPosition.vy = work->targetPos.vy;
        targetPosition.vz = work->targetPos.vz;
        _stalkerZebraIvoryTurnToward(task, &targetPosition, ACTOR_400600_ON_BACK_TURN_STEP);
        _stalkerZebraIvoryTickOnBackCrawl(task);
    }
}

/// Folds the arms and dispatches setup or the anchored righting motion.
///
/// Running behavior 11, requiring sub-state 0..1 and the live initialized rig.
/// Setup captures body part 14's world X/Z; motion pins it until completion,
/// clears the on-back flag and returns to walking. Requires composed ancestors
/// and the initialized part-query scratch stack; playback is ticked by the caller.
static void _actor400600RunRighting(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartRighting, _stalkerZebraIvoryRightItself };

    _stalkerZebraIvoryFoldArms(task);
    phases[work->subState](task);
}

/// Folds the arms and dispatches ceiling-leap setup, motion or landing completion.
///
/// Running behavior 12, requiring sub-state 0..2, the initialized rig/model and
/// a ceiling height already stored in `leapY`. The phases handle the windup,
/// vertical motion, collision flags and attachment overhead, then resume walking
/// with a ceiling cooldown. Playback and final root rotation belong to the caller.
static void _actor400600RunCeilingLeap(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable3                phases = D_actor_400600_80131F70;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

/// Folds the arms and dispatches the voluntary ceiling drop's four phases.
///
/// Running behavior 13, with sub-state 0..3 and a live initialized model/rig.
/// The phases defer death, snapshot the landing point, fall toward it and wait
/// for the upright landing before walking. Animation and final rotation are
/// updated by the running-task caller.
static void _actor400600RunCeilingDrop(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable4                phases = D_actor_400600_80131F7C;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

/// Folds the arms and dispatches a hit-induced ceiling fall onto the Stalker's back.
///
/// Running behavior 14, requiring sub-state 0..3 and a live initialized rig.
/// The phases reveal the actor, defer death through the fall and landing, then
/// handle pending reactions or begin the timed on-back crawl. The running-task
/// caller advances animation and applies the final root rotation.
static void _actor400600RunCeilingFall(Task* task)
{
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable4                phases = D_actor_400600_80131F8C;

    _stalkerZebraIvoryFoldArms(task);
    phases.funcs[work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_run_sub_states.inc.c"

/// Folds the arms and dispatches the ceiling-exit setup or drop/grab selection.
///
/// Running behavior 16, requiring sub-state 0..1, live work and a current
/// horizontal target distance in root-parent coordinate units. The selection
/// phase disables the probe and chooses a drop beyond 2000 units or a grab
/// otherwise. This dispatcher does not advance animation.
static void _actor400600RunCeilingExit(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartCeilingExit, _stalkerZebraIvorySelectCeilingExit };

    _stalkerZebraIvoryFoldArms(task);
    phases[work->subState](task);
}

/// Folds the arms and dispatches the hidden idle's setup or countdown.
///
/// Running behavior 17, requiring sub-state 0..1 and a live initialized rig.
/// Enter after cloaking has been requested. Setup blends the idle pose and
/// seeds 90..153 updates; the countdown accepts reactions or attacks before
/// expiry reveals the actor and resumes walking. Playback belongs to the caller.
static void _actor400600RunHiddenIdle(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = { _actor400600StartHiddenIdle, _actor400600TickHiddenIdle };

    _stalkerZebraIvoryFoldArms(task);
    phases[work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_apply_rotation.inc.c"

#include "../../shared/stalker_zebra_ivory_restart_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_blend_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_frame_to_ticks.inc.c"

#include "../../shared/stalker_zebra_ivory_turn_toward.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_anim.inc.c"

#include "../../shared/stalker_zebra_ivory_request_clip_restart.inc.c"

/// Requests a body clip with a new playback rate and blend duration.
///
/// `clipIndex` selects 0..34 in this package's animation bank; `rate` is signed
/// 1/16-frame playback step (`ANIMATION_RATE_ONE` is normal). `blendFrames` is
/// the blend duration in frames. The next animation update consumes the request;
/// this call only updates the work block.
static void _actor400600RequestClipBlend(Task* task, s16 clipIndex, s16 rate, s16 blendFrames)
{
    _actor400600SetBlendRequest(task, clipIndex, rate, blendFrames);
}

#include "../../shared/stalker_zebra_ivory_part_world_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_pin_part_xz.inc.c"

/// Samples a part's world X/Y and the root's local Z into an entrance anchor.
///
/// `partIndex` selects 0..17 of the live model beneath `gGfxViewCoord`.
/// The writable six-byte output is borrowed only for this call; every component
/// narrows to a signed halfword. Composes the part, removes the view transform,
/// then marks the part dirty. Requires current ancestor/view caches and the
/// initialized scratch stack used by `gfxMakeRelativeTransform`.
static void _actor400600ReadPartWorldXY(Task* task, s16 partIndex, SVECTOR3* anchorPosition)
{
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* parts;

    parts     = task->extra.tmd->coords;
    partCoord = &parts[partIndex];
    actorRenderComposeCoord(partCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    anchorPosition->vx      = partToWorld.t[0];
    anchorPosition->vy      = partToWorld.t[1];
    anchorPosition->vz      = parts[0].coord.t[2];
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves the root in world X/Y to keep a selected part at its entrance anchor.
///
/// Requires a live model rooted directly beneath `gGfxViewCoord` and a valid
/// part index (0..17). Borrows the anchor's signed-halfword X/Y, ignores Z and
/// retains no pointer. Preserves root Z and rotation, then recomposes the part
/// and root. Requires current ancestor/view caches and the initialized scratch
/// stack used by `gfxMakeRelativeTransform`.
static void _actor400600PinPartXY(Task* task, s16 partIndex, const SVECTOR3* anchorPosition)
{
    MATRIX    rootToWorld;
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* parts;

    parts     = task->extra.tmd->coords;
    partCoord = &parts[partIndex];
    actorRenderComposeCoord(partCoord);
    // Measure the part offset in world space before shifting the root.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parts[0].workm, &rootToWorld);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    parts[0].coord.t[0]     = anchorPosition->vx - (partToWorld.t[0] - rootToWorld.t[0]);
    parts[0].coord.t[1]     = anchorPosition->vy - (partToWorld.t[1] - rootToWorld.t[1]);
    parts[0].composeStamp   = GRAPHICS_COORD_DIRTY;
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(partCoord);
    actorRenderComposeCoord(parts);
}

#include "../../shared/stalker_zebra_ivory_clip_done.inc.c"

/// Dispatches the Zebra Stalker task's spawn, running, death or entrance handler.
///
/// `Task::state` must be 0..8: 0 spawn, 1 running, 2 death, 3 corpse release,
/// 4 water entrance, 5 floor-drop entrance, 6 Junk Yard entrance, 7 group drop,
/// 8 delayed group entrance. Spawn needs the model and Enemy argument; later
/// states require live work. A selected handler may destroy the task, so no
/// task storage is read after dispatch. Rooms reach this callback through the
/// package's task descriptor rather than importing its symbol.
static void _actor400600Task(Task* task)
{
    TaskFuncTable9 states = D_actor_400600_80131EAC;

    states.funcs[task->state](task);
}

/// Dispatches the death sequence and draws its remaining corpse shadows.
///
/// Task state 2, requiring work state 0..11 and a live model/work. Running
/// control advances the death phase; running and paused control update lighting
/// and floor shadows at shadowHeight. Hidden control suppresses body and arm
/// drawing without changing arm colour. Phases can select corpse-release task
/// state 3, but keep task/work live through this callback's drawing phase.
static void _actor400600RunDeathSequence(Task* task)
{
    TmdObject*                    model  = task->extra.tmd;
    _Actor400600ZebraStalkerWork* work   = task->work;
    TaskFuncTable12               phases = D_actor_400600_80131E24;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor400600SyncArmModelFlags(task, ACTOR_400600_KEEP_ARM_COLOR);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            phases.funcs[work->state](task);
            // Fall through to presentation after the running update.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _stalkerZebraIvoryUpdateColor(task);
            _actor400600DrawFloorShadows(task, work->shadowHeight, work->shadowShade);
            break;
    }
}

/// Dispatches arm-task release or delayed enemy destruction after death.
///
/// Task state 3, requiring work state 0..1 and already-unlinked collision bodies.
/// Entry zero alerts other Stalkers, kills the arms and starts a 151-update
/// timer. Entry one can destroy the Enemy and task; neither is used afterward.
static void _actor400600RunCorpseRelease(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    TaskFunc                      phases[2] = {
        _actor400600StartCorpseRelease,
        _actor400600WaitCorpseRelease,
    };

    phases[work->state](task);
}

#include "../../shared/stalker_zebra_ivory_update_color.inc.c"

#include "../../shared/stalker_zebra_ivory_apply_room_command.inc.c"

/// Latches a request to release the player from the actor's hold.
///
/// Handles `ACTOR_MESSAGE_RELEASE_HOLD`, sent after struggle completion or
/// death. Takes no payload and retains no argument storage. The hold update
/// consumes the latch. Returns void to preserve the callback's binary ABI;
/// dispatch callers must ignore the unspecified result register.
static void _actor400600HandleHoldReleaseMessage(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    ((_Actor400600ZebraStalkerWork*)task->work)->playerDied = 1;
}

/// Empty initializer for the separately drawn left-arm model task.
///
/// The spawning parent supplies its part-10 attachment, transform, textures
/// and lighting; this callback changes no task state.
static void _actor400600InitLeftArmTask(Task* task)
{
}

/// Empty initializer for the separately drawn right-arm model task.
///
/// The spawning parent supplies its part-7 attachment, transform, textures
/// and lighting; this callback changes no task state.
static void _actor400600InitRightArmTask(Task* task)
{
}

/// Stops arm attacks and target tracking, then chooses the grounded, blast or ceiling death.
///
/// Entry zero of death task state 2, after health reaches zero and a mandatory
/// move has finished. A pending blast hides all models and resets the burst
/// timer. Grounded death restores normal body/arm colour and advances to the
/// death clip; ceiling death selects the fatal fall. Requires live work, Enemy,
/// body model and any attached arm tasks; collision bodies remain linked here.
static void _actor400600SelectDeathSequence(Task* task)
{
    Enemy*                        enemy;
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    model = task->extra.tmd;
    // Stop outgoing attacks and release locks before selecting the death path.
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldTargetUnlinkNode(&enemy->node);
    if (work->pendingAction == STALKER_ZEBRA_IVORY_PENDING_BLAST) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        _actor400600SyncArmModelFlags(task, ACTOR_400600_KEEP_ARM_COLOR);
        work->stateFrames = 0;
        _actor400600SelectState(task, ACTOR_400600_DEATH_BURST);
    } else if (work->onCeiling == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_DEFAULT);
        work->state++;
    } else {
        _actor400600SelectState(task, ACTOR_400600_DEATH_CEILING_FALL);
    }
}

#include "../../shared/stalker_zebra_ivory_start_death_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_death_clip.inc.c"

/// Unlinks the body's four collision shapes before corpse disposal.
///
/// Requires live work with valid links on any linked body. Retains the embedded
/// shape/contact storage and each shape kind; linked bodies lose their pass and
/// body-index flags. Unlinked bodies are left alone. No work or model is freed.
static inline void _actor400600UnlinkCorpseCollision(_Actor400600ZebraStalkerWork* work)
{
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmBody);
    worldCollisionUnlinkBody(&work->leftArmBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
}

/// Detaches collision and snapshots the dead body's transform for its burn-away sequence.
///
/// Death entry 3, after the death clip completes. Clears the Enemy's borrowed
/// hit-contact pointer, unlinks all four bodies, saves the root matrix and starts
/// at unit Q12 Y scale. Requests the body's weighted warm tint, resets the
/// update counter and advances to the pre-burn wait. Work/model ownership stays
/// with the task until corpse release.
static void _actor400600StartCorpseBurn(Task* task)
{
    _Actor400600ZebraStalkerWork* work      = task->work;
    GfxCoord*                     rootCoord = task->extra.tmd->coords;
    Enemy*                        enemy     = task->spawnArg2.pointer;

    // End contact access before detaching the borrowed collision links.
    enemy->recs = NULL;
    _actor400600UnlinkCorpseCollision(work);
    work->corpseScaleY = ONE;
    work->savedRootMtx = rootCoord->coord;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state++;
}

/// Waits 24 death updates before turning the corpse black and semitransparent.
///
/// Death entry 4, with `stateFrames` reset by corpse-burn setup. Applies black
/// lighting to the body and existing arm models, synchronizes their draw flags,
/// resets the counter and advances to shrinking/burning. Requires the live model,
/// work and Enemy; their collision links have already been removed.
static void _actor400600WaitCorpseBurn(Task* task)
{
    enum { ACTOR_400600_CORPSE_PRE_BURN_FRAMES = 24 };
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work  = task->work;
    model = task->extra.tmd;
    work->stateFrames++;
    if (work->stateFrames >= ACTOR_400600_CORPSE_PRE_BURN_FRAMES) {
        model->flags |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        _actor400600SyncArmModelFlags(task, ENEMY_COLOR_BLACK);
        work->stateFrames = 0;
        work->state++;
    }
}

/// Shrinks the corpse in Y, fades its shadows and starts the burn effect.
///
/// Death entry 5, after collision has been unlinked and the root matrix saved.
/// Rebuilds from that snapshot each update with unit Q12 X/Z scale and a Y
/// scale reduced by 48. Counter 8 spawns the burn; counter 17 or later hides
/// body/arms and advances toward release. Work and model remain task-owned.
static void _actor400600TickCorpseBurn(Task* task)
{
    enum { ACTOR_400600_CORPSE_SCALE_STEP        = 48,
           ACTOR_400600_CORPSE_BURN_EFFECT_FRAME = 8,
           ACTOR_400600_CORPSE_HIDE_FRAME        = 17,
           ACTOR_400600_CORPSE_BURN_VARIANT      = 4 };
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    GfxCoord*                     rootCoord;
    VECTOR                        scale;
    SVECTOR                       effectRotation;

    work                = task->work;
    model               = task->extra.tmd;
    rootCoord           = model->coords;
    work->shadowShade   = (u16)work->shadowShade + (-work->shadowShade >> 2);
    work->corpseScaleY -= ACTOR_400600_CORPSE_SCALE_STEP;
    scale.vx            = ONE;
    scale.vy            = work->corpseScaleY;
    scale.vz            = ONE;
    // Rescale the saved pose instead of compounding the previous update's scale.
    rootCoord->coord = work->savedRootMtx;
    ScaleMatrix(&rootCoord->coord, &scale);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->stateFrames++;
    if (work->stateFrames == ACTOR_400600_CORPSE_BURN_EFFECT_FRAME) {
        effectRotation.vx = 0;
        effectRotation.vy = 0;
        effectRotation.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, rootCoord, ACTOR_400600_CORPSE_BURN_VARIANT, &effectRotation);
    }
    if (work->stateFrames >= ACTOR_400600_CORPSE_HIDE_FRAME) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        _actor400600SyncArmModelFlags(task, ACTOR_400600_KEEP_ARM_COLOR);
        work->state++;
    }
}

/// Enters delayed corpse release at its first handler after the body has burned away.
///
/// Death entry 6. Requires live work and already-unlinked collision resources.
/// Selects task state 3 and resets both work cursors; the next update kills the
/// arm tasks and starts the release timer. No resources are freed in this call.
static void _actor400600EnterCorpseRelease(Task* task)
{
    enum { ACTOR_400600_TASK_CORPSE_RELEASE = 3,
           ACTOR_400600_STATE_INITIAL       = 0 };

    task->state = ACTOR_400600_TASK_CORPSE_RELEASE;
    _actor400600SelectStateInline(task, ACTOR_400600_STATE_INITIAL);
}

/// Delays blast-death teardown until the second update after hiding the actor.
///
/// Death entry 7. The death selector initializes `stateFrames` to zero; this
/// entry increments it as s16 and advances to burst effects/teardown at two.
/// Requires live work and retains all resources until the following entry.
static void _actor400600WaitDeathBurst(Task* task)
{
    enum { ACTOR_400600_DEATH_BURST_DELAY_FRAMES = 2 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames >= ACTOR_400600_DEATH_BURST_DELAY_FRAMES) {
        work->state++;
    }
}

static void func_actor_400600_8013A864(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    Enemy*                        enemy;

    model = arg0->extra.tmd;
    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    func_actor_400600_80137240(arg0);
    sceneReleaseBattleRefWithRewards(arg0, 0);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmBody);
    worldCollisionUnlinkBody(&work->leftArmBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
    work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
    arg0->state     = 3;
    work2->state    = 0;
    work2->subState = 0;
}

/// Reveals the dying Stalker and starts its fatal fall from the ceiling.
///
/// Death entry 9, requiring a live Enemy, model and initialized body rig. Blends
/// loaded recoil clip 9 at normal rate over two frames, zeroes s16 vertical
/// acceleration/speed, places shadows at the spawn floor height and ticks the
/// animation once before advancing. Positions use the root's parent frame.
static void _actor400600StartDeathCeilingFall(Task* task)
{
    enum { ACTOR_400600_DEATH_FALL_BLEND_FRAMES = 2 };
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    model         = task->extra.tmd;
    work          = task->work;
    model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    _actor400600RequestClipBlend(task, ACTOR_400600_CLIP_UPRIGHT_LIGHT_RECOIL, ANIMATION_RATE_ONE, ACTOR_400600_DEATH_FALL_BLEND_FRAMES);
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->shadowHeight = work->floorY;
    _stalkerZebraIvoryTickAnim(task);
    work->state++;
}

/// Accelerates the fatal ceiling fall and starts an on-back landing when it crosses the floor.
///
/// Death entry 10, with acceleration/speed initialized to zero. Each update
/// adds two to s16 acceleration, then that narrowed value to s16 speed, before
/// moving root Y in parent-coordinate units. Strictly crossing `floorY` clamps
/// Y, restarts loaded landing clip 19, flips roll by half a turn, recomposes the
/// root and advances to entry 11 with a reset landing counter. Ticks animation
/// after the landing test on every update. Requires live work/model and an
/// initialized rotation scratch stack; the collision bodies remain linked.
static void _actor400600TickDeathCeilingFall(Task* task)
{
    enum { ACTOR_400600_CLIP_ON_BACK_LANDING = 19 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _actor400600StepAcceleratedFall(work, rootCoord);
    if (work->floorY < rootCoord->coord.t[1]) {
        // Commit the on-back pose before the landing animation starts.
        rootCoord->coord.t[1] = work->floorY;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_ON_BACK_LANDING, ANIMATION_RATE_ONE);
        work->roll += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        _stalkerZebraIvoryApplyRotation(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        work->stateFrames = 0;
        work->onCeiling   = 0;
        work->onBack      = 1;
        work->state++;
    }
    _stalkerZebraIvoryTickAnim(task);
}

/// Sounds the fatal on-back landing once and rejoins the grounded death sequence.
///
/// Death entry 11, requiring landing clip 19 and a counter reset to zero. The
/// first update queues body-bank landing cue 6 with spatial offsets, then
/// latches the counter at one. A body-slot boundary, jump or settled end selects
/// death entry 1 and resets its sub-state. Animation is ticked after the test,
/// including after selecting that entry; this does not free any resources.
static void _actor400600FinishDeathCeilingFallLanding(Task* task)
{
    enum { ACTOR_400600_SOUND_ON_BACK_LANDING = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 6 };
    _Actor400600ZebraStalkerWork* work = task->work;

    if (work->stateFrames == 0) {
        _actor400600RequestPositionalSound(task, ACTOR_400600_SOUND_ON_BACK_LANDING);
        work->stateFrames++;
    }
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600SelectState(task, ACTOR_400600_DEATH_START_CLIP);
    }
    _stalkerZebraIvoryTickAnim(task);
}

/// Alerts the other Zebra Stalkers and releases this corpse's two arm tasks.
///
/// First entry of task state 3, after collision resources have been unlinked.
/// Kills each non-NULL arm task, resets the release timer and advances to its
/// waiting entry. The killed arm pointers must not be used again.
static void _actor400600StartCorpseRelease(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    Task*                         leftArmTask;
    Task*                         rightArmTask;

    work                                     = task->work;
    gSceneCombatState.zebraStalkerDeathAlert = 1;
    leftArmTask                              = work->armTasks[ACTOR_400600_ARM_LEFT];
    if (leftArmTask != NULL) {
        taskKill(leftArmTask);
    }
    rightArmTask = work->armTasks[ACTOR_400600_ARM_RIGHT];
    if (rightArmTask != NULL) {
        taskKill(rightArmTask);
    }
    work->stateFrames = 0;
    work->state++;
}

/// Destroys the enemy and its task after 151 corpse-release updates.
///
/// Requires task state 3, entry 1, with the counter reset by entry 0 and a live
/// owned Enemy in `spawnArg2.pointer`. Actor-specific collision links and arm
/// tasks must already be released. The enemy and task are invalid after teardown.
static void _actor400600WaitCorpseRelease(Task* task)
{
    enum { ACTOR_400600_CORPSE_RELEASE_FRAMES = 151 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames >= ACTOR_400600_CORPSE_RELEASE_FRAMES) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}

/// Hides the Stalker and disables body/arm pair tests before its Junk Yard entrance.
///
/// Entry zero of scripted task state 6, selected by spawn entrance kind 3.
/// Removes shadow brightness, requests cloaking, suppresses body drawing and
/// advances to the room-command wait. Requires live work/model, Enemy and any
/// arm tasks used by the cloak request. Collision links, grid tests and the
/// probe capsule remain intact.
static void _actor400600HideForJunkYardEntrance(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = task->work;
    model             = task->extra.tmd;
    work->shadowShade = 0;
    _actor400600StartCloakFade(task, ACTOR_400600_CLOAK);
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

static void func_actor_400600_8013AC14(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    GfxCoord*                     coord;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;
    if (work->roomCommand == 1) {
        func_dryfield_night_junk_yard_8017D9B8(0);
        coord->coord.t[0] = 0x40C8;
        coord->coord.t[1] = -0x708;
        coord->coord.t[2] = -0x3E8;
        work->pitch       = 0;
        work->yaw         = 0;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        _stalkerZebraIvoryRequestClipRestart(arg0, 0x15, ANIMATION_RATE_ONE);
        _actor400600StartCloakFade(arg0, ACTOR_400600_REVEAL);
        work->state++;
    } else if (work->roomCommand == 3) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]         = 0x32C2;
        coord->coord.t[2]         = 0x960;
        coord->coord.t[1]         = 0;
        work->pitch               = 0;
        work->yaw                 = 0xC00;
        work->roll                = 0;
        work->shadowShade         = 0;
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
    }
}

/// Starts the final Junk Yard drop after the elevated landing clip completes.
///
/// Entry 3 of scripted task state 6. Requires the live work and initialized
/// body rig. Resets the update counter and s16 vertical motion, requests loaded
/// clip 34 at normal rate and advances to the drop; does not move the root.
static void _actor400600StartJunkYardFloorDrop(Task* task)
{
    enum { ACTOR_400600_CLIP_JUNK_YARD_FLOOR_DROP = 34 };
    _Actor400600ZebraStalkerWork* work = task->work;

    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->stateFrames = 0;
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_JUNK_YARD_FLOOR_DROP, ANIMATION_RATE_ONE);
        work->state++;
    }
}

/// Advances the final Junk Yard drop and starts the floor landing at world Y zero.
///
/// Entry 4 of scripted task state 6, with counter and vertical motion reset.
/// Waits through four updates, then eases X toward 16565, adds 120 to Z and
/// accelerates downward by two per update. Acceleration/speed wrap through u16
/// before speed is read as s16. Landing resets the counter, starts clip 25,
/// clamps Y to zero, vibrates the pad and advances; it does not enable hits.
static void _actor400600TickJunkYardFloorDrop(Task* task)
{
    enum { ACTOR_400600_JUNK_YARD_DROP_FIRST_FRAME = 5 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;
    s32                           rootX;
    s32                           landingY;
    u16                           nextAccelBits;
    u16                           nextSpeedBits;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= ACTOR_400600_JUNK_YARD_DROP_FIRST_FRAME) {
        rootX                 = rootCoord->coord.t[0];
        rootCoord->coord.t[0] = rootX + ((0x40B5 - rootX) >> 3);
        rootCoord->coord.t[2] = rootCoord->coord.t[2] + 0x78;
        nextAccelBits         = (u16)work->moveAccel + 2;
        nextSpeedBits         = (u16)work->moveSpeed + nextAccelBits;
        work->moveSpeed       = nextSpeedBits;
        work->moveAccel       = nextAccelBits;
        landingY              = rootCoord->coord.t[1] + (s16)nextSpeedBits;
        rootCoord->coord.t[1] = landingY;
        if (landingY >= 0) {
            padScriptSpawnVariableMotorRamp(0x10, 0x80, 0x20);
            work->stateFrames = 0;
            _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE);
            rootCoord->coord.t[1] = 0;
            work->state++;
        }
    }
}

/// Disables body and arm pair tests while preserving their grid tests and links.
///
/// Requires the live work and initialized collision spheres. Leaves the probe
/// capsule, shape kinds and contact storage intact.
static inline void _actor400600DisableBodyAndArmPairs(_Actor400600ZebraStalkerWork* work)
{
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Hides the actor and disables pair tests before its commanded floor-drop entrance.
///
/// Entry 0 of scripted task state 5 (spawn entrance kind 2). Clears shadow
/// brightness, requests cloaking and advances to the room-command wait.
/// Requires live work/model and retains collision links, grid tests and resources.
static void _actor400600HideForFloorDropEntrance(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = task->work;
    model             = task->extra.tmd;
    work->shadowShade = 0;
    _actor400600StartCloakFade(task, ACTOR_400600_CLOAK);
    _actor400600DisableBodyAndArmPairs(work);
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

/// Starts the commanded floor drop or places the actor directly into running behavior.
///
/// Entry 1 of scripted task state 5. Latched request 2 (room command 1) reveals
/// at the elevated start, zeroes vertical motion and requests leap clip 21.
/// Request 3 (command 2) places it on the floor with body-only pair tests and
/// enters running behavior 0; it does not reveal or alter the model draw flag.
/// Other requests wait. Requires live work/model and an initialized body rig;
/// fixed positions use the root's parent frame, angles use 4096 units per turn.
static void _actor400600WaitFloorDropEntranceCommand(Task* task)
{
    enum { ACTOR_400600_FLOOR_DROP_REQUEST  = 2,
           ACTOR_400600_FLOOR_START_REQUEST = 3,
           ACTOR_400600_TASK_RUNNING        = 1,
           ACTOR_400600_STATE_INITIAL       = 0 };
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    GfxCoord*                     rootCoord;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (work->roomCommand == ACTOR_400600_FLOOR_DROP_REQUEST) {
        rootCoord->coord.t[0] = 0x1FDD;
        rootCoord->coord.t[1] = -0xE38;
        rootCoord->coord.t[2] = 0x5CE;
        work->pitch           = 0;
        work->yaw             = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        work->roll            = 0;
        model->flags         &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->moveAccel       = 0;
        work->moveSpeed       = 0;
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LEAP, ANIMATION_RATE_ONE);
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
        work->state++;
    } else if (work->roomCommand == ACTOR_400600_FLOOR_START_REQUEST) {
        _actor400600EnableBodyPairOnly(work);
        rootCoord->coord.t[0] = 0x640;
        rootCoord->coord.t[2] = 0x87A;
        rootCoord->coord.t[1] = 0;
        work->pitch           = 0;
        work->yaw             = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        work->roll            = 0;
        work->shadowShade     = 0;
        task->state           = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
    }
}

/// Waits for the commanded floor-drop landing, then enables body hits and starts walking.
///
/// Final entry 3 of scripted task state 5. Completion queues body-bank cue 4.
/// Enables body pair tests, disables both arm attack spheres and enters running
/// behavior 2 only at a body-slot boundary, jump or settled end. Requires live
/// work, model and initialized body rig plus a loaded sound bank and audio scratch.
static void _actor400600FinishFloorDropLanding(Task* task)
{
    enum { ACTOR_400600_TASK_RUNNING  = 1,
           ACTOR_400600_STATE_INITIAL = 0 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_BODY_CUE4);
        _actor400600EnableBodyPairOnly(work);
        // Preserve the task-entry reset before selecting walking.
        task->state = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
    }
}

/// Hides the body and disables pair tests before either water wall-walk entrance.
///
/// Entry 0 of scripted task state 4 (spawn entrance kind 1). Clears shadow
/// brightness and advances to the room-command wait without changing the cloak
/// target; the wall walk requests reveal at its timed cue.
/// Requires live work/model and retains collision links, grid tests and resources.
static void _actor400600HideForWaterEntrance(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = task->work;
    model             = task->extra.tmd;
    work->shadowShade = 0;
    _actor400600DisableBodyAndArmPairs(work);
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

/// Waits for the first water landing, then enables body hits, engages battle and walks.
///
/// Entry 4 of scripted task state 4, after the first wall walk and leap.
/// Enables body pair tests, disables both arm attack spheres and enters running
/// behavior 2 only at a body-slot boundary, jump or settled end. Requires live
/// work, model and initialized body rig.
static void _actor400600FinishFirstWaterLanding(Task* task)
{
    enum { ACTOR_400600_TASK_RUNNING  = 1,
           ACTOR_400600_STATE_INITIAL = 0 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600EnableBodyPairOnly(work);
        sceneEngageBattle(1);
        // Preserve the task-entry reset before selecting walking.
        task->state = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
    }
}

/// Walks the second water wall path, then reveals and starts its leap to the floor.
///
/// Entry 5 of scripted task state 4, with a zeroed counter and initialized walk
/// rig and hand anchors. Reveals at update 38, eases shadow shade toward 255 from
/// update 39, and at 80 resets the counter and vertical motion (s16 acceleration
/// -10, speed zero), blends leap clip 21 over four frames and advances to entry 6.
/// Requires live work/model and the wall-walk part-query and sound scratch stack.
static void _actor400600TickSecondWaterWallWalk(Task* task)
{
    enum { ACTOR_400600_SECOND_WATER_REVEAL_FRAME = 38,
           ACTOR_400600_SECOND_WATER_SHADOW_FRAME = 39,
           ACTOR_400600_SECOND_WATER_WALK_FRAMES  = 80,
           ACTOR_400600_SECOND_WATER_LEAP_ACCEL   = -10 };
    _Actor400600ZebraStalkerWork* work = task->work;

    work->stateFrames++;
    _actor400600TickWallWalk(task);
    if (work->stateFrames == ACTOR_400600_SECOND_WATER_REVEAL_FRAME) {
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    }
    if (work->stateFrames >= ACTOR_400600_SECOND_WATER_SHADOW_FRAME) {
        work->shadowShade = (u16)work->shadowShade + ((ACTOR_400600_SHADOW_SHADE_FULL - work->shadowShade) >> 4);
    }
    if (work->stateFrames == ACTOR_400600_SECOND_WATER_WALK_FRAMES) {
        work->stateFrames = 0;
        work->moveAccel   = ACTOR_400600_SECOND_WATER_LEAP_ACCEL;
        work->moveSpeed   = 0;
        _actor400600RequestClipBlend(task, ACTOR_400600_CLIP_LEAP, ANIMATION_RATE_ONE, ACTOR_400600_LEAP_BLEND_FRAMES);
        work->state++;
    }
}

/// Waits for the second water landing, then sounds its cue, engages battle and walks.
///
/// Final entry 7 of scripted task state 4. Completion queues water-bank cue 4.
/// Enables body pair tests, disables both arm attack spheres and enters running
/// behavior 2 only at a body-slot boundary, jump or settled end. Requires live
/// work, model and initialized body rig plus a loaded sound bank and audio scratch.
static void _actor400600FinishSecondWaterLanding(Task* task)
{
    enum { ACTOR_400600_TASK_RUNNING  = 1,
           ACTOR_400600_STATE_INITIAL = 0 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_WATER_CUE4);
        _actor400600EnableBodyPairOnly(work);
        sceneEngageBattle(1);
        // Preserve the task-entry reset before selecting walking.
        task->state = ACTOR_400600_TASK_RUNNING;
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_INITIAL);
        _stalkerZebraIvorySelectState(task, ACTOR_400600_STATE_WALK);
    }
}

/// Hides the actor and disables pair tests before the group-triggering drop.
///
/// Entry 0 of scripted task state 7 (spawn entrance kind 4). Clears shadow
/// brightness, requests cloaking and advances to the room-command wait.
/// Requires live work/model and retains collision links, grid tests and resources.
static void _actor400600HideForGroupDropEntrance(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = task->work;
    model             = task->extra.tmd;
    work->shadowShade = 0;
    _actor400600StartCloakFade(task, ACTOR_400600_CLOAK);
    _actor400600DisableBodyAndArmPairs(work);
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

/// Accelerates the group entrance to floor Y zero and requests its landing clip.
///
/// Entry 2 of scripted task state 7. Eases shadow shade toward 255 by 1/32 and
/// adds one to s16 acceleration, then that narrowed value to s16 speed before
/// moving s32 root Y in parent-coordinate units. At or below the floor, resets
/// the counter, queues body-bank landing cue 3, requests clip 25 at normal rate,
/// clamps Y to zero and advances. Requires live work/model, initialized body rig,
/// a loaded sound bank, current root cache and the audio-query scratch stack.
static void _actor400600TickGroupEntranceDrop(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;

    work               = task->work;
    rootCoord          = task->extra.tmd->coords;
    work->shadowShade += (ACTOR_400600_SHADOW_SHADE_FULL - work->shadowShade) >> 5;
    // Each motion assignment narrows to s16 before the next integration stage.
    work->moveAccel       += 1;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if (rootCoord->coord.t[1] >= 0) {
        work->stateFrames = 0;
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_LANDING);
        _stalkerZebraIvoryRequestClipRestart(task, ACTOR_400600_CLIP_LANDING, ANIMATION_RATE_ONE);
        rootCoord->coord.t[1] = 0;
        work->state++;
    }
}

/// Finishes the group drop, releases delayed entrants and starts walking.
///
/// Final entry 3 of task state 7, requiring a zeroed landing counter and
/// loaded clip 25. The first update triggers vibration. Clip completion sounds
/// body cue 4, enables only body pair tests and publishes delayed-group phase 1
/// before entering task state 1 and running behavior 2. Requires live work/model,
/// Enemy, initialized collision and the audio-query scratch stack.
static void _actor400600FinishGroupDropLanding(Task* task)
{
    enum { ACTOR_400600_TASK_RUNNING  = 1,
           ACTOR_400600_STATE_INITIAL = 0 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames == 1) {
        padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
    }
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _actor400600PlayJunkYardEntranceCue(task, ACTOR_400600_SOUND_BODY_CUE4);
        _actor400600EnableBodyPairOnly(work);
        gSceneCombatState.zebraStalkerGroupPhase = SCENE_COMBAT_ZEBRA_STALKER_DELAYED;
        task->state                              = ACTOR_400600_TASK_RUNNING;
        _actor400600SelectStateInline(task, ACTOR_400600_STATE_INITIAL);
        _actor400600SelectStateInline(task, ACTOR_400600_STATE_WALK);
    }
}

/// Queues the Zebra Stalker sound bundle once for the scene's enemy-bank latch.
///
/// Loads global-CDF group 40, suffix 6, file 1; night Water Hole variant 1
/// selects file 2. Sets the shared latch after enqueueing, before completion.
/// Requires live session state and free CD-ring capacity. The queue immediately
/// copies key bytes 3/2/0 and four load-option bytes, retaining neither pointer.
/// File-key byte 1 is unused and is left unwritten.
static void _actor400600QueueSoundBank(void)
{
    enum { ACTOR_400600_SOUND_FILE_GROUP      = 40,
           ACTOR_400600_SOUND_FILE_SUFFIX     = 6,
           ACTOR_400600_SOUND_FILE_DEFAULT    = 1,
           ACTOR_400600_SOUND_FILE_WATER_HOLE = 2 };
    u8 fileKeyBytes[4];
    u8 loadArgs[sizeof(((CdCmdEntry*)0)->args.bytes)];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 0, 0) && gGameSession->location.loc.variant == 1) {
            fileKeyBytes[2] = ACTOR_400600_SOUND_FILE_GROUP;
            fileKeyBytes[0] = ACTOR_400600_SOUND_FILE_WATER_HOLE;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = ACTOR_400600_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        } else {
            fileKeyBytes[2] = ACTOR_400600_SOUND_FILE_GROUP;
            fileKeyBytes[0] = ACTOR_400600_SOUND_FILE_DEFAULT;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = ACTOR_400600_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        }
        gSceneCombatState.enemySoundBankQueued = 1;
    }
}

static void func_actor_400600_8013B6F4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->turnStep    = 0x18;
    work->walkHurried = 0;
    work->walkStep    = 0x10;
    _actor400600TickHorizontalWalk(arg0, 0x10);
    work->subState++;
}

static void func_actor_400600_8013B740(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    SVECTOR                       pos;
    s16                           min;
    s16                           step;
    u32                           rnd;

    min = 0x10;
    if (work->playerDistance > 0xBB8 && work->walkHurried == 0) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if ((rnd >> 0x10) & 1) {
            min  = 0x18;
            step = 0x24;
        } else {
            min  = 0x14;
            step = 0x1E;
        }
        work->turnStep    = step;
        work->walkHurried = 1;
    }
    if (work->walkStep < min) {
        work->walkStep = min;
    }
    pos.vx = work->targetPos.vx;
    pos.vy = work->targetPos.vy;
    pos.vz = work->targetPos.vz;
    _stalkerZebraIvoryTurnToward(arg0, &pos, work->turnStep);
    _actor400600TickHorizontalWalk(arg0, work->walkStep);
}

/// Starts light-hit recoil for the upright or on-back pose and reveals the actor.
///
/// Entry of running behavior 3: requests clip 9 upright or 11 on the back,
/// at twice normal rate with a two-frame blend, then advances to the wait step.
/// Requires an initialized rig with the requested body clip loaded.
static void _actor400600StartLightRecoil(Task* task)
{
    enum { ACTOR_400600_CLIP_ON_BACK_LIGHT_RECOIL = 11,
           ACTOR_400600_LIGHT_RECOIL_BLEND_FRAMES = 2 };
    _Actor400600ZebraStalkerWork* work = task->work;

    if (work->onBack == 0) {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_UPRIGHT_LIGHT_RECOIL, ANIMATION_RATE_ONE * 2, ACTOR_400600_LIGHT_RECOIL_BLEND_FRAMES);
    } else {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_LIGHT_RECOIL, ANIMATION_RATE_ONE * 2, ACTOR_400600_LIGHT_RECOIL_BLEND_FRAMES);
    }
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Queues a body-clip restart at a rate measured in sixteenths of a frame per tick.
///
/// Requires a live initialized rig and a loaded clip (bank entries 1..22 or
/// 25..34) supporting body tracks 1..17. The next animation tick resets the
/// clip and frame counter even when repeating the clip; slot rates narrow to s8.
/// This writes only the request and leaves blending and slot playback untouched.
static inline void _actor400600SetRestartRequest(Task* task, s16 clipIndex, s16 rate)
{
    _Actor400600ZebraStalkerWork* animationWork = task->work;

    animationWork->animStep    = rate;
    animationWork->animClip    = clipIndex;
    animationWork->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
}

/// Restarts light recoil on another light hit, otherwise waits for reaction or completion.
///
/// Sub-state 1 of running behavior 3. A new armed light reaction restarts clip 9
/// upright or 11 on the back at twice normal rate. Other armed reactions take
/// precedence over returning a finished clip to walking or on-back crawling.
static void _actor400600TickLightRecoil(Task* task)
{
    enum { ACTOR_400600_CLIP_ON_BACK_LIGHT_RECOIL = 11 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if (work->pendingArmed != 0 && work->pendingAction == STALKER_ZEBRA_IVORY_PENDING_LIGHT) {
        if (work->onBack == 0) {
            _actor400600SetRestartRequest(task, ACTOR_400600_CLIP_UPRIGHT_LIGHT_RECOIL, ANIMATION_RATE_ONE * 2);
        } else {
            _actor400600SetRestartRequest(task, ACTOR_400600_CLIP_ON_BACK_LIGHT_RECOIL, ANIMATION_RATE_ONE * 2);
        }
        return;
    }
    if ((s16)_actor400600TakeArmedHitReaction(task) == 0 && (s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->onBack == 0) {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
        } else {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_ON_BACK);
        }
    }
}

/// Reveals the actor and starts heavy recoil for its upright or on-back pose.
///
/// Entry of running behavior 4: requests loaded clip 10 upright or 12 on the
/// back at normal rate with a three-frame blend, then advances to the wait step.
static void _actor400600StartHeavyRecoil(Task* task)
{
    enum { ACTOR_400600_CLIP_UPRIGHT_HEAVY_RECOIL = 10,
           ACTOR_400600_CLIP_ON_BACK_HEAVY_RECOIL = 12,
           ACTOR_400600_HEAVY_RECOIL_BLEND_FRAMES = 3 };
    _Actor400600ZebraStalkerWork* work = task->work;

    if (work->onBack == 0) {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_UPRIGHT_HEAVY_RECOIL, ANIMATION_RATE_ONE, ACTOR_400600_HEAVY_RECOIL_BLEND_FRAMES);
    } else {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_HEAVY_RECOIL, ANIMATION_RATE_ONE, ACTOR_400600_HEAVY_RECOIL_BLEND_FRAMES);
    }
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Returns completed heavy recoil to walking or crawling on the back.
///
/// Sub-state 1 of running behavior 4. Leaves the behavior unchanged until body
/// slot 1 reports completion; selecting behavior 2 or 10 resets its sub-state.
/// Unlike light recoil, this wait does not dispatch another armed reaction.
static void _actor400600FinishHeavyRecoil(Task* task)
{
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->onBack == 0) {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
        } else {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_ON_BACK);
        }
    }
}

/// Reveals the actor and starts its upright or on-back status-hold pose.
///
/// Upright requests clip 15 with an eight-frame blend; on the back requests
/// clip 17 with a three-frame blend. Both use normal rate and advance to the
/// sub-state that ticks enemy buildup.
static void _actor400600StartStatusHold(Task* task)
{
    enum {
        ACTOR_400600_CLIP_UPRIGHT_STATUS_HOLD    = 15,
        ACTOR_400600_CLIP_ON_BACK_STATUS_HOLD    = 17,
        ACTOR_400600_STATUS_UPRIGHT_BLEND_FRAMES = 8,
        ACTOR_400600_STATUS_ON_BACK_BLEND_FRAMES = 3,
    };
    _Actor400600ZebraStalkerWork* work = task->work;

    if (work->onBack == 0) {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_UPRIGHT_STATUS_HOLD, ANIMATION_RATE_ONE, ACTOR_400600_STATUS_UPRIGHT_BLEND_FRAMES);
    } else {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_STATUS_HOLD, ANIMATION_RATE_ONE, ACTOR_400600_STATUS_ON_BACK_BLEND_FRAMES);
    }
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Ticks enemy buildup until expiry, then starts the pose's recovery clip.
///
/// Leaves the pose unchanged until `damageTickEnemyBuildup` reports expiry.
/// Requests clip 16 upright or clip 18 on the back at normal rate with an
/// eight-frame blend, then advances to the recovery sub-state.
static void _actor400600WaitStatusHold(Task* task)
{
    enum {
        ACTOR_400600_CLIP_ON_BACK_STATUS_RECOVER = 18,
        ACTOR_400600_STATUS_RECOVER_BLEND_FRAMES = 8,
    };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        if (work->onBack == 0) {
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_STANDING_REST, ANIMATION_RATE_ONE, ACTOR_400600_STATUS_RECOVER_BLEND_FRAMES);
        } else {
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_STATUS_RECOVER, ANIMATION_RATE_ONE, ACTOR_400600_STATUS_RECOVER_BLEND_FRAMES);
        }
        work->subState++;
    }
}

/// Returns a completed status recovery to walking or crawling on the back.
///
/// Completion selects running behavior 2 upright or 10 on the back and clears
/// its sub-state. Does nothing while the recovery clip is still playing.
static void _actor400600FinishStatusRecovery(Task* task)
{
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->onBack == 0) {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
        } else {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_ON_BACK);
        }
    }
}

/// Blends into the left-arm strike, extends that arm and reveals the actor.
///
/// Entry of running behavior 6. Requests loaded clip 7 at normal rate with an
/// eight-frame blend and resets the update counter before advancing to the
/// strike step. The arm's attack sphere is enabled later by that step.
static void _actor400600StartLeftStrike(Task* task)
{
    enum { ACTOR_400600_CLIP_LEFT_STRIKE         = 7,
           ACTOR_400600_LEFT_STRIKE_BLEND_FRAMES = 8 };
    _Actor400600ZebraStalkerWork* work = task->work;

    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LEFT_STRIKE, ANIMATION_RATE_ONE, ACTOR_400600_LEFT_STRIKE_BLEND_FRAMES);
    work->stateFrames = 0;
    _actor400600ExtendStrikeArm(task, ACTOR_400600_ARM_LEFT);
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Blends into the right-arm strike, extends that arm and reveals the actor.
///
/// Entry of running behavior 7. Requests loaded clip 8 at normal rate with an
/// eight-frame blend and resets the update counter before advancing to the
/// strike step. The arm's attack sphere is enabled later by that step.
static void _actor400600StartRightStrike(Task* task)
{
    enum { ACTOR_400600_CLIP_RIGHT_STRIKE         = 8,
           ACTOR_400600_RIGHT_STRIKE_BLEND_FRAMES = 8 };
    _Actor400600ZebraStalkerWork* work = task->work;

    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_RIGHT_STRIKE, ANIMATION_RATE_ONE, ACTOR_400600_RIGHT_STRIKE_BLEND_FRAMES);
    work->stateFrames = 0;
    _actor400600ExtendStrikeArm(task, ACTOR_400600_ARM_RIGHT);
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Reveals the Stalker and starts the grab's first windup unless a hit interrupts.
///
/// Entry 0 of running behavior 8. On no armed reaction, resets the update
/// counter, requests reveal and a four-frame blend into loaded clip 1 at normal
/// rate, then advances the sub-state. Requires live work/model and body rig;
/// neither the wall probe nor the player's scripted hold begins here.
static void _actor400600StartGrabWindup(Task* task)
{
    enum { ACTOR_400600_CLIP_GRAB_READY         = 1,
           ACTOR_400600_GRAB_READY_BLEND_FRAMES = 4 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        work->stateFrames = 0;
        _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_GRAB_READY, ANIMATION_RATE_ONE, ACTOR_400600_GRAB_READY_BLEND_FRAMES);
        work->subState++;
    }
}

/// Waits seventeen uninterrupted grab updates, then blends into the leap windup.
///
/// Entry 1 of running behavior 8. Gives an armed hit reaction priority on every
/// update. Otherwise increments the s16 counter; at 17 or later resets it,
/// requests loaded leap clip 21 at normal rate with a four-frame blend and
/// advances. Requires live work and initialized rig; animation end is not tested.
static void _actor400600StartGrabLeapWindup(Task* task)
{
    enum { ACTOR_400600_GRAB_READY_FRAMES = 17 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        work->stateFrames++;
        if (work->stateFrames >= ACTOR_400600_GRAB_READY_FRAMES) {
            work->stateFrames = 0;
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_LEAP, ANIMATION_RATE_ONE, ACTOR_400600_LEAP_BLEND_FRAMES);
            work->subState++;
        }
    }
}

/// Waits seventeen uninterrupted leap-windup updates before the grab's target probe.
///
/// Entry 2 of running behavior 8, with the counter reset by the preceding step.
/// Gives an armed hit reaction priority; otherwise increments the s16 counter
/// and advances at 17 or later. Retains that counter and the animation request.
/// Requires the live work; this is a timed gate, not an animation-end test.
static void _actor400600WaitGrabLeapWindup(Task* task)
{
    enum { ACTOR_400600_GRAB_LEAP_WINDUP_FRAMES = 17 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    if ((s16)_actor400600TakeArmedHitReaction(task) == 0) {
        work->stateFrames++;
        if (work->stateFrames >= ACTOR_400600_GRAB_LEAP_WINDUP_FRAMES) {
            work->subState++;
        }
    }
}

/// Probes for a wall toward the grab target and clears the previous player-death latch.
///
/// Entry 3 of running behavior 8. Requires live work/model, a current target in
/// the root's parent frame and the linked probe capsule. Enables its grid test
/// and clears old contacts; a collision update must fill the new contacts before
/// the next grab step tests them. Advances the sub-state without taking a hit
/// reaction or changing the frame counter, animation or player task.
static void _actor400600StartGrabTargetProbe(Task* task)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    _actor400600StartWallProbe(task, ACTOR_400600_PROBE_TARGET);
    work->playerDied = 0;
    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_release_hold.inc.c"

/// Starts the backward leap's rear wall probe before advancing to its resolution.
///
/// Entry 0 of running behavior 9. Requires the linked capsule and live model.
/// Clears contacts and enables its grid test for a 3000-unit rear probe; the
/// collision pass must fill contacts before sub-state 1 consumes the distance.
/// Leaves the frame counter, animation and receiving sphere unchanged.
static void _actor400600StartBackwardLeapProbe(Task* task)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    _actor400600StartWallProbe(task, ACTOR_400600_PROBE_BACK);
    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_wait_clip.inc.c"

/// Starts righting from the back and captures the body part's world X/Z anchor.
///
/// Entry of running behavior 11. Resets the update counter, requests loaded
/// clip 22 at normal rate with a two-frame blend, then samples part 14 for the
/// following righting step to pin. Requires current ancestor/view matrices and
/// the initialized part-query scratch stack; advances only the sub-state.
static void _actor400600StartRighting(Task* task)
{
    enum { ACTOR_400600_CLIP_RIGHTING         = 22,
           ACTOR_400600_RIGHTING_BLEND_FRAMES = 2,
           ACTOR_400600_RIGHTING_ANCHOR_PART  = 14 };
    _Actor400600ZebraStalkerWork* work;

    work              = task->work;
    work->stateFrames = 0;
    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_RIGHTING, ANIMATION_RATE_ONE, ACTOR_400600_RIGHTING_BLEND_FRAMES);
    _stalkerZebraIvoryReadPartWorldXZ(task, ACTOR_400600_RIGHTING_ANCHOR_PART, &work->anchorPos);
    work->subState++;
}

/// Starts the ceiling leap's windup and resets its update counter.
///
/// Entry of running behavior 12: requests loaded clip 21 at normal rate with
/// a four-frame blend, then advances to the leap step. The ceiling probe must
/// already have supplied `leapY`.
static void _actor400600StartCeilingLeap(Task* task)
{
    enum { ACTOR_400600_CLIP_CEILING_LEAP         = 21,
           ACTOR_400600_CEILING_LEAP_BLEND_FRAMES = 4 };
    _Actor400600ZebraStalkerWork* work = task->work;

    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_CEILING_LEAP, ANIMATION_RATE_ONE, ACTOR_400600_CEILING_LEAP_BLEND_FRAMES);
    work->stateFrames = 0;
    work->subState++;
}

/// Returns a completed ceiling-leap landing to walking with a 210..241-update cooldown.
///
/// Final sub-state of running behavior 12, with the actor attached overhead.
/// A pending hit reaction takes precedence over clip completion. Only an
/// uninterrupted completion consumes one random draw and selects walking.
static void _actor400600FinishCeilingLeapLanding(Task* task)
{
    enum { ACTOR_400600_CEILING_COOLDOWN_BASE = 210,
           ACTOR_400600_CEILING_COOLDOWN_MASK = 31 };
    _Actor400600ZebraStalkerWork* work;
    u32                           randomValue;

    work = task->work;
    if ((s16)_stalkerZebraIvoryApplyPendingReaction(task) == 0 && (s16)_stalkerZebraIvoryClipDone(task) != 0) {
        randomValue           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = randomValue;
        work->ceilingCooldown = ((randomValue >> 0x10) & ACTOR_400600_CEILING_COOLDOWN_MASK) + ACTOR_400600_CEILING_COOLDOWN_BASE;
        _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
    }
}

/// Defers death until the voluntary ceiling drop has landed.
///
/// Entry of running behavior 13: latches the mandatory-move flag and advances
/// to drop setup. The landing handler clears the flag so death can then begin.
static void _actor400600CommitCeilingDrop(Task* task)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    work->holding = 1;
    work->subState++;
}

/// Prepares the voluntary ceiling drop and snapshots its landing X/Z.
///
/// Sub-state 1 of running behavior 13, after death has been deferred. Disables
/// the capsule probe and requests loaded clip 32 at normal rate over four
/// blend frames. Starts with speed zero and acceleration 64 game units per
/// update squared; the following step increases acceleration while falling.
static void _actor400600StartCeilingDrop(Task* task)
{
    enum { ACTOR_400600_CLIP_CEILING_DROP          = 32,
           ACTOR_400600_CEILING_DROP_BLEND_FRAMES  = 4,
           ACTOR_400600_CEILING_DROP_INITIAL_ACCEL = 64 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _stalkerZebraIvoryDisableCapsuleGrid(task);
    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_CEILING_DROP, ANIMATION_RATE_ONE, ACTOR_400600_CEILING_DROP_BLEND_FRAMES);
    work->moveAccel    = ACTOR_400600_CEILING_DROP_INITIAL_ACCEL;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->leapX        = rootCoord->coord.t[0];
    work->leapZ        = rootCoord->coord.t[2];
    work->shadowHeight = work->floorY;
    work->subState++;
}

/// Starts a hit-induced fall from the ceiling and reveals the actor.
///
/// Entry of running behavior 14: requests loaded clip 9 at normal rate with a
/// two-frame blend, defers death until landing and zeroes vertical acceleration
/// and speed. Moves shadows to the spawn floor height and advances to falling.
static void _actor400600StartCeilingFall(Task* task)
{
    enum { ACTOR_400600_CEILING_FALL_BLEND_FRAMES = 2 };
    _Actor400600ZebraStalkerWork* work;

    work = task->work;
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_UPRIGHT_LIGHT_RECOIL, ANIMATION_RATE_ONE, ACTOR_400600_CEILING_FALL_BLEND_FRAMES);
    work->holding   = 1;
    work->moveAccel = 0;
    work->moveSpeed = 0;
    work->subState++;
    work->shadowHeight = work->floorY;
}

/// Accelerates a ceiling fall downward and begins the on-back landing when it crosses the floor.
///
/// Sub-state 1 of running behavior 14. Each update adds two to s16 acceleration,
/// then adds that narrowed value to s16 speed before the root Y moves.
/// Crossing `floorY` clamps height, requests loaded clip 19 at normal rate,
/// flips roll by half a turn and advances to the landing step. Cache composition
/// remains the frame driver's responsibility.
static void _actor400600TickCeilingFall(Task* task)
{
    enum { ACTOR_400600_CLIP_ON_BACK_LANDING = 19 };
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     rootCoord;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    work->moveAccel        = work->moveAccel + 2;
    work->moveSpeed        = work->moveSpeed + work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if (work->floorY < rootCoord->coord.t[1]) {
        rootCoord->coord.t[1] = work->floorY;
        _actor400600SetRestartRequest(task, ACTOR_400600_CLIP_ON_BACK_LANDING, ANIMATION_RATE_ONE);
        work->onBack      = 1;
        work->stateFrames = 0;
        work->onCeiling   = 0;
        work->roll        = work->roll + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_finish_ceiling_fall.inc.c"

/// Begins the knockdown recoil for the current upright or on-back pose.
///
/// Requests clip 26 upright or 27 on the back with a three-frame blend at normal
/// rate, starts revealing the enemy, then advances to the waiting sub-state.
static void _actor400600StartKnockdownRecoil(Task* task)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)task->work;

    if (work->onBack == 0) {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_KNOCKDOWN_UPRIGHT, ANIMATION_RATE_ONE, ACTOR_400600_KNOCKDOWN_RECOIL_BLEND_FRAMES);
    } else {
        _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_KNOCKDOWN_ON_BACK, ANIMATION_RATE_ONE, ACTOR_400600_KNOCKDOWN_RECOIL_BLEND_FRAMES);
    }
    _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
    work->subState++;
}

/// Blends a finished knockdown recoil into its resting pose and advances the sub-state.
///
/// Upright uses clip 16 at normal rate; on the back uses clip 20 at half rate.
/// Both blend over 30 frames. Does nothing until the current clip completes.
static void _actor400600BlendKnockdownRest(Task* task)
{
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->onBack == 0) {
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_STANDING_REST, ANIMATION_RATE_ONE, ACTOR_400600_KNOCKDOWN_REST_BLEND_FRAMES);
        } else {
            _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_ON_BACK_REST, ANIMATION_RATE_ONE / 2, ACTOR_400600_KNOCKDOWN_REST_BLEND_FRAMES);
        }
        work->subState++;
    }
}

/// Returns a completed knockdown rest to walking upright or crawling on the back.
///
/// A completed clip selects running state 2 or 10 respectively and resets its
/// sub-state. Does nothing while the clip is still playing.
static void _actor400600FinishKnockdownRest(Task* task)
{
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)task->work;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        if (work->onBack == 0) {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
        } else {
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_ON_BACK);
        }
    }
}

/// Advances ceiling-exit behavior to its distance-based move selection.
///
/// Entry of running behavior 16. Requires live work and sub-state zero; changes
/// only the signed-halfword sub-state to one, selecting the drop/grab chooser.
static void _actor400600StartCeilingExit(Task* task)
{
    _Actor400600ZebraStalkerWork* work = task->work;

    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_select_ceiling_exit.inc.c"

/// Starts the idle pose and a 90..153-update wait after cloaking.
///
/// Entry of running behavior 17. Requests loaded clip 1 at normal rate with a
/// four-frame blend, consumes one random draw and advances to the countdown.
/// The preceding behavior has already requested cloaking; this does not fade.
static void _actor400600StartHiddenIdle(Task* task)
{
    enum { ACTOR_400600_CLIP_IDLE         = 1,
           ACTOR_400600_IDLE_BLEND_FRAMES = 4,
           ACTOR_400600_IDLE_TIMER_BASE   = 90,
           ACTOR_400600_IDLE_TIMER_MASK   = 63 };
    _Actor400600ZebraStalkerWork* work = task->work;
    u32                           randomValue;

    _actor400600SetBlendRequest(task, ACTOR_400600_CLIP_IDLE, ANIMATION_RATE_ONE, ACTOR_400600_IDLE_BLEND_FRAMES);
    randomValue      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState  = randomValue;
    work->idleFrames = ((randomValue >> 0x10) & ACTOR_400600_IDLE_TIMER_MASK) + ACTOR_400600_IDLE_TIMER_BASE;
    work->subState++;
}

/// Waits hidden for a reaction, an attack opportunity or expiry of the idle timer.
///
/// Sub-state 1 of running behavior 17, initialized with 90..153 updates left.
/// Armed reactions run before attack selection; either prevents the countdown.
/// Expiry consumes one random draw for a 30..93-update hide cooldown, reveals
/// the actor and returns to walking with its sub-state reset. The countdown
/// narrows to u16 before testing zero.
static void _actor400600TickHiddenIdle(Task* task)
{
    enum { ACTOR_400600_HIDE_COOLDOWN_BASE = 30,
           ACTOR_400600_HIDE_COOLDOWN_MASK = 63 };
    _Actor400600ZebraStalkerWork* work;
    u16                           framesLeft;
    u32                           randomValue;

    work = task->work;
    if (((s16)_actor400600TakeArmedHitReaction(task) == 0) && ((s16)_actor400600TrySelectAttack(task) == 0)) {
        framesLeft       = work->idleFrames - 1;
        work->idleFrames = framesLeft;
        if (((u32)framesLeft << 16) == 0) {
            randomValue        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState    = randomValue;
            work->hideCooldown = ((randomValue >> 0x10) & ACTOR_400600_HIDE_COOLDOWN_MASK) + ACTOR_400600_HIDE_COOLDOWN_BASE;
            _actor400600StartCloakFade(task, ACTOR_400600_REVEAL);
            _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
        }
    }
}

/// Computes a room-axis horizontal push away from a body contact's centre.
///
/// Inputs share the composed view frame and use signed integer game units;
/// `contact->distance` is the summed sphere radii. Depth is X/Z overlap clamped
/// to zero, while direction uses full XYZ separation normalized to Q12 and
/// transformed by the active grid view basis's transpose. Writes s16 X/Z and
/// zero Y; `pad` is untouched. Inputs are borrowed only during this call.
/// Requires the active grid/view cache, SDK normalization range and s32-safe
/// squared differences/products. GTE state changes even for zero overlap.
static void _actorContactCalcHorizontalPushback(const SVECTOR* position, const WorldCollisionContact* contact, SVECTOR* pushDelta)
{
    enum { ACTOR_CONTACT_DIRECTION_FRACTION_BITS = 12 };
    VECTOR delta;
    VECTOR normalizedDirection;
    s32    dx;
    s32    dz;
    s32    penetrationDepth;

    // Measure horizontal depth, then remove the view rotation from XYZ direction.
    dx               = position->vx - contact->point.vx;
    delta.vy         = 0;
    delta.vx         = dx;
    dz               = position->vz - contact->point.vz;
    delta.vz         = dz;
    penetrationDepth = contact->distance - SquareRoot0(dx * dx + dz * dz);
    penetrationDepth = (penetrationDepth <= 0) ? 0 : penetrationDepth;
    delta.vx         = position->vx - contact->point.vx;
    delta.vy         = position->vy - contact->point.vy;
    delta.vz         = position->vz - contact->point.vz;
    VectorNormal(&delta, &normalizedDirection);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &normalizedDirection, &delta);
    pushDelta->vx = (penetrationDepth * delta.vx) >> ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
    pushDelta->vy = 0;
    pushDelta->vz = (penetrationDepth * delta.vz) >> ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
}

/// Chooses one horizontal correction from a grid step and an eased body push.
///
/// Both inputs and the returned step are signed-halfword game units. Arithmetic
/// right-shift by three eases the body push (negative values round downward).
/// With a zero grid step returns that push; opposite directions keep the grid
/// step; matching directions take whichever has greater magnitude. No state or
/// pointers are retained and no saturation is performed.
static s16 _actor400600SelectCollisionStep(s16 gridStep, s16 bodyPush)
{
    enum { ACTOR_400600_BODY_PUSH_EASING_SHIFT = 3 };
    s16 easedBodyPush;

    easedBodyPush = bodyPush >> ACTOR_400600_BODY_PUSH_EASING_SHIFT;
    if (gridStep == 0) {
        return easedBodyPush;
    }
    if ((gridStep > 0 && easedBodyPush < 0) || (gridStep < 0 && easedBodyPush > 0)) {
        return gridStep;
    }
    if (gridStep > 0) {
        if (easedBodyPush < gridStep) {
            return gridStep;
        }
        return easedBodyPush;
    }
    if (easedBodyPush < gridStep) {
        return easedBodyPush;
    }
    return gridStep;
}

/// Enters the running task at behavior zero with a reset sub-state cursor.
///
/// Requires the live Zebra Stalker work. Writes `Task::state` as 1 and both
/// work cursors as zero. Animation, counters, collision flags and pending
/// reactions are left for the caller or running behavior entry to initialize.
static inline void _actor400600EnterRunningTask(Task* task)
{
    enum { ACTOR_400600_TASK_RUNNING  = 1,
           ACTOR_400600_STATE_INITIAL = 0 };
    _Actor400600ZebraStalkerWork* stateWork = task->work;

    task->state         = ACTOR_400600_TASK_RUNNING;
    stateWork->state    = ACTOR_400600_STATE_INITIAL;
    stateWork->subState = 0;
}

/// Hides the actor and disables body/arm pair collisions before the group entrance.
///
/// Entry zero of scripted task state 8. Starts cloaking if needed, suppresses
/// the body draw and floor-shadow brightness, then waits for the scene's group
/// phase. Collision bodies remain linked; only pair testing is disabled.
static void _actor400600HideForGroupEntrance(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = task->work;
    model             = task->extra.tmd;
    work->shadowShade = 0;
    _actor400600StartHideFadeInline(task);
    work->body.flags         &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->rightArmBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->leftArmBody.flags  &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

/// Waits for the scene's group phase, then schedules a staggered entrance or joins immediately.
///
/// Entry one of scripted task state 8. DELAYED consumes one random draw for a
/// 20..27-update countdown and advances to entry two; ACTIVE enables body pair
/// collision and enters running behavior zero. Both arms stay non-attacking.
/// Other phases leave this entry waiting.
static void _actor400600WaitGroupEntrance(Task* task)
{
    enum { ACTOR_400600_GROUP_DELAY_BASE = 20,
           ACTOR_400600_GROUP_DELAY_MASK = 7 };
    _Actor400600ZebraStalkerWork* work;
    u32                           randomValue;

    work = task->work;
    if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_DELAYED) {
        randomValue       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState   = randomValue;
        work->stateFrames = ((randomValue >> 0x10) & ACTOR_400600_GROUP_DELAY_MASK) + ACTOR_400600_GROUP_DELAY_BASE;
        work->state++;
    } else if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_ACTIVE) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->leftArmBody.flags  &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        _actor400600EnterRunningTask(task);
    }
}

/// Counts down the staggered group entrance and starts walking with its positional sound.
///
/// Entry two of scripted task state 8, with a positive 20..27-update countdown.
/// On zero, enables body pair collision, keeps both arm spheres non-attacking
/// and switches to running behavior 2 with its sub-state reset. The task-state
/// switch first resets behavior to zero, then selects walking.
static void _actor400600TickGroupEntranceDelay(Task* task)
{
    _Actor400600ZebraStalkerWork* work;
    s32                           soundId;
    s32                           soundPan;

    work = task->work;
    work->stateFrames--;
    if (work->stateFrames == 0) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400600_SOUND_LANDING;
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->leftArmBody.flags  &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        _actor400600EnterRunningTask(task);
        _actor400600SelectBehavior(task, ACTOR_400600_STATE_WALK);
    }
}

#include "../../shared/stalker_zebra_ivory_apply_armed_reaction.inc.c"

/// Marks the selected strike arm as extended for its swing update.
///
/// `armIndex` is 0 for left or 1 for right. Other byte values do nothing; the
/// opposite arm's flag remains unchanged.
static void _actor400600ExtendStrikeArm(Task* task, u8 armIndex)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)task->work;

    if (armIndex == ACTOR_400600_ARM_LEFT) {
        work->leftArmOut = 1;
    } else if (armIndex == ACTOR_400600_ARM_RIGHT) {
        work->rightArmOut = 1;
    }
}

/// Requests a positional sound script using this enemy's placement instance.
///
/// `cueId` packs bank/type in bits 16..31 and entry in bits 0..7; leave its
/// instance byte clear because the placement index is ORed into bits 8..11.
/// For water entrance kind 1, replaces only bits 16..23 with bank byte 0x4A.
/// Samples the live root's composed view origin for pan and depth, narrowing
/// each to a signed byte. Requires the model, enemy and initialized projection
/// scratch stack; banks must stay loaded for the deferred script. Ignores
/// admission failure and neither composes the root nor advances animation.
static void _actor400600RequestPositionalSound(Task* task, s32 cueId)
{
    enum { ACTOR_400600_SOUND_KEEP_BANK_HIGH_BYTE = 0xFF00FFFFU,
           ACTOR_400600_SOUND_WATER_BANK_BYTE     = 0x004A0000 };
    s32 soundId;
    s32 soundPan;

    if ((task->spawnArg1.value & ACTOR_400600_SPAWN_ENTRANCE_MASK) == ACTOR_400600_SPAWN_WATER_ENTRANCE) {
        cueId &= ACTOR_400600_SOUND_KEEP_BANK_HIGH_BYTE;
        cueId |= ACTOR_400600_SOUND_WATER_BANK_BYTE;
    }
    // Keep placement voices distinct while retaining the cue's type and entry.
    soundId  = cueId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Selects an entry of the current task state's handler table and resets its sub-state.
///
/// Requires the live Zebra Stalker work and a `state` valid for the table
/// selected by `Task::state`; this does not change the task state, animation or
/// frame counters. Death handlers use entries 1 (clip), 7 (burst) and 9 (fall).
static void _actor400600SelectState(Task* task, s16 state)
{
    _actor400600SelectStateInline(task, state);
}
