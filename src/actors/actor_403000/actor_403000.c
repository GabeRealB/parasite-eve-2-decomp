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

/// Waypoint grid for `func_actor_403000_80134204`: two rows of five indices
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

/// Pairs of hit-effect vectors `func_actor_403000_80134910` picks from by
/// turn magnitude; `pad` indexes the display object's coordinate parts.
extern SVECTOR D_actor_403000_80158C48[];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id -- `func_actor_403000_8013ACBC` scales a byte id by 8 into it
/// the same way -- so a record's `vx`/`vy`/`vz` are the vector an actor's
/// handlers copy out of it. Lives in the overlay's trailing data region.
extern SVECTOR D_actor_403000_80158CE0[];

/// Four trigger points (`vx`/`vz` used) `func_actor_403000_80134E00` measures
/// the display object against, one per bit of `gameFlagGetNibble(0xE2)`.
extern SVECTOR D_actor_403000_80158D64[];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Turn joint `coord` by `yaw` about Y in view space, keeping it expressed in
/// its parent's frame.

/// Tick the work block's animation: start the requested clip on the main rig
/// when `animStart` asks (blending in, or from its first frame), start the
/// overlay clip when `overlayStart` asks, then tick every slot at `animRate`,
/// mixing the overlay in while `overlayActive` is set. After the clips come
/// the procedural joint rotations and the clip's sound cues.
static void func_actor_403000_80133AF8(Task* arg0);

static s32 func_actor_403000_80134204(GfxCoord* coord);

/// Step `coord` by the movement the first `count` records of `recs` resolve
/// to; returns whether the actor moved on X or Z.

/// Copy `placement` onto the actor's root coordinate (Y then X then Z) and
/// cache the resulting heading in `Actor403000Work::placementYaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

/// Report whether the root capsule's contact table holds a live entry:
/// the walk stops at the first empty `key` and answers 1 if any record it
/// passed carried the 0x10 kind bits.
static s16 func_actor_403000_8013D48C(Task* task);

/// `Task::exitCallback` installed by the spawn handler, for the teardown path
/// where the enemy was created: hand the four collision spheres' bodies back to
/// `worldCollisionUnlinkBody`, drop the enemy's `recs` slot, then let `enemyDestroy`
/// free the enemy and the task.
static void func_actor_403000_8013D4F4(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
static void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

static void func_actor_403000_8013D5F8(Task* arg0);

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

/// Integer part of the last movement step `ActorContact_PushContact`
/// applied to the actor's root coordinate.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_actor_403000_80132AE0(GfxCoord* coord);
static void func_actor_403000_80134F44(Task* arg0);
static void func_actor_403000_8013D72C(Task* arg0);
static void func_actor_403000_8013D648(Task* arg0);
static void func_actor_403000_801377C8(Task* arg0);
static void func_actor_403000_8013B74C(Task* arg0);

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
s32                 func_actor_403000_801324EC(Task*, s32, ActorCommand*, s32);
s32                 func_actor_403000_8013D268(Task*, s32, s32, s32);
s32                 func_actor_403000_8013D324(Task*, s32, s32, s32);
s32                 func_actor_403000_8013D364(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32                 func_actor_403000_8013D464(Task*, s32, AnimationPlayRequest*, s32);
static void         func_actor_403000_8013D59C(Task*);
s32                 func_actor_403000_8013D260(Task*, s32, s32, s32);

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
    { 2015, func_actor_403000_8013D260 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_403000_8013D268 },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_403000_8013D324 },
    { ACTOR_MESSAGE_PLACE, func_actor_403000_8013D364 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_403000_801324EC },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_403000_8013D464 },
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
static void                func_actor_403000_801332E8(Task* arg0);
static void                func_actor_403000_80133444(Task* arg0);
static void                func_actor_403000_801336B4(Task* arg0);
static s32                 func_actor_403000_801337E0(Task* arg0, Actor403000Work* work);
static s32                 func_actor_403000_80133FC0(Task* arg0, s16 arg1, s16 arg2);
static void                func_actor_403000_801343B8(Enemy* arg0, Task* arg1);
static void                func_actor_403000_80134910(Task* arg0, s16 arg1, s32 arg2);
static s32                 func_actor_403000_80134E00(Task* arg0);
static inline s32          func_actor_403000_FindHit(SVECTOR* pos, WorldCollisionContact* records);
static inline s16          func_actor_403000_WrapAngle(s16 angle);
static inline void         func_actor_403000_PlaySound(Task* arg0, Enemy* enemy, s32 id);
static void                func_actor_403000_80135F08(Task* arg0);
static __inline__ void     Actor403000_FaceScale(GfxCoord* coord, s16 sy);
static void                func_actor_403000_8013603C(Task* arg0);
static void                func_actor_403000_801365D0(Task* arg0);
static void                func_actor_403000_80136B14(Task* arg0);
static void                func_actor_403000_80136D68(Task* arg0);
static __inline__ void     Actor403000_ScaleVec(SVECTOR* v, u16 k);
static __inline__ SVECTOR* Actor403000_PushVec(void);
static __inline__ void     Actor403000_PopVec(void);
static inline s8           Actor403000_Cell(GfxCoord* coord);
static void                func_actor_403000_80137084(Task* arg0);
static void                func_actor_403000_801384E8(Task* arg0);
static void                func_actor_403000_801386E8(Task* arg0);
static void                func_actor_403000_80138DB0(Task* arg0);
static void                func_actor_403000_801399A0(Task* arg0);
static void                func_actor_403000_80139AE0(Task* arg0);
static void                func_actor_403000_8013A08C(Task* arg0);
static void                func_actor_403000_8013A678(Task* arg0);
static void                func_actor_403000_8013ACBC(Task* arg0);
static void                func_actor_403000_8013B238(Task* arg0);
static __inline__ s32      Actor403000_Outside(SVECTOR* v, s32 r);
static void                func_actor_403000_8013BDE0(Task* arg0);
static void                func_actor_403000_8013C050(Task* arg0);
static void                func_actor_403000_8013C2D4(Task* arg0);
static void                func_actor_403000_8013C864(Enemy* arg0, Task* arg1);
static s32                 func_actor_403000_8013D98C(s32 arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

s32 func_actor_403000_801324EC(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3)
{
    Actor403000Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;

    work->lastCommandStage = arg2->context.loc.stage;
    work->lastCommandArea  = arg2->context.loc.area;
    work->lastCommandId    = arg2->command;
    if (arg2->context.key == 0x204) {
        switch (arg2->command) {
            case 0:
                enemy->hp   = 0;
                work->state = ACTOR_403000_STATE_HIDDEN;
                return 1;
            case 1:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->param          = &D_actor_403000_8013DA00;
                enemy->hp             = D_actor_403000_8013DA00.hpMax;
                work->requestedAnimId = 0x18;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                return 1;
            case 2:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->param          = &D_actor_403000_8013DA00;
                enemy->hp             = D_actor_403000_8013DA00.hpMax;
                work->requestedAnimId = 0x19;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                return 1;
            case 3:
                enemy->hp = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = 0x1A;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                enemy->reactionFlags  = 0;
                enemy->hp             = D_actor_403000_8013DA10.hpMax;
                enemy->param          = &D_actor_403000_8013DA10;
                return 1;
            case 4:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = 0x1B;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                return 1;
            case 5:
                work->state                        = ACTOR_403000_STATE_SCRIPTED_BURN;
                work->prevState                    = -1;
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                return 1;
            case 6:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                enemy->hp             = D_actor_403000_8013DA10.hpMax;
                enemy->param          = &D_actor_403000_8013DA10;
                work->requestedAnimId = 0x19;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                return 1;
            case 7:
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                work->state                        = ACTOR_403000_STATE_DISSOLVE;
                work->prevState                    = -1;
                return 1;
            case 10:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags  = 0;
                work->requestedAnimId = 0x18;
                work->state           = ACTOR_403000_STATE_SCRIPTED;
                work->prevState       = -1;
                return 1;
            case 11:
                work->state         = ACTOR_403000_STATE_TURN;
                work->prevState     = -1;
                work->watchRingDir  = -1;
                work->patrolRingDir = 1;
                work->turnRingDir   = 1;
                if (arg0->extra.tmd->texturePageOffset == 2) {
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_403000_8013DA10.hpMax;
                    enemy->param         = &D_actor_403000_8013DA10;
                }
                return 1;
            case 12:
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                work->state                        = ACTOR_403000_STATE_SCRIPTED_DISSOLVE;
                work->prevState                    = -1;
                return 1;
            case 13:
                work->state                        = ACTOR_403000_STATE_SCRIPTED_SMOLDER;
                work->prevState                    = -1;
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
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
    MATRIX*                   m;
    GfxCoord*                 walker;
    SVECTOR*                  pos;
    s16                       i;
    POLY_FT4*                 prim;
    SVECTOR*                  n;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    m                                          = &scratch->emitter.coord;
    MATRIX_PAIR(&scratch->emitter.coord, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2)                       = 0;
    MATRIX_PAIR(m, 1, 1)                       = 0x1000;
    MATRIX_PAIR(m, 2, 0)                       = 0;
    m->m[2][2]                                 = 0x1000;
    scratch->emitter.coord.t[0]                = -0x3C;
    scratch->emitter.coord.t[1]                = -0x28;
    scratch->emitter.coord.t[2]                = 0x12C;
    scratch->emitter.parent                    = parent;
    scratch->emitter.composeStamp              = GRAPHICS_COORD_DIRTY;
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
    MATRIX*                   m;
    GfxCoord*                 walker;
    SVECTOR*                  pos;
    s16                       i;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    /* Identity, written as three words and a short through a second pointer. */
    m                                          = &scratch->emitter.coord;
    MATRIX_PAIR(&scratch->emitter.coord, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2)                       = 0;
    MATRIX_PAIR(m, 1, 1)                       = 0x1000;
    MATRIX_PAIR(m, 2, 0)                       = 0;
    m->m[2][2]                                 = 0x1000;
    scratch->emitter.coord.t[0]                = -0x3C;
    scratch->emitter.coord.t[1]                = -0x28;
    scratch->emitter.parent                    = parent;
    scratch->emitter.coord.t[2]                = 0x12C;
    scratch->emitter.composeStamp              = GRAPHICS_COORD_DIRTY;
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

static void func_actor_403000_801332E8(Task* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;

    work   = arg0->work;
    orig   = work->torsoSwayTarget;
    target = orig;
    if (orig > 700) {
        target = 700;
    }
    if (orig < -700) {
        target = -700;
    }
    if (work->torsoSway < target) {
        if (target - work->torsoSway > 64) {
            work->torsoSway += 64;
        } else {
            work->torsoSway = target;
        }
    }
    if (target < work->torsoSway) {
        diff = work->torsoSway - target;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff > 64) {
            work->torsoSway -= 64;
        } else {
            work->torsoSway = target;
        }
    }
    gfxRotMatrixZ(&arg0->extra.tmd->coords[22].coord, work->torsoSway / 2, GRAPHICS_ROTATION_REPLACE);
    arg0->extra.tmd->coords[22].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&arg0->extra.tmd->coords[23].coord, work->torsoSway * 3 / 4, GRAPHICS_ROTATION_REPLACE);
    arg0->extra.tmd->coords[23].composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_80133444(Task* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;
    s32              delta;

    work = arg0->work;
    if (work->neckYaw < work->prevNeckYaw && (diff = abs(work->neckYaw - work->prevNeckYaw)) >= 8 && work->headSwayTarget > -0x280) {
        if (diff >= 24) {
            if (work->headSwayTarget > 0) {
                work->headSwayTarget = -32;
            } else {
                work->headSwayTarget -= 32;
            }
        } else {
            if (work->headSwayTarget > 0) {
                work->headSwayTarget = -2;
            } else {
                work->headSwayTarget -= 2;
            }
        }
    } else if (work->neckYaw > work->prevNeckYaw && (diff = abs(work->neckYaw - work->prevNeckYaw)) >= 8 && work->headSwayTarget < 0x280) {
        if (diff >= 24) {
            if (work->headSwayTarget < 0) {
                work->headSwayTarget = 32;
            } else {
                work->headSwayTarget += 32;
            }
        } else {
            if (work->headSwayTarget < 0) {
                work->headSwayTarget = 2;
            } else {
                work->headSwayTarget += 2;
            }
        }
    } else {
        work->headSwayTarget = 0;
    }
    target            = work->headSwayTarget;
    work->prevNeckYaw = work->neckYaw;
    orig              = target;
    if (orig > 640) {
        target = 640;
    }
    if (orig < -640) {
        target = -640;
    }
    if (work->headSway < target) {
        if (target - work->headSway > 48) {
            work->headSway += 48;
        } else {
            work->headSway = target;
        }
    }
    if (target < work->headSway) {
        delta = work->headSway - target;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta > 48) {
            work->headSway -= 48;
        } else {
            work->headSway = target;
        }
    }
    gfxRotMatrixZ(&arg0->extra.tmd->coords[6].coord, work->headSway / 2, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&arg0->extra.tmd->coords[7].coord, work->headSway * 3 / 4, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_801336B4(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    Actor403000Work*  work;

    work   = arg0->work;
    weight = work->overlayWeight;
    anim   = &work->anim;
    for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = work->overlayRate;
            work->slots[i].rate      = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blendAnim, i, &blendPose, 0);
            animationApplyPoseWithBlendedRotation(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->anim, i);
        }
    }
}

static s32 func_actor_403000_801337E0(Task* arg0, Actor403000Work* work)
{
    s32 ret;

    ret = 0;
    if (work->slotCueIndex[1] == (work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        return ret;
    }
    switch ((s16)(work->requestedAnimId - 1)) {
        case 0:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x21 && work->slotCueIndex[1] < 0x21) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x2C && work->slotCueIndex[1] < 0x2C) {
                ret = 0x401E0001;
            }
            break;
        case 8:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 5 && work->slotCueIndex[1] < 5) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xA && work->slotCueIndex[1] < 0xA) {
                ret = 0x401E0001;
            }
            break;
        case 13:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->slotCueIndex[1] < 0xF) {
                ret = 0x401E000E;
            }
            break;
        case 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x1D && work->slotCueIndex[1] < 0x1D) {
                ret = 0x401E0003;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x17 && work->slotCueIndex[1] < 0x17) {
                ret = 0x401E0004;
            }
            break;
        case 11:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x13 && work->slotCueIndex[1] < 0x13) {
                ret = 0x401E0008;
            }
        case 10:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x12 && work->slotCueIndex[1] < 0x12) {
                ret = 0x401E0007;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->slotCueIndex[1] < 0xF) {
                ret = 0x401E000C;
            }
            break;
        case 6:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x15 && work->slotCueIndex[1] < 0x15) {
                ret = 0x401E0009;
            }
            break;
        case 16:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 8 && work->slotCueIndex[1] < 8) {
                ret = 0x401E000B;
            }
            break;
        case 7:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xD && work->slotCueIndex[1] < 0xD) {
                ret = 0x401E000D;
            }
            break;
    }
    work->slotCueIndex[1] = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    return ret;
}

