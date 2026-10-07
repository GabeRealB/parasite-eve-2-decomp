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

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `gSceneCombatState.zebraStalkerGroupPhase` and `gSceneCombatState.zebraStalkerDeathAlert` are
 * bytes of the run of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `_Actor400600ZebraStalkerWork` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `gSceneCombatState.zebraStalkerDeathAlert` matches with `SOFT_BARRIER()` after
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

static void func_actor_400600_8013203C(Task* arg0);
static void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
static void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_400600_801328A8(Task* arg0);
static void func_actor_400600_801329EC(Task* arg0);
static void func_actor_400600_80132B3C(Task* arg0);
static void func_actor_400600_80132C70(Task* arg0);
static void func_actor_400600_80132E10(Task* arg0);
static void func_actor_400600_80132F3C(Task* arg0);
static void func_actor_400600_80133118(Task* arg0);
static void func_actor_400600_801332F4(Task* arg0);
static void func_actor_400600_80133434(Task* arg0);
static void func_actor_400600_801337A8(Task* arg0);
static void func_actor_400600_80133B88(Task* arg0);
static void func_actor_400600_80133CB0(Task* arg0);
static void func_actor_400600_80133E38(Task* arg0);
static void func_actor_400600_80133FC0(Task* arg0);
static void func_actor_400600_80134218(Task* arg0);
static void func_actor_400600_80134570(Task* arg0);
static void func_actor_400600_8013479C(Task* arg0);
static void func_actor_400600_80134970(Task* arg0);
static void func_actor_400600_80134E28(Task* arg0);
static void func_actor_400600_801350F4(Task* arg0);
static void func_actor_400600_80135450(Task* arg0);
static void func_actor_400600_80135578(Task* arg0);
static void func_actor_400600_801356E0(Task* arg0);
static void func_actor_400600_80135998(Task* arg0, s16 arg1);
static void func_actor_400600_801361AC(Task* arg0);
static void func_actor_400600_80136558(Task* arg0);
static void func_actor_400600_80136670(Task* arg0);
static void func_actor_400600_80136968(Task* arg0);
static s32  func_actor_400600_80136FA8(Task* arg0);
static void func_actor_400600_80137240(Task* arg0);
static void func_actor_400600_80137498(Task* arg0, s16 arg1);
static void func_actor_400600_80137840(Task* arg0);
static s32  func_actor_400600_80137AF0(Task* arg0);
static s32  func_actor_400600_80137C34(Task* arg0);
static void func_actor_400600_80137EF0(Task* arg0);
static void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade);
static void func_actor_400600_801387DC(Task* arg0, s32 arg1);
static s32  func_actor_400600_8013886C(Task* arg0);
static s32  func_actor_400600_8013892C(Task* arg0);
static void func_actor_400600_8013896C(Task* arg0, s16 arg1);
static void func_actor_400600_80138A24(Task* arg0, s16 arg1);
static void func_actor_400600_80138AB8(Task* arg0);
static void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
static void func_actor_400600_80138C34(Task* arg0);
static void func_actor_400600_80138D78(Task* arg0);
static void func_actor_400600_80138EA0(Task* arg0);
static void func_actor_400600_80138FD4(Task* arg0);
static void func_actor_400600_801390FC(Task* arg0);
static void func_actor_400600_80139110(Task* arg0);
static void func_actor_400600_80139218(Task* arg0);
static void func_actor_400600_80139280(Task* arg0);
static void func_actor_400600_801392E8(Task* arg0);
static void func_actor_400600_8013935C(Task* arg0);
static void func_actor_400600_801393D0(Task* arg0);
static void func_actor_400600_80139444(Task* arg0);
static void func_actor_400600_801394E0(Task* arg0);
static void func_actor_400600_80139560(Task* arg0);
static void func_actor_400600_80139608(Task* arg0);
static void func_actor_400600_80139670(Task* arg0);
static void func_actor_400600_801396E4(Task* arg0);
static void func_actor_400600_80139764(Task* arg0);
static void func_actor_400600_80139878(Task* arg0);
static void func_actor_400600_801398E0(Task* arg0);
static void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_actor_400600_80139F4C(Task* arg0, s16 arg1, SVECTOR3* arg2);
static void func_actor_400600_80139FE0(Task* arg0, s16 arg1, SVECTOR3* arg2);
void        func_actor_400600_8013A0F0(Task* arg0);
static void func_actor_400600_8013A170(Task* arg0);
static void func_actor_400600_8013A26C(Task* arg0);
void        func_actor_400600_8013A3A8(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
void        func_actor_400600_8013A3B8(Task* task);
void        func_actor_400600_8013A3C0(Task* task);
static void func_actor_400600_8013A3C8(Task* arg0);
static void func_actor_400600_8013A570(Task* arg0);
static void func_actor_400600_8013A638(Task* arg0);
static void func_actor_400600_8013A6C4(Task* arg0);
static void func_actor_400600_8013A808(Task* arg0);
static void func_actor_400600_8013A820(Task* arg0);
static void func_actor_400600_8013A864(Task* arg0);
static void func_actor_400600_8013A908(Task* arg0);
static void func_actor_400600_8013A990(Task* arg0);
static void func_actor_400600_8013AA5C(Task* arg0);
static void func_actor_400600_8013AAD8(Task* arg0);
static void func_actor_400600_8013AB44(Task* arg0);
static void func_actor_400600_8013AB98(Task* arg0);
static void func_actor_400600_8013AC14(Task* arg0);
static void func_actor_400600_8013AD3C(Task* arg0);
static void func_actor_400600_8013ADA4(Task* arg0);
static void func_actor_400600_8013AE88(Task* arg0);
static void func_actor_400600_8013AF04(Task* arg0);
static void func_actor_400600_8013B018(Task* arg0);
static void func_actor_400600_8013B0FC(Task* arg0);
static void func_actor_400600_8013B150(Task* arg0);
static void func_actor_400600_8013B1DC(Task* arg0);
static void func_actor_400600_8013B2A8(Task* arg0);
static void func_actor_400600_8013B394(Task* arg0);
static void func_actor_400600_8013B410(Task* arg0);
static void func_actor_400600_8013B520(Task* arg0);
static void func_actor_400600_8013B640(void);
static void func_actor_400600_8013B6F4(Task* arg0);
static void func_actor_400600_8013B740(Task* arg0);
static void func_actor_400600_8013B830(Task* arg0);
static void func_actor_400600_8013B8AC(Task* arg0);
static void func_actor_400600_8013B984(Task* arg0);
static void func_actor_400600_8013BA00(Task* arg0);
static void func_actor_400600_8013BA6C(Task* arg0);
static void func_actor_400600_8013BAEC(Task* arg0);
static void func_actor_400600_8013BB88(Task* arg0);
static void func_actor_400600_8013BBF4(Task* arg0);
static void func_actor_400600_8013BC68(Task* arg0);
static void func_actor_400600_8013BCD8(Task* arg0);
static void func_actor_400600_8013BD54(Task* arg0);
static void func_actor_400600_8013BDF0(Task* arg0);
static void func_actor_400600_8013BE58(Task* arg0);
static void func_actor_400600_8013BF48(Task* arg0);
static void func_actor_400600_8013BFD4(Task* arg0);
static void func_actor_400600_8013C038(Task* arg0);
static void func_actor_400600_8013C074(Task* arg0);
static void func_actor_400600_8013C104(Task* arg0);
static void func_actor_400600_8013C124(Task* arg0);
static void func_actor_400600_8013C1C0(Task* arg0);
static void func_actor_400600_8013C238(Task* arg0);
static void func_actor_400600_8013C394(Task* arg0);
static void func_actor_400600_8013C410(Task* arg0);
static void func_actor_400600_8013C4AC(Task* arg0);
static void func_actor_400600_8013C518(Task* arg0);
static void func_actor_400600_8013C598(Task* arg0);
static void func_actor_400600_8013C5F8(Task* arg0);
static void func_actor_400600_8013C6B0(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out);
static s32  func_actor_400600_8013C7E8(s16 arg0, s16 arg1);
static void func_actor_400600_8013C874(Task* arg0);
static void func_actor_400600_8013C940(Task* arg0);
static void func_actor_400600_8013C9DC(Task* arg0);
static void func_actor_400600_8013CB40(Task* arg0, u8 arg1);
static void func_actor_400600_8013CB70(Task* arg0, s32 arg1);
static void func_actor_400600_8013CC04(Task* arg0, s16 arg1);

static TmdSource _gActor400600ZebraStalkerBody;
void             func_actor_400600_8013A0F0(Task*);

static TmdSource _gActor400600ZebraStalkerBurstArmLeft;
static TmdSource _gActor400600ZebraStalkerBurstArmLeft13064;
void             func_actor_400600_8013A3A8(Task*, s32, s32, s32);
void             func_actor_400600_8013A3B8(Task*);
void             func_actor_400600_8013A3C0(Task*);

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
    { ACTOR_COMMAND_MESSAGE_APPLY, stalkerZebraIvorySetMoveMode },
    { 2014, func_actor_400600_8013A3A8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_400600_80151AF8[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A3B8, { .model = &_gActor400600ZebraStalkerBurstArmLeft13064 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A3C0, { .model = &_gActor400600ZebraStalkerBurstArmLeft } },
};

TaskDesc D_actor_400600_80151B10 = { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A0F0, { .model = &_gActor400600ZebraStalkerBody } };

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

static inline void _actor400600SetCoordRotation(GfxCoord* coord, s16 angle);

#include "../../shared/stalker_zebra_ivory_inlines.inc.c"

static void func_actor_400600_8013203C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;

    work                        = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->body.coord            = &arg0->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0x96;
    work->body.pos.vz           = 0x110;
    work->body.key              = 0x30006;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT && (gGameSession->location.loc.area == 0x1F || gGameSession->location.loc.area == 0x1D)) {
        work->body.radius = 0x260;
    } else {
        work->body.radius = 0x200;
    }
    work->body.flags = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->capsule.ends[0].vz          = 0xBB8;
    work->capsule.end0Radius          = 0xA;
    work->capsule.end1Radius          = 0xA;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.contacts            = work->capsuleContacts;
    work->body.flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->capsuleBody.coord           = arg0->extra.tmd->coords;
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = -0x190;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0x30006;
    work->capsuleBody.radius          = 0;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->capsuleBody);
    worldCollisionInitContacts(work->capsuleContacts, ARRAY_SIZE(work->capsuleContacts), 0);
    work->capsuleBody.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->rightArmBody.key              = damagePackAttackKey(D_actor_400600_80144EA8, 0);
    work->rightArmBody.coord            = &arg0->extra.tmd->coords[7];
    work->rightArmBody.context.contacts = work->rightArmContacts;
    work->rightArmBody.pos.vx           = -0x200;
    work->rightArmBody.pos.vy           = 0;
    work->rightArmBody.pos.vz           = 0;
    work->rightArmBody.radius           = 0x190;
    work->rightArmBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->rightArmBody);
    worldCollisionInitContacts(work->rightArmContacts, ARRAY_SIZE(work->rightArmContacts), 0);
    work->rightArmBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.key              = damagePackAttackKey(D_actor_400600_80144EA8, 0);
    work->leftArmBody.coord            = &arg0->extra.tmd->coords[10];
    work->leftArmBody.context.contacts = work->leftArmContacts;
    work->leftArmBody.pos.vx           = 0x200;
    work->leftArmBody.pos.vy           = 0;
    work->leftArmBody.pos.vz           = 0;
    work->leftArmBody.radius           = 0x190;
    work->leftArmBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->leftArmBody);
    worldCollisionInitContacts(work->leftArmContacts, ARRAY_SIZE(work->leftArmContacts), 0);
    work->leftArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

