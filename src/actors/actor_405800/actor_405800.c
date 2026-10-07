#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

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
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/frame_capture.h"
#include "../../shared/limb_shadows.h"
#define STALKER_ZEBRA_IVORY_KIND STALKER_IVORY
#include "../../shared/stalker_zebra_ivory.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

/// Values of `_Actor405800IvoryStalkerWork::cloakRequest`.
enum {
    ACTOR_405800_CLOAK_HIDE      = 0,    // cloak, then fade the body out and drop it from lock-on
    ACTOR_405800_CLOAK_SHOW      = 1,    // fade the body in and make it lockable, then drop the cloak
    ACTOR_405800_CLOAK_KIND_MASK = 0x7F, // the fade a request asks for
    ACTOR_405800_CLOAK_RUNNING   = 0x80  // the fade has not finished
};

/// Values of `_Actor405800IvoryStalkerWork::stunKind`: what ends the status hold.
enum {
    ACTOR_405800_STUN_NONE   = 0,
    ACTOR_405800_STUN_TIMED  = 1, // a count of 21 frames
    ACTOR_405800_STUN_STATUS = 2  // the enemy's status buildup running out
};

/// Work block of the Ivory Stalker task.
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
/// | 0 | ask for a hide, then 1 |
/// | 1 | wait for the target within 0x1450, then show and leap to the ceiling |
/// | 2 | walk at the target; picks an attack once `timer` has run out |
/// | 3, 4 | light and heavy recoil |
/// | 5 | status hold, ended as `stunKind` says |
/// | 6, 7 | left-arm and right-arm strike |
/// | 8 | grab: probe the line to the target, then hold and bite the player |
/// | 9 | leap back, as far as the probe behind it allows |
/// | 0xA | crawl on its back until `countdown` runs out |
/// | 0xB | right itself |
/// | 0xC, 0xD | leap to the ceiling, drop from it |
/// | 0xE | knocked off the ceiling onto its back |
/// | 0xF | knockdown |
/// | 0x10 | leave the ceiling: drop when the target is beyond 2000, else grab |
/// | 0x11 | idle for `idleFrames`, then show and walk |
///
/// While it dies (task state 2) `state` walks twelve steps: 0 picks the death
/// (1 on the floor, 7 after a blast, 9 on the ceiling); 1..6 play the death
/// clip, unlink the bodies and burn the corpse away; 7 and 8 burst the body
/// into chunks; 9..0xB drop it from the ceiling onto its back and rejoin 1.
/// Task state 3 kills the arm tasks and destroys the enemy 0x12D frames later.
typedef struct {
    MATRIX                savedRootMtx;           // root matrix when the corpse began to burn; each frame rescales a copy of it
    MATRIX                colorMtx;               // storage for the `TmdObject::colorMtx` of the body and both arm models
    MATRIX                lightMtx;               // storage for their `TmdObject::lightMtx`
    byte                  field_60[0x10];         // never accessed
    VECTOR                prevRootPos;            // root position at the start of the frame; X and Z are restored when the collision step reports a conflict
    s16                   pitch;                  // root pitch in 4096ths of a turn; swept through half a turn by a leap to the ceiling or a drop from it
    s16                   yaw;                    // root heading in 4096ths of a turn
    s16                   roll;                   // root roll in 4096ths of a turn; half a turn apart on its feet and on the ceiling or its back
    byte                  field_86[2];            // never accessed
    SVECTOR3              anchorPos;              // view-space X and Z a part is pinned to while the root moves around it; `vy` is never accessed
    byte                  field_8E[2];            // never accessed
    s16                   spawnX;                 // root X at spawn; never read
    s16                   floorY;                 // root Y at spawn: the height every leap, drop and fall lands on
    s16                   spawnZ;                 // root Z at spawn; never read
    byte                  field_96[2];            // never accessed
    s16                   leapX;                  // X a leap back or a drop brings the root to; after a hold, the X step per frame of the hop off the player
    s16                   leapY;                  // height the leap to the ceiling rises to (-0x9C4) or the hop off the player lands on (`floorY`)
    s16                   leapZ;                  // Z counterpart of `leapX`
    byte                  field_9E[0xA];          // never accessed
    SVECTOR               targetPos;              // point it hunts: the player's root, or (0x834, the player's Y, 0xD48) while a battle has the player at X 15000 or beyond
    ActorAnimRig18        rig;                    // playback of the model's parts: slots 1 to 17 play `animClip`, and slot 1's status tells when it ended
    WorldCollisionBody    body;                   // sphere on part 3 that takes hits; left out of pair tests while the hold has the player
    WorldCollisionContact bodyContacts[8];        // contacts of `body`; also the enemy's hit records
    WorldCollisionBody    gridBody;               // larger sphere on the root tested against the room grid, except during a hold or a leap to the ceiling
    WorldCollisionContact gridContacts[8];        // contacts of `gridBody`; they give the push-out of each frame
    WorldCollisionBody    rightArmInner;          // attack sphere nearer the root of part 7; shares `rightArmContacts`
    WorldCollisionBody    leftArmInner;           // attack sphere nearer the root of part 10; shares `leftArmContacts`
    WorldCollisionBody    rightArmOuter;          // attack sphere far along part 7; enabled only on frames 0x16..0x1B of the right-arm strike
    WorldCollisionBody    leftArmOuter;           // attack sphere far along part 10; enabled only on frames 0x16..0x1B of the left-arm strike
    WorldCollisionContact rightArmContacts[1];    // contacts of the two right-arm spheres
    WorldCollisionContact leftArmContacts[1];     // contacts of the two left-arm spheres
    WorldCollisionBody    capsuleBody;            // probe on the root, tested against the room grid only while a grab or a leap back looks for walls
    WorldCollisionCapsule capsule;                // its shape: from the root to the target for a grab, 6000 behind the root for a leap back
    WorldCollisionContact capsuleContacts[8];     // contacts of `capsuleBody`; the first grid face among them is the wall
    EffectSpawnArg        effectArg;              // argument record of the effects its hits spawn, hung off part 3
    Task*                 armTasks[2];            // tasks of the arm models (0 left on part 10, 1 right on part 7), shown only while that arm is swung out
    byte                  field_82C[4];           // never accessed
    s16                   cloakTimer;             // frames the holding phase of a cloak fade has lasted
    s16                   cloakLevel;             // grey level of the cloak shading, 0 (none) to 0xFF
    s16                   colorBlend;             // the model's `TmdObject::shading.colorBlend`, 0 to `TMD_OBJECT_COLOR_BLEND_ONE`
    s16                   hideHoldFrames;         // frames a hide holds the cloak before the body fades out; longer at lower health
    s16                   hideCooldownReset;      // value `hideCooldown` starts from when a show completes; longer at lower health, 0x1E..0x5D after an idle
    s16                   hideCooldown;           // frames before another hide may be requested
    u16                   previousAnimationFlags; // Slot 1's ANIMATION_SLOT_* results as the last running update's tick left them
    s16                   corpseScaleY;           // Y-axis scale of the burning corpse's root matrix (4.12, ONE = unscaled); lowered every frame of the burn
    u16                   frameCount;             // frames the enemy has run, counted from a random start; never read
    s16                   stateFrames;            // frames spent in the current state or sub-state
    s16                   holdFrames;             // frames since the hold caught the player
    s16                   state;                  // index into the state table of the current task state
    s16                   subState;               // index into the step table of the current state
    s16                   animBlend;              // frames a blend request takes; cleared when the blend starts
    s16                   moveAccel;              // added to `moveSpeed` each frame; itself grows each frame
    s16                   moveSpeed;              // vertical speed of a leap, a drop or a fall
    s16                   animStep;               // playback rate of slots 1..17; `ANIMATION_RATE_ONE` is normal speed
    s16                   playerDistance;         // horizontal distance from the root to `targetPos`
    u16                   bearingFromPlayer;      // heading from `targetPos` to the root relative to the player's heading, 0..0xFFF; 0 when the player faces it
    u16                   targetBearing;          // heading of `targetPos` relative to `yaw`, 0..0xFFF
    s16                   pendingArmed;           // 1 when a hit or a status tick dealt damage this frame; lets `pendingAction` be consumed
    s16                   pendingAction;          // `STALKER_ZEBRA_IVORY_PENDING_*` awaiting the state machine
    s16                   timer;                  // frames before the walk may pick its next attack
    s16                   leapCooldown;           // frames before another leap to the ceiling or drop from it; 0x12C after one
    s16                   nextAnchorPart;         // the walking part (8 or 0xB) the walk is not pinning; never read
    byte                  field_862[4];           // never accessed
    s16                   shadowShade;            // brightness of the limb shadows, 0 to 0xFF
    byte                  field_868[2];           // never accessed
    s16                   shadowHeight;           // height the limb shadows are laid at: `floorY`, or -0x9C4 once a leap nears the ceiling
    byte                  field_86C[2];           // never accessed
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
    s16                   holdLoops;              // bite clips the hold has completed; the hold ends at 4
    byte                  field_886[2];           // never accessed
    s8                    damageOverTimeSeen;     // set once the enemy has carried a damage-over-time status; never read
    byte                  field_889[1];           // never accessed
    u8                    roomCommand;            // kind of the last room command (1..4); never read
    u8                    playerDied;             // set by message 2014, broadcast when damage takes the player's last health; ends a hold
    u8                    holdKilledPlayer;       // 1 once a bite has taken the player's last health; the release then leaves the player's animation and scripted mode alone
    u8                    rightArmOut;            // 1 while the right-arm strike keeps its arm model swung out
    u8                    leftArmOut;             // 1 while the left-arm strike keeps its arm model swung out
    u8                    holding;                // 1 while the current move must finish before the death sequence starts
    u8                    onCeiling;              // 1 while it hangs from the ceiling
    u8                    onBack;                 // 1 once it has landed on its back; cleared when it rights itself
    u8                    distanceMode;           // how the probe's wall contact is measured (0 hit or miss, 1 X/Z distance, 2 X/Y distance)
    u8                    walkHurried;            // 1 once the walk has raised its steps for a target beyond 3000
    byte                  field_894[1];           // never accessed
    u8                    cloakRequest;           // `ACTOR_405800_CLOAK_*`; 0 once the fade has finished
    s8                    cloakPhase;             // step of the running cloak fade (0 first ramp, 1 hold, 2 second ramp)
    u8                    field_897;              // 0 lets a finished fade keep `WORLD_TARGET_KEEP_SCANNED` on the target; nothing writes it, role unproven
    u8                    stunKind;               // `ACTOR_405800_STUN_*`
    byte                  field_899[3];           // never accessed
} _Actor405800IvoryStalkerWork;
STATIC_ASSERT_SIZEOF(_Actor405800IvoryStalkerWork, 0x89C);

/// The work block the `stalkerZebraIvory` fragments included below operate on:
/// this package's own. `stalker_zebra_ivory.h` lists the members they reach.
typedef _Actor405800IvoryStalkerWork StalkerZebraIvoryWork;

/* `D_800678F0` selects the model stream the next `effectSpawn` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where an
 * empty-asm barrier was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

extern TaskDesc      D_actor_405800_801514B4[];
extern EnemyParams   D_actor_405800_801418FC;
extern AnimationSet* D_actor_405800_801513F8[6];
extern AnimationSet* D_actor_405800_80151410[35];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_405800_8015149C[3];
extern u8               gStalkerZebraIvoryResumeClips[];

/* The records closing four of the overlay's model streams, selected through
   `D_800678F0`. */
static TmdSource _gActor405800IvoryStalkerBurstHead;
static TmdSource _gActor405800StalkerBurstTorso;
static TmdSource _gActor405800StalkerEffect;
static TmdSource _gActor405800StalkerBurstHandLeft;

static void func_actor_405800_80132670(Task* arg0);
static void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_405800_80132FE0(Task* arg0);
static void func_actor_405800_8013315C(Task* arg0);
static void func_actor_405800_8013340C(Task* arg0);
static void func_actor_405800_801334B8(Task* arg0);
static void func_actor_405800_80133800(Task* arg0);
static void func_actor_405800_80133CD0(Task* arg0);
static void func_actor_405800_80133DB0(Task* arg0);
static void func_actor_405800_80133F48(Task* arg0);
static void func_actor_405800_801340E0(Task* arg0);
static void func_actor_405800_80134314(Task* arg0);
static void func_actor_405800_8013471C(Task* arg0);
static void func_actor_405800_801348E4(Task* arg0);
static void func_actor_405800_80134A64(Task* arg0);
static void func_actor_405800_80134E80(Task* arg0);
static void func_actor_405800_801351BC(Task* arg0);
static void func_actor_405800_80135558(Task* arg0);
static void func_actor_405800_801356A8(Task* arg0);
static void func_actor_405800_80135780(Task* arg0);
static void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
static void func_actor_405800_801361F8(Task* arg0);
static void func_actor_405800_80136388(Task* arg0);
static s32  func_actor_405800_80136A1C(Task* arg0);
static s32  func_actor_405800_80136CE0(Task* arg0);
static void func_actor_405800_80136E14(Task* task);
static void func_actor_405800_8013706C(Task* arg0, s16 arg1);
static s32  func_actor_405800_801373E0(Task* arg0);
static void func_actor_405800_801375C4(Task* task);
static s32  func_actor_405800_80137908(Task* arg0);
static void func_actor_405800_8013795C(Task* task);
static void func_actor_405800_80137A14(Task* task);
static void func_actor_405800_80137A60(Task* task);
static void func_actor_405800_80137B34(Task* task);
static void func_actor_405800_80137B9C(Task* task);
static void func_actor_405800_80137C04(Task* task);
static void func_actor_405800_80137C78(Task* task);
static void func_actor_405800_80137CEC(Task* task);
static void func_actor_405800_80137D60(Task* task);
static void func_actor_405800_80137DE4(Task* task);
static void func_actor_405800_80137E64(Task* task);
static void func_actor_405800_80137EF0(Task* task);
static void func_actor_405800_80137F58(Task* task);
static void func_actor_405800_80137FCC(Task* task);
static void func_actor_405800_80138040(Task* task);
static void func_actor_405800_80138154(Task* task);
static void func_actor_405800_801381BC(Task* task);
void        func_actor_405800_80138634(Task* task);
static void func_actor_405800_80138698(Task* arg0);
static void func_actor_405800_80138788(Task* arg0);
void        func_actor_405800_801388C4(Task* task, s32 msgId, s32 arg2, s32 arg3);
void        func_actor_405800_801388D4(Task* task);
void        func_actor_405800_801388DC(Task* task);
static void func_actor_405800_801388E4(Task* task);
static void func_actor_405800_80138A70(Task* task);
static void func_actor_405800_80138B50(Task* task);
static void func_actor_405800_80138BD4(Task* task);
static void func_actor_405800_80138BEC(Task* task);
static void func_actor_405800_80138C30(Task* task);
static void func_actor_405800_80138CF0(Task* task);
static void func_actor_405800_80138D54(Task* task);
static void func_actor_405800_80138E20(Task* task);
static void func_actor_405800_80138EF0(Task* task);
static void func_actor_405800_80138F54(Task* task);
static void func_actor_405800_80138FA8(Task* task);
static void func_actor_405800_8013902C(Task* task);
static void func_actor_405800_801390FC(Task* arg0);
static void func_actor_405800_80139188(Task* arg0);
static void func_actor_405800_80139260(Task* arg0);
static void func_actor_405800_801392EC(Task* arg0);
static void func_actor_405800_80139358(Task* arg0);
static void func_actor_405800_801393E8(Task* arg0);
static void func_actor_405800_801394E4(Task* arg0);
static void func_actor_405800_80139550(Task* task);
static void func_actor_405800_801395E8(Task* task);
static void func_actor_405800_8013967C(Task* task);
static void func_actor_405800_801397B8(Task* task);
static void func_actor_405800_80139844(Task* task);
static void func_actor_405800_80139880(Task* task);
static void func_actor_405800_801398C0(Task* task);
static void func_actor_405800_80139928(Task* task);
static void func_actor_405800_801399C4(Task* arg0);
static void func_actor_405800_80139AC4(Task* arg0);
static void func_actor_405800_80139B3C(Task* arg0);
static void func_actor_405800_80139C98(Task* arg0);
static void func_actor_405800_80139D24(Task* arg0);
static void func_actor_405800_80139DC0(Task* arg0);
static void func_actor_405800_80139E2C(Task* task);
static void func_actor_405800_80139EAC(Task* arg0);
static void func_actor_405800_80139F0C(Task* task, u8 arg1);
static void func_actor_405800_80139FB0(Task* task, s16 arg1);
static void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3);

