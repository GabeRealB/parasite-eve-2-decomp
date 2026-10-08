#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
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

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_contacts.h"

/// One collision sphere of the actor: a body and the contact table it records
/// into.
///
/// The work block holds four. Three ride model parts and are pair-tested
/// against other bodies while the actor is active, and some states also test
/// one of them against the room grid; the fourth sits on the root coordinate
/// and is only ever grid-tested. `contacts` is the table that body alone
/// fills: the hit handler scans the part spheres' tables for an attack
/// contact, the movement states resolve push-back from a table's contacts,
/// and each frame's update ends by resetting every table's occupied entries.
/// The spawn handler links each body and the task's exit callback unlinks it.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked into the world's body list; `coord` is the part it rides
    WorldCollisionContact contacts[5]; // The body's own contact table
} _Actor403000CollisionSphere;
STATIC_ASSERT_SIZEOF(_Actor403000CollisionSphere, 0x98);

/// Entries in a contact table of the actor's collision bodies.
///
/// Each table is initialised to this length, which puts the final-entry marker
/// on its last element, and a scan of one reads at most this many entries,
/// stopping early at a zero key.
enum { ACTOR_403000_BODY_CONTACT_COUNT = 5 };

/// One collision capsule of the actor: a body, the capsule shape the body
/// points at and the contact table that shape records into.
///
/// The work block holds two, laid out alike. One lies ahead of the root along
/// its facing and is only ever tested against the room grid. The other rides a
/// model part and is pair-tested against other bodies while the actor is
/// active: the hit handler scans its table for an attack contact after the
/// part spheres', and an attack state looks there for a contact with the
/// player.
///
/// The spawn handler fills the shape, points the body at it and it at
/// `contacts`, links the body and initialises the table; each frame's update
/// ends by resetting the table's occupied entries. Unlike the spheres, a
/// capsule's body is never unlinked by this package: the task's exit callback
/// leaves both on the world's body list.
typedef struct {
    WorldCollisionBody    body;                                      // Capsule body on the world's list; `coord` is the part it rides, `pos` and `radius` stay zero
    WorldCollisionCapsule shape;                                     // Segment end points and end radii in that part's space; its table pointer is `contacts`
    WorldCollisionContact contacts[ACTOR_403000_BODY_CONTACT_COUNT]; // The table `shape` records into, private to this capsule
} _Actor403000CollisionCapsule;
STATIC_ASSERT_SIZEOF(_Actor403000CollisionCapsule, 0xB0);

/// Values of `Actor403000Work::state`: the handler the per-frame update runs.
///
/// The arena floor is a ring of ten waypoints, and most of the movement states
/// walk it. The catch states run while the player plays one of this package's
/// catch clips.
enum {
    ACTOR_403000_STATE_HIDDEN            = 0,  // Model hidden, hit points zeroed, wall push-back off
    ACTOR_403000_STATE_SCRIPTED          = 1,  // Plays the requested clip as it is; entered by actor commands and play requests
    ACTOR_403000_STATE_PATROL            = 2,  // Walks the ring to the cell across from the player, then watches, leaves for the ambush or approaches
    ACTOR_403000_STATE_WATCH             = 3,  // Stands on its waypoint facing a neighbour until the player comes round or 300 frames pass
    ACTOR_403000_STATE_TURN              = 4,  // Turns in place toward a neighbouring waypoint
    ACTOR_403000_STATE_CHASE             = 5,  // Turns to the player and walks at it; picks the grab or the lunge by distance
    ACTOR_403000_STATE_GRAB_CATCH        = 6,  // The grab landed: pushes the caught player sideways
    ACTOR_403000_STATE_GRAB              = 7,  // Walks in, then rushes; a player contact on the neck sphere lands it
    ACTOR_403000_STATE_LUNGE             = 8,  // Lunges from range; a player contact on the head capsule lands it
    ACTOR_403000_STATE_LUNGE_CATCH       = 9,  // The lunge landed: carries the caught player forward, then throws it aside
    ACTOR_403000_STATE_RETREAT           = 10, // Walks the ring away from the player for at least 60 frames; nothing in this package enters it
    ACTOR_403000_STATE_APPROACH          = 11, // Walks the ring toward the player until it can chase; the state after spawning
    ACTOR_403000_STATE_DROP              = 12, // Falls from the ambush point onto where the player stood
    ACTOR_403000_STATE_DROP_CATCH        = 13, // The drop landed: settles onto the caught player
    ACTOR_403000_STATE_PROWL             = 14, // Slow walk round the ring after a watch that ran out
    ACTOR_403000_STATE_AMBUSH            = 15, // Waits out of sight above the arena, then drops
    ACTOR_403000_STATE_STUNNED           = 16, // Down and twitching until the build-up reaction runs out
    ACTOR_403000_STATE_KNOCKDOWN         = 17, // Falls; ends stunned, down or dissolving
    ACTOR_403000_STATE_DOWN              = 18, // Lies for 10 to 25 frames, then gets up
    ACTOR_403000_STATE_GET_UP            = 19, // Gets up and resumes the patrol
    ACTOR_403000_STATE_DISSOLVE          = 20, // Death: smokes, flattens and vanishes, and reports to the room
    ACTOR_403000_STATE_SCRIPTED_BURN     = 21, // Entered by actor command: plays clip 0x1C with burn bursts, not lockable
    ACTOR_403000_STATE_SCRIPTED_DISSOLVE = 22, // Entered by actor command: the dissolve on a shorter clock, without the room report
    ACTOR_403000_STATE_SCRIPTED_SMOLDER  = 23, // Entered by actor command: clip 0x1C under 300 frames of smoke
    ACTOR_403000_STATE_DOWN_HIT          = 24, // Hit while down: a short jolt, then stunned, down or dissolving
};

/// Values of `Actor403000Work::animStart` and `Actor403000Work::overlayStart`:
/// what the animation tick still has to do for a rig.
enum {
    ACTOR_403000_ANIM_BLEND_IN = 1, // Blend the slots into the requested clip, unless it is the one playing
    ACTOR_403000_ANIM_RESTART  = 2, // Reset the slots to the start of the requested clip
    ACTOR_403000_ANIM_PLAYING  = 3, // Nothing pending; the slots are only ticked
};

/// Per-actor work block of the Blizzard Chaser.
///
/// The spawn handler allocates it zeroed and hangs it behind `Task::work`; the
/// task's exit callback unlinks the collision spheres. It holds the state
/// machine the per-frame update runs, two animation rigs over the model's 24
/// parts, the procedural joint rotations layered on the animation, the
/// collision bodies, and the bookkeeping of the attacks that catch the player.
///
/// The main rig plays the state's clip. The overlay rig plays a flinch that is
/// mixed over parts 1 to 10 - torso, neck, head and one foreleg - while
/// `overlayActive` is set.
///
/// A cell is the index of the ring waypoint that owns a position, and a ring
/// direction is +1 or -1: the step from a cell to the neighbour walked toward.
typedef struct {
    s16                          state;                                       // Handler the update runs (ACTOR_403000_STATE_*)
    s16                          prevState;                                   // `state` at the previous update; -1 makes the next update an entry
    s16                          stateEntered;                                // 1 on a state's first update, 0 after it
    s16                          stateFrame;                                  // The running state's frame counter or countdown, restarted on entry
    s16                          stillFrames;                                 // Consecutive ambush frames the root has not moved
    byte                         pad_A[0x2];                                  // No accesses; role unproven
    s16                          placementYaw;                                // Root heading left by the last placement message; never read back
    byte                         pad_E[0x6];                                  // No accesses; role unproven
    AnimationContext             anim;                                        // Main playback context, bound to `slots` and `poses`
    AnimationSlot                slots[24];                                   // One slot per model part; slot 0, the root, is not driven
    u8                           poses[24][ANIMATION_POSE_BUFFER_BYTES];      // Encoded transition pose for the slot at the same index
    AnimationContext             blendAnim;                                   // Overlay playback context, bound to `blendSlots` and `blendPoses`
    AnimationSlot                blendSlots[24];                              // Overlay slots, indexed like `slots`
    u8                           blendPoses[24][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the overlay slot at the same index
    byte                         pad_ABC[0x4];                                // No accesses; role unproven
    s16                          animStart;                                   // What the tick still has to do for the main rig (ACTOR_403000_ANIM_*)
    s16                          overlayActive;                               // Nonzero while the overlay clip is mixed in; cleared when that clip settles
    s16                          animId;                                      // Clip the main slots were last started on
    s16                          requestedAnimId;                             // Clip the states ask for; started when `animStart` says so
    u16                          animFrames;                                  // Ticks since the main rig was last started; never read back
    s16                          animRate;                                    // Rate copied into every main slot each tick, in sixteenths of a frame (16 normal, 0 frozen)
    s16                          field_ACC;                                   // Set to 16 beside `animRate` at spawn and never read; role unproven
    s16                          overlayStart;                                // What the tick still has to do for the overlay rig (ACTOR_403000_ANIM_*)
    s16                          overlayAnimId;                               // Clip the overlay rig plays
    s16                          overlayRate;                                 // Rate of the overlay slots, in sixteenths of a frame
    s16                          overlayWeight;                               // Share of the overlay pose in the mix (4096 all)
    s16                          forelegYawTarget;                            // Yaw `forelegYaw` eases toward, limited to +-0x200; only ever cleared
    s16                          neckYawTarget;                               // Heading error to what the state walks or turns toward; `neckYaw` eases to it
    s16                          torsoSwayTarget;                             // Roll `torsoSway` eases toward, limited to +-700; never written
    s16                          headSwayTarget;                              // Roll `headSway` eases toward: builds while `neckYaw` changes and drops to 0 when it holds
    s16                          forelegYaw;                                  // Yaw applied to part 10, the foot of one foreleg
    s16                          neckYaw;                                     // Yaw spread over the neck parts 2, 3 and 4, limited to +-0x500
    s16                          torsoSway;                                   // Roll applied to parts 22 and 23, which hang below the torso
    s16                          headSway;                                    // Roll applied to parts 6 and 7, which hang below the head
    s16                          prevNeckYaw;                                 // `neckYaw` as the head sway last saw it
    s8                           neckYawEnabled;                              // 1 applies `neckYaw`
    s8                           headSwayEnabled;                             // 1 applies `headSway`
    s8                           forelegYawEnabled;                           // 1 applies `forelegYaw`
    s8                           torsoSwayEnabled;                            // 1 applies `torsoSway`
    u32                          slotCueIndex[24];                            // Cue index last seen per slot, indexed like `slots` and zeroed when the main rig starts a clip; only slot 1's is kept, to fire each sound cue once
    byte                         pad_B4C[0x4];                                // No accesses; role unproven
    _Actor403000CollisionSphere  torsoSphere;                                 // On part 1, the torso; its table is the enemy's hit records
    _Actor403000CollisionSphere  hindSphere;                                  // On part 15, the hindquarters
    _Actor403000CollisionSphere  neckSphere;                                  // On part 4, the end of the neck; a player contact here lands the grab
    _Actor403000CollisionSphere  rootSphere;                                  // On the root; grid-tested only, its contacts push the actor out of walls
    _Actor403000CollisionCapsule rootCapsule;                                 // On the root, reaching ahead along the facing; grid-tested
    _Actor403000CollisionCapsule headCapsule;                                 // On part 5, the head; takes hits, and a player contact here lands the lunge
    MATRIX                       light;                                       // Light matrix the spawn handler binds to the model
    MATRIX                       color;                                       // Colour matrix bound to the model; actor command 10 zeroes it during a scripted clip
    byte                         pad_F50[0x20];                               // No accesses; role unproven
    s16                          hitCooldown;                                 // Frames before another hit registers, from the last hit's parameter
    byte                         pad_F72[0x2];                                // No accesses; role unproven
    SVECTOR                      attackTarget;                                // Latched as an attack starts: the offset to the player for the grab (never read back), the player's position for the drop, which lands on it
    SVECTOR                      lungePlayerPos;                              // Player position as a lunge starts; never read back
    s16                          lungeStep;                                   // Forward step per frame of the lunge, from the distance at its start
    s16                          grabStep;                                    // Forward step per frame of the grab's rush, from the distance at its start
    s16                          seenTargetsDestroyed;                        // `GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED` as last read; a bit newly set near the actor is a blast hit
    byte                         pad_F8A[0x2];                                // No accesses; role unproven
    s16                          roomEventPending;                            // 1 from a killing blow on a downed actor until the dissolve reports it to the room
    byte                         pad_F8E[0x2];                                // No accesses; role unproven
    AnimationPlayRequest         playerAnimation;                             // Request sent to the caught player: this package's catch clips, then the player's own bank back
    u8                           lastCommandStage;                            // Stage tag of the last actor command received; never read back
    u8                           lastCommandArea;                             // Area tag of that command; never read back
    u8                           lastCommandId;                               // Low byte of that command's action; the scripted state acts on 4 and 10
    byte                         pad_FA7[0x1];                                // No accesses; role unproven
    EffectSpawnArg               hitEffectArg;                                // Argument record filled for each hit effect
    SVECTOR                      turnDrift;                                   // Root translation added per frame while turning in place
    SVECTOR                      lastRootPos;                                 // Root position at the previous late ambush frame
    s16                          playerCaught;                                // 1 while the caught player plays the catch clips; the actor cannot be killed meanwhile
    s16                          field_FC2;                                   // Cleared as the walking states start and never read; role unproven
    byte                         pad_FC4[0x4];                                // No accesses; role unproven
    s16                          turnStep;                                    // Yaw added per frame while turning in place
    s16                          lockOnSuspended;                             // 1 while the running state owns the world-target flags; 0 lets the update set them from range and sight
    u16                          catchFrame;                                  // Frames into the caught player's current clip; times the rumble and the sound
    byte                         pad_FCE[0x3];                                // No accesses; role unproven
    s8                           seekRingDir;                                 // Ring direction the approach, retreat and prowl walk
    s8                           patrolRingDir;                               // Ring direction the patrol walks; 0 has the patrol choose on entry
    s8                           watchRingDir;                                // Neighbour the watch faces; ending states hand it to `patrolRingDir` and reverse it
    s8                           patrolGoalCell;                              // Cell the patrol ends at (-1 none)
    s8                           turnRingDir;                                 // Neighbour the turn state turns toward
    s8                           ambushRequested;                             // 1 after a hit taken while watching: the patrol ends in the ambush
    s8                           playerVisible;                               // 1 when the segment test from the actor to the player finds nothing; sampled on odd frames
    s8                           playerInRange;                               // 1 when the player is within 9000; sampled on even frames
    u8                           dropDelay;                                   // Frames the drop waits before it starts
    u8                           blastPuffFrames;                             // Frames left of the puffs that follow a blast hit
    byte                         pad_FDB[0x1];                                // No accesses; role unproven
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Scratch-stack block of the damage step, which runs each frame the actor has
/// health left and no hit cooldown running.
///
/// The step looks for a damaging contact, kind 0x20000, on the torso, hind and
/// neck spheres and then the head capsule, and keeps the first. The blast of a
/// room target destroyed nearby is applied first, as a fixed 400 with no key,
/// and discards the contact found that frame. A contact's damage is rolled for
/// the player's range, taken off the enemy's health and its bearing off the
/// facing handed to the hit reaction. The damage-over-time tick that follows
/// reuses `damage` alone. The block is released before the step returns.
/// Angles are 4096ths of a turn.
typedef struct {
    VECTOR  playerDelta;     // Player's position minus the actor's, world units; `pad` is never written
    SVECTOR hitOffset;       // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing, and `pad` is never written
    SVECTOR hitPos;          // Point of the contact found; `pad` is never written
    s32     hitKey;          // Key of the contact found: the kind over the attack's packed id; 0 when no body holds a damaging contact, and after a blast
    u32     damage;          // Damage being applied: 400 for a blast; the roll for `playerDistance`, times 4 on a critical roll, for a contact; then a quarter of the over-time tick's
    s32     playerDistance;  // Length of `playerDelta`, the range a contact's damage is rolled for
    s16     hitYaw;          // Bearing of `hitOffset` off the actor's facing, wrapped to [-0x800, 0x800]
    byte    unknown_2E[0x6]; // Reserved with the block and never accessed; role unproven
} _Actor403000DamageScratch;
STATIC_ASSERT_SIZEOF(_Actor403000DamageScratch, 0x34);

/// Scratch-stack block of a state body that acts by where the player is: the
/// player's offset and range, a working vector, the yaws worked out from them,
/// the player's reply to a damage message and the two waypoint cells.
///
/// One block serves one frame of one state. Each body fills only the members
/// it needs and the rest are left as the scratch stack had them; one body
/// reads `offset` on frames it has not filled it, taking whatever the stack
/// last held there. Yaws are 4096 units per turn, and a wrapped one lies in
/// [-0x800, 0x800]. A cell is the 0..9 index of a waypoint on the room's ring.
typedef struct {
    VECTOR  playerDelta;    // Player's position minus the actor's on the ground plane (`vy` zero), world units
    SVECTOR offset;         // Working vector: the player's root position minus the actor's, for the bearing; a facing or side axis scaled to a length, as the offset the actor is placed at from the player or the displacement the player is pushed by; the drop target minus the player's position
    s32     playerDistance; // Length of `playerDelta`
    s16     playerYaw;      // Heading the player faces
    s16     yawFromPlayer;  // Bearing from the player to the actor, wrapped: the reverse of `offset`'s. Within a quarter turn of `playerYaw` when the player faces the actor
    s16     turn;           // Wrapped turn from the actor's heading to the player; a body turning at the player limits it and adds the actor's heading, which leaves the yaw the rotation is rebuilt around
    s16     messageResult;  // Reply to `GAME_ACTOR_MESSAGE_APPLY_DAMAGE`; on 1 the body starts the death fade
    s8      cell;           // Cell the actor stands in
    s8      playerCell;     // Cell the player stands in
} _Actor403000ChaseScratch;
STATIC_ASSERT_SIZEOF(_Actor403000ChaseScratch, 0x28);

/// Number of waypoints on the ring the actor patrols; ring indices and cells
/// run 0 to one less and wrap at either end.
enum { ACTOR_403000_RING_WAYPOINT_COUNT = 10 };

/// Scratch-stack block of a state body that moves the actor along the room's
/// ring of ten waypoints: the cells of the actor and the player, the waypoint
/// steered for, its offset and the turn toward it.
///
/// One block serves one frame of one state. A cell is the 0..9 index of the
/// waypoint whose grid cell a position falls in, so stepping a cell by a ring
/// direction of +1 or -1 and wrapping at the ends gives the next waypoint
/// round. Yaws are 4096 units per turn, and a wrapped one lies in
/// [-0x800, 0x800].
typedef struct {
    SVECTOR offset;          // Waypoint's position, then that minus the actor's; the turn state afterwards reuses it for the actor's side and facing axes scaled into its drift
    s16     waypoint;        // Ring index of the waypoint steered for: `cell` stepped by the state's ring direction, wrapped to 0..9
    byte    unknown_A[0x2];  // Reserved with the block and never accessed; role unproven
    s16     turn;            // Wrapped turn from the actor's heading to the waypoint, then that turn limited to the state's step and added to the heading: the yaw the rotation is rebuilt around
    s16     cell;            // Cell the actor stands in
    s16     playerCell;      // Cell the player stands in
    byte    unknown_12[0x2]; // Reserved with the block and never accessed; role unproven
} _Actor403000WaypointScratch;
STATIC_ASSERT_SIZEOF(_Actor403000WaypointScratch, 0x14);

/// Scratch-stack block of the choice of which way round the waypoint ring the
/// actor should go from where it stands.
///
/// The choice looks at the waypoint after the actor's cell in ring order and
/// answers +1, toward it, unless it lies a quarter turn or more to the
/// positive side of the actor's heading, where it answers -1. The answer is
/// read back out of the block after the block is released.
typedef struct {
    SVECTOR offset;         // Position of the waypoint after the actor's cell, then its `vx` and `vz` minus the actor's
    s8      waypoint;       // Ring index of that waypoint, 0..9
    s8      ringDir;        // Direction chosen: +1 up the ring order, -1 down it
    byte    unknown_A[0x2]; // Reserved with the block and never accessed; role unproven
} _Actor403000RingDirScratch;
STATIC_ASSERT_SIZEOF(_Actor403000RingDirScratch, 0xC);

/// A projected screen point, as the one word the GTE stores it in and as its
/// two coordinates.
///
/// The trail keeps the point it drew last as well as the current one, and
/// carries one over to the other with a single word copy.
typedef union {
    s32     word; // Both coordinates at once: X in the low half, Y in the high
    DVECTOR xy;   // Signed screen pixels
} _Actor403000ScreenPoint;
STATIC_ASSERT_SIZEOF(_Actor403000ScreenPoint, 0x4);

/// Scratch-stack block of the ribbon trailing from the actor's flare: the
/// point the trail leaves from, then the projection of one history point
/// after another into the ribbon's quads.
///
/// A block serves one call. `emitter` is hung from the joint the flare is
/// drawn on, at the flare's offset, and its origin in world space becomes the
/// newest of the 18 history points. Each history point is then projected and
/// joined to the one before it by a quad as wide as `edgeOffset` either side.
/// A point whose projection reports an error ends the run of quads, and the
/// first point only seeds `prevScreen`. The routine that records a point
/// without drawing uses `emitter` and `emitterPos` alone and returns without
/// releasing its block, which stays reserved until the main loop resets the
/// scratch stack for the next frame.
typedef struct {
    GfxCoord                emitter;             // Coordinate the trail leaves from: identity rotation, the flare's offset as translation, parented to the flare's joint
    byte                    unknown_50[0x18];    // Reserved with the block and never accessed; role unproven
    SVECTOR                 emitterPos;          // `emitter`'s origin carried up the parent chain into world space; left zero when the chain does not reach the view coordinate. `pad` is never written
    _Actor403000ScreenPoint screen;              // Projection of the history point being drawn
    _Actor403000ScreenPoint prevScreen;          // `screen` of the history point before it
    s32                     depthCue;            // Depth-cue factor of the projection; stored and never read
    s32                     projectionFlags;     // GTE flag word of the projection; negative when it reports an error
    s32                     otz;                 // A quarter of the projected point's screen Z: scales the ribbon's width and picks its ordering-table slot
    byte                    unknown_84[0x4];     // Reserved with the block and never accessed; role unproven
    s32                     prevProjectionFlags; // `projectionFlags` of the history point before it; a quad is drawn only when neither is negative
    SVECTOR                 edgeOffset;          // Screen offset from `screen` to the ribbon's edges: the segment's perpendicular at length 0x1000, scaled to 50 world units at the point's depth when `otz` is positive
    byte                    unknown_94[0x20];    // Reserved with the block and never accessed; role unproven
} _Actor403000TrailScratch;
STATIC_ASSERT_SIZEOF(_Actor403000TrailScratch, 0xB4);

/// Scratch-stack block of the actor's per-frame update, reserved on the
/// frames the update gets past its paused and hidden returns and released
/// before it returns.
///
/// The update samples the player's range on even animation frames and the
/// line of sight on odd ones, so a frame fills either `playerDelta` and
/// `playerDistance` or the two sight positions. `flareOffset` is filled every
/// frame. The state handler the update calls reserves blocks of its own below
/// this one.
typedef struct {
    VECTOR  playerDelta;     // Player's position minus the actor's, world units; `pad` is never written
    byte    unknown_10[0x8]; // Reserved with the block and never accessed; role unproven
    SVECTOR sightTarget;     // Player's composed position: the far end of the sight segment; `pad` is never written
    SVECTOR sightOrigin;     // Actor's composed position: the near end of the sight segment; `pad` is never written
    SVECTOR flareOffset;     // Where the flare is drawn, in the frame of the joint carrying it; `pad` is never written
    s32     playerDistance;  // Length of `playerDelta`; beyond 9000 the player is out of range
    byte    unknown_34[0x4]; // Reserved with the block and never accessed; role unproven
} _Actor403000UpdateScratch;
STATIC_ASSERT_SIZEOF(_Actor403000UpdateScratch, 0x38);

/// The actor's state handlers, stored as a value for whole-table copies.
///
/// `Actor403000Work::state` is the index. The package defines one table; the
/// per-frame update copies all of it to the stack and calls the current
/// state's handler with the actor's task. The call has no terminator or
/// bounds check. The table has 35 slots, of which the 25 `ACTOR_403000_STATE_*`
/// values are filled; the rest are `NULL` and must not be selected.
typedef struct {
    TaskFunc handlers[35]; // Handler of each state, in state order, taking the actor's task
} _Actor403000StateTable;
STATIC_ASSERT_SIZEOF(_Actor403000StateTable, 0x8C);
static const _Actor403000StateTable D_actor_403000_80131F44;

/// Waypoint grid for `_actor403000ChooseRingDirection`: two rows of five indices
/// (row by `coord.t[2]`, column by `coord.t[0]` band), each one less than the
/// `D_actor_403000_80158CE0` entry it selects.
extern u8 D_actor_403000_80158D48[];

/// Static storage for the displacement the actor pushes the player by.
///
/// `push` is the payload of `GAME_ACTOR_MESSAGE_MOVE_BY`, lent to the player
/// for the length of each dispatch. A shoving state aims it once and sends it
/// again every frame the shove lasts, so it has to outlive the frame's scratch
/// block; when the player answers that a wall blocks them, the displacement is
/// zeroed and the later sends move nothing.
///
/// Twelve zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    GameActorMoveBy push;           // Push along the ground: `displacement.vy` is always zero, the player keeps control and every collision update is requested
    u8              unknown_14[12]; // Zero in the image; no access established and role unproven
} _Actor403000MoveByStorage;
STATIC_ASSERT_SIZEOF(_Actor403000MoveByStorage, 32);

/// Static storage for the placement the actor gives the player as it catches
/// them.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, lent to the player
/// for the length of the dispatch, which consumes it. A catch fills it in as
/// it takes hold: the player keeps their position and is turned to a yaw fixed
/// relative to the actor's heading, so the paired animations line up.
///
/// Eight zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    ActorTransform placement;     // Player's own position, with the actor's heading turned by an amount the catch fixes (-0x500, +0x400 or -0x400) as the yaw, and no pitch or roll
    u8             unknown_18[8]; // Zero in the image; no access established and role unproven
} _Actor403000TransformStorage;
STATIC_ASSERT_SIZEOF(_Actor403000TransformStorage, 32);

/// Static storage for the button-press hold the actor puts the player in as it
/// catches them.
///
/// `hold` is the payload of `GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES`, lent to
/// the player for the length of the dispatch. Nothing in the package writes
/// it, so it asks for no presses and the hold completes on its first tick;
/// what the catch uses is the answer, which says whether the player could be
/// taken.
///
/// Eight zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    GameActorButtonPressHold hold;          // Record the player borrows; left zero throughout
    u8                       unknown_18[8]; // Zero in the image; no access established and role unproven
} _Actor403000ButtonPressHoldStorage;
STATIC_ASSERT_SIZEOF(_Actor403000ButtonPressHoldStorage, 32);