static void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorLimbShadowScratch* s;
    s16                     angle;
    GfxCoord*               secondCoord;
    GfxCoord*               firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfY;
    GfxCoord*               coords;
    POLY_FT4*               poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = SCRATCH_STACK_RESERVE_BLOCK(ActorLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->firstPos.vx             = s->firstMatrix.t[0];
        s->firstPos.vy             = s->firstMatrix.t[1];
        s->secondPos.vx            = s->secondMatrix.t[0];
        s->secondPos.vy            = s->secondMatrix.t[1];
        s->firstPos.vz             = height;
        s->secondPos.vz            = height;
        angle                      = ratan2(s->secondPos.vx - s->firstPos.vx, s->secondPos.vy - s->firstPos.vy);
        halfX                      = (s->firstPos.vx - s->secondPos.vx) / 2;
        halfY                      = (s->firstPos.vy - s->secondPos.vy) / 2;
        s->corners[0].vx           = halfX + (s->firstPos.vx - ((s32)(rcos(angle) * width) >> 0xC));
        offset0                    = rsin(angle) * width;
        s->corners[0].vz           = height;
        s->corners[0].vy           = halfY + (s->firstPos.vy + (offset0 >> 0xC));
        s->corners[1].vx           = halfX + (s->firstPos.vx + ((s32)(rcos(angle) * width) >> 0xC));
        offset1                    = rsin(angle) * width;
        s->corners[1].vz           = height;
        s->corners[1].vy           = halfY + (s->firstPos.vy - (offset1 >> 0xC));
        s->corners[2].vx           = (s->secondPos.vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset2                    = rsin(angle) * width;
        s->corners[2].vz           = height;
        s->corners[2].vy           = (s->secondPos.vy + (offset2 >> 0xC)) - halfY;
        s->corners[3].vx           = (s->secondPos.vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset3                    = rsin(angle) * width;
        s->corners[3].vz           = height;
        s->corners[3].vy           = (s->secondPos.vy - (offset3 >> 0xC)) - halfY;
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corners[0], &s->corners[1], &s->corners[2], &s->corners[3], &s->screenCorners[0], &s->screenCorners[1],
                                 &s->screenCorners[2], &s->screenCorners[3], &s->depthCue, &s->flag);
        if (s->flag >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screenCorners[0];
            GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screenCorners[1];
            GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screenCorners[2];
            GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screenCorners[3];
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorLimbShadowScratch);
    }
}

static void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_400600_80132294(arg0, 3, 9, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 9, 0xA, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xA, 0xB, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 3, 6, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 6, 7, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 7, 8, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 5, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xC, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xC, 0xD, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xD, 0xE, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xF, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xF, 0x10, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0x10, 0x11, 0x80, arg1, arg2);
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
        Gp_SpawnPadLerp(0xA, 0xC0, 0x80);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531A0009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        func_dryfield_night_junk_yard_8017D9B8(1);
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x30);
        coords->coord.t[1] = -0x508;
        work->state++;
    }
}

static void func_actor_400600_801329EC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    u32                           sound;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->stateFrames == 0) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x531A000A;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    work->stateFrames++;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40060004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
        work3                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state              = 2;
        work3->subState           = 0;
    }
}

static void func_actor_400600_80132B3C(Task* arg0)
{
    GfxCoord*                     coords;
    _Actor400600ZebraStalkerWork* work;
    s32                           sound;
    s32                           pan;

    coords              = arg0->extra.tmd->coords;
    work                = (_Actor400600ZebraStalkerWork*)arg0->work;
    coords->coord.t[0] += (0x1C54 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0xED5 - coords->coord.t[2]) >> 2;
    work->moveAccel    += 2;
    work->moveSpeed    += work->moveAccel;
    coords->coord.t[1] += work->moveSpeed;
    if (coords->coord.t[1] >= 0) {
        Gp_SpawnPadLerp(0x10, 0x80, 0x40);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531A000A;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->state++;
    }
}

static void func_actor_400600_80132C70(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    _Actor400600ZebraStalkerWork* work4;
    GfxCoord*                     coords;
    u8                            mode;
    s16                           yaw;

    work   = (_Actor400600ZebraStalkerWork*)arg0->work;
    mode   = work->roomCommand;
    coords = arg0->extra.tmd->coords;
    if (mode == 1) {
        coords->coord.t[0] = 0x36B0;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0x7D0;
        work->pitch        = 0;
        work->yaw          = 0x400;
        work->roll         = 0x400;
        work->stateFrames  = 0;
        stalkerZebraIvoryPlayClip(arg0, 2, 0x10);
        work->shadowWallZ = -0x7D0;
        work->state++;
    } else if (mode == 2) {
        coords->coord.t[0] = 0x4A38;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0xFA0;
        yaw                = -0x400;
        work->yaw          = yaw;
        work->pitch        = 0;
        work->roll         = 0x400;
        work->stateFrames  = 0;
        stalkerZebraIvoryPlayClip(arg0, 2, 0x10);
        work->shadowWallZ = -0xFA0;
        work2             = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state      = 5;
        work2->subState   = 0;
    } else if (mode == 3) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coords->coord.t[0]        = 0x2AF8;
        coords->coord.t[2]        = -0x3E8;
        coords->coord.t[1]        = 0;
        work->pitch               = 0;
        work->yaw                 = 0xC00;
        work3                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work3->state              = 0;
        work3->subState           = 0;
    } else if (mode == 4) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coords->coord.t[0]        = 0x3A98;
        coords->coord.t[2]        = -0xBB8;
        coords->coord.t[1]        = 0;
        work->pitch               = 0;
        work->yaw                 = 0x400;
        work4                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work4->state              = 0;
        work4->subState           = 0;
    }
}

static void func_actor_400600_80132E10(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    u32                           sound;
    s32                           pan;

    work->stateFrames++;
    func_actor_400600_801361AC(arg0);
    if (work->stateFrames == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if (work->stateFrames >= 0x27) {
        work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 4);
    }
    if (work->stateFrames == 0x5A) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A0004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = 0;
        work->moveAccel   = -0xA;
        work->moveSpeed   = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->state++;
    }
}

static void func_actor_400600_80132F3C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coords;
    s32                           sound;
    s32                           pan;

    work   = (_Actor400600ZebraStalkerWork*)arg0->work;
    coords = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= 0x11) {
        work->shadowShade  += -work->shadowShade >> 3;
        work->moveAccel    += 2;
        work->moveSpeed    += work->moveAccel;
        coords->coord.t[1] += work->moveSpeed;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x4588 - coords->coord.t[0]) >> 2;
        work->yaw          += (0xC00 - work->yaw) >> 2;
        work->roll         += -work->roll >> 3;
        if (work->stateFrames == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if (work->stateFrames == 0x23 || work->stateFrames == 0x25) {
            Gp_SpawnEff(gRoomEffectWaterRippleId, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0003;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->state++;
        }
    }
}

static void func_actor_400600_80133118(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coords;
    s32                           sound;
    s32                           pan;

    work   = (_Actor400600ZebraStalkerWork*)arg0->work;
    coords = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= 0x11) {
        work->shadowShade  += -work->shadowShade >> 3;
        work->moveAccel    += 2;
        work->moveSpeed    += work->moveAccel;
        coords->coord.t[1] += work->moveSpeed;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x3A98 - coords->coord.t[0]) >> 2;
        work->yaw          += (0x400 - work->yaw) >> 2;
        work->roll         += -work->roll >> 3;
        if (work->stateFrames == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if (work->stateFrames == 0x23 || work->stateFrames == 0x25) {
            Gp_SpawnEff(gRoomEffectWaterRippleId, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0003;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->state++;
        }
    }
}

static void func_actor_400600_801332F4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    GfxCoord*                     coord;
    s32                           mode;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    mode  = work->roomCommand;
    coord = model->coords;
    if (mode == 1) {
        Gp_SpawnPadLerp(0x14, 0xFF, 0x80);
        coord->coord.t[0] = 0xCE4;
        coord->coord.t[1] = -0xBB8;
        coord->coord.t[2] = 0;
        work->pitch       = 0;
        work->yaw         = 0xC00;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->stateFrames = 0;
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->state++;
    } else if (mode == 2) {
        work->body.flags                        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]                        = -0x6A4;
        coord->coord.t[2]                        = -0x514;
        coord->coord.t[1]                        = 0;
        work->yaw                                = 0x400;
        work->shadowShade                        = 0xFF;
        work->pitch                              = 0;
        work->roll                               = 0;
        gSceneCombatState.zebraStalkerGroupPhase = mode;
        work2                                    = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state                              = 1;
        work2->state                             = 0;
        work2->subState                          = 0;
    }
}

static const TaskFuncTable12 D_actor_400600_80131E24 = { {
    func_actor_400600_8013A3C8,
    stalkerZebraIvoryResumeClip,
    stalkerZebraIvoryAnimateUntilDone,
    func_actor_400600_8013A570,
    func_actor_400600_8013A638,
    func_actor_400600_8013A6C4,
    func_actor_400600_8013A808,
    func_actor_400600_8013A820,
    func_actor_400600_8013A864,
    func_actor_400600_8013A908,
    func_actor_400600_8013A990,
    func_actor_400600_8013AA5C,
} };

static const TaskFuncTable6 D_actor_400600_80131E54 = { {
    func_actor_400600_8013AB98,
    func_actor_400600_8013AC14,
    func_actor_400600_801328A8,
    func_actor_400600_8013AD3C,
    func_actor_400600_8013ADA4,
    func_actor_400600_801329EC,
} };

static const TaskFuncTable4 D_actor_400600_80131E6C = { {
    func_actor_400600_8013AE88,
    func_actor_400600_8013AF04,
    func_actor_400600_80132B3C,
    func_actor_400600_8013B018,
} };