static void func_actor_403000_80133AF8(Task* arg0)
{
    Actor403000Work* seekWork;
    Actor403000Work* resetWork;
    Actor403000Work* turnWork;
    Actor403000Work* secondaryWork;
    Actor403000Work* tickWork;
    Actor403000Work* work;
    s32              animation;
    s32              updatedTurn;
    s16              currentTurn;
    s16              thirdAngle;
    s16              state;
    s32              currentAngle;
    s32              targetAngle;
    s16              angle;
    s32              seekSlotIndex;
    s32              resetSlotIndex;
    s32              secondarySlotIndex;
    s32              tickSlotIndex;
    s32              signedTurn;
    s32              sound;
    s32              resetIndex;
    s32              secondaryIndex;
    s32              tickIndex;
    s32              seekIndex;
    s32              delta;
    AnimationSlot*   tickSlot;
    AnimationSlot*   resetSlot;
    AnimationSlot*   secondarySlot;
    s32              pan;
    s32              currentAngleBits;
    u16              originalTurn;
    s32              targetAngleBits;
    u16              updatedTurnBits;
    s16              clampedAngle;
    s32              targetTurn;

    work  = arg0->work;
    state = work->animStart;
    if (state == ACTOR_403000_ANIM_BLEND_IN) {
        if (work->animId != work->requestedAnimId) {
            seekWork  = work;
            seekIndex = 1;
            do {
                seekSlotIndex               = seekIndex;
                work->slots[seekIndex].rate = seekWork->animRate;
                animation                   = seekWork->requestedAnimId;
                animationSeekSlotWithBlend(&seekWork->anim, seekSlotIndex, (s16)(animation), 0, D_actor_403000_80158364[seekWork->animId][animation]);
                seekIndex += 1;
            } while (seekIndex < ARRAY_SIZE(work->slots));
            seekWork->animId = seekWork->requestedAnimId;
        }
        work->animStart  = ACTOR_403000_ANIM_PLAYING;
        work->animFrames = 0;
        memFillBytes(work->slotCueIndex, 0U, sizeof(work->slotCueIndex));
    } else if (state == ACTOR_403000_ANIM_RESTART) {
        resetWork  = work;
        resetIndex = 1;
        resetSlot  = work->slots;
        do {
            resetSlotIndex    = resetIndex;
            resetSlot[1].rate = resetWork->animRate;
            resetSlot        += 1;
            animationResetSlot(&resetWork->anim, resetSlotIndex, resetWork->requestedAnimId);
            resetIndex += 1;
        } while (resetIndex < ARRAY_SIZE(work->slots));
        resetWork->animId = resetWork->requestedAnimId;
        work->animStart   = ACTOR_403000_ANIM_PLAYING;
        work->animFrames  = 0U;
        memFillBytes(work->slotCueIndex, 0U, sizeof(work->slotCueIndex));
    }
    if (work->overlayStart == ACTOR_403000_ANIM_RESTART) {
        secondaryWork                = arg0->work;
        secondaryIndex               = 1;
        secondarySlot                = secondaryWork->slots;
        secondaryWork->overlayRate   = 0x20;
        secondaryWork->overlayWeight = 0x800;
        do {
            secondarySlotIndex    = secondaryIndex;
            secondarySlot[1].rate = secondaryWork->overlayRate;
            secondarySlot        += 1;
            animationResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, secondaryWork->overlayAnimId);
            secondaryIndex += 1;
        } while (secondaryIndex < ARRAY_SIZE(work->slots));
        work->overlayStart = ACTOR_403000_ANIM_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->overlayActive == 0) {
        tickWork  = arg0->work;
        tickIndex = 1;
        tickSlot  = tickWork->slots;
        do {
            tickSlotIndex    = tickIndex;
            tickSlot[1].rate = tickWork->animRate;
            animationTickSlot(&tickWork->anim, tickSlotIndex);
            tickSlot  += 1;
            tickIndex += 1;
        } while (tickIndex < ARRAY_SIZE(work->slots));
    } else {
        func_actor_403000_801336B4(arg0);
        if (work->blendSlots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->overlayActive = 0;
        }
    }
    targetAngle      = work->neckYawTarget;
    currentAngle     = work->neckYaw;
    targetAngleBits  = (u16)work->neckYawTarget;
    currentAngleBits = (u16)work->neckYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->neckYaw = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->neckYaw = currentAngleBits - 0x71;
    } else {
    block_26:
        work->neckYaw = targetAngleBits;
    }
    if (work->neckYawEnabled == 1) {
        angle        = work->neckYaw;
        clampedAngle = angle;
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[2], thirdAngle);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[3], thirdAngle);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[4], (s16)clampedAngle / 2);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->headSwayEnabled == 1) {
        func_actor_403000_80133444(arg0);
    }
    if (work->forelegYawEnabled == 1) {
        turnWork     = arg0->work;
        targetTurn   = (u16)turnWork->forelegYawTarget;
        originalTurn = targetTurn;
        if ((s16)targetTurn >= 0x201) {
            targetTurn = 0x200;
        }
        if ((s16)originalTurn < -0x200) {
            targetTurn = -0x200;
        }
        signedTurn  = (s16)targetTurn;
        currentTurn = turnWork->forelegYaw;
        if (currentTurn < signedTurn) {
            if ((signedTurn - currentTurn) >= 0xD) {
                turnWork->forelegYaw = (s16)((u16)turnWork->forelegYaw + 0xC);
            } else {
                turnWork->forelegYaw = (s16)targetTurn;
            }
        }
        updatedTurn     = turnWork->forelegYaw;
        updatedTurnBits = (u16)turnWork->forelegYaw;
        if ((s16)targetTurn < updatedTurn) {
            delta = updatedTurn - (s16)targetTurn;
            if (delta < 0) {
                delta = -delta;
            }
            if (delta >= 0xD) {
                turnWork->forelegYaw = (s16)(updatedTurnBits - 0xC);
            } else {
                turnWork->forelegYaw = (s16)targetTurn;
            }
        }
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->forelegYaw * -1));
        arg0->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->torsoSwayEnabled == 1) {
        func_actor_403000_801332E8(arg0);
    }
    sound = func_actor_403000_801337E0(arg0, work);
    if (sound != 0) {
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static s32 func_actor_403000_80133FC0(Task* arg0, s16 arg1, s16 arg2)
{
    GfxCoord*         coord;
    ActorTurnScratch* scratch;
    s16               angle;
    s32               mag;
    GfxCoord*         coord2;

    if (arg1 == arg2) {
        return 1;
    }
    switch (arg1) {
        case 0:
            if (arg2 == 9) {
                goto calc;
            }
            if (arg2 < 4) {
                goto calc;
            }
            return 0;
        case 1:
        case 2:
        case 3:
            if (arg2 < 5) {
                goto calc;
            }
            return 0;
        case 4:
            if (arg2 >= 6) {
                return 0;
            }
            if (arg2 != 0) {
                goto calc;
            }
            return 0;
        case 5:
            if (arg2 < 4) {
                return 0;
            }
            if (arg2 != 9) {
                goto calc;
            }
            return 0;
        case 6:
        case 7:
        case 8:
            if (arg2 >= 5) {
                goto calc;
            }
            return 0;
        case 9:
        default:
            if (arg2 >= 6) {
                goto calc;
            }
            if (arg2 != 0) {
                return 0;
            }
            break;
    }
calc:
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    coord             = arg0->extra.tmd->coords;
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    coord2            = arg0->extra.tmd->coords;
    angle             = ratan2(scratch->delta.vx, scratch->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    scratch->angle = mag = angle;
    if ((mag < 0 ? -mag : mag) < 0x200) {
        SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    return 0;
}

static s32 func_actor_403000_80134204(GfxCoord* arg0)
{
    GfxCoord*                   coord;
    _Actor403000RingDirScratch* scratch;
    SVECTOR*                    table;
    SVECTOR*                    v;
    s32                         x;
    s32                         z;
    s8                          col;
    s8                          row;
    s16                         angle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000RingDirScratch);
    coord   = arg0;
    x       = coord->coord.t[0];
    z       = coord->coord.t[2];
    col     = 4;
    if (x >= 0xD48) {
        col = 3;
        if (x >= 0x1A90) {
            col = 2;
            if (x >= 0x2AF8) {
                col = x < 0x3C8C;
            }
        }
    }
    row               = z >= 0x1068;
    scratch->waypoint = D_actor_403000_80158D48[col + row * 5] + 1;
    if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
        scratch->waypoint = 0;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->waypoint];
    scratch->offset.vx  = v->vx;
    scratch->offset.vy  = v->vy;
    scratch->offset.vz  = v->vz;
    scratch->offset.vx -= coord->coord.t[0];
    scratch->offset.vz -= coord->coord.t[2];
    angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    if (angle < 0x400) {
        scratch->ringDir = 1;
    } else {
        scratch->ringDir = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000RingDirScratch);
    return scratch->ringDir;
}

static void func_actor_403000_801343B8(Enemy* arg0, Task* arg1)
{
    SVECTOR                dir;
    VECTOR                 pos;
    Actor403000Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    WorldCollisionBody*    torsoBody;
    WorldCollisionBody*    hindBody;
    WorldCollisionBody*    neckBody;
    AnimationSet**         animSrc;
    WorldCollisionContact* rootContacts;
    WorldCollisionContact* headContacts;
    SVECTOR*               dirp;
    WorldCollisionContact* torsoContacts;
    Actor403000Work*       taskWork;
    TmdObject*             tmd;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    arg1->work = (work = memCalloc(sizeof(Actor403000Work), false));
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->exitCallback = func_actor_403000_8013D4F4;
    taskWork           = arg1->work;
    tmd                = arg1->extra.tmd;
    tmd->lightMtx      = &taskWork->light;
    tmd->colorMtx      = &taskWork->color;
    arg0->field_4      = &arg1->extra.tmd->coords->coord;
    arg0->field_48     = 0;
    arg0->bodyPos.vx   = 0;
    arg0->bodyPos.vy   = 0;
    arg0->bodyPos.vz   = 0;
    arg0->coord        = &arg1->extra.tmd->coords[2];
    worldTargetLinkNode(&arg0->node);
    animSrc               = D_actor_403000_80158B50;
    work->lockOnSuspended = 1;
    arg0->reactionFlags   = 0;
    arg0->hp              = D_actor_403000_8013DA00.hpMax;
    arg0->param           = &D_actor_403000_8013DA00;
    arg0->recs            = (torsoContacts = work->torsoSphere.contacts);
    animationInitContext(&work->anim, animSrc, obj,
                         work->poses, work->slots);
    animationInitContext(&work->blendAnim, animSrc, obj,
                         work->blendPoses, work->blendSlots);
    work->animStart         = ACTOR_403000_ANIM_RESTART;
    work->overlayActive     = 0;
    work->requestedAnimId   = 0;
    work->neckYaw           = 0;
    work->neckYawTarget     = 0;
    work->field_ACC         = 0x10;
    work->animRate          = 0x10;
    work->torsoSwayEnabled  = 1;
    work->forelegYawEnabled = 1;
    work->headSwayEnabled   = 1;
    work->neckYawEnabled    = 1;
    func_actor_403000_80133AF8(arg1);
    work->rootSphere.body.context.contacts = work->rootSphere.contacts;
    work->rootSphere.body.coord            = coord;
    work->rootSphere.body.pos.vx           = 0;
    work->rootSphere.body.pos.vy           = -0x11C;
    work->rootSphere.body.pos.vz           = 0;
    work->rootSphere.body.key              = 0x30001;
    work->rootSphere.body.radius           = 0x12C;
    work->rootSphere.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootSphere.body);
    work->rootCapsule.shape.ends[0].vy     = -0x180;
    work->rootCapsule.shape.ends[1].vy     = -0x180;
    work->rootCapsule.shape.ends[1].vz     = 0x2BC;
    work->rootCapsule.body.context.capsule = &work->rootCapsule.shape;
    work->rootCapsule.body.key             = 0x30001;
    work->rootCapsule.shape.ends[0].vx     = 0;
    work->rootCapsule.shape.ends[0].vz     = 0;
    work->rootCapsule.shape.ends[1].vx     = 0;
    work->rootCapsule.shape.end0Radius     = 0x12C;
    work->rootCapsule.shape.end1Radius     = 0x12C;
    work->rootCapsule.shape.contacts = rootContacts = work->rootCapsule.contacts;
    work->rootCapsule.body.coord                    = coord;
    work->rootCapsule.body.pos.vx                   = 0;
    work->rootCapsule.body.pos.vy                   = 0;
    work->rootCapsule.body.pos.vz                   = 0;
    work->rootCapsule.body.radius                   = 0;
    work->rootCapsule.body.flags                    = WORLD_COLLISION_BODY_CAPSULE;
    work->rootSphere.body.flags                    |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootCapsule.body);
    work->headCapsule.shape.ends[0].vz = -0x3E8;
    work->headCapsule.shape.ends[1].vz = 0x190;
    work->headCapsule.shape.end0Radius = 0x200;
    work->headCapsule.shape.end1Radius = 0x200;
    work->headCapsule.shape.ends[0].vx = 0;
    work->headCapsule.shape.ends[0].vy = 0;
    work->headCapsule.shape.ends[1].vx = 0;
    work->headCapsule.shape.ends[1].vy = 0;
    work->headCapsule.shape.contacts = headContacts = work->headCapsule.contacts;
    work->rootCapsule.body.flags                   |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->headCapsule.body.coord                    = &arg1->extra.tmd->coords[5];
    work->headCapsule.body.context.capsule          = &work->headCapsule.shape;
    work->headCapsule.body.pos.vx                   = 0;
    work->headCapsule.body.pos.vy                   = 0;
    work->headCapsule.body.pos.vz                   = 0;
    work->headCapsule.body.key                      = 0x3001E;
    work->headCapsule.body.radius                   = 0;
    work->headCapsule.body.flags                    = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headCapsule.body);
    work->headCapsule.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(rootContacts, ARRAY_SIZE(work->rootCapsule.contacts), 0);
    worldCollisionInitContacts(headContacts, ARRAY_SIZE(work->headCapsule.contacts), 0);
    worldCollisionInitContacts(work->rootSphere.body.context.contacts, ARRAY_SIZE(work->rootSphere.contacts), 0);
    torsoBody                   = &work->torsoSphere.body;
    torsoBody->coord            = &arg1->extra.tmd->coords[1];
    torsoBody->context.contacts = torsoContacts;
    torsoBody->pos.vx           = 0;
    torsoBody->pos.vy           = 0;
    torsoBody->pos.vz           = 0;
    torsoBody->key              = 0x3001E;
    torsoBody->radius           = 0x3E8;
    torsoBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->torsoSphere.body);
    torsoBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(torsoBody->context.contacts, ARRAY_SIZE(work->torsoSphere.contacts), 0);
    hindBody                   = &work->hindSphere.body;
    hindBody->coord            = &arg1->extra.tmd->coords[15];
    hindBody->context.contacts = work->hindSphere.contacts;
    hindBody->pos.vx           = 0;
    hindBody->pos.vy           = 0;
    hindBody->pos.vz           = 0;
    hindBody->key              = 0x3001E;
    hindBody->radius           = 0x320;
    hindBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hindSphere.body);
    hindBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hindBody->context.contacts, ARRAY_SIZE(work->hindSphere.contacts), 0);
    neckBody                   = &work->neckSphere.body;
    neckBody->coord            = &arg1->extra.tmd->coords[4];
    neckBody->context.contacts = work->neckSphere.contacts;
    neckBody->pos.vx           = 0;
    neckBody->pos.vy           = 0;
    neckBody->pos.vz           = 0;
    neckBody->key              = 0x3001E;
    neckBody->radius           = 0x320;
    neckBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->neckSphere.body);
    neckBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(neckBody->context.contacts, ARRAY_SIZE(work->neckSphere.contacts), 0);
    work->hindSphere.body.pos.vx = 0;
    work->hindSphere.body.pos.vy = 0;
    work->hindSphere.body.pos.vz = -0x100;
    work->playerCaught           = 0;
    work->seenTargetsDestroyed   = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    dirp   = &dir;
    VectorNormalSS(dirp, dirp);
    gte_lddp(0x1388);
    gte_ldsv(dirp);
    gte_gpf12();
    gte_stsv(dirp);
    work->playerAnimation.source.sets          = D_actor_403000_80158C08;
    work->playerAnimation.animationId          = 1;
    work->playerAnimation.blendFrames          = 3;
    work->playerAnimation.blend                = 0;
    work->playerAnimation.enableWorldCollision = 1;
    work->roomEventPending                     = 0;
    arg1->msgTable                             = D_actor_403000_80158CA8;
    coord->parent                              = &gGfxViewCoord;
    coord->composeStamp                        = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->patrolRingDir = -1;
    work->watchRingDir  = -1;
    work->state         = ACTOR_403000_STATE_APPROACH;
    arg1->state++;
}