static TmdSource _gActor405800IvoryStalkerBody;
void             func_actor_405800_80138634(Task*);

static TmdSource _gActor405800IvoryStalkerBurstArmRight;
static TmdSource _gActor405800IvoryStalkerBurstArmLeft;
void             func_actor_405800_801388C4(Task*, s32, s32, s32);
void             func_actor_405800_801388D4(Task*);
void             func_actor_405800_801388DC(Task*);

static TmdBone _gActor405800IvoryStalkerBodySkeleton[18] = {
#include "assets/ivory_stalker_body_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBodyPartVerts[18] = {
#include "assets/ivory_stalker_body_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBodyVerts[257] = {
#include "assets/ivory_stalker_body_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBodyNormals[249] = {
#include "assets/ivory_stalker_body_normals.inc"
};

static u32 _gActor405800IvoryStalkerBodyStream[3666] = {
#include "assets/ivory_stalker_body_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBody = {
    0,
    36152,
    13736,
    18,
    _gActor405800IvoryStalkerBodyPartVerts,
    _gActor405800IvoryStalkerBodyVerts,
    _gActor405800IvoryStalkerBodyNormals,
    _gActor405800IvoryStalkerBodySkeleton,
    _gActor405800IvoryStalkerBodyStream,
};

static TmdBone _gActor405800IvoryStalkerBurstArmRightSkeleton[1] = {
#include "assets/ivory_stalker_burst_arm_right_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmRightPartVerts[1] = {
#include "assets/ivory_stalker_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmRightVerts[10] = {
#include "assets/ivory_stalker_burst_arm_right_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmRightNormals[12] = {
#include "assets/ivory_stalker_burst_arm_right_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmRightStream[85] = {
#include "assets/ivory_stalker_burst_arm_right_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstArmRight = {
    0,
    528,
    0,
    1,
    _gActor405800IvoryStalkerBurstArmRightPartVerts,
    _gActor405800IvoryStalkerBurstArmRightVerts,
    _gActor405800IvoryStalkerBurstArmRightNormals,
    _gActor405800IvoryStalkerBurstArmRightSkeleton,
    _gActor405800IvoryStalkerBurstArmRightStream,
};

static TmdBone _gActor405800IvoryStalkerBurstArmLeftSkeleton[1] = {
#include "assets/ivory_stalker_burst_arm_left_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmLeftPartVerts[1] = {
#include "assets/ivory_stalker_burst_arm_left_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmLeftVerts[10] = {
#include "assets/ivory_stalker_burst_arm_left_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmLeftNormals[17] = {
#include "assets/ivory_stalker_burst_arm_left_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmLeftStream[85] = {
#include "assets/ivory_stalker_burst_arm_left_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstArmLeft = {
    0,
    528,
    0,
    1,
    _gActor405800IvoryStalkerBurstArmLeftPartVerts,
    _gActor405800IvoryStalkerBurstArmLeftVerts,
    _gActor405800IvoryStalkerBurstArmLeftNormals,
    _gActor405800IvoryStalkerBurstArmLeftSkeleton,
    _gActor405800IvoryStalkerBurstArmLeftStream,
};

static TmdBone _gActor405800IvoryStalkerBurstHeadSkeleton[1] = {
#include "assets/ivory_stalker_burst_head_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstHeadPartVerts[1] = {
#include "assets/ivory_stalker_burst_head_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstHeadVerts[36] = {
#include "assets/ivory_stalker_burst_head_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstHeadNormals[50] = {
#include "assets/ivory_stalker_burst_head_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstHeadStream[342] = {
#include "assets/ivory_stalker_burst_head_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstHead = {
    0,
    2300,
    0,
    1,
    _gActor405800IvoryStalkerBurstHeadPartVerts,
    _gActor405800IvoryStalkerBurstHeadVerts,
    _gActor405800IvoryStalkerBurstHeadNormals,
    _gActor405800IvoryStalkerBurstHeadSkeleton,
    _gActor405800IvoryStalkerBurstHeadStream,
};

static TmdBone _gActor405800StalkerBurstTorsoSkeleton[1] = {
#include "assets/stalker_burst_torso_skeleton.inc"
};

static u32 _gActor405800StalkerBurstTorsoPartVerts[1] = {
#include "assets/stalker_burst_torso_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstTorsoVerts[55] = {
#include "assets/stalker_burst_torso_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstTorsoNormals[62] = {
#include "assets/stalker_burst_torso_normals.inc"
};

static u32 _gActor405800StalkerBurstTorsoStream[600] = {
#include "assets/stalker_burst_torso_stream.inc"
};

static TmdSource _gActor405800StalkerBurstTorso = {
    0,
    3988,
    0,
    1,
    _gActor405800StalkerBurstTorsoPartVerts,
    _gActor405800StalkerBurstTorsoVerts,
    _gActor405800StalkerBurstTorsoNormals,
    _gActor405800StalkerBurstTorsoSkeleton,
    _gActor405800StalkerBurstTorsoStream,
};

static TmdBone _gActor405800StalkerEffectSkeleton[1] = {
#include "assets/stalker_effect_skeleton.inc"
};

static u32 _gActor405800StalkerEffectPartVerts[1] = {
#include "assets/stalker_effect_partVerts.inc"
};

static SVECTOR _gActor405800StalkerEffectVerts[27] = {
#include "assets/stalker_effect_verts.inc"
};

static SVECTOR _gActor405800StalkerEffectNormals[34] = {
#include "assets/stalker_effect_normals.inc"
};

static u32 _gActor405800StalkerEffectStream[284] = {
#include "assets/stalker_effect_stream.inc"
};

static TmdSource _gActor405800StalkerEffect = {
    0,
    1860,
    0,
    1,
    _gActor405800StalkerEffectPartVerts,
    _gActor405800StalkerEffectVerts,
    _gActor405800StalkerEffectNormals,
    _gActor405800StalkerEffectSkeleton,
    _gActor405800StalkerEffectStream,
};

static TmdBone _gActor405800StalkerBurstHandLeftSkeleton[1] = {
#include "assets/stalker_burst_hand_left_skeleton.inc"
};

static u32 _gActor405800StalkerBurstHandLeftPartVerts[1] = {
#include "assets/stalker_burst_hand_left_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstHandLeftVerts[23] = {
#include "assets/stalker_burst_hand_left_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstHandLeftNormals[26] = {
#include "assets/stalker_burst_hand_left_normals.inc"
};

static u32 _gActor405800StalkerBurstHandLeftStream[211] = {
#include "assets/stalker_burst_hand_left_stream.inc"
};

static TmdSource _gActor405800StalkerBurstHandLeft = {
    0,
    1400,
    0,
    1,
    _gActor405800StalkerBurstHandLeftPartVerts,
    _gActor405800StalkerBurstHandLeftVerts,
    _gActor405800StalkerBurstHandLeftNormals,
    _gActor405800StalkerBurstHandLeftSkeleton,
    _gActor405800StalkerBurstHandLeftStream,
};

static TmdBone _gActor405800StalkerBurstFootRightSkeleton[1] = {
#include "assets/stalker_burst_foot_right_skeleton.inc"
};

static u32 _gActor405800StalkerBurstFootRightPartVerts[1] = {
#include "assets/stalker_burst_foot_right_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstFootRightVerts[19] = {
#include "assets/stalker_burst_foot_right_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstFootRightNormals[25] = {
#include "assets/stalker_burst_foot_right_normals.inc"
};

static u32 _gActor405800StalkerBurstFootRightStream[188] = {
#include "assets/stalker_burst_foot_right_stream.inc"
};

static TmdSource _gActor405800StalkerBurstFootRight = {
    0,
    1220,
    0,
    1,
    _gActor405800StalkerBurstFootRightPartVerts,
    _gActor405800StalkerBurstFootRightVerts,
    _gActor405800StalkerBurstFootRightNormals,
    _gActor405800StalkerBurstFootRightSkeleton,
    _gActor405800StalkerBurstFootRightStream,
};

DamageAttack D_actor_405800_801418F0[3] = {
    { 30, 3 },
    { 25, 10 },
    { 12, 2 },
};

EnemyParams D_actor_405800_801418FC = { D_actor_405800_801418F0, 1000, 600, 300, 15, 100, 8, 0, 0 };

static AnimationPackedPose _gActor405800Animation107B4Bank1[26] = {
#include "assets/actor_405800_animation_107B4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation107B4Bank4[323] = {
#include "assets/actor_405800_animation_107B4_bank4.inc"
};

static AnimationRecord _gActor405800Animation107B4Records[408] = {
#include "assets/actor_405800_animation_107B4_records.inc"
};

static u16 _gActor405800Animation107B4Indices[18] = {
#include "assets/actor_405800_animation_107B4_indices.inc"
};