static const TaskFuncTable8 D_actor_400600_80131E7C = { {
    func_actor_400600_8013B0FC,
    func_actor_400600_80132C70,
    func_actor_400600_80132E10,
    func_actor_400600_80132F3C,
    func_actor_400600_8013B150,
    func_actor_400600_8013B1DC,
    func_actor_400600_80133118,
    func_actor_400600_8013B2A8,
} };

static const TaskFuncTable4 D_actor_400600_80131E9C = { {
    func_actor_400600_8013B394,
    func_actor_400600_801332F4,
    func_actor_400600_8013B410,
    func_actor_400600_8013B520,
} };

static const TaskFuncTable9 D_actor_400600_80131EAC = { {
    func_actor_400600_80133434,
    func_actor_400600_801337A8,
    func_actor_400600_8013A170,
    func_actor_400600_8013A26C,
    func_actor_400600_80138C34,
    func_actor_400600_80138D78,
    func_actor_400600_80138EA0,
    func_actor_400600_80138FD4,
    func_actor_400600_80137EF0,
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
    func_actor_400600_8013B640();
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

    stalkerZebraIvoryTickAnimInline(arg0);

    coord->parent = &gGfxViewCoord;
    work->yaw     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    func_actor_400600_8013203C(arg0);
    work->body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    func_actor_400600_801356E0(arg0);
    (sceneAcquireBattleRef)(0);
    func_actor_400600_80138A24(arg0, 1);
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
    func_actor_400600_801390FC,
    func_actor_400600_80133B88,
    func_actor_400600_80139110,
    func_actor_400600_80139218,
    func_actor_400600_80139280,
    func_actor_400600_801392E8,
    func_actor_400600_8013935C,
    func_actor_400600_801393D0,
    func_actor_400600_80139444,
    func_actor_400600_801394E0,
    func_actor_400600_80139560,
    func_actor_400600_80139608,
    func_actor_400600_80139670,
    func_actor_400600_801396E4,
    func_actor_400600_80139764,
    stalkerZebraIvoryRunSubStates,
    func_actor_400600_80139878,
    func_actor_400600_801398E0,
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
            func_actor_400600_801387DC(arg0, -1);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            fns.funcs[work->state](arg0);
            func_actor_400600_80138AB8(arg0);
            func_actor_400600_80137840(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryTickAnimInline(arg0);
            work->previousAnimationFlags = work->rig.slots[1].status.fields.flags;
            stalkerZebraIvoryApplyRotationInline(arg0);
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
            actorUpdateModelColor(arg0);
            func_actor_400600_80138224(arg0, work->shadowHeight, work->shadowShade);
            if (work->cloaked == 0) {
                model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                func_actor_400600_801387DC(arg0, -1);
            }
            break;
    }
}

static void func_actor_400600_80133B88(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    s32                           sound;
    s32                           pan;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->playerDistance < 0xBB8 || gSceneCombatState.zebraStalkerDeathAlert != 0) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        func_actor_400600_80138B5C(arg0, 0);
        sceneEngageBattle(1);
        rnd                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = rnd;
        work->hideCooldown = ((rnd >> 0x10) & 0x3F) + 0x1E;
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state       = 2;
        work2->subState    = 0;
        return;
    }
    if ((s16)func_actor_400600_80136FA8(arg0) != 0) {
        sceneEngageBattle(1);
    }
}

static void func_actor_400600_80133CB0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           id;
    u32                           sound;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->leftArmBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == 0x1C) {
        work->leftArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0x2D);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->targetBearing - 0x200) > 0xC00 && (u16)(work->bearingFromPlayer - 0x200) > 0xC00) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_80133E38(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           id;
    u32                           sound;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->rightArmBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == 0x1C) {
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0x2D);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->targetBearing - 0x200) > 0xC00 && (u16)(work->bearingFromPlayer - 0x200) > 0xC00) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_80133FC0(Task* arg0)
{
    AnimationPlayRequest          msg;
    GameActorButtonPressHold      query;
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           base;
    s32                           sound;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (((GameActor*)gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work)->mode == GAME_ACTOR_MODE_SCRIPTED || (stalkerZebraIvoryWallDistance(arg0) << 0x10) != 0 || work->playerDistance >= 0x7D0 || (u32)(work->targetBearing - 0x200) < 0xC01U) {
        stalkerZebraIvoryDropCapsuleGrid(arg0);
        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state    = 2;
        work2->subState = 0;
        func_actor_400600_80135998(arg0, work->walkStep);
        return;
    }
    query.pressCount = 8;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &query, 0) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        if (work->onCeiling == 0) {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state    = 0xD;
        work2->subState = 0;
        return;
    }
    work->shadowHeight = work->floorY;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work->onCeiling          = 0;
    Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
    work->holding            = 1;
    work->leapY              = work->floorY;
    msg.source.sets          = D_actor_400600_80151A48;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    msg.animationId          = 1;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
    work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = ANIMATION_RATE_ONE;
    work2->animClip    = 5;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames  = 0;
    work->holdFrames   = 0;
    base               = 0x40060004;
    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->holdLoops = 0;
    work->subState++;
}

static void func_actor_400600_80134218(Task* arg0)
{
    AnimationPlayRequest          msg;
    SVECTOR                       vec;
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    GfxCoord*                     root;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s32                           sound2;
    s32                           pan2;
    s32                           y;
    s32                           ty;

    work               = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    player             = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll        += -work->roll >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    y                  = coord->coord.t[1];
    ty                 = y + 900;
    coord->coord.t[1]  = y + ((player->coord.t[1] - ty) >> 2);
    work->stateFrames++;
    if (++work->holdFrames == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->playerDied == 1 || work->holdKilledPlayer == 1 || enemy->hp <= 0 || work->holdLoops >= 3) {
        work->playerDied = 0;
        if (work->holdKilledPlayer == 0) {
            msg.source.sets          = D_actor_400600_80151A48;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 8;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            msg.animationId          = 2;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        }
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animBlend   = 8;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 6;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->stateFrames  = 0;
        work->body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->subState++;
        return;
    }
    if (work->stateFrames == 0xD || work->stateFrames == 0x1A) {
        root = &gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40060009;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 1), 0) != 0) {
            work->holdKilledPlayer = 1;
        }
        vec.vx = 0;
        vec.vy = -200;
        vec.vz = 0;
        Gp_SpawnEff(EFFECT_HIT_SPLATTER_SPRAY, root, 0x10100, &vec);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->stateFrames = 0;
        work->holdLoops++;
    }
}

static void func_actor_400600_80134570(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    GfxCoord*                     root;
    SVECTOR                       vec;
    s32                           i;
    s32                           y;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s16                           vy;

    work        = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord       = arg0->extra.tmd->coords;
    player      = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll += -work->roll >> 2;
    work->stateFrames++;
    if (work->stateFrames >= 8) {
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->moveAccel  += 2;
        work->moveSpeed  += work->moveAccel;
        y                 = coord->coord.t[1] + work->moveSpeed;
        coord->coord.t[1] = y;
        if (y >= work->leapY) {
            coord->coord.t[1]    = work->leapY;
            player->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(player);
            if (work->inWater != 0) {
                vy   = -0x1A4;
                root = arg0->extra.tmd->coords;
                Gp_SpawnEff(gRoomEffectWaterRippleId, root, 0x40, NULL);
                for (i = 0; i < 16; i++) {
                    vec.vx = (u32)rsin(i << 8) >> 3;
                    vec.vy = vy;
                    vec.vz = (u32)rcos(i << 8) >> 3;
                    Gp_SpawnEff(gRoomEffectWaterSprayId, root, 0x01202148, &vec);
                }
            }
            id = 0x40060003;
            if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->animBlend   = 2;
            work2->animClip    = 0x19;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            work->subState++;
        }
    }
}

static void func_actor_400600_8013479C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s16                           v;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    v    = stalkerZebraIvoryWallDistance(arg0);
    if (v != 0) {
        if ((u16)(v - 0x4E9) >= 0x6D0U) {
            stalkerZebraIvoryDropCapsuleGrid(arg0);
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work->leapX = ((rsin(work->yaw + 0x800) * (v - 0x100)) >> 12) / 20;
        work->leapZ = ((rcos(work->yaw + 0x800) * (v - 0x100)) >> 12) / 20;
    } else {
        work->leapX = ((rsin(work->yaw + 0x800) * 3000) >> 12) / 20;
        work->leapZ = ((rcos(work->yaw + 0x800) * 3000) >> 12) / 20;
    }
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = ANIMATION_RATE_ONE;
    work2->animClip    = 0x15;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->moveAccel    = -0x2A;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->subState++;
}

static void func_actor_400600_80134970(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;
    GfxCoord*                     root;
    SVECTOR                       vec;
    s32                           i;
    s32                           y;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s16                           vy;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if (work->stateFrames == 0x11) {
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding       = 1;
    }
    coord->coord.t[0] += work->leapX;
    coord->coord.t[2] += work->leapZ;
    work->moveAccel   += 6;
    work->moveSpeed   += work->moveAccel;
    y                  = coord->coord.t[1] + work->moveSpeed;
    coord->coord.t[1]  = y;
    if (y >= work->floorY) {
        coord->coord.t[1] = work->floorY;
        vy                = -0x1A4;
        if (work->inWater != 0) {
            root = arg0->extra.tmd->coords;
            Gp_SpawnEff(gRoomEffectWaterRippleId, root, 0x40, NULL);
            for (i = 0; i < 16; i++) {
                vec.vx = (u32)rsin(i << 8) >> 3;
                vec.vy = vy;
                vec.vz = (u32)rcos(i << 8) >> 3;
                Gp_SpawnEff(gRoomEffectWaterSprayId, root, 0x01202148, &vec);
            }
        }
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animClip    = 0x19;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->holding      = 0;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_right_itself.inc.c"

static void func_actor_400600_80134E28(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s32                           y;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if (work->stateFrames == 0x11) {
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding       = 1;
        work->shadowHeight  = work->leapY;
        work->body.flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    /* A separate statement: written inline, fold turns `a - (y + 400)` into
     * `(a - 400) - y`. */
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += (work->leapY - y) >> 3;
    work->pitch       += (0x800 - work->pitch) >> 3;
    if (work->leapY >= coord->coord.t[1]) {
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[1] = work->leapY;
        work->pitch       = 0;
        work->roll        = 0x800;
        work->yaw        += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 0x19;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->holding      = 0;
        work->onCeiling    = 1;
        work->subState++;
    }
}

static void func_actor_400600_801350F4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 8) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 3, &work->anchorPos);
        return;
    }
    work->anchorPos.vx += (work->leapX - work->anchorPos.vx) >> 2;
    work->anchorPos.vz += (work->leapZ - work->anchorPos.vz) >> 2;
    stalkerZebraIvoryPinPartXZ(arg0, 3, &work->anchorPos);
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if ((work->pitch & 0xFFF) != 0x800) {
        work->pitch -= 0x80;
    }
    if (work->floorY < coord->coord.t[1]) {
        work->onCeiling   = 0;
        coord->coord.t[0] = work->leapX;
        coord->coord.t[1] = work->floorY;
        coord->coord.t[2] = work->leapZ;
        work->pitch       = 0;
        work->roll        = 0;
        work->yaw        += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 0x19;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        stalkerZebraIvoryTickAnimInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->stateFrames = 0;
        work->subState++;
    }
}