static void func_actor_403000_80134910(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         scratch;
    Actor403000Work* work;
    EffectSpawnArg*  eff;
    s32              mag;

    scratch = (SCRATCH_STACK_CURSOR(SVECTOR) -= 2);
    mag     = (arg1 >= 0) ? arg1 : -arg1;
    work    = arg0->work;
    if (mag < 0x200) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[0];
                scratch[1] = D_actor_403000_80158C48[3];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[1];
                scratch[1] = D_actor_403000_80158C48[2];
                break;
            case 2:
                scratch[0] = D_actor_403000_80158C48[2];
                scratch[1] = D_actor_403000_80158C48[0];
                break;
            case 3:
                scratch[0] = D_actor_403000_80158C48[3];
                scratch[1] = D_actor_403000_80158C48[1];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[4];
                break;
        }
    } else if (mag > 0x600) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 2) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[5];
                scratch[1] = D_actor_403000_80158C48[6];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[6];
                scratch[1] = D_actor_403000_80158C48[7];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[7];
                scratch[1] = D_actor_403000_80158C48[5];
                break;
        }
    } else if (arg1 > 0) {
        scratch[0] = D_actor_403000_80158C48[8];
        scratch[1] = D_actor_403000_80158C48[9];
    } else {
        scratch[0] = D_actor_403000_80158C48[10];
        scratch[1] = D_actor_403000_80158C48[11];
    }
    work->hitEffectArg.coord      = &arg0->extra.tmd->coords[scratch[0].pad];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 3;
    eff                           = &work->hitEffectArg;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->extra.tmd->coords[scratch[0].pad], &scratch[0], eff);
    work->hitEffectArg.coord      = &arg0->extra.tmd->coords[scratch[1].pad];
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = 2;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->extra.tmd->coords[scratch[1].pad], &scratch[1], eff);
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}