/// Pairs of hit-effect vectors `_actor403000SpawnHitEffects` picks from by
/// turn magnitude; `pad` indexes the display object's coordinate parts.
extern SVECTOR D_actor_403000_80158C48[];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id -- `_actor403000WatchState` scales a byte id by 8 into it
/// the same way -- so a record's `vx`/`vy`/`vz` are the vector an actor's
/// handlers copy out of it. Lives in the overlay's trailing data region.
extern SVECTOR D_actor_403000_80158CE0[];

/// Four trigger points (`vx`/`vz` used) `_actor403000CheckTargetBlast` measures
/// the display object against, one per bit of `gameFlagGetNibble(0xE2)`.
extern SVECTOR D_actor_403000_80158D64[];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Turn joint `coord` by `yaw` about Y in view space, keeping it expressed in
/// its parent's frame.

/// Eases a joint-angle lvalue toward a symmetrically clamped target.
///
/// `sway` and `target` must be distinct, side-effect-free writable s16 lvalues.
/// The target is captured once, then replaced by its clamped value; both
/// lvalues are evaluated repeatedly. `limit` and `step` must be positive,
/// side-effect-free angle quantities fitting s16 and are evaluated repeatedly.
/// All quantities use the same angle units. Captures no caller locals and
/// forms one compound statement; callers provide the trailing semicolon.
#define ACTOR_403000_EASE_JOINT_ANGLE(sway, target, limit, step) \
    {                                                            \
        s16 requestedTarget;                                     \
        s32 difference;                                          \
                                                                 \
        requestedTarget = (target);                              \
        if (requestedTarget > (limit)) {                         \
            (target) = (limit);                                  \
        }                                                        \
        if (requestedTarget < -(limit)) {                        \
            (target) = -(limit);                                 \
        }                                                        \
        if ((sway) < (target)) {                                 \
            if ((target) - (sway) > (step)) {                    \
                (sway) += (step);                                \
            } else {                                             \
                (sway) = (target);                               \
            }                                                    \
        }                                                        \
        if ((target) < (sway)) {                                 \
            difference = (sway) - (target);                      \
            if (difference < 0) {                                \
                difference = -difference;                        \
            }                                                    \
            if (difference > (step)) {                           \
                (sway) -= (step);                                \
            } else {                                             \
                (sway) = (target);                               \
            }                                                    \
        }                                                        \
    }

/// Arena geometry, clip and message identities used by the state callbacks.
///
/// Player clip IDs index the borrowed catch banks; the aim-state slot keeps
/// scripted playback running after fatal damage without the usual death fallback.
enum {
    ACTOR_403000_ACTIVE_TORSO_RADIUS     = 1000,
    ACTOR_403000_RING_PROBE_REACH        = 800,
    ACTOR_403000_CLIP_WALK               = 2,
    ACTOR_403000_CLIP_STAND              = 4,
    ACTOR_403000_CLIP_DROP               = 8,
    ACTOR_403000_PLAYER_FATAL_HOLD_STATE = 10,
    ACTOR_403000_SOUND_CATCH             = 7,
};

static void _actor403000UpdateAnimation(Task* task);

static s32 _actor403000ChooseRingDirection(GfxCoord* rootCoord);

/// Step `coord` by the movement the first `count` records of `recs` resolve
/// to; returns whether the actor moved on X or Z.

/// Report whether the root capsule's contact table holds a live entry:
/// the walk stops at the first empty `key` and answers 1 if any record it
/// passed carried the 0x10 kind bits.
static s16 func_actor_403000_8013D48C(Task* task);

static void _actor403000Exit(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
static void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

static void _actor403000HiddenState(Task* actorTask);

/// Per-frame update for the actor once its work block exists: on the frame
/// `stateEntered` is set, reinstate the display object's buffers and restart the
/// animation state machine on clip 0x10, then tick `stateFrame` and the playback
/// state, and when slot 1 has `ANIMATION_SLOT_SETTLED`, raise
/// `patrolRingDir`/`watchRingDir` and move the state machine to the patrol.
static void func_actor_403000_8013D850(Task* arg0);

/// Per-frame countdown: on the frame `stateEntered` is set, reload the `stateFrame`
/// tick from a fresh `gRandomLcgState` draw masked to 0xA..0x19, then decrement
/// it. When the tick underflows and the enemy still has HP left
/// (`Enemy::hp`), `state` becomes `ACTOR_403000_STATE_GET_UP`.
static void func_actor_403000_8013D910(Task* arg0);

extern EnemyParams   D_actor_403000_8013DA00;
extern EnemyParams   D_actor_403000_8013DA10;
extern AnimationSet* D_actor_403000_80158B50[46];
extern AnimationSet* D_actor_403000_80158C08[8];
extern AnimationSet* D_actor_403000_80158C28[8];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_403000_80158CA8[7];

extern _Actor403000TransformStorage D_actor_403000_80158D90;

extern _Actor403000MoveByStorage D_actor_403000_80158DB0;

extern _Actor403000ButtonPressHoldStorage D_actor_403000_80158DD0;

/// Trail history `func_actor_403000_801330D4` shifts down one slot per call,
/// storing the newest position in slot 0.
extern SVECTOR D_actor_403000_80158DF0[18];
extern s8      D_actor_403000_80158364[45][45];

extern ActorCommand D_actor_403000_80158D8C;

/// Integer part of the last movement step `_actorContactApplyGridPushback`
/// applied to the actor's root coordinate.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_actor_403000_80132AE0(GfxCoord* coord);
static void func_actor_403000_80134F44(Task* arg0);
static void func_actor_403000_8013D72C(Task* arg0);
static void _actor403000DownHitState(Task* actorTask);
static void _actor403000GrabState(Task* actorTask);
static void _actor403000DropState(Task* actorTask);

static AnimationSet _gActor403000Animation139B8;
static AnimationSet _gActor403000Animation14AB4;
static AnimationSet _gActor403000Animation15528;
static AnimationSet _gActor403000Animation1607C;
static AnimationSet _gActor403000Animation16A1C;
static AnimationSet _gActor403000Animation17038;
static AnimationSet _gActor403000Animation17694;
static AnimationSet _gActor403000Animation189B4;
static AnimationSet _gActor403000Animation19984;
static AnimationSet _gActor403000Animation1A69C;
static AnimationSet _gActor403000Animation1B000;
static AnimationSet _gActor403000Animation1BABC;
static AnimationSet _gActor403000Animation1C69C;
static AnimationSet _gActor403000Animation1CE64;
static AnimationSet _gActor403000Animation1D9E8;
static AnimationSet _gActor403000Animation1E058;
static AnimationSet _gActor403000Animation1EAC4;
static AnimationSet _gActor403000Animation20714;
static AnimationSet _gActor403000Animation21A1C;
static AnimationSet _gActor403000Animation22500;
static AnimationSet _gActor403000Animation229E8;
static AnimationSet _gActor403000Animation23230;
static AnimationSet _gActor403000Animation239CC;
static AnimationSet _gActor403000Animation241D8;
static AnimationSet _gActor403000Animation249EC;
static AnimationSet _gActor403000Animation24BD0;
static AnimationSet _gActor403000Animation250C4;
static AnimationSet _gActor403000Animation25484;
static AnimationSet _gActor403000Animation25B24;
static AnimationSet _gActor403000Animation26334;
static AnimationSet _gActor403000Animation2651C;
static TmdSource    _gActor403000BlizzardChaserBody;
enum {
    ACTOR_403000_MESSAGE_IGNORED        = 2015,
    ACTOR_403000_PREVIOUS_STATE_INVALID = -1,
};

static s32  _actor403000ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32  _actor403000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32  _actor403000IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32  _actor403000Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);
static s32  _actor403000PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static void func_actor_403000_8013D59C(Task*);
static void _actor403000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

DamageAttack D_actor_403000_8013D9E0[4] = {
    { 30, 7 },
    { 20, 7 },
    { 20, 7 },
    { 25, 0 },
};

DamageAttack D_actor_403000_8013D9F0[4] = {
    { 20, 3 },
    { 12, 7 },
    { 12, 7 },
    { 18, 0 },
};

EnemyParams D_actor_403000_8013DA00 = { D_actor_403000_8013D9E0, 500, 300, 200, 10, 100, 6, 100, 0 };

EnemyParams D_actor_403000_8013DA10 = { D_actor_403000_8013D9F0, 2500, 500, 300, 30, 100, 3, 100, 20 };

static TmdBone _gActor403000BlizzardChaserBodySkeleton[24] = {
#include "assets/blizzard_chaser_body_skeleton.inc"
};

static u32 _gActor403000BlizzardChaserBodyPartVerts[24] = {
#include "assets/blizzard_chaser_body_partVerts.inc"
};

static SVECTOR _gActor403000BlizzardChaserBodyVerts[398] = {
#include "assets/blizzard_chaser_body_verts.inc"
};

static SVECTOR _gActor403000BlizzardChaserBodyNormals[410] = {
#include "assets/blizzard_chaser_body_normals.inc"
};

static u32 _gActor403000BlizzardChaserBodyStream[5889] = {
#include "assets/blizzard_chaser_body_stream.inc"
};

static TmdSource _gActor403000BlizzardChaserBody = {
    0,
    29692,
    10264,
    24,
    _gActor403000BlizzardChaserBodyPartVerts,
    _gActor403000BlizzardChaserBodyVerts,
    _gActor403000BlizzardChaserBodyNormals,
    _gActor403000BlizzardChaserBodySkeleton,
    _gActor403000BlizzardChaserBodyStream,
};

static AnimationPackedPose _gActor403000Animation139B8Bank1[6] = {
#include "assets/actor_403000_animation_139B8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation139B8Bank4[73] = {
#include "assets/actor_403000_animation_139B8_bank4.inc"
};

static AnimationRecord _gActor403000Animation139B8Records[189] = {
#include "assets/actor_403000_animation_139B8_records.inc"
};

static u16 _gActor403000Animation139B8Indices[24] = {
#include "assets/actor_403000_animation_139B8_indices.inc"
};