static void func_actor_400600_80135450(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    u32                           sound;
    s32                           id;
    s32                           pan;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->stateFrames == 0) {
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->holding = 0;
        work->stateFrames++;
    }
    if ((stalkerZebraIvoryTakePending(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        rnd                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = rnd;
        work->ceilingCooldown = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                 = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state          = 2;
        work2->subState       = 0;
    }
}

static void func_actor_400600_80135578(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    u32                           sound;
    s32                           id;
    s32                           pan;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->stateFrames == 0) {
        work->holding = 0;
        id            = 0x40060006;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0006;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames++;
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->pendingAction != STALKER_ZEBRA_IVORY_PENDING_STATUS) {
            work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->animBlend   = 2;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animClip    = 0x14;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            work->subState++;
            return;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        rnd                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = rnd;
        work->countdown     = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3               = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state        = 5;
        work3->subState     = 0;
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
    GfxMatrix*                    pm;
    GfxMatrix*                    pm2;
    GfxMatrix                     m;

    root              = arg0->extra.tmd->coords;
    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    parent            = &root[7];
    parent2           = &root[10];
    task              = taskSpawnFromTable(D_actor_400600_80151AF8, 0, 0, 0);
    work->armTasks[0] = task;
    if (task != NULL) {
        obj                      = task->extra.tmd;
        coord                    = obj->coords;
        obj->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        coord->coord.t[0]        = 0x200;
        coord->parent            = parent2;
        coord->coord.t[1]        = 0;
        coord->coord.t[2]        = 0;
        pm                       = &m;
        pm->rotationWords.m00M01 = ONE;
        pm->rotationWords.m02M10 = 0;
        pm->rotationWords.m11M12 = ONE;
        pm->rotationWords.m20M21 = 0;
        pm->rotationWords.m22    = ONE;
        RotMatrixY(-0x180, &pm->mat);
        mdst                   = &coord->coord;
        mdst->m[0][0]          = pm->mat.m[0][0];
        mdst->m[0][1]          = pm->mat.m[0][1];
        mdst->m[0][2]          = pm->mat.m[0][2];
        mdst->m[1][0]          = pm->mat.m[1][0];
        mdst->m[1][1]          = pm->mat.m[1][1];
        mdst->m[1][2]          = pm->mat.m[1][2];
        mdst->m[2][0]          = pm->mat.m[2][0];
        mdst->m[2][1]          = pm->mat.m[2][1];
        mdst->m[2][2]          = pm->mat.m[2][2];
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
        pm2                       = &m;
        pm2->rotationWords.m00M01 = ONE;
        pm2->rotationWords.m02M10 = 0;
        pm2->rotationWords.m11M12 = ONE;
        pm2->rotationWords.m20M21 = 0;
        pm2->rotationWords.m22    = ONE;
        RotMatrixY(0x180, &pm2->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm2->mat.m[0][0];
        mdst->m[0][1] = pm2->mat.m[0][1];
        mdst->m[0][2] = pm2->mat.m[0][2];
        mdst->m[1][0] = pm2->mat.m[1][0];
        mdst->m[1][1] = pm2->mat.m[1][1];
        mdst->m[1][2] = pm2->mat.m[1][2];
        mdst->m[2][0] = pm2->mat.m[2][0];
        mdst->m[2][1] = pm2->mat.m[2][1];
        mdst->m[2][2] = pm2->mat.m[2][2];
        obj->lightMtx = &work->lightMtx;
        obj->colorMtx = &work->colorMtx;
    }
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `stalkerZebraIvoryStepClip4`, plus the dust ring each window spawns when
/// `inWater` is set.
static void func_actor_400600_80135998(Task* arg0, s16 arg1)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;
    /* The first window starts at frame 0, written as the literal both of its
     * tests fold against; the wider first temp is what keeps each zero arm a
     * fresh constant instead of a copy of the previous one. */
    u32     tmp0;
    u8      tmp1;
    u8      tmp2;
    u8      end0;
    u8      start1;
    u8      end1;
    s32     id;
    u32     sound;
    u32     voice;
    s32     pan;
    SVECTOR vec;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 2;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        stalkerZebraIvoryTickAnimInline(arg0);
    }
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013892C(arg0) << 0x10) != 0) {
        work->animFrame = 0;
        work->animStep  = arg1;
    }
    if (work->animFrame == 0) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 0xB, &work->anchorPos);
        id = 0x40060001;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (work->inWater != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, &vec);
        }
    }
    if (work->animFrame == start1) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 8, &work->anchorPos);
        id = 0x40060002;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (work->inWater != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, &vec);
        }
    }
    if (work->animFrame >= 0 && work->animFrame <= end0) {
        stalkerZebraIvoryPinPartXZ(arg0, 0xB, &work->anchorPos);
        work->nextAnchorPart = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        stalkerZebraIvoryPinPartXZ(arg0, 8, &work->anchorPos);
        work->nextAnchorPart = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/stalker_zebra_ivory_step_clip4.inc.c"

/// Drives the two sound/tracking windows of the current animation. The window
/// bounds are frame counts derived from the playback rate (`animStep`), each
/// read back through `arg0->work` rather than the cached `work` pointer.
static void func_actor_400600_801361AC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;
    /* The bounds are computed into their own temporaries first; a `u8` temp is
     * what keeps the zero arm of each test out of the surrounding block. */
    u8 tmp0;
    u8 tmp1;
    u8 tmp2;
    u8 end0;
    u8 start1;
    u8 end1;
    /* The first window starts at frame 0: `start0` is a `u8` bound like the
     * other three, assigned first as in `stalkerZebraIvoryStepClip4`. Its two
     * tests then compare against a register the image zeroes at the first of
     * them; a literal 0 folds. */
    u8  start0;
    u32 sound;
    s32 pan;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 2;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        stalkerZebraIvoryTickAnimInline(arg0);
    }
    start0 = 0;
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((_Actor400600ZebraStalkerWork*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((_Actor400600ZebraStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->animFrame = 0;
    }
    if (work->animFrame == start0) {
        func_actor_400600_80139F4C(arg0, 0xB, &work->anchorPos);
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000A;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame == start1) {
        func_actor_400600_80139F4C(arg0, 8, &work->anchorPos);
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000B;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame >= start0 && work->animFrame <= end0) {
        func_actor_400600_80139FE0(arg0, 0xB, &work->anchorPos);
        work->nextAnchorPart = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        func_actor_400600_80139FE0(arg0, 8, &work->anchorPos);
        work->nextAnchorPart = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400600_80136558(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    TmdObject*                    model = arg0->extra.tmd;
    Enemy*                        enemy = (Enemy*)arg0->spawnArg2.pointer;
    s16                           count;

    if (work->cloaked == 0 && work->cloakFading == 1) {
        work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 5);
        work->cloakFadeFrames++;
        if (work->cloakFadeFrames >= 0x20) {
            model->flags &= ~TMD_OBJECT_SEMI_TRANS;
            func_actor_400600_801387DC(arg0, -1);
            work->cloakFadeFrames = 0;
            work->cloakFading     = 0;
        }
    } else if (work->cloaked == 1 && work->cloakFading == 1) {
        work->shadowShade     = (u16)work->shadowShade + (-work->shadowShade >> 3);
        count                 = work->cloakFadeFrames + 1;
        work->cloakFadeFrames = count;
        if (count >= 0x12) {
            if (work->markedFrames != 0) {
                enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            } else {
                enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
            }
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_400600_801387DC(arg0, -1);
            work->cloakFadeFrames = 0;
            work->cloakFading     = 0;
            work->shadowShade     = 0;
        }
    }
}

static void func_actor_400600_80136670(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    GameActor*                    actor;
    Task*                         slot;
    SVECTOR                       v;
    s16                           a;
    s16                           b;

    work                 = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord                = arg0->extra.tmd->coords;
    slot                 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    actor  = slot->work;
    if (work->routesByZone != 0) {
        a = func_actor_400600_8013886C(arg0);
        b = func_actor_400600_8013886C(slot);
        if (a == 2 && (b == 1 || b == 6)) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else if (a == 3 && ((b >= 1 && b <= 2) || b == 6)) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = -0x1194;
        } else if (a == 4 && ((b >= 1 && b <= 3) || b == 6)) {
            work->targetPos.vx = -0x125C;
            work->targetPos.vy = 0;
            work->targetPos.vz = -0x1194;
        } else if (a == 5 && ((b >= 1 && b <= 4) || b == 6)) {
            work->targetPos.vx = -0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0x1194;
        } else if (b == 1 && a == 6) {
            work->targetPos.vx = 0;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else if (b != 1 && a == 1) {
            work->targetPos.vx = 0x1194;
            work->targetPos.vy = 0;
            work->targetPos.vz = 0;
        } else {
            work->targetPos.vx = player->coord.t[0];
            work->targetPos.vy = player->coord.t[1];
            work->targetPos.vz = player->coord.t[2];
        }
    } else {
        work->targetPos.vx = player->coord.t[0];
        work->targetPos.vy = player->coord.t[1];
        work->targetPos.vz = player->coord.t[2];
    }
    v.vx                 = work->targetPos.vx - coord->coord.t[0];
    v.vy                 = work->targetPos.vy - coord->coord.t[1];
    v.vz                 = work->targetPos.vz - coord->coord.t[2];
    work->playerDistance = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->targetBearing     = (ratan2(v.vx, v.vz) - work->yaw) & 0xFFF;
    work->bearingFromPlayer = (ratan2(-v.vx, -v.vz) - actor->rotation.vy) & 0xFFF;
}

static const TaskFuncTable3 D_actor_400600_80131F34 = { {
    func_actor_400600_8013BA6C,
    func_actor_400600_8013BAEC,
    func_actor_400600_8013BB88,
} };

static const TaskFuncTable8 D_actor_400600_80131F40 = { {
    func_actor_400600_8013BCD8,
    func_actor_400600_8013BD54,
    func_actor_400600_8013BDF0,
    func_actor_400600_8013BE58,
    func_actor_400600_80133FC0,
    func_actor_400600_80134218,
    func_actor_400600_80134570,
    stalkerZebraIvoryReleaseHold,
} };

static const TaskFuncTable4 D_actor_400600_80131F60 = { {
    func_actor_400600_8013BF48,
    func_actor_400600_8013479C,
    func_actor_400600_80134970,
    stalkerZebraIvoryWaitClip,
} };

static const TaskFuncTable3 D_actor_400600_80131F70 = { {
    func_actor_400600_8013C038,
    func_actor_400600_80134E28,
    func_actor_400600_8013C074,
} };

static const TaskFuncTable4 D_actor_400600_80131F7C = { {
    func_actor_400600_8013C104,
    func_actor_400600_8013C124,
    func_actor_400600_801350F4,
    func_actor_400600_80135450,
} };

static const TaskFuncTable4 D_actor_400600_80131F8C = { {
    func_actor_400600_8013C1C0,
    func_actor_400600_8013C238,
    func_actor_400600_80135578,
    stalkerZebraIvoryWaitClipThenRest,
} };

static const TaskFuncTable3 gStalkerZebraIvorySubStates = { {
    func_actor_400600_8013C394,
    func_actor_400600_8013C410,
    func_actor_400600_8013C4AC,
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
                func_actor_400600_8013C6B0(&pos, &work->bodyContacts[i], &push);
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
                    dmg                = Gp_ComputeDamage(work->bodyContacts[i].key.value, work->playerDistance, 0, 0);
                    amount             = dmg;
                    work->hitCooldown  = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
                    if (damageRollCriticalHit(enemy, work->bodyContacts[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
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
                        func_800FDB18(damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value),
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
                    func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
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
        work->anchorPos.vx += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        work->anchorPos.vz += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->coord.t[0]  += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        coord->coord.t[2]  += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->markedFrames != 0) {
        work->markedFrames--;
        if ((work->markedFrames & 7) == 1) {
            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, eff, 0x10200, NULL);
        }
    }
}

static s32 func_actor_400600_80136FA8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->pendingArmed != 1) {
        return 0;
    }
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 3;
                work2->subState = 0;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 4;
                work2->subState = 0;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 5;
                work2->subState = 0;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 4;
                work2->subState = 0;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 0xF;
                work2->subState = 0;
                break;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
    } else {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                work2               = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                work2               = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state    = 0xE;
                work2->subState = 0;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                work2               = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                work2               = (_Actor400600ZebraStalkerWork*)arg0->work;
                work2->state        = 0xE;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                break;
        }
    }
    work->ceilingProbePending = 0;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    return 1;
}

#include "../../shared/stalker_zebra_ivory_take_pending.inc.c"

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
    eff           = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[4], 0x200, NULL);
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
    eff2          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[2], 0x200, NULL);
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
    eff3          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[16], 0x200, NULL);
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
    eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[10], 0x200, NULL);
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
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