static s32 func_actor_403000_80134E00(Task* arg0)
{
    Actor403000Work* work;
    s16              flags;
    s16              i;
    VECTOR           d;

    work  = arg0->work;
    flags = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    if (flags == work->seenTargetsDestroyed) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (((flags >> i) & 1) && !((work->seenTargetsDestroyed >> i) & 1)) {
            d.vx = arg0->extra.tmd->coords->coord.t[0] - D_actor_403000_80158D64[i].vx;
            d.vz = arg0->extra.tmd->coords->coord.t[2] - D_actor_403000_80158D64[i].vz;
            if (SquareRoot0(d.vx * d.vx + d.vz * d.vz) < 3000) {
                work->seenTargetsDestroyed = flags;
                return 1;
            }
        }
    }
    work->seenTargetsDestroyed = flags;
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
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[15], 0x800001FF, NULL);
                    break;
                case 8:
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[23], 0x800001FF, NULL);
                    break;
                case 19:
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[11], 0x800001FF, NULL);
                    break;
            }
        }
        if ((s16)func_actor_403000_80134E00(arg0) != 0) {
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
            func_800E2C78(enemy, scratch->hitKey, scratch->damage, 0);
            func_800DA6E8(&enemy->node, scratch->damage, 0);
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
            work->hitCooldown = Gp_GetIdParam2(scratch->hitKey);
            switch (Gp_GetIdParam0(scratch->hitKey) & 0xFFFF) {
                case 0:
                case 5:
                case 6:
                case 7:
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
                case 2:
                    if (work->state != ACTOR_403000_STATE_AMBUSH && !(work->state == ACTOR_403000_STATE_DROP && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->state != ACTOR_403000_STATE_KNOCKDOWN && work->state != ACTOR_403000_STATE_DROP_CATCH && work->playerCaught != 1) {
                        Gp_SetObjFlag2(enemy, scratch->hitKey, 0);
                        if (work->state == ACTOR_403000_STATE_STUNNED || work->state == ACTOR_403000_STATE_DOWN || work->state == ACTOR_403000_STATE_DOWN_HIT || (work->state == ACTOR_403000_STATE_GET_UP && work->stateFrame < 0x12)) {
                            work->state     = ACTOR_403000_STATE_DOWN_HIT;
                            work->prevState = -1;
                            break;
                        }
                        work->state = ACTOR_403000_STATE_KNOCKDOWN;
                    }
                    break;
                case 3:
                    Gp_SetObjFlag4(enemy, scratch->hitKey, 0);
                    break;
                case 1:
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
            scratch->damage         = Gp_ComputeDamage(scratch->hitKey, scratch->playerDistance, 0, 0);
            if (Gp_RollEnemyChance(enemy, scratch->hitKey, 0) != 0) {
                scratch->damage *= 4;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], 0, NULL);
            }
            scratch->hitOffset.vx = scratch->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            scratch->hitOffset.vz = scratch->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            yaw                   = ratan2(scratch->hitOffset.vx, scratch->hitOffset.vz);
            scratch->hitYaw       = yaw - ratan2(-arg0->extra.tmd->coords->workm.m[2][0], arg0->extra.tmd->coords->workm.m[2][2]);
            scratch->hitYaw       = func_actor_403000_WrapAngle(scratch->hitYaw);
            func_actor_403000_80134910(arg0, scratch->hitYaw, scratch->hitKey);
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
            func_800E2C78(enemy, scratch->hitKey, scratch->damage, 0);
            func_800DA6E8(&enemy->node, scratch->damage, 0);
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
            scratch->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (scratch->damage != 0) {
                scratch->damage >>= 2;
                func_800DA6E8(&enemy->node, scratch->damage, 0);
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

static void func_actor_403000_80135F08(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           obj;
    TmdObject*       tmd;
    u32              seed;

    work = arg0->work;
    obj  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        tmd->flags            = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->animRate               = 0x10;
        work->requestedAnimId        = 0xF;
        work->animStart              = ACTOR_403000_ANIM_RESTART;
        work->stateFrame             = 0;
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) {
        seed             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = seed;
        work->stateFrame = (seed >> 0x10) & 0x1F;
    }
    if (work->stateFrame > 0) {
        work->stateFrame--;
        work->animRate = 0;
    } else {
        work->animRate = 0x10;
    }
    if ((Gp_TickObjFlag2(obj) == 1) || (obj->hp <= 0)) {
        obj->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state         = ACTOR_403000_STATE_DOWN;
    }
}

static __inline__ void Actor403000_FaceScale(GfxCoord* coord, s16 sy)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* scratch;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    scratch                                    = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = scratch;
    scratch->yaw                               = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&scratch->rotation, scratch->yaw, 1);
    scratch->scale.vx = 0x1000;
    scratch->scale.vy = sy;
    scratch->scale.vz = 0x1000;
    ScaleMatrix(&scratch->rotation, &scratch->scale);
    coord->coord.m[0][0] = head[-1].rotation.m[0][0];
    coord->coord.m[0][1] = scratch->rotation.m[0][1];
    coord->coord.m[0][2] = scratch->rotation.m[0][2];
    coord->coord.m[1][0] = scratch->rotation.m[1][0];
    coord->coord.m[1][1] = scratch->rotation.m[1][1];
    coord->coord.m[1][2] = scratch->rotation.m[1][2];
    coord->coord.m[2][0] = scratch->rotation.m[2][0];
    coord->coord.m[2][1] = scratch->rotation.m[2][1];
    coord->coord.m[2][2] = scratch->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
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
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
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
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch (work->stateFrame) {
            case 1:
                arg0->extra.tmd->flags = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x76:
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x78:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
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
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000 - (t - 0x64) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000);
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
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
    }
    if (work->stateFrame <= 0x1000) {
        work->stateFrame++;
        if (work->stateFrame % 5 == 0 && work->stateFrame < 100) {
            switch ((s16)((s16)(work->stateFrame / 5) % 4)) {
                case 0:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch (work->stateFrame) {
            case 1:
                arg0->extra.tmd->flags = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x58:
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x5A:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
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
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000 - (t - 0x46) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000);
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
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
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
    func_actor_403000_80133AF8(arg0);
    if (work->stateFrame < 0x28) {
        work->stateFrame++;
    }
    switch (work->stateFrame) {
        case 2:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            break;
        case 5:
            arg0->extra.tmd->coords[12].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[12]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[12], 1, NULL);
            break;
        case 15:
            arg0->extra.tmd->coords[16].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[16]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
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
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
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
    func_actor_403000_80133AF8(arg0);
    if (work->stateFrame < 300) {
        work->stateFrame++;
        switch (work->stateFrame % 24) {
            case 10:
            case 13:
            case 18:
            case 21:
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x01032600, NULL);
                break;
            default:
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[(s16)(work->stateFrame % 24)], 0x01032600, NULL);
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
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            arg0->extra.tmd->otOffset = 0;
            break;
    }
}

static __inline__ void Actor403000_ScaleVec(SVECTOR* v, u16 k)
{
    gte_lddp(k);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
}

static __inline__ SVECTOR* Actor403000_PushVec(void)
{
    SVECTOR* head;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    return head - 1;
}

static __inline__ void Actor403000_PopVec(void)
{
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static inline s8 Actor403000_Cell(GfxCoord* coord)
{
    s32 x;
    s32 z;
    s8  col;
    s8  row;
    s32 cell;

    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x < 0xD48) {
        col = 4;
    } else if (x < 0x1A90) {
        col = 3;
    } else if (x < 0x2AF8) {
        col = 2;
    } else {
        col = x < 0x3C8C;
    }
    row  = z >= 0x1068;
    cell = (s8)D_actor_403000_80158D48[col + row * 5];
    return cell;
}

/// Which way round the ring of cells to go for a cell difference of `diff`
/// (own cell minus the player's): -1 when the difference is below -5 or in
/// 0..4, 1 otherwise.
static inline s8 _actor403000RingSide(s16 diff)
{
    if (diff < -5) {
        return -1;
    }
    if (diff < 0) {
        return 1;
    }
    if (diff < 5) {
        return -1;
    }
    return 1;
}

/// Turn toward the camera target (state 4) and, once facing it, walk at it
/// (state 2): hand off to state 8 or 7 by distance, or to state 4 with a
/// fresh `watchRingDir` direction when the heading error grows past 0x300.
static void func_actor_403000_80137084(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    _Actor403000ChaseScratch* head;
    GfxCoord*                 coord;
    GfxCoord*                 pos;
    GfxCoord*                 pos2;
    SVECTOR*                  dir;
    SVECTOR*                  t;
    s32                       mag;
    s16                       diff;
    s32                       dist;
    s8                        sign;
    PlayerStatus*             wip;
    TmdObject*                tmd;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    wip                                            = &gPlayerStatus;
    head                                           = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = head - 1;
    scratch                                        = head - 1;
    if (work->stateEntered != 0) {
        tmd                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        tmd->flags            = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->torsoSphere.body.radius      = 0x3E8;
        work->animStart                    = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                     = 0x10;
        work->requestedAnimId              = 4;
        work->overlayActive                = 0;
        work->forelegYawTarget             = 0;
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x384;
        work->rootSphere.body.flags       |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->requestedAnimId == 4) {
        t             = &scratch->offset;
        pos           = arg0->extra.tmd->coords;
        t->vx         = wip->coordMtx->t[0] - pos->coord.t[0];
        t->vy         = wip->coordMtx->t[1] - pos->coord.t[1];
        t->vz         = wip->coordMtx->t[2] - pos->coord.t[2];
        scratch->turn = mag = actorYawTo(arg0->extra.tmd->coords, t->vx, t->vz);
        work->neckYawTarget = mag;
        if (scratch->turn > 0x40) {
            scratch->turn = 0x40;
        } else if (scratch->turn < -0x40) {
            scratch->turn = -0x40;
        }
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        if (ABS(work->neckYawTarget) < 0x80) {
            work->requestedAnimId = 2;
            work->animStart       = ACTOR_403000_ANIM_BLEND_IN;
        }
        ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    }
    if (work->requestedAnimId == 2) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
        t             = &scratch->offset;
        pos2          = arg0->extra.tmd->coords;
        t->vx         = gPlayerStatus.coordMtx->t[0] - pos2->coord.t[0];
        t->vy         = gPlayerStatus.coordMtx->t[1] - pos2->coord.t[1];
        t->vz         = gPlayerStatus.coordMtx->t[2] - pos2->coord.t[2];
        scratch->turn = mag = actorYawTo(arg0->extra.tmd->coords, t->vx, t->vz);
        work->neckYawTarget = mag;
        coord               = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dir);
            VectorNormalSS(dir, dir);
            Actor403000_ScaleVec(dir, 300);
            coord->coord.t[0]  += dir->vx;
            coord->coord.t[1]  += dir->vy;
            coord->coord.t[2]  += dir->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        scratch->playerDelta.vx               = wip->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy               = 0;
        scratch->playerDelta.vz               = wip->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = dist = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        dist                          -= 0x1964;
        if (dist < 0) {
            dist = -dist;
        }
        if (dist < 0x1F4 && ABS(scratch->turn) < 0x200) {
            work->state = ACTOR_403000_STATE_LUNGE;
        }
        if (scratch->playerDistance < 0x1770 && ABS(scratch->turn) < 0x200) {
            work->state = ACTOR_403000_STATE_GRAB;
        }
        if (ABS(work->neckYawTarget) > 0x300) {
            scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
            scratch->cell       = Actor403000_Cell(arg0->extra.tmd->coords);
            work->state         = ACTOR_403000_STATE_TURN;
            diff                = scratch->cell - scratch->playerCell;
            sign                = _actor403000RingSide(diff);
            work->turnRingDir = work->watchRingDir = -sign;
        }
        if (scratch->turn > 0x40) {
            scratch->turn = 0x40;
        } else if (scratch->turn < -0x40) {
            scratch->turn = -0x40;
        }
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Grab approach (animation 2 then 0xB): on the frame `stateEntered` is set, record
/// the player's offset; walk forward until the player is within 0xDAC, then
/// switch to the lunge clip with a per-frame step of a fifteenth of the distance.
/// On frame 10 of the lunge, if a hit record is live and the player accepts
/// message 0x3F8, snap the model in front of the player and send the grab
/// (front or back by the facing difference). `ANIMATION_SLOT_SETTLED` on slot 1 ends the
/// lunge in state 4 with a fresh `watchRingDir` direction.
static void func_actor_403000_801377C8(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    _Actor403000ChaseScratch* head;
    GfxCoord*                 coord;
    GfxCoord*                 coord2;
    GfxCoord*                 pos;
    GfxCoord*                 rot;
    SVECTOR*                  dir;
    SVECTOR*                  t;
    SVECTOR*                  v;
    SVECTOR*                  dirA;
    SVECTOR*                  dirB;
    Task*                     task;
    GameActor*                pw;
    WorldCollisionContact*    recs;
    s16                       angle;
    s16                       step;
    s16                       i;
    s16                       diff;
    s32                       found;
    s32                       value;
    s32                       mag;
    s8                        sign;
    s16                       cell;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                           = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = head - 1;
    enemy                                          = arg0->spawnArg2.pointer;
    scratch                                        = head - 1;
    if (work->stateEntered != 0) {
        work->attackTarget.vx = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        work->attackTarget.vy = player->extra.tmd->coords->coord.t[1] - arg0->extra.tmd->coords->coord.t[1];
        work->attackTarget.vz = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        work->requestedAnimId = 2;
        work->animStart       = ACTOR_403000_ANIM_BLEND_IN;
        work->stateFrame      = 0;
    }
    if (work->requestedAnimId == 2) {
        scratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = 0;
        scratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        if (scratch->playerDistance < 0xDAC) {
            work->requestedAnimId = 0xB;
            work->animStart       = ACTOR_403000_ANIM_RESTART;
            work->stateFrame      = -1;
            work->grabStep        = scratch->playerDistance / 15;
        }
    }
    if (work->requestedAnimId == 0xB) {
        if (work->stateFrame == 0xA) {
            t     = &scratch->offset;
            pos   = arg0->extra.tmd->coords;
            t->vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
            t->vy = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
            t->vz = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
            rot   = arg0->extra.tmd->coords;
            angle = ratan2(t->vx, t->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
            if (angle < 0) {
                for (;;) {
                    if (angle >= -0x800) {
                        goto wrapped;
                    }
                    angle += 0x1000;
                }
            } else {
                for (;;) {
                    if (angle <= 0x800) {
                        goto wrapped;
                    }
                    angle -= 0x1000;
                }
            }
        wrapped:
            scratch->turn = mag = angle;
            if (ABS(mag) < 0x400) {
                recs = work->neckSphere.contacts;
                for (i = 0; i < ARRAY_SIZE(work->neckSphere.contacts); i++) {
                    value = recs[i].key.value;
                    if (value == 0) {
                        break;
                    }
                    if ((value & 0xFFFF0000) == 0x10000) {
                        found = 1;
                        goto done;
                    }
                }
                found = 0;
            done:
                if (found != 0 && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
                    work->state            = ACTOR_403000_STATE_GRAB_CATCH;
                    work->playerCaught     = 1;
                    work->attackTarget.vx  = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
                    work->attackTarget.vy  = player->extra.tmd->coords->coord.t[1] - arg0->extra.tmd->coords->coord.t[1];
                    work->attackTarget.vz  = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
                    scratch->playerYaw     = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0], gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
                    t                      = &scratch->offset;
                    pos                    = arg0->extra.tmd->coords;
                    t->vx                  = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
                    t->vy                  = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
                    t->vz                  = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
                    scratch->yawFromPlayer = ratan2(scratch->offset.vx, scratch->offset.vz) + 0x800;
                    angle                  = scratch->yawFromPlayer;
                    if (angle < 0) {
                        for (;;) {
                            if (angle >= -0x800) {
                                goto wrapped2;
                            }
                            angle += 0x1000;
                        }
                    } else {
                        for (;;) {
                            if (angle <= 0x800) {
                                goto wrapped2;
                            }
                            angle -= 0x1000;
                        }
                    }
                wrapped2:
                    scratch->yawFromPlayer = mag = angle;
                    mag                         -= scratch->playerYaw;
                    if (ABS(mag) < 0x400) {
                        work->playerAnimation.source.sets          = D_actor_403000_80158C08;
                        work->playerAnimation.animationId          = 3;
                        work->playerAnimation.blend                = 0;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        dir = &scratch->offset;
                        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, dir);
                        VectorNormalSS(dir, dir);
                        gte_lddp(-0x546);
                        gte_ldsv(dir);
                        gte_gpf12();
                        gte_stsv(dir);
                        arg0->extra.tmd->coords->coord.t[0]      = player->extra.tmd->coords->coord.t[0] + scratch->offset.vx;
                        arg0->extra.tmd->coords->coord.t[1]      = player->extra.tmd->coords->coord.t[1] + scratch->offset.vy;
                        arg0->extra.tmd->coords->coord.t[2]      = player->extra.tmd->coords->coord.t[2] + scratch->offset.vz;
                        arg0->extra.tmd->coords->composeStamp    = GRAPHICS_COORD_DIRTY;
                        D_actor_403000_80158D90.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
                        D_actor_403000_80158D90.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
                        D_actor_403000_80158D90.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
                        D_actor_403000_80158D90.placement.rot.vx = 0;
                        D_actor_403000_80158D90.placement.rot.vy = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - 0x500;
                        D_actor_403000_80158D90.placement.rot.vz = 0;
                        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403000_80158D90.placement, 0);
                    } else {
                        work->playerAnimation.source.sets          = D_actor_403000_80158C28;
                        work->playerAnimation.animationId          = 3;
                        work->playerAnimation.blend                = 0;
                        work->playerAnimation.blendFrames          = 0;
                        work->playerAnimation.enableWorldCollision = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
                        dir = &scratch->offset;
                        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, dir);
                        VectorNormalSS(dir, dir);
                        gte_lddp(-0x546);
                        gte_ldsv(dir);
                        gte_gpf12();
                        gte_stsv(dir);
                        arg0->extra.tmd->coords->coord.t[0]      = player->extra.tmd->coords->coord.t[0] + scratch->offset.vx;
                        arg0->extra.tmd->coords->coord.t[1]      = player->extra.tmd->coords->coord.t[1] + scratch->offset.vy;
                        arg0->extra.tmd->coords->coord.t[2]      = player->extra.tmd->coords->coord.t[2] + scratch->offset.vz;
                        arg0->extra.tmd->coords->composeStamp    = GRAPHICS_COORD_DIRTY;
                        D_actor_403000_80158D90.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
                        D_actor_403000_80158D90.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
                        D_actor_403000_80158D90.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
                        D_actor_403000_80158D90.placement.rot.vx = 0;
                        D_actor_403000_80158D90.placement.rot.vy = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) + 0x400;
                        D_actor_403000_80158D90.placement.rot.vz = 0;
                        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403000_80158D90.placement, 0);
                    }
                    task                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    scratch->messageResult = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 1), 0);
                    if (scratch->messageResult == 1) {
                        pw                              = (GameActor*)player->work;
                        gGameSession->deathFadeFrames   = 0x28;
                        gGameSession->deathRestartDelay = 0x28;
                        pw->state                       = 0xA;
                    }
                }
            }
        }
    }
    if (work->requestedAnimId == 2) {
        coord = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dirA = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dirA);
            VectorNormalSS(dirA, dirA);
            Actor403000_ScaleVec(dirA, 300);
            coord->coord.t[0]  += dirA->vx;
            coord->coord.t[1]  += dirA->vy;
            coord->coord.t[2]  += dirA->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
    }
    if (work->requestedAnimId == 0xB && work->stateFrame < 0xE) {
        coord2 = arg0->extra.tmd->coords;
        step   = work->grabStep;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dirB = Actor403000_PushVec();
            v    = dirB;
            if (step != 0) {
                gfxReadMatrixZAxis(&coord2->coord, dirB);
                VectorNormalSS(dirB, dirB);
                Actor403000_ScaleVec(v, step);
                coord2->coord.t[0]  += dirB->vx;
                coord2->coord.t[1]  += dirB->vy;
                coord2->coord.t[2]  += dirB->vz;
                coord2->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            Actor403000_PopVec();
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 1 && work->requestedAnimId == 2 && work->stateFrame >= 0x10) {
        work->state       = ACTOR_403000_STATE_TURN;
        work->turnRingDir = -work->watchRingDir;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->requestedAnimId == 0xB) {
            scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
            cell                = Actor403000_Cell(arg0->extra.tmd->coords);
            scratch->cell       = cell;
            diff                = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg1;
            }
            if (diff < 0) {
                goto pos1;
            }
            if (diff < 5) {
            neg1:
                sign = -1;
            } else {
            pos1:
                sign = 1;
            }
            work->watchRingDir = sign;
            diff               = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg2;
            }
            if (diff < 0) {
                goto pos2;
            }
            if (diff < 5) {
            neg2:
                sign = -1;
            } else {
            pos2:
                sign = 1;
            }
            work->turnRingDir = work->watchRingDir = -sign;
            work->state                            = ACTOR_403000_STATE_TURN;
        }
    }
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Per-frame push: on the frame `stateEntered` is set, turn the display object's
/// first matrix column into a short push vector and play the enemy's sound,
/// then send it to the player as `GAME_ACTOR_MESSAGE_MOVE_BY` for the first 0x28 frames.
/// `ANIMATION_SLOT_REACHED_BOUNDARY` on slot 1 moves the state machine to 4 and flips `watchRingDir`.
static void func_actor_403000_801384E8(Task* arg0)
{
    Actor403000Work*          work;
    Enemy*                    enemy;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    s32                       sound;
    s32                       pan;
    s32                       ret;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch                                        = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) - 1;
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = scratch;
    if (work->stateEntered != 0) {
        enemy            = arg0->spawnArg2.pointer;
        work->stateFrame = 0;
        gfxReadMatrixXAxis(&arg0->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(0x55);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        sound                                          = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
        pan                                            = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrame < 0x28) {
        ret = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.push, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.push.displacement.vx   = 0;
            D_actor_403000_80158DB0.push.displacement.vy   = 0;
            D_actor_403000_80158DB0.push.displacement.vz   = 0;
            D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
            D_actor_403000_80158DB0.push.keepControl       = ret;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state        = ACTOR_403000_STATE_TURN;
        work->watchRingDir = work->turnRingDir = work->patrolRingDir = -work->watchRingDir;
    }
    func_actor_403000_80133AF8(arg0);
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Lunge toward the player: on the frame `stateEntered` is set, record the player's
/// position and turn the distance to it into the per-frame step `lungeStep`;
/// for frames 5..24 push the display object along its third matrix column by
/// that step, for frames 5..14 turn it toward the camera target by at most
/// 0x40, and on frame 0x17 send the grab messages if a hit record is live.
static void func_actor_403000_801386E8(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    _Actor403000ChaseScratch* head;
    GfxCoord*                 coord;
    SVECTOR*                  dir;
    Task*                     task;
    s16                       step;
    s16                       angle;
    s16                       i;
    s32                       found;
    s32                       value;
    s32                       dist;
    s32                       mag;
    SVECTOR*                  t;
    GfxCoord*                 coord2;
    GfxCoord*                 coord3;
    WorldCollisionContact*    recs;
    GameActor*                pw;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                           = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = head - 1;
    enemy                                          = arg0->spawnArg2.pointer;
    scratch                                        = head - 1;
    if (work->stateEntered != 0) {
        work->lungePlayerPos.vx = player->extra.tmd->coords->coord.t[0];
        work->lungePlayerPos.vy = player->extra.tmd->coords->coord.t[1];
        work->lungePlayerPos.vz = player->extra.tmd->coords->coord.t[2];
        scratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->playerDelta.vy = 0;
        scratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->playerDistance = dist = SquareRoot0(scratch->playerDelta.vx * scratch->playerDelta.vx + scratch->playerDelta.vy * scratch->playerDelta.vy + scratch->playerDelta.vz * scratch->playerDelta.vz);
        work->requestedAnimId          = 7;
        work->animStart                = ACTOR_403000_ANIM_BLEND_IN;
        work->stateFrame               = 0;
        work->rootSphere.body.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->neckSphere.body.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->lungeStep                = (dist - 900) / 20;
    }
    if (work->stateFrame >= 5 && work->stateFrame < 25) {
        coord = arg0->extra.tmd->coords;
        step  = work->lungeStep;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            if (step != 0) {
                gfxReadMatrixZAxis(&coord->coord, dir);
                VectorNormalSS(dir, dir);
                Actor403000_ScaleVec(dir, step);
                coord->coord.t[0]  += dir->vx;
                coord->coord.t[1]  += dir->vy;
                coord->coord.t[2]  += dir->vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            Actor403000_PopVec();
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((u16)(work->stateFrame - 5) < 10) {
        t      = &scratch->offset;
        coord3 = arg0->extra.tmd->coords;
        t->vx  = gPlayerStatus.coordMtx->t[0] - coord3->coord.t[0];
        t->vy  = gPlayerStatus.coordMtx->t[1] - coord3->coord.t[1];
        t->vz  = gPlayerStatus.coordMtx->t[2] - coord3->coord.t[2];
        coord2 = arg0->extra.tmd->coords;
        angle  = ratan2(t->vx, t->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->turn = mag = angle;
        if (scratch->turn > 0x40) {
            scratch->turn = 0x40;
        } else if (scratch->turn < -0x40) {
            scratch->turn = -0x40;
        }
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    }
    recs = work->headCapsule.contacts;
    if (work->stateFrame == 0x17) {
        for (i = 0; i < ARRAY_SIZE(work->headCapsule.contacts); i++) {
            value = recs[i].key.value;
            if (value == 0) {
                break;
            }
            if ((value & 0xFFFF0000) == 0x10000) {
                found = 1;
                goto done;
            }
        }
        found = 0;
    done:
        if (found != 0 && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
            D_actor_403000_80158D90.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
            D_actor_403000_80158D90.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
            D_actor_403000_80158D90.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
            D_actor_403000_80158D90.placement.rot.vx = 0;
            D_actor_403000_80158D90.placement.rot.vy = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - 0x400;
            D_actor_403000_80158D90.placement.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403000_80158D90.placement, 0);
            task                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->messageResult = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0);
            if (scratch->messageResult == 1) {
                pw                              = (GameActor*)player->work;
                gGameSession->deathFadeFrames   = 0x1C;
                gGameSession->deathRestartDelay = 0x1E;
                pw->state                       = 0xA;
            }
            work->state                                = ACTOR_403000_STATE_LUNGE_CATCH;
            work->playerAnimation.source.sets          = D_actor_403000_80158C08;
            work->playerAnimation.animationId          = 1;
            work->playerAnimation.blend                = 0;
            work->playerAnimation.blendFrames          = 3;
            work->playerAnimation.enableWorldCollision = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
            work->playerCaught = 1;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state         = ACTOR_403000_STATE_PATROL;
        work->patrolRingDir = work->watchRingDir;
        work->watchRingDir  = -work->watchRingDir;
    }
    func_actor_403000_80133AF8(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 0) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->neckSphere.contacts, ARRAY_SIZE(work->neckSphere.contacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
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
    GfxCoord*                 rot;
    GfxCoord*                 pos;
    GfxCoord*                 pos2;
    SVECTOR*                  dir;
    SVECTOR*                  t1;
    SVECTOR*                  vp;
    SVECTOR*                  t2;
    SVECTOR*                  t3;
    SVECTOR*                  t4;
    SVECTOR                   v;
    s16                       angle;
    s16                       diff;
    s32                       dist;
    s32                       ret;
    s8                        sign;
    s8                        sign2;
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
        Actor403000_ScaleVec(t1, 0x41A);
        arg0->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] - scratch->offset.vx;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - scratch->offset.vy;
        arg0->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] - scratch->offset.vz;
        pos                                 = arg0->extra.tmd->coords;
        t1->vx                              = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
        t1->vy                              = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
        t1->vz                              = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
        rot                                 = arg0->extra.tmd->coords;
        angle                               = ratan2(t1->vx, t1->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->turn  = angle;
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        t2                                    = &scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t2);
        VectorNormalSS(t2, t2);
        Actor403000_ScaleVec(t2, 0x96);
        D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        Gp_SpawnPadLerp(5, 0xFF, 0x80);
        work->hitEffectArg.coord      = &player->extra.tmd->coords[3];
        work->hitEffectArg.spawnArgLo = 0x500;
        work->hitEffectArg.spawnArgHi = 3;
        func_800FDB18(Gp_GetIdParam1(0x100F) & 0xFFFF, &player->extra.tmd->coords[3], 0, &work->hitEffectArg);
    }
    if (work->stateFrame < 10) {
        coord = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dir);
            VectorNormalSS(dir, dir);
            Actor403000_ScaleVec(dir, 0x78);
            coord->coord.t[0]  += dir->vx;
            coord->coord.t[1]  += dir->vy;
            coord->coord.t[2]  += dir->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
        v.vz = 0;
        v.vy = 0;
        v.vx = 0;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &v);
        vp = &v;
        VectorNormalSS(vp, vp);
        Actor403000_ScaleVec(vp, 0x546);
        v.vx = v.vx + arg0->extra.tmd->coords->coord.t[0] - player->extra.tmd->coords->coord.t[0];
        v.vy = 0;
        v.vz = v.vz + arg0->extra.tmd->coords->coord.t[2] - player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(vp, vp);
        Actor403000_ScaleVec(vp, 0x96);
        D_actor_403000_80158DB0.push.displacement.vy   = 0;
        D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.push.keepControl       = 1;
        D_actor_403000_80158DB0.push.displacement.vx   = v.vx;
        D_actor_403000_80158DB0.push.displacement.vz   = v.vz;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.push, 0);
    }
    if (work->stateFrame < 6) {
        t3     = &scratch->offset;
        pos2   = arg0->extra.tmd->coords;
        t3->vx = gPlayerStatus.coordMtx->t[0] - pos2->coord.t[0];
        t3->vy = gPlayerStatus.coordMtx->t[1] - pos2->coord.t[1];
        t3->vz = gPlayerStatus.coordMtx->t[2] - pos2->coord.t[2];
        rot    = arg0->extra.tmd->coords;
        angle  = ratan2(t3->vx, t3->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
        if (angle < 0) {
        loop_neg2:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg2;
            }
        } else {
        loop_pos2:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos2;
            }
        }
        scratch->turn  = angle;
        scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->stateFrame == 0x29) {
        t4 = &scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        Actor403000_ScaleVec(t4, 0x21);
        D_actor_403000_80158DB0.push.displacement.vx = scratch->offset.vx;
        D_actor_403000_80158DB0.push.displacement.vy = 0;
        D_actor_403000_80158DB0.push.displacement.vz = scratch->offset.vz;
        gfxReadMatrixXAxis(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        Actor403000_ScaleVec(t4, 0x7D);
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
        scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
        cell                = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->cell       = cell;
        diff                = (s8)cell - scratch->playerCell;
        if (diff < -5) {
            goto neg;
        }
        if (diff < 0) {
            goto pos;
        }
        if (diff < 5) {
        neg:
            sign = -1;
        } else {
        pos:
            sign = 1;
        }
        work->watchRingDir = sign;
        diff               = scratch->cell - scratch->playerCell;
        if (diff < -5) {
            goto neg2;
        }
        if (diff < 0) {
            goto pos2;
        }
        if (diff < 5) {
        neg2:
            sign2 = -1;
        } else {
        pos2:
            sign2 = 1;
        }
        work->turnRingDir = work->watchRingDir = -sign2;
        work->state                            = ACTOR_403000_STATE_TURN;
    }
    func_actor_403000_80133AF8(arg0);
    if (++work->stateFrame < 10) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
        ActorContact_PushContact(arg0->extra.tmd->coords, work->neckSphere.contacts, ARRAY_SIZE(work->neckSphere.contacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_801399A0(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->requestedAnimId         = 0xE;
        work->animRate                = 0x10;
        work->lockOnSuspended         = 0;
        work->neckYaw                 = 0;
        work->neckYawTarget           = 0;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->torsoSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts)) == 0) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->torsoSphere.contacts, ARRAY_SIZE(work->torsoSphere.contacts));
    }
    func_actor_403000_80133AF8(arg0);
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && work->requestedAnimId == 0xE) {
        work->torsoSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
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
}