static AnimationSet _gActor403000Animation139B8 = {
    _gActor403000Animation139B8Records,
    _gActor403000Animation139B8Indices,
    { NULL, _gActor403000Animation139B8Bank1, NULL, NULL, _gActor403000Animation139B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation14AB4Bank1[49] = {
#include "assets/actor_403000_animation_14AB4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation14AB4Bank4[361] = {
#include "assets/actor_403000_animation_14AB4_bank4.inc"
};

static AnimationRecord _gActor403000Animation14AB4Records[557] = {
#include "assets/actor_403000_animation_14AB4_records.inc"
};

static u16 _gActor403000Animation14AB4Indices[24] = {
#include "assets/actor_403000_animation_14AB4_indices.inc"
};

static AnimationSet _gActor403000Animation14AB4 = {
    _gActor403000Animation14AB4Records,
    _gActor403000Animation14AB4Indices,
    { NULL, _gActor403000Animation14AB4Bank1, NULL, NULL, _gActor403000Animation14AB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation15528Bank1[28] = {
#include "assets/actor_403000_animation_15528_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation15528Bank4[211] = {
#include "assets/actor_403000_animation_15528_bank4.inc"
};

static AnimationRecord _gActor403000Animation15528Records[352] = {
#include "assets/actor_403000_animation_15528_records.inc"
};

static u16 _gActor403000Animation15528Indices[24] = {
#include "assets/actor_403000_animation_15528_indices.inc"
};

static AnimationSet _gActor403000Animation15528 = {
    _gActor403000Animation15528Records,
    _gActor403000Animation15528Indices,
    { NULL, _gActor403000Animation15528Bank1, NULL, NULL, _gActor403000Animation15528Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1607CBank1[28] = {
#include "assets/actor_403000_animation_1607C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1607CBank4[257] = {
#include "assets/actor_403000_animation_1607C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1607CRecords[362] = {
#include "assets/actor_403000_animation_1607C_records.inc"
};

static u16 _gActor403000Animation1607CIndices[24] = {
#include "assets/actor_403000_animation_1607C_indices.inc"
};

static AnimationSet _gActor403000Animation1607C = {
    _gActor403000Animation1607CRecords,
    _gActor403000Animation1607CIndices,
    { NULL, _gActor403000Animation1607CBank1, NULL, NULL, _gActor403000Animation1607CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation16A1CBank1[14] = {
#include "assets/actor_403000_animation_16A1C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation16A1CBank4[192] = {
#include "assets/actor_403000_animation_16A1C_bank4.inc"
};

static AnimationRecord _gActor403000Animation16A1CRecords[360] = {
#include "assets/actor_403000_animation_16A1C_records.inc"
};

static u16 _gActor403000Animation16A1CIndices[24] = {
#include "assets/actor_403000_animation_16A1C_indices.inc"
};

static AnimationSet _gActor403000Animation16A1C = {
    _gActor403000Animation16A1CRecords,
    _gActor403000Animation16A1CIndices,
    { NULL, _gActor403000Animation16A1CBank1, NULL, NULL, _gActor403000Animation16A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation17038Bank1[10] = {
#include "assets/actor_403000_animation_17038_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation17038Bank4[142] = {
#include "assets/actor_403000_animation_17038_bank4.inc"
};

static AnimationRecord _gActor403000Animation17038Records[197] = {
#include "assets/actor_403000_animation_17038_records.inc"
};

static u16 _gActor403000Animation17038Indices[24] = {
#include "assets/actor_403000_animation_17038_indices.inc"
};

static AnimationSet _gActor403000Animation17038 = {
    _gActor403000Animation17038Records,
    _gActor403000Animation17038Indices,
    { NULL, _gActor403000Animation17038Bank1, NULL, NULL, _gActor403000Animation17038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation17694Bank1[14] = {
#include "assets/actor_403000_animation_17694_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation17694Bank4[140] = {
#include "assets/actor_403000_animation_17694_bank4.inc"
};

static AnimationRecord _gActor403000Animation17694Records[203] = {
#include "assets/actor_403000_animation_17694_records.inc"
};

static u16 _gActor403000Animation17694Indices[24] = {
#include "assets/actor_403000_animation_17694_indices.inc"
};

static AnimationSet _gActor403000Animation17694 = {
    _gActor403000Animation17694Records,
    _gActor403000Animation17694Indices,
    { NULL, _gActor403000Animation17694Bank1, NULL, NULL, _gActor403000Animation17694Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation189B4Bank1[39] = {
#include "assets/actor_403000_animation_189B4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation189B4Bank4[474] = {
#include "assets/actor_403000_animation_189B4_bank4.inc"
};

static AnimationRecord _gActor403000Animation189B4Records[611] = {
#include "assets/actor_403000_animation_189B4_records.inc"
};

static u16 _gActor403000Animation189B4Indices[24] = {
#include "assets/actor_403000_animation_189B4_indices.inc"
};

static AnimationSet _gActor403000Animation189B4 = {
    _gActor403000Animation189B4Records,
    _gActor403000Animation189B4Indices,
    { NULL, _gActor403000Animation189B4Bank1, NULL, NULL, _gActor403000Animation189B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation19984Bank1[26] = {
#include "assets/actor_403000_animation_19984_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation19984Bank4[401] = {
#include "assets/actor_403000_animation_19984_bank4.inc"
};

static AnimationRecord _gActor403000Animation19984Records[511] = {
#include "assets/actor_403000_animation_19984_records.inc"
};

static u16 _gActor403000Animation19984Indices[24] = {
#include "assets/actor_403000_animation_19984_indices.inc"
};

static AnimationSet _gActor403000Animation19984 = {
    _gActor403000Animation19984Records,
    _gActor403000Animation19984Indices,
    { NULL, _gActor403000Animation19984Bank1, NULL, NULL, _gActor403000Animation19984Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1A69CBank1[21] = {
#include "assets/actor_403000_animation_1A69C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1A69CBank4[325] = {
#include "assets/actor_403000_animation_1A69C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1A69CRecords[428] = {
#include "assets/actor_403000_animation_1A69C_records.inc"
};

static u16 _gActor403000Animation1A69CIndices[24] = {
#include "assets/actor_403000_animation_1A69C_indices.inc"
};

static AnimationSet _gActor403000Animation1A69C = {
    _gActor403000Animation1A69CRecords,
    _gActor403000Animation1A69CIndices,
    { NULL, _gActor403000Animation1A69CBank1, NULL, NULL, _gActor403000Animation1A69CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1B000Bank1[24] = {
#include "assets/actor_403000_animation_1B000_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1B000Bank4[220] = {
#include "assets/actor_403000_animation_1B000_bank4.inc"
};

static AnimationRecord _gActor403000Animation1B000Records[287] = {
#include "assets/actor_403000_animation_1B000_records.inc"
};

static u16 _gActor403000Animation1B000Indices[24] = {
#include "assets/actor_403000_animation_1B000_indices.inc"
};

static AnimationSet _gActor403000Animation1B000 = {
    _gActor403000Animation1B000Records,
    _gActor403000Animation1B000Indices,
    { NULL, _gActor403000Animation1B000Bank1, NULL, NULL, _gActor403000Animation1B000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1BABCBank1[23] = {
#include "assets/actor_403000_animation_1BABC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1BABCBank4[258] = {
#include "assets/actor_403000_animation_1BABC_bank4.inc"
};

static AnimationRecord _gActor403000Animation1BABCRecords[338] = {
#include "assets/actor_403000_animation_1BABC_records.inc"
};

static u16 _gActor403000Animation1BABCIndices[24] = {
#include "assets/actor_403000_animation_1BABC_indices.inc"
};

static AnimationSet _gActor403000Animation1BABC = {
    _gActor403000Animation1BABCRecords,
    _gActor403000Animation1BABCIndices,
    { NULL, _gActor403000Animation1BABCBank1, NULL, NULL, _gActor403000Animation1BABCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1C69CBank1[23] = {
#include "assets/actor_403000_animation_1C69C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1C69CBank4[296] = {
#include "assets/actor_403000_animation_1C69C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1C69CRecords[373] = {
#include "assets/actor_403000_animation_1C69C_records.inc"
};

static u16 _gActor403000Animation1C69CIndices[24] = {
#include "assets/actor_403000_animation_1C69C_indices.inc"
};

static AnimationSet _gActor403000Animation1C69C = {
    _gActor403000Animation1C69CRecords,
    _gActor403000Animation1C69CIndices,
    { NULL, _gActor403000Animation1C69CBank1, NULL, NULL, _gActor403000Animation1C69CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1CE64Bank1[11] = {
#include "assets/actor_403000_animation_1CE64_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1CE64Bank4[184] = {
#include "assets/actor_403000_animation_1CE64_bank4.inc"
};

static AnimationRecord _gActor403000Animation1CE64Records[259] = {
#include "assets/actor_403000_animation_1CE64_records.inc"
};

static u16 _gActor403000Animation1CE64Indices[24] = {
#include "assets/actor_403000_animation_1CE64_indices.inc"
};

static AnimationSet _gActor403000Animation1CE64 = {
    _gActor403000Animation1CE64Records,
    _gActor403000Animation1CE64Indices,
    { NULL, _gActor403000Animation1CE64Bank1, NULL, NULL, _gActor403000Animation1CE64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1D9E8Bank1[16] = {
#include "assets/actor_403000_animation_1D9E8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1D9E8Bank4[288] = {
#include "assets/actor_403000_animation_1D9E8_bank4.inc"
};

static AnimationRecord _gActor403000Animation1D9E8Records[379] = {
#include "assets/actor_403000_animation_1D9E8_records.inc"
};

static u16 _gActor403000Animation1D9E8Indices[24] = {
#include "assets/actor_403000_animation_1D9E8_indices.inc"
};

static AnimationSet _gActor403000Animation1D9E8 = {
    _gActor403000Animation1D9E8Records,
    _gActor403000Animation1D9E8Indices,
    { NULL, _gActor403000Animation1D9E8Bank1, NULL, NULL, _gActor403000Animation1D9E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1E058Bank1[7] = {
#include "assets/actor_403000_animation_1E058_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1E058Bank4[122] = {
#include "assets/actor_403000_animation_1E058_bank4.inc"
};

static AnimationRecord _gActor403000Animation1E058Records[247] = {
#include "assets/actor_403000_animation_1E058_records.inc"
};

static u16 _gActor403000Animation1E058Indices[24] = {
#include "assets/actor_403000_animation_1E058_indices.inc"
};

static AnimationSet _gActor403000Animation1E058 = {
    _gActor403000Animation1E058Records,
    _gActor403000Animation1E058Indices,
    { NULL, _gActor403000Animation1E058Bank1, NULL, NULL, _gActor403000Animation1E058Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1EAC4Bank1[15] = {
#include "assets/actor_403000_animation_1EAC4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1EAC4Bank4[272] = {
#include "assets/actor_403000_animation_1EAC4_bank4.inc"
};

static AnimationRecord _gActor403000Animation1EAC4Records[328] = {
#include "assets/actor_403000_animation_1EAC4_records.inc"
};

static u16 _gActor403000Animation1EAC4Indices[24] = {
#include "assets/actor_403000_animation_1EAC4_indices.inc"
};

static AnimationSet _gActor403000Animation1EAC4 = {
    _gActor403000Animation1EAC4Records,
    _gActor403000Animation1EAC4Indices,
    { NULL, _gActor403000Animation1EAC4Bank1, NULL, NULL, _gActor403000Animation1EAC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation20714Bank1[47] = {
#include "assets/actor_403000_animation_20714_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation20714Bank4[726] = {
#include "assets/actor_403000_animation_20714_bank4.inc"
};

static AnimationRecord _gActor403000Animation20714Records[923] = {
#include "assets/actor_403000_animation_20714_records.inc"
};

static u16 _gActor403000Animation20714Indices[24] = {
#include "assets/actor_403000_animation_20714_indices.inc"
};

static AnimationSet _gActor403000Animation20714 = {
    _gActor403000Animation20714Records,
    _gActor403000Animation20714Indices,
    { NULL, _gActor403000Animation20714Bank1, NULL, NULL, _gActor403000Animation20714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation21A1CBank1[49] = {
#include "assets/actor_403000_animation_21A1C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation21A1CBank4[476] = {
#include "assets/actor_403000_animation_21A1C_bank4.inc"
};

static AnimationRecord _gActor403000Animation21A1CRecords[575] = {
#include "assets/actor_403000_animation_21A1C_records.inc"
};

static u16 _gActor403000Animation21A1CIndices[20] = {
#include "assets/actor_403000_animation_21A1C_indices.inc"
};

static AnimationSet _gActor403000Animation21A1C = {
    _gActor403000Animation21A1CRecords,
    _gActor403000Animation21A1CIndices,
    { NULL, _gActor403000Animation21A1CBank1, NULL, NULL, _gActor403000Animation21A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation22500Bank1[20] = {
#include "assets/actor_403000_animation_22500_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation22500Bank4[283] = {
#include "assets/actor_403000_animation_22500_bank4.inc"
};

static AnimationRecord _gActor403000Animation22500Records[334] = {
#include "assets/actor_403000_animation_22500_records.inc"
};

static u16 _gActor403000Animation22500Indices[20] = {
#include "assets/actor_403000_animation_22500_indices.inc"
};

static AnimationSet _gActor403000Animation22500 = {
    _gActor403000Animation22500Records,
    _gActor403000Animation22500Indices,
    { NULL, _gActor403000Animation22500Bank1, NULL, NULL, _gActor403000Animation22500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation229E8Bank1[10] = {
#include "assets/actor_403000_animation_229E8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation229E8Bank4[110] = {
#include "assets/actor_403000_animation_229E8_bank4.inc"
};

static AnimationRecord _gActor403000Animation229E8Records[154] = {
#include "assets/actor_403000_animation_229E8_records.inc"
};

static u16 _gActor403000Animation229E8Indices[20] = {
#include "assets/actor_403000_animation_229E8_indices.inc"
};

static AnimationSet _gActor403000Animation229E8 = {
    _gActor403000Animation229E8Records,
    _gActor403000Animation229E8Indices,
    { NULL, _gActor403000Animation229E8Bank1, NULL, NULL, _gActor403000Animation229E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation23230Bank1[21] = {
#include "assets/actor_403000_animation_23230_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation23230Bank4[193] = {
#include "assets/actor_403000_animation_23230_bank4.inc"
};

static AnimationRecord _gActor403000Animation23230Records[254] = {
#include "assets/actor_403000_animation_23230_records.inc"
};

static u16 _gActor403000Animation23230Indices[20] = {
#include "assets/actor_403000_animation_23230_indices.inc"
};

static AnimationSet _gActor403000Animation23230 = {
    _gActor403000Animation23230Records,
    _gActor403000Animation23230Indices,
    { NULL, _gActor403000Animation23230Bank1, NULL, NULL, _gActor403000Animation23230Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation239CCBank1[20] = {
#include "assets/actor_403000_animation_239CC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation239CCBank4[172] = {
#include "assets/actor_403000_animation_239CC_bank4.inc"
};

static AnimationRecord _gActor403000Animation239CCRecords[235] = {
#include "assets/actor_403000_animation_239CC_records.inc"
};

static u16 _gActor403000Animation239CCIndices[20] = {
#include "assets/actor_403000_animation_239CC_indices.inc"
};

static AnimationSet _gActor403000Animation239CC = {
    _gActor403000Animation239CCRecords,
    _gActor403000Animation239CCIndices,
    { NULL, _gActor403000Animation239CCBank1, NULL, NULL, _gActor403000Animation239CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation241D8Bank1[15] = {
#include "assets/actor_403000_animation_241D8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation241D8Bank4[206] = {
#include "assets/actor_403000_animation_241D8_bank4.inc"
};

static AnimationRecord _gActor403000Animation241D8Records[244] = {
#include "assets/actor_403000_animation_241D8_records.inc"
};

static u16 _gActor403000Animation241D8Indices[20] = {
#include "assets/actor_403000_animation_241D8_indices.inc"
};

static AnimationSet _gActor403000Animation241D8 = {
    _gActor403000Animation241D8Records,
    _gActor403000Animation241D8Indices,
    { NULL, _gActor403000Animation241D8Bank1, NULL, NULL, _gActor403000Animation241D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation249ECBank1[14] = {
#include "assets/actor_403000_animation_249EC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation249ECBank4[207] = {
#include "assets/actor_403000_animation_249EC_bank4.inc"
};

static AnimationRecord _gActor403000Animation249ECRecords[248] = {
#include "assets/actor_403000_animation_249EC_records.inc"
};

static u16 _gActor403000Animation249ECIndices[20] = {
#include "assets/actor_403000_animation_249EC_indices.inc"
};

static AnimationSet _gActor403000Animation249EC = {
    _gActor403000Animation249ECRecords,
    _gActor403000Animation249ECIndices,
    { NULL, _gActor403000Animation249ECBank1, NULL, NULL, _gActor403000Animation249ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation24BD0Bank1[2] = {
#include "assets/actor_403000_animation_24BD0_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation24BD0Bank4[21] = {
#include "assets/actor_403000_animation_24BD0_bank4.inc"
};

static AnimationRecord _gActor403000Animation24BD0Records[72] = {
#include "assets/actor_403000_animation_24BD0_records.inc"
};

static u16 _gActor403000Animation24BD0Indices[24] = {
#include "assets/actor_403000_animation_24BD0_indices.inc"
};

static AnimationSet _gActor403000Animation24BD0 = {
    _gActor403000Animation24BD0Records,
    _gActor403000Animation24BD0Indices,
    { NULL, _gActor403000Animation24BD0Bank1, NULL, NULL, _gActor403000Animation24BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation250C4Bank1[8] = {
#include "assets/actor_403000_animation_250C4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation250C4Bank4[103] = {
#include "assets/actor_403000_animation_250C4_bank4.inc"
};

static AnimationRecord _gActor403000Animation250C4Records[168] = {
#include "assets/actor_403000_animation_250C4_records.inc"
};

static u16 _gActor403000Animation250C4Indices[24] = {
#include "assets/actor_403000_animation_250C4_indices.inc"
};

static AnimationSet _gActor403000Animation250C4 = {
    _gActor403000Animation250C4Records,
    _gActor403000Animation250C4Indices,
    { NULL, _gActor403000Animation250C4Bank1, NULL, NULL, _gActor403000Animation250C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation25484Bank1[6] = {
#include "assets/actor_403000_animation_25484_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation25484Bank4[78] = {
#include "assets/actor_403000_animation_25484_bank4.inc"
};

static AnimationRecord _gActor403000Animation25484Records[122] = {
#include "assets/actor_403000_animation_25484_records.inc"
};

static u16 _gActor403000Animation25484Indices[24] = {
#include "assets/actor_403000_animation_25484_indices.inc"
};

static AnimationSet _gActor403000Animation25484 = {
    _gActor403000Animation25484Records,
    _gActor403000Animation25484Indices,
    { NULL, _gActor403000Animation25484Bank1, NULL, NULL, _gActor403000Animation25484Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation25B24Bank1[10] = {
#include "assets/actor_403000_animation_25B24_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation25B24Bank4[156] = {
#include "assets/actor_403000_animation_25B24_bank4.inc"
};

static AnimationRecord _gActor403000Animation25B24Records[216] = {
#include "assets/actor_403000_animation_25B24_records.inc"
};

static u16 _gActor403000Animation25B24Indices[24] = {
#include "assets/actor_403000_animation_25B24_indices.inc"
};

static AnimationSet _gActor403000Animation25B24 = {
    _gActor403000Animation25B24Records,
    _gActor403000Animation25B24Indices,
    { NULL, _gActor403000Animation25B24Bank1, NULL, NULL, _gActor403000Animation25B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation26334Bank1[13] = {
#include "assets/actor_403000_animation_26334_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation26334Bank4[171] = {
#include "assets/actor_403000_animation_26334_bank4.inc"
};

static AnimationRecord _gActor403000Animation26334Records[284] = {
#include "assets/actor_403000_animation_26334_records.inc"
};

static u16 _gActor403000Animation26334Indices[24] = {
#include "assets/actor_403000_animation_26334_indices.inc"
};

static AnimationSet _gActor403000Animation26334 = {
    _gActor403000Animation26334Records,
    _gActor403000Animation26334Indices,
    { NULL, _gActor403000Animation26334Bank1, NULL, NULL, _gActor403000Animation26334Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation2651CBank1[2] = {
#include "assets/actor_403000_animation_2651C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation2651CBank4[22] = {
#include "assets/actor_403000_animation_2651C_bank4.inc"
};

static AnimationRecord _gActor403000Animation2651CRecords[72] = {
#include "assets/actor_403000_animation_2651C_records.inc"
};

static u16 _gActor403000Animation2651CIndices[24] = {
#include "assets/actor_403000_animation_2651C_indices.inc"
};

static AnimationSet _gActor403000Animation2651C = {
    _gActor403000Animation2651CRecords,
    _gActor403000Animation2651CIndices,
    { NULL, _gActor403000Animation2651CBank1, NULL, NULL, _gActor403000Animation2651CBank4, NULL, NULL, NULL },
};

s8 D_actor_403000_80158364[45][45] = {
    /*  0 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  1 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  2 */ { 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  3 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  4 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  5 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  6 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  7 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  8 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  9 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 10 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 11 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 12 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 13 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 14 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 15 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 16 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 17 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 18 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 19 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 20 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 21 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 22 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 23 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 24 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 25 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 26 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 27 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 28 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 29 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 30 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 31 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 32 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 33 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 34 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 35 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 36 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 37 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 38 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 39 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 40 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 41 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 42 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 43 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 44 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_403000_80158B50[46] = {
    &_gActor403000Animation139B8,
    &_gActor403000Animation14AB4,
    &_gActor403000Animation15528,
    &_gActor403000Animation1607C,
    &_gActor403000Animation16A1C,
    &_gActor403000Animation17038,
    &_gActor403000Animation17694,
    &_gActor403000Animation189B4,
    &_gActor403000Animation19984,
    &_gActor403000Animation1A69C,
    &_gActor403000Animation1B000,
    &_gActor403000Animation1BABC,
    &_gActor403000Animation1C69C,
    &_gActor403000Animation1CE64,
    &_gActor403000Animation1D9E8,
    &_gActor403000Animation1E058,
    &_gActor403000Animation1EAC4,
    &_gActor403000Animation20714,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor403000Animation24BD0,
    &_gActor403000Animation250C4,
    &_gActor403000Animation25484,
    &_gActor403000Animation25B24,
    &_gActor403000Animation2651C,
    &_gActor403000Animation26334,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403000_80158C08[8] = {
    NULL,
    &_gActor403000Animation21A1C,
    &_gActor403000Animation22500,
    &_gActor403000Animation23230,
    &_gActor403000Animation241D8,
    &_gActor403000Animation229E8,
    &_gActor403000Animation249EC,
    NULL,
};

AnimationSet* D_actor_403000_80158C28[8] = {
    NULL,
    &_gActor403000Animation21A1C,
    &_gActor403000Animation22500,
    &_gActor403000Animation239CC,
    &_gActor403000Animation249EC,
    &_gActor403000Animation229E8,
    &_gActor403000Animation249EC,
    NULL,
};

SVECTOR D_actor_403000_80158C48[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

TaskMessageEntry D_actor_403000_80158CA8[7] = {
    { ACTOR_403000_MESSAGE_IGNORED, _actor403000IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor403000SetModelDraw },
    { ACTOR_MESSAGE_IS_PRESENT, _actor403000IsPresent },
    { ACTOR_MESSAGE_PLACE, _actor403000Place },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor403000ApplyCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor403000PlayAnimation },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_403000_80158CE0[13] = {
    { 0x445C, 2, 2100, 0 },
    { 0x33C2, 2, 2100, 0 },
    { 8900, 2, 2100, 0 },
    { 5100, 2, 2100, 0 },
    { 500, 2, 2100, 0 },
    { 500, 2, 6700, 0 },
    { 5100, 2, 6700, 0 },
    { 8900, 2, 6700, 0 },
    { 0x33C2, 2, 6700, 0 },
    { 0x445C, 2, 6700, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

u8 D_actor_403000_80158D48[16] = {
    0,
    1,
    2,
    3,
    4,
    9,
    8,
    7,
    6,
    5,
    0,
    0,
    1,
    0,
    3,
    2,
};

TaskDesc D_actor_403000_80158D58 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_403000_8013D59C, { .model = &_gActor403000BlizzardChaserBody } };

SVECTOR D_actor_403000_80158D64[4] = {
    { 4550, 0, 310, 0 },
    { 0x3566, 0, 330, 0 },
    { 4340, 0, 8710, 0 },
    { 0x3552, 0, 8710, 0 },
};

SVECTOR ActorContact_ScratchPosition = { 0 };

ActorCommand D_actor_403000_80158D8C = { 0 };

_Actor403000TransformStorage D_actor_403000_80158D90;

_Actor403000MoveByStorage D_actor_403000_80158DB0;

_Actor403000ButtonPressHoldStorage D_actor_403000_80158DD0;

SVECTOR D_actor_403000_80158DF0[18];

static void                func_actor_403000_801327B0(GfxCoord* coord, SVECTOR* pos, s32 arg2);
static void                func_actor_403000_801330D4(GfxCoord* parent);
static void                _actor403000UpdateTorsoSway(Task* task);
static void                _actor403000UpdateHeadSway(Task* task);
static void                _actor403000TickBlendedSlots(Task* task);
static s32                 _actor403000PollAnimationSound(Task* task, Actor403000Work* work);
static s32                 _actor403000CanChasePlayer(Task* task, s16 actorCell, s16 playerCell);
static void                _actor403000Spawn(Enemy* enemy, Task* task);
static void                _actor403000SpawnHitEffects(Task* task, s16 hitYaw, s32 attackKey);
static s32                 _actor403000CheckTargetBlast(Task* task);
static inline s32          func_actor_403000_FindHit(SVECTOR* pos, WorldCollisionContact* records);
static inline s16          func_actor_403000_WrapAngle(s16 angle);
static inline void         func_actor_403000_PlaySound(Task* arg0, Enemy* enemy, s32 id);
static void                _actor403000StunnedState(Task* task);
static void                func_actor_403000_8013603C(Task* arg0);
static void                func_actor_403000_801365D0(Task* arg0);
static void                func_actor_403000_80136B14(Task* arg0);
static void                func_actor_403000_80136D68(Task* arg0);
static __inline__ void     _actor403000ScaleVectorQ12(SVECTOR* vector, u16 scaleQ12);
static __inline__ SVECTOR* _actor403000ReserveScratchVector(void);
static __inline__ void     _actor403000ReleaseScratchVector(void);
static inline s8           _actor403000GetRingCell(GfxCoord* coord);
static void                _actor403000ChaseState(Task* actorTask);
static void                _actor403000GrabCatchState(Task* actorTask);
static void                _actor403000LungeState(Task* actorTask);
static void                func_actor_403000_80138DB0(Task* arg0);
static void                _actor403000KnockdownState(Task* actorTask);
static void                _actor403000ApproachState(Task* actorTask);
static void                _actor403000RetreatState(Task* actorTask);
static void                _actor403000PatrolState(Task* actorTask);
static void                _actor403000WatchState(Task* actorTask);
static void                _actor403000TurnState(Task* actorTask);
static void                _actor403000DropCatchState(Task* actorTask);
static void                _actor403000AmbushState(Task* actorTask);
static void                _actor403000ProwlState(Task* actorTask);
static void                func_actor_403000_8013C864(Enemy* arg0, Task* arg1);
static s32                 func_actor_403000_8013D98C(s32 arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Applies the Mine Cavern command namespace to this actor's health and state.
///
/// Requires live enemy/model/work objects and a readable command through dispatch.
/// All commands latch their stage, area and low action byte; other contexts and
/// unsupported actions return 0. Handled actions return 1. Clip requests restart
/// state entry; burned commands also select the burned texture and palette.
/// Scripted action 4 chains clip 27 into 29; action 10 darkens the model.
/// The message ID and second payload are ignored; the command is not retained.
static s32 _actor403000ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_403000_COMMAND_CONTEXT                = GAME_STAGE_MINE_SHELTER | (GAME_AREA_MINE_CAVERN << 8),
        ACTOR_403000_COMMAND_HIDE                   = 0,
        ACTOR_403000_COMMAND_RESET_CLIP24           = 1,
        ACTOR_403000_COMMAND_RESET_CLIP25           = 2,
        ACTOR_403000_COMMAND_RESET_CLIP26_ALTERNATE = 3,
        ACTOR_403000_COMMAND_PLAY_CLIP27            = 4,
        ACTOR_403000_COMMAND_BURN                   = 5,
        ACTOR_403000_COMMAND_RESET_CLIP25_ALTERNATE = 6,
        ACTOR_403000_COMMAND_DISSOLVE               = 7,
        ACTOR_403000_COMMAND_PLAY_CLIP24            = 10,
        ACTOR_403000_COMMAND_START_TURN             = 11,
        ACTOR_403000_COMMAND_SCRIPTED_DISSOLVE      = 12,
        ACTOR_403000_COMMAND_SMOLDER                = 13,
        ACTOR_403000_ANIM_COMMAND24                 = 0x18,
        ACTOR_403000_ANIM_COMMAND25                 = 0x19,
        ACTOR_403000_ANIM_COMMAND26                 = 0x1A,
        ACTOR_403000_ANIM_COMMAND27                 = 0x1B,
        ACTOR_403000_TEXTURE_BURNED                 = 2,
        ACTOR_403000_CLUT_BURNED                    = 4,
    };

    Actor403000Work* work  = task->work;
    Enemy*           enemy = task->spawnArg2.pointer;

    // Latch every command, including commands from other namespaces.
    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommandId    = (u8)command->command;
    if (command->context.key == ACTOR_403000_COMMAND_CONTEXT) {
        switch (command->command) {
            case ACTOR_403000_COMMAND_HIDE:
                enemy->hp   = 0;
                work->state = ACTOR_403000_STATE_HIDDEN;
                return 1;
            case ACTOR_403000_COMMAND_RESET_CLIP24:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->param          = &D_actor_403000_8013DA00;
                enemy->hp             = D_actor_403000_8013DA00.hpMax;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND24;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_RESET_CLIP25:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->param          = &D_actor_403000_8013DA00;
                enemy->hp             = D_actor_403000_8013DA00.hpMax;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND25;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_RESET_CLIP26_ALTERNATE:
                enemy->hp = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND26;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                enemy->reactionFlags  = 0;
                enemy->hp             = D_actor_403000_8013DA10.hpMax;
                enemy->param          = &D_actor_403000_8013DA10;
                return 1;
            case ACTOR_403000_COMMAND_PLAY_CLIP27:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND27;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_BURN:
                work->state                        = ACTOR_403000_STATE_SCRIPTED_BURN;
                work->prevState                    = ACTOR_403000_PREVIOUS_STATE_INVALID;
                task->extra.tmd->texturePageOffset = ACTOR_403000_TEXTURE_BURNED;
                task->extra.tmd->clutRowOffset     = ACTOR_403000_CLUT_BURNED;
                return 1;
            case ACTOR_403000_COMMAND_RESET_CLIP25_ALTERNATE:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->hp             = D_actor_403000_8013DA10.hpMax;
                enemy->param          = &D_actor_403000_8013DA10;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND25;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_DISSOLVE:
                task->extra.tmd->texturePageOffset = ACTOR_403000_TEXTURE_BURNED;
                task->extra.tmd->clutRowOffset     = ACTOR_403000_CLUT_BURNED;
                work->state                        = ACTOR_403000_STATE_DISSOLVE;
                work->prevState                    = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_PLAY_CLIP24:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = ACTOR_403000_ANIM_COMMAND24;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_START_TURN:
                work->state         = ACTOR_403000_STATE_TURN;
                work->prevState     = ACTOR_403000_PREVIOUS_STATE_INVALID;
                work->watchRingDir  = -1;
                work->patrolRingDir = 1;
                work->turnRingDir   = 1;
                if (task->extra.tmd->texturePageOffset == ACTOR_403000_TEXTURE_BURNED) {
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_403000_8013DA10.hpMax;
                    enemy->param         = &D_actor_403000_8013DA10;
                }
                return 1;
            case ACTOR_403000_COMMAND_SCRIPTED_DISSOLVE:
                task->extra.tmd->texturePageOffset = ACTOR_403000_TEXTURE_BURNED;
                task->extra.tmd->clutRowOffset     = ACTOR_403000_CLUT_BURNED;
                work->state                        = ACTOR_403000_STATE_SCRIPTED_DISSOLVE;
                work->prevState                    = ACTOR_403000_PREVIOUS_STATE_INVALID;
                return 1;
            case ACTOR_403000_COMMAND_SMOLDER:
                work->state                        = ACTOR_403000_STATE_SCRIPTED_SMOLDER;
                work->prevState                    = ACTOR_403000_PREVIOUS_STATE_INVALID;
                task->extra.tmd->texturePageOffset = ACTOR_403000_TEXTURE_BURNED;
                task->extra.tmd->clutRowOffset     = ACTOR_403000_CLUT_BURNED;
                return 1;
        }
    }
    return 0;
}

static void func_actor_403000_801327B0(GfxCoord* coord, SVECTOR* pos, s32 arg2)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    u16       x;
    u16       y;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(pos);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stflg(&flag);
    gte_stszotz(&otz);
    if (flag >= 0) {
        x               = sxy;
        y               = sxy >> 16;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        radius          = (s32)(((gRandomLcgState >> 16) & 0xF) + 0x1E) * 0x160 / (otz * 4);
        for (i = 0; i < 8; i++) {
            prim = gGpuPrimCursor;
            // Preserve the textured-triangle-sized reservation for this gouraud packet.
            gGpuPrimCursor = (u8*)prim + sizeof(POLY_GT3);
            setPolyG3(prim);
            setRGB0(prim, 0xFF, 0x60, 0x60);
            setRGB1(prim, 0xF, 8, 8);
            setRGB2(prim, 0x2F, 8, 8);
            prim->x0 = x;
            prim->y0 = y;
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> 12);
            prim->y1 = y + ((rcos(i << 9) * radius) >> 12);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> 12);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> 12);
            addPrim(&gGpuCurrentOt[(otz - 6) >> 4], prim);
            dr = gGpuPrimCursor;
            // Preserve the draw-mode-sized reservation for this texture-page packet.
            gGpuPrimCursor = (u8*)dr + sizeof(DR_MODE);
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[(otz - 6) >> 4], dr);
        }
    }
}

static void func_actor_403000_80132AE0(GfxCoord* parent)
{
    _Actor403000TrailScratch* scratch;
    GfxCoord*                 walker;
    SVECTOR*                  pos;
    s16                       i;
    POLY_FT4*                 prim;
    SVECTOR*                  n;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    gfxSetRotIdentity(&scratch->emitter.coord);
    scratch->emitter.coord.t[0]   = -0x3C;
    scratch->emitter.coord.t[1]   = -0x28;
    scratch->emitter.coord.t[2]   = 0x12C;
    scratch->emitter.parent       = parent;
    scratch->emitter.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&scratch->emitter);
    walker                 = &scratch->emitter;
    pos                    = &scratch->emitterPos;
    scratch->emitterPos.vz = 0;
    scratch->emitterPos.vy = 0;
    scratch->emitterPos.vx = 0;
    {
        SVECTOR local;
        VECTOR  result;
        s32     flag;

        local.vx = 0;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(&local);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    D_actor_403000_80158DF0[0].vx = scratch->emitterPos.vx;
    D_actor_403000_80158DF0[0].vy = scratch->emitterPos.vy;
    D_actor_403000_80158DF0[0].vz = scratch->emitterPos.vz;
    for (i = 0; i < 18; i++) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&D_actor_403000_80158DF0[i]);
        gte_rtps();
        gte_stsxy(&scratch->screen.word);
        gte_stdp(&scratch->depthCue);
        gte_stflg(&scratch->projectionFlags);
        gte_stszotz(&scratch->otz);
        if (i == 0 || scratch->projectionFlags < 0) {
            scratch->prevScreen.word     = scratch->screen.word;
            scratch->prevProjectionFlags = scratch->projectionFlags;
            continue;
        }
        n                      = &scratch->edgeOffset;
        scratch->edgeOffset.vz = 0;
        scratch->edgeOffset.vx = scratch->screen.xy.vy - scratch->prevScreen.xy.vy;
        scratch->edgeOffset.vy = scratch->prevScreen.xy.vx - scratch->screen.xy.vx;
        VectorNormalSS(n, n);
        if (scratch->otz > 0) {
            gte_lddp((gDisplayState.screenDistance * 50 / scratch->otz) >> 2);
            gte_ldsv(n);
            gte_gpf12();
            gte_stsv(n);
        }
        prim->x2 = scratch->screen.xy.vx + scratch->edgeOffset.vx;
        prim->y2 = scratch->screen.xy.vy + scratch->edgeOffset.vy;
        prim->x3 = scratch->screen.xy.vx - scratch->edgeOffset.vx;
        prim->y3 = scratch->screen.xy.vy - scratch->edgeOffset.vy;
        if (scratch->prevProjectionFlags >= 0) {
            if (i != 1) {
                POLY_FT4* prev = prim - 1;

                GPU_PRIMITIVE_XY_WORD(prim, 0) = GPU_PRIMITIVE_XY_WORD(prev, 2);
                GPU_PRIMITIVE_XY_WORD(prim, 1) = GPU_PRIMITIVE_XY_WORD(prev, 3);
            } else {
                prim->x0 = scratch->prevScreen.xy.vx + scratch->edgeOffset.vx;
                prim->y0 = scratch->prevScreen.xy.vy + scratch->edgeOffset.vy;
                prim->x1 = scratch->prevScreen.xy.vx - scratch->edgeOffset.vx;
                prim->y1 = scratch->prevScreen.xy.vy - scratch->edgeOffset.vy;
            }
            prim->u2                          = 4;
            prim->u0                          = 4;
            prim->u3                          = 5;
            prim->u1                          = 5;
            prim->v1                          = 7;
            prim->v0                          = 7;
            prim->v3                          = 8;
            prim->v2                          = 8;
            prim->tpage                       = 0x3F;
            prim->clut                        = 0x3C51;
            GPU_PRIMITIVE_COLOR_WORD(prim, 0) = ((17 - i) * 4) & 0xFF;
            setlen(prim, 9);
            prim->code = 0x2E;
            addPrim(&gGpuCurrentOt[(((u32)(scratch->otz - 10) << gDisplayState.otDepthShift) >> 4) & 0x3FF], prim);
        }
        scratch->prevScreen.word     = scratch->screen.word;
        scratch->prevProjectionFlags = scratch->projectionFlags;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000TrailScratch);
}

static void func_actor_403000_801330D4(GfxCoord* parent)
{
    _Actor403000TrailScratch* scratch;
    GfxCoord*                 walker;
    SVECTOR*                  pos;
    s16                       i;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    gfxSetRotIdentity(&scratch->emitter.coord);
    scratch->emitter.coord.t[0]   = -0x3C;
    scratch->emitter.coord.t[1]   = -0x28;
    scratch->emitter.parent       = parent;
    scratch->emitter.coord.t[2]   = 0x12C;
    scratch->emitter.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&scratch->emitter);
    walker                 = &scratch->emitter;
    pos                    = &scratch->emitterPos;
    scratch->emitterPos.vz = 0;
    scratch->emitterPos.vy = 0;
    scratch->emitterPos.vx = 0;
    {
        SVECTOR local;
        VECTOR  result;
        s32     flag;

        local.vx = 0;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(&local);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    D_actor_403000_80158DF0[0].vx = scratch->emitterPos.vx;
    D_actor_403000_80158DF0[0].vy = scratch->emitterPos.vy;
    D_actor_403000_80158DF0[0].vz = scratch->emitterPos.vz;
}

/// Eases the torso roll and replaces the rotations of its two sway parts.
///
/// Angles use 4096 units per turn. The target is limited to +-700 and the
/// stored sway moves at most 64 units per call; parts 22 and 23 receive half
/// and three quarters of that roll. `task` must own a live 24-part model and
/// an `Actor403000Work` block.
static void _actor403000UpdateTorsoSway(Task* task)
{
    enum {
        ACTOR_403000_TORSO_SWAY_LIMIT = 700,
        ACTOR_403000_TORSO_SWAY_STEP  = 64,
    };

    Actor403000Work* work;
    s16              clampedTarget;

    work          = task->work;
    clampedTarget = work->torsoSwayTarget;
    ACTOR_403000_EASE_JOINT_ANGLE(work->torsoSway, clampedTarget, ACTOR_403000_TORSO_SWAY_LIMIT, ACTOR_403000_TORSO_SWAY_STEP);
    gfxRotMatrixZ(&task->extra.tmd->coords[22].coord, work->torsoSway / 2, GRAPHICS_ROTATION_REPLACE);
    task->extra.tmd->coords[22].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&task->extra.tmd->coords[23].coord, work->torsoSway * 3 / 4, GRAPHICS_ROTATION_REPLACE);
    task->extra.tmd->coords[23].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Adds a head roll that responds to changes in the smoothed neck yaw.
///
/// Angles use 4096 units per turn. Neck changes of at least 8 build the roll
/// target in their direction (2 units below 24, otherwise 32); a quiet neck
/// resets it to zero. The sway eases by at most 48 toward the target clamped
/// to +-640, then composes half and three quarters onto parts 6 and 7.
/// Requires a live 24-part model and `Actor403000Work` at `task->work`.
static void _actor403000UpdateHeadSway(Task* task)
{
    enum {
        ACTOR_403000_HEAD_SWAY_LIMIT            = 0x280,
        ACTOR_403000_HEAD_SWAY_STEP             = 48,
        ACTOR_403000_HEAD_SWAY_MIN_NECK_CHANGE  = 8,
        ACTOR_403000_HEAD_SWAY_FAST_NECK_CHANGE = 24,
        ACTOR_403000_HEAD_SWAY_FAST_TARGET_STEP = 32,
        ACTOR_403000_HEAD_SWAY_SLOW_TARGET_STEP = 2,
    };

    Actor403000Work* work;
    s16              clampedTarget;
    s32              neckYawChange;

    work = task->work;
    // Build the roll target from this frame's neck movement before easing it.
    if (work->neckYaw < work->prevNeckYaw && (neckYawChange = abs(work->neckYaw - work->prevNeckYaw)) >= ACTOR_403000_HEAD_SWAY_MIN_NECK_CHANGE && work->headSwayTarget > -ACTOR_403000_HEAD_SWAY_LIMIT) {
        if (neckYawChange >= ACTOR_403000_HEAD_SWAY_FAST_NECK_CHANGE) {
            if (work->headSwayTarget > 0) {
                work->headSwayTarget = -ACTOR_403000_HEAD_SWAY_FAST_TARGET_STEP;
            } else {
                work->headSwayTarget -= ACTOR_403000_HEAD_SWAY_FAST_TARGET_STEP;
            }
        } else {
            if (work->headSwayTarget > 0) {
                work->headSwayTarget = -ACTOR_403000_HEAD_SWAY_SLOW_TARGET_STEP;
            } else {
                work->headSwayTarget -= ACTOR_403000_HEAD_SWAY_SLOW_TARGET_STEP;
            }
        }
    } else if (work->neckYaw > work->prevNeckYaw && (neckYawChange = abs(work->neckYaw - work->prevNeckYaw)) >= ACTOR_403000_HEAD_SWAY_MIN_NECK_CHANGE && work->headSwayTarget < ACTOR_403000_HEAD_SWAY_LIMIT) {
        if (neckYawChange >= ACTOR_403000_HEAD_SWAY_FAST_NECK_CHANGE) {
            if (work->headSwayTarget < 0) {
                work->headSwayTarget = ACTOR_403000_HEAD_SWAY_FAST_TARGET_STEP;
            } else {
                work->headSwayTarget += ACTOR_403000_HEAD_SWAY_FAST_TARGET_STEP;
            }
        } else {
            if (work->headSwayTarget < 0) {
                work->headSwayTarget = ACTOR_403000_HEAD_SWAY_SLOW_TARGET_STEP;
            } else {
                work->headSwayTarget += ACTOR_403000_HEAD_SWAY_SLOW_TARGET_STEP;
            }
        }
    } else {
        work->headSwayTarget = 0;
    }
    clampedTarget     = work->headSwayTarget;
    work->prevNeckYaw = work->neckYaw;
    ACTOR_403000_EASE_JOINT_ANGLE(work->headSway, clampedTarget, ACTOR_403000_HEAD_SWAY_LIMIT, ACTOR_403000_HEAD_SWAY_STEP);
    gfxRotMatrixZ(&task->extra.tmd->coords[6].coord, work->headSway / 2, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&task->extra.tmd->coords[7].coord, work->headSway * 3 / 4, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Advances both rigs and mixes the overlay rotation over model parts 1..10.
///
/// All main slots 1..23 run at `animRate - 3` sixteenths of a frame; overlay
/// slots 1..10 run at `overlayRate`. Translation comes from the main pose.
/// `overlayWeight` is passed as the main rotation's weight and its complement
/// weights the overlay, in units of 1/4096. Requires both bound rigs and a live
/// 24-part model; both temporary poses remain live through each application.
static void _actor403000TickBlendedSlots(Task* task)
{
    enum {
        ACTOR_403000_OVERLAY_PART_END       = 0xB,
        ACTOR_403000_OVERLAY_MAIN_RATE_BIAS = 3,
    };

    AnimationPose     mainPose;
    AnimationPose     overlayPose;
    AnimationContext* mainAnim;
    s16               mainWeight;
    s16               slotIndex;
    Actor403000Work*  work;

    work       = task->work;
    mainWeight = work->overlayWeight;
    mainAnim   = &work->anim;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        if (slotIndex < ACTOR_403000_OVERLAY_PART_END) {
            work->blendSlots[slotIndex].rate = work->overlayRate;
            work->slots[slotIndex].rate      = (work->animRate - ACTOR_403000_OVERLAY_MAIN_RATE_BIAS);
            animationTickSlotPose(mainAnim, slotIndex, &mainPose, 0);
            animationTickSlotPose(&work->blendAnim, slotIndex, &overlayPose, 0);
            animationApplyPoseWithBlendedRotation(mainAnim, slotIndex, &mainPose, &overlayPose, mainWeight, ONE - mainWeight);
        } else {
            work->slots[slotIndex].rate = (work->animRate - ACTOR_403000_OVERLAY_MAIN_RATE_BIAS);
            animationTickSlot(&work->anim, slotIndex);
        }
    }
}

/// Polls slot 1 for a newly crossed sound cue and returns its script id, or zero.
///
/// Uses the low ten bits of the current pose's record index and remembers
/// them in `slotCueIndex[1]`. Decreasing indices rearm later forward crossings.
/// Only the last matching cue in switch order is returned if several are
/// crossed together. `work` must remain live; the unused `task` parameter
/// preserves the call signature present in the image.
static s32 _actor403000PollAnimationSound(Task* task, Actor403000Work* work)
{
    enum {
        ACTOR_403000_CUE_CLIP_PROWL       = 1,
        ACTOR_403000_CUE_CLIP_WALK        = 2,
        ACTOR_403000_CUE_CLIP_LUNGE       = 7,
        ACTOR_403000_CUE_CLIP_DROP        = 8,
        ACTOR_403000_CUE_CLIP_TURN        = 9,
        ACTOR_403000_CUE_CLIP_GRAB_RUSH   = 11,
        ACTOR_403000_CUE_CLIP12           = 12, // No local request establishes this clip's action
        ACTOR_403000_CUE_CLIP_KNOCKDOWN   = 14,
        ACTOR_403000_CUE_CLIP_LUNGE_CATCH = 17,
    };
    enum {
        ACTOR_403000_SOUND_PROWL_TURN_EARLY   = 0x401E0002,
        ACTOR_403000_SOUND_PROWL_TURN_LATE    = 0x401E0001,
        ACTOR_403000_SOUND_KNOCKDOWN          = 0x401E000E,
        ACTOR_403000_SOUND_WALK_LATE          = 0x401E0003,
        ACTOR_403000_SOUND_WALK_EARLY         = 0x401E0004,
        ACTOR_403000_SOUND_CLIP12_RECORD19    = 0x401E0008,
        ACTOR_403000_SOUND_GRAB_RUSH_RECORD18 = 0x401E0007,
        ACTOR_403000_SOUND_GRAB_RUSH_RECORD15 = 0x401E000C,
        ACTOR_403000_SOUND_LUNGE              = 0x401E0009,
        ACTOR_403000_SOUND_LUNGE_CATCH        = 0x401E000B,
        ACTOR_403000_SOUND_DROP               = 0x401E000D,
    };

    s32 soundId;

    soundId = SOUND_SCRIPT_REQUEST_NO_OP;
    if (work->slotCueIndex[1] == (work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        return soundId;
    }
    switch ((s16)(work->requestedAnimId - 1)) {
        case ACTOR_403000_CUE_CLIP_PROWL - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x21 && work->slotCueIndex[1] < 0x21) {
                soundId = ACTOR_403000_SOUND_PROWL_TURN_EARLY;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x2C && work->slotCueIndex[1] < 0x2C) {
                soundId = ACTOR_403000_SOUND_PROWL_TURN_LATE;
            }
            break;
        case ACTOR_403000_CUE_CLIP_TURN - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 5 && work->slotCueIndex[1] < 5) {
                soundId = ACTOR_403000_SOUND_PROWL_TURN_EARLY;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xA && work->slotCueIndex[1] < 0xA) {
                soundId = ACTOR_403000_SOUND_PROWL_TURN_LATE;
            }
            break;
        case ACTOR_403000_CUE_CLIP_KNOCKDOWN - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->slotCueIndex[1] < 0xF) {
                soundId = ACTOR_403000_SOUND_KNOCKDOWN;
            }
            break;
        case ACTOR_403000_CUE_CLIP_WALK - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x1D && work->slotCueIndex[1] < 0x1D) {
                soundId = ACTOR_403000_SOUND_WALK_LATE;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x17 && work->slotCueIndex[1] < 0x17) {
                soundId = ACTOR_403000_SOUND_WALK_EARLY;
            }
            break;
        case ACTOR_403000_CUE_CLIP12 - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x13 && work->slotCueIndex[1] < 0x13) {
                soundId = ACTOR_403000_SOUND_CLIP12_RECORD19;
            }
            // Clip 12 shares the following cue tests with clip 11.
        case ACTOR_403000_CUE_CLIP_GRAB_RUSH - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x12 && work->slotCueIndex[1] < 0x12) {
                soundId = ACTOR_403000_SOUND_GRAB_RUSH_RECORD18;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->slotCueIndex[1] < 0xF) {
                soundId = ACTOR_403000_SOUND_GRAB_RUSH_RECORD15;
            }
            break;
        case ACTOR_403000_CUE_CLIP_LUNGE - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x15 && work->slotCueIndex[1] < 0x15) {
                soundId = ACTOR_403000_SOUND_LUNGE;
            }
            break;
        case ACTOR_403000_CUE_CLIP_LUNGE_CATCH - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 8 && work->slotCueIndex[1] < 8) {
                soundId = ACTOR_403000_SOUND_LUNGE_CATCH;
            }
            break;
        case ACTOR_403000_CUE_CLIP_DROP - 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xD && work->slotCueIndex[1] < 0xD) {
                soundId = ACTOR_403000_SOUND_DROP;
            }
            break;
    }
    work->slotCueIndex[1] = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    return soundId;
}

/// Blends main slots 1..23 into the requested clip and latches its id.
///
/// Both clip ids must index the 45-by-45 signed-byte transition-duration
/// table; durations count normal-rate frames. Requires the bound main rig,
/// loaded clips and a live 24-part model. Slot 0 is not driven.
static inline void _actor403000SeekSlots(Task* task)
{
    Actor403000Work* work;
    s32              requestedAnimId;
    s32              slotIndex;

    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        work->slots[slotIndex].rate = work->animRate;
        requestedAnimId             = work->requestedAnimId;
        animationSeekSlotWithBlend(&work->anim, slotIndex, requestedAnimId, 0, D_actor_403000_80158364[work->animId][requestedAnimId]);
    }
    work->animId = work->requestedAnimId;
}

/// Restarts main slots 1..23 on the requested clip and latches its id.
///
/// Requires a bound main rig, a loaded clip with all 24 part tracks and a
/// live model. Resetting each slot replaces its just-written rate with
/// `ANIMATION_RATE_ONE`; the subsequent tick supplies the requested rate.
static inline void _actor403000RestartSlots(Task* task)
{
    Actor403000Work* work;
    s32              slotIndex;

    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        work->slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->anim, slotIndex, work->requestedAnimId);
    }
    work->animId = work->requestedAnimId;
}

/// Restarts overlay slots 1..23 with a double-speed, half-weight overlay request.
///
/// Requires both bound rigs, a loaded overlay clip and a live 24-part model.
/// The rate write targets the main slots; the following animation tick
/// overwrites it. Resetting the overlay slots supplies normal rate until
/// parts 1..10 are ticked at the requested double rate.
static inline void _actor403000RestartOverlaySlots(Task* task)
{
    Actor403000Work* work;
    s32              slotIndex;

    work                = task->work;
    work->overlayRate   = ANIMATION_RATE_ONE * 2;
    work->overlayWeight = ONE / 2;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        work->slots[slotIndex].rate = work->overlayRate;
        animationResetSlot(&work->blendAnim, slotIndex, work->overlayAnimId);
    }
}

/// Advances main slots 1..23 at `animRate`, in sixteenths of a frame.
///
/// Requires a bound main rig, loaded clip storage and a live 24-part model.
/// Slot 0 is left to the actor's movement code.
static inline void _actor403000TickSlots(Task* task)
{
    Actor403000Work* work;
    s32              slotIndex;

    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        work->slots[slotIndex].rate = work->animRate;
        animationTickSlot(&work->anim, slotIndex);
    }
}

/// Eases the foreleg yaw and applies its negation to model part 10.
///
/// Angles use 4096 units per turn. The target is limited to +-512 and the
/// stored yaw moves at most 12 units per call. Requires a live 24-part model
/// and `Actor403000Work` at `task->work`; the joint retains its parent frame.
static inline void _actor403000TurnForeleg(Task* task)
{
    enum {
        ACTOR_403000_FORELEG_YAW_LIMIT = 0x200,
        ACTOR_403000_FORELEG_YAW_STEP  = 0xC,
    };

    Actor403000Work* work;
    s16              clampedTarget;

    work          = task->work;
    clampedTarget = work->forelegYawTarget;
    ACTOR_403000_EASE_JOINT_ANGLE(work->forelegYaw, clampedTarget, ACTOR_403000_FORELEG_YAW_LIMIT, ACTOR_403000_FORELEG_YAW_STEP);
    _actorRenderYawJointInWorld(&task->extra.tmd->coords[10], -work->forelegYaw);
    task->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
}

#undef ACTOR_403000_EASE_JOINT_ANGLE

/// Eases the stored neck yaw toward its target by at most 113 angle units.
///
/// Angles use 4096 units per turn. This advances even while neck application
/// is disabled and does not clamp the stored target or the stored result.
/// Requires a live writable work block.
static inline void _actor403000EaseNeckYaw(Actor403000Work* work)
{
    enum { ACTOR_403000_NECK_YAW_STEP = 0x71 };
    s32 neckYaw;
    s32 neckYawTarget;
    u16 neckYawBits;
    u16 neckYawTargetBits;

    // Signed comparisons and halfword-bit updates share the stored angle.
    neckYawTarget     = work->neckYawTarget;
    neckYaw           = work->neckYaw;
    neckYawTargetBits = (u16)work->neckYawTarget;
    neckYawBits       = (u16)work->neckYaw;
    if (neckYaw < neckYawTarget) {
        if ((neckYawTarget - neckYaw) >= (ACTOR_403000_NECK_YAW_STEP + 1)) {
            work->neckYaw = neckYawBits + ACTOR_403000_NECK_YAW_STEP;
        } else {
            work->neckYaw = neckYawTargetBits;
        }
    } else if ((neckYaw - neckYawTarget) >= (ACTOR_403000_NECK_YAW_STEP + 1)) {
        work->neckYaw = neckYawBits - ACTOR_403000_NECK_YAW_STEP;
    } else {
        work->neckYaw = neckYawTargetBits;
    }
}

/// Drives the actor's clip requests, pose playback, procedural joints and sound cues.
///
/// Requires `Actor403000Work` at `task->work`, two bound 24-part rigs and their
/// loaded clip data. Main clip starts clear all 24 cue-index latches; slot 0
/// stays undriven. An active overlay slows main playback by three sixteenths
/// of a frame and mixes parts 1..10 until overlay slot 1 settles.
/// Neck yaw eases even when its joint application is disabled; the remaining
/// procedural joints advance only while their individual flags equal one.
/// `animFrames` wraps as an unsigned halfword.
static void _actor403000UpdateAnimation(Task* task)
{
    enum {
        ACTOR_403000_NECK_YAW_LIMIT = 0x500,
    };

    Actor403000Work* work;
    s16              neckThirdYaw;
    s16              startMode;
    s16              smoothedNeckYaw;
    s32              soundId;
    s32              panOffset;
    s16              clampedNeckYaw;

    work = task->work;
    // Consume clip requests before advancing either rig or testing its cues.
    startMode = work->animStart;
    if (startMode == ACTOR_403000_ANIM_BLEND_IN) {
        if (work->animId != work->requestedAnimId) {
            _actor403000SeekSlots(task);
        }
        work->animStart  = ACTOR_403000_ANIM_PLAYING;
        work->animFrames = 0;
        memFillBytes(work->slotCueIndex, 0U, sizeof(work->slotCueIndex));
    } else if (startMode == ACTOR_403000_ANIM_RESTART) {
        _actor403000RestartSlots(task);
        work->animStart  = ACTOR_403000_ANIM_PLAYING;
        work->animFrames = 0U;
        memFillBytes(work->slotCueIndex, 0U, sizeof(work->slotCueIndex));
    }
    if (work->overlayStart == ACTOR_403000_ANIM_RESTART) {
        _actor403000RestartOverlaySlots(task);
        work->overlayStart = ACTOR_403000_ANIM_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->overlayActive == 0) {
        _actor403000TickSlots(task);
    } else {
        _actor403000TickBlendedSlots(task);
        if (work->blendSlots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->overlayActive = 0;
        }
    }
    // Layer the procedural joint angles over this frame's animation pose.
    _actor403000EaseNeckYaw(work);
    if (work->neckYawEnabled == 1) {
        smoothedNeckYaw = work->neckYaw;
        clampedNeckYaw  = smoothedNeckYaw;
        if (smoothedNeckYaw >= (ACTOR_403000_NECK_YAW_LIMIT + 1)) {
            clampedNeckYaw = ACTOR_403000_NECK_YAW_LIMIT;
        }
        if (smoothedNeckYaw < -ACTOR_403000_NECK_YAW_LIMIT) {
            clampedNeckYaw = -ACTOR_403000_NECK_YAW_LIMIT;
        }
        neckThirdYaw = (s16)clampedNeckYaw / 3;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[2], neckThirdYaw);
        task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[3], neckThirdYaw);
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[4], (s16)clampedNeckYaw / 2);
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->headSwayEnabled == 1) {
        _actor403000UpdateHeadSway(task);
    }
    if (work->forelegYawEnabled == 1) {
        _actor403000TurnForeleg(task);
    }
    if (work->torsoSwayEnabled == 1) {
        _actor403000UpdateTorsoSway(task);
    }
    // Position the one selected cue at the actor's root.
    soundId = _actor403000PollAnimationSound(task, work);
    if (soundId != SOUND_SCRIPT_REQUEST_NO_OP) {
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Tests whether the arena cells and player bearing permit a direct chase.
///
/// Cells must be ring indices 0..9 from `_actor403000GetRingCell` in the
/// arena's parent-coordinate frame. Equal cells permit pursuit immediately;
/// allowed unequal pairs also require a player bearing strictly inside
/// +-512 angle units (45 degrees). Returns 1 if permitted, otherwise 0.
/// `task` must own a live model; the live player root supplies the other end.
static s32 _actor403000CanChasePlayer(Task* task, s16 actorCell, s16 playerCell)
{
    enum {
        ACTOR_403000_CHASE_TURN_LIMIT = 0x200,
    };

    ActorTurnScratch* scratch;
    s16               playerTurn;
    s32               signedTurn;

    if (actorCell == playerCell) {
        return 1;
    }
    switch (actorCell) {
        case 0:
            if (playerCell == 9) {
                break;
            }
            if (playerCell < 4) {
                break;
            }
            return 0;
        case 1:
        case 2:
        case 3:
            if (playerCell < 5) {
                break;
            }
            return 0;
        case 4:
            if (playerCell >= 6) {
                return 0;
            }
            if (playerCell != 0) {
                break;
            }
            return 0;
        case 5:
            if (playerCell < 4) {
                return 0;
            }
            if (playerCell != 9) {
                break;
            }
            return 0;
        case 6:
        case 7:
        case 8:
            if (playerCell >= 5) {
                break;
            }
            return 0;
        case 9:
        default:
            if (playerCell >= 6) {
                break;
            }
            if (playerCell != 0) {
                return 0;
            }
            break;
    }
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    playerTurn     = _actorAngleTurnToPlayer(task, &scratch->delta, &gPlayerStatus);
    scratch->angle = signedTurn = playerTurn;
    if ((signedTurn < 0 ? -signedTurn : signedTurn) < ACTOR_403000_CHASE_TURN_LIMIT) {
        SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    return 0;
}

/// Chooses +1 or -1 around the ring from the heading toward its next waypoint.
///
/// Requires a root in the arena's coordinate frame. Its cell is 0..9; the
/// target is the following cell with wrap at ten. A signed turn below a
/// quarter-turn chooses +1, otherwise -1, using 4096 angle units per turn.
/// Borrows one scratch block, restores the cursor, then reads the saved result
/// before another reservation can reuse it.
static s32 _actor403000ChooseRingDirection(GfxCoord* rootCoord)
{
    GfxCoord*                   coord;
    _Actor403000RingDirScratch* scratch;
    const SVECTOR*              waypoints;
    const SVECTOR*              waypoint;
    s32                         positionX;
    s32                         positionZ;
    s8                          column;
    s8                          row;
    s16                         waypointTurn;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000RingDirScratch);
    coord     = rootCoord;
    positionX = coord->coord.t[0];
    positionZ = coord->coord.t[2];
    column    = 4;
    if (positionX >= 0xD48) {
        column = 3;
        if (positionX >= 0x1A90) {
            column = 2;
            if (positionX >= 0x2AF8) {
                column = positionX < 0x3C8C;
            }
        }
    }
    row               = positionZ >= 0x1068;
    scratch->waypoint = D_actor_403000_80158D48[column + row * (ACTOR_403000_RING_WAYPOINT_COUNT / 2)] + 1;
    if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
        scratch->waypoint = 0;
    }
    waypoints           = D_actor_403000_80158CE0;
    waypoint            = &waypoints[scratch->waypoint];
    scratch->offset.vx  = waypoint->vx;
    scratch->offset.vy  = waypoint->vy;
    scratch->offset.vz  = waypoint->vz;
    scratch->offset.vx -= coord->coord.t[0];
    scratch->offset.vz -= coord->coord.t[2];
    waypointTurn        = _actorAngleTurnToOffset(coord, scratch->offset.vx, scratch->offset.vz);
    if (waypointTurn < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
        scratch->ringDir = 1;
    } else {
        scratch->ringDir = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000RingDirScratch);
    return scratch->ringDir;
}

/// Initializes and links one model-part sphere and its five contact slots.
///
/// Requires a live task-owned sphere and coordinate, in game-coordinate units.
/// The key is an enemy-body identity; pair contacts are enabled after linking.
static inline void _actor403000InitPartSphere(_Actor403000CollisionSphere* sphere, GfxCoord* partCoord, s32 contactKey, u16 radius)
{
    WorldCollisionBody* body = &sphere->body;

    body->coord            = partCoord;
    body->context.contacts = sphere->contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = contactKey;
    body->radius           = radius;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &sphere->body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(sphere->contacts), 0);
}

/// Creates the Blizzard Chaser's work, animation rigs, collision bodies and targeting.
///
/// Enemy-task initialization requires the loaded 24-part model and clip tables.
/// Allocation failure destroys the enemy/task. Success installs the exit callback,
/// binds model lighting to task-owned work, links six bodies and enters APPROACH.
/// The root sphere/capsule are grid-only; torso, hind, neck and head receive pair
/// contacts. The player pushback table excludes both body identities (1 and 30).
static void _actor403000Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_403000_GRID_BODY_KEY         = WORLD_COLLISION_CONTACT_ENEMY_BODY | 1,
        ACTOR_403000_HIT_BODY_KEY          = WORLD_COLLISION_CONTACT_ENEMY_BODY | 30,
        ACTOR_403000_ROOT_BODY_RADIUS      = 300,
        ACTOR_403000_HEAD_CAPSULE_RADIUS   = 512,
        ACTOR_403000_TORSO_RADIUS          = 1000,
        ACTOR_403000_HIND_NECK_RADIUS      = 800,
        ACTOR_403000_HEADING_VECTOR_LENGTH = 5000,
        ACTOR_403000_ANIM_INITIAL          = 0,
        ACTOR_403000_PLAYER_ANIM_INITIAL   = 1,
        ACTOR_403000_PLAYER_BLEND_FRAMES   = 3,
    };

    SVECTOR                scaledHeading;
    VECTOR                 worldPosition;
    Actor403000Work*       work;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    AnimationSet**         animationSets;
    WorldCollisionContact* rootContacts;
    WorldCollisionContact* headContacts;
    SVECTOR*               headingVector;
    Actor403000Work*       lightingWork;
    TmdObject*             lightingModel;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (work = memCalloc(sizeof(Actor403000Work), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback      = _actor403000Exit;
    lightingWork            = task->work;
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &lightingWork->light;
    lightingModel->colorMtx = &lightingWork->color;
    enemy->field_4          = &task->extra.tmd->coords->coord;
    enemy->field_48         = 0;
    enemy->bodyPos.vx       = 0;
    enemy->bodyPos.vy       = 0;
    enemy->bodyPos.vz       = 0;
    enemy->coord            = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    animationSets         = D_actor_403000_80158B50;
    work->lockOnSuspended = 1;
    enemy->reactionFlags  = 0;
    enemy->hp             = D_actor_403000_8013DA00.hpMax;
    enemy->param          = &D_actor_403000_8013DA00;
    enemy->recs           = work->torsoSphere.contacts;
    // Bind both rigs before their first tick initializes the model pose.
    animationInitContext(&work->anim, animationSets, model,
                         work->poses, work->slots);
    animationInitContext(&work->blendAnim, animationSets, model,
                         work->blendPoses, work->blendSlots);
    work->animStart         = ACTOR_403000_ANIM_RESTART;
    work->overlayActive     = 0;
    work->requestedAnimId   = ACTOR_403000_ANIM_INITIAL;
    work->neckYaw           = 0;
    work->neckYawTarget     = 0;
    work->field_ACC         = 0x10;
    work->animRate          = ANIMATION_RATE_ONE;
    work->torsoSwayEnabled  = 1;
    work->forelegYawEnabled = 1;
    work->headSwayEnabled   = 1;
    work->neckYawEnabled    = 1;
    _actor403000UpdateAnimation(task);
    // Grid bodies correct movement; the part bodies receive attacks and catches.
    work->rootSphere.body.context.contacts = work->rootSphere.contacts;
    work->rootSphere.body.coord            = rootCoord;
    work->rootSphere.body.pos.vx           = 0;
    work->rootSphere.body.pos.vy           = -0x11C;
    work->rootSphere.body.pos.vz           = 0;
    work->rootSphere.body.key              = ACTOR_403000_GRID_BODY_KEY;
    work->rootSphere.body.radius           = ACTOR_403000_ROOT_BODY_RADIUS;
    work->rootSphere.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootSphere.body);
    work->rootCapsule.shape.ends[0].vy     = -0x180;
    work->rootCapsule.shape.ends[1].vy     = -0x180;
    work->rootCapsule.shape.ends[1].vz     = 0x2BC;
    work->rootCapsule.body.context.capsule = &work->rootCapsule.shape;
    work->rootCapsule.body.key             = ACTOR_403000_GRID_BODY_KEY;
    work->rootCapsule.shape.ends[0].vx     = 0;
    work->rootCapsule.shape.ends[0].vz     = 0;
    work->rootCapsule.shape.ends[1].vx     = 0;
    work->rootCapsule.shape.end0Radius     = ACTOR_403000_ROOT_BODY_RADIUS;
    work->rootCapsule.shape.end1Radius     = ACTOR_403000_ROOT_BODY_RADIUS;
    work->rootCapsule.shape.contacts = rootContacts = work->rootCapsule.contacts;
    work->rootCapsule.body.coord                    = rootCoord;
    work->rootCapsule.body.pos.vx                   = 0;
    work->rootCapsule.body.pos.vy                   = 0;
    work->rootCapsule.body.pos.vz                   = 0;
    work->rootCapsule.body.radius                   = 0;
    work->rootCapsule.body.flags                    = WORLD_COLLISION_BODY_CAPSULE;
    work->rootSphere.body.flags                    |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootCapsule.body);
    work->headCapsule.shape.ends[0].vz = -0x3E8;
    work->headCapsule.shape.ends[1].vz = 0x190;
    work->headCapsule.shape.end0Radius = ACTOR_403000_HEAD_CAPSULE_RADIUS;
    work->headCapsule.shape.end1Radius = ACTOR_403000_HEAD_CAPSULE_RADIUS;
    work->headCapsule.shape.ends[0].vx = 0;
    work->headCapsule.shape.ends[0].vy = 0;
    work->headCapsule.shape.ends[1].vx = 0;
    work->headCapsule.shape.ends[1].vy = 0;
    work->headCapsule.shape.contacts = headContacts = work->headCapsule.contacts;
    work->rootCapsule.body.flags                   |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->headCapsule.body.coord                    = &task->extra.tmd->coords[5];
    work->headCapsule.body.context.capsule          = &work->headCapsule.shape;
    work->headCapsule.body.pos.vx                   = 0;
    work->headCapsule.body.pos.vy                   = 0;
    work->headCapsule.body.pos.vz                   = 0;
    work->headCapsule.body.key                      = ACTOR_403000_HIT_BODY_KEY;
    work->headCapsule.body.radius                   = 0;
    work->headCapsule.body.flags                    = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headCapsule.body);
    work->headCapsule.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(rootContacts, ARRAY_SIZE(work->rootCapsule.contacts), 0);
    worldCollisionInitContacts(headContacts, ARRAY_SIZE(work->headCapsule.contacts), 0);
    worldCollisionInitContacts(work->rootSphere.body.context.contacts, ARRAY_SIZE(work->rootSphere.contacts), 0);
    _actor403000InitPartSphere(&work->torsoSphere, &task->extra.tmd->coords[1], ACTOR_403000_HIT_BODY_KEY, ACTOR_403000_TORSO_RADIUS);
    _actor403000InitPartSphere(&work->hindSphere, &task->extra.tmd->coords[15], ACTOR_403000_HIT_BODY_KEY, ACTOR_403000_HIND_NECK_RADIUS);
    _actor403000InitPartSphere(&work->neckSphere, &task->extra.tmd->coords[4], ACTOR_403000_HIT_BODY_KEY, ACTOR_403000_HIND_NECK_RADIUS);
    work->hindSphere.body.pos.vx = 0;
    work->hindSphere.body.pos.vy = 0;
    work->hindSphere.body.pos.vz = -0x100;
    work->playerCaught           = 0;
    work->seenTargetsDestroyed   = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    // The scaled heading is discarded, but these GTE operations still run.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &scaledHeading);
    scaledHeading.vy = 0;
    headingVector    = &scaledHeading;
    VectorNormalSS(headingVector, headingVector);
    gte_lddp(ACTOR_403000_HEADING_VECTOR_LENGTH);
    gte_ldsv(headingVector);
    gte_gpf12();
    gte_stsv(headingVector);
    work->playerAnimation.source.sets          = D_actor_403000_80158C08;
    work->playerAnimation.animationId          = ACTOR_403000_PLAYER_ANIM_INITIAL;
    work->playerAnimation.blendFrames          = ACTOR_403000_PLAYER_BLEND_FRAMES;
    work->playerAnimation.blend                = ANIMATION_BLEND_RESET;
    work->playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->roomEventPending                     = 0;
    task->msgTable                             = D_actor_403000_80158CA8;
    rootCoord->parent                          = &gGfxViewCoord;
    rootCoord->composeStamp                    = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
    work->patrolRingDir = -1;
    work->watchRingDir  = -1;
    work->state         = ACTOR_403000_STATE_APPROACH;
    task->state++;
}

/// Spawns two attack-selected hit effects on parts chosen by the hit bearing.
///
/// Requires a live 24-part model/work and two free scratch vectors. `hitYaw` is
/// signed in 4096-per-turn units; its magnitude selects front, rear or side
/// placements, whose fourth halfwords are model-part indices. `attackKey` is the
/// packed player-attack contact key. Offset values are consumed during each spawn;
/// the task-owned argument record remains live and the scratch cursor is restored.
static void _actor403000SpawnHitEffects(Task* task, s16 hitYaw, s32 attackKey)
{
    enum {
        ACTOR_403000_HIT_EFFECT_COUNT       = 2,
        ACTOR_403000_HIT_FRONT_YAW_LIMIT    = 0x200,
        ACTOR_403000_HIT_REAR_YAW_LIMIT     = 0x600,
        ACTOR_403000_HIT_PRIMARY_ARGUMENT   = 0x500,
        ACTOR_403000_HIT_SECONDARY_ARGUMENT = 0x400,
        ACTOR_403000_HIT_PRIMARY_REPEATS    = 3,
        ACTOR_403000_HIT_SECONDARY_REPEATS  = 2,
    };

    SVECTOR*         hitOffsets;
    Actor403000Work* work;
    EffectSpawnArg*  spawnRecord;
    s32              hitYawMagnitude;

    hitOffsets      = (SCRATCH_STACK_CURSOR(SVECTOR) -= ACTOR_403000_HIT_EFFECT_COUNT);
    hitYawMagnitude = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work            = task->work;
    if (hitYawMagnitude < ACTOR_403000_HIT_FRONT_YAW_LIMIT) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) {
            case 0:
                hitOffsets[0] = D_actor_403000_80158C48[0];
                hitOffsets[1] = D_actor_403000_80158C48[3];
                break;
            case 1:
                hitOffsets[0] = D_actor_403000_80158C48[1];
                hitOffsets[1] = D_actor_403000_80158C48[2];
                break;
            case 2:
                hitOffsets[0] = D_actor_403000_80158C48[2];
                hitOffsets[1] = D_actor_403000_80158C48[0];
                break;
            case 3:
                hitOffsets[0] = D_actor_403000_80158C48[3];
                hitOffsets[1] = D_actor_403000_80158C48[1];
                break;
            default:
                hitOffsets[0] = D_actor_403000_80158C48[4];
                break;
        }
    } else if (hitYawMagnitude > ACTOR_403000_HIT_REAR_YAW_LIMIT) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 2) {
            case 0:
                hitOffsets[0] = D_actor_403000_80158C48[5];
                hitOffsets[1] = D_actor_403000_80158C48[6];
                break;
            case 1:
                hitOffsets[0] = D_actor_403000_80158C48[6];
                hitOffsets[1] = D_actor_403000_80158C48[7];
                break;
            default:
                hitOffsets[0] = D_actor_403000_80158C48[7];
                hitOffsets[1] = D_actor_403000_80158C48[5];
                break;
        }
    } else if (hitYaw > 0) {
        hitOffsets[0] = D_actor_403000_80158C48[8];
        hitOffsets[1] = D_actor_403000_80158C48[9];
    } else {
        hitOffsets[0] = D_actor_403000_80158C48[10];
        hitOffsets[1] = D_actor_403000_80158C48[11];
    }
    // Each spawn consumes its offset before the scratch pair is released.
    work->hitEffectArg.coord      = &task->extra.tmd->coords[hitOffsets[0].pad];
    work->hitEffectArg.spawnArgLo = ACTOR_403000_HIT_PRIMARY_ARGUMENT;
    work->hitEffectArg.spawnArgHi = ACTOR_403000_HIT_PRIMARY_REPEATS;
    spawnRecord                   = &work->hitEffectArg;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitOffsets[0].pad], &hitOffsets[0], spawnRecord);
    work->hitEffectArg.coord      = &task->extra.tmd->coords[hitOffsets[1].pad];
    work->hitEffectArg.spawnArgLo = ACTOR_403000_HIT_SECONDARY_ARGUMENT;
    work->hitEffectArg.spawnArgHi = ACTOR_403000_HIT_SECONDARY_REPEATS;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitOffsets[1].pad], &hitOffsets[1], spawnRecord);
    SCRATCH_STACK_CURSOR(SVECTOR) += ACTOR_403000_HIT_EFFECT_COUNT;
}

/// Consumes new destroyed-target bits and reports a blast within 3000 horizontal units.
///
/// Requires live model/work. Tests newly set bits of the Mine Cavern's four-target
/// flag against the matching point in the actor root's parent frame; Y is ignored.
/// Distance must be strictly below the radius. The entire current nibble is latched
/// even when no point is near, so each change is consumed once. Returns 0 or 1.
static s32 _actor403000CheckTargetBlast(Task* task)
{
    enum { ACTOR_403000_TARGET_BLAST_RADIUS = 3000 };

    Actor403000Work* work;
    s16              destroyedTargets;
    s16              targetIndex;
    VECTOR           targetOffset;

    work             = task->work;
    destroyedTargets = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    if (destroyedTargets == work->seenTargetsDestroyed) {
        return 0;
    }
    for (targetIndex = 0; targetIndex < ARRAY_SIZE(D_actor_403000_80158D64); targetIndex++) {
        if (((destroyedTargets >> targetIndex) & 1) && !((work->seenTargetsDestroyed >> targetIndex) & 1)) {
            targetOffset.vx = task->extra.tmd->coords->coord.t[0] - D_actor_403000_80158D64[targetIndex].vx;
            targetOffset.vz = task->extra.tmd->coords->coord.t[2] - D_actor_403000_80158D64[targetIndex].vz;
            if (SquareRoot0(targetOffset.vx * targetOffset.vx + targetOffset.vz * targetOffset.vz) < ACTOR_403000_TARGET_BLAST_RADIUS) {
                work->seenTargetsDestroyed = destroyedTargets;
                return 1;
            }
        }
    }
    work->seenTargetsDestroyed = destroyedTargets;
    return 0;
}

static inline s32 func_actor_403000_FindHit(SVECTOR* pos, WorldCollisionContact* records)
{
    s16 i;
    for (i = 0; i < 5; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static inline s16 func_actor_403000_WrapAngle(s16 angle)
{
    if (angle < 0) {
        while (1) {
            if (angle >= -0x800)
                break;
            angle += 0x1000;
        }
    } else {
        while (1) {
            if (angle <= 0x800)
                break;
            angle -= 0x1000;
        }
    }
    return angle;
}

static inline void func_actor_403000_PlaySound(Task* arg0, Enemy* enemy, s32 id)
{
    s32 sound;
    s32 pan;

    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
}

static void func_actor_403000_80134F44(Task* arg0)
{
    Task*                      player;
    Enemy*                     enemy;
    PlayerStatus*              config;
    Actor403000Work*           work;
    _Actor403000DamageScratch* scratch;
    s32                        yaw;
    s32                        dx;
    s32                        dy;
    s32                        dz;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    config = &gPlayerStatus;
    work   = arg0->work;
    if (enemy->hp > 0) {
        if (work->hitCooldown > 0) {
            work->hitCooldown--;
            return;
        }
        scratch         = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000DamageScratch);
        scratch->hitKey = func_actor_403000_FindHit(&scratch->hitPos, work->torsoSphere.contacts);
        if (scratch->hitKey == 0) {
            scratch->hitKey = func_actor_403000_FindHit(&scratch->hitPos, work->hindSphere.contacts);
        }
        if (scratch->hitKey == 0) {
            scratch->hitKey = func_actor_403000_FindHit(&scratch->hitPos, work->neckSphere.contacts);
        }
        if (scratch->hitKey == 0) {
            scratch->hitKey = func_actor_403000_FindHit(&scratch->hitPos, work->headCapsule.contacts);
        }
        if (work->blastPuffFrames != 0) {
            work->blastPuffFrames--;
            switch (work->blastPuffFrames % 30) {
                case 0:
                    effectSpawn(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[15], 0x800001FF, NULL);
                    break;
                case 8:
                    effectSpawn(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[23], 0x800001FF, NULL);
                    break;
                case 19:
                    effectSpawn(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[11], 0x800001FF, NULL);
                    break;
            }
        }
        if ((s16)_actor403000CheckTargetBlast(arg0) != 0) {
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->blastPuffFrames = ((gRandomLcgState >> 16) & 0x1F) + 0xA0;
            if (work->state != ACTOR_403000_STATE_STUNNED && work->state != ACTOR_403000_STATE_DOWN && !(work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame >= 0x13)) {
                if (work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_DROP_CATCH && work->playerCaught != 1) {
                    work->state = ACTOR_403000_STATE_KNOCKDOWN;
                }
            }
            scratch->hitKey     = 0;
            scratch->damage     = 400;
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
            damageAccumulateLifeDrainHp(enemy, scratch->hitKey, scratch->damage, 0);
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            enemy->hp -= scratch->damage;
            if (enemy->hp <= 0 && gPlayerStatus.hp <= 0) {
                enemy->hp = 1;
            }
            if ((work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->state == ACTOR_403000_STATE_DROP_CATCH || work->state == ACTOR_403000_STATE_AMBUSH || work->playerCaught == 1) {
                if (enemy->hp <= 0) {
                    enemy->hp = 1;
                }
            }
            if (enemy->hp <= 0) {
                if ((work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12) || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN_HIT) {
                    work->roomEventPending = 1;
                    work->state            = ACTOR_403000_STATE_DISSOLVE;
                } else {
                    work->state = ACTOR_403000_STATE_KNOCKDOWN;
                }
                if (arg0->extra.tmd->texturePageOffset == 0) {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0011);
                } else {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0012);
                }
            } else {
                func_actor_403000_PlaySound(arg0, enemy, 0x401E0005);
            }
        }
        if (scratch->hitKey != 0) {
            if (work->state == ACTOR_403000_STATE_WATCH) {
                work->ambushRequested = 1;
            }
            work->hitCooldown = damageGetPlayerAttackHitCooldown(scratch->hitKey);
            switch (damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF) {
                case DAMAGE_PLAYER_REACTION_NONE:
                case 5:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                case 8:
                case 9:
                    if (work->state == ACTOR_403000_STATE_WATCH || work->state == ACTOR_403000_STATE_PROWL) {
                        work->state         = ACTOR_403000_STATE_PATROL;
                        work->prevState     = -1;
                        work->patrolRingDir = work->watchRingDir;
                        work->watchRingDir  = -work->watchRingDir;
                        break;
                    }
                    if ((u16)(work->state - ACTOR_403000_STATE_STUNNED) >= 3 && work->state != ACTOR_403000_STATE_DOWN_HIT && work->playerCaught != 1 && !(work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12)) {
                        work->overlayAnimId = 0xD;
                        work->overlayActive = 1;
                        work->overlayStart  = ACTOR_403000_ANIM_RESTART;
                    }
                    break;
                case 4:
                    if (work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_DOWN_HIT || (work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12)) {
                        work->state     = ACTOR_403000_STATE_DOWN_HIT;
                        work->prevState = -1;
                        break;
                    }
                    if (work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_DROP_CATCH && work->playerCaught != 1) {
                        work->state = ACTOR_403000_STATE_KNOCKDOWN;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    if (work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_DROP_CATCH && work->playerCaught != 1) {
                        damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                        if (work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_DOWN_HIT || (work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12)) {
                            work->state     = ACTOR_403000_STATE_DOWN_HIT;
                            work->prevState = -1;
                            break;
                        }
                        work->state = ACTOR_403000_STATE_KNOCKDOWN;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    damageTryStartEnemyDamageOverTime(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    if (work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_DOWN_HIT || (work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12)) {
                        work->state     = ACTOR_403000_STATE_DOWN_HIT;
                        work->prevState = -1;
                        break;
                    }
                    if (work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_DROP_CATCH && work->playerCaught != 1) {
                        work->overlayActive = 1;
                        work->overlayAnimId = 0xD;
                        work->overlayStart  = ACTOR_403000_ANIM_RESTART;
                        if (work->state == ACTOR_403000_STATE_WATCH || work->state == ACTOR_403000_STATE_PROWL) {
                            work->state         = ACTOR_403000_STATE_PATROL;
                            work->prevState     = -1;
                            work->patrolRingDir = work->watchRingDir;
                            work->watchRingDir  = -work->watchRingDir;
                        }
                    }
                    break;
            }
            dx                      = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            scratch->playerDelta.vx = dx;
            dy                      = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            scratch->playerDelta.vy = dy;
            dz                      = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->playerDelta.vz = dz;
            scratch->playerDistance = SquareRoot0(dx * dx + dy * dy + dz * dz);
            scratch->damage         = damageComputePlayerAttack(scratch->hitKey, scratch->playerDistance, 0, 0);
            if (damageRollCriticalHit(enemy, scratch->hitKey, 0) != 0) {
                scratch->damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], 0, NULL);
            }
            scratch->hitOffset.vx = scratch->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            scratch->hitOffset.vz = scratch->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            yaw                   = ratan2(scratch->hitOffset.vx, scratch->hitOffset.vz);
            scratch->hitYaw       = yaw - ratan2(-arg0->extra.tmd->coords->workm.m[2][0], arg0->extra.tmd->coords->workm.m[2][2]);
            scratch->hitYaw       = func_actor_403000_WrapAngle(scratch->hitYaw);
            _actor403000SpawnHitEffects(arg0, scratch->hitYaw, scratch->hitKey);
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
            damageAccumulateLifeDrainHp(enemy, scratch->hitKey, scratch->damage, 0);
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            enemy->hp -= scratch->damage;
            if ((work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->state == ACTOR_403000_STATE_DROP_CATCH || work->state == ACTOR_403000_STATE_AMBUSH || work->playerCaught == 1) {
                if (enemy->hp <= 0) {
                    enemy->hp = 1;
                }
            }
            if (enemy->hp <= 0) {
                if ((work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12) || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN_HIT) {
                    work->roomEventPending = 1;
                    work->state            = ACTOR_403000_STATE_DISSOLVE;
                } else {
                    work->state = ACTOR_403000_STATE_KNOCKDOWN;
                }
                D_actor_403000_80158D8C.context.loc.stage = 9;
                D_actor_403000_80158D8C.context.loc.area  = 1;
                D_actor_403000_80158D8C.command           = 3;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403000_80158D8C, ACTOR_COMMAND_MESSAGE_APPLY);
                if (arg0->extra.tmd->texturePageOffset == 0) {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0011);
                } else {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0012);
                }
            } else {
                func_actor_403000_PlaySound(arg0, enemy, 0x401E0005);
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (scratch->damage != 0) {
                scratch->damage >>= 2;
                worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
                enemy->hp -= scratch->damage;
                if ((work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->state == ACTOR_403000_STATE_DROP_CATCH || work->state == ACTOR_403000_STATE_AMBUSH || work->playerCaught == 1) {
                    if (enemy->hp <= 0) {
                        enemy->hp = 1;
                    }
                }
                if (enemy->hp <= 0) {
                    if (work->state != ACTOR_403000_STATE_DOWN && work->state != ACTOR_403000_STATE_DOWN_HIT && work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_STUNNED) {
                        work->state = ACTOR_403000_STATE_KNOCKDOWN;
                    } else {
                        work->roomEventPending = 1;
                        work->state            = ACTOR_403000_STATE_DISSOLVE;
                    }
                } else {
                    if ((work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12) || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_DOWN_HIT || work->state == ACTOR_403000_STATE_STUNNED) {
                        work->state     = ACTOR_403000_STATE_DOWN_HIT;
                        work->prevState = -1;
                    } else if (work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_DROP_CATCH) {
                        work->overlayActive = 1;
                        work->overlayAnimId = 0xD;
                        work->overlayStart  = ACTOR_403000_ANIM_RESTART;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(_Actor403000DamageScratch);
    }
}

/// Plays the stunned twitch with random pauses until build-up ends or health runs out.
///
/// Requires live model/enemy/work and animation rigs. Entry restores drawing and
/// root grid collision, resumes normal-rate playback and restarts clip 15. Slot-1
/// jumps or settlement choose a 0..31-frame pause; each positive count freezes
/// playback on the following tick. Build-up completion or death selects DOWN.
static void _actor403000StunnedState(Task* task)
{
    enum { ACTOR_403000_ANIM_STUNNED    = 0xF,
           ACTOR_403000_STUN_PAUSE_MASK = 0x1F };

    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       model;
    u32              randomState;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                 = task->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate               = ANIMATION_RATE_ONE;
        work->requestedAnimId        = ACTOR_403000_ANIM_STUNNED;
        work->animStart              = ACTOR_403000_ANIM_RESTART;
        work->stateFrame             = 0;
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor403000UpdateAnimation(task);
    // Pause the twitch clip briefly at each control jump or settled boundary.
    if (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) {
        randomState      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = randomState;
        work->stateFrame = (randomState >> 0x10) & ACTOR_403000_STUN_PAUSE_MASK;
    }
    if (work->stateFrame > 0) {
        work->stateFrame--;
        work->animRate = 0;
    } else {
        work->animRate = ANIMATION_RATE_ONE;
    }
    if ((damageTickEnemyBuildup(enemy) == 1) || (enemy->hp <= 0)) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_403000_STATE_DOWN;
    }
}

static void func_actor_403000_8013603C(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd->flags = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->lockOnSuspended        = 1;
        work->rootSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        worldTargetDisableNodeLockOn(&enemy->node);
        work->stateFrame = 0;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
    }
    if (work->roomEventPending == 1 && Gp_StateC08.mode != work->roomEventPending && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
        work->roomEventPending = 0;
    }
    if (work->stateFrame <= 0x1000) {
        work->stateFrame++;
        if (work->stateFrame % 5 == 0 && work->stateFrame < 130) {
            switch ((s16)((s16)(work->stateFrame / 5) % 4)) {
                case 0:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch (work->stateFrame) {
            case 1:
                arg0->extra.tmd->flags = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x76:
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x78:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                break;
            case 0x88:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->stateFrame;
        if (t > 0x64) {
            if ((t - 0x64) * 0x3C < 0x1000) {
                _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, ONE - (t - 0x64) * 0x6B);
            } else {
                _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, 0);
            }
        } else {
            _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, ONE);
        }
    }
}

static void func_actor_403000_801365D0(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd->flags = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->lockOnSuspended        = 1;
        work->rootSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        worldTargetDisableNodeLockOn(&enemy->node);
        work->stateFrame = 0;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
    }
    if (work->stateFrame <= 0x1000) {
        work->stateFrame++;
        if (work->stateFrame % 5 == 0 && work->stateFrame < 100) {
            switch ((s16)((s16)(work->stateFrame / 5) % 4)) {
                case 0:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch (work->stateFrame) {
            case 1:
                arg0->extra.tmd->flags = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x58:
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x5A:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                break;
            case 0x6A:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->stateFrame;
        if (t > 0x46) {
            if ((t - 0x46) * 0x3C < 0x1000) {
                _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, ONE - (t - 0x46) * 0x6B);
            } else {
                _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, 0);
            }
        } else {
            _actorRenderRescaleYawY(arg0->extra.tmd->coords, ONE, ONE);
        }
    }
}

static void func_actor_403000_80136B14(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd->flags = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        enemy->reactionFlags          = 0;
        work->animRate                = 0x10;
        work->requestedAnimId         = 0x1C;
        work->animStart               = ACTOR_403000_ANIM_RESTART;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lockOnSuspended         = 1;
        worldTargetDisableNodeLockOn(&enemy->node);
        arg0->extra.tmd->otOffset = 8;
        work->stateFrame          = 0;
    }
    _actor403000UpdateAnimation(arg0);
    if (work->stateFrame < 0x28) {
        work->stateFrame++;
    }
    switch (work->stateFrame) {
        case 2:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            break;
        case 5:
            arg0->extra.tmd->coords[12].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[12]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[12], 1, NULL);
            break;
        case 15:
            arg0->extra.tmd->coords[16].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[16]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            arg0->extra.tmd->otOffset = 0;
            break;
    }
}

static void func_actor_403000_80136D68(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd->flags = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        enemy->reactionFlags          = 0;
        work->animRate                = 0x10;
        work->requestedAnimId         = 0x1C;
        work->animStart               = ACTOR_403000_ANIM_RESTART;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lockOnSuspended         = 1;
        worldTargetDisableNodeLockOn(&enemy->node);
        arg0->extra.tmd->otOffset = 8;
        work->stateFrame          = 0;
    }
    _actor403000UpdateAnimation(arg0);
    if (work->stateFrame < 300) {
        work->stateFrame++;
        switch (work->stateFrame % 24) {
            case 10:
            case 13:
            case 18:
            case 21:
                effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x01032600, NULL);
                break;
            default:
                effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[(s16)(work->stateFrame % 24)], 0x01032600, NULL);
                break;
        }
    }
    switch (work->stateFrame) {
        case 2:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            break;
        case 5:
            arg0->extra.tmd->coords[12].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[12]);
            break;
        case 15:
            arg0->extra.tmd->coords[16].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[16]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            effectSpawn(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            arg0->extra.tmd->otOffset = 0;
            break;
    }
}

/// Scales a short vector in place by a factor with twelve fractional bits.
///
/// `ONE` is unity. For a direction normalized to that length, the multiplier
/// supplies a distance in coordinate units; quantization can change its magnitude.
/// Requires a live, aligned writable `SVECTOR`; XYZ use the GTE's signed short
/// saturation and `pad` is untouched. Clobbers GTE working state and retains
/// no storage. The low halfword of `scaleQ12` is transferred unchanged to the
/// signed GTE multiplier.
static __inline__ void _actor403000ScaleVectorQ12(SVECTOR* vector, u16 scaleQ12)
{
    gte_lddp(scaleQ12);
    gte_ldsv(vector);
    gte_gpf12();
    gte_stsv(vector);
}

/// Reserves one uninitialized short vector on the shared downward scratch stack.
///
/// The cursor must be initialized and have room for an aligned `SVECTOR`.
/// Release the vector in reverse reservation order with
/// `SCRATCH_STACK_RELEASE_BLOCK(SVECTOR)`; its lifetime ends at release.
static __inline__ SVECTOR* _actor403000ReserveScratchVector(void)
{
    SVECTOR* scratchEnd;

    scratchEnd                    = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = scratchEnd - 1;
    return scratchEnd - 1;
}

/// Releases the most recently reserved short vector from the scratch stack.
///
/// The top reservation must be one `SVECTOR`; release in reverse order.
/// Its bytes are untouched and become available to the next reservation.
static __inline__ void _actor403000ReleaseScratchVector(void)
{
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Maps an arena position to its waypoint-ring cell, in 0..9.
///
/// Reads the local X/Z translation of a live coordinate in the arena's parent
/// frame. Five X bands split at 3400, 6800, 11000 and 15500; two Z bands split
/// at 4200. Their ten entries follow the ring around both rows, with the
/// opposite row reversed. Boundary points belong to the higher coordinate band.
/// No coordinate composition or Y test is performed.
static inline s8 _actor403000GetRingCell(GfxCoord* coord)
{
    s32 positionX;
    s32 positionZ;
    s8  column;
    s8  row;
    s32 ringCell;

    positionX = coord->coord.t[0];
    positionZ = coord->coord.t[2];
    if (positionX < 0xD48) {
        column = 4;
    } else if (positionX < 0x1A90) {
        column = 3;
    } else if (positionX < 0x2AF8) {
        column = 2;
    } else {
        column = positionX < 0x3C8C;
    }
    row      = positionZ >= 0x1068;
    ringCell = (s8)D_actor_403000_80158D48[column + row * (ACTOR_403000_RING_WAYPOINT_COUNT / 2)];
    return ringCell;
}

/// Selects the shorter direction around the ten-cell ring toward the player.
///
/// `cellDifference` is own cell minus player cell, in -9..9. Returns -1 for
/// differences below -5 or in 0..4, otherwise +1. Equal cells choose -1;
/// half-ring ties choose +1. Negating the result gives the away direction.
static inline s8 _actor403000RingSide(s16 cellDifference)
{
    if (cellDifference < -(ACTOR_403000_RING_WAYPOINT_COUNT / 2)) {
        return -1;
    }
    if (cellDifference >= 0) {
        if (cellDifference < ACTOR_403000_RING_WAYPOINT_COUNT / 2) {
            return -1;
        }
    }
    return 1;
}

/// Selects the neighboring waypoint of a 0..9 cell in direction +/-1.
///
/// Borrows writable waypoint scratch and preserves the signed-halfword sum
/// before wrapping either end of the ring. Does not reserve or release storage.
static inline void _actor403000SelectRingNeighbor(_Actor403000WaypointScratch* waypointScratch, s8 ringDirection)
{
    waypointScratch->waypoint = waypointScratch->cell + ringDirection;
    if (waypointScratch->waypoint != -1) {
        if (waypointScratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
            waypointScratch->waypoint = 0;
        }
    } else {
        waypointScratch->waypoint = ACTOR_403000_RING_WAYPOINT_COUNT - 1;
    }
}

/// Advances a chase root along normalized local Z while actors are unfrozen.
///
/// `rootCoord` is live in the arena parent frame; distance is signed world
/// units per tick. Reserves/releases one scratch SVECTOR even at distance zero.
static inline void _actor403000AdvanceChaseRoot(GfxCoord* rootCoord, s16 distance)
{
    SVECTOR* forwardStep;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        forwardStep = _actor403000ReserveScratchVector();
        if (distance != 0) {
            gfxReadMatrixZAxis(&rootCoord->coord, forwardStep);
            VectorNormalSS(forwardStep, forwardStep);
            _actor403000ScaleVectorQ12(forwardStep, distance);
            rootCoord->coord.t[0]  += forwardStep->vx;
            rootCoord->coord.t[1]  += forwardStep->vy;
            rootCoord->coord.t[2]  += forwardStep->vz;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        _actor403000ReleaseScratchVector();
    }
}

/// Selects the settled downed reaction, leaving a lethal room report pending.
///
/// Requires writable actor work and a live enemy. Positive health selects
/// stunned while build-up is active, otherwise down; zero or negative health
/// selects dissolve. The report is consumed by the later dissolve state.
static inline void _actor403000FinishDownReaction(Actor403000Work* work, const Enemy* enemy)
{
    if (enemy->hp > 0) {
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ACTOR_403000_STATE_STUNNED;
        } else {
            work->state = ACTOR_403000_STATE_DOWN;
        }
    } else {
        work->roomEventPending = 1;
        work->state            = ACTOR_403000_STATE_DISSOLVE;
    }
}

/// Faces the player, then advances and chooses a grab or a ranged lunge.
///
/// Requires live actor work/model, enemy collision tables and the live player
/// in the same arena coordinate frame. Walks 300 parent-coordinate units per
/// unfrozen tick, with yaw in 4096 units per turn and a 64-unit turn limit.
/// An excessive heading error hands control to the ring-turn state. Reserves
/// and releases one chase scratch block plus the forward-step workspace.
static void _actor403000ChaseState(Task* actorTask)
{
    enum {
        ACTOR_403000_CHASE_PROBE_REACH      = 900,
        ACTOR_403000_CHASE_TURN_STEP        = 0x40,
        ACTOR_403000_CHASE_WALK_ALIGNMENT   = 0x80,
        ACTOR_403000_CHASE_ATTACK_ALIGNMENT = 0x200,
        ACTOR_403000_CHASE_TURN_REENTRY     = 0x300,
        ACTOR_403000_CHASE_WALK_STEP        = 300,
        ACTOR_403000_CHASE_LUNGE_DISTANCE   = 0x1964,
        ACTOR_403000_CHASE_LUNGE_TOLERANCE  = 0x1F4,
        ACTOR_403000_CHASE_GRAB_DISTANCE    = 0x1770,
    };

    Actor403000Work*          work;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    GfxCoord*                 movementCoord;
    GfxCoord*                 turnCoord;
    GfxCoord*                 walkCoord;
    SVECTOR*                  toPlayer;
    s32                       headingError;
    s16                       cellDifference;
    s32                       lungeRangeError;
    s8                        towardPlayerDir;
    PlayerStatus*             playerStatus;
    TmdObject*                model;

    work         = actorTask->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius      = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart                    = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                     = ANIMATION_RATE_ONE;
        work->requestedAnimId              = ACTOR_403000_CLIP_STAND;
        work->overlayActive                = 0;
        work->forelegYawTarget             = 0;
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_CHASE_PROBE_REACH;
        work->rootSphere.body.flags       |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _actor403000UpdateAnimation(actorTask);
    // Align before walking; the newly selected walk phase can run this same tick.
    if (work->requestedAnimId == ACTOR_403000_CLIP_STAND) {
        toPlayer  = &scratch->offset;
        turnCoord = actorTask->extra.tmd->coords;
        _actorPositionDeltaToPlayer(playerStatus, turnCoord, toPlayer);
        scratch->turn = headingError = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, toPlayer->vx, toPlayer->vz);
        work->neckYawTarget          = headingError;
        if (scratch->turn > ACTOR_403000_CHASE_TURN_STEP) {
            scratch->turn = ACTOR_403000_CHASE_TURN_STEP;
        } else if (scratch->turn < -ACTOR_403000_CHASE_TURN_STEP) {
            scratch->turn = -ACTOR_403000_CHASE_TURN_STEP;
        }
        scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
        if (ABS(work->neckYawTarget) < ACTOR_403000_CHASE_WALK_ALIGNMENT) {
            work->requestedAnimId = ACTOR_403000_CLIP_WALK;
            work->animStart       = ACTOR_403000_ANIM_BLEND_IN;
        }
        _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    }
    if (work->requestedAnimId == ACTOR_403000_CLIP_WALK) {
        _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
        toPlayer  = &scratch->offset;
        walkCoord = actorTask->extra.tmd->coords;
        _actorPositionDeltaToPlayer(&gPlayerStatus, walkCoord, toPlayer);
        scratch->turn = headingError = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, toPlayer->vx, toPlayer->vz);
        work->neckYawTarget          = headingError;
        movementCoord                = actorTask->extra.tmd->coords;
        _actor403000AdvanceChaseRoot(movementCoord, ACTOR_403000_CHASE_WALK_STEP);
        actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        // Choose attacks from the horizontal range after the forward step.
        scratch->playerDelta.vx = playerStatus->coordMtx->t[0] - actorTask->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = 0;
        scratch->playerDelta.vz = playerStatus->coordMtx->t[2] - actorTask->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = lungeRangeError = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        lungeRangeError                          -= ACTOR_403000_CHASE_LUNGE_DISTANCE;
        if (lungeRangeError < 0) {
            lungeRangeError = -lungeRangeError;
        }
        if (lungeRangeError < ACTOR_403000_CHASE_LUNGE_TOLERANCE && ABS(scratch->turn) < ACTOR_403000_CHASE_ATTACK_ALIGNMENT) {
            work->state = ACTOR_403000_STATE_LUNGE;
        }
        if (scratch->playerDistance < ACTOR_403000_CHASE_GRAB_DISTANCE && ABS(scratch->turn) < ACTOR_403000_CHASE_ATTACK_ALIGNMENT) {
            work->state = ACTOR_403000_STATE_GRAB;
        }
        if (ABS(work->neckYawTarget) > ACTOR_403000_CHASE_TURN_REENTRY) {
            scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
            scratch->cell       = _actor403000GetRingCell(actorTask->extra.tmd->coords);
            work->state         = ACTOR_403000_STATE_TURN;
            cellDifference      = scratch->cell - scratch->playerCell;
            towardPlayerDir     = _actor403000RingSide(cellDifference);
            work->turnRingDir = work->watchRingDir = -towardPlayerDir;
        }
        if (scratch->turn > ACTOR_403000_CHASE_TURN_STEP) {
            scratch->turn = ACTOR_403000_CHASE_TURN_STEP;
        } else if (scratch->turn < -ACTOR_403000_CHASE_TURN_STEP) {
            scratch->turn = -ACTOR_403000_CHASE_TURN_STEP;
        }
        scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Returns whether the leading contact records include a player/companion body.
///
/// Reads at most `count` elements, stopping at the first zero key. Requires that
/// many readable records when count is positive; a nonpositive count returns 0.
/// Ignores contact flags and tests the packed category without changing the table.
static inline s32 _actor403000HasPlayerContact(const WorldCollisionContact* records, s16 count)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < count; contactIndex++) {
        if (records[contactIndex].key.value == 0) {
            break;
        }
        if ((records[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Aligns the grab models and places the player at the paired clip's yaw.
///
/// Requires live actor/player roots in the same parent frame and writable
/// chase scratch. Places the actor 1350 units short of the player position
/// along the actor's facing, then sends a borrowed placement synchronously. `playerYawOffset`
/// uses 4096 units per turn relative to the actor; the shared record persists.
static inline void _actor403000AlignGrabCatch(Task* actorTask, Task* player, _Actor403000ChaseScratch* scratch, s16 playerYawOffset)
{
    enum { ACTOR_403000_GRAB_ALIGNMENT_DISTANCE = 1350 };
    SVECTOR* alignmentOffset;

    alignmentOffset = &scratch->offset;
    gfxReadMatrixZAxis(&actorTask->extra.tmd->coords->coord, alignmentOffset);
    VectorNormalSS(alignmentOffset, alignmentOffset);
    gte_lddp(-ACTOR_403000_GRAB_ALIGNMENT_DISTANCE);
    gte_ldsv(alignmentOffset);
    gte_gpf12();
    gte_stsv(alignmentOffset);
    actorTask->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + scratch->offset.vx;
    actorTask->extra.tmd->coords->coord.t[1]   = player->extra.tmd->coords->coord.t[1] + scratch->offset.vy;
    actorTask->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + scratch->offset.vz;
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    D_actor_403000_80158D90.placement.pos.vx   = player->extra.tmd->coords->coord.t[0];
    D_actor_403000_80158D90.placement.pos.vy   = player->extra.tmd->coords->coord.t[1];
    D_actor_403000_80158D90.placement.pos.vz   = player->extra.tmd->coords->coord.t[2];
    D_actor_403000_80158D90.placement.rot.vx   = 0;
    D_actor_403000_80158D90.placement.rot.vy   = ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]) + playerYawOffset;
    D_actor_403000_80158D90.placement.rot.vz   = 0;
    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403000_80158D90.placement, 0);
}

/// Approaches the player and tries to catch them during the grab rush.
///
/// Requires live actor work/model/enemy, player and initialized contact tables.
/// Starts the rush below 3500 horizontal units and moves by distance/15 per
/// unfrozen tick. At rush frame 10, a neck contact and an accepted player hold
/// start the paired catch clip and damage attack 1. The player borrows this
/// overlay's animation bank until catch playback restores the equipped bank.
/// Entry and movement use the arena's parent frame; yaw uses 4096 per turn.
/// All chase and vector scratch reservations are released before return.
static void _actor403000GrabState(Task* actorTask)
{
    enum {
        ACTOR_403000_ATTACK_GRAB                  = 1,
        ACTOR_403000_PLAYER_CLIP_GRAB_CATCH       = 3,
        ACTOR_403000_CLIP_GRAB_RUSH               = 11,
        ACTOR_403000_GRAB_RUSH_DISTANCE           = 0xDAC,
        ACTOR_403000_GRAB_RUSH_TRAVEL_TICKS       = 15,
        ACTOR_403000_GRAB_BEFORE_FIRST_FRAME      = -1,
        ACTOR_403000_GRAB_CATCH_FRAME             = 0xA,
        ACTOR_403000_GRAB_FRONT_PLAYER_YAW_OFFSET = 0x500,
        ACTOR_403000_GRAB_FATAL_DELAY             = 0x28,
        ACTOR_403000_GRAB_WALK_STEP               = 300,
        ACTOR_403000_GRAB_RUSH_END_FRAME          = 0xE,
        ACTOR_403000_GRAB_WALL_TURN_DELAY         = 0x10,
    };

    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    GfxCoord*                 walkCoord;
    GfxCoord*                 rushCoord;
    GfxCoord*                 rootCoord;
    SVECTOR*                  toPlayer;
    Task*                     damageTarget;
    GameActor*                playerActor;
    s16                       rushDistance;
    s16                       cellDifference;
    s32                       headingError;
    s32                       playerFacingDifference;
    s8                        towardPlayerDir;
    s16                       actorCell;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    enemy   = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attackTarget.vx = player->extra.tmd->coords->coord.t[0] - actorTask->extra.tmd->coords->coord.t[0];
        work->attackTarget.vy = player->extra.tmd->coords->coord.t[1] - actorTask->extra.tmd->coords->coord.t[1];
        work->attackTarget.vz = player->extra.tmd->coords->coord.t[2] - actorTask->extra.tmd->coords->coord.t[2];
        work->requestedAnimId = ACTOR_403000_CLIP_WALK;
        work->animStart       = ACTOR_403000_ANIM_BLEND_IN;
        work->stateFrame      = 0;
    }
    if (work->requestedAnimId == ACTOR_403000_CLIP_WALK) {
        scratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] - actorTask->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = 0;
        scratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] - actorTask->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        if (scratch->playerDistance < ACTOR_403000_GRAB_RUSH_DISTANCE) {
            work->requestedAnimId = ACTOR_403000_CLIP_GRAB_RUSH;
            work->animStart       = ACTOR_403000_ANIM_RESTART;
            work->stateFrame      = ACTOR_403000_GRAB_BEFORE_FIRST_FRAME;
            work->grabStep        = scratch->playerDistance / ACTOR_403000_GRAB_RUSH_TRAVEL_TICKS;
        }
    }
    // Catch only at the rush contact frame; the hold must accept before damage.
    if (work->requestedAnimId == ACTOR_403000_CLIP_GRAB_RUSH) {
        if (work->stateFrame == ACTOR_403000_GRAB_CATCH_FRAME) {
            toPlayer  = &scratch->offset;
            rootCoord = actorTask->extra.tmd->coords;
            _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, toPlayer);
            scratch->turn = headingError = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, toPlayer->vx, toPlayer->vz);
            if (ABS(headingError) < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                if (_actor403000HasPlayerContact(work->neckSphere.contacts, ARRAY_SIZE(work->neckSphere.contacts)) && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
                    work->state           = ACTOR_403000_STATE_GRAB_CATCH;
                    work->playerCaught    = 1;
                    work->attackTarget.vx = player->extra.tmd->coords->coord.t[0] - actorTask->extra.tmd->coords->coord.t[0];
                    work->attackTarget.vy = player->extra.tmd->coords->coord.t[1] - actorTask->extra.tmd->coords->coord.t[1];
                    work->attackTarget.vz = player->extra.tmd->coords->coord.t[2] - actorTask->extra.tmd->coords->coord.t[2];
                    scratch->playerYaw    = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0], gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
                    toPlayer              = &scratch->offset;
                    rootCoord             = actorTask->extra.tmd->coords;
                    _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, toPlayer);
                    // Select paired player clips from the facing relative to the attacker.
                    scratch->yawFromPlayer = ratan2(scratch->offset.vx, scratch->offset.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                    scratch->yawFromPlayer = playerFacingDifference = _actorAngleNormalizeYaw(scratch->yawFromPlayer);
                    playerFacingDifference                         -= scratch->playerYaw;
                    if (ABS(playerFacingDifference) < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                        work->playerAnimation.source.sets          = D_actor_403000_80158C08;
                        work->playerAnimation.animationId          = ACTOR_403000_PLAYER_CLIP_GRAB_CATCH;
                        work->playerAnimation.blend                = ANIMATION_BLEND_RESET;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        _actor403000AlignGrabCatch(actorTask, player, scratch, -ACTOR_403000_GRAB_FRONT_PLAYER_YAW_OFFSET);
                    } else {
                        work->playerAnimation.source.sets          = D_actor_403000_80158C28;
                        work->playerAnimation.animationId          = ACTOR_403000_PLAYER_CLIP_GRAB_CATCH;
                        work->playerAnimation.blend                = ANIMATION_BLEND_RESET;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        _actor403000AlignGrabCatch(actorTask, player, scratch, ACTOR_TRANSFORM_ANGLE_TURN / 4);
                    }
                    damageTarget           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    scratch->messageResult = taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_403000_ATTACK_GRAB), 0);
                    if (scratch->messageResult == 1) {
                        playerActor                     = player->work;
                        gGameSession->deathFadeFrames   = ACTOR_403000_GRAB_FATAL_DELAY;
                        gGameSession->deathRestartDelay = ACTOR_403000_GRAB_FATAL_DELAY;
                        playerActor->state              = ACTOR_403000_PLAYER_FATAL_HOLD_STATE;
                    }
                }
            }
        }
    }
    if (work->requestedAnimId == ACTOR_403000_CLIP_WALK) {
        walkCoord = actorTask->extra.tmd->coords;
        _actor403000AdvanceChaseRoot(walkCoord, ACTOR_403000_GRAB_WALK_STEP);
    }
    if (work->requestedAnimId == ACTOR_403000_CLIP_GRAB_RUSH && work->stateFrame < ACTOR_403000_GRAB_RUSH_END_FRAME) {
        rushCoord    = actorTask->extra.tmd->coords;
        rushDistance = work->grabStep;
        _actor403000AdvanceChaseRoot(rushCoord, rushDistance);
    }
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (_actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 1 && work->requestedAnimId == ACTOR_403000_CLIP_WALK && work->stateFrame >= ACTOR_403000_GRAB_WALL_TURN_DELAY) {
        work->state       = ACTOR_403000_STATE_TURN;
        work->turnRingDir = -work->watchRingDir;
    }
    _actor403000UpdateAnimation(actorTask);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->requestedAnimId == ACTOR_403000_CLIP_GRAB_RUSH) {
            scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
            actorCell           = _actor403000GetRingCell(actorTask->extra.tmd->coords);
            scratch->cell       = actorCell;
            cellDifference      = scratch->cell - scratch->playerCell;
            towardPlayerDir     = _actor403000RingSide(cellDifference);
            work->watchRingDir  = towardPlayerDir;
            cellDifference      = scratch->cell - scratch->playerCell;
            towardPlayerDir     = _actor403000RingSide(cellDifference);
            work->turnRingDir = work->watchRingDir = -towardPlayerDir;
            work->state                            = ACTOR_403000_STATE_TURN;
        }
    }
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Pushes the caught player sideways until the grab clip reaches its boundary.
///
/// Requires live actor work/model/enemy and player. Entry latches an 85-unit
/// ground displacement in persistent message storage; the first 40 ticks send
/// it synchronously with player control retained. A wall reply cancels later
/// movement. The boundary hands the reversed ring direction to the turn state.
/// The chase scratch reservation is released before return.
static void _actor403000GrabCatchState(Task* actorTask)
{
    enum {
        ACTOR_403000_GRAB_CATCH_PUSH_STEP  = 0x55,
        ACTOR_403000_GRAB_CATCH_PUSH_TICKS = 0x28,
    };

    Actor403000Work*          work;
    Enemy*                    enemy;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    s32                       soundId;
    s32                       panOffset;
    s32                       wallBlocked;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    if (work->stateEntered != 0) {
        enemy            = actorTask->spawnArg2.pointer;
        work->stateFrame = 0;
        gfxReadMatrixXAxis(&actorTask->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(ACTOR_403000_GRAB_CATCH_PUSH_STEP);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        soundId                                        = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403000_SOUND_CATCH;
        panOffset                                      = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
    }
    // Keep resending the latched shove until a wall reply cancels its displacement.
    if (work->stateFrame < ACTOR_403000_GRAB_CATCH_PUSH_TICKS) {
        wallBlocked = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.push, 0);
        if (wallBlocked == 1) {
            D_actor_403000_80158DB0.push.displacement.vx   = 0;
            D_actor_403000_80158DB0.push.displacement.vy   = 0;
            D_actor_403000_80158DB0.push.displacement.vz   = 0;
            D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
            D_actor_403000_80158DB0.push.keepControl       = wallBlocked;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state        = ACTOR_403000_STATE_TURN;
        work->watchRingDir = work->turnRingDir = work->patrolRingDir = -work->watchRingDir;
    }
    _actor403000UpdateAnimation(actorTask);
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Lunges at the player and catches them on a head-capsule contact at frame 23.
///
/// Requires live actor work/model/enemy, player and initialized contact tables.
/// Entry computes a per-tick forward step of (horizontal distance - 900)/20.
/// Movement runs on frames 5..24 and steering on 5..14, with a 64-unit yaw
/// limit in 4096-per-turn angles. A successful hold starts paired player clip
/// 1 and damage attack 0; its bank remains live through catch playback.
/// Movement uses the arena parent frame. Scratch reservations are released.
static void _actor403000LungeState(Task* actorTask)
{
    enum {
        ACTOR_403000_ATTACK_LUNGE              = 0,
        ACTOR_403000_PLAYER_CLIP_LUNGE_CATCH   = 1,
        ACTOR_403000_CLIP_LUNGE                = 7,
        ACTOR_403000_LUNGE_STOP_DISTANCE       = 900,
        ACTOR_403000_LUNGE_TRAVEL_TICKS        = 20,
        ACTOR_403000_LUNGE_MOVE_START_FRAME    = 5,
        ACTOR_403000_LUNGE_MOVE_END_FRAME      = 25,
        ACTOR_403000_LUNGE_STEER_TICKS         = 10,
        ACTOR_403000_LUNGE_TURN_STEP           = 0x40,
        ACTOR_403000_LUNGE_PLAYER_BLEND_TICKS  = 3,
        ACTOR_403000_LUNGE_CATCH_FRAME         = 0x17,
        ACTOR_403000_LUNGE_FATAL_FADE_TICKS    = 0x1C,
        ACTOR_403000_LUNGE_FATAL_RESTART_TICKS = 0x1E,
    };

    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    GfxCoord*                 rootCoord;
    Task*                     damageTarget;
    s16                       stepDistance;
    s32                       playerDistance;
    SVECTOR*                  toPlayer;
    GfxCoord*                 steeringCoord;
    GameActor*                playerActor;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    enemy   = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->lungePlayerPos.vx = player->extra.tmd->coords->coord.t[0];
        work->lungePlayerPos.vy = player->extra.tmd->coords->coord.t[1];
        work->lungePlayerPos.vz = player->extra.tmd->coords->coord.t[2];
        scratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] - actorTask->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = 0;
        scratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] - actorTask->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = playerDistance = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        work->requestedAnimId                    = ACTOR_403000_CLIP_LUNGE;
        work->animStart                          = ACTOR_403000_ANIM_BLEND_IN;
        work->stateFrame                         = 0;
        work->rootSphere.body.flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->neckSphere.body.flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->lungeStep                          = (playerDistance - ACTOR_403000_LUNGE_STOP_DISTANCE) / ACTOR_403000_LUNGE_TRAVEL_TICKS;
    }
    if (work->stateFrame >= ACTOR_403000_LUNGE_MOVE_START_FRAME && work->stateFrame < ACTOR_403000_LUNGE_MOVE_END_FRAME) {
        rootCoord    = actorTask->extra.tmd->coords;
        stepDistance = work->lungeStep;
        _actor403000AdvanceChaseRoot(rootCoord, stepDistance);
        actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((u16)(work->stateFrame - ACTOR_403000_LUNGE_MOVE_START_FRAME) < ACTOR_403000_LUNGE_STEER_TICKS) {
        toPlayer      = &scratch->offset;
        steeringCoord = actorTask->extra.tmd->coords;
        _actorPositionDeltaToPlayer(&gPlayerStatus, steeringCoord, toPlayer);
        scratch->turn = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, toPlayer->vx, toPlayer->vz);
        if (scratch->turn > ACTOR_403000_LUNGE_TURN_STEP) {
            scratch->turn = ACTOR_403000_LUNGE_TURN_STEP;
        } else if (scratch->turn < -ACTOR_403000_LUNGE_TURN_STEP) {
            scratch->turn = -ACTOR_403000_LUNGE_TURN_STEP;
        }
        scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    }
    // Contact and hold acceptance gate the paired animation and attack damage.
    if (work->stateFrame == ACTOR_403000_LUNGE_CATCH_FRAME) {
        if (_actor403000HasPlayerContact(work->headCapsule.contacts, ARRAY_SIZE(work->headCapsule.contacts)) && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
            D_actor_403000_80158D90.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
            D_actor_403000_80158D90.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
            D_actor_403000_80158D90.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
            D_actor_403000_80158D90.placement.rot.vx = 0;
            D_actor_403000_80158D90.placement.rot.vy = ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]) - (ACTOR_TRANSFORM_ANGLE_TURN / 4);
            D_actor_403000_80158D90.placement.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403000_80158D90.placement, 0);
            damageTarget           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->messageResult = taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_403000_ATTACK_LUNGE), 0);
            if (scratch->messageResult == 1) {
                playerActor                     = player->work;
                gGameSession->deathFadeFrames   = ACTOR_403000_LUNGE_FATAL_FADE_TICKS;
                gGameSession->deathRestartDelay = ACTOR_403000_LUNGE_FATAL_RESTART_TICKS;
                playerActor->state              = ACTOR_403000_PLAYER_FATAL_HOLD_STATE;
            }
            work->state                                = ACTOR_403000_STATE_LUNGE_CATCH;
            work->playerAnimation.source.sets          = D_actor_403000_80158C08;
            work->playerAnimation.animationId          = ACTOR_403000_PLAYER_CLIP_LUNGE_CATCH;
            work->playerAnimation.blend                = ANIMATION_BLEND_RESET;
            work->playerAnimation.blendFrames          = ACTOR_403000_LUNGE_PLAYER_BLEND_TICKS;
            work->playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
            work->playerCaught = 1;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state         = ACTOR_403000_STATE_PATROL;
        work->patrolRingDir = work->watchRingDir;
        work->watchRingDir  = -work->watchRingDir;
    }
    _actor403000UpdateAnimation(actorTask);
    if (_actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 0) {
        _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->neckSphere.contacts, ARRAY_SIZE(work->neckSphere.contacts));
    }
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
    work->stateFrame++;
}

/// Lunge-and-shove (animation 0x11): on the frame `stateEntered` is set, place the
/// model a fixed distance short of the player along its facing, turn it toward
/// the camera target and start the hit effect; for the first ten frames step it
/// forward and push the player with `GAME_ACTOR_MESSAGE_MOVE_BY`, turn for the first six, and
/// on frame 0x29 aim a sideways push that frames 0x2C..0x35 keep resending.
/// `ANIMATION_SLOT_REACHED_BOUNDARY` on slot 1 moves the state machine to 4 with a fresh `watchRingDir`.
static void func_actor_403000_80138DB0(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    _Actor403000ChaseScratch* head;
    GfxCoord*                 coord;
    GfxCoord*                 pos;
    GfxCoord*                 pos2;
    SVECTOR*                  dir;
    SVECTOR*                  t1;
    SVECTOR*                  vp;
    SVECTOR*                  t2;
    SVECTOR*                  t3;
    SVECTOR*                  t4;
    SVECTOR                   v;
    s16                       diff;
    s32                       dist;
    s32                       ret;
    s16                       cell;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                           = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = head - 1;
    scratch                                        = head - 1;
    if (work->stateEntered != 0) {
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->lungePlayerPos.vx      = player->extra.tmd->coords->coord.t[0];
        work->lungePlayerPos.vy      = player->extra.tmd->coords->coord.t[1];
        work->lungePlayerPos.vz      = player->extra.tmd->coords->coord.t[2];
        scratch->playerDelta.vx      = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy      = 0;
        scratch->playerDelta.vz      = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = dist = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        work->requestedAnimId          = 0x11;
        work->animStart                = ACTOR_403000_ANIM_RESTART;
        work->stateFrame               = 0;
        work->lungeStep                = (dist - 900) / 20;
        t1                             = &scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t1);
        VectorNormalSS(t1, t1);
        _actor403000ScaleVectorQ12(t1, 0x41A);
        arg0->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] - scratch->offset.vx;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - scratch->offset.vy;
        arg0->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] - scratch->offset.vz;
        pos                                 = arg0->extra.tmd->coords;
        t1->vx                              = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
        t1->vy                              = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
        t1->vz                              = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
        scratch->turn                       = _actorAngleTurnToOffset(arg0->extra.tmd->coords, t1->vx, t1->vz);
        scratch->turn                      += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        t2                                    = &scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t2);
        VectorNormalSS(t2, t2);
        _actor403000ScaleVectorQ12(t2, 0x96);
        D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        padScriptSpawnVariableMotorRamp(5, 0xFF, 0x80);
        work->hitEffectArg.coord      = &player->extra.tmd->coords[3];
        work->hitEffectArg.spawnArgLo = 0x500;
        work->hitEffectArg.spawnArgHi = 3;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x100F), &player->extra.tmd->coords[3], 0, &work->hitEffectArg);
    }
    if (work->stateFrame < 10) {
        coord = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = _actor403000ReserveScratchVector();
            gfxReadMatrixZAxis(&coord->coord, dir);
            VectorNormalSS(dir, dir);
            _actor403000ScaleVectorQ12(dir, 0x78);
            coord->coord.t[0]  += dir->vx;
            coord->coord.t[1]  += dir->vy;
            coord->coord.t[2]  += dir->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            _actor403000ReleaseScratchVector();
        }
        v.vz = 0;
        v.vy = 0;
        v.vx = 0;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &v);
        vp = &v;
        VectorNormalSS(vp, vp);
        _actor403000ScaleVectorQ12(vp, 0x546);
        v.vx = v.vx + arg0->extra.tmd->coords->coord.t[0] - player->extra.tmd->coords->coord.t[0];
        v.vy = 0;
        v.vz = v.vz + arg0->extra.tmd->coords->coord.t[2] - player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(vp, vp);
        _actor403000ScaleVectorQ12(vp, 0x96);
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        D_actor_403000_80158DB0.push.displacement.vx   = v.vx;
        D_actor_403000_80158DB0.push.displacement.vz   = v.vz;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.push, 0);
    }
    if (work->stateFrame < 6) {
        t3             = &scratch->offset;
        pos2           = arg0->extra.tmd->coords;
        t3->vx         = gPlayerStatus.coordMtx->t[0] - pos2->coord.t[0];
        t3->vy         = gPlayerStatus.coordMtx->t[1] - pos2->coord.t[1];
        t3->vz         = gPlayerStatus.coordMtx->t[2] - pos2->coord.t[2];
        scratch->turn  = _actorAngleTurnToOffset(arg0->extra.tmd->coords, t3->vx, t3->vz);
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->stateFrame == 0x29) {
        t4 = &scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        _actor403000ScaleVectorQ12(t4, 0x21);
        D_actor_403000_80158DB0.push.displacement.vx = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy = 0;
        D_actor_403000_80158DB0.push.displacement.vz = scratch->offset.vz;
        gfxReadMatrixXAxis(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        _actor403000ScaleVectorQ12(t4, 0x7D);
        D_actor_403000_80158DB0.push.displacement.vx  += scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vz  += scratch->offset.vz;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
    }
    if ((u16)(work->stateFrame - 0x2C) < 10) {
        ret = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.push, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.push.displacement.vx   = 0;
            D_actor_403000_80158DB0.push.displacement.vy   = 0;
            D_actor_403000_80158DB0.push.displacement.vz   = 0;
            D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
            D_actor_403000_80158DB0.push.keepControl       = ret;
        }
    }
    if (work->stateFrame == 0x39) {
        work->playerAnimation.source.sets          = D_actor_403000_80158C08;
        work->playerAnimation.animationId          = 2;
        work->playerAnimation.blend                = 0;
        work->playerAnimation.blendFrames          = 0;
        work->playerAnimation.enableWorldCollision = 1;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
        cell                = _actor403000GetRingCell(arg0->extra.tmd->coords);
        scratch->cell       = cell;
        diff                = (s8)cell - scratch->playerCell;
        work->watchRingDir  = _actor403000RingSide(diff);
        diff                = scratch->cell - scratch->playerCell;
        work->turnRingDir = work->watchRingDir = -_actor403000RingSide(diff);
        work->state                            = ACTOR_403000_STATE_TURN;
    }
    _actor403000UpdateAnimation(arg0);
    if (++work->stateFrame < 10) {
        _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
        _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->neckSphere.contacts, ARRAY_SIZE(work->neckSphere.contacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays the fall and chooses stunned, down or dissolve when it settles.
///
/// Requires live actor work/model/enemy and initialized root/torso contacts.
/// Enables both grid probes during the fall, resolving the torso only when
/// the root produces no horizontal pushback, then disables the torso probe.
/// A lethal fall leaves the room report pending for the dissolve state.
static void _actor403000KnockdownState(Task* actorTask)
{
    enum {
        ACTOR_403000_CLIP_KNOCKDOWN = 14,
    };

    Actor403000Work* work;
    Enemy*           enemy;

    work  = actorTask->work;
    enemy = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actorTask->extra.tmd->flags   = 0;
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->requestedAnimId         = ACTOR_403000_CLIP_KNOCKDOWN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lockOnSuspended         = 0;
        work->neckYaw                 = 0;
        work->neckYawTarget           = 0;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->torsoSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (_actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 0) {
        _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->torsoSphere.contacts, ARRAY_SIZE(work->torsoSphere.contacts));
    }
    _actor403000UpdateAnimation(actorTask);
    // The torso grid probe is needed only while the falling clip is active.
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && work->requestedAnimId == ACTOR_403000_CLIP_KNOCKDOWN) {
        work->torsoSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor403000FinishDownReaction(work, enemy);
    }
}

/// Walks the shorter ring route toward the player until direct pursuit is allowed.
///
/// Requires live actor work/model, player and initialized arena scratch stack.
/// Ring cells are 0..9 and the latched direction is +/-1. Each unfrozen tick
/// advances 300 parent-coordinate units toward the neighboring waypoint and
/// turns at most 48 angle units (4096 per turn). Releases waypoint scratch.
static void _actor403000ApproachState(Task* actorTask)
{
    enum {
        ACTOR_403000_APPROACH_TURN_STEP = 0x30,
        ACTOR_403000_APPROACH_WALK_STEP = 0x12C,
    };

    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   model;
    const SVECTOR*               waypoints;
    const SVECTOR*               waypointPos;
    s16                          waypointTurn;
    s16                          cellDifference;
    s8                           actorCell;
    s8                           towardPlayerDir;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_WALK;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(actorTask);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_RING_PROBE_REACH;
        scratch->playerCell                = _actor403000GetRingCell(player->extra.tmd->coords);
        scratch->cell                      = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        cellDifference                     = scratch->cell - scratch->playerCell;
        towardPlayerDir                    = _actor403000RingSide(cellDifference);
        work->seekRingDir                  = towardPlayerDir;
    }
    _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
    actorCell           = _actor403000GetRingCell(actorTask->extra.tmd->coords);
    scratch->cell       = actorCell;
    if ((s16)_actor403000CanChasePlayer(actorTask, actorCell, scratch->playerCell)) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    // Follow the neighboring waypoint even on the tick that selects pursuit.
    _actor403000SelectRingNeighbor(scratch, work->seekRingDir);
    waypoints           = D_actor_403000_80158CE0;
    waypointPos         = &waypoints[scratch->waypoint];
    scratch->offset.vx  = waypointPos->vx;
    scratch->offset.vy  = waypointPos->vy;
    scratch->offset.vz  = waypointPos->vz;
    scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
    waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
    scratch->turn       = waypointTurn;
    work->neckYawTarget = waypointTurn;
    _actor403000UpdateAnimation(actorTask);
    if (scratch->turn > ACTOR_403000_APPROACH_TURN_STEP) {
        scratch->turn = ACTOR_403000_APPROACH_TURN_STEP;
    }
    if (scratch->turn < -ACTOR_403000_APPROACH_TURN_STEP) {
        scratch->turn = -ACTOR_403000_APPROACH_TURN_STEP;
    }
    scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actorMovementStepForward(actorTask->extra.tmd->coords, ACTOR_403000_APPROACH_WALK_STEP);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Walks away around the ring before permitting a return to direct pursuit.
///
/// Requires live actor work/model, player and initialized arena scratch stack.
/// Ring cells are 0..9 and direction is +/-1; equal entry cells reverse the
/// existing seek direction. Advances 300 parent-coordinate units per unfrozen
/// tick, turning at most 64 angle units in 4096 per turn. Pursuit is gated
/// until the state counter exceeds 60. Releases waypoint scratch.
static void _actor403000RetreatState(Task* actorTask)
{
    enum {
        ACTOR_403000_RETREAT_MIN_TICKS = 0x3C,
        ACTOR_403000_RETREAT_TURN_STEP = 0x40,
        ACTOR_403000_RETREAT_WALK_STEP = 0x12C,
    };

    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   model;
    const SVECTOR*               waypoints;
    const SVECTOR*               waypointPos;
    s16                          waypointTurn;
    s16                          cellDifference;
    s32                          entryCell;
    s8                           actorCell;
    s8                           towardPlayerDir;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_WALK;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(actorTask);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_RING_PROBE_REACH;
        scratch->playerCell                = _actor403000GetRingCell(player->extra.tmd->coords);
        entryCell                          = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        scratch->cell                      = entryCell;
        if (entryCell != scratch->playerCell) {
            cellDifference  = entryCell - scratch->playerCell;
            towardPlayerDir = _actor403000RingSide(cellDifference);
        } else {
            towardPlayerDir = work->seekRingDir;
        }
        work->seekRingDir = -towardPlayerDir;
        work->stateFrame  = 0;
    }
    work->stateFrame++;
    _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
    actorCell           = _actor403000GetRingCell(actorTask->extra.tmd->coords);
    scratch->cell       = actorCell;
    if ((s16)_actor403000CanChasePlayer(actorTask, actorCell, scratch->playerCell) && work->stateFrame > ACTOR_403000_RETREAT_MIN_TICKS) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    // Follow the latched away direction throughout the minimum retreat interval.
    _actor403000SelectRingNeighbor(scratch, work->seekRingDir);
    waypoints           = D_actor_403000_80158CE0;
    waypointPos         = &waypoints[scratch->waypoint];
    scratch->offset.vx  = waypointPos->vx;
    scratch->offset.vy  = waypointPos->vy;
    scratch->offset.vz  = waypointPos->vz;
    scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
    waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
    scratch->turn       = waypointTurn;
    work->neckYawTarget = waypointTurn;
    _actor403000UpdateAnimation(actorTask);
    if (scratch->turn > ACTOR_403000_RETREAT_TURN_STEP) {
        scratch->turn = ACTOR_403000_RETREAT_TURN_STEP;
    }
    if (scratch->turn < -ACTOR_403000_RETREAT_TURN_STEP) {
        scratch->turn = -ACTOR_403000_RETREAT_TURN_STEP;
    }
    scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actorMovementStepForward(actorTask->extra.tmd->coords, ACTOR_403000_RETREAT_WALK_STEP);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Walks to a waypoint across the arena and chooses watch, approach or ambush.
///
/// Requires live actor work/model, player and initialized arena scratch stack.
/// Player cells 0..2, 3..4, 5..7 and 8..9 select goals 5, 9, 0 and 4.
/// A preselected direction is retained; otherwise entry chooses away from the
/// player. Advances 300 parent-coordinate units per unfrozen tick and turns
/// at most 64 units in 4096-per-turn angles. Releases waypoint scratch.
static void _actor403000PatrolState(Task* actorTask)
{
    enum {
        ACTOR_403000_TEXTURE_BURNED           = 2,
        ACTOR_403000_PATROL_GOAL_NONE         = -1,
        ACTOR_403000_PATROL_TURN_STEP         = 0x40,
        ACTOR_403000_PATROL_WALK_STEP         = 0x12C,
        ACTOR_403000_PATROL_DIRECTION_UNSET   = 0,
        ACTOR_403000_PATROL_ROLL_MASK         = 0xF,
        ACTOR_403000_PATROL_FIRST_ROLL_SPLIT  = 7,
        ACTOR_403000_PATROL_SECOND_ROLL_SPLIT = 0xB,
    };

    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   model;
    const SVECTOR*               waypoints;
    const SVECTOR*               waypointPos;
    s16                          waypointTurn;
    s16                          cellDifference;
    s16                          behaviorRoll;
    s8                           actorCell;
    s8                           towardPlayerDir;
    s8                           goalCell;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_WALK;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(actorTask);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_RING_PROBE_REACH;
        scratch->playerCell                = _actor403000GetRingCell(player->extra.tmd->coords);
        scratch->cell                      = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        switch ((u8)scratch->playerCell) {
            case 0:
            case 1:
            case 2:
                goalCell = 5;
                break;
            case 3:
            case 4:
                goalCell = 9;
                break;
            case 5:
            case 6:
            case ACTOR_403000_PATROL_FIRST_ROLL_SPLIT:
                goalCell = 0;
                break;
            case 8:
            case 9:
                goalCell = 4;
                break;
            default:
                goalCell = ACTOR_403000_PATROL_GOAL_NONE;
                break;
        }
        work->patrolGoalCell = goalCell;
        if (work->patrolRingDir == ACTOR_403000_PATROL_DIRECTION_UNSET) {
            cellDifference      = scratch->cell - scratch->playerCell;
            towardPlayerDir     = _actor403000RingSide(cellDifference);
            work->patrolRingDir = -towardPlayerDir;
        }
    }
    work->stateFrame++;
    _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    actorCell     = _actor403000GetRingCell(actorTask->extra.tmd->coords);
    scratch->cell = actorCell;
    // Burned appearance changes the roll thresholds; an explicit ambush wins first.
    if (actorCell == work->patrolGoalCell) {
        if (work->ambushRequested == 1) {
            work->state = ACTOR_403000_STATE_AMBUSH;
        } else {
            behaviorRoll = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_403000_PATROL_ROLL_MASK;
            if (actorTask->extra.tmd->texturePageOffset == ACTOR_403000_TEXTURE_BURNED) {
                if (behaviorRoll <= 0) {
                    work->state = ACTOR_403000_STATE_WATCH;
                } else if (behaviorRoll < ACTOR_403000_PATROL_FIRST_ROLL_SPLIT) {
                    work->state = ACTOR_403000_STATE_AMBUSH;
                } else {
                    work->state = ACTOR_403000_STATE_APPROACH;
                }
            } else if (behaviorRoll < ACTOR_403000_PATROL_FIRST_ROLL_SPLIT) {
                work->state = ACTOR_403000_STATE_WATCH;
            } else if (behaviorRoll < ACTOR_403000_PATROL_SECOND_ROLL_SPLIT) {
                work->state = ACTOR_403000_STATE_AMBUSH;
            } else {
                work->state = ACTOR_403000_STATE_APPROACH;
            }
        }
    }
    _actor403000SelectRingNeighbor(scratch, work->patrolRingDir);
    waypoints           = D_actor_403000_80158CE0;
    waypointPos         = &waypoints[scratch->waypoint];
    scratch->offset.vx  = waypointPos->vx;
    scratch->offset.vy  = waypointPos->vy;
    scratch->offset.vz  = waypointPos->vz;
    scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
    waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
    scratch->turn       = waypointTurn;
    work->neckYawTarget = waypointTurn;
    _actor403000UpdateAnimation(actorTask);
    if (scratch->turn > ACTOR_403000_PATROL_TURN_STEP) {
        scratch->turn = ACTOR_403000_PATROL_TURN_STEP;
    }
    if (scratch->turn < -ACTOR_403000_PATROL_TURN_STEP) {
        scratch->turn = -ACTOR_403000_PATROL_TURN_STEP;
    }
    scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actorMovementStepForward(actorTask->extra.tmd->coords, ACTOR_403000_PATROL_WALK_STEP);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Waits on its current waypoint facing a neighboring cell of the arena ring.
///
/// Requires live actor work/model, player and initialized arena scratch stack.
/// Entry snaps XYZ to the current cell's waypoint and faces the neighbor in
/// watch direction +/-1. Returns to patrol when the player enters its cell,
/// permits pursuit after 60 ticks, or prowls after 300. Cells must be 0..9;
/// positions use the arena parent frame. Releases waypoint scratch.
static void _actor403000WatchState(Task* actorTask)
{
    enum {
        ACTOR_403000_WATCH_TIMEOUT_TICKS     = 300,
        ACTOR_403000_WATCH_CHASE_DELAY_TICKS = 60,
    };

    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   model;
    const SVECTOR*               waypoints;
    const SVECTOR*               waypointPos;
    s16                          waypointTurn;
    s16                          neighborCell;
    s8                           actorCell;
    const SVECTOR*               lastWaypoint;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_STAND;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(actorTask);
        // Snap to the cell waypoint and face its neighbor without walking.
        actorCell                                  = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        scratch->cell                              = actorCell;
        waypoints                                  = D_actor_403000_80158CE0;
        waypointPos                                = &waypoints[actorCell];
        scratch->offset.vx                         = waypointPos->vx;
        scratch->offset.vy                         = waypointPos->vy;
        scratch->offset.vz                         = waypointPos->vz;
        actorTask->extra.tmd->coords->coord.t[0]   = scratch->offset.vx;
        actorTask->extra.tmd->coords->coord.t[1]   = scratch->offset.vy;
        actorTask->extra.tmd->coords->coord.t[2]   = scratch->offset.vz;
        actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->watchRingDir == 1) {
            if (scratch->cell + 1 >= ACTOR_403000_RING_WAYPOINT_COUNT) {
                scratch->offset.vx = waypoints[0].vx;
                scratch->offset.vy = waypoints[0].vy;
                scratch->offset.vz = waypoints[0].vz;
            } else {
                neighborCell       = scratch->cell + 1;
                scratch->offset.vx = waypoints[neighborCell].vx;
                scratch->offset.vy = waypoints[neighborCell].vy;
                scratch->offset.vz = waypoints[neighborCell].vz;
            }
        } else {
            if (scratch->cell - 1 < 0) {
                lastWaypoint       = &waypoints[ACTOR_403000_RING_WAYPOINT_COUNT - 1];
                scratch->offset.vx = lastWaypoint->vx;
                scratch->offset.vy = lastWaypoint->vy;
                scratch->offset.vz = lastWaypoint->vz;
            } else {
                neighborCell       = scratch->cell - 1;
                scratch->offset.vx = waypoints[neighborCell].vx;
                scratch->offset.vy = waypoints[neighborCell].vy;
                scratch->offset.vz = waypoints[neighborCell].vz;
            }
        }
        scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
        scratch->offset.vy  = 0;
        scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
        waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
        scratch->turn       = waypointTurn;
        work->neckYawTarget = 0;
        scratch->turn      += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_RING_PROBE_REACH;
        scratch->playerCell                = _actor403000GetRingCell(player->extra.tmd->coords);
    }
    work->stateFrame++;
    scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
    scratch->cell       = _actor403000GetRingCell(actorTask->extra.tmd->coords);
    _actor403000UpdateAnimation(actorTask);
    if (work->stateFrame > ACTOR_403000_WATCH_TIMEOUT_TICKS) {
        work->state = ACTOR_403000_STATE_PROWL;
    } else if (scratch->playerCell == scratch->cell) {
        work->state         = ACTOR_403000_STATE_PATROL;
        work->patrolRingDir = work->watchRingDir;
        work->watchRingDir  = -work->watchRingDir;
    } else if ((s16)_actor403000CanChasePlayer(actorTask, scratch->cell, scratch->playerCell)) {
        if (work->stateFrame > ACTOR_403000_WATCH_CHASE_DELAY_TICKS) {
            work->state = ACTOR_403000_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Turns toward a neighboring waypoint while applying a latched root drift.
///
/// Requires live actor work/model, root contacts and initialized arena scratch.
/// Cells are 0..9 and turn direction is +/-1. Entry chooses the neighboring
/// waypoint and converts negative turns to a positive 0..4095 yaw sweep,
/// divided into 36 steps. Drift combines 27 units along X and -41 along Z
/// of the entry facing, in parent-coordinate units. A turn below a quarter
/// turn or a clip boundary selects patrol. Releases waypoint scratch.
static void _actor403000TurnState(Task* actorTask)
{
    enum {
        ACTOR_403000_CLIP_TURN        = 9,
        ACTOR_403000_TURN_SIDE_DRIFT  = 0x1B,
        ACTOR_403000_TURN_BACK_DRIFT  = 0x29,
        ACTOR_403000_TURN_SWEEP_TICKS = 36,
    };

    Actor403000Work*             work;
    _Actor403000WaypointScratch* scratch;
    s32                          actorCell;
    TmdObject*                   model;
    const SVECTOR*               waypointPos;
    const SVECTOR*               waypoints;
    s16                          waypointTurn;

    work    = actorTask->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_TURN;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorCell                     = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        scratch->cell                 = actorCell;
        scratch->waypoint             = scratch->cell + work->turnRingDir;
        if (scratch->waypoint >= ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint -= ACTOR_403000_RING_WAYPOINT_COUNT;
        } else if (scratch->waypoint < 0) {
            scratch->waypoint += ACTOR_403000_RING_WAYPOINT_COUNT;
        }
        waypoints           = D_actor_403000_80158CE0;
        waypointPos         = &waypoints[scratch->waypoint];
        scratch->offset.vx  = waypointPos->vx;
        scratch->offset.vy  = waypointPos->vy;
        scratch->offset.vz  = waypointPos->vz;
        scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
        scratch->offset.vy -= actorTask->extra.tmd->coords->coord.t[1];
        scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
        waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
        scratch->turn       = waypointTurn;
        if (ABS(waypointTurn) < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
            work->state = ACTOR_403000_STATE_PATROL;
        }
        // Sweep in the positive yaw direction, carrying the entry-facing drift.
        if (scratch->turn < 0) {
            scratch->turn += ACTOR_TRANSFORM_ANGLE_TURN;
        }
        gfxReadMatrixXAxis(&actorTask->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(ACTOR_403000_TURN_SIDE_DRIFT);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        work->turnDrift = scratch->offset;
        gfxReadMatrixZAxis(&actorTask->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(-ACTOR_403000_TURN_BACK_DRIFT);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        work->turnStep          = scratch->turn / ACTOR_403000_TURN_SWEEP_TICKS;
        work->turnDrift.vx     += scratch->offset.vx;
        work->turnDrift.vy     += scratch->offset.vy;
        work->turnDrift.vz     += scratch->offset.vz;
        work->torsoSwayEnabled  = 0;
        work->forelegYawEnabled = 0;
        work->headSwayEnabled   = 0;
        work->neckYawEnabled    = 0;
        work->stateFrame        = 0;
    }
    work->stateFrame++;
    _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->turn = work->turnStep + ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    actorTask->extra.tmd->coords->coord.t[0]  += work->turnDrift.vx;
    actorTask->extra.tmd->coords->coord.t[1]  += work->turnDrift.vy;
    actorTask->extra.tmd->coords->coord.t[2]  += work->turnDrift.vz;
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor403000UpdateAnimation(actorTask);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403000_STATE_PATROL;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Drops from the ambush point onto the player's entry position.
///
/// Requires live actor work/model/enemy, player and initialized arena scratch.
/// Entry records the target and freezes the drop clip through the initial
/// delay. The root falls 300 units per tick and eases X/Z toward that target.
/// Frame 10 catches a player still within 1000 horizontal units if their hold
/// accepts, starts paired clip 5 and applies attack 3. The overlay bank must
/// remain live through player playback. Releases each scratch reservation.
static void _actor403000DropState(Task* actorTask)
{
    enum {
        ACTOR_403000_ATTACK_DROP            = 3,
        ACTOR_403000_PLAYER_CLIP_DROP_CATCH = 5,
        ACTOR_403000_DROP_ORIGIN_X          = 0x2134,
        ACTOR_403000_DROP_HEIGHT            = 0x1518,
        ACTOR_403000_DROP_ORIGIN_Z          = 0x1194,
        ACTOR_403000_DROP_CATCH_FRAME       = 10,
        ACTOR_403000_DROP_CATCH_RADIUS      = 1000,
        ACTOR_403000_DROP_FATAL_DELAY       = 0x28,
        ACTOR_403000_DROP_FALL_STEP         = 0x12C,
        ACTOR_403000_DROP_TURN_THRESHOLD    = 0x300,
    };

    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    Task*                     damageTarget;
    s16                       playerCell;
    GameActor*                playerActor;
    s32                       stateFrame;
    s16                       playerTurn;
    work   = actorTask->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attackTarget.vx                    = player->extra.tmd->coords->coord.t[0];
        work->attackTarget.vy                    = player->extra.tmd->coords->coord.t[1];
        work->attackTarget.vz                    = player->extra.tmd->coords->coord.t[2];
        actorTask->extra.tmd->coords->coord.t[0] = ACTOR_403000_DROP_ORIGIN_X;
        actorTask->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - ACTOR_403000_DROP_HEIGHT;
        actorTask->extra.tmd->coords->coord.t[2] = ACTOR_403000_DROP_ORIGIN_Z;
        work->requestedAnimId                    = ACTOR_403000_CLIP_DROP;
        work->animStart                          = ACTOR_403000_ANIM_RESTART;
        work->stateFrame                         = 0;
        work->animRate                           = 0;
        work->lockOnSuspended                    = 0;
        scratch                                  = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
        playerCell                               = _actor403000GetRingCell(player->extra.tmd->coords);
        scratch->playerCell                      = playerCell;
        switch (scratch->playerCell) {
            case 0:
                gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_REPLACE);
                break;

            case 1:
            case 2:
            case 3:
            case 4:
                gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, (ACTOR_TRANSFORM_ANGLE_TURN / 4), GRAPHICS_ROTATION_REPLACE);
                break;

            case 5:
                gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, 0, GRAPHICS_ROTATION_REPLACE);
                break;

            case 6:
            case 7:
            case 8:
            case 9:
            default:
                gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, -(ACTOR_TRANSFORM_ANGLE_TURN / 4), GRAPHICS_ROTATION_REPLACE);
                break;
        }

        SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
    }
    // Delay freezes the whole drop update, including its frame counter.
    if (work->dropDelay != 0) {
        work->dropDelay--;
        return;
    }
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    stateFrame = work->stateFrame;
    // Catch tests the latched target before moving the falling root this tick.
    if (stateFrame == ACTOR_403000_DROP_CATCH_FRAME) {
        work->animRate     = ANIMATION_RATE_ONE;
        scratch->offset.vx = work->attackTarget.vx - player->extra.tmd->coords->coord.t[0];
        scratch->offset.vy = work->attackTarget.vy - player->extra.tmd->coords->coord.t[1];
        scratch->offset.vz = work->attackTarget.vz - player->extra.tmd->coords->coord.t[2];
        if (!_actorRangeOutsideRadiusXZ(&scratch->offset, ACTOR_403000_DROP_CATCH_RADIUS) && enemy->hp > 0 &&
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
            damageTarget           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->messageResult = taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_403000_ATTACK_DROP), 0);
            if (scratch->messageResult == 1) {
                playerActor                     = player->work;
                gGameSession->deathFadeFrames   = ACTOR_403000_DROP_FATAL_DELAY;
                gGameSession->deathRestartDelay = ACTOR_403000_DROP_FATAL_DELAY;
                playerActor->state              = ACTOR_403000_PLAYER_FATAL_HOLD_STATE;
            }
            work->state                                = ACTOR_403000_STATE_DROP_CATCH;
            work->playerAnimation.source.sets          = D_actor_403000_80158C08;
            work->playerAnimation.animationId          = ACTOR_403000_PLAYER_CLIP_DROP_CATCH;
            work->playerAnimation.blend                = ANIMATION_BLEND_RESET;
            work->playerAnimation.blendFrames          = 0;
            work->playerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
            work->playerCaught = 1;
        }
    }
    if (actorTask->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        actorTask->extra.tmd->coords->coord.t[1] += ACTOR_403000_DROP_FALL_STEP;
    }
    if (work->stateFrame >= 4) {
        actorTask->extra.tmd->coords->coord.t[0] += (work->attackTarget.vx - actorTask->extra.tmd->coords->coord.t[0]) >> 2;
        actorTask->extra.tmd->coords->coord.t[2] += (work->attackTarget.vz - actorTask->extra.tmd->coords->coord.t[2]) >> 2;
    }
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor403000UpdateAnimation(actorTask);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        playerTurn    = _actorAngleTurnToPlayer(actorTask, &scratch->offset, &gPlayerStatus);
        scratch->turn = playerTurn;
        if (ABS(playerTurn) > ACTOR_403000_DROP_TURN_THRESHOLD) {
            work->state        = ACTOR_403000_STATE_TURN;
            work->watchRingDir = (work->patrolRingDir = (work->turnRingDir = -_actor403000ChooseRingDirection(actorTask->extra.tmd->coords)));
        } else {
            work->state         = ACTOR_403000_STATE_PATROL;
            work->watchRingDir  = -_actor403000ChooseRingDirection(actorTask->extra.tmd->coords);
            work->patrolRingDir = (work->turnRingDir = _actor403000ChooseRingDirection(actorTask->extra.tmd->coords));
        }
    }
    if (work->stateFrame >= 2) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Settles toward the captured drop target and prepares the shared player displacement.
///
/// Requires live actor work/model/enemy, player and initialized arena scratch.
/// Descends 300 parent-coordinate units per tick while above the player and
/// eases X/Z toward the latched target. The first eight ticks derive a -42-unit
/// ground displacement from the player's facing; later ticks read the bytes
/// already occupying the uninitialized reservation. This handler fills the
/// persistent push payload without dispatching it. A boundary from tick 11
/// selects the ring turn. Releases chase scratch before return.
static void _actor403000DropCatchState(Task* actorTask)
{
    enum {
        ACTOR_403000_DROP_CATCH_FALL_STEP      = 0x12C,
        ACTOR_403000_DROP_CATCH_PUSH_TICKS     = 8,
        ACTOR_403000_DROP_CATCH_PUSH_STEP      = 0x2A,
        ACTOR_403000_DROP_CATCH_TURN_MIN_TICKS = 0xB,
    };

    Actor403000Work*          work;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    Enemy*                    enemy;
    s32                       soundId;
    s32                       panOffset;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
    if (work->stateEntered != 0) {
        enemy                 = actorTask->spawnArg2.pointer;
        work->lockOnSuspended = 0;
        work->stateFrame      = 0;
        soundId               = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_403000_DROP_CATCH_PUSH_TICKS) | ACTOR_403000_SOUND_CATCH;
        panOffset             = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
    }
    if (actorTask->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        actorTask->extra.tmd->coords->coord.t[1] += ACTOR_403000_DROP_CATCH_FALL_STEP;
        actorTask->extra.tmd->coords->coord.t[0] += (work->attackTarget.vx - actorTask->extra.tmd->coords->coord.t[0]) >> 2;
        actorTask->extra.tmd->coords->coord.t[2] += (work->attackTarget.vz - actorTask->extra.tmd->coords->coord.t[2]) >> 2;
    }
    if (work->stateFrame < ACTOR_403000_DROP_CATCH_PUSH_TICKS) {
        gfxReadMatrixZAxis(&player->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(-ACTOR_403000_DROP_CATCH_PUSH_STEP);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
    }
    // Preserve the reused scratch bytes after the sampling interval; no move is sent here.
    D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
    D_actor_403000_80158DB0.push.displacement.vy   = 0;
    D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
    D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
    D_actor_403000_80158DB0.push.keepControl       = 1;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->stateFrame >= ACTOR_403000_DROP_CATCH_TURN_MIN_TICKS) {
        work->state        = ACTOR_403000_STATE_TURN;
        work->watchRingDir = work->patrolRingDir = work->turnRingDir = -_actor403000ChooseRingDirection(actorTask->extra.tmd->coords);
    }
    _actor403000UpdateAnimation(actorTask);
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Waits above the arena before announcing and scheduling the drop attack.
///
/// Requires live actor work/model/enemy and player. Entry fixes X/Z to the
/// ambush point, 5400 parent-coordinate units above the player, suspends
/// automatic lock-on and freezes the drop clip. After 60 ticks it selects
/// drop with a further 20-tick delay. The retained stationary-position check
/// applies only if this handler continues beyond tick 100.
static void _actor403000AmbushState(Task* actorTask)
{
    enum {
        ACTOR_403000_SOUND_AMBUSH             = 0x401E0010,
        ACTOR_403000_AMBUSH_ORIGIN_X          = 0x2134,
        ACTOR_403000_AMBUSH_HEIGHT            = 0x1518,
        ACTOR_403000_AMBUSH_ORIGIN_Z          = 0x1194,
        ACTOR_403000_AMBUSH_WAIT_TICKS        = 0x3C,
        ACTOR_403000_AMBUSH_DROP_DELAY_TICKS  = 0x14,
        ACTOR_403000_AMBUSH_STILL_CHECK_START = 0x64,
        ACTOR_403000_AMBUSH_STILL_TICKS       = 0x1E,
    };

    Actor403000Work* work;
    Task*            player;
    Enemy*           enemy;
    s32              timedSoundId;
    s32              timedPanOffset;
    s32              stillSoundId;
    s32              stillPanOffset;

    work   = actorTask->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->ambushRequested                    = 0;
        actorTask->extra.tmd->coords->coord.t[0] = ACTOR_403000_AMBUSH_ORIGIN_X;
        actorTask->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - ACTOR_403000_AMBUSH_HEIGHT;
        actorTask->extra.tmd->coords->coord.t[2] = ACTOR_403000_AMBUSH_ORIGIN_Z;
        work->lockOnSuspended                    = 1;
        work->requestedAnimId                    = ACTOR_403000_CLIP_DROP;
        work->animStart                          = ACTOR_403000_ANIM_RESTART;
        work->stateFrame                         = 0;
        work->stillFrames                        = 0;
        work->animRate                           = 0;
        _actor403000UpdateAnimation(actorTask);
    }
    if (work->stateFrame > ACTOR_403000_AMBUSH_WAIT_TICKS) {
        work->dropDelay = ACTOR_403000_AMBUSH_DROP_DELAY_TICKS;
        timedSoundId    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403000_SOUND_AMBUSH;
        timedPanOffset  = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
        sndEvtRequestScriptStart(timedSoundId, timedPanOffset, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
        work->state = ACTOR_403000_STATE_DROP;
    }
    // Normally the earlier drop transition ends this state before the late fallback.
    if (work->stateFrame > ACTOR_403000_AMBUSH_STILL_CHECK_START) {
        if (work->lastRootPos.vx == actorTask->extra.tmd->coords->coord.t[0] &&
            work->lastRootPos.vy == actorTask->extra.tmd->coords->coord.t[1] &&
            work->lastRootPos.vz == actorTask->extra.tmd->coords->coord.t[2]) {
            work->stillFrames++;
        } else {
            work->stillFrames = 0;
        }
        if (work->stillFrames > ACTOR_403000_AMBUSH_STILL_TICKS) {
            work->dropDelay = ACTOR_403000_AMBUSH_DROP_DELAY_TICKS;
            stillSoundId    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403000_SOUND_AMBUSH;
            stillPanOffset  = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
            sndEvtRequestScriptStart(stillSoundId, stillPanOffset, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
            work->state = ACTOR_403000_STATE_DROP;
        }
        work->lastRootPos.vx = actorTask->extra.tmd->coords->coord.t[0];
        work->lastRootPos.vy = actorTask->extra.tmd->coords->coord.t[1];
        work->lastRootPos.vz = actorTask->extra.tmd->coords->coord.t[2];
    }
    work->stateFrame++;
}

/// Walks slowly around the ring until the player can be pursued directly.
///
/// Requires live actor work/model, player and initialized arena scratch stack.
/// Entry chooses direction +/-1 from the current facing. Cells are 0..9;
/// each unfrozen tick advances 22 parent-coordinate units toward the neighbor
/// and turns at most eight angle units in 4096 per turn. Releases waypoint scratch.
static void _actor403000ProwlState(Task* actorTask)
{
    enum {
        ACTOR_403000_CLIP_PROWL      = 1,
        ACTOR_403000_PROWL_TURN_STEP = 8,
        ACTOR_403000_PROWL_WALK_STEP = 0x16,
    };

    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   model;
    const SVECTOR*               waypoints;
    const SVECTOR*               waypointPos;
    s16                          waypointTurn;
    s8                           actorCell;

    work    = actorTask->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        model                 = actorTask->extra.tmd;
        work->lockOnSuspended = 0;
        model->flags          = 0;
        tmdAllocPrimitiveBuffer(model);
        work->torsoSphere.body.radius = ACTOR_403000_ACTIVE_TORSO_RADIUS;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->overlayActive           = 0;
        work->requestedAnimId         = ACTOR_403000_CLIP_PROWL;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(actorTask);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = ACTOR_403000_RING_PROBE_REACH;
        scratch->playerCell                = _actor403000GetRingCell(player->extra.tmd->coords);
        scratch->cell                      = _actor403000GetRingCell(actorTask->extra.tmd->coords);
        work->seekRingDir                  = _actor403000ChooseRingDirection(actorTask->extra.tmd->coords);
    }
    _actorContactApplyGridPushback(actorTask->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = _actor403000GetRingCell(player->extra.tmd->coords);
    actorCell           = _actor403000GetRingCell(actorTask->extra.tmd->coords);
    scratch->cell       = actorCell;
    if ((s16)_actor403000CanChasePlayer(actorTask, actorCell, scratch->playerCell)) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    // Continue round the ring at the slow clip's small steering and movement steps.
    _actor403000SelectRingNeighbor(scratch, work->seekRingDir);
    waypoints           = D_actor_403000_80158CE0;
    waypointPos         = &waypoints[scratch->waypoint];
    scratch->offset.vx  = waypointPos->vx;
    scratch->offset.vy  = waypointPos->vy;
    scratch->offset.vz  = waypointPos->vz;
    scratch->offset.vx -= actorTask->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= actorTask->extra.tmd->coords->coord.t[2];
    waypointTurn        = _actorAngleTurnToOffset(actorTask->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
    scratch->turn       = waypointTurn;
    work->neckYawTarget = waypointTurn;
    _actor403000UpdateAnimation(actorTask);
    if (scratch->turn > ACTOR_403000_PROWL_TURN_STEP) {
        scratch->turn = ACTOR_403000_PROWL_TURN_STEP;
    }
    if (scratch->turn < -ACTOR_403000_PROWL_TURN_STEP) {
        scratch->turn = -ACTOR_403000_PROWL_TURN_STEP;
    }
    scratch->turn += ratan2(-actorTask->extra.tmd->coords->coord.m[2][0], actorTask->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&actorTask->extra.tmd->coords->coord, scratch->turn, GRAPHICS_ROTATION_REPLACE);
    actorTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actorMovementStepForward(actorTask->extra.tmd->coords, ACTOR_403000_PROWL_WALK_STEP);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

static const _Actor403000StateTable D_actor_403000_80131F44 = {
    {
        _actor403000HiddenState,
        func_actor_403000_8013D72C,
        _actor403000PatrolState,
        _actor403000WatchState,
        _actor403000TurnState,
        _actor403000ChaseState,
        _actor403000GrabCatchState,
        _actor403000GrabState,
        _actor403000LungeState,
        func_actor_403000_80138DB0,
        _actor403000RetreatState,
        _actor403000ApproachState,
        _actor403000DropState,
        _actor403000DropCatchState,
        _actor403000ProwlState,
        _actor403000AmbushState,
        _actor403000StunnedState,
        _actor403000KnockdownState,
        func_actor_403000_8013D910,
        func_actor_403000_8013D850,
        func_actor_403000_8013603C,
        func_actor_403000_80136B14,
        func_actor_403000_801365D0,
        func_actor_403000_80136D68,
        _actor403000DownHitState,
    }
};

static void func_actor_403000_8013C864(Enemy* arg0, Task* arg1)
{
    VECTOR3                    pos;
    Actor403000Work*           work;
    Task*                      player;
    _Actor403000UpdateScratch* scratch;
    _Actor403000StateTable     states;
    PlayerStatus*              config;
    u32                        sound;
    s32                        pan;

    work                                  = arg1->work;
    player                                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                = &gPlayerStatus;
    states                                = D_actor_403000_80131F44;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_403000_STATE_SCRIPTED_DISSOLVE && work->state != ACTOR_403000_STATE_DISSOLVE && work->state != ACTOR_403000_STATE_HIDDEN) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_403000_STATE_SCRIPTED_DISSOLVE && work->state != ACTOR_403000_STATE_DISSOLVE && work->state != ACTOR_403000_STATE_HIDDEN) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->rootSphere.contacts);
            worldCollisionClearContacts(work->torsoSphere.contacts);
            worldCollisionClearContacts(work->hindSphere.contacts);
            worldCollisionClearContacts(work->neckSphere.contacts);
            worldCollisionClearContacts(work->rootCapsule.contacts);
            worldCollisionClearContacts(work->headCapsule.contacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->rootSphere.contacts);
            worldCollisionClearContacts(work->torsoSphere.contacts);
            worldCollisionClearContacts(work->hindSphere.contacts);
            worldCollisionClearContacts(work->neckSphere.contacts);
            worldCollisionClearContacts(work->rootCapsule.contacts);
            worldCollisionClearContacts(work->headCapsule.contacts);
            return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000UpdateScratch);
    if (config->hp > 0) {
        func_actor_403000_80134F44(arg1);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    states.handlers[work->state](arg1);
    if (work->state != ACTOR_403000_STATE_SCRIPTED_DISSOLVE && work->state != ACTOR_403000_STATE_DISSOLVE && work->state != ACTOR_403000_STATE_SCRIPTED_BURN && work->state != ACTOR_403000_STATE_HIDDEN) {
        work->torsoSphere.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hindSphere.body.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->neckSphere.body.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->headCapsule.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->torsoSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hindSphere.body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->neckSphere.body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->headCapsule.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->lockOnSuspended == 0) {
        if (work->playerInRange == 1 && work->playerVisible == 1) {
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_KEEP_SCANNED);
        } else {
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_KEEP_SCANNED | WORLD_TARGET_NOT_LOCKABLE);
            worldTargetDisableNodeLockOn(&arg0->node);
        }
    }
    if (!(gDisplayState.animFrame & 1)) {
        scratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] - arg1->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = player->extra.tmd->coords->coord.t[1] - arg1->extra.tmd->coords->coord.t[1];
        scratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] - arg1->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        if (scratch->playerDistance > 9000) {
            work->playerInRange = 0;
        } else {
            work->playerInRange = 1;
        }
    } else {
        scratch->sightTarget.vx = player->extra.tmd->coords->workm.t[0];
        scratch->sightTarget.vy = player->extra.tmd->coords->workm.t[1];
        scratch->sightTarget.vz = player->extra.tmd->coords->workm.t[2];
        scratch->sightOrigin.vx = arg1->extra.tmd->coords->workm.t[0];
        scratch->sightOrigin.vy = arg1->extra.tmd->coords->workm.t[1];
        scratch->sightOrigin.vz = arg1->extra.tmd->coords->workm.t[2];
        if (worldCollisionSegmentOccluded(&scratch->sightOrigin, &scratch->sightTarget) != 1) {
            work->playerVisible = 1;
        } else {
            work->playerVisible = 0;
        }
    }
    if (work->playerCaught == 1) {
        work->catchFrame++;
        if (work->playerAnimation.source.sets != Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets) {
            switch (work->playerAnimation.animationId) {
                case 1:
                    if (work->catchFrame == 42) {
                        padScriptSpawnVariableMotorRamp(0xC, 0xFF, 0x80);
                    }
                    if (work->catchFrame == 52) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 3:
                    if (work->catchFrame == 4) {
                        padScriptSpawnVariableMotorRamp(0xC, 0x58, 0xFF);
                    }
                    if (work->catchFrame == 15) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 5:
                    if (work->catchFrame == 1) {
                        padScriptSpawnVariableMotorRamp(0xC, 0x58, 0xFF);
                    }
                    if (work->catchFrame == 10) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 2:
                case 4:
                case 6:
                    break;
            }
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            work->catchFrame = 0;
            if (work->playerAnimation.source.sets != Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets) {
                switch (work->playerAnimation.animationId) {
                    case 1:
                        work->playerAnimation.animationId          = 2;
                        work->playerAnimation.blend                = 1;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        break;
                    case 3:
                        work->playerAnimation.animationId          = 4;
                        work->playerAnimation.blend                = 1;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        break;
                    case 5:
                        work->playerAnimation.animationId          = 6;
                        work->playerAnimation.blend                = 1;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        break;
                    case 2:
                    case 4:
                    case 6:
                        work->playerAnimation.source.sets          = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets;
                        work->playerAnimation.animationId          = 7;
                        work->playerAnimation.blendFrames          = 0x10;
                        work->playerAnimation.blend                = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        break;
                }
            } else {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                work->playerCaught = 0;
            }
        }
    }
    scratch->flareOffset.vx = -60;
    scratch->flareOffset.vy = -40;
    scratch->flareOffset.vz = 300;
    if (work->state != ACTOR_403000_STATE_SCRIPTED_DISSOLVE && work->state != ACTOR_403000_STATE_DISSOLVE && work->state != ACTOR_403000_STATE_SCRIPTED_BURN) {
        func_actor_403000_801327B0(&arg1->extra.tmd->coords[4], &scratch->flareOffset, 0);
        func_actor_403000_80132AE0(&arg1->extra.tmd->coords[4]);
    } else {
        func_actor_403000_801330D4(&arg1->extra.tmd->coords[4]);
    }
    worldCollisionClearContacts(work->rootSphere.contacts);
    worldCollisionClearContacts(work->torsoSphere.contacts);
    worldCollisionClearContacts(work->hindSphere.contacts);
    worldCollisionClearContacts(work->neckSphere.contacts);
    worldCollisionClearContacts(work->rootCapsule.contacts);
    worldCollisionClearContacts(work->headCapsule.contacts);
    if (player->extra.tmd->coords->coord.t[1] > 3) {
        player->extra.tmd->coords->coord.t[1] = 3;
    }
    if (player->extra.tmd->coords->coord.t[2] > 0x2260) {
        player->extra.tmd->coords->coord.t[2] = 0x2260;
    }
    if (player->extra.tmd->coords->coord.t[2] < -400) {
        player->extra.tmd->coords->coord.t[2] = -400;
    }
    if (player->extra.tmd->coords->coord.t[0] < 250) {
        player->extra.tmd->coords->coord.t[0] = 250;
    }
    if (player->extra.tmd->coords->coord.t[0] > 0x477C) {
        player->extra.tmd->coords->coord.t[0] = 0x477C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000UpdateScratch);
}