/* `n` is one variable carrying first `out.vz` and then the speed: the reuse is
 * an anti-dependence that keeps the `field_4` store ahead of `li 10` in sched1,
 * so the tail matches case 2's and jump2 cross-jumps them. Each branch keeps
 * its own matrix pointer so local-alloc puts it in `$s0` ahead of `work`. */
static void func_actor_400600_80137498(Task* arg0, s16 arg1)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    SVECTOR                       v;
    SVECTOR                       out;
    GfxMatrix                     rot;
    s16                           n;

    work->distanceMode = arg1;
    switch (arg1) {
        case 0:
            if (work->onCeiling == 0) {
                GfxMatrix* m = &rot;

                v.vx                     = work->targetPos.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy                     = work->targetPos.vy - arg0->extra.tmd->coords->coord.t[1] - 0x384;
                v.vz                     = work->targetPos.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                m->rotationWords.m11M12  = ONE;
                rot.rotationWords.m20M21 = 0;
                m->rotationWords.m22     = ONE;
                rot.mat.t[0]             = 0;
                rot.mat.t[1]             = 0;
                rot.mat.t[2]             = 0;
                RotMatrixY(-work->yaw, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                GfxMatrix* m = &rot;

                v.vx                     = work->targetPos.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy                     = work->targetPos.vy - arg0->extra.tmd->coords->coord.t[1] - 0x640;
                v.vz                     = work->targetPos.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                m->rotationWords.m11M12  = ONE;
                rot.rotationWords.m20M21 = 0;
                m->rotationWords.m22     = ONE;
                rot.mat.t[0]             = 0;
                rot.mat.t[1]             = 0;
                rot.mat.t[2]             = 0;
                RotMatrixY(-work->yaw, &m->mat);
                RotMatrixZ(-work->roll, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->capsule.ends[0].vx = out.vx;
            work->capsule.ends[0].vy = out.vy;
            n                        = out.vz;
            work->capsule.ends[0].vz = n;
            n                        = 0xA;
            work->capsule.end0Radius = n;
            work->capsule.end1Radius = n;
            break;
        case 1:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = 0x190;
            work->capsule.ends[0].vz = -0xBB8;
            work->capsule.end0Radius = 0x50;
            work->capsule.end1Radius = 0x50;
            break;
        case 2:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = -0xBB8;
            work->capsule.ends[0].vz = 0;
            work->capsule.end0Radius = 0xA;
            work->capsule.end1Radius = 0xA;
            break;
    }
    work->capsule.ends[1].vx = 0;
    work->capsule.ends[1].vy = 0x64;
    work->capsule.ends[1].vz = 0;
    worldCollisionClearContacts(work->capsuleContacts);
    work->capsuleBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#include "../../shared/stalker_zebra_ivory_wall_distance.inc.c"

/// Replaces the rotation of `coord` with the one `RotMatrixY` applies to an
/// identity matrix for `angle`.
static inline void _actor400600SetCoordRotation(GfxCoord* coord, s16 angle)
{
    GfxMatrix  rot;
    GfxMatrix* m;
    MATRIX*    dst;

    rot.rotationWords.m00M01 = ONE;
    rot.rotationWords.m02M10 = 0;
    m                        = &rot;
    m->rotationWords.m11M12  = ONE;
    rot.rotationWords.m20M21 = 0;
    m->rotationWords.m22     = ONE;
    RotMatrixY(angle, &m->mat);
    dst          = &coord->coord;
    dst->m[0][0] = rot.mat.m[0][0];
    dst->m[0][1] = rot.mat.m[0][1];
    dst->m[0][2] = rot.mat.m[0][2];
    dst->m[1][0] = rot.mat.m[1][0];
    dst->m[1][1] = rot.mat.m[1][1];
    dst->m[1][2] = rot.mat.m[1][2];
    dst->m[2][0] = rot.mat.m[2][0];
    dst->m[2][1] = rot.mat.m[2][1];
    dst->m[2][2] = rot.mat.m[2][2];
}

/// Sets the root rotation of the child task held in work field `child`, if it
/// has been spawned.
#define _ACTOR400600_ROTATE_CHILD(task, child, angle)                         \
    do {                                                                      \
        Task* _child = ((_Actor400600ZebraStalkerWork*)(task)->work)->child;  \
                                                                              \
        if (_child != NULL) {                                                 \
            _actor400600SetCoordRotation(_child->extra.tmd->coords, (angle)); \
        }                                                                     \
    } while (0)

static void func_actor_400600_80137840(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    s32                           angle;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->rightArmOut != 0) {
        work->armSwingAngles[1] += (0x380 - work->armSwingAngles[1]) >> 2;
        _ACTOR400600_ROTATE_CHILD(arg0, armTasks[1], work->armSwingAngles[1]);
    } else {
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->armSwingAngles[1]  += -work->armSwingAngles[1] >> 3;
        angle                     = work->armSwingAngles[1];
        _ACTOR400600_ROTATE_CHILD(arg0, armTasks[1], angle);
    }
    if (work->leftArmOut != 0) {
        work->armSwingAngles[0] += (0x380 - work->armSwingAngles[0]) >> 2;
        _ACTOR400600_ROTATE_CHILD(arg0, armTasks[0], -work->armSwingAngles[0]);
    } else {
        work->leftArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->armSwingAngles[0] += -work->armSwingAngles[0] >> 3;
        _ACTOR400600_ROTATE_CHILD(arg0, armTasks[0], -work->armSwingAngles[0]);
    }
    if (work->state != 6 && work->state != 7) {
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

static s32 func_actor_400600_80137AF0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    TmdObject*                    model2;
    Enemy*                        enemy;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->hideCooldown > 0) {
        work->hideCooldown--;
        return 0;
    }
    if ((u32)(work->targetBearing - 0x400) >= 0x801U && (u32)(work->bearingFromPlayer - 0x300) >= 0xA01U) {
        if (work->playerDistance < 0xBB8) {
            model = arg0->extra.tmd;
            if (work->cloaked != 1) {
                work->cloaked         = 1;
                work->cloakFading     = 1;
                work->cloakFadeFrames = 0;
                model->flags         |= TMD_OBJECT_SEMI_TRANS;
                worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
                func_actor_400600_801387DC(arg0, 2);
            }
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 0x11;
            work2->subState = 0;
            return 1;
        }
    } else {
        work = (_Actor400600ZebraStalkerWork*)arg0->work;
    }
    model2 = arg0->extra.tmd;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    if (work->cloaked != 0) {
        work->cloaked         = 0;
        work->cloakFading     = 1;
        work->cloakFadeFrames = 0;
        model2->flags         = (model2->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
        func_actor_400600_801387DC(arg0, 0);
    }
    return 0;
}

static s32 func_actor_400600_80137C34(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    GfxCoord*                     coord;
    u32                           rnd1;
    u32                           rnd2;
    u32                           rnd;
    s32                           dist;
    s16                           y;

    work            = (_Actor400600ZebraStalkerWork*)arg0->work;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord           = arg0->extra.tmd->coords;
    rnd             = gRandomLcgState >> 0x10;
    switch (work->ceilingProbePending) {
        case 0:
            if (work->timer != 0) {
                return 0;
            }
            if (work->onCeiling == 0) {
                if ((rnd & 0xF) == 0) {
                    if (!(arg0->spawnArg1.value & 1) && work->ceilingCooldown == 0) {
                        func_actor_400600_80137498(arg0, 2);
                        work->ceilingProbePending = 1;
                    }
                    return 0;
                }
                if ((rnd & 7) == 1 || (rnd & 7) == 2) {
                    if (work->playerDistance < 0x5DC && (u32)(work->targetBearing - 0x200) >= 0xC01U) {
                        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                        work2->state    = 8;
                        work2->subState = 0;
                        return 1;
                    }
                } else if (work->playerDistance < 0x5DC) {
                    if ((s16)work->targetBearing < 0x400) {
                        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                        work2->state    = 6;
                        work2->subState = 0;
                        return 1;
                    }
                    if ((s16)work->targetBearing > 0xC00) {
                        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                        work2->state    = 7;
                        work2->subState = 0;
                        return 1;
                    }
                }
            } else if ((rnd & 7) == 0) {
                if (work->ceilingCooldown == 0) {
                    work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                    work2->state    = 0x10;
                    work2->subState = 0;
                    return 1;
                }
            } else if ((rnd & 0xF) == 1) {
                if (work->ceilingCooldown == 0) {
                    work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
                    work2->state    = 0xD;
                    work2->subState = 0;
                    return 1;
                }
            } else {
                rnd1                                               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                rnd2                                               = (rnd1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState                                    = rnd2;
                ((_Actor400600ZebraStalkerWork*)arg0->work)->timer = 0x3C + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
                return 0;
            }
            return 0;
        case 1:
            work->ceilingProbePending = 0;
            dist                      = stalkerZebraIvoryWallDistance(arg0);
            if ((u16)(dist - 0x7D1) < 0x3E8) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 8, 0, 0)) {
                    y = -0x9C4;
                } else {
                    y = coord->coord.t[1] - dist;
                }
                work->leapY                                                     = y;
                work3                                                           = (_Actor400600ZebraStalkerWork*)arg0->work;
                work3->state                                                    = 0xC;
                work3->subState                                                 = 0;
                ((_Actor400600ZebraStalkerWork*)arg0->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                return 1;
            }
            ((_Actor400600ZebraStalkerWork*)arg0->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            return 0;
    }
    return 0;
}

static const TaskFuncTable3 D_actor_400600_80132030 = { {
    func_actor_400600_8013C874,
    func_actor_400600_8013C940,
    func_actor_400600_8013C9DC,
} };

static void func_actor_400600_80137EF0(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable3                fns   = D_actor_400600_80132030;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnimInline(arg0);
            fns.funcs[work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotationInline(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            actorUpdateModelColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->shadowShade);
            break;
    }
}

/// Refreshes the parts listed in `D_actor_400600_80151B88`, projects each into
/// view space with `arg1` as the Y, and passes nine fixed pairs of the resulting
/// points to `func_actor_400600_801383E4` along with `arg2` (`shadowShade` at
/// every call site).
static void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2)
{
    MATRIX    mtx;
    SVECTOR   pts[11];
    GfxCoord* coord;
    GfxCoord* root;
    s32       i;

    root                       = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    root->composeStamp         = GRAPHICS_COORD_DIRTY;
    for (i = 0; D_actor_400600_80151B88[i] != -1; i++) {
        coord               = &arg0->extra.tmd->coords[D_actor_400600_80151B88[i]];
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &mtx);
        pts[i].vx = mtx.t[0];
        pts[i].vy = arg1;
        pts[i].vz = mtx.t[2];
    }
    func_actor_400600_801383E4(&pts[1], &pts[5], 0x80, arg2);
    func_actor_400600_801383E4(&pts[5], &pts[6], 0x80, arg2);
    func_actor_400600_801383E4(&pts[1], &pts[3], 0x80, arg2);
    func_actor_400600_801383E4(&pts[3], &pts[4], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[2], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[7], 0x80, arg2);
    func_actor_400600_801383E4(&pts[7], &pts[8], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[9], 0x80, arg2);
    func_actor_400600_801383E4(&pts[9], &pts[10], 0x80, arg2);
}

/// Draws a semi-transparent textured quad along the segment from `arg0` to
/// `arg1`: widened by `width` either side, pulled in by half its length at both
/// ends, and shaded grey `shade`. The per-model counterpart of
/// `ActorsShared80163354`, taking view-space points instead of joints.
static void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade)
{
    _Actor400600LimbShadowQuadScratch* s;
    s16                                angle;
    s32                                halfX;
    s32                                halfZ;
    POLY_FT4*                          poly;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    s                          = SCRATCH_STACK_RESERVE_BLOCK(_Actor400600LimbShadowQuadScratch);
    actorRenderComposeCoord(&gGfxViewCoord);
    angle            = ratan2(arg1->vx - arg0->vx, arg1->vz - arg0->vz);
    halfX            = (arg0->vx - arg1->vx) / 2;
    halfZ            = (arg0->vz - arg1->vz) / 2;
    s->corners[0].vx = halfX + (arg0->vx - ((s32)(rcos(angle) * width) >> 0xC));
    s->corners[0].vy = arg0->vy;
    s->corners[0].vz = halfZ + (arg0->vz + ((s32)(rsin(angle) * width) >> 0xC));
    s->corners[1].vx = halfX + (arg0->vx + ((s32)(rcos(angle) * width) >> 0xC));
    s->corners[1].vy = arg0->vy;
    s->corners[1].vz = halfZ + (arg0->vz - ((s32)(rsin(angle) * width) >> 0xC));
    s->corners[2].vx = (arg1->vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corners[2].vy = arg1->vy;
    s->corners[2].vz = (arg1->vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    s->corners[3].vx = (arg1->vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corners[3].vy = arg1->vy;
    s->corners[3].vz = (arg1->vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    s->depth = RotTransPers4(&s->corners[0], &s->corners[1], &s->corners[2], &s->corners[3], &s->screenCorners[0], &s->screenCorners[1],
                             &s->screenCorners[2], &s->screenCorners[3], &s->depthCue, &s->flag);
    if (s->flag >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 9);
        poly->code                     = 0x2E;
        GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screenCorners[0];
        GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screenCorners[1];
        GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screenCorners[2];
        GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screenCorners[3];
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade, shade, shade);
        addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor400600LimbShadowQuadScratch);
}

/// Copies this actor's model flags onto both child tasks' models and, for a
/// non-negative `arg1`, sets the children's light mode to it.
static void func_actor_400600_801387DC(Task* arg0, s32 arg1)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Task*                         child;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    if (work->armTasks[0] != NULL) {
        child                   = work->armTasks[0];
        child->extra.tmd->flags = model->flags;
        if (arg1 >= 0) {
            worldCoordSetActorColorMode(child->spawnArg2.pointer, arg1);
        }
    }
    if (work->armTasks[1] != NULL) {
        child                   = work->armTasks[1];
        child->extra.tmd->flags = model->flags;
        if (arg1 >= 0) {
            worldCoordSetActorColorMode(child->spawnArg2.pointer, arg1);
        }
    }
}

/// Returns the id of the first `D_actor_400600_80151B40` zone containing the
/// actor's world XZ position, or 0 if none does.
static s32 func_actor_400600_8013886C(Task* arg0)
{
    GfxCoord*  coord;
    ActorZone* zone;
    s16        x;
    s16        z;

    coord = arg0->extra.tmd->coords;
    x     = (u16)coord->coord.t[0];
    z     = (u16)coord->coord.t[2];
    for (zone = D_actor_400600_80151B40; zone->id != ACTOR_ZONE_END; zone++) {
        if (zone->x <= x && x <= zone->x + zone->width && zone->z <= z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return 0;
}

static s32 func_actor_400600_8013892C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if ((work->previousAnimationFlags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

static void func_actor_400600_8013896C(Task* arg0, s16 arg1)
{
    GfxCoord* coord;
    SVECTOR   vec;
    s32       i;

    coord = arg0->extra.tmd->coords;
    Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, NULL);
    for (i = 0; i < 16; i++) {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = arg1;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
    }
}

static void func_actor_400600_80138A24(Task* arg0, s16 arg1)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Enemy*                        enemy;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if (arg1 != 0) {
        enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
        work->cloaked         = 1;
        work->cloakFadeFrames = 0;
        work->cloakFading     = 0;
        work->shadowShade     = 0;
    }
}

#include "../../shared/stalker_zebra_ivory_clear_queued.inc.c"

static void func_actor_400600_80138AB8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->timer > 0) {
        work->timer--;
    }
    if (work->ceilingCooldown > 0) {
        work->ceilingCooldown--;
    }
}

#include "../../shared/stalker_zebra_ivory_seed_timer.inc.c"

#include "../../shared/stalker_zebra_ivory_drop_capsule_grid.inc.c"

static void func_actor_400600_80138B5C(Task* arg0, s32 arg1)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    Enemy*                        enemy;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (!(arg1 & 0xFF)) {
        if (work->cloaked != 0) {
            work->cloaked         = 0;
            work->cloakFading     = 1;
            work->cloakFadeFrames = 0;
            model->flags          = (model->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            func_actor_400600_801387DC(arg0, 0);
        }
    } else if (work->cloaked != 1) {
        work->cloaked         = 1;
        work->cloakFading     = 1;
        work->cloakFadeFrames = 0;
        model->flags         |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
    }
}

static void func_actor_400600_80138C34(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable8                fns   = D_actor_400600_80131E7C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnim(arg0);
            fns.funcs[work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80132704(arg0, work->shadowWallZ, work->shadowShade);
            break;
    }
}

static void func_actor_400600_80138D78(Task* arg0)
{
    TmdObject*                    model    = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work     = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable4                handlers = D_actor_400600_80131E9C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnim(arg0);
            handlers.funcs[work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->shadowShade);
            break;
    }
}