/// Waypoint-grid cell under `coord`: column by `coord.t[0]` band, row by
/// `coord.t[2]`, as `func_actor_403000_80134204` computes it inline.
static void func_actor_403000_80139AE0(Task* arg0)
{
    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   obj;
    GfxCoord*                    coord;
    SVECTOR*                     table;
    SVECTOR*                     v;
    s16                          angle;
    s16                          diff;
    s32                          mag;
    s8                           base;
    s8                           dir;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 2;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x320;
        scratch->playerCell                = Actor403000_Cell(player->extra.tmd->coords);
        scratch->cell                      = Actor403000_Cell(arg0->extra.tmd->coords);
        diff                               = scratch->cell - scratch->playerCell;
        if (diff < -5) {
            goto neg;
        }
        if (diff < 0) {
            goto pos;
        }
        if (diff < 5) {
        neg:
            dir = -1;
        } else {
        pos:
            dir = 1;
        }
        work->seekRingDir = dir;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
    base                = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->cell       = base;
    if (func_actor_403000_80133FC0(arg0, base, scratch->playerCell) << 16) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    scratch->waypoint = scratch->cell + work->seekRingDir;
    if (scratch->waypoint != -1) {
        if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint = 0;
        }
    } else {
        scratch->waypoint = ACTOR_403000_RING_WAYPOINT_COUNT - 1;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->waypoint];
    scratch->offset.vx  = v->vx;
    scratch->offset.vy  = v->vy;
    scratch->offset.vz  = v->vz;
    scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag                 = angle;
    scratch->turn       = mag;
    work->neckYawTarget = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->turn > 0x30) {
        scratch->turn = 0x30;
    }
    if (scratch->turn < -0x30) {
        scratch->turn = -0x30;
    }
    scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Waypoint-ring variant of `func_actor_403000_80139AE0`: on entry pick the