/// The enemy's three state handlers - spawn, per-frame tick and teardown -
/// indexed by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_403000_80132004 = {
    {
        _actor403000Spawn,
        func_actor_403000_8013C864,
        enemyDestroy,
    },
};

/// Ignores message 2015 without accessing the receiver or either payload.
///
/// Defines no dispatch result; senders must ignore it.
static void _actor403000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
}

/// Applies a model draw mode, hiding actor behavior for every recognized mode except SHOW.
///
/// Requires live model/work. Modes 0 and 1 replace all model flags with hidden or
/// visible drawing and allocate a missing primitive buffer. Mode 2 adds automatic
/// buffer exclusion to the existing flags; mode 3 keeps only that exclusion.
/// SHOW preserves the actor state. Other modes do nothing. Ignores the message ID
/// and second payload and returns 0.
static s32 _actor403000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_403000_DRAW_HIDE                         = 0,
        ACTOR_403000_DRAW_SHOW                         = 1,
        ACTOR_403000_DRAW_KEEP_FLAGS_SKIP_AUTO_BUFFER  = 2,
        ACTOR_403000_DRAW_CLEAR_FLAGS_SKIP_AUTO_BUFFER = 3,
    };

    TmdObject*       model;
    Actor403000Work* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (drawMode) {
        case ACTOR_403000_DRAW_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_403000_STATE_HIDDEN;
            break;
        case ACTOR_403000_DRAW_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_403000_DRAW_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_403000_STATE_HIDDEN;
            break;
        case ACTOR_403000_DRAW_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_403000_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Returns 1 while the enemy has health or the model remains visible, else 0.
///
/// Requires live model/enemy objects. Ignores the message ID and both payloads.
static s32 _actor403000IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    Enemy* enemy = task->spawnArg2.pointer;

    if (enemy->hp <= 0 && (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }
    return 1;
}

/// Places the model root with yaw, pitch and roll, and records the resulting heading.
///
/// Requires live model/work and a readable placement through dispatch. XYZ uses
/// the existing parent's coordinate units; angles use 4096 per turn. Builds
/// Ry * Rx * Rz, invalidates composition and stores the matrix-derived yaw.
/// Ignores the message ID and second payload; retains no pointer and returns 1.
static s32 _actor403000Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*        coord;
    Actor403000Work* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    work->placementYaw                    = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    return 1;
}