static void func_actor_400600_80138EA0(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable6                fns   = D_actor_400600_80131E54;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            fns.funcs[work->state](arg0);
            stalkerZebraIvoryTickAnim(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->shadowShade);
            break;
    }
}

static void func_actor_400600_80138FD4(Task* arg0)
{
    TmdObject*                    model    = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work     = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable4                handlers = D_actor_400600_80131E6C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_400600_80136670(arg0);
            handlers.funcs[work->state](arg0);
            stalkerZebraIvoryTickAnim(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldCollisionClearContacts(work->bodyContacts);
            worldCollisionClearContacts(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->shadowShade);
            break;
    }
}

static void func_actor_400600_801390FC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->state    = 1;
    work->subState = 0;
}

static void func_actor_400600_80139110(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    stalkerZebraIvoryClearQueued(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[work->subState](arg0);
        if ((s16)func_actor_400600_80137C34(arg0) == 0 && (s16)func_actor_400600_80137AF0(arg0) == 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 8, 0, 0) && work->onCeiling != 0 && arg0->extra.tmd->coords->coord.t[0] > 10000) {
            _Actor400600ZebraStalkerWork* cur = (_Actor400600ZebraStalkerWork*)arg0->work;

            cur->state    = 0xD;
            cur->subState = 0;
        }
    }
}

static void func_actor_400600_80139218(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013B830, func_actor_400600_8013B8AC };

    stalkerZebraIvoryClearQueued(arg0);
    fns[work->subState](arg0);
}

static void func_actor_400600_80139280(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013B984, func_actor_400600_8013BA00 };

    stalkerZebraIvoryClearQueued(arg0);
    fns[work->subState](arg0);
}

static void func_actor_400600_801392E8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable3                fns  = D_actor_400600_80131F34;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[work->subState](arg0);
}

static void func_actor_400600_8013935C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013BBF4, func_actor_400600_80133CB0 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[work->subState](arg0);
    }
}

static void func_actor_400600_801393D0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013BC68, func_actor_400600_80133E38 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[work->subState](arg0);
    }
}