/// ring direction from the player/model cell difference, then steer (clamped to
/// 0x40 per frame) toward the next waypoint and step forward 0x12C; switch to 5
/// once `func_actor_403000_80133FC0` allows it after 60 frames.
static void func_actor_403000_8013A08C(Task* arg0)
{
    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   obj;
    GfxCoord*                    coord;
    SVECTOR*                     table;
    SVECTOR*                     v;
    s16                          angle;
    s16                          diff;
    s32                          mag;
    s32                          cell;
    s8                           base;
    s8                           dir;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 2;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x320;
        scratch->playerCell                = Actor403000_Cell(player->extra.tmd->coords);
        cell                               = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->cell                      = cell;
        if (cell != scratch->playerCell) {
            diff = cell - scratch->playerCell;
            if (diff < -5) {
                goto neg;
            }
            if (diff < 0) {
                goto pos;
            }
            if (diff < 5) {
            neg:
                dir = -1;
            } else {
            pos:
                dir = 1;
            }
        } else {
            dir = work->seekRingDir;
        }
        work->seekRingDir = -dir;
        work->stateFrame  = 0;
    }
    work->stateFrame++;
    ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
    base                = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->cell       = base;
    if ((func_actor_403000_80133FC0(arg0, base, scratch->playerCell) << 16) && work->stateFrame > 0x3C) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    scratch->waypoint = scratch->cell + work->seekRingDir;
    if (scratch->waypoint != -1) {
        if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint = 0;
        }
    } else {
        scratch->waypoint = ACTOR_403000_RING_WAYPOINT_COUNT - 1;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->waypoint];
    scratch->offset.vx  = v->vx;
    scratch->offset.vy  = v->vy;
    scratch->offset.vz  = v->vz;
    scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag                 = angle;
    scratch->turn       = mag;
    work->neckYawTarget = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->turn > 0x40) {
        scratch->turn = 0x40;
    }
    if (scratch->turn < -0x40) {
        scratch->turn = -0x40;
    }
    scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Waypoint-ring patrol toward a goal waypoint: on entry derive the goal from