static AnimationSet _gActor405800Animation107B4 = {
    _gActor405800Animation107B4Records,
    _gActor405800Animation107B4Indices,
    { NULL, _gActor405800Animation107B4Bank1, NULL, NULL, _gActor405800Animation107B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation10EECBank1[22] = {
#include "assets/actor_405800_animation_10EEC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation10EECBank4[156] = {
#include "assets/actor_405800_animation_10EEC_bank4.inc"
};

static AnimationRecord _gActor405800Animation10EECRecords[221] = {
#include "assets/actor_405800_animation_10EEC_records.inc"
};

static u16 _gActor405800Animation10EECIndices[18] = {
#include "assets/actor_405800_animation_10EEC_indices.inc"
};

static AnimationSet _gActor405800Animation10EEC = {
    _gActor405800Animation10EECRecords,
    _gActor405800Animation10EECIndices,
    { NULL, _gActor405800Animation10EECBank1, NULL, NULL, _gActor405800Animation10EECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation115D0Bank1[14] = {
#include "assets/actor_405800_animation_115D0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation115D0Bank4[157] = {
#include "assets/actor_405800_animation_115D0_bank4.inc"
};

static AnimationRecord _gActor405800Animation115D0Records[223] = {
#include "assets/actor_405800_animation_115D0_records.inc"
};

static u16 _gActor405800Animation115D0Indices[18] = {
#include "assets/actor_405800_animation_115D0_indices.inc"
};

static AnimationSet _gActor405800Animation115D0 = {
    _gActor405800Animation115D0Records,
    _gActor405800Animation115D0Indices,
    { NULL, _gActor405800Animation115D0Bank1, NULL, NULL, _gActor405800Animation115D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation11E68Bank1[25] = {
#include "assets/actor_405800_animation_11E68_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation11E68Bank4[191] = {
#include "assets/actor_405800_animation_11E68_bank4.inc"
};

static AnimationRecord _gActor405800Animation11E68Records[265] = {
#include "assets/actor_405800_animation_11E68_records.inc"
};

static u16 _gActor405800Animation11E68Indices[18] = {
#include "assets/actor_405800_animation_11E68_indices.inc"
};

static AnimationSet _gActor405800Animation11E68 = {
    _gActor405800Animation11E68Records,
    _gActor405800Animation11E68Indices,
    { NULL, _gActor405800Animation11E68Bank1, NULL, NULL, _gActor405800Animation11E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation12728Bank1[17] = {
#include "assets/actor_405800_animation_12728_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation12728Bank4[214] = {
#include "assets/actor_405800_animation_12728_bank4.inc"
};

static AnimationRecord _gActor405800Animation12728Records[276] = {
#include "assets/actor_405800_animation_12728_records.inc"
};

static u16 _gActor405800Animation12728Indices[18] = {
#include "assets/actor_405800_animation_12728_indices.inc"
};

static AnimationSet _gActor405800Animation12728 = {
    _gActor405800Animation12728Records,
    _gActor405800Animation12728Indices,
    { NULL, _gActor405800Animation12728Bank1, NULL, NULL, _gActor405800Animation12728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation130A0Bank1[19] = {
#include "assets/actor_405800_animation_130A0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation130A0Bank4[233] = {
#include "assets/actor_405800_animation_130A0_bank4.inc"
};

static AnimationRecord _gActor405800Animation130A0Records[297] = {
#include "assets/actor_405800_animation_130A0_records.inc"
};

static u16 _gActor405800Animation130A0Indices[18] = {
#include "assets/actor_405800_animation_130A0_indices.inc"
};

static AnimationSet _gActor405800Animation130A0 = {
    _gActor405800Animation130A0Records,
    _gActor405800Animation130A0Indices,
    { NULL, _gActor405800Animation130A0Bank1, NULL, NULL, _gActor405800Animation130A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation13574Bank1[9] = {
#include "assets/actor_405800_animation_13574_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation13574Bank4[114] = {
#include "assets/actor_405800_animation_13574_bank4.inc"
};

static AnimationRecord _gActor405800Animation13574Records[149] = {
#include "assets/actor_405800_animation_13574_records.inc"
};

static u16 _gActor405800Animation13574Indices[18] = {
#include "assets/actor_405800_animation_13574_indices.inc"
};

static AnimationSet _gActor405800Animation13574 = {
    _gActor405800Animation13574Records,
    _gActor405800Animation13574Indices,
    { NULL, _gActor405800Animation13574Bank1, NULL, NULL, _gActor405800Animation13574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation14154Bank1[23] = {
#include "assets/actor_405800_animation_14154_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation14154Bank4[295] = {
#include "assets/actor_405800_animation_14154_bank4.inc"
};

static AnimationRecord _gActor405800Animation14154Records[377] = {
#include "assets/actor_405800_animation_14154_records.inc"
};

static u16 _gActor405800Animation14154Indices[18] = {
#include "assets/actor_405800_animation_14154_indices.inc"
};

static AnimationSet _gActor405800Animation14154 = {
    _gActor405800Animation14154Records,
    _gActor405800Animation14154Indices,
    { NULL, _gActor405800Animation14154Bank1, NULL, NULL, _gActor405800Animation14154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation146ACBank1[9] = {
#include "assets/actor_405800_animation_146AC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation146ACBank4[125] = {
#include "assets/actor_405800_animation_146AC_bank4.inc"
};

static AnimationRecord _gActor405800Animation146ACRecords[171] = {
#include "assets/actor_405800_animation_146AC_records.inc"
};

static u16 _gActor405800Animation146ACIndices[18] = {
#include "assets/actor_405800_animation_146AC_indices.inc"
};

static AnimationSet _gActor405800Animation146AC = {
    _gActor405800Animation146ACRecords,
    _gActor405800Animation146ACIndices,
    { NULL, _gActor405800Animation146ACBank1, NULL, NULL, _gActor405800Animation146ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1506CBank1[18] = {
#include "assets/actor_405800_animation_1506C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1506CBank4[242] = {
#include "assets/actor_405800_animation_1506C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1506CRecords[309] = {
#include "assets/actor_405800_animation_1506C_records.inc"
};

static u16 _gActor405800Animation1506CIndices[18] = {
#include "assets/actor_405800_animation_1506C_indices.inc"
};

static AnimationSet _gActor405800Animation1506C = {
    _gActor405800Animation1506CRecords,
    _gActor405800Animation1506CIndices,
    { NULL, _gActor405800Animation1506CBank1, NULL, NULL, _gActor405800Animation1506CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation15498Bank1[9] = {
#include "assets/actor_405800_animation_15498_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation15498Bank4[95] = {
#include "assets/actor_405800_animation_15498_bank4.inc"
};

static AnimationRecord _gActor405800Animation15498Records[126] = {
#include "assets/actor_405800_animation_15498_records.inc"
};

static u16 _gActor405800Animation15498Indices[18] = {
#include "assets/actor_405800_animation_15498_indices.inc"
};

static AnimationSet _gActor405800Animation15498 = {
    _gActor405800Animation15498Records,
    _gActor405800Animation15498Indices,
    { NULL, _gActor405800Animation15498Bank1, NULL, NULL, _gActor405800Animation15498Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation15A04Bank1[9] = {
#include "assets/actor_405800_animation_15A04_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation15A04Bank4[136] = {
#include "assets/actor_405800_animation_15A04_bank4.inc"
};

static AnimationRecord _gActor405800Animation15A04Records[165] = {
#include "assets/actor_405800_animation_15A04_records.inc"
};

static u16 _gActor405800Animation15A04Indices[18] = {
#include "assets/actor_405800_animation_15A04_indices.inc"
};

static AnimationSet _gActor405800Animation15A04 = {
    _gActor405800Animation15A04Records,
    _gActor405800Animation15A04Indices,
    { NULL, _gActor405800Animation15A04Bank1, NULL, NULL, _gActor405800Animation15A04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation163BCBank1[15] = {
#include "assets/actor_405800_animation_163BC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation163BCBank4[227] = {
#include "assets/actor_405800_animation_163BC_bank4.inc"
};

static AnimationRecord _gActor405800Animation163BCRecords[331] = {
#include "assets/actor_405800_animation_163BC_records.inc"
};

static u16 _gActor405800Animation163BCIndices[18] = {
#include "assets/actor_405800_animation_163BC_indices.inc"
};

static AnimationSet _gActor405800Animation163BC = {
    _gActor405800Animation163BCRecords,
    _gActor405800Animation163BCIndices,
    { NULL, _gActor405800Animation163BCBank1, NULL, NULL, _gActor405800Animation163BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation16928Bank1[10] = {
#include "assets/actor_405800_animation_16928_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation16928Bank4[129] = {
#include "assets/actor_405800_animation_16928_bank4.inc"
};

static AnimationRecord _gActor405800Animation16928Records[169] = {
#include "assets/actor_405800_animation_16928_records.inc"
};

static u16 _gActor405800Animation16928Indices[18] = {
#include "assets/actor_405800_animation_16928_indices.inc"
};

static AnimationSet _gActor405800Animation16928 = {
    _gActor405800Animation16928Records,
    _gActor405800Animation16928Indices,
    { NULL, _gActor405800Animation16928Bank1, NULL, NULL, _gActor405800Animation16928Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation176B4Bank1[16] = {
#include "assets/actor_405800_animation_176B4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation176B4Bank4[363] = {
#include "assets/actor_405800_animation_176B4_bank4.inc"
};

static AnimationRecord _gActor405800Animation176B4Records[437] = {
#include "assets/actor_405800_animation_176B4_records.inc"
};

static u16 _gActor405800Animation176B4Indices[18] = {
#include "assets/actor_405800_animation_176B4_indices.inc"
};

static AnimationSet _gActor405800Animation176B4 = {
    _gActor405800Animation176B4Records,
    _gActor405800Animation176B4Indices,
    { NULL, _gActor405800Animation176B4Bank1, NULL, NULL, _gActor405800Animation176B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation17D44Bank1[12] = {
#include "assets/actor_405800_animation_17D44_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation17D44Bank4[167] = {
#include "assets/actor_405800_animation_17D44_bank4.inc"
};

static AnimationRecord _gActor405800Animation17D44Records[198] = {
#include "assets/actor_405800_animation_17D44_records.inc"
};

static u16 _gActor405800Animation17D44Indices[18] = {
#include "assets/actor_405800_animation_17D44_indices.inc"
};

static AnimationSet _gActor405800Animation17D44 = {
    _gActor405800Animation17D44Records,
    _gActor405800Animation17D44Indices,
    { NULL, _gActor405800Animation17D44Bank1, NULL, NULL, _gActor405800Animation17D44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation180CCBank1[5] = {
#include "assets/actor_405800_animation_180CC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation180CCBank4[79] = {
#include "assets/actor_405800_animation_180CC_bank4.inc"
};

static AnimationRecord _gActor405800Animation180CCRecords[113] = {
#include "assets/actor_405800_animation_180CC_records.inc"
};

static u16 _gActor405800Animation180CCIndices[18] = {
#include "assets/actor_405800_animation_180CC_indices.inc"
};

static AnimationSet _gActor405800Animation180CC = {
    _gActor405800Animation180CCRecords,
    _gActor405800Animation180CCIndices,
    { NULL, _gActor405800Animation180CCBank1, NULL, NULL, _gActor405800Animation180CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1866CBank1[11] = {
#include "assets/actor_405800_animation_1866C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1866CBank4[139] = {
#include "assets/actor_405800_animation_1866C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1866CRecords[169] = {
#include "assets/actor_405800_animation_1866C_records.inc"
};

static u16 _gActor405800Animation1866CIndices[18] = {
#include "assets/actor_405800_animation_1866C_indices.inc"
};

static AnimationSet _gActor405800Animation1866C = {
    _gActor405800Animation1866CRecords,
    _gActor405800Animation1866CIndices,
    { NULL, _gActor405800Animation1866CBank1, NULL, NULL, _gActor405800Animation1866CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation18AE8Bank1[10] = {
#include "assets/actor_405800_animation_18AE8_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation18AE8Bank4[104] = {
#include "assets/actor_405800_animation_18AE8_bank4.inc"
};

static AnimationRecord _gActor405800Animation18AE8Records[134] = {
#include "assets/actor_405800_animation_18AE8_records.inc"
};

static u16 _gActor405800Animation18AE8Indices[18] = {
#include "assets/actor_405800_animation_18AE8_indices.inc"
};

static AnimationSet _gActor405800Animation18AE8 = {
    _gActor405800Animation18AE8Records,
    _gActor405800Animation18AE8Indices,
    { NULL, _gActor405800Animation18AE8Bank1, NULL, NULL, _gActor405800Animation18AE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation19654Bank1[40] = {
#include "assets/actor_405800_animation_19654_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation19654Bank4[266] = {
#include "assets/actor_405800_animation_19654_bank4.inc"
};

static AnimationRecord _gActor405800Animation19654Records[326] = {
#include "assets/actor_405800_animation_19654_records.inc"
};

static u16 _gActor405800Animation19654Indices[18] = {
#include "assets/actor_405800_animation_19654_indices.inc"
};

static AnimationSet _gActor405800Animation19654 = {
    _gActor405800Animation19654Records,
    _gActor405800Animation19654Indices,
    { NULL, _gActor405800Animation19654Bank1, NULL, NULL, _gActor405800Animation19654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation199C0Bank1[6] = {
#include "assets/actor_405800_animation_199C0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation199C0Bank4[78] = {
#include "assets/actor_405800_animation_199C0_bank4.inc"
};

static AnimationRecord _gActor405800Animation199C0Records[104] = {
#include "assets/actor_405800_animation_199C0_records.inc"
};

static u16 _gActor405800Animation199C0Indices[18] = {
#include "assets/actor_405800_animation_199C0_indices.inc"
};

static AnimationSet _gActor405800Animation199C0 = {
    _gActor405800Animation199C0Records,
    _gActor405800Animation199C0Indices,
    { NULL, _gActor405800Animation199C0Bank1, NULL, NULL, _gActor405800Animation199C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation19EC0Bank1[11] = {
#include "assets/actor_405800_animation_19EC0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation19EC0Bank4[113] = {
#include "assets/actor_405800_animation_19EC0_bank4.inc"
};

static AnimationRecord _gActor405800Animation19EC0Records[155] = {
#include "assets/actor_405800_animation_19EC0_records.inc"
};

static u16 _gActor405800Animation19EC0Indices[18] = {
#include "assets/actor_405800_animation_19EC0_indices.inc"
};

static AnimationSet _gActor405800Animation19EC0 = {
    _gActor405800Animation19EC0Records,
    _gActor405800Animation19EC0Indices,
    { NULL, _gActor405800Animation19EC0Bank1, NULL, NULL, _gActor405800Animation19EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A4BCBank1[10] = {
#include "assets/actor_405800_animation_1A4BC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A4BCBank4[150] = {
#include "assets/actor_405800_animation_1A4BC_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A4BCRecords[184] = {
#include "assets/actor_405800_animation_1A4BC_records.inc"
};

static u16 _gActor405800Animation1A4BCIndices[18] = {
#include "assets/actor_405800_animation_1A4BC_indices.inc"
};

static AnimationSet _gActor405800Animation1A4BC = {
    _gActor405800Animation1A4BCRecords,
    _gActor405800Animation1A4BCIndices,
    { NULL, _gActor405800Animation1A4BCBank1, NULL, NULL, _gActor405800Animation1A4BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A680Bank1[2] = {
#include "assets/actor_405800_animation_1A680_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A680Bank4[16] = {
#include "assets/actor_405800_animation_1A680_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A680Records[72] = {
#include "assets/actor_405800_animation_1A680_records.inc"
};

static u16 _gActor405800Animation1A680Indices[18] = {
#include "assets/actor_405800_animation_1A680_indices.inc"
};

static AnimationSet _gActor405800Animation1A680 = {
    _gActor405800Animation1A680Records,
    _gActor405800Animation1A680Indices,
    { NULL, _gActor405800Animation1A680Bank1, NULL, NULL, _gActor405800Animation1A680Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A844Bank1[2] = {
#include "assets/actor_405800_animation_1A844_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A844Bank4[16] = {
#include "assets/actor_405800_animation_1A844_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A844Records[72] = {
#include "assets/actor_405800_animation_1A844_records.inc"
};

static u16 _gActor405800Animation1A844Indices[18] = {
#include "assets/actor_405800_animation_1A844_indices.inc"
};

static AnimationSet _gActor405800Animation1A844 = {
    _gActor405800Animation1A844Records,
    _gActor405800Animation1A844Indices,
    { NULL, _gActor405800Animation1A844Bank1, NULL, NULL, _gActor405800Animation1A844Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1AA08Bank1[2] = {
#include "assets/actor_405800_animation_1AA08_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1AA08Bank4[16] = {
#include "assets/actor_405800_animation_1AA08_bank4.inc"
};

static AnimationRecord _gActor405800Animation1AA08Records[72] = {
#include "assets/actor_405800_animation_1AA08_records.inc"
};

static u16 _gActor405800Animation1AA08Indices[18] = {
#include "assets/actor_405800_animation_1AA08_indices.inc"
};

static AnimationSet _gActor405800Animation1AA08 = {
    _gActor405800Animation1AA08Records,
    _gActor405800Animation1AA08Indices,
    { NULL, _gActor405800Animation1AA08Bank1, NULL, NULL, _gActor405800Animation1AA08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1ABCCBank1[2] = {
#include "assets/actor_405800_animation_1ABCC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1ABCCBank4[16] = {
#include "assets/actor_405800_animation_1ABCC_bank4.inc"
};

static AnimationRecord _gActor405800Animation1ABCCRecords[72] = {
#include "assets/actor_405800_animation_1ABCC_records.inc"
};

static u16 _gActor405800Animation1ABCCIndices[18] = {
#include "assets/actor_405800_animation_1ABCC_indices.inc"
};

static AnimationSet _gActor405800Animation1ABCC = {
    _gActor405800Animation1ABCCRecords,
    _gActor405800Animation1ABCCIndices,
    { NULL, _gActor405800Animation1ABCCBank1, NULL, NULL, _gActor405800Animation1ABCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1B074Bank1[8] = {
#include "assets/actor_405800_animation_1B074_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1B074Bank4[105] = {
#include "assets/actor_405800_animation_1B074_bank4.inc"
};

static AnimationRecord _gActor405800Animation1B074Records[150] = {
#include "assets/actor_405800_animation_1B074_records.inc"
};

static u16 _gActor405800Animation1B074Indices[18] = {
#include "assets/actor_405800_animation_1B074_indices.inc"
};

static AnimationSet _gActor405800Animation1B074 = {
    _gActor405800Animation1B074Records,
    _gActor405800Animation1B074Indices,
    { NULL, _gActor405800Animation1B074Bank1, NULL, NULL, _gActor405800Animation1B074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1C248Bank1[36] = {
#include "assets/actor_405800_animation_1C248_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1C248Bank4[451] = {
#include "assets/actor_405800_animation_1C248_bank4.inc"
};

static AnimationRecord _gActor405800Animation1C248Records[563] = {
#include "assets/actor_405800_animation_1C248_records.inc"
};

static u16 _gActor405800Animation1C248Indices[18] = {
#include "assets/actor_405800_animation_1C248_indices.inc"
};

static AnimationSet _gActor405800Animation1C248 = {
    _gActor405800Animation1C248Records,
    _gActor405800Animation1C248Indices,
    { NULL, _gActor405800Animation1C248Bank1, NULL, NULL, _gActor405800Animation1C248Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1CAE4Bank1[20] = {
#include "assets/actor_405800_animation_1CAE4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1CAE4Bank4[213] = {
#include "assets/actor_405800_animation_1CAE4_bank4.inc"
};

static AnimationRecord _gActor405800Animation1CAE4Records[259] = {
#include "assets/actor_405800_animation_1CAE4_records.inc"
};

static u16 _gActor405800Animation1CAE4Indices[18] = {
#include "assets/actor_405800_animation_1CAE4_indices.inc"
};

static AnimationSet _gActor405800Animation1CAE4 = {
    _gActor405800Animation1CAE4Records,
    _gActor405800Animation1CAE4Indices,
    { NULL, _gActor405800Animation1CAE4Bank1, NULL, NULL, _gActor405800Animation1CAE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1D83CBank1[25] = {
#include "assets/actor_405800_animation_1D83C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1D83CBank4[343] = {
#include "assets/actor_405800_animation_1D83C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1D83CRecords[416] = {
#include "assets/actor_405800_animation_1D83C_records.inc"
};

static u16 _gActor405800Animation1D83CIndices[20] = {
#include "assets/actor_405800_animation_1D83C_indices.inc"
};

static AnimationSet _gActor405800Animation1D83C = {
    _gActor405800Animation1D83CRecords,
    _gActor405800Animation1D83CIndices,
    { NULL, _gActor405800Animation1D83CBank1, NULL, NULL, _gActor405800Animation1D83CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1E084Bank1[15] = {
#include "assets/actor_405800_animation_1E084_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1E084Bank4[203] = {
#include "assets/actor_405800_animation_1E084_bank4.inc"
};

static AnimationRecord _gActor405800Animation1E084Records[262] = {
#include "assets/actor_405800_animation_1E084_records.inc"
};

static u16 _gActor405800Animation1E084Indices[20] = {
#include "assets/actor_405800_animation_1E084_indices.inc"
};

static AnimationSet _gActor405800Animation1E084 = {
    _gActor405800Animation1E084Records,
    _gActor405800Animation1E084Indices,
    { NULL, _gActor405800Animation1E084Bank1, NULL, NULL, _gActor405800Animation1E084Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1E25CBank1[2] = {
#include "assets/actor_405800_animation_1E25C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1E25CBank4[16] = {
#include "assets/actor_405800_animation_1E25C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1E25CRecords[76] = {
#include "assets/actor_405800_animation_1E25C_records.inc"
};

static u16 _gActor405800Animation1E25CIndices[20] = {
#include "assets/actor_405800_animation_1E25C_indices.inc"
};

static AnimationSet _gActor405800Animation1E25C = {
    _gActor405800Animation1E25CRecords,
    _gActor405800Animation1E25CIndices,
    { NULL, _gActor405800Animation1E25CBank1, NULL, NULL, _gActor405800Animation1E25CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1EDF4Bank1[20] = {
#include "assets/actor_405800_animation_1EDF4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1EDF4Bank4[292] = {
#include "assets/actor_405800_animation_1EDF4_bank4.inc"
};

static AnimationRecord _gActor405800Animation1EDF4Records[370] = {
#include "assets/actor_405800_animation_1EDF4_records.inc"
};

static u16 _gActor405800Animation1EDF4Indices[20] = {
#include "assets/actor_405800_animation_1EDF4_indices.inc"
};

static AnimationSet _gActor405800Animation1EDF4 = {
    _gActor405800Animation1EDF4Records,
    _gActor405800Animation1EDF4Indices,
    { NULL, _gActor405800Animation1EDF4Bank1, NULL, NULL, _gActor405800Animation1EDF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1F5B0Bank1[15] = {
#include "assets/actor_405800_animation_1F5B0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1F5B0Bank4[191] = {
#include "assets/actor_405800_animation_1F5B0_bank4.inc"
};

static AnimationRecord _gActor405800Animation1F5B0Records[239] = {
#include "assets/actor_405800_animation_1F5B0_records.inc"
};

static u16 _gActor405800Animation1F5B0Indices[20] = {
#include "assets/actor_405800_animation_1F5B0_indices.inc"
};

static AnimationSet _gActor405800Animation1F5B0 = {
    _gActor405800Animation1F5B0Records,
    _gActor405800Animation1F5B0Indices,
    { NULL, _gActor405800Animation1F5B0Bank1, NULL, NULL, _gActor405800Animation1F5B0Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_405800_801513F8[6] = {
    NULL,
    &_gActor405800Animation1D83C,
    &_gActor405800Animation1E084,
    &_gActor405800Animation1E25C,
    &_gActor405800Animation1EDF4,
    &_gActor405800Animation1F5B0,
};

AnimationSet* D_actor_405800_80151410[35] = {
    NULL,
    &_gActor405800Animation107B4,
    &_gActor405800Animation10EEC,
    &_gActor405800Animation115D0,
    &_gActor405800Animation11E68,
    NULL,
    NULL,
    &_gActor405800Animation12728,
    &_gActor405800Animation130A0,
    &_gActor405800Animation13574,
    &_gActor405800Animation14154,
    &_gActor405800Animation146AC,
    &_gActor405800Animation1506C,
    &_gActor405800Animation15498,
    &_gActor405800Animation15A04,
    &_gActor405800Animation163BC,
    &_gActor405800Animation16928,
    &_gActor405800Animation176B4,
    &_gActor405800Animation17D44,
    &_gActor405800Animation180CC,
    &_gActor405800Animation1866C,
    &_gActor405800Animation18AE8,
    &_gActor405800Animation19654,
    NULL,
    NULL,
    &_gActor405800Animation199C0,
    &_gActor405800Animation19EC0,
    &_gActor405800Animation1A4BC,
    &_gActor405800Animation1A680,
    &_gActor405800Animation1A844,
    &_gActor405800Animation1AA08,
    &_gActor405800Animation1ABCC,
    &_gActor405800Animation1B074,
    &_gActor405800Animation1C248,
    &_gActor405800Animation1CAE4,
};

TaskMessageEntry D_actor_405800_8015149C[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, stalkerZebraIvorySetMoveMode },
    { 2014, func_actor_405800_801388C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_405800_801514B4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_405800_801388D4, { .model = &_gActor405800IvoryStalkerBurstArmLeft } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_405800_801388DC, { .model = &_gActor405800IvoryStalkerBurstArmRight } },
};

TaskDesc D_actor_405800_801514CC = { { { TASK_BODY_TMD, 96 } }, func_actor_405800_80138634, { .model = &_gActor405800IvoryStalkerBody } };

u8 gStalkerZebraIvoryResumeClips[35] = { 26, 26, 26, 27, 27, 15, 15, 26, 26, 26, 26, 27, 27, 15, 15, 26, 26, 27, 27, 30, 27, 26, 26, 26, 26, 26, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

static __inline__ void Actor405800_ProjectPart(GfxCoord* part);
static __inline__ void _actor405800SetBehaviour(Task* task, s16 id);

#include "../../shared/stalker_zebra_ivory_inlines.inc.c"

#include "../../shared/frame_capture.inc.c"

static void func_actor_405800_80132670(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;

    work                        = (_Actor405800IvoryStalkerWork*)arg0->work;
    work->body.coord            = &arg0->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0x110;
    work->body.key              = 0x3003A;
    work->body.radius           = 0x2F0;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->gridBody.coord            = arg0->extra.tmd->coords;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x220;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x3003A;
    work->gridBody.radius           = 0x460;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->capsule.ends[0].vz          = 0xBB8;
    work->capsule.end0Radius          = 0xA;
    work->capsule.end1Radius          = 0xA;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.contacts            = work->capsuleContacts;
    work->gridBody.flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->capsuleBody.coord           = arg0->extra.tmd->coords;
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = -0x190;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0x30005;
    work->capsuleBody.radius          = 0;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->capsuleBody);
    worldCollisionInitContacts(work->capsuleContacts, ARRAY_SIZE(work->capsuleContacts), 0);
    work->capsuleBody.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->rightArmOuter.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->rightArmOuter.coord            = &arg0->extra.tmd->coords[7];
    work->rightArmOuter.context.contacts = work->rightArmContacts;
    work->rightArmOuter.pos.vx           = -0x460;
    work->rightArmOuter.pos.vy           = 0;
    work->rightArmOuter.pos.vz           = 0;
    work->rightArmOuter.radius           = 0x290;
    work->rightArmOuter.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->rightArmOuter);
    worldCollisionInitContacts(work->rightArmContacts, ARRAY_SIZE(work->rightArmContacts), 0);
    work->rightArmOuter.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->rightArmInner.coord            = &arg0->extra.tmd->coords[7];
    work->rightArmInner.context.contacts = work->rightArmContacts;
    work->rightArmInner.pos.vx           = -0x200;
    work->rightArmInner.pos.vy           = 0;
    work->rightArmInner.pos.vz           = 0;
    work->rightArmInner.radius           = 0x250;
    work->rightArmInner.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->rightArmInner);
    worldCollisionInitContacts(work->rightArmContacts, ARRAY_SIZE(work->rightArmContacts), 0);
    work->rightArmInner.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmOuter.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->leftArmOuter.coord            = &arg0->extra.tmd->coords[10];
    work->leftArmOuter.context.contacts = work->leftArmContacts;
    work->leftArmOuter.pos.vx           = 0x460;
    work->leftArmOuter.pos.vy           = 0;
    work->leftArmOuter.pos.vz           = 0;
    work->leftArmOuter.radius           = 0x290;
    work->leftArmOuter.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->leftArmOuter);
    worldCollisionInitContacts(work->leftArmContacts, ARRAY_SIZE(work->leftArmContacts), 0);
    work->leftArmOuter.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmInner.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->leftArmInner.coord            = &arg0->extra.tmd->coords[10];
    work->leftArmInner.context.contacts = work->leftArmContacts;
    work->leftArmInner.pos.vx           = 0x200;
    work->leftArmInner.pos.vy           = 0;
    work->leftArmInner.pos.vz           = 0;
    work->leftArmInner.radius           = 0x250;
    work->leftArmInner.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->leftArmInner);
    worldCollisionInitContacts(work->leftArmContacts, ARRAY_SIZE(work->leftArmContacts), 0);
    work->leftArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

#include "../../shared/limb_shadows_segment.inc.c"

static void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2)
{
    _limbShadowDrawSegment(arg0, 3, 9, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 9, 0xA, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 0xA, 0xB, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 3, 6, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 6, 7, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 7, 8, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 1, 5, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 1, 0xC, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 0xC, 0xD, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 0xD, 0xE, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 1, 0xF, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 0xF, 0x10, 0x100, arg1, arg2);
    _limbShadowDrawSegment(arg0, 0x10, 0x11, 0x100, arg1, arg2);
}

static void func_actor_405800_80132FE0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    TmdObject*                    model;
    GfxCoord*                     coord;
    VECTOR                        scale;
    SVECTOR                       rot;

    work                      = (_Actor405800IvoryStalkerWork*)arg0->work;
    model                     = arg0->extra.tmd;
    coord                     = model->coords;
    work->cloakLevel          = (u16)work->cloakLevel + ((s16)(0xFF - (u16)work->cloakLevel) >> 4);
    work->colorBlend          = (u16)work->colorBlend + ((s16)(-(u16)work->colorBlend) >> 4);
    work->shadowShade         = (u16)work->shadowShade + (-work->shadowShade >> 2);
    model->shading.colorBlend = work->colorBlend;
    modelLightingSetLayerMaterials(work->cloakLevel);
    work->corpseScaleY -= 0x30;
    scale.vx            = ONE;
    scale.vy            = work->corpseScaleY;
    scale.vz            = ONE;
    coord->coord        = work->savedRootMtx;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->stateFrames++;
    if (work->stateFrames == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, coord, 3, &rot);
    }
    if (work->stateFrames >= 0x41) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}

static void func_actor_405800_8013315C(Task* arg0)
{
    Enemy*                        enemy;
    _Actor405800IvoryStalkerWork* work;
    TmdObject*                    model;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    // A negative request byte is `ACTOR_405800_CLOAK_RUNNING`.
    if ((s8)work->cloakRequest < 0) {
        if (!(work->cloakRequest & ACTOR_405800_CLOAK_SHOW)) {
            // Hide: raise the cloak, hold it, then fade the body and its shadows out.
            switch (work->cloakPhase) {
                case 0:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16)(0xFF - (u16)work->cloakLevel) >> 2);
                    if (work->cloakLevel >= 0xF8) {
                        work->cloakLevel = 0xFF;
                        work->cloakTimer = 0;
                        work->cloakPhase++;
                    }
                    modelLightingSetLayerMaterials(work->cloakLevel);
                    break;
                case 1:
                    work->cloakTimer++;
                    if (work->hideHoldFrames < work->cloakTimer) {
                        work->cloakPhase++;
                    }
                    break;
                case 2:
                    work->colorBlend  = (u16)work->colorBlend + ((s16)(-(u16)work->colorBlend) >> 2);
                    work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 2);
                    if (work->colorBlend == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        if (work->field_897 == 0) {
                            enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
                        }
                        work->shadowShade  = 0;
                        work->cloakRequest = 0;
                    }
                    model->shading.colorBlend = work->colorBlend;
                    break;
            }
        } else {
            // Show: fade the body and its shadows in, hold, then drop the cloak.
            switch (work->cloakPhase) {
                case 0:
                    enemy->node.state.parts.flags = 0;
                    if (work->field_897 == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    }
                    work->colorBlend  = (u16)work->colorBlend + ((s16)(TMD_OBJECT_COLOR_BLEND_ONE - (u16)work->colorBlend) >> 2);
                    work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 2);
                    if (work->colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE - 16) {
                        work->shadowShade = 0xFF;
                        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
                        work->cloakTimer  = 0;
                        work->cloakPhase++;
                    }
                    model->shading.colorBlend = work->colorBlend;
                    break;
                case 1:
                    work->cloakTimer++;
                    if (work->cloakTimer >= 0x11) {
                        work->cloakPhase++;
                    }
                    break;
                case 2:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16)(-(u16)work->cloakLevel) >> 2);
                    if (work->cloakLevel < 9) {
                        work->cloakLevel   = 0;
                        work->cloakRequest = 0;
                        func_actor_405800_8013340C(arg0);
                        if (work->hideCooldown == 0) {
                            work->hideCooldown = work->hideCooldownReset;
                        }
                    }
                    modelLightingSetLayerMaterials(work->cloakLevel);
                    break;
            }
        }
    }
    if (work->hideCooldown > 0) {
        work->hideCooldown--;
    }
}

static void func_actor_405800_8013340C(Task* arg0)
{
    Enemy*                        enemy;
    _Actor405800IvoryStalkerWork* work;
    s16                           hp;
    s32                           maxHp;
    s32                           quarter;

    enemy   = (Enemy*)arg0->spawnArg2.pointer;
    hp      = enemy->hp;
    work    = (_Actor405800IvoryStalkerWork*)arg0->work;
    maxHp   = enemy->hpMax << 0x10;
    quarter = maxHp >> 0x12;
    if ((quarter + (maxHp >> 0x11)) < hp) {
        work->hideHoldFrames    = 0x10;
        work->hideCooldownReset = 0;
        return;
    }
    if (quarter < hp) {
        work->hideHoldFrames    = 0x20;
        work->hideCooldownReset = 0x40;
        return;
    }
    if ((maxHp >> 0x13) < hp) {
        work->hideHoldFrames    = 0x30;
        work->hideCooldownReset = 0x80;
        return;
    }
    if ((maxHp >> 0x14) < hp) {
        work->hideHoldFrames    = 0x40;
        work->hideCooldownReset = 0xC0;
        return;
    }
    work->hideHoldFrames    = 0x50;
    work->hideCooldownReset = 0x100;
}

static void func_actor_405800_801334B8(Task* arg0)
{
    TmdObject*                    model;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* w2;
    _Actor405800IvoryStalkerWork* w3;
    _Actor405800IvoryStalkerWork* w4;
    TmdObject*                    extra;
    u32                           rnd;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    coord = model->coords;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 8, 0, 0)) {
        enemyDestroy(enemy, arg0);
        return;
    }
    arg0->work = memCalloc(sizeof(_Actor405800IvoryStalkerWork), false);
    work       = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work == NULL) {
        enemyDestroy(enemy, arg0);
        return;
    }
    gStageSceneMusicEntry = 2;
    model->lightMtx       = &work->lightMtx;
    model->colorMtx       = &work->colorMtx;
    model->flags          = 0;
    arg0->msgTable        = D_actor_405800_8015149C;
    enemy->field_4        = &coord->coord;
    enemy->field_48       = 0;
    enemy->bodyPos.vx     = 0;
    enemy->bodyPos.vy     = 0;
    enemy->bodyPos.vz     = 0;
    enemy->coord          = &arg0->extra.tmd->coords[3];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->param                  = &D_actor_405800_801418FC;
    enemy->recs                   = work->bodyContacts;
    work->effectArg.coord         = &arg0->extra.tmd->coords[3];
    work->effectArg.spawnArgLo    = 0x100;
    work->effectArg.spawnArgHi    = 2;
    enemy->hp = enemy->hpMax = D_actor_405800_801418FC.hpMax;
    animationInitContext(&work->rig.anim, D_actor_405800_80151410, model, work->rig.poses, work->rig.slots);

    w2              = (_Actor405800IvoryStalkerWork*)arg0->work;
    w2->animStep    = 0x10;
    w2->animClip    = 1;
    w2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;

    _stalkerZebraIvoryTickAnimInline(arg0);

    coord->parent = &gGfxViewCoord;
    func_actor_405800_80132670(arg0);
    func_actor_405800_80135780(arg0);
    (sceneAcquireBattleRef)(0);
    w3           = (_Actor405800IvoryStalkerWork*)arg0->work;
    w3->state    = 0;
    w3->subState = 0;
    if (gGameSession->location.loc.warp == 1) {
        coord->coord.t[0] = 0x14B4;
        coord->coord.t[2] = 0xD7A;
        coord->coord.t[1] = 0;
        work->yaw         = 0x400;
    } else {
        coord->coord.t[0] = 0x514;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0x251C;
        work->yaw         = 0;
    }
    work->spawnX       = coord->coord.t[0];
    work->floorY       = coord->coord.t[1];
    work->spawnZ       = coord->coord.t[2];
    rnd                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = rnd;
    work->frameCount   = rnd >> 0x10;
    work->shadowHeight = work->floorY;
    w4                 = (_Actor405800IvoryStalkerWork*)arg0->work;
    extra              = arg0->extra.tmd;
    w4->cloakLevel     = 0xFF;
    w4->colorBlend     = 0;
    w4->shadowShade    = 0;
    w4->hideHoldFrames = 0x10;
    modelLightingSetLayerMaterials(w4->cloakLevel);
    extra->shading.colorBlend = w4->colorBlend;
    w3                        = (_Actor405800IvoryStalkerWork*)arg0->work;
    arg0->state               = 1;
    w3->state                 = 0;
    w3->subState              = 0;
}

/// Behaviour handlers `func_actor_405800_80138698` runs by `state`.
static const TaskFuncTable12 D_actor_405800_80131E24 = {
    {
        func_actor_405800_801388E4,
        stalkerZebraIvoryResumeClip,
        stalkerZebraIvoryAnimateUntilDone,
        func_actor_405800_80138A70,
        func_actor_405800_80138B50,
        func_actor_405800_80132FE0,
        func_actor_405800_80138BD4,
        func_actor_405800_80138BEC,
        func_actor_405800_80138C30,
        func_actor_405800_80138CF0,
        func_actor_405800_80138D54,
        func_actor_405800_80138E20,
    },
};

/// The task's four state handlers, run by `func_actor_405800_80138634`.
static const TaskFuncTable4 D_actor_405800_80131E54 = {
    {
        func_actor_405800_801334B8,
        func_actor_405800_80133800,
        func_actor_405800_80138698,
        func_actor_405800_80138788,
    },
};

/// Behaviour handlers `func_actor_405800_80133800` runs by `state`.
static const TaskFuncTable18 D_actor_405800_80131E64 = {
    {
        func_actor_405800_80137A14,
        func_actor_405800_80133CD0,
        func_actor_405800_80137A60,
        func_actor_405800_80137B34,
        func_actor_405800_80137B9C,
        func_actor_405800_80137C04,
        func_actor_405800_80137C78,
        func_actor_405800_80137CEC,
        func_actor_405800_80137D60,
        func_actor_405800_80137DE4,
        func_actor_405800_80137E64,
        func_actor_405800_80137EF0,
        func_actor_405800_80137F58,
        func_actor_405800_80137FCC,
        func_actor_405800_80138040,
        stalkerZebraIvoryRunSubStates,
        func_actor_405800_80138154,
        func_actor_405800_801381BC,
    },
};

/// Sub-state handlers of `func_actor_405800_80137C04`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EAC = {
    {
        func_actor_405800_80139358,
        func_actor_405800_801393E8,
        func_actor_405800_801394E4,
    },
};

/// Sub-state handlers of `func_actor_405800_80137D60`, by `subState`.
static const TaskFuncTable5 D_actor_405800_80131EB8 = {
    {
        func_actor_405800_8013967C,
        func_actor_405800_801340E0,
        func_actor_405800_80134314,
        func_actor_405800_8013471C,
        stalkerZebraIvoryReleaseHold,
    },
};

/// Sub-state handlers of `func_actor_405800_80137DE4`, by `subState`.
static const TaskFuncTable4 D_actor_405800_80131ECC = {
    {
        func_actor_405800_801397B8,
        func_actor_405800_801348E4,
        func_actor_405800_80134A64,
        stalkerZebraIvoryWaitClip,
    },
};

/// Sub-state handlers of `func_actor_405800_80137F58`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EDC = {
    {
        func_actor_405800_80139880,
        func_actor_405800_80134E80,
        func_actor_405800_801398C0,
    },
};

/// Sub-state handlers of `func_actor_405800_80137FCC`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EE8 = {
    {
        func_actor_405800_80139928,
        func_actor_405800_801351BC,
        func_actor_405800_801399C4,
    },
};

/// Sub-state handlers of `func_actor_405800_80138040`, by `subState`.
static const TaskFuncTable4 D_actor_405800_80131EF4 = {
    {
        func_actor_405800_80139AC4,
        func_actor_405800_80139B3C,
        func_actor_405800_80135558,
        stalkerZebraIvoryWaitClipThenRest,
    },
};

/// Sub-state handlers of `stalkerZebraIvoryRunSubStates`, by `subState`.
static const TaskFuncTable3 gStalkerZebraIvorySubStates = {
    {
        func_actor_405800_80139C98,
        func_actor_405800_80139D24,
        func_actor_405800_80139DC0,
    },
};

static __inline__ void Actor405800_ProjectPart(GfxCoord* part)
{
    void**                   scratch;
    u8*                      head;
    ActorOriginDepthScratch* block;
    SVECTOR*                 vec;
    MATRIX*                  wm;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch));
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->origin.vx               = 0;
    block->origin.vy               = 0;
    block->origin.vz               = 0;
    actorRenderComposeCoord(part);
    vec = &block->origin;
    wm  = &part->workm;
    gte_SetRotMatrix(wm);
    gte_SetTransMatrix(wm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->screenPos);
    gte_stdp(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->depthCue);
    gte_stflg(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->flag);
    gte_stszotz(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    frameCaptureQueue(block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOriginDepthScratch);
}

static void func_actor_405800_80133800(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor405800IvoryStalkerWork* work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    GfxCoord*                     coord = model->coords;
    Enemy*                        enemy = (Enemy*)arg0->spawnArg2.pointer;
    GfxCoord*                     part  = &coord[2];
    GfxCoord*                     root  = coord;
    TaskFuncTable18               fns   = D_actor_405800_80131E64;
    _Actor405800IvoryStalkerWork* w;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            func_actor_405800_801361F8(arg0);
            fns.funcs[work->state](arg0);
            func_actor_405800_8013795C(arg0);
            func_actor_405800_801375C4(arg0);
            func_actor_405800_8013315C(arg0);
            _stalkerZebraIvoryTickAnimInline(arg0);
            work->previousAnimationFlags = work->rig.slots[1].status.fields.flags;
            root->composeStamp           = GRAPHICS_COORD_DIRTY;
            _stalkerZebraIvoryApplyRotationInline(arg0);
            func_actor_405800_80136388(arg0);
            if (enemy->hp <= 0 && work->holding == 0) {
                w           = (_Actor405800IvoryStalkerWork*)arg0->work;
                arg0->state = 2;
                w->state    = 0;
                w->subState = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            actorUpdateModelColor(arg0);
            func_actor_405800_80132E3C(arg0, work->shadowHeight, work->shadowShade);
            Actor405800_ProjectPart(part);
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void func_actor_405800_80133CD0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    u32                           sound;
    s32                           pan;

    if (((_Actor405800IvoryStalkerWork*)arg0->work)->playerDistance < 0x1450) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work = (_Actor405800IvoryStalkerWork*)arg0->work;
        if (((s8)work->cloakRequest >= 0) || ((work->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
            work->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
            work->cloakPhase   = 0;
        }
        sceneEngageBattle(1);
        work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->state    = 0xC;
        work2->subState = 0;
    }
}

static void func_actor_405800_80133DB0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    s32                           id;
    u32                           sound;
    s32                           pan;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->leftArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->leftArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == 0x1C) {
        work->leftArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        _stalkerZebraIvorySeedTimer(arg0, 0);
        _stalkerZebraIvoryFoldArms(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->targetBearing - 0x200) > 0xC00 && (u16)(work->bearingFromPlayer - 0x200) > 0xC00) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80133F48(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    s32                           id;
    u32                           sound;
    s32                           pan;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->rightArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateFrames == 0x1C) {
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        _stalkerZebraIvorySeedTimer(arg0, 0);
        _stalkerZebraIvoryFoldArms(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->targetBearing - 0x200) > 0xC00 && (u16)(work->bearingFromPlayer - 0x200) > 0xC00) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_801340E0(Task* arg0)
{
    AnimationPlayRequest          msg;
    GameActorButtonPressHold      query;
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    s32                           base;
    s32                           sound;
    s32                           pan;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((GameActor*)gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work)->mode == GAME_ACTOR_MODE_SCRIPTED || (_stalkerZebraIvoryWallDistance(arg0) << 0x10) != 0) {
        _stalkerZebraIvoryDisableCapsuleGrid(arg0);
        work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
        func_actor_405800_80135A3C(arg0, work->walkStep);
        return;
    }
    query.pressCount = 0x18;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &query, 0) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        if (work->onCeiling == 0) {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->state    = 0xD;
        work2->subState = 0;
        return;
    }
    work->shadowHeight = work->floorY;
    _stalkerZebraIvoryDisableCapsuleGrid(arg0);
    work->onCeiling          = 0;
    Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
    work->holding            = 1;
    work->leapY              = work->floorY;
    msg.source.sets          = D_actor_405800_801513F8;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    msg.animationId          = 4;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
    work->body.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    work2                 = (_Actor405800IvoryStalkerWork*)arg0->work;
    work2->animStep       = 0x10;
    work2->animClip       = 0x21;
    work2->animBlend      = 4;
    work2->animRequest    = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames     = 0;
    work->holdFrames      = 0;
    base                  = 0x40050004;
    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->holdLoops = 0;
    work->subState++;
}

static void func_actor_405800_80134314(Task* arg0)
{
    AnimationPlayRequest          msg;
    SVECTOR                       vec;
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    GfxCoord*                     root;
    PlayerStatus*                 cfg;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s32                           sound2;
    s32                           pan2;

    work               = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    player             = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll        += -work->roll >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    coord->coord.t[1] += (player->coord.t[1] - coord->coord.t[1]) >> 2;
    work->stateFrames++;
    cfg = &gPlayerStatus;
    if (++work->holdFrames == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->playerDied == 1 || work->holdKilledPlayer == 1 || enemy->hp <= 0 || work->holdLoops >= 4) {
        work->playerDied = 0;
        if (work->holdKilledPlayer == 0) {
            msg.source.sets          = D_actor_405800_801513F8;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 8;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            msg.animationId          = 5;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        }
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->animBlend   = 8;
        work2->animStep    = 0x10;
        work2->animClip    = 0x22;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->moveAccel    = -0x2A;
        work->moveSpeed    = 0;
        work->stateFrames  = 0;
        work->subState++;
        return;
    }
    if (work->stateFrames == 1 || work->stateFrames == 0x10 || work->stateFrames == 0x25) {
        root = &gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords[4];
        padScriptSpawnVariableMotorRamp(0xA, 0xC0, 8);
        id = 0x40050009;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 2), 0);
        vec.vx      = 0;
        vec.vy      = -200;
        vec.vz      = 0;
        work->leapX = ((rsin(work->yaw + 0x800) * 3000) >> 12) / 20;
        work->leapZ = ((rcos(work->yaw + 0x800) * 3000) >> 12) / 20;
        effectSpawn(EFFECT_HIT_SPLATTER_SPRAY, root, 0x10100, &vec);
        if (cfg->hp <= 0) {
            work->holdKilledPlayer = 1;
        }
    }
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->stateFrames = 0;
        work->holdLoops++;
    }
}

static void func_actor_405800_8013471C(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    s32                           y;
    s32                           id;
    s32                           sound;
    s32                           pan;

    work        = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord       = arg0->extra.tmd->coords;
    player      = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll += -work->roll >> 2;
    work->stateFrames++;
    if (work->stateFrames >= 8) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[0]    += work->leapX;
        coord->coord.t[2]    += work->leapZ;
        work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->moveAccel      += 6;
        work->moveSpeed      += work->moveAccel;
        y                     = coord->coord.t[1] + work->moveSpeed;
        coord->coord.t[1]     = y;
        if (y >= work->leapY) {
            coord->coord.t[1]    = work->leapY;
            player->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(player);
            id = 0x40050003;
            if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work2                 = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->animBlend      = 2;
            work2->animClip       = 0x19;
            work2->animStep       = 0x10;
            work2->animRequest    = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            work->subState++;
        }
    }
}

static void func_actor_405800_801348E4(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    GfxCoord*                     coord;
    s16                           v;

    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    v     = _stalkerZebraIvoryWallDistance(arg0);
    if (v != 0) {
        if (v < 0x4E9) {
            _stalkerZebraIvoryDisableCapsuleGrid(arg0);
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work->leapX = (u16)coord->coord.t[0] + ((rsin(work->yaw + 0x800) * (v - 0x100)) >> 12);
        work->leapZ = (u16)coord->coord.t[2] + ((rcos(work->yaw + 0x800) * (v - 0x100)) >> 12);
    } else {
        work->leapX = (u16)coord->coord.t[0] + ((rsin(work->yaw + 0x800) * 0x1770) >> 12);
        work->leapZ = (u16)coord->coord.t[2] + ((rcos(work->yaw + 0x800) * 0x1770) >> 12);
    }
    _stalkerZebraIvoryDisableCapsuleGrid(arg0);
    work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x15;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->moveAccel    = -0x2A;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->subState++;
}

static void func_actor_405800_80134A64(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;
    s32                           y;
    s32                           id;
    s32                           sound;
    s32                           pan;

    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if (work->stateFrames == 0x11) {
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding       = 1;
    }
    coord->coord.t[0] += (work->leapX - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += (work->leapZ - coord->coord.t[2]) >> 4;
    work->moveAccel   += 6;
    work->moveSpeed   += work->moveAccel;
    y                  = coord->coord.t[1] + work->moveSpeed;
    coord->coord.t[1]  = y;
    if (y >= work->floorY) {
        coord->coord.t[1] = work->floorY;
        id                = 0x40050003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animClip    = 0x19;
        work2->animStep    = 0x10;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->holding      = 0;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_right_itself.inc.c"

static void func_actor_405800_80134E80(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;
    s32                           id;
    s32                           sound;
    s32                           pan;
    s32                           y;

    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if (work->stateFrames == 0x11) {
        work->pendingAction   = STALKER_ZEBRA_IVORY_PENDING_NONE;
        work->holding         = 1;
        work->gridBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->stateFrames >= 0x11 && work->stateFrames < 0x15) {
        work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 1);
    }
    if (work->stateFrames == 0x15) {
        work->shadowHeight = -0x9C4;
    }
    if (work->stateFrames >= 0x15) {
        work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 1);
    }
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += (work->leapY - y) >> 3;
    work->pitch       += (0x800 - work->pitch) >> 3;
    if (work->leapY >= coord->coord.t[1]) {
        work->shadowShade = 0xFF;
        id                = 0x40050003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[1]     = work->leapY;
        work->pitch           = 0;
        work->roll            = 0x800;
        work->yaw            += 0x800;
        _stalkerZebraIvoryApplyRotationInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        work->holding      = 0;
        work->onCeiling    = 1;
        work->subState++;
    }
}

static void func_actor_405800_801351BC(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;

    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames < 8) {
        work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 1);
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 3, &work->anchorPos);
        return;
    }
    work->shadowShade   = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 1);
    work->shadowHeight  = work->floorY;
    work->anchorPos.vx += (work->leapX - work->anchorPos.vx) >> 2;
    work->anchorPos.vz += (work->leapZ - work->anchorPos.vz) >> 2;
    _stalkerZebraIvoryPinPartXZ(arg0, 3, &work->anchorPos);
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if ((work->pitch & 0xFFF) != 0x800) {
        work->pitch -= 0x80;
    }
    if (work->floorY < coord->coord.t[1]) {
        work->onCeiling   = 0;
        work->shadowShade = 0xFF;
        coord->coord.t[0] = work->leapX;
        coord->coord.t[1] = work->floorY;
        coord->coord.t[2] = work->leapZ;
        work->pitch       = 0;
        work->roll        = 0;
        work->yaw        += 0x800;
        _stalkerZebraIvoryApplyRotationInline(arg0);
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        _stalkerZebraIvoryTickAnimInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->stateFrames = 0;
        work->subState++;
    }
}

static void func_actor_405800_80135558(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    u32                           sound;
    s32                           pan;
    u32                           rnd;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->stateFrames == 0) {
        work->holding = 0;
        sound         = 0x40050006 | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan           = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan         >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames++;
    }
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->pendingAction != STALKER_ZEBRA_IVORY_PENDING_STATUS) {
            work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->animBlend   = 2;
            work2->animStep    = 0x10;
            work2->animClip    = 0x14;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            work->subState++;
            return;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        rnd                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = rnd;
        work->countdown     = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3               = (_Actor405800IvoryStalkerWork*)arg0->work;
        work3->state        = 5;
        work3->subState     = 0;
    }
}

static void func_actor_405800_801356A8(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    u16                           count;
    u32                           rnd;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((func_actor_405800_80136A1C(arg0) << 0x10) == 0) && ((func_actor_405800_801373E0(arg0) << 0x10) == 0)) {
        count            = work->idleFrames - 1;
        work->idleFrames = count;
        if ((count << 0x10) == 0) {
            rnd                     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->hideCooldownReset = ((rnd >> 0x10) & 0x3F) + 0x1E;
            work2                   = (_Actor405800IvoryStalkerWork*)arg0->work;
            gRandomLcgState         = rnd;
            if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
                work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
                work2->cloakPhase   = 0;
            }
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

/// Spawn the two child models from `D_actor_405800_801514B4`, parent them to
/// root parts 10 and 7 at +/-0x400 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
static void func_actor_405800_80135780(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
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
    work              = (_Actor405800IvoryStalkerWork*)arg0->work;
    parent            = &root[7];
    parent2           = &root[10];
    task              = taskSpawnFromTable(D_actor_405800_801514B4, 0, 0, 0);
    work->armTasks[0] = task;
    obj               = task->extra.tmd;
    coord             = obj->coords;
    obj->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent     = parent2;
    coord->coord.t[0] = 0x400;
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
    task = work->armTasks[1] = taskSpawnFromTable(D_actor_405800_801514B4, 1, 0, 0);
    obj                      = task->extra.tmd;
    coord                    = obj->coords;
    obj->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent            = parent;
    coord->coord.t[0]        = -0x400;
    coord->coord.t[1]        = 0;
    coord->coord.t[2]        = 0;
    src                      = arg0->extra.tmd;
    dst                      = task->extra.tmd;
    dst->texturePageOffset   = src->texturePageOffset;
    dst->clutRowOffset       = src->clutRowOffset;
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

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135998`, one frame-count pair per sound event.
static void func_actor_405800_80135A3C(Task* arg0, s16 arg1)
{
    _Actor405800IvoryStalkerWork* work;
    GfxCoord*                     coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = 0x10;
        work->animClip    = 2;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        _stalkerZebraIvoryTickAnimInline(arg0);
    }
    start0 = 0;
    if (((_Actor405800IvoryStalkerWork*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((_Actor405800IvoryStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((_Actor405800IvoryStalkerWork*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((_Actor405800IvoryStalkerWork*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((_Actor405800IvoryStalkerWork*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((_Actor405800IvoryStalkerWork*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_405800_80137908(arg0) << 0x10) != 0) {
        work->animFrame = 0;
        work->animStep  = arg1;
    }
    if (work->animFrame == start0) {
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        id = 0x40050001;
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
    }
    if (work->animFrame == start1) {
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 8, &work->anchorPos);
        id = 0x40050002;
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
    }
    if (work->animFrame >= start0 && work->animFrame <= end0) {
        _stalkerZebraIvoryPinPartXZ(arg0, 0xB, &work->anchorPos);
        work->nextAnchorPart = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        _stalkerZebraIvoryPinPartXZ(arg0, 8, &work->anchorPos);
        work->nextAnchorPart = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/stalker_zebra_ivory_step_clip4.inc.c"

static void func_actor_405800_801361F8(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    GfxCoord*                     coord;
    GfxCoord*                     player;
    GameActor*                    actor;
    SVECTOR                       v;
    s16                           py;

    work                 = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord                = arg0->extra.tmd->coords;
    arg0                 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    if (arg0 == NULL) {
        return;
    }
    player = arg0->extra.tmd->coords;
    actor  = arg0->work;
    if (player->coord.t[0] < 0x3A98 || gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        work->targetPos.vx = (u16)player->coord.t[0];
        work->targetPos.vy = (u16)player->coord.t[1];
        work->targetPos.vz = (u16)player->coord.t[2];
    } else {
        work->targetPos.vx = 0x834;
        py                 = (u16)player->coord.t[1];
        work->targetPos.vz = 0xD48;
        work->timer        = 2;
        work->targetPos.vy = py;
    }
    v.vx                 = (u16)work->targetPos.vx - (u16)coord->coord.t[0];
    v.vy                 = (u16)work->targetPos.vy - (u16)coord->coord.t[1];
    v.vz                 = (u16)work->targetPos.vz - (u16)coord->coord.t[2];
    work->playerDistance = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->targetBearing     = (ratan2(v.vx, v.vz) - work->yaw) & 0xFFF;
    work->bearingFromPlayer = (ratan2(-v.vx, -v.vz) - actor->rotation.vy) & 0xFFF;
}

static void func_actor_405800_80136388(Task* arg0)
{
    WorldCollisionDelta           delta;
    s16                           maxX;
    s16                           maxZ;
    s16                           stepX;
    s16                           stepZ;
    GfxCoord*                     coord;
    u8                            blocked;
    _Actor405800IvoryStalkerWork* work;
    Enemy*                        enemy;
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
    work               = (_Actor405800IvoryStalkerWork*)arg0->work;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    work->pendingArmed = 0;

    for (i = 0; i < (s32)ARRAY_SIZE(work->bodyContacts); i++) {
        if ((work->bodyContacts[i].key.value & 0xFFFF0000) == 0x20000) {
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
                effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value),
                               &arg0->extra.tmd->coords[4], NULL, &work->effectArg);
                if (amount >= 0xB4) {
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_HEAVY;
                } else if (amount >= 0x78) {
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_LIGHT;
                } else {
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                }
                switch (damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        damageStartEnemyStagger(enemy);
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->bodyContacts[i].key.value, 0);
                        work->stunKind = ACTOR_405800_STUN_STATUS;
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
                    case 9:
                        if (work->stunKind != ACTOR_405800_STUN_STATUS) {
                            work->stunKind      = ACTOR_405800_STUN_TIMED;
                            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_STATUS;
                        }
                        break;
                }
            } else if ((damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value)) == 0xD) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
            }
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->pendingAction   = STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->pendingAction   = STALKER_ZEBRA_IVORY_PENDING_STATUS;
        work->stunKind        = ACTOR_405800_STUN_STATUS;
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

    switch (worldCollisionResolvePushback(work->gridContacts, &delta, ARRAY_SIZE(work->gridContacts), NULL)) {
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
            coord->coord.t[0] = work->prevRootPos.vx;
            blocked           = 1;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }

    worldCollisionClearContacts(work->bodyContacts);
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
    if (blocked == 0) {
        work->anchorPos.vx += actorPickStep(stepX, maxX >> 3);
        work->anchorPos.vz += actorPickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += actorPickStep(stepX, (u16)maxX >> 3);
        coord->coord.t[2]  += actorPickStep(stepZ, (u16)maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static s32 func_actor_405800_80136A1C(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->pendingArmed == 1) {
        if (work->onCeiling == 0) {
            switch (work->pendingAction) {
                case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 3;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 5;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 0xF;
                    work2->subState     = 0;
                    return 1;
            }
            return 0;
        } else {
            switch (work->pendingAction) {
                case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 3;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                    work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state    = 0xE;
                    work2->subState = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 0xE;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

#include "../../shared/stalker_zebra_ivory_apply_pending_reaction.inc.c"

static s32 func_actor_405800_80136CE0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->pendingArmed == 1) {
        if (work->onCeiling == 0) {
            switch (work->pendingAction) {
                case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 0;
                case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 5;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 0xF;
                    work2->subState     = 0;
                    return 1;
            }
            work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
            return 0;
        } else {
            switch (work->pendingAction) {
                case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 0;
                case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                    work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state    = 0xE;
                    work2->subState = 0;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
                case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                    work2               = (_Actor405800IvoryStalkerWork*)arg0->work;
                    work2->state        = 0xE;
                    work2->subState     = 0;
                    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

static void func_actor_405800_80136E14(Task* task)
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

    D_800678F0[0] = &_gActor405800IvoryStalkerBurstHead;
    eff           = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[5], 0x200, NULL);
    if (eff != NULL) {
        src                    = task->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerBurstTorso;
    eff2          = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = task->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdBuildBufferHalf(dst2);
            tmdBuildBufferHalf(dst2);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerEffect;
    eff3          = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = task->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdBuildBufferHalf(dst3);
            tmdBuildBufferHalf(dst3);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerBurstHandLeft;
    eff4          = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4                    = task->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdBuildBufferHalf(dst4);
            tmdBuildBufferHalf(dst4);
        }
    }
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], 0x200, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[2], 0x200, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[3], 0x200, NULL);
}

static void func_actor_405800_8013706C(Task* arg0, s16 arg1)
{
    _Actor405800IvoryStalkerWork* work = (_Actor405800IvoryStalkerWork*)arg0->work;
    SVECTOR                       v;
    SVECTOR                       out;
    MATRIX                        rot;
    s16                           n;

    work->distanceMode = arg1;
    switch (arg1) {
        case 0:
            if (work->onCeiling == 0) {

                v.vx = work->targetPos.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy = work->targetPos.vy - arg0->extra.tmd->coords->coord.t[1] - 0x384;
                v.vz = work->targetPos.vz - arg0->extra.tmd->coords->coord.t[2];
                gfxSetRotIdentity(&rot);
                rot.t[0] = 0;
                rot.t[1] = 0;
                rot.t[2] = 0;
                RotMatrixY(-work->yaw, &rot);
                ApplyMatrixSV(&rot, &v, &out);
            } else {

                v.vx = work->targetPos.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy = work->targetPos.vy - arg0->extra.tmd->coords->coord.t[1] - 0x640;
                v.vz = work->targetPos.vz - arg0->extra.tmd->coords->coord.t[2];
                gfxSetRotIdentity(&rot);
                rot.t[0] = 0;
                rot.t[1] = 0;
                rot.t[2] = 0;
                RotMatrixY(-work->yaw, &rot);
                RotMatrixZ(-work->roll, &rot);
                ApplyMatrixSV(&rot, &v, &out);
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
            work->capsule.ends[0].vz = -0x1770;
            work->capsule.end0Radius = 0x50;
            work->capsule.end1Radius = 0x50;
            break;
    }
    work->capsule.ends[1].vx = 0;
    work->capsule.ends[1].vy = 0;
    work->capsule.ends[1].vz = 0;
    worldCollisionClearContacts(work->capsuleContacts);
    work->capsuleBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#include "../../shared/stalker_zebra_ivory_wall_distance.inc.c"

/// `func_actor_405800_80139FB0`'s body, inlined: switch to behaviour `id`
/// and restart its step counter.
static __inline__ void _actor405800SetBehaviour(Task* task, s16 id)
{
    _Actor405800IvoryStalkerWork* work = (_Actor405800IvoryStalkerWork*)task->work;

    work->state    = id;
    work->subState = 0;
}

static s32 func_actor_405800_801373E0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    GfxCoord*                     coord;
    u32                           rnd;
    u32                           bits;
    s16                           ang;

    work            = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord           = arg0->extra.tmd->coords;
    rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    bits            = rnd >> 0x10;
    gRandomLcgState = rnd;
    if (work->timer == 0) {
        if (work->onCeiling == 0) {
            if ((bits & 0xF) == 0) {
                if ((work->leapCooldown == 0) && (coord->coord.t[0] < 0x2710)) {
                    _actor405800SetBehaviour(arg0, 0xC);
                    return 1;
                }
                return 0;
            }
            if ((bits & 7) >= 1 && (bits & 7) <= 3) {
                if ((work->playerDistance < 0x7D0) && (work->targetBearing < 0x200 || work->targetBearing > 0xE00) && (work->bearingFromPlayer > 0x600 && work->bearingFromPlayer < 0xA00)) {
                    _actor405800SetBehaviour(arg0, 8);
                    return 1;
                }
            } else if (work->playerDistance < 0x640) {
                ang = (s16)work->targetBearing;
                if (ang < 0x400) {
                    _actor405800SetBehaviour(arg0, 6);
                    return 1;
                }
                if (ang >= 0xC01) {
                    _actor405800SetBehaviour(arg0, 7);
                    return 1;
                }
            }
        } else if ((bits & 7) == 0) {
            if (work->leapCooldown == 0) {
                _actor405800SetBehaviour(arg0, 0x10);
                return 1;
            }
            return 0;
        } else if ((bits & 0xF) == 1) {
            if (work->leapCooldown == 0) {
                _actor405800SetBehaviour(arg0, 0xD);
                return 1;
            }
            return 0;
        } else if (work->playerDistance < 0x640) {
            ang = (s16)work->targetBearing;
            if (ang >= 0xC01) {
                _actor405800SetBehaviour(arg0, 6);
                return 1;
            }
            if (ang < 0x400) {
                _actor405800SetBehaviour(arg0, 7);
                return 1;
            }
            return 0;
        } else {
            return 0;
        }
    }
    return 0;
}

/// Replaces the rotation of `coord` with the one `RotMatrixY` applies to an
/// identity matrix for `angle`.
static inline void _actor405800SetCoordRotation(GfxCoord* coord, s16 angle)
{
    MATRIX    rot;
    MATRIX*   dst;

    gfxSetRotIdentity(&rot);
    RotMatrixY(angle, &rot);
    dst          = &coord->coord;
    dst->m[0][0] = rot.m[0][0];
    dst->m[0][1] = rot.m[0][1];
    dst->m[0][2] = rot.m[0][2];
    dst->m[1][0] = rot.m[1][0];
    dst->m[1][1] = rot.m[1][1];
    dst->m[1][2] = rot.m[1][2];
    dst->m[2][0] = rot.m[2][0];
    dst->m[2][1] = rot.m[2][1];
    dst->m[2][2] = rot.m[2][2];
}

static void func_actor_405800_801375C4(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    Task*                         child;
    s16                           angle;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (work->rightArmOut != 0) {
        angle                   = (u16)work->armSwingAngles[1] + ((0x380 - work->armSwingAngles[1]) >> 2);
        work->armSwingAngles[1] = angle;
        child                   = ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[1];
        child->extra.tmd->flags = 0;
        _actor405800SetCoordRotation(child->extra.tmd->coords, angle);
    } else {
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        angle                      = (u16)work->armSwingAngles[1] + (-work->armSwingAngles[1] >> 3);
        work->armSwingAngles[1]    = angle;
        if (angle < 9) {
            ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            child                   = ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[1];
            child->extra.tmd->flags = 0;
            _actor405800SetCoordRotation(child->extra.tmd->coords, angle);
        }
    }

    if (work->leftArmOut != 0) {
        angle                   = (u16)work->armSwingAngles[0] + ((0x380 - work->armSwingAngles[0]) >> 2);
        work->armSwingAngles[0] = angle;
        child                   = ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[0];
        angle                   = -angle;
        child->extra.tmd->flags = 0;
        _actor405800SetCoordRotation(child->extra.tmd->coords, angle);
    } else {
        work->leftArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        angle                     = (u16)work->armSwingAngles[0] + (-work->armSwingAngles[0] >> 3);
        work->armSwingAngles[0]   = angle;
        if (angle < 9) {
            ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            child                   = ((_Actor405800IvoryStalkerWork*)task->work)->armTasks[0];
            child->extra.tmd->flags = 0;
            _actor405800SetCoordRotation(child->extra.tmd->coords, -angle);
        }
    }

    if (work->state != 6 && work->state != 7) {
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

static s32 func_actor_405800_80137908(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work = (_Actor405800IvoryStalkerWork*)arg0->work;

    if ((work->previousAnimationFlags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

#include "../../shared/stalker_zebra_ivory_fold_arms.inc.c"

static void func_actor_405800_8013795C(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (work->timer > 0) {
        work->timer = work->timer - 1;
    }
    if (work->leapCooldown > 0) {
        work->leapCooldown = work->leapCooldown - 1;
    }
}

#include "../../shared/stalker_zebra_ivory_seed_timer.inc.c"

#include "../../shared/stalker_zebra_ivory_disable_capsule_grid.inc.c"

static void func_actor_405800_80137A14(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* cur;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s8)work->cloakRequest >= 0 || (work->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK)) && work->hideCooldown == 0) {
        work->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_HIDE;
        work->cloakPhase   = 0;
    }
    cur           = (_Actor405800IvoryStalkerWork*)task->work;
    cur->state    = 1;
    cur->subState = 0;
}

/// Per-frame entry point for one of this actor's states: folds both arms
/// away, then runs the sub-state handler `subState` selects unless
/// `func_actor_405800_80136A1C` or `func_actor_405800_801373E0` already
/// consumed the frame. After the handler, a set `onCeiling` plus a root
/// world X past 10000 switches to state 0xD. The two-entry table is small
/// enough that GCC materialises each callback with its own `lui`/`addiu`
/// pair instead of copying a `.rodata` pool.
static void func_actor_405800_80137A60(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80138FA8, func_actor_405800_8013902C };

    _stalkerZebraIvoryFoldArms(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0 && (s16)func_actor_405800_801373E0(task) == 0) {
        states[work->subState](task);
        if (work->onCeiling != 0 && task->extra.tmd->coords->coord.t[0] > 10000) {
            _Actor405800IvoryStalkerWork* cur = (_Actor405800IvoryStalkerWork*)task->work;

            cur->state    = 0xD;
            cur->subState = 0;
        }
    }
}

/// Per-frame entry point for one of this actor's states: folds both arms
/// away, then runs the sub-state handler `subState` selects. The
/// two-entry table is small enough that GCC materialises each callback with its
/// own `lui`/`addiu` pair instead of copying a `.rodata` pool.
static void func_actor_405800_80137B34(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_801390FC, func_actor_405800_80139188 };

    _stalkerZebraIvoryFoldArms(task);
    states[work->subState](task);
}

static void func_actor_405800_80137B9C(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80139260, func_actor_405800_801392EC };

    _stalkerZebraIvoryFoldArms(task);
    states[work->subState](task);
}

/// Per-frame handler for one of this actor's behaviours: clears `leftArmOut` /
/// `rightArmOut` through `_stalkerZebraIvoryFoldArms`, then runs the sub-state
/// handler of `D_actor_405800_80131EAC` that `subState` selects.
static void func_actor_405800_80137C04(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable3                states = D_actor_405800_80131EAC;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

static void func_actor_405800_80137C78(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80139550, func_actor_405800_80133DB0 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[work->subState](task);
    }
}

static void func_actor_405800_80137CEC(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_801395E8, func_actor_405800_80133F48 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[work->subState](task);
    }
}

static void func_actor_405800_80137D60(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable5                states = D_actor_405800_80131EB8;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

static void func_actor_405800_80137DE4(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable4                states = D_actor_405800_80131ECC;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

static void func_actor_405800_80137E64(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    s16                           count;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    _stalkerZebraIvoryFoldArms(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0) {
        count           = work->countdown - 1;
        work->countdown = count;
        if (count == 0) {
            work2           = (_Actor405800IvoryStalkerWork*)task->work;
            work2->state    = 0xB;
            work2->subState = 0;
            return;
        }
        stalkerZebraIvoryTurnToward(task, &work->targetPos, 0x18);
        stalkerZebraIvoryStepClip4(task);
    }
}

static void func_actor_405800_80137EF0(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80139844, stalkerZebraIvoryRightItself };

    _stalkerZebraIvoryFoldArms(task);
    states[work->subState](task);
}

static void func_actor_405800_80137F58(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable3                states = D_actor_405800_80131EDC;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

static void func_actor_405800_80137FCC(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable3                states = D_actor_405800_80131EE8;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

static void func_actor_405800_80138040(Task* task)
{
    _Actor405800IvoryStalkerWork* work   = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFuncTable4                states = D_actor_405800_80131EF4;

    _stalkerZebraIvoryFoldArms(task);
    states.funcs[work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_run_sub_states.inc.c"

static void func_actor_405800_80138154(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80139E2C, stalkerZebraIvoryPickRange };

    _stalkerZebraIvoryFoldArms(task);
    states[work->subState](task);
}

static void func_actor_405800_801381BC(Task* task)
{
    _Actor405800IvoryStalkerWork* work      = (_Actor405800IvoryStalkerWork*)task->work;
    TaskFunc                      states[2] = { func_actor_405800_80139EAC, func_actor_405800_801356A8 };

    _stalkerZebraIvoryFoldArms(task);
    states[work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_restart_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_blend_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_frame_to_ticks.inc.c"

#include "../../shared/stalker_zebra_ivory_turn_toward.inc.c"

#include "../../shared/stalker_zebra_ivory_part_world_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_pin_part_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_clip_done.inc.c"

/// Per-frame entry point of the actor's task: runs whichever of the four
/// handlers in `D_actor_405800_80131E54` the task's `state` selects. The table
/// is a local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_405800_80138634(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_405800_80131E54;
    states.funcs[task->state](task);
}

static void func_actor_405800_80138698(Task* arg0)
{
    TmdObject*                    model = arg0->extra.tmd;
    _Actor405800IvoryStalkerWork* work  = (_Actor405800IvoryStalkerWork*)arg0->work;
    TaskFuncTable12               fns   = D_actor_405800_80131E24;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            fns.funcs[work->state](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_405800_80132E3C(arg0, work->shadowHeight, work->shadowShade);
            break;
    }
}

static void func_actor_405800_80138788(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work                = (_Actor405800IvoryStalkerWork*)arg0->work;
    void                          (*states[2])(Task*) = {
        func_actor_405800_80138EF0,
        func_actor_405800_80138F54,
    };

    states[work->state](arg0);
}

#include "../../shared/stalker_zebra_ivory_update_color.inc.c"

#include "../../shared/stalker_zebra_ivory_set_move_mode.inc.c"

void func_actor_405800_801388C4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ((_Actor405800IvoryStalkerWork*)task->work)->playerDied = 1;
}

void func_actor_405800_801388D4(Task* task)
{
}

void func_actor_405800_801388DC(Task* task)
{
}

static void func_actor_405800_801388E4(Task* task)
{
    Enemy*                        enemy;
    _Actor405800IvoryStalkerWork* work;
    TmdObject*                    model;

    enemy                      = (Enemy*)task->spawnArg2.pointer;
    work                       = (_Actor405800IvoryStalkerWork*)task->work;
    model                      = task->extra.tmd;
    work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldTargetUnlinkNode(&enemy->node);
    if (work->pendingAction == STALKER_ZEBRA_IVORY_PENDING_BLAST) {
        work->stateFrames = 0;
        model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_405800_80139FB0(task, 7);
    } else if (work->onCeiling == 0) {
        work->state = work->state + 1;
    } else {
        func_actor_405800_80139FB0(task, 9);
    }
}

#include "../../shared/stalker_zebra_ivory_resume_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_animate_until_done.inc.c"

static void func_actor_405800_80138A70(Task* task)
{
    _Actor405800IvoryStalkerWork* work  = (_Actor405800IvoryStalkerWork*)task->work;
    GfxCoord*                     coord = task->extra.tmd->coords;

    ((Enemy*)task->spawnArg2.pointer)->recs = 0;
    worldCollisionUnlinkBody(&work->gridBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmOuter);
    worldCollisionUnlinkBody(&work->leftArmOuter);
    worldCollisionUnlinkBody(&work->rightArmInner);
    worldCollisionUnlinkBody(&work->leftArmInner);
    worldCollisionUnlinkBody(&work->capsuleBody);
    work->corpseScaleY = ONE;
    work->savedRootMtx = coord->coord;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state++;
}

static void func_actor_405800_80138B50(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    TmdObject*                    ext;
    u16                           count;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    ext               = task->extra.tmd;
    count             = work->stateFrames + 1;
    work->stateFrames = count;
    if ((s16)count >= 0x18) {
        work->cloakLevel  = 0;
        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
        work->shadowShade = 0xFF;
        modelLightingSetLayerMaterials(work->cloakLevel);
        ext->shading.colorBlend = work->colorBlend;
        work->stateFrames       = 0;
        work->state             = work->state + 1;
    }
}

static void func_actor_405800_80138BD4(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work           = (_Actor405800IvoryStalkerWork*)task->work;
    task->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

static void func_actor_405800_80138BEC(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    u16                           count;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    count             = work->stateFrames + 1;
    work->stateFrames = count;
    if ((s16)count >= 2) {
        work->state = work->state + 1;
    }
}

static void func_actor_405800_80138C30(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    TmdObject*                    model;
    Enemy*                        enemy;

    model = task->extra.tmd;
    work  = (_Actor405800IvoryStalkerWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    func_actor_405800_80136E14(task);
    work->shadowShade = 0;
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->gridBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmOuter);
    worldCollisionUnlinkBody(&work->leftArmOuter);
    worldCollisionUnlinkBody(&work->rightArmInner);
    worldCollisionUnlinkBody(&work->leftArmInner);
    worldCollisionUnlinkBody(&work->capsuleBody);
    work2           = (_Actor405800IvoryStalkerWork*)task->work;
    task->state     = 3;
    work2->state    = 0;
    work2->subState = 0;
}

static void func_actor_405800_80138CF0(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    func_actor_405800_8013A1F8(task, 9, 0x10, 2);
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->shadowHeight = work->floorY;
    _stalkerZebraIvoryTickAnim(task);
    work->state = work->state + 1;
}

static void func_actor_405800_80138D54(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    GfxCoord*                     coord;

    work               = (_Actor405800IvoryStalkerWork*)task->work;
    coord              = task->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (work->floorY < coord->coord.t[1]) {
        coord->coord.t[1] = work->floorY;
        _stalkerZebraIvoryRequestClipRestart(task, 0x13, ANIMATION_RATE_ONE);
        work->roll += 0x800;
        _stalkerZebraIvoryApplyRotation(task);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->stateFrames = 0;
        work->onCeiling   = 0;
        work->onBack      = 1;
        work->state++;
    }
    _stalkerZebraIvoryTickAnim(task);
}

static void func_actor_405800_80138E20(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    u32                           sound;
    s32                           pan;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (work->stateFrames == 0) {
        sound   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050006;
        pan     = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateFrames++;
    }
    if ((_stalkerZebraIvoryClipDone(task) << 0x10) != 0) {
        func_actor_405800_80139FB0(task, 1);
    }
    _stalkerZebraIvoryTickAnim(task);
}

static void func_actor_405800_80138EF0(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (work->armTasks[0] != NULL) {
        taskKill(work->armTasks[0]);
    }
    if (work->armTasks[1] != NULL) {
        taskKill(work->armTasks[1]);
    }
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

static void func_actor_405800_80138F54(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    u16                           count;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    count             = work->stateFrames + 1;
    work->stateFrames = count;
    if ((s16)count >= 0x12D) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}

static void func_actor_405800_80138FA8(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s8)work->cloakRequest >= 0 || (work->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK)) && work->hideCooldown == 0) {
        work->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_HIDE;
        work->cloakPhase   = 0;
    }
    work->turnStep    = 0x18;
    work->walkHurried = 0;
    work->walkStep    = 0x10;
    func_actor_405800_80135A3C(task, 0x10);
    work->subState = work->subState + 1;
}

static void func_actor_405800_8013902C(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    s16                           min;
    s16                           step;
    u32                           rnd;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    min  = 0x10;
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
    stalkerZebraIvoryTurnToward(task, &work->targetPos, work->turnStep);
    func_actor_405800_80135A3C(task, work->walkStep);
}

static void func_actor_405800_801390FC(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
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
    work2 = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_80139188(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
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
    if ((func_actor_405800_80136A1C(arg0) << 0x10) == 0 && (_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139260(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0xA;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0xC;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    work2 = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_801392EC(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139358(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->onBack == 0) {
        work->animBlend   = 8;
        work->animStep    = 0x10;
        work->animClip    = 0xF;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x11;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    work2 = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_405800_801393E8(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;
    _Actor405800IvoryStalkerWork* work4;
    _Actor405800IvoryStalkerWork* work5;
    u16                           count;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->stunKind == ACTOR_405800_STUN_STATUS) {
        if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
            if (work->onBack == 0) {
                work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
                work2->animBlend   = 8;
                work2->animStep    = 0x10;
                work2->animClip    = 0x10;
                work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            } else {
                work3              = (_Actor405800IvoryStalkerWork*)arg0->work;
                work3->animBlend   = 8;
                work3->animStep    = 0x10;
                work3->animClip    = 0x12;
                work3->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            }
            work->subState = work->subState + 1;
        }
    } else {
        count             = work->stateFrames + 1;
        work->stateFrames = count;
        if ((s16)count >= 0x15) {
            if (work->onBack == 0) {
                work4              = (_Actor405800IvoryStalkerWork*)arg0->work;
                work4->animBlend   = 8;
                work4->animStep    = 0x10;
                work4->animClip    = 0x10;
                work4->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            } else {
                work5              = (_Actor405800IvoryStalkerWork*)arg0->work;
                work5->animBlend   = 8;
                work5->animStep    = 0x10;
                work5->animClip    = 0x12;
                work5->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
            }
            work->subState = work->subState + 1;
        }
    }
}

static void func_actor_405800_801394E4(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->stunKind = ACTOR_405800_STUN_NONE;
        if (work->onBack == 0) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139550(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 7;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    func_actor_405800_80139F0C(task, 0);
    work2 = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_801395E8(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 8;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    func_actor_405800_80139F0C(task, 1);
    work2 = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_8013967C(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    func_actor_405800_8013706C(task, 0);
    work->playerDied = 0;
    work2            = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    func_actor_405800_80135A3C(task, work->walkStep);
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_release_hold.inc.c"

static void func_actor_405800_801397B8(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    func_actor_405800_8013706C(task, 1);
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_wait_clip.inc.c"

static void func_actor_405800_80139844(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work               = (_Actor405800IvoryStalkerWork*)task->work;
    work->stateFrames  = 0;
    work2              = (_Actor405800IvoryStalkerWork*)task->work;
    work2->animStep    = 0x10;
    work2->animClip    = 0x16;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
    work->subState     = work->subState + 1;
}

static void func_actor_405800_80139880(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0x15;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->leapY       = -0x9C4;
    work->subState    = work->subState + 1;
}

static void func_actor_405800_801398C0(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)task->work;
    if (((s16)_stalkerZebraIvoryApplyPendingReaction(task) == 0) && ((s16)_stalkerZebraIvoryClipDone(task) != 0)) {
        work->leapCooldown = 0x12C;
        work2              = (_Actor405800IvoryStalkerWork*)task->work;
        work2->state       = 2;
        work2->subState    = 0;
    }
}

static void func_actor_405800_80139928(Task* task)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;
    u16                           next;

    work  = (_Actor405800IvoryStalkerWork*)task->work;
    coord = task->extra.tmd->coords;
    _stalkerZebraIvoryDisableCapsuleGrid(task);
    work2              = (_Actor405800IvoryStalkerWork*)task->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x20;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->moveAccel    = 0x40;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->leapX        = coord->coord.t[0];
    next               = work->subState;
    work->leapZ        = coord->coord.t[2];
    work->holding      = 1;
    work->subState     = next + 1;
}

static void func_actor_405800_801399C4(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    u32                           sound;
    s32                           id;
    s32                           pan;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->stateFrames == 0) {
        id = 0x40050003;
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
    if ((_stalkerZebraIvoryApplyPendingReaction(arg0) << 0x10) == 0 && (_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->leapCooldown = 0x12C;
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->state       = 2;
        work2->subState    = 0;
    }
}

static void func_actor_405800_80139AC4(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((s8)work->cloakRequest >= 0) || ((work->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work->cloakPhase   = 0;
    }
    work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = 0x10;
    work2->animClip    = 9;
    work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    work->holding      = 1;
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->subState     = work->subState + 1;
    work->shadowHeight = work->floorY;
}

static void func_actor_405800_80139B3C(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    GfxCoord*                     coord;

    work               = (_Actor405800IvoryStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel    = work->moveAccel + 2;
    work->moveSpeed    = work->moveSpeed + work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (work->floorY < coord->coord.t[1]) {
        coord->coord.t[1]  = work->floorY;
        work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
        work2->animStep    = 0x10;
        work2->animClip    = 0x13;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        work->onBack       = 1;
        work->stateFrames  = 0;
        work->onCeiling    = 0;
        work->roll         = work->roll + 0x800;
        work->subState     = work->subState + 1;
    }
}

#include "../../shared/stalker_zebra_ivory_wait_clip_then_rest.inc.c"

static void func_actor_405800_80139C98(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x1A;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x1B;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    }
    work2 = (_Actor405800IvoryStalkerWork*)arg0->work;
    if (((s8)work2->cloakRequest >= 0) || ((work2->cloakRequest & ACTOR_405800_CLOAK_KIND_MASK) != ACTOR_405800_CLOAK_SHOW)) {
        work2->cloakRequest = ACTOR_405800_CLOAK_RUNNING | ACTOR_405800_CLOAK_SHOW;
        work2->cloakPhase   = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_80139D24(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2              = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->animBlend   = 0x1E;
            work2->animStep    = 0x10;
            work2->animClip    = 0x10;
            work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        } else {
            work3              = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->animBlend   = 0x1E;
            work3->animStep    = 8;
            work3->animClip    = 0x14;
            work3->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        }
        work->subState = work->subState + 1;
    }
}

static void func_actor_405800_80139DC0(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work;
    _Actor405800IvoryStalkerWork* work2;
    _Actor405800IvoryStalkerWork* work3;

    work = (_Actor405800IvoryStalkerWork*)arg0->work;
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (_Actor405800IvoryStalkerWork*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139E2C(Task* task)
{
    _Actor405800IvoryStalkerWork* work;

    work           = (_Actor405800IvoryStalkerWork*)task->work;
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_pick_range.inc.c"

static void func_actor_405800_80139EAC(Task* arg0)
{
    _Actor405800IvoryStalkerWork* work = (_Actor405800IvoryStalkerWork*)arg0->work;
    u32                           rnd;

    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 1;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
    rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState   = rnd;
    work->idleFrames  = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->subState    = work->subState + 1;
}

static void func_actor_405800_80139F0C(Task* task, u8 arg1)
{
    _Actor405800IvoryStalkerWork* work = (_Actor405800IvoryStalkerWork*)task->work;
    s32                           mode = arg1;

    if (mode == 0) {
        work->leftArmOut = 1;
    } else if (mode == 1) {
        work->rightArmOut = mode;
    }
}

#include "../../shared/stalker_zebra_ivory_take_armed_pending.inc.c"

static void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    _Actor405800IvoryStalkerWork* work;

    work           = (_Actor405800IvoryStalkerWork*)task->work;
    work->state    = arg1;
    work->subState = 0;
}

#include "../../shared/stalker_zebra_ivory_apply_rotation.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_anim.inc.c"

#include "../../shared/stalker_zebra_ivory_request_clip_restart.inc.c"

static void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    _Actor405800IvoryStalkerWork* work;

    work              = (_Actor405800IvoryStalkerWork*)task->work;
    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
}