static void func_actor_400600_80139444(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable8                fns  = D_actor_400600_80131F40;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[work->subState](arg0);
}

static void func_actor_400600_801394E0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work     = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable4                handlers = D_actor_400600_80131F60;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[work->subState](arg0);
}

static void func_actor_400600_80139560(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    SVECTOR                       pos;
    s16                           count;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    stalkerZebraIvoryClearQueued(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        count           = work->countdown - 1;
        work->countdown = count;
        if (count == 0) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 0xB;
            work2->subState = 0;
            return;
        }
        pos.vx = work->targetPos.vx;
        pos.vy = work->targetPos.vy;
        pos.vz = work->targetPos.vz;
        stalkerZebraIvoryTurnToward(arg0, &pos, 0x18);
        stalkerZebraIvoryStepClip4(arg0);
    }
}

static void func_actor_400600_80139608(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013BFD4, stalkerZebraIvoryRightItself };

    stalkerZebraIvoryClearQueued(arg0);
    fns[work->subState](arg0);
}

static void func_actor_400600_80139670(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable3                fns  = D_actor_400600_80131F70;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[work->subState](arg0);
}

static void func_actor_400600_801396E4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work     = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable4                handlers = D_actor_400600_80131F7C;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[work->subState](arg0);
}

static void func_actor_400600_80139764(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work     = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable4                handlers = D_actor_400600_80131F8C;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[work->subState](arg0);
}

#include "../../shared/stalker_zebra_ivory_run_sub_states.inc.c"

static void func_actor_400600_80139878(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013C518, stalkerZebraIvoryPickRange };

    stalkerZebraIvoryClearQueued(arg0);
    fns[work->subState](arg0);
}

static void func_actor_400600_801398E0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work             = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*fns[2])(Task*) = { func_actor_400600_8013C598, func_actor_400600_8013C5F8 };

    stalkerZebraIvoryClearQueued(arg0);
    fns[work->subState](arg0);
}

#include "../../shared/stalker_zebra_ivory_apply_rotation.inc.c"

#include "../../shared/stalker_zebra_ivory_restart_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_blend_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_scale_frame.inc.c"

#include "../../shared/stalker_zebra_ivory_turn_toward.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_anim.inc.c"

#include "../../shared/stalker_zebra_ivory_play_clip.inc.c"

static void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
}

#include "../../shared/stalker_zebra_ivory_part_view_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_pin_part_xz.inc.c"

static void func_actor_400600_80139F4C(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[arg1];
    actorRenderComposeCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    arg2->vx            = local.t[0];
    arg2->vy            = local.t[1];
    arg2->vz            = coords[0].coord.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400600_80139FE0(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    root;
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[arg1];
    actorRenderComposeCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &root);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0]   = arg2->vx - (local.t[0] - root.t[0]);
    coords[0].coord.t[1]   = arg2->vy - (local.t[1] - root.t[1]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    actorRenderComposeCoord(coords);
}

#include "../../shared/stalker_zebra_ivory_clip_done.inc.c"

void func_actor_400600_8013A0F0(Task* arg0)
{
    TaskFuncTable9 states = D_actor_400600_80131EAC;

    states.funcs[arg0->state](arg0);
}

static void func_actor_400600_8013A170(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    TaskFuncTable12               fns   = D_actor_400600_80131E24;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            fns.funcs[work->state](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, work->shadowHeight, work->shadowShade);
            break;
    }
}

static void func_actor_400600_8013A26C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work                = (_Actor400600ZebraStalkerWork*)arg0->work;
    void                          (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[work->state](arg0);
}

#include "../../shared/stalker_zebra_ivory_update_color.inc.c"

#include "../../shared/stalker_zebra_ivory_set_move_mode.inc.c"

void func_actor_400600_8013A3A8(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    ((_Actor400600ZebraStalkerWork*)arg0->work)->playerDied = 1;
}

void func_actor_400600_8013A3B8(Task* task)
{
}

void func_actor_400600_8013A3C0(Task* task)
{
}

static void func_actor_400600_8013A3C8(Task* arg0)
{
    Enemy*                        enemy;
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    enemy                     = (Enemy*)arg0->spawnArg2.pointer;
    work                      = (_Actor400600ZebraStalkerWork*)arg0->work;
    model                     = arg0->extra.tmd;
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldTargetUnlinkNode(&enemy->node);
    if (work->pendingAction == STALKER_ZEBRA_IVORY_PENDING_BLAST) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_400600_801387DC(arg0, -1);
        work->stateFrames = 0;
        func_actor_400600_8013CC04(arg0, 7);
    } else if (work->onCeiling == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        func_actor_400600_801387DC(arg0, 0);
        work->state++;
    } else {
        func_actor_400600_8013CC04(arg0, 9);
    }
}

#include "../../shared/stalker_zebra_ivory_resume_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_animate_until_done.inc.c"

static void func_actor_400600_8013A570(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    GfxCoord*                     coord = arg0->extra.tmd->coords;

    ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmBody);
    worldCollisionUnlinkBody(&work->leftArmBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
    work->corpseScaleY = 0x1000;
    work->savedRootMtx = coord->coord;
    worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state++;
}

static void func_actor_400600_8013A638(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    work->stateFrames++;
    if (work->stateFrames >= 0x18) {
        model->flags |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
        work->stateFrames = 0;
        work->state++;
    }
}

static void func_actor_400600_8013A6C4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;
    GfxCoord*                     coord;
    VECTOR                        scale;
    SVECTOR                       rot;

    work                = (_Actor400600ZebraStalkerWork*)arg0->work;
    model               = arg0->extra.tmd;
    coord               = model->coords;
    work->shadowShade   = (u16)work->shadowShade + (-work->shadowShade >> 2);
    work->corpseScaleY -= 0x30;
    scale.vx            = 0x1000;
    scale.vy            = work->corpseScaleY;
    scale.vz            = 0x1000;
    coord->coord        = work->savedRootMtx;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->stateFrames++;
    if (work->stateFrames == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 4, &rot);
    }
    if (work->stateFrames >= 0x11) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_400600_801387DC(arg0, -1);
        work->state++;
    }
}

static void func_actor_400600_8013A808(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    arg0->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

static void func_actor_400600_8013A820(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames >= 2) {
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

static void func_actor_400600_8013A908(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    model         = arg0->extra.tmd;
    work          = (_Actor400600ZebraStalkerWork*)arg0->work;
    model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    func_actor_400600_80139DB0(arg0, 9, 0x10, 2);
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->shadowHeight = work->floorY;
    stalkerZebraIvoryTickAnim(arg0);
    work->state++;
}

static void func_actor_400600_8013A990(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;

    work               = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (work->floorY < coord->coord.t[1]) {
        coord->coord.t[1] = work->floorY;
        stalkerZebraIvoryPlayClip(arg0, 0x13, 0x10);
        work->roll += 0x800;
        stalkerZebraIvoryApplyRotation(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->stateFrames = 0;
        work->onCeiling   = 0;
        work->onBack      = 1;
        work->state++;
    }
    stalkerZebraIvoryTickAnim(arg0);
}

static void func_actor_400600_8013AA5C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->stateFrames == 0) {
        func_actor_400600_8013CB70(arg0, 0x40060006);
        work->stateFrames++;
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        func_actor_400600_8013CC04(arg0, 1);
    }
    stalkerZebraIvoryTickAnim(arg0);
}

static void func_actor_400600_8013AAD8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    Task*                         child;
    Task*                         child2;

    work                                     = (_Actor400600ZebraStalkerWork*)arg0->work;
    gSceneCombatState.zebraStalkerDeathAlert = 1;
    child                                    = work->armTasks[0];
    if (child != NULL) {
        taskKill(child);
    }
    child2 = work->armTasks[1];
    if (child2 != NULL) {
        taskKill(child2);
    }
    work->stateFrames = 0;
    work->state++;
}

static void func_actor_400600_8013AB44(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames >= 0x97) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}

static void func_actor_400600_8013AB98(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    model             = arg0->extra.tmd;
    work->shadowShade = 0;
    func_actor_400600_80138B5C(arg0, 1);
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
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
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

static void func_actor_400600_8013AD3C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->stateFrames = 0;
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x22, 0x10);
        work->state++;
    }
}

static void func_actor_400600_8013ADA4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coord;
    s32                           x;
    s32                           y;
    u16                           step;
    u16                           accum;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= 5) {
        x                 = coord->coord.t[0];
        coord->coord.t[0] = x + ((0x40B5 - x) >> 3);
        coord->coord.t[2] = coord->coord.t[2] + 0x78;
        step              = (u16)work->moveAccel + 2;
        accum             = (u16)work->moveSpeed + step;
        work->moveSpeed   = accum;
        work->moveAccel   = step;
        y                 = coord->coord.t[1] + (s16)accum;
        coord->coord.t[1] = y;
        if (y >= 0) {
            Gp_SpawnPadLerp(0x10, 0x80, 0x20);
            work->stateFrames = 0;
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coord->coord.t[1] = 0;
            work->state++;
        }
    }
}

static void func_actor_400600_8013AE88(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    model             = arg0->extra.tmd;
    work->shadowShade = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

static void func_actor_400600_8013AF04(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    GfxCoord*                     coord;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;
    if (work->roomCommand == 2) {
        coord->coord.t[0] = 0x1FDD;
        coord->coord.t[1] = -0xE38;
        coord->coord.t[2] = 0x5CE;
        work->pitch       = 0;
        work->yaw         = 0x400;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->moveAccel   = 0;
        work->moveSpeed   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->state++;
    } else if (work->roomCommand == 3) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]         = 0x640;
        coord->coord.t[2]         = 0x87A;
        coord->coord.t[1]         = 0;
        work->pitch               = 0;
        work->yaw                 = 0x400;
        work->roll                = 0;
        work->shadowShade         = 0;
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
    }
}

static void func_actor_400600_8013B018(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           soundId;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
        work3                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state              = 2;
        work3->subState           = 0;
    }
}

static void func_actor_400600_8013B0FC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work                      = (_Actor400600ZebraStalkerWork*)arg0->work;
    model                     = arg0->extra.tmd;
    work->shadowShade         = 0;
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

static void func_actor_400600_8013B150(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        sceneEngageBattle(1);
        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state     = 1;
        work2->state    = 0;
        work2->subState = 0;
        work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
    }
}

static void func_actor_400600_8013B1DC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->stateFrames++;
    func_actor_400600_801361AC(arg0);
    if (work->stateFrames == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if (work->stateFrames >= 0x27) {
        work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 4);
    }
    if (work->stateFrames == 0x50) {
        work->stateFrames = 0;
        work->moveAccel   = -0xA;
        work->moveSpeed   = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->state++;
    }
}

static void func_actor_400600_8013B2A8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           soundId;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        sceneEngageBattle(1);
        work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state     = 1;
        work2->state    = 0;
        work2->subState = 0;
        work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
    }
}