/// the player's cell and, if unset, the ring direction `patrolRingDir`; every frame
/// on reaching the goal pick the next state (3, 11 or 15) from a random roll,
/// then steer (clamped to 0x40) toward the next waypoint and step forward 0x12C.
static void func_actor_403000_8013A678(Task* arg0)
{
    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   obj;
    GfxCoord*                    coord;
    SVECTOR*                     table;
    SVECTOR*                     v;
    s16                          angle;
    s16                          diff;
    s32                          mag;
    s16                          r;
    s8                           base;
    s8                           dir;
    s8                           goal;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 2;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x320;
        scratch->playerCell                = Actor403000_Cell(player->extra.tmd->coords);
        scratch->cell                      = Actor403000_Cell(arg0->extra.tmd->coords);
        switch ((u8)scratch->playerCell) {
            case 0:
            case 1:
            case 2:
                goal = 5;
                break;
            case 3:
            case 4:
                goal = 9;
                break;
            case 5:
            case 6:
            case 7:
                goal = 0;
                break;
            case 8:
            case 9:
                goal = 4;
                break;
            default:
                goal = -1;
                break;
        }
        work->patrolGoalCell = goal;
        if (work->patrolRingDir == 0) {
            diff = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg;
            }
            if (diff < 0) {
                goto pos;
            }
            if (diff < 5) {
            neg:
                dir = -1;
            } else {
            pos:
                dir = 1;
            }
            work->patrolRingDir = -dir;
        }
    }
    work->stateFrame++;
    ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    base          = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->cell = base;
    if (base == work->patrolGoalCell) {
        if (work->ambushRequested == 1) {
            work->state = ACTOR_403000_STATE_AMBUSH;
        } else {
            r = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF;
            if (arg0->extra.tmd->texturePageOffset == 2) {
                if (r <= 0) {
                    work->state = ACTOR_403000_STATE_WATCH;
                } else if (r < 7) {
                    work->state = ACTOR_403000_STATE_AMBUSH;
                } else {
                    work->state = ACTOR_403000_STATE_APPROACH;
                }
            } else if (r < 7) {
                work->state = ACTOR_403000_STATE_WATCH;
            } else if (r < 0xB) {
                work->state = ACTOR_403000_STATE_AMBUSH;
            } else {
                work->state = ACTOR_403000_STATE_APPROACH;
            }
        }
    }
    scratch->waypoint = scratch->cell + work->patrolRingDir;
    if (scratch->waypoint != -1) {
        if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint = 0;
        }
    } else {
        scratch->waypoint = ACTOR_403000_RING_WAYPOINT_COUNT - 1;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->waypoint];
    scratch->offset.vx  = v->vx;
    scratch->offset.vy  = v->vy;
    scratch->offset.vz  = v->vz;
    scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag                 = angle;
    scratch->turn       = mag;
    work->neckYawTarget = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->turn > 0x40) {
        scratch->turn = 0x40;
    }
    if (scratch->turn < -0x40) {
        scratch->turn = -0x40;
    }
    scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Walk the waypoint ring: on the entry frame snap the model onto its cell's
/// waypoint and face the neighbour in the `watchRingDir` direction; every frame
/// finish (state 2) on reaching the player's cell, give up (14) after 300
/// frames, or switch to 5 once `func_actor_403000_80133FC0` allows it.
static void func_actor_403000_8013ACBC(Task* arg0)
{
    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   obj;
    GfxCoord*                    coord;
    SVECTOR*                     table;
    SVECTOR*                     v;
    s16                          angle;
    s16                          index;
    s8                           base;
    SVECTOR*                     last;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 4;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        base                                  = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->cell                         = base;
        table                                 = D_actor_403000_80158CE0;
        v                                     = &table[base];
        scratch->offset.vx                    = v->vx;
        scratch->offset.vy                    = v->vy;
        scratch->offset.vz                    = v->vz;
        arg0->extra.tmd->coords->coord.t[0]   = scratch->offset.vx;
        arg0->extra.tmd->coords->coord.t[1]   = scratch->offset.vy;
        arg0->extra.tmd->coords->coord.t[2]   = scratch->offset.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->watchRingDir == 1) {
            if (scratch->cell + 1 >= ACTOR_403000_RING_WAYPOINT_COUNT) {
                scratch->offset.vx = table[0].vx;
                scratch->offset.vy = table[0].vy;
                scratch->offset.vz = table[0].vz;
            } else {
                index              = scratch->cell + 1;
                scratch->offset.vx = table[index].vx;
                scratch->offset.vy = table[index].vy;
                scratch->offset.vz = table[index].vz;
            }
        } else {
            if (scratch->cell - 1 < 0) {
                last               = &table[ACTOR_403000_RING_WAYPOINT_COUNT - 1];
                scratch->offset.vx = last->vx;
                scratch->offset.vy = last->vy;
                scratch->offset.vz = last->vz;
            } else {
                index              = scratch->cell - 1;
                scratch->offset.vx = table[index].vx;
                scratch->offset.vy = table[index].vy;
                scratch->offset.vz = table[index].vz;
            }
        }
        scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
        scratch->offset.vy  = 0;
        scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
        coord               = arg0->extra.tmd->coords;
        angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->turn       = angle;
        work->neckYawTarget = 0;
        scratch->turn      += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x320;
        scratch->playerCell                = Actor403000_Cell(player->extra.tmd->coords);
    }
    work->stateFrame++;
    scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
    scratch->cell       = Actor403000_Cell(arg0->extra.tmd->coords);
    func_actor_403000_80133AF8(arg0);
    if (work->stateFrame > 300) {
        work->state = ACTOR_403000_STATE_PROWL;
    } else if (scratch->playerCell == scratch->cell) {
        work->state         = ACTOR_403000_STATE_PATROL;
        work->patrolRingDir = work->watchRingDir;
        work->watchRingDir  = -work->watchRingDir;
    } else if (func_actor_403000_80133FC0(arg0, scratch->cell, scratch->playerCell) << 16) {
        if (work->stateFrame > 60) {
            work->state = ACTOR_403000_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

/// Turn toward the next waypoint: on the entry frame pick it from the grid
/// offset by `turnRingDir`, set state 2 if it is within a quarter turn, and fold
/// the display object's scaled first and third matrix columns into the drift
/// `turnDrift`; every frame re-aim by `turnStep` and apply that drift.
static void func_actor_403000_8013B238(Task* arg0)
{
    Actor403000Work*             work;
    _Actor403000WaypointScratch* scratch;
    GfxCoord*                    coord;
    s32                          x;
    s32                          z;
    s8                           col;
    s32                          b;
    s8                           row;
    TmdObject*                   obj;
    SVECTOR*                     v;
    SVECTOR*                     table;
    s32                          mag;
    _Actor403000WaypointScratch* head;
    s16                          angle;

    head                                              = SCRATCH_STACK_CURSOR(_Actor403000WaypointScratch);
    work                                              = arg0->work;
    SCRATCH_STACK_CURSOR(_Actor403000WaypointScratch) = head - 1;
    scratch                                           = head - 1;
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 9;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        x                             = arg0->extra.tmd->coords->coord.t[0];
        z                             = arg0->extra.tmd->coords->coord.t[2];
        if (x < 0xD48) {
            col = 4;
        } else if (x < 0x1A90) {
            col = 3;
        } else if (x < 0x2AF8) {
            col = 2;
        } else {
            col = x < 0x3C8C;
        }
        row               = z >= 0x1068;
        b                 = (s8)D_actor_403000_80158D48[col + row * 5];
        scratch->cell     = b;
        scratch->waypoint = scratch->cell + work->turnRingDir;
        if (scratch->waypoint >= ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint -= ACTOR_403000_RING_WAYPOINT_COUNT;
        } else if (scratch->waypoint < 0) {
            scratch->waypoint += ACTOR_403000_RING_WAYPOINT_COUNT;
        }
        table               = D_actor_403000_80158CE0;
        v                   = &table[scratch->waypoint];
        scratch->offset.vx  = v->vx;
        scratch->offset.vy  = v->vy;
        scratch->offset.vz  = v->vz;
        scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
        scratch->offset.vy -= arg0->extra.tmd->coords->coord.t[1];
        scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
        coord               = arg0->extra.tmd->coords;
        angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        mag           = angle;
        scratch->turn = mag;
        if (ABS(mag) < 0x400) {
            work->state = ACTOR_403000_STATE_PATROL;
        }
        if (scratch->turn < 0) {
            scratch->turn += 0x1000;
        }
        gfxReadMatrixXAxis(&arg0->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(0x1B);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        work->turnDrift = scratch->offset;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(-0x29);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        work->turnStep          = scratch->turn / 36;
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
    ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->turn = work->turnStep + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    arg0->extra.tmd->coords->coord.t[0]  += work->turnDrift.vx;
    arg0->extra.tmd->coords->coord.t[1]  += work->turnDrift.vy;
    arg0->extra.tmd->coords->coord.t[2]  += work->turnDrift.vz;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403000_STATE_PATROL;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

static __inline__ s32 Actor403000_Outside(SVECTOR* v, s32 r)
{
    OverlayRangeScratch* s;
    OverlayRangeScratch* head;
    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    s                                         = head - 1;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = s;
    s->dx                                     = v->vx;
    s->dz                                     = v->vz;
    s->radius                                 = r;
    s->dx                                    *= s->dx;
    s->dz                                    *= s->dz;
    s->radius                                *= s->radius;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = head;
    return (s->dx + s->dz) >= s->radius;
}

static void func_actor_403000_8013B74C(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    Enemy*                    enemy;
    _Actor403000ChaseScratch* scratch;
    _Actor403000ChaseScratch* head;
    GfxCoord*                 coord;
    GfxCoord*                 coord2;
    SVECTOR*                  t;
    Task*                     task;
    s16                       b;
    GameActor*                pw;
    s32                       frame;
    s16                       angle;
    s32                       mag;
    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attackTarget.vx               = player->extra.tmd->coords->coord.t[0];
        work->attackTarget.vy               = player->extra.tmd->coords->coord.t[1];
        work->attackTarget.vz               = player->extra.tmd->coords->coord.t[2];
        arg0->extra.tmd->coords->coord.t[0] = 0x2134;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - 0x1518;
        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
        work->requestedAnimId               = 8;
        work->animStart                     = ACTOR_403000_ANIM_RESTART;
        work->stateFrame                    = 0;
        work->animRate                      = 0;
        work->lockOnSuspended               = 0;
        scratch                             = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000ChaseScratch);
        b                                   = Actor403000_Cell(player->extra.tmd->coords);
        scratch->playerCell                 = b;
        switch (scratch->playerCell) {
            case 0:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
                break;

            case 1:
            case 2:
            case 3:
            case 4:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x400, 1);
                break;

            case 5:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0, 1);
                break;

            case 6:
            case 7:
            case 8:
            case 9:
            default:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                break;
        }

        SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
    }
    if (work->dropDelay != 0) {
        work->dropDelay--;
        return;
    }
    head    = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch);
    scratch = (SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = head - 1);
    frame   = work->stateFrame;
    if (frame == 10) {
        work->animRate     = 0x10;
        scratch->offset.vx = work->attackTarget.vx - player->extra.tmd->coords->coord.t[0];
        scratch->offset.vy = work->attackTarget.vy - player->extra.tmd->coords->coord.t[1];
        scratch->offset.vz = work->attackTarget.vz - player->extra.tmd->coords->coord.t[2];
        if (!Actor403000_Outside(&scratch->offset, 1000) && enemy->hp > 0 &&
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.hold, 0) == 0) {
            task                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->messageResult = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 3), 0);
            if (scratch->messageResult == 1) {
                pw                              = (GameActor*)player->work;
                gGameSession->deathFadeFrames   = 0x28;
                gGameSession->deathRestartDelay = 0x28;
                pw->state                       = frame;
            }
            work->state                                = ACTOR_403000_STATE_DROP_CATCH;
            work->playerAnimation.source.sets          = D_actor_403000_80158C08;
            work->playerAnimation.animationId          = 5;
            work->playerAnimation.blend                = 0;
            work->playerAnimation.blendFrames          = 0;
            work->playerAnimation.enableWorldCollision = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnimation, 0);
            work->playerCaught = 1;
        }
    }
    if (arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        arg0->extra.tmd->coords->coord.t[1] += 0x12C;
    }
    if (work->stateFrame >= 4) {
        arg0->extra.tmd->coords->coord.t[0] += (work->attackTarget.vx - arg0->extra.tmd->coords->coord.t[0]) >> 2;
        arg0->extra.tmd->coords->coord.t[2] += (work->attackTarget.vz - arg0->extra.tmd->coords->coord.t[2]) >> 2;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        t      = &scratch->offset;
        coord2 = arg0->extra.tmd->coords;
        t->vx  = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
        t->vy  = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
        t->vz  = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
        coord  = arg0->extra.tmd->coords;
        angle  = ratan2(t->vx, t->vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < (-0x800)) {
                angle += 0x1000;
                goto loop_neg;
            }

        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->turn = (mag = angle);
        if (ABS(mag) > 0x300) {
            work->state        = ACTOR_403000_STATE_TURN;
            work->watchRingDir = (work->patrolRingDir = (work->turnRingDir = -func_actor_403000_80134204(arg0->extra.tmd->coords)));
        } else {
            work->state         = ACTOR_403000_STATE_PATROL;
            work->watchRingDir  = -func_actor_403000_80134204(arg0->extra.tmd->coords);
            work->patrolRingDir = (work->turnRingDir = func_actor_403000_80134204(arg0->extra.tmd->coords));
        }
    }
    if (work->stateFrame >= 2) {
        work->animRate = 0x10;
    }
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