/// Latches the requested clip and restarts entry to the scripted state.
///
/// Requires writable work and a readable request through dispatch. Only the clip
/// ID is consumed. Its low signed halfword must select a populated clip (0..17
/// or 24..29); there is no bounds check. Other playback choices are ignored.
/// Ignores the message ID and
/// second payload, retains no pointer and returns 0.
static s32 _actor403000PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    Actor403000Work* work = task->work;

    work->requestedAnimId = request->animationId;
    work->state           = ACTOR_403000_STATE_SCRIPTED;
    work->prevState       = ACTOR_403000_PREVIOUS_STATE_INVALID;
    return 0;
}

static s16 func_actor_403000_8013D48C(Task* task)
{
    Actor403000Work* work  = task->work;
    s16              found = 0;
    s16              i;
    s32              value;

    for (i = 0; i < ARRAY_SIZE(work->rootCapsule.contacts); i++) {
        value = work->rootCapsule.contacts[i].key.value;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

/// Unlinks the four collision spheres and releases the enemy and its task.
///
/// Accepts absent work after a failed spawn; otherwise clears the enemy's borrowed
/// contact-table pointer before release. This callback omits both capsule unlinks
/// before their work storage is freed.
static void _actor403000Exit(Task* task)
{
    Actor403000Work* work  = task->work;
    Enemy*           enemy = task->spawnArg2.pointer;

    if (work != NULL) {
        worldCollisionUnlinkBody(&work->torsoSphere.body);
        worldCollisionUnlinkBody(&work->hindSphere.body);
        worldCollisionUnlinkBody(&work->neckSphere.body);
        worldCollisionUnlinkBody(&work->rootSphere.body);
        // Task teardown frees the capsule storage without unlinking those bodies here.
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}

static void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_403000_80158CE0[(s16)arg1].vx;
    arg0->vy = D_actor_403000_80158CE0[(s16)arg1].vy;
    arg0->vz = D_actor_403000_80158CE0[(s16)arg1].vz;
}

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
static void func_actor_403000_8013D59C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_403000_80132004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Hides the actor and clears its health when entering the hidden state.
///
/// Requires live actor work/model/enemy. Entry suspends automatic lock-on,
/// hides active model drawing and disables root-sphere grid testing. Later
/// hidden ticks perform no animation update or other work.
static void _actor403000HiddenState(Task* actorTask)
{
    TmdObject*       model;
    Actor403000Work* work;
    Enemy*           enemy;

    work = actorTask->work;
    if (work->stateEntered != 0) {
        model                        = actorTask->extra.tmd;
        enemy                        = actorTask->spawnArg2.pointer;
        work->lockOnSuspended        = 1;
        model->flags                |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->rootSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                    = 0;
    }
}

/// Plays a short downed-hit jolt, then chooses stunned, down or dissolve.
///
/// Requires live actor work/model/enemy. Entry restarts the jolt at three
/// times normal animation rate and enables the root grid probe. The result
/// is selected on a jump/settle flag or at counter 5. A lethal result leaves
/// the dissolve state's room report pending.
static void _actor403000DownHitState(Task* actorTask)
{
    enum {
        ACTOR_403000_CLIP_DOWN_HIT      = 15,
        ACTOR_403000_DOWN_HIT_MAX_TICKS = 5,
    };

    TmdObject*       model;
    Actor403000Work* work;
    Enemy*           enemy;

    work  = actorTask->work;
    enemy = actorTask->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                        = actorTask->extra.tmd;
        work->lockOnSuspended        = 0;
        model->flags                 = 0;
        work->animRate               = (3 * ANIMATION_RATE_ONE);
        work->requestedAnimId        = ACTOR_403000_CLIP_DOWN_HIT;
        work->animStart              = ACTOR_403000_ANIM_RESTART;
        work->stateFrame             = 0;
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    work->stateFrame++;
    _actor403000UpdateAnimation(actorTask);
    if ((work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) || work->stateFrame >= ACTOR_403000_DOWN_HIT_MAX_TICKS) {
        _actor403000FinishDownReaction(work, enemy);
    }
}

static void func_actor_403000_8013D72C(Task* arg0)
{
    Enemy*           enemy;
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        enemy                 = arg0->spawnArg2.pointer;
        work->lockOnSuspended = 1;
        worldTargetDisableNodeLockOn(&enemy->node);
        obj->flags = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate               = 0x10;
        work->animStart              = ACTOR_403000_ANIM_RESTART;
        work->forelegYawTarget       = 0;
        work->neckYawTarget          = 0;
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(arg0);
        return;
    }
    _actor403000UpdateAnimation(arg0);
    if (work->lastCommandId == 0xA) {
        work->color.t[2]    = 0;
        work->color.t[1]    = 0;
        work->color.t[0]    = 0;
        work->color.m[2][2] = 0;
        work->color.m[2][1] = 0;
        work->color.m[2][0] = 0;
        work->color.m[1][2] = 0;
        work->color.m[1][1] = 0;
        work->color.m[1][0] = 0;
        work->color.m[0][2] = 0;
        work->color.m[0][1] = 0;
        work->color.m[0][0] = 0;
    }
    if (work->lastCommandId == 4 && work->requestedAnimId == 0x1B && (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->requestedAnimId = 0x1D;
        work->animStart       = ACTOR_403000_ANIM_RESTART;
    }
}

static void func_actor_403000_8013D850(Task* arg0)
{
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 0x10;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor403000UpdateAnimation(arg0);
        work->stateFrame = 0;
    }
    work->stateFrame++;
    _actor403000UpdateAnimation(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->watchRingDir  = 1;
        work->patrolRingDir = 1;
        work->state         = ACTOR_403000_STATE_PATROL;
    }
}

static void func_actor_403000_8013D910(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    u32              rng;
    s16              timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        rng              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = rng;
        work->stateFrame = ((rng >> 16) & 0xF) + 0xA;
    }
    timer            = work->stateFrame - 1;
    work->stateFrame = timer;
    if (timer < 0 && enemy->hp > 0) {
        work->state = ACTOR_403000_STATE_GET_UP;
    }
}

static s32 func_actor_403000_8013D98C(s32 arg0)
{
    switch (arg0 & 0xFF) {
        case 0:
        case 1:
        case 2:
            return 5;
        case 3:
        case 4:
            return 9;
        case 5:
        case 6:
        case 7:
            return 0;
        case 8:
        case 9:
            return 4;
        default:
            return -1;
    }
}