static void func_actor_400600_8013B394(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    TmdObject*                    model;

    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    model             = arg0->extra.tmd;
    work->shadowShade = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

static void func_actor_400600_8013B410(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    GfxCoord*                     coords;
    s32                           soundId;
    s32                           pan;

    work                = (_Actor400600ZebraStalkerWork*)arg0->work;
    coords              = arg0->extra.tmd->coords;
    work->shadowShade  += (0xFF - work->shadowShade) >> 5;
    work->moveAccel    += 1;
    work->moveSpeed    += work->moveAccel;
    coords->coord.t[1] += work->moveSpeed;
    if (coords->coord.t[1] >= 0) {
        work->stateFrames = 0;
        soundId           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060003;
        pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->state++;
    }
}

static void func_actor_400600_8013B520(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           soundId;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 1) {
        Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags                        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        gSceneCombatState.zebraStalkerGroupPhase = SCENE_COMBAT_ZEBRA_STALKER_DELAYED;
        work2                                    = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state                              = 1;
        work2->state                             = 0;
        work2->subState                          = 0;
        work3                                    = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state                             = 2;
        work3->subState                          = 0;
    }
}

static void func_actor_400600_8013B640(void)
{
    u8 param1[8];
    u8 param2[8];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        /* Same shape as ActorsShared801692e8: each branch makes its own call
         * and jump2's cross-jumping merges the identical tails. */
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 32, 0, 0) && gGameSession->location.loc.variant == 1) {
            param1[2] = 0x28;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        } else {
            param1[2] = 0x28;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
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
    func_actor_400600_80135998(arg0, 0x10);
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
    stalkerZebraIvoryTurnToward(arg0, &pos, work->turnStep);
    func_actor_400600_80135998(arg0, work->walkStep);
}

static void func_actor_400600_8013B830(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 2;
        work->animStep    = 0x20;
        work->animClip    = 9;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 2;
        work->animStep    = 0x20;
        work->animClip    = 0xB;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013B8AC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (work->pendingArmed != 0 && work->pendingAction == STALKER_ZEBRA_IVORY_PENDING_LIGHT) {
        if (work->onBack == 0) {
            work->animStep    = 0x20;
            work->animClip    = 9;
            work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        } else {
            work->animStep    = 0x20;
            work->animClip    = 0xB;
            work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        }
        return;
    }
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013B984(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0xA;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0xC;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013BA00(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013BA6C(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 8;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0xF;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0x11;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013BAEC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
        if (work->onBack == 0) {
            work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->animBlend   = 8;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animClip    = 0x10;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        } else {
            work3              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->animBlend   = 8;
            work3->animStep    = ANIMATION_RATE_ONE;
            work3->animClip    = 0x12;
            work3->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        }
        work->subState++;
    }
}

static void func_actor_400600_8013BB88(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013BBF4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->animBlend   = 8;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 7;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    func_actor_400600_8013CB40(arg0, 0);
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013BC68(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->animBlend   = 8;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 8;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    func_actor_400600_8013CB40(arg0, 1);
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013BCD8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        work->stateFrames = 0;
        func_actor_400600_80138B5C(arg0, 0);
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animBlend   = 4;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 1;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}

static void func_actor_400600_8013BD54(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        work->stateFrames++;
        if (work->stateFrames >= 0x11) {
            work->stateFrames  = 0;
            work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->animBlend   = 4;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animClip    = 0x15;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            work->subState++;
        }
    }
}

static void func_actor_400600_8013BDF0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        work->stateFrames++;
        if (work->stateFrames >= 0x11) {
            work->subState++;
        }
    }
}

static void func_actor_400600_8013BE58(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    func_actor_400600_80137498(arg0, 0);
    work->playerDied = 0;
    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_release_hold.inc.c"

static void func_actor_400600_8013BF48(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    func_actor_400600_80137498(arg0, 1);
    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_wait_clip.inc.c"

static void func_actor_400600_8013BFD4(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;

    work               = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames  = 0;
    work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = ANIMATION_RATE_ONE;
    work2->animClip    = 0x16;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    stalkerZebraIvoryReadPartViewXZ(arg0, 0xE, &work->anchorPos);
    work->subState++;
}

static void func_actor_400600_8013C038(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->animBlend   = 4;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0x15;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->subState++;
}

static void func_actor_400600_8013C074(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (((stalkerZebraIvoryTakePending(arg0) << 0x10) == 0) && ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0)) {
        rnd                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = rnd;
        work->ceilingCooldown = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                 = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->state          = 2;
        work2->subState       = 0;
    }
}

static void func_actor_400600_8013C104(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->holding = 1;
    work->subState++;
}

static void func_actor_400600_8013C124(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;

    work  = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = ANIMATION_RATE_ONE;
    work2->animClip    = 0x20;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->moveAccel    = 0x40;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->leapX        = coord->coord.t[0];
    work->leapZ        = coord->coord.t[2];
    work->shadowHeight = work->floorY;
    work->subState++;
}

static void func_actor_400600_8013C1C0(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    func_actor_400600_80138B5C(arg0, 0);
    work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = ANIMATION_RATE_ONE;
    work2->animClip    = 9;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->holding      = 1;
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->subState++;
    work->shadowHeight = work->floorY;
}

static void func_actor_400600_8013C238(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    GfxCoord*                     coord;

    work               = (_Actor400600ZebraStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel    = work->moveAccel + 2;
    work->moveSpeed    = work->moveSpeed + work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (work->floorY < coord->coord.t[1]) {
        coord->coord.t[1]  = work->floorY;
        work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 0x13;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        work->onBack       = 1;
        work->stateFrames  = 0;
        work->onCeiling    = 0;
        work->roll         = work->roll + 0x800;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_wait_clip_then_rest.inc.c"

static void func_actor_400600_8013C394(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0x1A;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0x1B;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState++;
}

static void func_actor_400600_8013C410(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->animBlend   = 0x1E;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animClip    = 0x10;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        } else {
            work3              = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->animBlend   = 0x1E;
            work3->animStep    = 8;
            work3->animClip    = 0x14;
            work3->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        }
        work->subState++;
    }
}

static void func_actor_400600_8013C4AC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013C518(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->subState++;
}

#include "../../shared/stalker_zebra_ivory_pick_range.inc.c"

static void func_actor_400600_8013C598(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    u32                           rnd;

    work->animBlend   = 4;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 1;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState   = rnd;
    work->idleFrames  = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->subState++;
}

static void func_actor_400600_8013C5F8(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    u16                           count;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (((func_actor_400600_80136FA8(arg0) << 0x10) == 0) && ((func_actor_400600_80137C34(arg0) << 0x10) == 0)) {
        count            = work->idleFrames - 1;
        work->idleFrames = count;
        if ((count << 0x10) == 0) {
            rnd                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState    = rnd;
            work->hideCooldown = ((rnd >> 0x10) & 0x3F) + 0x1E;
            func_actor_400600_80138B5C(arg0, 0);
            work2           = (_Actor400600ZebraStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        }
    }
}

static void func_actor_400600_8013C6B0(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out)
{
    VECTOR v;
    VECTOR n;
    s32    dx;
    s32    dz;
    s32    dist;
    s32    t;

    dx   = pos->vx - rec->point.vx;
    v.vy = 0;
    v.vx = dx;
    dz   = pos->vz - rec->point.vz;
    v.vz = dz;
    dist = rec->distance - SquareRoot0(dx * dx + dz * dz);
    t    = dist;
    if (dist <= 0) {
        t = 0;
    }
    dist = t;
    v.vx = pos->vx - rec->point.vx;
    v.vy = pos->vy - rec->point.vy;
    v.vz = pos->vz - rec->point.vz;
    VectorNormal(&v, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &n, &v);
    out->vx = (dist * v.vx) >> 12;
    out->vy = 0;
    out->vz = (dist * v.vz) >> 12;
}

static s32 func_actor_400600_8013C7E8(s16 arg0, s16 arg1)
{
    s16 v;

    v = arg1 >> 3;
    if (arg0 == 0) {
        return v;
    }
    if ((arg0 > 0 && v < 0) || (arg0 < 0 && v > 0)) {
        return arg0;
    }
    if (arg0 > 0) {
        if (v < arg0) {
            return arg0;
        }
        return v;
    }
    if (v < arg0) {
        return v;
    }
    return arg0;
}

static void func_actor_400600_8013C874(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    TmdObject*                    model;
    TmdObject*                    model2;

    work              = (_Actor400600ZebraStalkerWork*)arg0->work;
    model             = arg0->extra.tmd;
    work->shadowShade = 0;
    work2             = (_Actor400600ZebraStalkerWork*)arg0->work;
    model2            = arg0->extra.tmd;
    if (work2->cloaked != 1) {
        work2->cloaked         = 1;
        work2->cloakFading     = 1;
        work2->cloakFadeFrames = 0;
        model2->flags         |= TMD_OBJECT_SEMI_TRANS;
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
    }
    work->body.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state++;
}

static void func_actor_400600_8013C940(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    u32                           rnd;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_DELAYED) {
        rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState   = rnd;
        work->stateFrames = ((rnd >> 0x10) & 7) + 0x14;
        work->state++;
    } else if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_ACTIVE) {
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
    }
}

static void func_actor_400600_8013C9DC(Task* arg0)
{
    _Actor400600ZebraStalkerWork* work;
    _Actor400600ZebraStalkerWork* work2;
    _Actor400600ZebraStalkerWork* work3;
    s32                           soundId;
    s32                           pan;

    work = (_Actor400600ZebraStalkerWork*)arg0->work;
    work->stateFrames--;
    if (work->stateFrames == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060003;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->body.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        arg0->state               = 1;
        work2->state              = 0;
        work2->subState           = 0;
        work3                     = (_Actor400600ZebraStalkerWork*)arg0->work;
        work3->state              = 2;
        work3->subState           = 0;
    }
}

#include "../../shared/stalker_zebra_ivory_take_armed_pending.inc.c"

static void func_actor_400600_8013CB40(Task* arg0, u8 arg1)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;
    s32                           mode = arg1;

    if (mode == 0) {
        work->leftArmOut = 1;
    } else if (mode == 1) {
        work->rightArmOut = mode;
    }
}

static void func_actor_400600_8013CB70(Task* arg0, s32 arg1)
{
    s32 soundId;
    s32 pan;

    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        arg1 &= 0xFF00FFFF;
        arg1 |= 0x4A0000;
    }
    soundId = arg1 | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
}

static void func_actor_400600_8013CC04(Task* arg0, s16 arg1)
{
    _Actor400600ZebraStalkerWork* work = (_Actor400600ZebraStalkerWork*)arg0->work;

    work->state    = arg1;
    work->subState = 0;
}