/// Ease the display object up toward the player and across to
/// `attackTarget.vx`/`attackTarget.vz`, turn the player's third matrix column into the
/// push vector for the first 8 frames, and once `ANIMATION_SLOT_REACHED_BOUNDARY` is set on slot 1
/// after frame 0xB move the state machine to 4.
static void func_actor_403000_8013BDE0(Task* arg0)
{
    Actor403000Work*          work;
    Task*                     player;
    _Actor403000ChaseScratch* scratch;
    Enemy*                    enemy;
    s32                       sound;
    s32                       pan;

    work                                           = arg0->work;
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch                                        = SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) - 1;
    SCRATCH_STACK_CURSOR(_Actor403000ChaseScratch) = scratch;
    if (work->stateEntered != 0) {
        enemy                 = arg0->spawnArg2.pointer;
        work->lockOnSuspended = 0;
        work->stateFrame      = 0;
        sound                 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
        pan                   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        arg0->extra.tmd->coords->coord.t[1] += 0x12C;
        arg0->extra.tmd->coords->coord.t[0] += (work->attackTarget.vx - arg0->extra.tmd->coords->coord.t[0]) >> 2;
        arg0->extra.tmd->coords->coord.t[2] += (work->attackTarget.vz - arg0->extra.tmd->coords->coord.t[2]) >> 2;
    }
    if (work->stateFrame < 8) {
        gfxReadMatrixZAxis(&player->extra.tmd->coords->coord, &scratch->offset);
        VectorNormalSS(&scratch->offset, &scratch->offset);
        gte_lddp(-0x2A);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
    }
    D_actor_403000_80158DB0.push.displacement.vx   = scratch->offset.vx;
    D_actor_403000_80158DB0.push.displacement.vy   = 0;
    D_actor_403000_80158DB0.push.displacement.vz   = scratch->offset.vz;
    D_actor_403000_80158DB0.push.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
    D_actor_403000_80158DB0.push.keepControl       = 1;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->stateFrame >= 0xB) {
        work->state        = ACTOR_403000_STATE_TURN;
        work->watchRingDir = work->patrolRingDir = work->turnRingDir = -func_actor_403000_80134204(arg0->extra.tmd->coords);
    }
    func_actor_403000_80133AF8(arg0);
    work->stateFrame++;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000ChaseScratch);
}

static void func_actor_403000_8013C050(Task* arg0)
{
    Actor403000Work* work;
    Task*            player;
    Enemy*           enemy;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->ambushRequested               = 0;
        arg0->extra.tmd->coords->coord.t[0] = 0x2134;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - 0x1518;
        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
        work->lockOnSuspended               = 1;
        work->requestedAnimId               = 8;
        work->animStart                     = ACTOR_403000_ANIM_RESTART;
        work->stateFrame                    = 0;
        work->stillFrames                   = 0;
        work->animRate                      = 0;
        func_actor_403000_80133AF8(arg0);
    }
    if (work->stateFrame > 0x3C) {
        work->dropDelay = 0x14;
        sound           = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401E0010;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->state = ACTOR_403000_STATE_DROP;
    }
    if (work->stateFrame > 0x64) {
        if (work->lastRootPos.vx == arg0->extra.tmd->coords->coord.t[0] &&
            work->lastRootPos.vy == arg0->extra.tmd->coords->coord.t[1] &&
            work->lastRootPos.vz == arg0->extra.tmd->coords->coord.t[2]) {
            work->stillFrames++;
        } else {
            work->stillFrames = 0;
        }
        if (work->stillFrames > 0x1E) {
            work->dropDelay = 0x14;
            sound2          = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401E0010;
            pan2            = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->state = ACTOR_403000_STATE_DROP;
        }
        work->lastRootPos.vx = arg0->extra.tmd->coords->coord.t[0];
        work->lastRootPos.vy = arg0->extra.tmd->coords->coord.t[1];
        work->lastRootPos.vz = arg0->extra.tmd->coords->coord.t[2];
    }
    work->stateFrame++;
}

/// Seek the next waypoint: on the entry frame restart the animation and latch
/// the steering from `func_actor_403000_80134204`; every frame compare the
/// player's and the actor's grid cells, turn at most 8 units toward the chosen
/// waypoint and step forward.
static void func_actor_403000_8013C2D4(Task* arg0)
{
    Actor403000Work*             work;
    Task*                        player;
    _Actor403000WaypointScratch* scratch;
    TmdObject*                   obj;
    GfxCoord*                    coord;
    SVECTOR*                     table;
    SVECTOR*                     v;
    s16                          angle;
    s32                          mag;
    s8                           base;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403000WaypointScratch);
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        work->lockOnSuspended = 0;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->torsoSphere.body.radius = 0x3E8;
        work->animStart               = ACTOR_403000_ANIM_BLEND_IN;
        work->animRate                = 0x10;
        work->overlayActive           = 0;
        work->requestedAnimId         = 1;
        work->forelegYawTarget        = 0;
        work->rootSphere.body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->stateFrame                   = 0;
        work->stillFrames                  = 0;
        work->field_FC2                    = 0;
        work->rootCapsule.shape.ends[1].vz = 0x320;
        scratch->playerCell                = Actor403000_Cell(player->extra.tmd->coords);
        scratch->cell                      = Actor403000_Cell(arg0->extra.tmd->coords);
        work->seekRingDir                  = func_actor_403000_80134204(arg0->extra.tmd->coords);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->rootSphere.contacts, ARRAY_SIZE(work->rootSphere.contacts));
    scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
    base                = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->cell       = base;
    if (func_actor_403000_80133FC0(arg0, base, scratch->playerCell) << 16) {
        work->state = ACTOR_403000_STATE_CHASE;
    }
    scratch->waypoint = scratch->cell + work->seekRingDir;
    if (scratch->waypoint != -1) {
        if (scratch->waypoint == ACTOR_403000_RING_WAYPOINT_COUNT) {
            scratch->waypoint = 0;
        }
    } else {
        scratch->waypoint = ACTOR_403000_RING_WAYPOINT_COUNT - 1;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->waypoint];
    scratch->offset.vx  = v->vx;
    scratch->offset.vy  = v->vy;
    scratch->offset.vz  = v->vz;
    scratch->offset.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->offset.vy  = 0;
    scratch->offset.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(scratch->offset.vx, scratch->offset.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
        while (1) {
            if (angle < -0x800) {
                angle += 0x1000;
                continue;
            }
            break;
        }
    } else {
        while (1) {
            if (angle > 0x800) {
                angle -= 0x1000;
                continue;
            }
            break;
        }
    }
    mag                 = angle;
    scratch->turn       = mag;
    work->neckYawTarget = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->turn > 8) {
        scratch->turn = 8;
    }
    if (scratch->turn < -8) {
        scratch->turn = -8;
    }
    scratch->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->turn, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x16);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403000WaypointScratch);
}

static const _Actor403000StateTable D_actor_403000_80131F44 = {
    {
        func_actor_403000_8013D5F8,
        func_actor_403000_8013D72C,
        func_actor_403000_8013A678,
        func_actor_403000_8013ACBC,
        func_actor_403000_8013B238,
        func_actor_403000_80137084,
        func_actor_403000_801384E8,
        func_actor_403000_801377C8,
        func_actor_403000_801386E8,
        func_actor_403000_80138DB0,
        func_actor_403000_8013A08C,
        func_actor_403000_80139AE0,
        func_actor_403000_8013B74C,
        func_actor_403000_8013BDE0,
        func_actor_403000_8013C2D4,
        func_actor_403000_8013C050,
        func_actor_403000_80135F08,
        func_actor_403000_801399A0,
        func_actor_403000_8013D910,
        func_actor_403000_8013D850,
        func_actor_403000_8013603C,
        func_actor_403000_80136B14,
        func_actor_403000_801365D0,
        func_actor_403000_80136D68,
        func_actor_403000_8013D648,
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
    Gp_UpdateActorColor(arg0, (VECTOR*)&pos, 0, 0);
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
        if (func_800E0308(&scratch->sightOrigin, &scratch->sightTarget) != 1) {
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
                        Gp_SpawnPadLerp(0xC, 0xFF, 0x80);
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
                        Gp_SpawnPadLerp(0xC, 0x58, 0xFF);
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
                        Gp_SpawnPadLerp(0xC, 0x58, 0xFF);
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
        func_actor_403000_801343B8,
        func_actor_403000_8013C864,
        enemyDestroy,
    },
};

s32 func_actor_403000_8013D260(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// Handler for message 0x7D5 in the actor's message table. `arg2` picks the
/// display mode: 0 hides the model (flag 0x80) and 1 shows it again, both
/// re-running `tmdAllocPrimitiveBuffer`; 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on top of the current flags
/// and 3 replaces them with just `TMD_OBJECT_SKIP_AUTO_BUFFER`. Every mode but 1 sets
/// `state` to `ACTOR_403000_STATE_HIDDEN`. `arg1` is unused.
s32 func_actor_403000_8013D268(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    Actor403000Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_403000_STATE_HIDDEN;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_403000_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_403000_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
s32 func_actor_403000_8013D324(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (((Enemy*)task->spawnArg2.pointer)->hp > 0) {
        goto return_one;
    }

    if ((task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }

return_one:
    return 1;
}

/// Handler for message 0x7D4: place the model's root coordinate at
/// `placement` - translation, then rotation about Y, X and Z - and cache the
/// resulting heading in `Actor403000Work::placementYaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*        coord;
    Actor403000Work* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 1);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    work->placementYaw                    = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    return 1;
}

s32 func_actor_403000_8013D464(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor403000Work* work = task->work;

    work->requestedAnimId = msg->animationId;
    work->state           = ACTOR_403000_STATE_SCRIPTED;
    work->prevState       = -1;
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

static void func_actor_403000_8013D4F4(Task* task)
{
    Actor403000Work* work  = task->work;
    Enemy*           enemy = (Enemy*)task->spawnArg2.pointer;

    if (work != NULL) {
        worldCollisionUnlinkBody(&work->torsoSphere.body);
        worldCollisionUnlinkBody(&work->hindSphere.body);
        worldCollisionUnlinkBody(&work->neckSphere.body);
        worldCollisionUnlinkBody(&work->rootSphere.body);
        enemy->recs = 0;
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

static void func_actor_403000_8013D5F8(Task* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    Enemy*           enemy;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                          = arg0->extra.tmd;
        enemy                        = arg0->spawnArg2.pointer;
        work->lockOnSuspended        = 1;
        obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->rootSphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                    = 0;
    }
}

static void func_actor_403000_8013D648(Task* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                          = arg0->extra.tmd;
        work->lockOnSuspended        = 0;
        obj->flags                   = 0;
        work->animRate               = 0x30;
        work->requestedAnimId        = 0xF;
        work->animStart              = ACTOR_403000_ANIM_RESTART;
        work->stateFrame             = 0;
        work->rootSphere.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    work->stateFrame++;
    func_actor_403000_80133AF8(arg0);
    if ((work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) || work->stateFrame >= 5) {
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
        func_actor_403000_80133AF8(arg0);
        return;
    }
    func_actor_403000_80133AF8(arg0);
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
        func_actor_403000_80133AF8(arg0);
        work->stateFrame = 0;
    }
    work->stateFrame++;
    func_actor_403000_80133AF8(arg0);
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
