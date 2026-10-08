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
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
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
#include "main/gameflag.h"
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

#include "rooms/neo_ark_bridge.h"

#include "rooms/neo_ark_island.h"

#include "rooms/neo_ark_pavilion.h"

#include "rooms/neo_ark_submarine_gallery.h"

#include "rooms/neo_ark_submarine_tunnel.h"

#include "rooms/shelter_b2_main_corridor.h"

#include "rooms/shelter_b2_septic_tank.h"

#include "rooms/shelter_b4_lower_sewer.h"

#include "rooms/shelter_b4_reservoir.h"

#include "rooms/shelter_b4_upper_sewer.h"
#include "../../shared/limb_shadows.h"
#include "../../shared/coord_math.h"
#include "../../shared/diver.h"

/// Handlers of the diver's states in water, stored as a value for whole-table
/// copies.
///
/// `_Actor00400Work::state` is the index: an `ACTOR_00400_SWIM_STATE_*` or one
/// of the three `ACTOR_00400_STATE_*` values the stranded table shares. The
/// water tick copies the package's one table to the stack and calls the
/// current state's handler with the diver's task once a frame; there is no
/// terminator or bounds check, and every slot is filled.
typedef struct {
    TaskFunc funcs[15]; // Handlers in state order
} _Actor00400SwimStateTable;
STATIC_ASSERT_SIZEOF(_Actor00400SwimStateTable, 0x3C);

/// Half the side of the ground stain, which is a square centred on the root.
#define ACTOR_00400_GROUND_STAIN_HALF_SIZE 0x5DC

/// Work block of the stain on the ground under the diver that spawns wounded
/// and lying out of the water.
///
/// The spawn state gives the stain a task of its own and fixes the square at
/// the root's position and height. The task draws it every frame: the blob
/// texture the limb shadows use, subtracted from the picture with red at half
/// strength, which leaves a red patch. It is drawn at full strength while the
/// diver lives and then fades by one step a frame, and the task ends when it
/// has faded out.
typedef struct {
    Enemy*  enemy;       // the diver's enemy record; the stain starts to fade once its `hp` is 0 or less
    SVECTOR vertices[4]; // corners in the view coordinate's space, in the order the quad is drawn; `pad` is never written
    u8      intensity;   // strength of the subtraction: 0xFF from the spawn, less 1 a frame while fading, and 0 ends the task
} _Actor00400GroundStainWork;
STATIC_ASSERT_SIZEOF(_Actor00400GroundStainWork, 0x28);

/// Vertical speed the shot leaves with; negative is up.
#define ACTOR_00400_SHOT_LAUNCH_SPEED_Y (-0x14)

/// Added to the shot's vertical speed every frame.
#define ACTOR_00400_SHOT_GRAVITY 2

/// Frame count at which a shot still flying bursts by itself.
#define ACTOR_00400_SHOT_LIFETIME 0x3D

/// Billboard/flash size 768, with one extra size unit for the room particle.
enum { ACTOR_00400_SHOT_BURST_SIZE_AND_SPRAY_BIAS = 0x1300 };

/// Work block of the shot the attack in water fires.
///
/// The shot is a task of its own with a coordinate for a body, parented to the
/// view coordinate. The attack places it at part 5 of the model and aims it
/// along the head; from then on it moves by `velocity` every frame, falling
/// under `ACTOR_00400_SHOT_GRAVITY`, and trails sparks and spray. It bursts
/// when its sphere touches a body or the room, when the room asks the divers
/// to hide, or when it has flown `ACTOR_00400_SHOT_LIFETIME` frames, and the Diver
/// library's teardown then unlinks the sphere and ends the task.
typedef struct {
    DiverStrikeWork       child;       // head the Diver library's teardown reads: the sphere carrying the attack, linked while the shot flies
    WorldCollisionContact contacts[2]; // contacts of the sphere: what the shot touched this frame
    SVECTOR               velocity;    // movement a frame in the view coordinate's space: the head's forward axis times the speed of the enemy's placement row, with `vy` relaunched at `ACTOR_00400_SHOT_LAUNCH_SPEED_Y`; `pad` is never accessed
    s32                   frames;      // frames the shot has flown; the phase of its sparks and spray
} _Actor00400ShotWork;
STATIC_ASSERT_SIZEOF(_Actor00400ShotWork, 0x64);

/// Scratch-stack block of the ground stain's drawer: `RotTransPers4`'s outputs
/// for the four corners it projects through the view.
///
/// The drawer reserves one block and releases it once the quad is queued;
/// nothing in it outlives the call.
typedef struct {
    long screenCorners[4]; // projected corners: screen X in bits 0..15, Y in bits 16..31, copied whole into the primitive
    long depthCue;         // depth-cueing interpolation value of the projection; never read
    long flags;            // GTE FLAG word of the projection; a set bit 31 drops the quad
    s32  depth;            // last corner's screen Z / 4, which picks the ordering-table entry
} _Actor00400GroundStainScratch;
STATIC_ASSERT_SIZEOF(_Actor00400GroundStainScratch, 0x1C);

/// Value of `bestDistance` in `_Actor00400NearestSurfaceSpotScratch` before the
/// search has taken a spot. Only a strictly smaller distance replaces it.
#define ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE 0x7FFFFFFF

/// Scratch-stack block of the search for the surface spot nearest the diver's
/// target.
///
/// Each eligible entry of `_Actor00400Work::surfaceSpots` is measured against
/// `targetPos` on the XZ plane, from entry 1 up to the terminator. A claimed
/// entry is eligible only when it is this diver's own. `nearestIndex` stays 0,
/// the entry that is never searched, when no entry was eligible.
typedef struct {
    VECTOR delta;        // `targetPos` minus the spot under test; only `vx` and `vz` are stored, and nothing reads them back
    s32    bestDistance; // smallest `distance` taken so far, or `ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE`
    s32    distance;     // XZ distance from the spot under test to `targetPos`
    s16    spotIndex;    // index of the spot under test
    s16    nearestIndex; // index of the spot `bestDistance` was measured at; the result of the search that claims a spot
} _Actor00400NearestSurfaceSpotScratch;
STATIC_ASSERT_SIZEOF(_Actor00400NearestSurfaceSpotScratch, 0x1C);

/// Values of `_Actor00400Work::hitReaction`, the reaction a hit asks for.
enum {
    ACTOR_00400_HIT_REACTION_NONE   = 0,
    ACTOR_00400_HIT_REACTION_LIGHT  = 1, // light recoil
    ACTOR_00400_HIT_REACTION_HEAVY  = 2, // heavy recoil
    ACTOR_00400_HIT_REACTION_STATUS = 3, // held until the status buildup runs out
    ACTOR_00400_HIT_REACTION_BLAST  = 4, // heavy recoil; a blast that kills bursts the body
    ACTOR_00400_HIT_REACTION_FLINCH = 5  // no state change: raises the combat alert and shakes the root
};

/// Values of `_Actor00400Work::command`: what the room asks of the diver.
///
/// The first five are cues the scripted introductions wait on, each read by
/// the one step that expects it; the last acts on arrival.
enum {
    ACTOR_00400_COMMAND_TUNNEL_SWIM   = 1, // tunnel introduction: swim ahead for 48 frames
    ACTOR_00400_COMMAND_TUNNEL_PATROL = 2, // tunnel introduction: start circling the waypoints
    ACTOR_00400_COMMAND_INTRO_SWIM    = 3, // room introduction: swim ahead for 48 frames
    ACTOR_00400_COMMAND_INTRO_SURFACE = 4, // room introduction: come up and discharge
    ACTOR_00400_COMMAND_FIGHT         = 5, // introduction over: become a target and start fighting
    ACTOR_00400_COMMAND_STRAND        = 6  // carry on out of the water, in the stranded states
};

/// Values of `_Actor00400Work::neckPhase`.
enum {
    ACTOR_00400_NECK_FREE       = 0, // the animation poses the neck
    ACTOR_00400_NECK_STRAIGHTEN = 1, // the captured neck angles ease to zero
    ACTOR_00400_NECK_RETRACTED  = 2  // the neck is straight and `neckScale` shortens it
};

/// Values of `_Actor00400Work::state` both living state tables give the same
/// meaning, so that one hit handler serves the swimming and the stranded diver.
enum {
    ACTOR_00400_STATE_RECOIL_LIGHT = 7, // plays the light recoil, then decides
    ACTOR_00400_STATE_RECOIL_HEAVY = 8, // plays the heavy recoil, then decides
    ACTOR_00400_STATE_STATUS_HOLD  = 9  // lies stunned until the status buildup runs out
};

/// Values of `_Actor00400Work::state` while the diver is in water (task state
/// 3). 5 and 6 have empty handlers and are never selected.
enum {
    ACTOR_00400_SWIM_STATE_START         = 0x0, // draws the frame counters and starts the patrol
    ACTOR_00400_SWIM_STATE_PATROL        = 0x1, // swims round `waypoints`, breaching at each, until a target shows or the alert is up
    ACTOR_00400_SWIM_STATE_DECIDE        = 0x2, // picks the attack or the dive
    ACTOR_00400_SWIM_STATE_EMERGE        = 0x3, // comes up at `emergePos`, then attacks or looks for a nearer spot
    ACTOR_00400_SWIM_STATE_DIVE          = 0x4, // goes under and swims to `surfaceSpot`
    ACTOR_00400_SWIM_STATE_ATTACK        = 0xA, // discharges, then fires the shot
    ACTOR_00400_SWIM_STATE_TUNNEL_PATROL = 0xB, // tunnel introduction: circles `waypoints`, never a target
    ACTOR_00400_SWIM_STATE_TUNNEL_INTRO  = 0xC, // tunnel introduction: swims past on the room's cues
    ACTOR_00400_SWIM_STATE_ROOM_INTRO    = 0xD, // room introduction: swims in, surfaces and discharges on the room's cues
    ACTOR_00400_SWIM_STATE_AWAIT_FIGHT   = 0xE  // waits for `ACTOR_00400_COMMAND_FIGHT`
};

/// Values of `_Actor00400Work::state` while the diver is stranded (task state
/// 1). 6 has an empty handler; neither it nor `SETTLE` is ever selected.
enum {
    ACTOR_00400_STRANDED_STATE_START     = 0, // becomes a target and starts the idle animation
    ACTOR_00400_STRANDED_STATE_WAIT      = 1, // idles until a target comes near in front, or a hit lands
    ACTOR_00400_STRANDED_STATE_DECIDE    = 2, // picks the discharge in reach, the crawl otherwise
    ACTOR_00400_STRANDED_STATE_SETTLE    = 3, // plays animation 6, then decides
    ACTOR_00400_STRANDED_STATE_CRAWL     = 4, // drags itself toward the target
    ACTOR_00400_STRANDED_STATE_DISCHARGE = 5  // sets off the spark discharge around its trunk, which `attackBody` carries
};

/// Values of `_Actor00400Work::state` during the death in water (task state 4).
enum {
    ACTOR_00400_SWIM_DEATH_START       = 0, // unlinks the bodies; a blast skips to `BLAST_HIDE`
    ACTOR_00400_SWIM_DEATH_FALL        = 1, // plays the death animation out
    ACTOR_00400_SWIM_DEATH_REST        = 2, // 60 frames
    ACTOR_00400_SWIM_DEATH_WEIGH       = 3, // switches to the weighted colours
    ACTOR_00400_SWIM_DEATH_FADE        = 4, // 24 frames, then draws translucent
    ACTOR_00400_SWIM_DEATH_DARKEN      = 5, // turns black on frame 16 and ends after 32
    ACTOR_00400_SWIM_DEATH_HIDE        = 6, // hides the model and despawns
    ACTOR_00400_SWIM_DEATH_BLAST_HIDE  = 7, // hides the model
    ACTOR_00400_SWIM_DEATH_BLAST_WAIT  = 8, // two frames
    ACTOR_00400_SWIM_DEATH_BLAST_BURST = 9, // frees the model's buffers and throws the body chunks
    ACTOR_00400_SWIM_DEATH_BLAST_END   = 10 // despawns
};

/// Values of `_Actor00400Work::state` during the stranded death (task state 2).
enum {
    ACTOR_00400_STRANDED_DEATH_START       = 0, // unlinks the bodies; a blast skips to `BLAST_HIDE`
    ACTOR_00400_STRANDED_DEATH_FALL        = 1, // 30 frames of the death animation
    ACTOR_00400_STRANDED_DEATH_WEIGH       = 2, // saves the root matrix and switches to the weighted colours
    ACTOR_00400_STRANDED_DEATH_FADE        = 3, // 24 frames, then draws translucent
    ACTOR_00400_STRANDED_DEATH_SHRINK      = 4, // flattens and burns away; hidden once done
    ACTOR_00400_STRANDED_DEATH_END         = 5, // despawns
    ACTOR_00400_STRANDED_DEATH_BLAST_HIDE  = 6, // hides the model
    ACTOR_00400_STRANDED_DEATH_BLAST_WAIT  = 7, // two frames
    ACTOR_00400_STRANDED_DEATH_BLAST_BURST = 8, // frees the model's buffers and throws the body chunks
    ACTOR_00400_STRANDED_DEATH_BLAST_END   = 9  // despawns
};

/// Work block of the Bog Diver task.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation playback and its storage, the four collision spheres with
/// their contact records, storage for the model's matrices, what the room
/// lends the diver - a patrol ring, the spots to emerge at and the water
/// level - and the state machine.
///
/// The task state picks a table of states, `state` an entry of that table and
/// `subState` a step of that entry's own table. Entering a task state clears
/// both and selecting a state clears `subState`. The task states are the spawn
/// (0), the stranded diver (1) and its death (2), the diver in water (3) and
/// its death (4), the despawn (5), and the two wounded spawns, lying on the
/// ground (6) and floating (7); the last three each have a table of two.
///
/// Angles are 4096ths of a turn, and positions are in the root coordinate's
/// parent space, which is the view's. Model parts are numbered as the
/// skeleton's coordinates: 0 the root, 1 the trunk, 2 and 3 the neck, 4 the
/// head, 11 and 14 the tips of the arms.
typedef struct {
    ActorAnimRig15        rig;               // animation playback of the model, one slot per part; 1..14 play `animClip`, and slot 1's status tells when it ended
    WorldCollisionBody    trunkBody;         // sphere on part 1 that other bodies touch; its contacts carry the hits
    WorldCollisionBody    headBody;          // smaller sphere on part 4; shares `hitContacts`
    WorldCollisionContact hitContacts[6];    // contacts of `trunkBody` and `headBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // sphere on the root, tested against the room grid while `gridCollision`
    WorldCollisionContact gridContacts[6];   // contacts of `gridBody`; their push-out moves the root
    WorldCollisionBody    attackBody;        // sphere on part 1 carrying the enemy's attack key; enabled only by a discharge out of the water
    WorldCollisionContact attackContacts[3]; // contacts of `attackBody`
    byte                  field_544[0x2];    // never accessed
    u16                   lookYaw;           // stranded: signed yaw of the look toward the target, spread over parts 2, 3 and 4 a third each
    byte                  field_548[0x4];    // never accessed
    SVECTOR               prevRootPos;       // root position at the start of the frame; its XZ is restored when the collision step reports a conflict
    SVECTOR               rotation;          // root rotation: `vy` heading, `vz` roll; `vx` is only ever cleared and never applied
    byte                  field_55C[0x8];    // never accessed
    SVECTOR               armAnchor;         // XZ the midpoint of the arm tips is held at through a crawl stride; moved with every push-out
    SVECTOR               surfaceSpot;       // XZ of the claimed entry of `surfaceSpots`; `vy` stays 0
    SVECTOR               emergePos;         // `surfaceSpot` as the emerge began; the root eases onto its XZ
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    MATRIX                savedRootMtx;      // root matrix at the start of the stranded death's shrink; each frame rescales a copy of it
    EffectSpawnArg        effectArg;         // argument record of the hit and discharge effects, hung off part 1
    SVECTOR               targetPos;         // root position of the nearer of the player and the companion
    SVECTOR               lowerNeckAngles;   // Euler angles of part 2 as the neck retracted, eased to zero; eased back to the animation's on release
    SVECTOR               upperNeckAngles;   // the same for part 3
    byte                  field_5FC[0xC];    // never accessed
    SVECTOR*              surfaceSpots;      // the room's spots to emerge at, shared by its divers, or NULL: `pad` is 1 on a claimed entry and -1 ends the list; entry 0 is never searched
    SVECTOR*              waypoints;         // ring of eight points the patrol swims round, picked from the room's sets by the spawn argument; `vy` is the depth swum at
    s32                   critChanceScale;   // factor on the critical chance of the hits taken: 100 during a status hold, 0 for none
    s16                   stateHistory[3];   // ring of the states the last three decisions chose; three attacks or discharges running force the other choice
    byte                  field_61A[0x2];    // never accessed
    s16                   hitCooldown;       // frames before another hit is taken; set from the hit's id parameter 2
    s16                   shrinkScaleY;      // Y scale of the stranded death's shrink, 0x1000 = 1.0, less 0x40 a frame
    s16                   field_620;         // 0x1000 from the spawn; never read, role unproven
    s16                   field_622;         // 0x1000 from the spawn; never read, role unproven
    s16                   animRequest;       // `DIVER_ANIM_REQUEST_*`
    s16                   animPlaying;       // animation last applied to the slots
    s16                   animClip;          // requested animation: index into the package's animation bank
    s16                   animFrames;        // frames since `animClip` was applied; rescaled to the new rate when a blend re-requests the playing animation
    u16                   animStatus;        // slot 1's ANIMATION_SLOT_* results of the last frame's ticks; the states test this copy to learn their clip ended
    s16                   bobPhase;          // frame counter driving the root coordinate's vertical bob (64 frames a cycle, 16 units either way)
    s16                   frameCount;        // frames the diver has run, from a random start in water and from 0x174B stranded; paces the swim sound and is the phase of the flinch shake
    s16                   animStep;          // playback rate of slots 1..14; `ANIMATION_RATE_ONE` is normal speed
    u16                   targetBearing;     // heading from part 1 to `targetPos` relative to `rotation.vy`, 0..0xFFF
    s16                   stateFrames;       // frames spent in the current state or step
    s16                   state;             // index into the state table of the current task state: `ACTOR_00400_*_STATE_*`, `ACTOR_00400_*_DEATH_*`
    s16                   subState;          // index into the step table of the current state
    s16                   animBlend;         // frames a blend request takes; cleared once a different animation has been blended into
    s16                   goalY;             // Y the root eases a sixteenth of the way to each frame while in water
    s16                   targetDistance;    // horizontal distance from part 1 to `targetPos`
    s16                   hitTaken;          // 1 when a hit or a status tick dealt damage this frame; lets `hitReaction` be consumed
    s16                   hitReaction;       // `ACTOR_00400_HIT_REACTION_*` awaiting the state machine
    s16                   attackFrames;      // frames left of a spark discharge, 24 from its start; paces its sound and sparks, and out of the water enables `attackBody` for the last 22
    s16                   shadowShade;       // shade of the limb shadows during the stranded death: 0x80, less 7 a frame of the shrink
    s16                   surfaceSpotIndex;  // index of the claimed entry of `surfaceSpots`; 0 until the first claim
    s16                   emergeCooldown;    // frames before a dive may end in an emerge: 90 after an attack was declined or chosen three times running
    s16                   waterLevel;        // Y of the room's water surface, from the room's area row; 0 where the row gives none
    s16                   shakeFrames;       // frames left of the flinch shake, 10 from each flinch
    s16                   neckPhase;         // `ACTOR_00400_NECK_*`
    s16                   neckScale;         // Z scale of part 2, 0x1000 = 1.0: eased to 0x2AA while retracted and back on release; the head takes the inverse
    byte                  field_656[0x2];    // never accessed
    u16                   floatOffset;       // added to the goal height of the floating wounded spawn and of the death in water: 200 for that spawn, 0 otherwise
    u8                    stateHistoryIndex; // entry of `stateHistory` the next decision writes
    u8                    waypointIndex;     // entry of `waypoints` being swum to, 0..7
    u8                    strideCount;       // crawl strides begun; its low bit alternates the stride's two sounds
    u8                    wasHit;            // set by every hit that deals damage; never read
    u8                    command;           // last `ACTOR_00400_COMMAND_*` received; never cleared
    u8                    ambientOff;        // 1 on the tunnel introduction's diver: every colour update is followed by zeroing the model's background colour
    u8                    neckRetracted;     // 1 asks for the neck drawn in, as it is under water; 0 releases it
    u8                    gridCollision;     // 1 has `gridBody` tested against the room grid; read once, as the bodies are linked, by when spawn kind 0 and the area rows that ask for it have set it
    byte                  field_662[0x1];    // never accessed
    u8                    suspended;         // 1 while the room has the model hidden: the living and dying states do not run
    u8                    targetPart;        // part the enemy's target point and hit effects hang off: 4, or 1 during the status hold in water and on the wounded spawns
    u8                    lookDisabled;      // 1 during the stranded status hold: `lookYaw` only decays
    u8                    inWater;           // 1 in water, 0 stranded or lying wounded
} _Actor00400Work;
STATIC_ASSERT_SIZEOF(_Actor00400Work, 0x668);

/// The Diver library's name for this package's work block (see diver.h).
typedef _Actor00400Work DiverWork;

/// Value of `_Actor00400AreaConfig::stage` on the row that ends the table.
#define ACTOR_00400_AREA_CONFIG_END 0xFF

/// Bits of `_Actor00400AreaConfig::flags`.
enum {
    ACTOR_00400_AREA_CONFIG_NO_DIVER       = 0x1, // the room takes no diver: the spawn destroys the enemy; no row of the package sets it
    ACTOR_00400_AREA_CONFIG_GRID_COLLISION = 0x2  // sets `_Actor00400Work::gridCollision`
};

/// What one room lends the divers that spawn in it.
///
/// The package holds one row for each room a diver appears in, ended by a row
/// whose `stage` is `ACTOR_00400_AREA_CONFIG_END`. The spawn state takes the
/// row of the session's current stage and area, and destroys the enemy when
/// there is none. Each pointer names data of that room's package, or is NULL
/// to leave the work block's member as the spawn found it.
typedef struct {
    SVECTOR** waypointSets; // the room's four patrol rings, picked by bits 4..5 of the spawn argument: becomes `_Actor00400Work::waypoints`
    SVECTOR*  surfaceSpots; // the room's spots to emerge at: becomes `_Actor00400Work::surfaceSpots`
    s16*      waterLevel;   // Y of the room's water surface, read once at the spawn: seeds `_Actor00400Work::waterLevel`
    s16       stage;        // `GAME_STAGE_*` of the room, or `ACTOR_00400_AREA_CONFIG_END`
    s16       area;         // `GAME_AREA_*` of the room within `stage`
    u16       flags;        // `ACTOR_00400_AREA_CONFIG_*`
} _Actor00400AreaConfig;
STATIC_ASSERT_SIZEOF(_Actor00400AreaConfig, 0x14);

/// Clip indices used by the water status hold and the scripted introductions.
enum {
    ACTOR_00400_ANIM_SURFACED               = 1,
    ACTOR_00400_ANIM_SWIM                   = 3,
    ACTOR_00400_ANIM_DISCHARGE              = 7,
    ACTOR_00400_ANIM_SWIM_STATUS_HOLD_ENTER = 14
};

/// Clip indices used by the crawl, recoil and swimming status hold.
enum {
    ACTOR_00400_ANIM_CRAWL                 = 4,
    ACTOR_00400_ANIM_SWIM_RECOIL_LIGHT     = 10,
    ACTOR_00400_ANIM_STRANDED_RECOIL_LIGHT = 12,
    ACTOR_00400_ANIM_STRANDED_RECOIL_HEAVY = 13,
    ACTOR_00400_ANIM_SWIM_STATUS_HOLD_LOOP = 16,
    ACTOR_00400_ANIM_SWIM_STATUS_HOLD_HIT  = 18
};

/// Clip indices used by stranded state 3, discharge and status hold.
enum {
    ACTOR_00400_ANIM_STRANDED_STATE3            = 6,
    ACTOR_00400_ANIM_STRANDED_DISCHARGE         = 9,
    ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_ENTER = 15,
    ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_LOOP  = 17
};

/// Horizontal target distance below which a crawl returns to the decision state.
enum { ACTOR_00400_STRANDED_CRAWL_STOP_DISTANCE = 948 };

/// Placement-tagged sound scripts used by these movement and hit handlers.
enum {
    ACTOR_00400_SOUND_SWIM        = 0x40040001,
    ACTOR_00400_SOUND_STRIDE_ODD  = 0x40040002,
    ACTOR_00400_SOUND_STRIDE_EVEN = 0x40040003,
    ACTOR_00400_SOUND_HIT         = 0x40040006
};

/// Sound-script instance tag: the diver's placement index occupies bits 8..15.
enum { ACTOR_00400_SOUND_INSTANCE_SHIFT = 8 };

/// Scripted room-introduction positions in the root's view-parent space.
enum {
    ACTOR_00400_ROOM_INTRO_X       = -0x6C0,
    ACTOR_00400_ROOM_INTRO_START_Z = -0xBB8,
    ACTOR_00400_ROOM_INTRO_STOP_Z  = -0x2008,
    ACTOR_00400_ROOM_INTRO_DEPTH   = 0x3E8
};

/// Spawn argument compared whole by the swimming death handlers.
enum { ACTOR_00400_SPAWN_WOUNDED_FLOAT = 7 };

/// Shared idle index of the lying and floating wounded state tables.
enum { ACTOR_00400_WOUNDED_STATE_IDLE = 0 };

/// Scale of the wounded float's Q12 sine: Q10 reduction gives 64 units,
/// Q9 reduction gives 128 units.
enum { ACTOR_00400_WOUNDED_FLOAT_BOB_SCALE = 16 };

/// Outer task states selected or tested by the command, visibility and death handlers.
enum {
    ACTOR_00400_TASK_STRANDED       = 1,
    ACTOR_00400_TASK_STRANDED_DEATH = 2,
    ACTOR_00400_TASK_SWIM_DEATH     = 4,
    ACTOR_00400_TASK_DESPAWN        = 5
};

/// Entry of the two-step despawn table that releases the room's surface-spot claim.
enum { ACTOR_00400_DESPAWN_STATE_RELEASE_SPOT = 0 };

/// Placement-tagged sound scripts for the surface cycle and discharge.
enum {
    ACTOR_00400_SOUND_SURFACE_CUE  = 0x40040004,
    ACTOR_00400_SOUND_NECK_RELEASE = 0x40040007,
    ACTOR_00400_SOUND_DIVE         = 0x40040008,
    ACTOR_00400_SOUND_DISCHARGE    = 0x4004000B
};

/// Normal living limb-shadow intensity, before a stranded death fades it.
enum { ACTOR_00400_LIVING_SHADOW_SHADE = 128 };

static void _actor00400TurnTowardPointMaskedRange(Task* task, const SVECTOR* target, s32 yawStep, s32 deadband);
static void _actor00400RequestClipBlend(Task* task, s16 clipIndex, s16 rate, s16 blendFrames);
static void _actor00400UpdateNeckRetraction(Task* task, s32 unusedNeckRetracted);
static void _actor00400ClaimNearestSurfaceSpot(Task* task);
static void _actor00400LaunchShot(Task* task);
static void _actor00400Despawn(Task* task);
static void Actor00400_Fn03920(Task* arg0);
static void _actor00400StrandedTask(Task* task);
static void _actor00400StrandedDeathTask(Task* task);
static void _actor00400SwimTask(Task* task);
static void _actor00400SwimDeathTask(Task* task);
static void _actor00400WoundedGroundTask(Task* task);
static void _actor00400WoundedFloatTask(Task* task);
static void _actor00400BlendRequestedClip(Task* task);
static s16  _actor00400ScaleFramesForAnimRate(Task* task, s16 frames);
static void _actor00400TickAnimation(Task* task);
static s16  _actor00400ClipEnded(Task* task);
static void _actor00400CapturePartMidpointXZ(Task* task, s16 firstPartIndex, s16 secondPartIndex, SVECTOR* midpoint);
static void _actor00400AlignPartMidpointXZ(Task* task, s16 firstPartIndex, s16 secondPartIndex, const SVECTOR* anchor);
static void _actor00400SwimLightRecoilWait(Task* task);
static void _actor00400SwimHeavyRecoilWait(Task* task);
static void _actor00400WoundedGroundIdleTick(Task* task);
static void _actor00400WoundedGroundIdleEnter(Task* task);
static void _actor00400WoundedGroundIdle(Task* task);
static void _actor00400WoundedGroundFlinch(Task* task);
static void _actor00400WoundedGroundFlinchEnter(Task* task);
static void _actor00400WoundedGroundFlinchWait(Task* task);
static void _actor00400StrandedLightRecoilEnter(Task* task);
static void _actor00400StrandedLightRecoilWait(Task* task);
static void _actor00400StrandedHeavyRecoilEnter(Task* task);
static void _actor00400StrandedHeavyRecoilWait(Task* task);
static void _actor00400StrandedStatusHoldEnter(Task* task);
static void _actor00400StrandedStatusHoldTick(Task* task);
static void _actor00400ReleaseSurfaceSpot(Task* task);
static void _actor00400DespawnWait(Task* task);
static void _actor00400SwimStatusHoldEnter(Task* task);
static void _actor00400SwimStatusHoldTick(Task* task);
static s16  _actor00400ApplyHitReaction(Task* task);
static void _actor00400StrandedDecide(Task* task);
static void _actor00400InitCollisionBodies(Task* task);
static void _actor00400GetTargetAnglesFromPart(Task* task, s16 partIndex, SVECTOR* targetAngles, s16 heightOffset);
static void _actor00400CopyRotation(const MATRIX* source, MATRIX* destination);

static s32  _actor00400SwimToNextWaypoint(Task* task);
static void _actor00400StrandedState3Enter(Task* task);
static void _actor00400StrandedState3Wait(Task* task);
static void _actor00400StrandedCrawlEnter(Task* task);
static void _actor00400StrandedCrawlTick(Task* task);
static void _actor00400StrandedDischargeEnter(Task* task);
static void _actor00400StrandedDischargeWait(Task* task);
static void _actor00400AwaitFightCue(Task* task);
static void _actor00400WoundedFloatIdleEnter(Task* task);
static void _actor00400WoundedFloatFlinchEnter(Task* task);

/* States the dispatch tables name before their definitions. */
static void _actor00400SwimDeathEnter(Task* task);
static void _actor00400SwimDeathFallWait(Task* task);
static void _actor00400StrandedDeathEnter(Task* task);
static void _actor00400SwimDecide(Task* task);
static void _actor00400SwimStart(Task* task);
static void _actor00400SwimPatrol(Task* task);
static void Actor00400_Fn078C8(Task* arg0);
static void _actor00400Dive(Task* task);
static void _actor00400SwimUnusedState5(Task* task);
static void _actor00400SwimUnusedState6(Task* task);
static void _actor00400SwimLightRecoil(Task* task);
static void Actor00400_Fn079FC(Task* arg0);
static void _actor00400SwimStatusHold(Task* task);
static void _actor00400SwimAttack(Task* task);
static void _actor00400TunnelPatrol(Task* task);
static void _actor00400TunnelIntro(Task* task);
static void _actor00400SwimDeathRest(Task* task);
static void _actor00400SwimDeathBeginColorFade(Task* task);
static void _actor00400SwimDeathFade(Task* task);
static void _actor00400SwimDeathDarken(Task* task);
static void _actor00400SwimDeathHide(Task* task);
static void _actor00400SwimDeathBlastHide(Task* task);
static void _actor00400SwimDeathBlastWait(Task* task);
static void _actor00400SwimDeathBlastBurst(Task* task);
static void _actor00400SwimDeathBlastEnd(Task* task);
static void _actor00400StrandedDeathFallWait(Task* task);
static void _actor00400StrandedDeathWeigh(Task* task);
static void _actor00400StrandedDeathFadeWait(Task* task);
static void _actor00400StrandedDeathShrink(Task* task);
static void _actor00400StrandedDeathEnd(Task* task);
static void _actor00400StrandedDeathBlastHide(Task* task);
static void _actor00400StrandedDeathBlastWait(Task* task);
static void _actor00400StrandedDeathBlastBurst(Task* task);
static void _actor00400StrandedDeathBlastEnd(Task* task);
static void _actor00400StrandedStart(Task* task);
static void _actor00400StrandedIdleWait(Task* task);
static void _actor00400StrandedDecision(Task* task);
static void _actor00400StrandedState3(Task* task);
static void _actor00400StrandedCrawl(Task* task);
static void _actor00400StrandedDischarge(Task* task);
static void _actor00400StrandedUnusedState6(Task* task);
static void _actor00400StrandedLightRecoil(Task* task);
static void _actor00400StrandedHeavyRecoil(Task* task);
static void _actor00400StrandedStatusHold(Task* task);
static void _actor00400SwimPatrolEnter(Task* task);
static void _actor00400SwimPatrolTravel(Task* task);
static void _actor00400SwimPatrolWaitForClip(Task* task);
static void _actor00400DiveWaitForClip(Task* task);
static void _actor00400SwimAttackEnter(Task* task);
static void _actor00400TunnelPatrolEnter(Task* task);
static void _actor00400TunnelPatrolWaitForCue(Task* task);
static void _actor00400TunnelIntroEnter(Task* task);
static void _actor00400TunnelIntroWaitForSwim(Task* task);
static void _actor00400TunnelIntroSwim(Task* task);
static void _actor00400TunnelIntroWaitForPatrol(Task* task);
static void _actor00400RoomIntro(Task* task);
static void _actor00400AwaitFight(Task* task);
static void _actor00400RoomIntroEnter(Task* task);
static void _actor00400RoomIntroWaitForSwim(Task* task);
static void _actor00400RoomIntroSwim(Task* task);
static void _actor00400RoomIntroWaitForSurface(Task* task);
static void _actor00400RoomIntroWaitAfterDischarge(Task* task);
static void _actor00400RoomIntroWaitForFight(Task* task);

extern EnemyParams Actor00400_D0FDC8;
/// Pair table `_actor00400LaunchShot` packs, at index 1, into the `key` of the
/// shot's sphere.
extern DamageAttack          Actor00400_D0FDC0[2];
extern TaskDesc              Actor00400_D16028[];
extern _Actor00400AreaConfig Actor00400_D15F20[];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor00400_D16010[3];

extern u16           Actor00400_D1609C[8];
extern AnimationSet* Actor00400_D1604C[20];
static TmdSource     _gActor00400DiverBurstHead;
static TmdSource     _gActor00400DiverBurstArmRight;
static TmdSource     _gActor00400DiverBurstArmLeft1;
static TmdSource     _gActor00400DiverBurstLegRight;
static TmdSource     _gActor00400DiverBurstArmLeft2;
static TmdSource     _gActor00400DiverEnergyBall;
extern void*         D_800678F0[1];

/* Inline rotation traversal helpers. Every ancestor rotation is copied out
   and renormalised before it is fed to the GTE, instead of being loaded
   straight from the coordinate. */

static AnimationSet _gActor00400Actor100400Animation10348;
static AnimationSet _gActor00400Actor100400Animation105F4;
static AnimationSet _gActor00400Actor100400Animation1097C;
static AnimationSet _gActor00400Actor100400Animation11054;
static AnimationSet _gActor00400Actor100400Animation11918;
static AnimationSet _gActor00400Actor100400Animation11C64;
static AnimationSet _gActor00400Actor100400Animation1236C;
static AnimationSet _gActor00400Actor100400Animation12C50;
static AnimationSet _gActor00400Actor100400Animation132B0;
static AnimationSet _gActor00400Actor100400Animation136E8;
static AnimationSet _gActor00400Actor100400Animation13C28;
static AnimationSet _gActor00400Actor100400Animation13FB0;
static AnimationSet _gActor00400Actor100400Animation14608;
static AnimationSet _gActor00400Actor100400Animation14C08;
static AnimationSet _gActor00400Actor100400Animation15050;
static AnimationSet _gActor00400Actor100400Animation15340;
static AnimationSet _gActor00400Actor100400Animation15624;
static AnimationSet _gActor00400Actor100400Animation15B34;
static AnimationSet _gActor00400Actor100400Animation15EF8;
static TmdSource    _gActor00400DiverBody;
static void         _actor00400GroundStainTask(Task* task);
static void         _actor00400ShotTask(Task* task);
static void         _actor00400ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg);
static void         _actor00400SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void         _actor00400BodyTask(Task* task);

static TmdBone _gActor00400DiverBodySkeleton[15] = {
#include "assets/diver_body_skeleton.inc"
};

static u32 _gActor00400DiverBodyPartVerts[15] = {
#include "assets/diver_body_partVerts.inc"
};

static SVECTOR _gActor00400DiverBodyVerts[145] = {
#include "assets/diver_body_verts.inc"
};

static SVECTOR _gActor00400DiverBodyNormals[142] = {
#include "assets/diver_body_normals.inc"
};

static u32 _gActor00400DiverBodyStream[2494] = {
#include "assets/diver_body_stream.inc"
};

static TmdSource _gActor00400DiverBody = {
    0,
    11652,
    5104,
    15,
    _gActor00400DiverBodyPartVerts,
    _gActor00400DiverBodyVerts,
    _gActor00400DiverBodyNormals,
    _gActor00400DiverBodySkeleton,
    _gActor00400DiverBodyStream,
};

static TmdBone _gActor00400DiverBurstHeadSkeleton[1] = {
#include "assets/diver_burst_head_skeleton.inc"
};

static u32 _gActor00400DiverBurstHeadPartVerts[1] = {
#include "assets/diver_burst_head_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstHeadVerts[35] = {
#include "assets/diver_burst_head_verts.inc"
};

static SVECTOR _gActor00400DiverBurstHeadNormals[44] = {
#include "assets/diver_burst_head_normals.inc"
};

static u32 _gActor00400DiverBurstHeadStream[360] = {
#include "assets/diver_burst_head_stream.inc"
};

static TmdSource _gActor00400DiverBurstHead = {
    0,
    2388,
    0,
    1,
    _gActor00400DiverBurstHeadPartVerts,
    _gActor00400DiverBurstHeadVerts,
    _gActor00400DiverBurstHeadNormals,
    _gActor00400DiverBurstHeadSkeleton,
    _gActor00400DiverBurstHeadStream,
};

static TmdBone _gActor00400DiverBurstArmRightSkeleton[1] = {
#include "assets/diver_burst_arm_right_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmRightPartVerts[1] = {
#include "assets/diver_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmRightVerts[14] = {
#include "assets/diver_burst_arm_right_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmRightNormals[24] = {
#include "assets/diver_burst_arm_right_normals.inc"
};

static u32 _gActor00400DiverBurstArmRightStream[143] = {
#include "assets/diver_burst_arm_right_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmRight = {
    0,
    904,
    0,
    1,
    _gActor00400DiverBurstArmRightPartVerts,
    _gActor00400DiverBurstArmRightVerts,
    _gActor00400DiverBurstArmRightNormals,
    _gActor00400DiverBurstArmRightSkeleton,
    _gActor00400DiverBurstArmRightStream,
};

static TmdBone _gActor00400DiverBurstArmLeft1Skeleton[1] = {
#include "assets/diver_burst_arm_left_1_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmLeft1PartVerts[1] = {
#include "assets/diver_burst_arm_left_1_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft1Verts[14] = {
#include "assets/diver_burst_arm_left_1_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft1Normals[24] = {
#include "assets/diver_burst_arm_left_1_normals.inc"
};

static u32 _gActor00400DiverBurstArmLeft1Stream[143] = {
#include "assets/diver_burst_arm_left_1_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmLeft1 = {
    0,
    904,
    0,
    1,
    _gActor00400DiverBurstArmLeft1PartVerts,
    _gActor00400DiverBurstArmLeft1Verts,
    _gActor00400DiverBurstArmLeft1Normals,
    _gActor00400DiverBurstArmLeft1Skeleton,
    _gActor00400DiverBurstArmLeft1Stream,
};

static TmdBone _gActor00400DiverBurstLegRightSkeleton[1] = {
#include "assets/diver_burst_leg_right_skeleton.inc"
};

static u32 _gActor00400DiverBurstLegRightPartVerts[1] = {
#include "assets/diver_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstLegRightVerts[19] = {
#include "assets/diver_burst_leg_right_verts.inc"
};

static SVECTOR _gActor00400DiverBurstLegRightNormals[33] = {
#include "assets/diver_burst_leg_right_normals.inc"
};

static u32 _gActor00400DiverBurstLegRightStream[210] = {
#include "assets/diver_burst_leg_right_stream.inc"
};

static TmdSource _gActor00400DiverBurstLegRight = {
    0,
    1360,
    0,
    1,
    _gActor00400DiverBurstLegRightPartVerts,
    _gActor00400DiverBurstLegRightVerts,
    _gActor00400DiverBurstLegRightNormals,
    _gActor00400DiverBurstLegRightSkeleton,
    _gActor00400DiverBurstLegRightStream,
};

static TmdBone _gActor00400DiverBurstArmLeft2Skeleton[1] = {
#include "assets/diver_burst_arm_left_2_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmLeft2PartVerts[1] = {
#include "assets/diver_burst_arm_left_2_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft2Verts[19] = {
#include "assets/diver_burst_arm_left_2_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft2Normals[33] = {
#include "assets/diver_burst_arm_left_2_normals.inc"
};

static u32 _gActor00400DiverBurstArmLeft2Stream[210] = {
#include "assets/diver_burst_arm_left_2_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmLeft2 = {
    0,
    1360,
    0,
    1,
    _gActor00400DiverBurstArmLeft2PartVerts,
    _gActor00400DiverBurstArmLeft2Verts,
    _gActor00400DiverBurstArmLeft2Normals,
    _gActor00400DiverBurstArmLeft2Skeleton,
    _gActor00400DiverBurstArmLeft2Stream,
};

static TmdBone _gActor00400DiverEnergyBallSkeleton[1] = {
#include "assets/diver_energy_ball_skeleton.inc"
};

static u32 _gActor00400DiverEnergyBallPartVerts[1] = {
#include "assets/diver_energy_ball_partVerts.inc"
};

static SVECTOR _gActor00400DiverEnergyBallVerts[24] = {
#include "assets/diver_energy_ball_verts.inc"
};

static SVECTOR _gActor00400DiverEnergyBallNormals[25] = {
#include "assets/diver_energy_ball_normals.inc"
};

static u32 _gActor00400DiverEnergyBallStream[270] = {
#include "assets/diver_energy_ball_stream.inc"
};

static TmdSource _gActor00400DiverEnergyBall = {
    0,
    1760,
    0,
    1,
    _gActor00400DiverEnergyBallPartVerts,
    _gActor00400DiverEnergyBallVerts,
    _gActor00400DiverEnergyBallNormals,
    _gActor00400DiverEnergyBallSkeleton,
    _gActor00400DiverEnergyBallStream,
};

DamageAttack Actor00400_D0FDC0[2] = {
    { 18, 5 },
    { 24, 5 },
};

EnemyParams Actor00400_D0FDC8 = { Actor00400_D0FDC0, 240, 70, 88, 3, 100, 10, 100, 0 };

static AnimationPackedPose _gActor00400Actor100400Animation10348Bank1[8] = {
#include "assets/actor_100400_animation_10348_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation10348Bank4[133] = {
#include "assets/actor_100400_animation_10348_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation10348Records[183] = {
#include "assets/actor_100400_animation_10348_records.inc"
};

static u16 _gActor00400Actor100400Animation10348Indices[16] = {
#include "assets/actor_100400_animation_10348_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation10348 = {
    _gActor00400Actor100400Animation10348Records,
    _gActor00400Actor100400Animation10348Indices,
    { NULL, _gActor00400Actor100400Animation10348Bank1, NULL, NULL, _gActor00400Actor100400Animation10348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation105F4Bank1[3] = {
#include "assets/actor_100400_animation_105F4_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation105F4Bank4[49] = {
#include "assets/actor_100400_animation_105F4_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation105F4Records[95] = {
#include "assets/actor_100400_animation_105F4_records.inc"
};

static u16 _gActor00400Actor100400Animation105F4Indices[16] = {
#include "assets/actor_100400_animation_105F4_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation105F4 = {
    _gActor00400Actor100400Animation105F4Records,
    _gActor00400Actor100400Animation105F4Indices,
    { NULL, _gActor00400Actor100400Animation105F4Bank1, NULL, NULL, _gActor00400Actor100400Animation105F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation1097CBank1[3] = {
#include "assets/actor_100400_animation_1097C_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation1097CBank4[78] = {
#include "assets/actor_100400_animation_1097C_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation1097CRecords[121] = {
#include "assets/actor_100400_animation_1097C_records.inc"
};

static u16 _gActor00400Actor100400Animation1097CIndices[16] = {
#include "assets/actor_100400_animation_1097C_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation1097C = {
    _gActor00400Actor100400Animation1097CRecords,
    _gActor00400Actor100400Animation1097CIndices,
    { NULL, _gActor00400Actor100400Animation1097CBank1, NULL, NULL, _gActor00400Actor100400Animation1097CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11054Bank1[15] = {
#include "assets/actor_100400_animation_11054_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11054Bank4[150] = {
#include "assets/actor_100400_animation_11054_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11054Records[225] = {
#include "assets/actor_100400_animation_11054_records.inc"
};

static u16 _gActor00400Actor100400Animation11054Indices[16] = {
#include "assets/actor_100400_animation_11054_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11054 = {
    _gActor00400Actor100400Animation11054Records,
    _gActor00400Actor100400Animation11054Indices,
    { NULL, _gActor00400Actor100400Animation11054Bank1, NULL, NULL, _gActor00400Actor100400Animation11054Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11918Bank1[8] = {
#include "assets/actor_100400_animation_11918_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11918Bank4[174] = {
#include "assets/actor_100400_animation_11918_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11918Records[345] = {
#include "assets/actor_100400_animation_11918_records.inc"
};

static u16 _gActor00400Actor100400Animation11918Indices[16] = {
#include "assets/actor_100400_animation_11918_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11918 = {
    _gActor00400Actor100400Animation11918Records,
    _gActor00400Actor100400Animation11918Indices,
    { NULL, _gActor00400Actor100400Animation11918Bank1, NULL, NULL, _gActor00400Actor100400Animation11918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11C64Bank1[6] = {
#include "assets/actor_100400_animation_11C64_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11C64Bank4[57] = {
#include "assets/actor_100400_animation_11C64_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11C64Records[118] = {
#include "assets/actor_100400_animation_11C64_records.inc"
};

static u16 _gActor00400Actor100400Animation11C64Indices[16] = {
#include "assets/actor_100400_animation_11C64_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11C64 = {
    _gActor00400Actor100400Animation11C64Records,
    _gActor00400Actor100400Animation11C64Indices,
    { NULL, _gActor00400Actor100400Animation11C64Bank1, NULL, NULL, _gActor00400Actor100400Animation11C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation1236CBank1[10] = {
#include "assets/actor_100400_animation_1236C_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation1236CBank4[167] = {
#include "assets/actor_100400_animation_1236C_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation1236CRecords[235] = {
#include "assets/actor_100400_animation_1236C_records.inc"
};

static u16 _gActor00400Actor100400Animation1236CIndices[16] = {
#include "assets/actor_100400_animation_1236C_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation1236C = {
    _gActor00400Actor100400Animation1236CRecords,
    _gActor00400Actor100400Animation1236CIndices,
    { NULL, _gActor00400Actor100400Animation1236CBank1, NULL, NULL, _gActor00400Actor100400Animation1236CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation12C50Bank1[10] = {
#include "assets/actor_100400_animation_12C50_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation12C50Bank4[213] = {
#include "assets/actor_100400_animation_12C50_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation12C50Records[308] = {
#include "assets/actor_100400_animation_12C50_records.inc"
};

static u16 _gActor00400Actor100400Animation12C50Indices[16] = {
#include "assets/actor_100400_animation_12C50_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation12C50 = {
    _gActor00400Actor100400Animation12C50Records,
    _gActor00400Actor100400Animation12C50Indices,
    { NULL, _gActor00400Actor100400Animation12C50Bank1, NULL, NULL, _gActor00400Actor100400Animation12C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation132B0Bank1[14] = {
#include "assets/actor_100400_animation_132B0_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation132B0Bank4[147] = {
#include "assets/actor_100400_animation_132B0_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation132B0Records[201] = {
#include "assets/actor_100400_animation_132B0_records.inc"
};

static u16 _gActor00400Actor100400Animation132B0Indices[16] = {
#include "assets/actor_100400_animation_132B0_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation132B0 = {
    _gActor00400Actor100400Animation132B0Records,
    _gActor00400Actor100400Animation132B0Indices,
    { NULL, _gActor00400Actor100400Animation132B0Bank1, NULL, NULL, _gActor00400Actor100400Animation132B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation136E8Bank1[6] = {
#include "assets/actor_100400_animation_136E8_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation136E8Bank4[103] = {
#include "assets/actor_100400_animation_136E8_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation136E8Records[131] = {
#include "assets/actor_100400_animation_136E8_records.inc"
};

static u16 _gActor00400Actor100400Animation136E8Indices[16] = {
#include "assets/actor_100400_animation_136E8_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation136E8 = {
    _gActor00400Actor100400Animation136E8Records,
    _gActor00400Actor100400Animation136E8Indices,
    { NULL, _gActor00400Actor100400Animation136E8Bank1, NULL, NULL, _gActor00400Actor100400Animation136E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation13C28Bank1[15] = {
#include "assets/actor_100400_animation_13C28_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation13C28Bank4[109] = {
#include "assets/actor_100400_animation_13C28_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation13C28Records[164] = {
#include "assets/actor_100400_animation_13C28_records.inc"
};

static u16 _gActor00400Actor100400Animation13C28Indices[16] = {
#include "assets/actor_100400_animation_13C28_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation13C28 = {
    _gActor00400Actor100400Animation13C28Records,
    _gActor00400Actor100400Animation13C28Indices,
    { NULL, _gActor00400Actor100400Animation13C28Bank1, NULL, NULL, _gActor00400Actor100400Animation13C28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation13FB0Bank1[6] = {
#include "assets/actor_100400_animation_13FB0_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation13FB0Bank4[78] = {
#include "assets/actor_100400_animation_13FB0_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation13FB0Records[112] = {
#include "assets/actor_100400_animation_13FB0_records.inc"
};

static u16 _gActor00400Actor100400Animation13FB0Indices[16] = {
#include "assets/actor_100400_animation_13FB0_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation13FB0 = {
    _gActor00400Actor100400Animation13FB0Records,
    _gActor00400Actor100400Animation13FB0Indices,
    { NULL, _gActor00400Actor100400Animation13FB0Bank1, NULL, NULL, _gActor00400Actor100400Animation13FB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation14608Bank1[12] = {
#include "assets/actor_100400_animation_14608_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation14608Bank4[149] = {
#include "assets/actor_100400_animation_14608_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation14608Records[203] = {
#include "assets/actor_100400_animation_14608_records.inc"
};

static u16 _gActor00400Actor100400Animation14608Indices[16] = {
#include "assets/actor_100400_animation_14608_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation14608 = {
    _gActor00400Actor100400Animation14608Records,
    _gActor00400Actor100400Animation14608Indices,
    { NULL, _gActor00400Actor100400Animation14608Bank1, NULL, NULL, _gActor00400Actor100400Animation14608Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation14C08Bank1[20] = {
#include "assets/actor_100400_animation_14C08_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation14C08Bank4[126] = {
#include "assets/actor_100400_animation_14C08_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation14C08Records[180] = {
#include "assets/actor_100400_animation_14C08_records.inc"
};

static u16 _gActor00400Actor100400Animation14C08Indices[16] = {
#include "assets/actor_100400_animation_14C08_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation14C08 = {
    _gActor00400Actor100400Animation14C08Records,
    _gActor00400Actor100400Animation14C08Indices,
    { NULL, _gActor00400Actor100400Animation14C08Bank1, NULL, NULL, _gActor00400Actor100400Animation14C08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15050Bank1[9] = {
#include "assets/actor_100400_animation_15050_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15050Bank4[94] = {
#include "assets/actor_100400_animation_15050_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15050Records[135] = {
#include "assets/actor_100400_animation_15050_records.inc"
};

static u16 _gActor00400Actor100400Animation15050Indices[16] = {
#include "assets/actor_100400_animation_15050_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15050 = {
    _gActor00400Actor100400Animation15050Records,
    _gActor00400Actor100400Animation15050Indices,
    { NULL, _gActor00400Actor100400Animation15050Bank1, NULL, NULL, _gActor00400Actor100400Animation15050Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15340Bank1[5] = {
#include "assets/actor_100400_animation_15340_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15340Bank4[43] = {
#include "assets/actor_100400_animation_15340_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15340Records[112] = {
#include "assets/actor_100400_animation_15340_records.inc"
};

static u16 _gActor00400Actor100400Animation15340Indices[16] = {
#include "assets/actor_100400_animation_15340_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15340 = {
    _gActor00400Actor100400Animation15340Records,
    _gActor00400Actor100400Animation15340Indices,
    { NULL, _gActor00400Actor100400Animation15340Bank1, NULL, NULL, _gActor00400Actor100400Animation15340Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15624Bank1[6] = {
#include "assets/actor_100400_animation_15624_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15624Bank4[42] = {
#include "assets/actor_100400_animation_15624_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15624Records[107] = {
#include "assets/actor_100400_animation_15624_records.inc"
};

static u16 _gActor00400Actor100400Animation15624Indices[16] = {
#include "assets/actor_100400_animation_15624_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15624 = {
    _gActor00400Actor100400Animation15624Records,
    _gActor00400Actor100400Animation15624Indices,
    { NULL, _gActor00400Actor100400Animation15624Bank1, NULL, NULL, _gActor00400Actor100400Animation15624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15B34Bank1[11] = {
#include "assets/actor_100400_animation_15B34_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15B34Bank4[108] = {
#include "assets/actor_100400_animation_15B34_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15B34Records[165] = {
#include "assets/actor_100400_animation_15B34_records.inc"
};

static u16 _gActor00400Actor100400Animation15B34Indices[16] = {
#include "assets/actor_100400_animation_15B34_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15B34 = {
    _gActor00400Actor100400Animation15B34Records,
    _gActor00400Actor100400Animation15B34Indices,
    { NULL, _gActor00400Actor100400Animation15B34Bank1, NULL, NULL, _gActor00400Actor100400Animation15B34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15EF8Bank1[6] = {
#include "assets/actor_100400_animation_15EF8_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15EF8Bank4[80] = {
#include "assets/actor_100400_animation_15EF8_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15EF8Records[125] = {
#include "assets/actor_100400_animation_15EF8_records.inc"
};

static u16 _gActor00400Actor100400Animation15EF8Indices[16] = {
#include "assets/actor_100400_animation_15EF8_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15EF8 = {
    _gActor00400Actor100400Animation15EF8Records,
    _gActor00400Actor100400Animation15EF8Indices,
    { NULL, _gActor00400Actor100400Animation15EF8Bank1, NULL, NULL, _gActor00400Actor100400Animation15EF8Bank4, NULL, NULL, NULL },
};

_Actor00400AreaConfig Actor00400_D15F20[12] = {
    { NULL, NULL, NULL, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 0 },
    { D_shelter_b4_reservoir_801851D4, D_shelter_b4_reservoir_801851E4, &D_shelter_b4_reservoir_80184F80, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_shelter_b2_septic_tank_801836A4, D_shelter_b2_septic_tank_801836B4, &gShelterB2SepticTankWaterY, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_shelter_b4_upper_sewer_801866F8, D_shelter_b4_upper_sewer_80186708, &D_shelter_b4_upper_sewer_80186438, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_neo_ark_bridge_801820BC, D_neo_ark_bridge_801820CC, &D_neo_ark_bridge_80181FF8, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 0 },
    { D_neo_ark_submarine_gallery_80181B0C, D_neo_ark_submarine_gallery_80181B1C, &D_neo_ark_submarine_gallery_80181A48, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_neo_ark_submarine_tunnel_80181F94, NULL, &D_neo_ark_submarine_tunnel_80181E90, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 0 },
    { D_neo_ark_pavilion_80183A64, D_neo_ark_pavilion_80183A74, &D_neo_ark_pavilion_801839A0, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 0 },
    { D_neo_ark_island_80181CE8, D_neo_ark_island_80181CF8, &D_neo_ark_island_80181C24, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 0 },
    { D_shelter_b2_main_corridor_80182EEC, D_shelter_b2_main_corridor_80182EFC, &gShelterB2MainCorridorWaterY, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 0 },
    { D_shelter_b4_lower_sewer_8018210C, D_shelter_b4_lower_sewer_8018211C, &D_shelter_b4_lower_sewer_80181E6C, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { NULL, NULL, NULL, ACTOR_00400_AREA_CONFIG_END, 0, 0 },
};

TaskMessageEntry Actor00400_D16010[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor00400ApplyCommand },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor00400SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor00400_D16028[3] = {
    { { { TASK_BODY_TMD, 96 } }, _actor00400BodyTask, { .model = &_gActor00400DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor00400ShotTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, _actor00400GroundStainTask, { .value = 0 } },
};

AnimationSet* Actor00400_D1604C[20] = {
    NULL,
    &_gActor00400Actor100400Animation10348,
    &_gActor00400Actor100400Animation105F4,
    &_gActor00400Actor100400Animation1097C,
    &_gActor00400Actor100400Animation11054,
    &_gActor00400Actor100400Animation11918,
    &_gActor00400Actor100400Animation11C64,
    &_gActor00400Actor100400Animation1236C,
    &_gActor00400Actor100400Animation12C50,
    &_gActor00400Actor100400Animation132B0,
    &_gActor00400Actor100400Animation136E8,
    &_gActor00400Actor100400Animation13C28,
    &_gActor00400Actor100400Animation13FB0,
    &_gActor00400Actor100400Animation14608,
    &_gActor00400Actor100400Animation14C08,
    &_gActor00400Actor100400Animation15050,
    &_gActor00400Actor100400Animation15340,
    &_gActor00400Actor100400Animation15624,
    &_gActor00400Actor100400Animation15B34,
    &_gActor00400Actor100400Animation15EF8,
};

u16 Actor00400_D1609C[8] = {
    190,
    200,
    210,
    220,
    230,
    250,
    270,
    290,
};

static void _actor00400WoundedFloatIdle(Task* task);

static void _actor00400WoundedFloatFlinch(Task* task);

static void _actor00400GroundStainHold(Task* task);

static void _actor00400GroundStainFade(Task* task);

static void _actor00400SwimEmergeEnter(Task* task);

static void Actor00400_Fn058C4(Task* arg0);

static void            _actor00400InitModelAndEnemy(Task* task);
static void            _actor00400CrawlStride(Task* task);
static void            _actor00400DrawLimbShadows(Task* actor, s16 worldY, u8 shade);
static void            _actor00400UpdatePartyTarget(Task* task);
static void            _actor00400UpdateStrandedLook(Task* task, s32 lookDisabled);
static void            _actor00400FlyShot(Task* task);
static void            _actor00400FindNearestSurfaceSpot(Task* task, SVECTOR* nearestPosition);
static void            _actor00400DrawGroundStain(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 intensity);
static __inline__ s32  _actor00400ApplyAreaConfig(Task* task);
static __inline__ void _actor00400SelectTrunkTarget(Task* task, Enemy* enemy,
                                                    _Actor00400Work* work, s32 gridCollisionEnabled);
static __inline__ void _actor00400UpdateModelColor(Task* task, GfxCoord* sampleCoord,
                                                   const _Actor00400Work* work, const TmdObject* model);
static inline void     _actor00400TurnTowardPoint(Task* task, const SVECTOR* target, s32 yawStep, s32 deadband);
static void            _actor00400DiveSwimToSurfaceSpot(Task* task);
static void            Actor00400_Fn061E8(Task* arg0);
static void            _actor00400SwimAttackWindup(Task* task);
static inline void     Actor00400_SpawnMarker(Task* arg0);
static void            Actor00400_Fn064B0(Task* arg0);
static void            _actor00400TunnelPatrolSwim(Task* task);
static void            _actor00400RoomIntroBeginDischarge(Task* task);
static inline s32      _actor00400ConsumeWoundedHitReaction(_Actor00400Work* work);
static void            _actor00400WoundedFloatIdleTick(Task* task);
static void            _actor00400WoundedFloatFlinchTick(Task* task);
static inline s32      _actor00400ConsumeSwimHeavyRecoilHitReaction(Task* task);

#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_impact_burst.inc.c"

#include "../../shared/diver_draw_spark.inc.c"

/// Advances the spark discharge and its stranded attack-sphere window.
///
/// Requires live work, enemy and model with trunk part 1. The signed halfword
/// counts remaining running frames: sound every eight, sparks at 48 and 24,
/// stranded pair testing enabled at 22 and disabled when the countdown ends.
/// A zero countdown leaves both effects and collision flags unchanged.
static void _actor00400TickDischarge(Task* task)
{
    enum {
        ACTOR_00400_DISCHARGE_SOUND_PERIOD  = 8,
        ACTOR_00400_DISCHARGE_SPARK_FIRST   = 24,
        ACTOR_00400_DISCHARGE_SPARK_SECOND  = 48,
        ACTOR_00400_DISCHARGE_CONTACT_START = 22
    };
    _Actor00400Work* work;

    work = task->work;
    if (work->attackFrames != 0) {
        if (!(work->attackFrames & (ACTOR_00400_DISCHARGE_SOUND_PERIOD - 1))) {
            s32 soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_DISCHARGE;
            s32 pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
        if (work->attackFrames == ACTOR_00400_DISCHARGE_SPARK_FIRST || work->attackFrames == ACTOR_00400_DISCHARGE_SPARK_SECOND) {
            effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, &task->extra.tmd->coords[1], NULL, &work->effectArg);
        }
        if (work->attackFrames == ACTOR_00400_DISCHARGE_CONTACT_START && work->inWater == 0) {
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        }
        if (--work->attackFrames == 0) {
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
}

/// Initializes the diver's model, tracked enemy and animation/collision storage.
///
/// Requires an allocated zeroed work block, live enemy and model with parts
/// 0..14, and an unlinked target node and collision bodies. Borrows the model
/// and enemy, pointing them into work storage until teardown. Starts at the
/// head target with lock-on disabled and full parameter-table HP. Parents the
/// root to the view and records its heading in 4096ths of a turn.
static void _actor00400InitModelAndEnemy(Task* task)
{
    enum {
        ACTOR_00400_INITIAL_TARGET_PART     = 4,
        ACTOR_00400_HIT_EFFECT_PART         = 1,
        ACTOR_00400_HIT_EFFECT_SIZE         = 1536,
        ACTOR_00400_HIT_EFFECT_REPEAT_COUNT = 3
    };
    TmdObject*       model;
    _Actor00400Work* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;
    GfxCoord*        partCoords;
    u16              maxHp;
    u8               targetPartIndex;

    model                      = task->extra.tmd;
    work                       = task->work;
    enemy                      = task->spawnArg2.pointer;
    model->lightMtx            = &work->lightMtx;
    model->flags               = 0;
    model->colorMtx            = &work->colorMtx;
    partCoords                 = task->extra.tmd->coords;
    rootCoord                  = model->coords;
    work->effectArg.spawnArgLo = ACTOR_00400_HIT_EFFECT_SIZE;
    work->effectArg.spawnArgHi = ACTOR_00400_HIT_EFFECT_REPEAT_COUNT;
    work->targetPart           = ACTOR_00400_INITIAL_TARGET_PART;
    work->effectArg.coord      = &partCoords[ACTOR_00400_HIT_EFFECT_PART];
    enemy->field_4             = &rootCoord->coord;
    enemy->field_48            = 0;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vy          = 0;
    enemy->bodyPos.vz          = 0;
    targetPartIndex            = work->targetPart;
    enemy->coord               = &task->extra.tmd->coords[targetPartIndex];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->hitContacts;
    enemy->param                  = &Actor00400_D0FDC8;
    maxHp                         = Actor00400_D0FDC8.hpMax;
    enemy->hpMax                  = maxHp;
    enemy->hp                     = maxHp;
    // The enemy and its spheres borrow model parts and work-owned contacts.
    rootCoord->parent = &gGfxViewCoord;
    animationInitContext(&work->rig.anim, Actor00400_D1604C, model, work->rig.poses, work->rig.slots);
    _actor00400InitCollisionBodies(task);
    work->rotation.vy = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
}

/// Consumes a pending clip request and ticks body slots 1..14.
///
/// Requires a live initialized rig. A blend resets the frame counter on a new
/// clip, or rescales it on a repeated clip; RESET restarts the requested clip
/// and clears the counter. Both become PLAYING. A PLAYING tick increments the
/// signed halfword counter. Other request values still tick the body slots.
/// Leaves the slot-1 status in the rig for the frame driver to publish.
static inline void _actor00400AdvanceAnimation(Task* task)
{
    _Actor00400Work* work;
    s32              slotIndex;

    work = task->work;
    if (work->animRequest == DIVER_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrames = 0;
        } else {
            work->animFrames = _actor00400ScaleFramesForAnimRate(task, work->animFrames);
        }
        _actor00400BlendRequestedClip(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (work->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
        work->animFrames++;
    }
    slotIndex = 1;
    do {
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Advances a crawl stride while keeping the arm tips anchored in XZ.
///
/// Requires a live diver rig, enemy and player task. Starts the crawl clip at
/// double rate when needed. A boundary, jump or settled status resets the
/// frame counter; a zero frame captures the midpoint and alternates the stride
/// sounds. Through the rate-scaled frame
/// 57 (narrowed to a byte), turns toward the player and restores that midpoint.
/// Positions use the root's view-parent space; the root is marked dirty.
static void _actor00400CrawlStride(Task* task)
{
    enum { ACTOR_00400_CRAWL_ANCHOR_LAST_NORMAL_FRAME = 57,
           ACTOR_00400_CRAWL_FIRST_ARM_TIP            = 11,
           ACTOR_00400_CRAWL_SECOND_ARM_TIP           = 14 };
    Task*            actor;
    Task*            player;
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    GfxCoord*        playerRoot;
    SVECTOR          playerPosition;
    u8               anchorEndFrame;

    actor     = task;
    player    = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    if (work->animClip != ACTOR_00400_ANIM_CRAWL) {
        _actor00400RequestClipBlend(actor, ACTOR_00400_ANIM_CRAWL, ANIMATION_RATE_ONE * 2, 10);
        _actor00400TickAnimation(actor);
    }
    anchorEndFrame = _actor00400ScaleFramesForAnimRate(actor, ACTOR_00400_CRAWL_ANCHOR_LAST_NORMAL_FRAME);
    if (_actor00400ClipEnded(actor)) {
        work->animFrames = 0;
    }
    // Each stride fixes the arm-tip midpoint before turning the body around it.
    if (work->animFrames == 0) {
        _actor00400CapturePartMidpointXZ(actor, ACTOR_00400_CRAWL_FIRST_ARM_TIP, ACTOR_00400_CRAWL_SECOND_ARM_TIP, &work->armAnchor);
        if (work->strideCount & 1) {
            s32 soundId   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_STRIDE_ODD;
            s32 panOffset = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        } else {
            s32 soundId   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_STRIDE_EVEN;
            s32 panOffset = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        }
        work->strideCount++;
    }
    if (work->animFrames >= 0 && anchorEndFrame >= work->animFrames) {
        playerRoot        = player->extra.tmd->coords;
        playerPosition.vx = playerRoot->coord.t[0];
        playerPosition.vy = playerRoot->coord.t[1];
        playerPosition.vz = playerRoot->coord.t[2];
        _actor00400TurnTowardPointMaskedRange(actor, &playerPosition, 8, 0x100);
        _actor00400AlignPartMidpointXZ(actor, ACTOR_00400_CRAWL_FIRST_ARM_TIP, ACTOR_00400_CRAWL_SECOND_ARM_TIP, &work->armAnchor);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/limb_shadows_segment.inc.c"

/// Draws thirteen limb-shadow segments of the diver's fifteen-part rig.
///
/// Requires the live model coordinates. `worldY` fixes the horizontal shadow
/// plane in game-coordinate units; `shade` is an eight-bit subtractive intensity.
/// Each segment uses its own fixed half-width and the shared blob texture.
static void _actor00400DrawLimbShadows(Task* actor, s16 worldY, u8 shade)
{
    _limbShadowDrawSegment(actor, 1, 2, 0x258, worldY, shade);
    _limbShadowDrawSegment(actor, 2, 3, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 3, 4, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 4, 5, 0x1F4, worldY, shade);
    _limbShadowDrawSegment(actor, 1, 6, 0x320, worldY, shade);
    _limbShadowDrawSegment(actor, 6, 7, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 7, 8, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 1, 0xC, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 0xC, 0xD, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 0xD, 0xE, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 1, 9, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 9, 0xA, 0x12C, worldY, shade);
    _limbShadowDrawSegment(actor, 0xA, 0xB, 0x12C, worldY, shade);
}

/// Saves the frame's root position and tracks the nearer party member.
///
/// Measures XZ distance from the trunk's world origin to the player and live
/// companion roots; ties select the player. Caches the chosen XYZ, signed
/// halfword distance and relative yaw (4096 units per turn, wrapped to 0..4095).
/// Offsets narrow to signed halfwords before measurement. With no player,
/// target fields are preserved; the root snapshot is still refreshed.
static void _actor00400UpdatePartyTarget(Task* task)
{
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    GfxCoord*        playerRoot;
    GfxCoord*        companionRoot;
    Task*            player;
    GfxCoord*        trunkCoord;
    SVECTOR          targetOffset;
    SVECTOR          companionOffset;
    SVECTOR          jointWorldPosition;
    s32              targetDistance;
    s32              companionDistance;

    work                 = task->work;
    rootCoord            = task->extra.tmd->coords;
    player               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    trunkCoord           = &rootCoord[1];
    work->prevRootPos.vx = rootCoord->coord.t[0];
    work->prevRootPos.vy = rootCoord->coord.t[1];
    work->prevRootPos.vz = rootCoord->coord.t[2];
    // Measure both roots from the trunk origin; ties retain the player.
    if (player != NULL) {
        playerRoot            = player->extra.tmd->coords;
        jointWorldPosition.vx = 0;
        jointWorldPosition.vy = 0;
        jointWorldPosition.vz = 0;
        _actorRenderTransformPointToWorld(trunkCoord, &jointWorldPosition);
        targetOffset.vx = playerRoot->coord.t[0] - jointWorldPosition.vx;
        targetOffset.vy = playerRoot->coord.t[1] - jointWorldPosition.vy;
        targetOffset.vz = playerRoot->coord.t[2] - jointWorldPosition.vz;
        targetDistance  = SquareRoot0(targetOffset.vx * targetOffset.vx + targetOffset.vz * targetOffset.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->targetPos.vx   = playerRoot->coord.t[0];
            work->targetPos.vy   = playerRoot->coord.t[1];
            work->targetPos.vz   = playerRoot->coord.t[2];
            work->targetDistance = targetDistance;
        } else {
            companionRoot      = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            companionOffset.vx = companionRoot->coord.t[0] - jointWorldPosition.vx;
            companionOffset.vy = companionRoot->coord.t[1] - jointWorldPosition.vy;
            companionOffset.vz = companionRoot->coord.t[2] - jointWorldPosition.vz;
            companionDistance  = SquareRoot0(companionOffset.vx * companionOffset.vx + companionOffset.vz * companionOffset.vz);
            if (companionDistance < targetDistance) {
                work->targetPos.vx = companionRoot->coord.t[0];
                work->targetPos.vy = companionRoot->coord.t[1];
                work->targetPos.vz = companionRoot->coord.t[2];
                targetOffset       = companionOffset;
                targetDistance     = companionDistance;
            } else {
                work->targetPos.vx = playerRoot->coord.t[0];
                work->targetPos.vy = playerRoot->coord.t[1];
                work->targetPos.vz = playerRoot->coord.t[2];
            }
            work->targetDistance = targetDistance;
        }
        VectorNormalSS(&targetOffset, &targetOffset);
        work->targetBearing = (ratan2(targetOffset.vx, targetOffset.vz) - work->rotation.vy) & (ACTOR_TRANSFORM_ANGLE_TURN - 1);
    }
}

/// Turns the stranded diver's neck and head toward its cached party target.
///
/// Requires live model parts 1..4 and the current animation pose. Angles use
/// 4096 units per turn. A zero low byte of `lookDisabled` chases target yaw
/// within +/-1535 by 24 units outside a 32-unit deadband; otherwise yaw eases
/// to zero, twice as fast when disabled. The decay narrows its scaled yaw to
/// a signed halfword before negation and shifting. Keeps each neck joint's
/// animated pitch, distributes look yaw across the neck/head, and compensates
/// the head for the trunk and neck rotations. Changes GTE state.
static void _actor00400UpdateStrandedLook(Task* task, s32 lookDisabled)
{
    enum {
        ACTOR_00400_LOOK_PART_TRUNK           = 1,
        ACTOR_00400_LOOK_PART_LOWER_NECK      = 2,
        ACTOR_00400_LOOK_PART_UPPER_NECK      = 3,
        ACTOR_00400_LOOK_PART_HEAD            = 4,
        ACTOR_00400_LOOK_TARGET_HEIGHT        = 1536,
        ACTOR_00400_LOOK_MAX_YAW              = 1535,
        ACTOR_00400_LOOK_DEADBAND             = 32,
        ACTOR_00400_LOOK_YAW_STEP             = 24,
        ACTOR_00400_LOOK_DECAY_SCALE          = 16,
        ACTOR_00400_LOOK_DECAY_SHIFT          = 8,
        ACTOR_00400_LOOK_DISABLED_DECAY_SHIFT = 7,
        ACTOR_00400_LOOK_JOINT_COUNT          = 3
    };
    SVECTOR          targetAngles;
    SVECTOR          lowerNeckAngles;
    SVECTOR          upperNeckAngles;
    MATRIX           inverseTrunkRotation;
    MATRIX           inverseLowerNeckRotation;
    MATRIX           inverseUpperNeckRotation;
    MATRIX           lowerNeckPitch;
    MATRIX           upperNeckPitch;
    MATRIX           headYawRotation;
    GfxCoord*        partCoords;
    GfxCoord*        trunkCoord;
    GfxCoord*        lowerNeckCoord;
    GfxCoord*        upperNeckCoord;
    GfxCoord*        headCoord;
    _Actor00400Work* work;
    MATRIX*          lowerNeckRotation;
    MATRIX*          upperNeckRotation;

    partCoords     = task->extra.tmd->coords;
    trunkCoord     = &partCoords[ACTOR_00400_LOOK_PART_TRUNK];
    lowerNeckCoord = &partCoords[ACTOR_00400_LOOK_PART_LOWER_NECK];
    upperNeckCoord = &partCoords[ACTOR_00400_LOOK_PART_UPPER_NECK];
    headCoord      = &partCoords[ACTOR_00400_LOOK_PART_HEAD];
    work           = task->work;
    _actor00400GetTargetAnglesFromPart(task, ACTOR_00400_LOOK_PART_HEAD, &targetAngles, ACTOR_00400_LOOK_TARGET_HEIGHT);
    if ((u8)lookDisabled == 0) {
        if ((u16)(targetAngles.vy + ACTOR_00400_LOOK_MAX_YAW) < (2 * ACTOR_00400_LOOK_MAX_YAW + 1)) {
            if ((u32)((targetAngles.vy - (s16)work->lookYaw) + ACTOR_00400_LOOK_DEADBAND) >= (2 * ACTOR_00400_LOOK_DEADBAND + 1)) {
                if ((s16)work->lookYaw < targetAngles.vy) {
                    work->lookYaw = work->lookYaw + ACTOR_00400_LOOK_YAW_STEP;
                } else {
                    work->lookYaw = work->lookYaw - ACTOR_00400_LOOK_YAW_STEP;
                }
            }
        } else {
            work->lookYaw = work->lookYaw + ((s32) - (s16)(work->lookYaw * ACTOR_00400_LOOK_DECAY_SCALE) >> ACTOR_00400_LOOK_DECAY_SHIFT);
        }
    } else {
        work->lookYaw = work->lookYaw + ((s32) - (s16)(work->lookYaw * ACTOR_00400_LOOK_DECAY_SCALE) >> ACTOR_00400_LOOK_DISABLED_DECAY_SHIFT);
    }

    // Rebuild neck pitch before distributing the look yaw.
    lowerNeckRotation = &lowerNeckCoord->coord;

    gfxSetRotIdentity(&lowerNeckPitch);
    gfxSetRotIdentity(&upperNeckPitch);
    gfxSetRotIdentity(&headYawRotation);

    gfxExtractEulerAngles(lowerNeckRotation, &lowerNeckAngles);
    upperNeckRotation = &upperNeckCoord->coord;
    gfxExtractEulerAngles(upperNeckRotation, &upperNeckAngles);
    RotMatrixX(lowerNeckAngles.vx, &lowerNeckPitch);
    RotMatrixX(upperNeckAngles.vx, &upperNeckPitch);
    _actor00400CopyRotation(&lowerNeckPitch, lowerNeckRotation);
    _actor00400CopyRotation(&upperNeckPitch, upperNeckRotation);
    actorRenderComposeCoord(trunkCoord);
    actorRenderComposeCoord(lowerNeckCoord);
    actorRenderComposeCoord(upperNeckCoord);
    _diverTurnJoint(lowerNeckCoord, (s16)work->lookYaw / ACTOR_00400_LOOK_JOINT_COUNT);
    _diverTurnJoint(upperNeckCoord, (s16)work->lookYaw / ACTOR_00400_LOOK_JOINT_COUNT);

    // Undo the trunk and both neck rotations in the head's local frame.
    gfxSetRotIdentity(&headYawRotation);

    RotMatrixY((s16)work->lookYaw / ACTOR_00400_LOOK_JOINT_COUNT, &headYawRotation);
    /// Converts a desired head basis into the trunk and neck's local frame.
    ///
    /// Each argument is a live MATRIX pointer evaluated once in statement order.
    /// Captures the three separate writable inverse*Rotation matrices declared
    /// above; they must not alias any argument. Transposes the Q12 trunk and
    /// neck bases, then multiplies them in that order and copies nine resulting
    /// coefficients, preserving head translation. An inverse requires
    /// orthonormal inputs. Expands as statements in this block; changes GTE state.
#define ACTOR_00400_LOCALIZE_HEAD_YAW(trunkRotation, lowerRotation, upperRotation, desiredRotation, headRotation) \
    TransposeMatrix((trunkRotation), &inverseTrunkRotation);                                                      \
    TransposeMatrix((lowerRotation), &inverseLowerNeckRotation);                                                  \
    TransposeMatrix((upperRotation), &inverseUpperNeckRotation);                                                  \
    MulMatrix(&inverseTrunkRotation, &inverseLowerNeckRotation);                                                  \
    MulMatrix(&inverseTrunkRotation, &inverseUpperNeckRotation);                                                  \
    MulMatrix(&inverseTrunkRotation, (desiredRotation));                                                          \
    _actor00400CopyRotation(&inverseTrunkRotation, (headRotation));

    ACTOR_00400_LOCALIZE_HEAD_YAW(&trunkCoord->coord, lowerNeckRotation, upperNeckRotation,
                                  &headYawRotation, &headCoord->coord);
#undef ACTOR_00400_LOCALIZE_HEAD_YAW
}

/// Initializes and links the diver's four collision spheres and contact tables.
///
/// Requires live work, enemy and model coordinates, with the spheres not yet
/// linked. The trunk and head share six hit contacts and accept pair tests;
/// the discharge sphere starts disabled. The root sphere accepts grid tests
/// only when `gridCollision` is set. Storage remains owned by the diver until
/// teardown unlinks the bodies.
static void _actor00400InitCollisionBodies(Task* task)
{
    enum {
        ACTOR_00400_COLLISION_PART_TRUNK    = 1,
        ACTOR_00400_COLLISION_PART_HEAD     = 4,
        ACTOR_00400_COLLISION_BODY_KEY      = WORLD_COLLISION_CONTACT_ENEMY_BODY | 4,
        ACTOR_00400_COLLISION_TRUNK_RADIUS  = 768,
        ACTOR_00400_COLLISION_HEAD_RADIUS   = 192,
        ACTOR_00400_COLLISION_ATTACK_RADIUS = 1152,
        ACTOR_00400_COLLISION_GRID_RADIUS   = 896
    };
    _Actor00400Work* work = task->work;

    /// Initializes and links an origin-centred sphere with borrowed coordinate and contacts.
    ///
    /// `sphere` must be an unlinked, side-effect-free WorldCollisionBody lvalue;
    /// it is evaluated repeatedly. Other arguments are evaluated once in field
    /// assignment order, with radius in game-coordinate units. Contact-table
    /// initialization and pair/grid enablement remain the caller's responsibility.
    /// Expands as statements in this braced function body and captures no locals.
#define ACTOR_00400_LINK_SPHERE(sphere, partCoord, contactTable, contactKey, sphereRadius, listIndex) \
    (sphere).coord            = (partCoord);                                                          \
    (sphere).context.contacts = (contactTable);                                                       \
    (sphere).pos.vx           = 0;                                                                    \
    (sphere).pos.vy           = 0;                                                                    \
    (sphere).pos.vz           = 0;                                                                    \
    (sphere).key              = (contactKey);                                                         \
    (sphere).radius           = (sphereRadius);                                                       \
    (sphere).flags            = WORLD_COLLISION_BODY_SPHERE;                                          \
    worldCollisionLinkBody((listIndex), &(sphere));

    ACTOR_00400_LINK_SPHERE(work->trunkBody, &task->extra.tmd->coords[ACTOR_00400_COLLISION_PART_TRUNK], work->hitContacts,
                            ACTOR_00400_COLLISION_BODY_KEY, ACTOR_00400_COLLISION_TRUNK_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->trunkBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    ACTOR_00400_LINK_SPHERE(work->headBody, &task->extra.tmd->coords[ACTOR_00400_COLLISION_PART_HEAD], work->hitContacts,
                            ACTOR_00400_COLLISION_BODY_KEY, ACTOR_00400_COLLISION_HEAD_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES);
    work->headBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    ACTOR_00400_LINK_SPHERE(work->attackBody, &task->extra.tmd->coords[ACTOR_00400_COLLISION_PART_TRUNK], work->attackContacts,
                            damagePackEnemyAttackKey(task->spawnArg2.pointer, 0), ACTOR_00400_COLLISION_ATTACK_RADIUS, WORLD_COLLISION_LIST_ENEMY_ATTACKS);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    ACTOR_00400_LINK_SPHERE(work->gridBody, task->extra.tmd->coords, work->gridContacts,
                            ACTOR_00400_COLLISION_BODY_KEY, ACTOR_00400_COLLISION_GRID_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    if (work->gridCollision != 0) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else {
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
#undef ACTOR_00400_LINK_SPHERE
}

/// Steps a root translation in a fractional correction's sign after its integer floor.
///
/// correctionWord is signed 16.16; translation is one writable root component.
/// Call after adding the signed high half. Negative fractions step one unit
/// below that floor; addition must fit the word. No pointer is retained.
static inline void _actor00400ApplyRootFractionalPush(s32 correctionWord, long* translation)
{
    if ((correctionWord & 0xFFFF) != 0) {
        if (correctionWord > 0) {
            (*translation)++;
        } else {
            (*translation)--;
        }
    }
}

/// Steps an arm anchor in a fractional correction's sign, retaining halfword narrowing.
///
/// correctionWord is signed 16.16; translation is one writable anchor component.
/// Call after adding the signed high half. Negative fractions step below the
/// floor, and the update narrows to the low signed halfword. No pointer is retained.
static inline void _actor00400ApplyAnchorFractionalPush(s32 correctionWord, short* translation)
{
    if ((correctionWord & 0xFFFF) != 0) {
        if (correctionWord > 0) {
            (*translation)++;
        } else {
            (*translation)--;
        }
    }
}

/// Applies attack contacts, status damage and room-grid correction to the diver.
///
/// Requires live enemy/root and the six initialized hit and grid records.
/// Scans category-2 contacts until a hit remains pending. Cooldown blocks HP
/// damage while still permitting Life Drain motes. Damage narrows to a signed
/// halfword before reaction and HP/readout updates.
/// Status damage retains its full HP subtraction but uses its signed low half
/// for the readout and hit test. Grid XZ corrections move the arm anchor and
/// root by the integer floor plus one unit in the fractional sign; opposed
/// corrections restore the previous root XZ. Clears both contact lists and
/// decrements or clamps the cooldown. Reaction consumption occurs next frame.
static void _actor00400ApplyContacts(Task* task)
{
    enum {
        ACTOR_00400_ATTACK_CATEGORY_MASK         = 0xFFFF0000,
        ACTOR_00400_ATTACK_CATEGORY_PLAYER       = 0x20000,
        ACTOR_00400_ATTACK_ROW_MASK              = 0x7F,
        ACTOR_00400_ATTACK_ATTACHMENT_BIT        = 0x8000,
        ACTOR_00400_WEAPON_ROW_FLINCH_ONLY       = 28,
        ACTOR_00400_HEAVY_HIT_MIN_DAMAGE         = 60,
        ACTOR_00400_ATTACK_REACTION_BLAST        = 4,
        ACTOR_00400_ATTACK_REACTION_HEAVY        = 5,
        ACTOR_00400_ATTACK_REACTION_SUPPRESS_HIT = 8,
        ACTOR_00400_ATTACK_REACTION_LIGHT        = 9,
        ACTOR_00400_HIT_EFFECT_NONE              = 0,
        ACTOR_00400_HIT_EFFECT_CRITICAL          = 1,
        ACTOR_00400_HIT_EFFECT_INCENDIARY        = 2,
        ACTOR_00400_CRITICAL_STYLE_YELLOW        = 0,
        ACTOR_00400_INCENDIARY_STYLE_CYAN        = 2
    };
    _Actor00400Work*    work;
    Enemy*              enemy;
    GfxCoord*           rootCoord;
    WorldCollisionDelta pushback;
    s32                 hitEffectKind;
    s16                 damageAmount;
    s32                 computedDamage;
    s32                 scratchValue;
    s32                 dotReadout;
    s32                 contactIndex;

    hitEffectKind  = ACTOR_00400_HIT_EFFECT_NONE;
    rootCoord      = task->extra.tmd->coords;
    work           = task->work;
    enemy          = task->spawnArg2.pointer;
    work->hitTaken = 0;
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        if ((work->hitContacts[contactIndex].key.value & ACTOR_00400_ATTACK_CATEGORY_MASK) == ACTOR_00400_ATTACK_CATEGORY_PLAYER) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = 1;
                work->wasHit      = 1;
                computedDamage    = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value, work->targetDistance, 0, 0);
                damageAmount      = computedDamage;
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                if (damageRollCriticalHit(enemy, work->hitContacts[contactIndex].key.value, work->critChanceScale) != 0) {
                    damageAmount  = ((u32)computedDamage << 16) >> 14;
                    hitEffectKind = ACTOR_00400_HIT_EFFECT_CRITICAL;
                }
                effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[contactIndex].key.value),
                               &task->extra.tmd->coords[work->targetPart], 0, &work->effectArg);
                work->hitReaction = (damageAmount < ACTOR_00400_HEAVY_HIT_MIN_DAMAGE) ? ACTOR_00400_HIT_REACTION_FLINCH : ACTOR_00400_HIT_REACTION_HEAVY;
                switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        damageStartEnemyStagger(enemy);
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case ACTOR_00400_ATTACK_REACTION_BLAST:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_BLAST;
                        break;
                    case ACTOR_00400_ATTACK_REACTION_HEAVY:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_HEAVY;
                        break;
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_BLAST;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        hitEffectKind     = ACTOR_00400_HIT_EFFECT_INCENDIARY;
                        work->hitReaction = ACTOR_00400_HIT_REACTION_HEAVY;
                        damageAmount     += damageAmount;
                        break;
                    case ACTOR_00400_ATTACK_REACTION_SUPPRESS_HIT:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
                        work->hitTaken    = 0;
                        break;
                    case ACTOR_00400_ATTACK_REACTION_LIGHT:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_LIGHT;
                        break;
                }
                if ((work->hitContacts[contactIndex].key.value & ACTOR_00400_ATTACK_ROW_MASK) == ACTOR_00400_WEAPON_ROW_FLINCH_ONLY && (work->hitContacts[contactIndex].key.value & ACTOR_00400_ATTACK_ATTACHMENT_BIT) == 0) {
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->hitReaction     = ACTOR_00400_HIT_REACTION_FLINCH;
                }
                scratchValue = hitEffectKind;
                switch (scratchValue) {
                    case ACTOR_00400_HIT_EFFECT_CRITICAL:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], ACTOR_00400_CRITICAL_STYLE_YELLOW, 0);
                        break;
                    case ACTOR_00400_HIT_EFFECT_INCENDIARY:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], ACTOR_00400_INCENDIARY_STYLE_CYAN, 0);
                        break;
                }
                damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, damageAmount, 0);
                worldTargetAddReadoutAmount(&enemy->node, damageAmount, 0);
                enemy->hp -= damageAmount;
                if ((s16)enemy->hp < 0) {
                    enemy->hp = 0;
                }
            } else if ((damageGetPlayerAttackEffectId(work->hitContacts[contactIndex].key.value)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], 0, &work->effectArg);
            }
        }
        if (work->hitTaken != 0) {
            break;
        }
    }

    // Status flags can replace the contact reaction before the next state tick.
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = ACTOR_00400_HIT_REACTION_HEAVY;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = ACTOR_00400_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        scratchValue = damageTickEnemyDamageOverTime(enemy);
        dotReadout   = (s16)scratchValue;
        if (dotReadout != 0) {
            enemy->hp -= scratchValue;
            if ((s16)enemy->hp < 0) {
                enemy->hp = 0;
            }
            worldTargetAddReadoutAmount(&enemy->node, dotReadout, 0);
            if ((s16)enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    // Move the arm anchor and root together; negative fractions step below the floor.
    switch (worldCollisionResolvePushback(work->gridContacts, &pushback, ARRAY_SIZE(work->gridContacts), 0)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            scratchValue        = pushback.fixed.vx.halves.integer;
            work->armAnchor.vx += scratchValue;
            scratchValue        = pushback.fixed.vz.halves.integer;
            work->armAnchor.vz += scratchValue;
            _actor00400ApplyAnchorFractionalPush(pushback.fixed.vx.word, &work->armAnchor.vx);
            _actor00400ApplyAnchorFractionalPush(pushback.fixed.vz.word, &work->armAnchor.vz);
            scratchValue           = pushback.fixed.vx.halves.integer;
            rootCoord->coord.t[0] += scratchValue;
            scratchValue           = pushback.fixed.vz.halves.integer;
            rootCoord->coord.t[2] += scratchValue;
            _actor00400ApplyRootFractionalPush(pushback.fixed.vx.word, &rootCoord->coord.t[0]);
            _actor00400ApplyRootFractionalPush(pushback.fixed.vz.word, &rootCoord->coord.t[2]);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }

    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
}

/// Applies a hit reaction to the living diver's swimming or stranded state.
///
/// Requires the diver's work at `task->work`. Only `hitTaken == 1` consumes
/// the pending reaction: light, heavy/blast and status reactions enter their
/// corresponding states and clear the substate; flinch engages combat and
/// starts a ten-frame root shake. Returns 1 only when it selected a state,
/// otherwise 0. The consumed reaction is cleared; `hitTaken` is preserved.
static s16 _actor00400ApplyHitReaction(Task* task)
{
    enum {
        ACTOR_00400_FLINCH_ALERT        = 1,
        ACTOR_00400_FLINCH_SHAKE_FRAMES = 10
    };
    _Actor00400Work* work;
    s16              reaction;

    work = task->work;
    if (work->hitTaken == 1) {
        reaction = work->hitReaction;
        if (reaction == ACTOR_00400_HIT_REACTION_LIGHT) {
            work->state       = ACTOR_00400_STATE_RECOIL_LIGHT;
            work->subState    = 0;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_00400_HIT_REACTION_HEAVY) {
            work->state       = ACTOR_00400_STATE_RECOIL_HEAVY;
            work->subState    = 0;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_00400_HIT_REACTION_STATUS) {
            work->state       = ACTOR_00400_STATE_STATUS_HOLD;
            work->subState    = 0;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_00400_HIT_REACTION_BLAST) {
            work->state       = ACTOR_00400_STATE_RECOIL_HEAVY;
            work->subState    = 0;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_00400_HIT_REACTION_FLINCH) {
            gSceneCombatState.signals.bytes.enemyAlert = ACTOR_00400_FLINCH_ALERT;
            sceneEngageBattle(1);
            work->shakeFrames = ACTOR_00400_FLINCH_SHAKE_FRAMES;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
            return 0;
        } else {
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
            return 0;
        }
        return 1;
    }
    return 0;
}

/// Swims toward a patrol waypoint and reports when its XZ arrival radius is met.
///
/// Requires a live root and an eight-entry borrowed waypoint ring, index 0..7.
/// Requests the normal swim clip and black colouring on a clip change. Sets
/// goal Y to waypoint depth plus water level. A signed-halfword XZ distance
/// below 400 advances the ring and returns 1; otherwise turns and steps 96
/// coordinate units along the heading, returning 0. Height is eased elsewhere.
static s32 _actor00400SwimToNextWaypoint(Task* task)
{
    enum { ACTOR_00400_PATROL_ARRIVAL_RADIUS = 400,
           ACTOR_00400_WAYPOINT_INDEX_MASK   = 7 };
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          waypointOffset;

    work              = task->work;
    rootCoord         = task->extra.tmd->coords;
    waypointOffset.vx = (u16)work->waypoints[work->waypointIndex].vx - rootCoord->coord.t[0];
    waypointOffset.vy = (u16)work->waypoints[work->waypointIndex].vy - rootCoord->coord.t[1];
    waypointOffset.vz = (u16)work->waypoints[work->waypointIndex].vz - rootCoord->coord.t[2];
    if (work->animClip != ACTOR_00400_ANIM_SWIM) {
        _actor00400RequestClipBlend(task, ACTOR_00400_ANIM_SWIM, ANIMATION_RATE_ONE, 14);
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    work->goalY = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    if ((s16)SquareRoot0(waypointOffset.vx * waypointOffset.vx + waypointOffset.vz * waypointOffset.vz) < ACTOR_00400_PATROL_ARRIVAL_RADIUS) {
        work->waypointIndex = (work->waypointIndex + 1) & ACTOR_00400_WAYPOINT_INDEX_MASK;
        return 1;
    } else {
        _actor00400TurnTowardPointMaskedRange(task, &work->waypoints[work->waypointIndex], 0x2C, 0x100);
        _diverStepForward(task, 0x60, work->rotation.vy);
        return 0;
    }
}

/// Gives a detached chunk the body's texture placement and rebuilds both buffers.
///
/// Requires a successful model-effect spawn. Models borrow their chunk sources;
/// this copies only texture-page and CLUT-row offsets. An absent buffer stays absent.
static inline void _actor00400BindChunkTexture(Task* task, EffectWork* chunkEffect)
{
    TmdObject* bodyModel  = task->extra.tmd;
    TmdObject* chunkModel = chunkEffect->task->extra.tmd;

    chunkModel->texturePageOffset = bodyModel->texturePageOffset;
    chunkModel->clutRowOffset     = bodyModel->clutRowOffset;
    if (chunkModel->buffer != NULL) {
        tmdBuildBufferHalf(chunkModel);
        tmdBuildBufferHalf(chunkModel);
    }
}

/// Spawns the blast death's head, arm, random limb and core chunks with a trunk burst.
///
/// Requires the live model's parts 1, 4, 8, 11 and 14 and loaded chunk sources.
/// Publishes each source before its synchronous spawn. One LCG advance picks
/// the part-8 source. Successful chunks inherit the body's texture placement
/// and rebuild both primitive-buffer halves; allocation failures skip only
/// that chunk's texture update. Effects own their tasks; the sources must stay
/// loaded through their release. The trunk particle spawns independently.
static void _actor00400SpawnBodyChunks(Task* task)
{
    enum {
        ACTOR_00400_CHUNK_PARTICLE_SIZE = 512
    };
    EffectWork* headEffect;
    EffectWork* rightArmEffect;
    EffectWork* leftArmEffect;
    EffectWork* randomLimbEffect;
    EffectWork* coreEffect;

    D_800678F0[0] = &_gActor00400DiverBurstHead;
    headEffect    = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[4], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    if (headEffect != NULL) {
        _actor00400BindChunkTexture(task, headEffect);
    }
    D_800678F0[0]  = &_gActor00400DiverBurstArmRight;
    rightArmEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[11], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    if (rightArmEffect != NULL) {
        _actor00400BindChunkTexture(task, rightArmEffect);
    }
    D_800678F0[0] = &_gActor00400DiverBurstArmLeft1;
    leftArmEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[14], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    if (leftArmEffect != NULL) {
        _actor00400BindChunkTexture(task, leftArmEffect);
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_800678F0[0]    = &_gActor00400DiverBurstLegRight;
        randomLimbEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[8], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    } else {
        D_800678F0[0]    = &_gActor00400DiverBurstArmLeft2;
        randomLimbEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[8], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    }
    if (randomLimbEffect != NULL) {
        _actor00400BindChunkTexture(task, randomLimbEffect);
    }
    D_800678F0[0] = &_gActor00400DiverEnergyBall;
    coreEffect    = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[1], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
    if (coreEffect != NULL) {
        _actor00400BindChunkTexture(task, coreEffect);
    }
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], ACTOR_00400_CHUNK_PARTICLE_SIZE, NULL);
}

/// Straightens and retracts the neck, or eases it back into the animated pose.
///
/// Requires the live model's root, trunk, two neck parts and head (0..4).
/// `neckRetracted` in the work block selects the behavior; the second argument
/// is ignored. Retraction captures the neck's Euler angles, eases them to
/// within 48 angle units of zero, then shortens the lower neck's Z basis.
/// The head keeps its rotation and receives inverse Z scale. Release restores
/// Q12 unit scale, then blends captured angles halfway toward the live pose.
/// Translations are retained and the head hierarchy is recomposed. Angles
/// use 4096 units per turn; the scale must stay positive for its reciprocal.
static void _actor00400UpdateNeckRetraction(Task* task, s32 unusedNeckRetracted)
{
    enum { ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD = 48,
           ACTOR_00400_NECK_RETRACTED_SCALE          = 0x2AA,
           ACTOR_00400_NECK_RELEASE_SCALE_THRESHOLD  = 0xF80 };
    VECTOR           axisScale;
    MATRIX           rotationMatrix;
    SVECTOR          lowerAnimatedAngles;
    MATRIX           lowerNeckBasis;
    SVECTOR          upperAnimatedAngles;
    MATRIX           upperNeckBasis;
    SVECTOR          headAngles;
    MATRIX           headBasis;
    _Actor00400Work* work;
    GfxCoord*        coords;
    GfxCoord*        lowerNeckCoord;
    GfxCoord*        upperNeckCoord;
    GfxCoord*        headCoord;
    s32              inverseNeckScale;

    /// Rebuilds both neck bases and inverse-scales the head, retaining translations.
    ///
    /// Captures work, coords, headCoord, headAngles, axisScale, the three basis
    /// matrices, rotationMatrix and inverseNeckScale. Requires positive Q12
    /// neckScale and initialized headAngles; expands as statements in this block.
#define ACTOR_00400_APPLY_NECK_SCALE()                          \
    gfxSetRotIdentity(&lowerNeckBasis);                         \
    axisScale.vx = ONE;                                         \
    axisScale.vy = ONE;                                         \
    axisScale.vz = work->neckScale;                             \
    ScaleMatrix(&lowerNeckBasis, &axisScale);                   \
    _actor00400CopyRotation(&lowerNeckBasis, &coords[2].coord); \
    gfxSetRotIdentity(&upperNeckBasis);                         \
    axisScale.vx = ONE;                                         \
    axisScale.vy = ONE;                                         \
    axisScale.vz = ONE;                                         \
    ScaleMatrix(&upperNeckBasis, &axisScale);                   \
    _actor00400CopyRotation(&upperNeckBasis, &coords[3].coord); \
    gfxSetRotIdentity(&headBasis);                              \
    axisScale.vx     = ONE;                                     \
    axisScale.vy     = ONE;                                     \
    inverseNeckScale = (ONE * ONE) / work->neckScale;           \
    axisScale.vz     = inverseNeckScale;                        \
    ScaleMatrix(&headBasis, &axisScale);                        \
    gfxSetRotIdentity(&rotationMatrix);                         \
    RotMatrix(&headAngles, &rotationMatrix);                    \
    MulMatrix(&headBasis, &rotationMatrix);                     \
    _actor00400CopyRotation(&headBasis, &headCoord->coord);

    coords         = task->extra.tmd->coords;
    work           = task->work;
    lowerNeckCoord = &coords[2];
    upperNeckCoord = &coords[3];
    headCoord      = &coords[4];
    if (work->neckRetracted != 0) {
        switch (work->neckPhase) {
            case ACTOR_00400_NECK_FREE:
                // Capture the animated neck pose before straightening it.
                coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(headCoord);
                gfxExtractEulerAngles(&coords[2].coord, &work->lowerNeckAngles);
                gfxExtractEulerAngles(&coords[3].coord, &work->upperNeckAngles);
                work->neckPhase = ACTOR_00400_NECK_STRAIGHTEN;
                work->neckScale = ONE;
                /* fallthrough */
            case ACTOR_00400_NECK_STRAIGHTEN: {

                work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((s32) - (work->lowerNeckAngles.vx * 0x10) >> 6);
                work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((s32) - (work->lowerNeckAngles.vy * 0x10) >> 6);
                work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((s32) - (work->lowerNeckAngles.vz * 0x10) >> 6);
                work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((s32) - (work->upperNeckAngles.vx * 0x10) >> 6);
                work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((s32) - (work->upperNeckAngles.vy * 0x10) >> 6);
                work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((s32) - (work->upperNeckAngles.vz * 0x10) >> 6);
                gfxSetRotIdentity(&rotationMatrix);
                RotMatrix(&work->lowerNeckAngles, &rotationMatrix);
                _actor00400CopyRotation(&rotationMatrix, &lowerNeckCoord->coord);
                gfxSetRotIdentity(&rotationMatrix);
                RotMatrix(&work->upperNeckAngles, &rotationMatrix);
                _actor00400CopyRotation(&rotationMatrix, &upperNeckCoord->coord);
                if ((abs(work->lowerNeckAngles.vx) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->lowerNeckAngles.vy) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->lowerNeckAngles.vz) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD) &&
                    (abs(work->upperNeckAngles.vx) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->upperNeckAngles.vy) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->upperNeckAngles.vz) < ACTOR_00400_NECK_STRAIGHT_ANGLE_THRESHOLD)) {
                    work->neckPhase = ACTOR_00400_NECK_RETRACTED;
                }
                lowerNeckCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                upperNeckCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                headCoord->composeStamp      = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(headCoord);
                break;
            }
            case ACTOR_00400_NECK_RETRACTED: {
                // Shorten the neck while compensating the head's Z scale.

                gfxExtractEulerAngles(&headCoord->coord, &headAngles);
                work->neckScale = (u16)work->neckScale + ((ACTOR_00400_NECK_RETRACTED_SCALE - work->neckScale) >> 3);
                ACTOR_00400_APPLY_NECK_SCALE();
                coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(headCoord);
                break;
            }
        }
    } else {
        coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(headCoord);
        // Restore neck length before easing back toward the animated angles.
        if (work->neckScale < ACTOR_00400_NECK_RELEASE_SCALE_THRESHOLD) {

            gfxExtractEulerAngles(&headCoord->coord, &headAngles);
            work->neckScale = (u16)work->neckScale + ((ONE - work->neckScale) >> 3);
            ACTOR_00400_APPLY_NECK_SCALE();
        } else {
            MATRIX* lowerNeckMatrix;
            MATRIX* upperNeckMatrix;

            lowerNeckMatrix = &coords[2].coord;
            gfxExtractEulerAngles(lowerNeckMatrix, &lowerAnimatedAngles);
            upperNeckMatrix = &coords[3].coord;
            gfxExtractEulerAngles(upperNeckMatrix, &upperAnimatedAngles);
            work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((lowerAnimatedAngles.vx - work->lowerNeckAngles.vx) >> 1);
            work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((lowerAnimatedAngles.vy - work->lowerNeckAngles.vy) >> 1);
            work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((lowerAnimatedAngles.vz - work->lowerNeckAngles.vz) >> 1);
            work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((upperAnimatedAngles.vx - work->upperNeckAngles.vx) >> 1);
            work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((upperAnimatedAngles.vy - work->upperNeckAngles.vy) >> 1);
            work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((upperAnimatedAngles.vz - work->upperNeckAngles.vz) >> 1);
            gfxSetRotIdentity(&rotationMatrix);
            RotMatrix(&work->lowerNeckAngles, &rotationMatrix);
            _actor00400CopyRotation(&rotationMatrix, lowerNeckMatrix);
            gfxSetRotIdentity(&rotationMatrix);
            RotMatrix(&work->upperNeckAngles, &rotationMatrix);
            _actor00400CopyRotation(&rotationMatrix, upperNeckMatrix);
        }
        coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(headCoord);
        work->neckPhase = ACTOR_00400_NECK_FREE;
    }
#undef ACTOR_00400_APPLY_NECK_SCALE
}

/// Advances the flying shot's lifetime, gravity and parent-space translation.
///
/// Requires live shot work/root. Counts one running frame, adds gravity to the
/// signed halfword vertical velocity, then moves XYZ by the updated velocity.
/// The caller owns composition invalidation, contact tests and lifetime expiry.
static inline void _actor00400AdvanceShotFlight(_Actor00400ShotWork* work, GfxCoord* rootCoord)
{
    work->frames          += 1;
    work->velocity.vy     += ACTOR_00400_SHOT_GRAVITY;
    rootCoord->coord.t[0] += work->velocity.vx;
    rootCoord->coord.t[1] += work->velocity.vy;
    rootCoord->coord.t[2] += work->velocity.vz;
}

/// Moves the shot and bursts it on contact, a hide request or its 61st running tick.
///
/// Requires launched shot work, root and linked sphere with two initialized
/// contacts. Only RUNNING advances gravity, view-parent translation, trail
/// phase and lifetime. Player/companion, enemy or hazard contact bursts it;
/// grid contact does too except when the room's allowed surface-class bit is
/// present. Clears contacts after testing. An impact disables grid and pair
/// tests and enters linger with a zero counter; teardown later unlinks and
/// frees the task. Root composition is dirtied even on paused/hidden calls.
static void _actor00400FlyShot(Task* task)
{
    enum {
        ACTOR_00400_SHOT_PASS_SURFACE_CLASS_1 = 1 << 1,
        ACTOR_00400_SHOT_PASS_SURFACE_CLASS_3 = 1 << 3
    };
    _Actor00400ShotWork* work;
    s32                  burstRequested;
    GfxCoord*            rootCoord;
    WorldCollisionDelta  pushback;
    s32                  surfaceMask;
    s32                  contactIndex;
    s32                  gridContactResult;
    u16                  burstKind;

    burstRequested = 0;
    work           = task->work;
    rootCoord      = task->extra.coordBody->coord;
    // Keep this scalar store before the frame-gate load.
    *&rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    burstKind                 = DIVER_BURST_TRAIL;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            _actor00400AdvanceShotFlight(work, rootCoord);
            if (worldCollisionFindContactIndex(work->contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->contacts); contactIndex++) {
                    switch (work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
                        case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                            burstRequested = 1;
                            break;
                        case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                            burstRequested = 1;
                            break;
                        case DAMAGE_HAZARD_CATEGORY:
                            burstRequested = 1;
                            break;
                    }
                }
            }
            // Surface classes are room-local; these rooms allow passage through one class.
            gridContactResult = worldCollisionResolvePushback(work->contacts, &pushback, ARRAY_SIZE(work->contacts), &surfaceMask);
            if (gridContactResult <= WORLD_COLLISION_PUSHBACK_OPPOSED) {
                if (gridContactResult > WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
                    if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER &&
                        (gGameSession->location.loc.area == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR || gGameSession->location.loc.area == GAME_AREA_SHELTER_B4_LOWER_SEWER ||
                         gGameSession->location.loc.area == GAME_AREA_SHELTER_B4_UPPER_SEWER || gGameSession->location.loc.area == GAME_AREA_SHELTER_B4_RESERVOIR ||
                         gGameSession->location.loc.area == GAME_AREA_SHELTER_B2_SEPTIC_TANK)) {
                        if ((surfaceMask & ACTOR_00400_SHOT_PASS_SURFACE_CLASS_1) == 0) {
                            burstRequested = 1;
                        }
                    } else if (gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK &&
                               (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_PAVILION || gGameSession->location.loc.area == GAME_AREA_NEO_ARK_ISLAND ||
                                gGameSession->location.loc.area == GAME_AREA_NEO_ARK_BRIDGE)) {
                        if ((surfaceMask & ACTOR_00400_SHOT_PASS_SURFACE_CLASS_1) == 0) {
                            burstRequested = 1;
                        }
                    } else if (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY && gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK) {
                        if ((surfaceMask & ACTOR_00400_SHOT_PASS_SURFACE_CLASS_3) == 0) {
                            burstRequested = 1;
                        }
                    } else {
                        burstRequested = 1;
                    }
                }
            }
            // Disable the still-linked attack sphere before handing its lifetime to linger.
            worldCollisionClearContacts(work->contacts);
            if ((++task->killCountdown >= ACTOR_00400_SHOT_LIFETIME) || (gSceneCombatState.actor00400HideRequested != 0) || (burstRequested != 0)) {
                task->killCountdown           = 0;
                work->child.attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                burstKind                     = DIVER_BURST_IMPACT;
                task->state                  += 1;
            }
            _diverImpactBurst(rootCoord, work->frames, burstKind, ACTOR_00400_SHOT_BURST_SIZE_AND_SPRAY_BIAS);
            break;
    }
}

/// Claims the available emergence spot nearest the diver's target in XZ.
///
/// The diver borrows a writable room table at `surfaceSpots`, valid through
/// its terminating `pad == -1` entry and for the lifetime of every claim.
/// Entries 1 onward are eligible when unclaimed or already held by this
/// diver. The first equally near entry wins. Only the chosen XZ is copied
/// to `surfaceSpot`; Y is preserved. The old claim is released if it moves.
/// With no eligible entry, the position stays unchanged and index 0 becomes
/// current (and is marked claimed if the previous index was nonzero).
/// Requires initialized scratch storage for one search block.
static void _actor00400ClaimNearestSurfaceSpot(Task* task)
{
    enum {
        ACTOR_00400_SURFACE_SPOT_FREE    = 0,
        ACTOR_00400_SURFACE_SPOT_CLAIMED = 1,
        ACTOR_00400_SURFACE_SPOT_END     = -1
    };
    _Actor00400NearestSurfaceSpotScratch* scratch;
    _Actor00400Work*                      work;
    SVECTOR*                              spot;
    s16                                   spotIndex;
    s16                                   claimMark;
    s32                                   deltaX;
    s32                                   deltaZ;
    s32                                   distanceXZ;

    scratch               = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
    work                  = task->work;
    scratch->spotIndex    = 1;
    scratch->nearestIndex = 0;
    scratch->bestDistance = ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE;
    // Search from entry 1, admitting our own claim and keeping the first tie.
    // Start after the reserved entry and admit only free spots or our own claim.
    for (;;) {
        spotIndex = scratch->spotIndex;
        claimMark = work->surfaceSpots[spotIndex].pad;
        spot      = &work->surfaceSpots[spotIndex];
        if (claimMark != ACTOR_00400_SURFACE_SPOT_END) {
            if ((claimMark != ACTOR_00400_SURFACE_SPOT_CLAIMED) || (spotIndex == work->surfaceSpotIndex)) {
                scratch->delta.vx = deltaX = work->targetPos.vx - spot->vx;
                scratch->delta.vz = deltaZ = work->targetPos.vz - work->surfaceSpots[scratch->spotIndex].vz;
                distanceXZ                 = SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
                scratch->distance          = distanceXZ;
                if (distanceXZ < scratch->bestDistance) {
                    work->surfaceSpot.vx  = work->surfaceSpots[scratch->spotIndex].vx;
                    work->surfaceSpot.vz  = work->surfaceSpots[scratch->spotIndex].vz;
                    scratch->bestDistance = scratch->distance;
                    scratch->nearestIndex = scratch->spotIndex;
                }
            }
            scratch->spotIndex = scratch->spotIndex + 1;
        } else {
            break;
        }
    }
    // Move the claim only after the search; entry 0 is the retained fallback.
    if (work->surfaceSpotIndex != scratch->nearestIndex) {
        work->surfaceSpots[work->surfaceSpotIndex].pad = ACTOR_00400_SURFACE_SPOT_FREE;
        work->surfaceSpotIndex                         = scratch->nearestIndex;
        work->surfaceSpots[work->surfaceSpotIndex].pad = ACTOR_00400_SURFACE_SPOT_CLAIMED;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
}

/// Writes the XZ of the eligible surface spot nearest the cached party target.
///
/// Borrows a room-owned list with entry 0 reserved and a `pad == -1`
/// terminator reachable from entry 1 before the signed-halfword index wraps.
/// Claimed entries (`pad == 1`) are eligible only at this diver's claim index.
/// Keeps the first strict minimum of integer XZ distance; with no eligible
/// entry, leaves `nearestPosition` unchanged. The writable output must be
/// separate from the list and cached target. Preserves its Y and pad and all
/// claims. Requires one scratch-stack block; no pointer is retained.
static void _actor00400FindNearestSurfaceSpot(Task* task, SVECTOR* nearestPosition)
{
    enum {
        ACTOR_00400_SURFACE_SPOT_CLAIMED = 1,
        ACTOR_00400_SURFACE_SPOT_END     = -1
    };
    _Actor00400NearestSurfaceSpotScratch* scratch;
    const _Actor00400Work*                work;
    SVECTOR*                              spot;
    s16                                   spotIndex;
    s16                                   claimMark;
    s32                                   deltaX;
    s32                                   deltaZ;
    s32                                   distanceXZ;

    scratch               = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
    work                  = task->work;
    scratch->spotIndex    = 1;
    scratch->nearestIndex = 0;
    scratch->bestDistance = ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE;
    for (;;) {
        spotIndex = scratch->spotIndex;
        claimMark = work->surfaceSpots[spotIndex].pad;
        spot      = &work->surfaceSpots[spotIndex];
        if (claimMark != ACTOR_00400_SURFACE_SPOT_END) {
            if ((claimMark != ACTOR_00400_SURFACE_SPOT_CLAIMED) || (spotIndex == work->surfaceSpotIndex)) {
                scratch->delta.vx = deltaX = work->targetPos.vx - spot->vx;
                scratch->delta.vz = deltaZ = work->targetPos.vz - work->surfaceSpots[scratch->spotIndex].vz;
                distanceXZ                 = SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
                scratch->distance          = distanceXZ;
                if (distanceXZ < scratch->bestDistance) {
                    nearestPosition->vx   = work->surfaceSpots[scratch->spotIndex].vx;
                    nearestPosition->vz   = work->surfaceSpots[scratch->spotIndex].vz;
                    scratch->bestDistance = scratch->distance;
                    scratch->nearestIndex = scratch->spotIndex;
                }
            }
            scratch->spotIndex = scratch->spotIndex + 1;
            continue;
        }
        break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
}

/// Queues one subtractive blob-texture quad for the wounded diver's ground stain.
///
/// Borrows the four packed screen-XY words and projected depth in `scratch`
/// for this call. The caller must have accepted the projection flags, provide
/// one word-aligned `POLY_FT4` in the current frame arena and a current ordering
/// table with 1024 depth tags. Depth is converted to a masked byte offset; high
/// bits wrap. Intensity is 0..255, with red at half strength. Uses the 4-bit blob
/// atlas and its palette; the packet stays live until the frame DMA finishes.
static inline void _actor00400QueueGroundStain(const _Actor00400GroundStainScratch* scratch, u8 intensity)
{
    enum { ACTOR_00400_GROUND_STAIN_TEXTURE_4_BIT = 0 };
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setSemiTrans(quad, true);
    GPU_PRIMITIVE_XY_WORD(quad, 0) = scratch->screenCorners[0];
    GPU_PRIMITIVE_XY_WORD(quad, 1) = scratch->screenCorners[1];
    GPU_PRIMITIVE_XY_WORD(quad, 2) = scratch->screenCorners[2];
    GPU_PRIMITIVE_XY_WORD(quad, 3) = scratch->screenCorners[3];
    setUV4(quad, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
    quad->tpage = getTPage(ACTOR_00400_GROUND_STAIN_TEXTURE_4_BIT, GPU_BLEND_SUBTRACT, 512, 0);
    quad->clut  = getClut(48, 266);
    setRGB0(quad, intensity >> 1, intensity, intensity);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), quad);
}

/// Queues a subtractive blob-texture quad for the diver's ground stain.
///
/// The four readable corners are in the view coordinate's space, in quad vertex
/// order. `intensity` is 0..255, with half strength on red, leaving a red
/// stain. Projection flags below zero suppress drawing. Requires initialized
/// scratch storage and room in the frame arena for one `POLY_FT4`; changes
/// the GTE state and links the packet into the current ordering table.
static void _actor00400DrawGroundStain(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 intensity)
{
    _Actor00400GroundStainScratch* scratch;

    scratch                    = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400GroundStainScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->depth = RotTransPers4(corner0, corner1, corner2, corner3, &scratch->screenCorners[0], &scratch->screenCorners[1], &scratch->screenCorners[2], &scratch->screenCorners[3],
                                   &scratch->depthCue, &scratch->flags);
    if (scratch->flags >= 0) {
        _actor00400QueueGroundStain(scratch, intensity);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400GroundStainScratch);
}

#include "../../shared/diver_turn_joint.inc.c"

/* The state tables below are defined among the functions, not with the other
   declarations, because `.rodata` follows source order: each sits between the
   jump tables of the functions around it. */

/// Shot states, indexed by `Task::state` in `_actor00400ShotTask`.
static const TaskFuncTable3 Actor00400_D0002C = { {
    _actor00400LaunchShot,
    _actor00400FlyShot,
    _diverStrikeTeardown,
} };

/// The eight states `_actor00400BodyTask` dispatches on `Task::state`. The zero
/// word after it in the image is the alignment pad of
/// `Actor00400_Fn03920`'s jump table, not a terminator.
static const TaskFuncTable8 Actor00400_D00038 = { {
    Actor00400_Fn03920,
    _actor00400StrandedTask,
    _actor00400StrandedDeathTask,
    _actor00400SwimTask,
    _actor00400SwimDeathTask,
    _actor00400Despawn,
    _actor00400WoundedGroundTask,
    _actor00400WoundedFloatTask,
} };

/// Applies the current room's diver configuration and reports spawn rejection.
///
/// Requires the freshly allocated work and current session location. Returns
/// 1 for an absent or disabled row, 0 for an admitted spawn. Non-NULL room
/// pointers lend the surface-spot list and one of four waypoint rings (spawn
/// bits 4..5); the pointed-to water height is sampled once in coordinate units.
/// NULL fields and an absent grid flag preserve existing work values. Room
/// storage must outlive its divers.
static __inline__ s32 _actor00400ApplyAreaConfig(Task* task)
{
    enum {
        ACTOR_00400_WAYPOINT_SET_SHIFT = 4,
        ACTOR_00400_WAYPOINT_SET_MASK  = 3
    };
    const _Actor00400AreaConfig* config;
    _Actor00400Work*             work;
    const GameLocationKey*       location;
    u16                          flags;

    work     = task->work;
    location = &gGameSession->location.loc;
    config   = Actor00400_D15F20;
    while (config->stage != ACTOR_00400_AREA_CONFIG_END) {
        if ((location->stage == config->stage) && (location->area == config->area)) {
            flags = config->flags;
            if (flags & ACTOR_00400_AREA_CONFIG_NO_DIVER) {
                return 1;
            }
            if (flags & ACTOR_00400_AREA_CONFIG_GRID_COLLISION) {
                work->gridCollision = 1;
            }
            if (config->waypointSets != NULL) {
                work->waypoints = config->waypointSets[(task->spawnArg1.value >> ACTOR_00400_WAYPOINT_SET_SHIFT) & ACTOR_00400_WAYPOINT_SET_MASK];
            }
            if (config->surfaceSpots != NULL) {
                work->surfaceSpots = config->surfaceSpots;
            }
            if (config->waterLevel != NULL) {
                work->waterLevel = (u16)*config->waterLevel;
            }
            return 0;
        }
        config++;
    }
    return 1;
}

/// Selects the wounded diver's trunk as its tracked target and enables lock-on.
///
/// Borrows the live enemy, work and model part 1; clears all target flags.
/// Stores the low byte of `gridCollisionEnabled` as the work's grid setting.
/// Both spawn paths call this after linking the bodies, so this store does
/// not change the already-applied grid-body flags.
static __inline__ void _actor00400SelectTrunkTarget(Task* task, Enemy* enemy,
                                                    _Actor00400Work* work, s32 gridCollisionEnabled)
{
    enum { ACTOR_00400_WOUNDED_TARGET_PART = 1 };
    enemy->coord                  = &task->extra.tmd->coords[ACTOR_00400_WOUNDED_TARGET_PART];
    enemy->node.state.parts.flags = 0;
    work->gridCollision           = gridCollisionEnabled;
}

static void Actor00400_Fn03920(Task* arg0)
{
    _Actor00400Work*            work;
    _Actor00400Work*            w;
    _Actor00400Work*            anim;
    Enemy*                      obj;
    _Actor00400GroundStainWork* stain;
    Enemy*                      stainEnemy;
    GfxCoord*                   pos;
    GfxCoord*                   coord;
    Task*                       task;
    s32                         spawnRejected;
    s32                         nibble;
    s32                         index;
    u16                         y;
    s32*                        spawnArg;

    spawnArg              = &arg0->spawnArg1.value;
    gStageSceneMusicEntry = 0xB;
    obj                   = arg0->spawnArg2.pointer;
    coord                 = arg0->extra.tmd->coords;
    if ((*spawnArg >> 16) & 1) {
        enemyDestroy(obj, arg0);
        return;
    }
    arg0->work = memCalloc(sizeof(_Actor00400Work), 0);
    work       = arg0->work;
    if (work == NULL) {
        enemyDestroy(obj, arg0);
        return;
    }

    spawnRejected = _actor00400ApplyAreaConfig(arg0);
    if (spawnRejected) {
        enemyDestroy(obj, arg0);
        return;
    }

    if ((arg0->spawnArg1.value & 0xF) == 0) {
        work->gridCollision = 1;
    }
    _actor00400InitModelAndEnemy(arg0);
    arg0->msgTable = Actor00400_D16010;
    switch (arg0->spawnArg1.value & 0xF) {
        case 7:
            work->floatOffset = 200;
            work->inWater     = 1;
            work->targetPart  = 1;
            work->goalY       = (u16)work->waterLevel;
            _actor00400SelectTrunkTarget(arg0, obj, work, 0);
            w              = arg0->work;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 0x10;
            w->animRequest = DIVER_ANIM_REQUEST_RESET;
            w              = arg0->work;
            arg0->state    = 7;
            w->state       = 0;
            w->subState    = 0;
            sceneAcquireBattleRef(0);
            obj->hp = (s16)obj->hpMax / 8;
            break;
        case 6:
            work->inWater    = 0;
            work->targetPart = 1;
            _actor00400SelectTrunkTarget(arg0, obj, work, 1);
            w              = arg0->work;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 0xF;
            w->animRequest = DIVER_ANIM_REQUEST_RESET;
            w              = arg0->work;
            arg0->state    = 6;
            w->state       = 0;
            w->subState    = 0;
            sceneAcquireBattleRef(0);
            obj->hp    = (s16)obj->hpMax / 8;
            pos        = arg0->extra.tmd->coords;
            stainEnemy = arg0->spawnArg2.pointer;
            y          = (u16)pos->coord.t[1];
            task       = taskSpawnFromTable(Actor00400_D16028, 2, 0, 0);
            if (task != NULL) {
                stain = memCalloc(sizeof(_Actor00400GroundStainWork), 0);
                if (stain == NULL) {
                    taskKill(task);
                } else {
                    task->work            = stain;
                    stain->vertices[0].vx = (u16)pos->coord.t[0] - ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[0].vy = y;
                    stain->vertices[0].vz = (u16)pos->coord.t[2] - ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[1].vx = (u16)pos->coord.t[0] + ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[1].vy = y;
                    stain->vertices[1].vz = (u16)pos->coord.t[2] - ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[2].vx = (u16)pos->coord.t[0] - ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[2].vy = y;
                    stain->vertices[2].vz = (u16)pos->coord.t[2] + ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[3].vx = (u16)pos->coord.t[0] + ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->vertices[3].vy = y;
                    stain->vertices[3].vz = (u16)pos->coord.t[2] + ACTOR_00400_GROUND_STAIN_HALF_SIZE;
                    stain->intensity      = 0xFF;
                    stain->enemy          = stainEnemy;
                }
            }
            break;
        case 0:
            work->inWater       = 0;
            work->gridCollision = 1;
            w                   = arg0->work;
            w->animStep         = ANIMATION_RATE_ONE;
            w->animClip         = 2;
            w->animRequest      = DIVER_ANIM_REQUEST_RESET;
            arg0->state         = arg0->state + 1;
            break;
        case 4:
            work->inWater = 1;
            nibble        = gameFlagGetNibble(GAME_FLAG_0EB);
            if (nibble != 2) {
                obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                w                           = arg0->work;
                w->animStep                 = ANIMATION_RATE_ONE;
                w->animClip                 = 1;
                w->animRequest              = DIVER_ANIM_REQUEST_RESET;
                w                           = arg0->work;
                arg0->state                 = 3;
                w->state                    = 0;
                w->subState                 = 0;
                w                           = arg0->work;
                w->state                    = ACTOR_00400_SWIM_STATE_ROOM_INTRO;
                w->subState                 = 0;
            } else {
                w              = arg0->work;
                w->animStep    = ANIMATION_RATE_ONE;
                w->animClip    = 1;
                w->animRequest = nibble;
                w              = arg0->work;
                arg0->state    = 3;
                w->state       = 0;
                w->subState    = 0;
            }
            break;
        case 5:
            work->inWater = 1;
            nibble        = gameFlagGetNibble(GAME_FLAG_0EB);
            if (nibble != 2) {
                obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                work->inWater               = 1;
                w                           = arg0->work;
                w->animStep                 = ANIMATION_RATE_ONE;
                w->animClip                 = 1;
                w->animRequest              = DIVER_ANIM_REQUEST_RESET;
                w                           = arg0->work;
                arg0->state                 = 3;
                w->state                    = 0;
                w->subState                 = 0;
                w                           = arg0->work;
                w->state                    = ACTOR_00400_SWIM_STATE_AWAIT_FIGHT;
                w->subState                 = 0;
            } else {
                w              = arg0->work;
                w->animStep    = ANIMATION_RATE_ONE;
                w->animClip    = 1;
                w->animRequest = nibble;
                w              = arg0->work;
                arg0->state    = 3;
                w->state       = 0;
                w->subState    = 0;
            }
            break;
        case 1:
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 45, 0, 0)) {
                if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 0) {
                    work->inWater  = 1;
                    w              = arg0->work;
                    w->animStep    = ANIMATION_RATE_ONE;
                    w->animClip    = 1;
                    w->animRequest = DIVER_ANIM_REQUEST_RESET;
                    w              = arg0->work;
                    arg0->state    = 3;
                    w->state       = 0;
                    w->subState    = 0;
                } else {
                    work->gridCollision = 1;
                    w                   = arg0->work;
                    w->animStep         = ANIMATION_RATE_ONE;
                    w->animClip         = 2;
                    w->animRequest      = DIVER_ANIM_REQUEST_RESET;
                    work->inWater       = 0;
                    coord->coord.t[1]   = 0;
                    w                   = arg0->work;
                    arg0->state         = 1;
                    w->state            = 0;
                    w->subState         = 0;
                }
            } else {
                work->inWater  = 1;
                w              = arg0->work;
                w->animStep    = ANIMATION_RATE_ONE;
                w->animClip    = 1;
                w->animRequest = DIVER_ANIM_REQUEST_RESET;
                w              = arg0->work;
                arg0->state    = 3;
                w->state       = 0;
                w->subState    = 0;
            }
            break;
        case 3:
            work->inWater  = 1;
            w              = arg0->work;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 3;
            w->animRequest = DIVER_ANIM_REQUEST_RESET;
            _actor00400ClaimNearestSurfaceSpot(arg0);
            coord->coord.t[0] = work->surfaceSpot.vx;
            work->goalY       = (u16)work->waterLevel;
            coord->coord.t[1] = work->surfaceSpot.vy + work->waterLevel + 0x7D0;
            coord->coord.t[2] = work->surfaceSpot.vz;
            sceneAcquireBattleRef(0);
            w           = arg0->work;
            arg0->state = 3;
            w->state    = 0;
            w->subState = 0;
            w           = arg0->work;
            w->state    = ACTOR_00400_SWIM_STATE_DIVE;
            w->subState = 0;
            break;
        case 2:
            work->inWater    = 1;
            work->ambientOff = 1;
            w                = arg0->work;
            w->animStep      = ANIMATION_RATE_ONE;
            w->animClip      = 3;
            w->animRequest   = DIVER_ANIM_REQUEST_RESET;
            w                = arg0->work;
            arg0->state      = 3;
            w->state         = 0;
            w->subState      = 0;
            if ((arg0->spawnArg1.value & 0xF0) == 0) {
                w           = arg0->work;
                w->state    = ACTOR_00400_SWIM_STATE_TUNNEL_INTRO;
                w->subState = 0;
            } else {
                w           = arg0->work;
                w->state    = ACTOR_00400_SWIM_STATE_TUNNEL_PATROL;
                w->subState = 0;
            }
            break;
    }

    anim = arg0->work;
    if (anim->animRequest == DIVER_ANIM_REQUEST_BLEND) {
        if (anim->animPlaying != anim->animClip) {
            anim->animFrames = 0;
        } else {
            anim->animFrames = _actor00400ScaleFramesForAnimRate(arg0, anim->animFrames);
        }
        _actor00400BlendRequestedClip(arg0);
        anim->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (anim->animRequest == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(arg0);
        anim->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        anim->animFrames  = 0;
    } else if (anim->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
        anim->animFrames = (u16)anim->animFrames + 1;
    }
    for (index = 1; index < ARRAY_SIZE(anim->rig.slots); index++) {
        animationTickSlot(&anim->rig.anim, index);
    }
    work->field_620 = 0x1000;
    work->field_622 = 0x1000;
}

/// Relights the diver at a composed coordinate and applies its ambient override.
///
/// `task` owns the enemy in `spawnArg2.pointer`; `work` and `model` are that
/// diver's live work and model. `sampleCoord->workm` must already contain the
/// world transform to sample. The callers use the trunk (part 1).
/// `ambientOff` clears the model's ambient term after the lighting query.
/// Requires initialized scratch storage for a `VECTOR` plus the lighting
/// query's nested reservations; releases its temporary position before return.
static __inline__ void _actor00400UpdateModelColor(Task* task, GfxCoord* sampleCoord,
                                                   const _Actor00400Work* work, const TmdObject* model)
{
    VECTOR* worldPosition = SCRATCH_STACK_CURSOR(VECTOR) - 1;

    worldPosition->vx            = sampleCoord->workm.t[0];
    worldPosition->vy            = sampleCoord->workm.t[1];
    worldPosition->vz            = sampleCoord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = worldPosition;
    worldCoordUpdateActorColor(task->spawnArg2.pointer, worldPosition, 0, 0);
    if (work->ambientOff != 0) {
        worldCoordSetModelAmbientColor(model, 0, 0, 0);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// States `_actor00400SwimDeathTask` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_SWIM_DEATH_*`.
static const TaskFuncTable11 Actor00400_D0007C = { {
    _actor00400SwimDeathEnter,
    _actor00400SwimDeathFallWait,
    _actor00400SwimDeathRest,
    _actor00400SwimDeathBeginColorFade,
    _actor00400SwimDeathFade,
    _actor00400SwimDeathDarken,
    _actor00400SwimDeathHide,
    _actor00400SwimDeathBlastHide,
    _actor00400SwimDeathBlastWait,
    _actor00400SwimDeathBlastBurst,
    _actor00400SwimDeathBlastEnd,
} };

/// Drives the Bog Diver's swimming death task state.
///
/// Requires live work, enemy and model; state indexes 11 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// A running, unsuspended tick publishes slot-1 status after death dispatch,
/// retracts the neck except on blast deaths, and eases Y toward goalY plus
/// the signed low half of floatOffset. Paused frames only update model lighting.
static void _actor00400SwimDeathTask(Task* task)
{
    TaskFuncTable11  states;
    _Actor00400Work* work;
    TmdObject*       model;
    TmdObject*       lightingModel;
    _Actor00400Work* lightingWork;
    GfxCoord*        rootCoord;
    s32              rootY;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    model     = task->extra.tmd;
    states    = Actor00400_D0007C;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            states.funcs[work->state](task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            if (work->hitReaction != ACTOR_00400_HIT_REACTION_BLAST) {
                work->neckRetracted = 1;
                _actor00400UpdateNeckRetraction(task, 1);
            }
            rootY                 = rootCoord->coord.t[1];
            rootCoord->coord.t[1] = rootY + ((work->goalY + (s16)work->floatOffset - rootY) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            _actor00400UpdateModelColor(task, &lightingModel->coords[1], lightingWork, lightingModel);
            break;
    }
}

/// Queues the diver's hit sound with its placement tag and signed-byte spatial controls.
///
/// Requires the live enemy and root coordinate. Pan and depth are deliberately
/// narrowed to signed bytes before promotion to the sound request's word arguments.
static inline void _actor00400PlayHitSound(Task* task)
{
    const Enemy* enemy;
    s32          soundId;
    s32          pan;

    enemy   = task->spawnArg2.pointer;
    soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_HIT;
    pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Removes the dying swimmer from combat and enters its fall or blast sequence.
///
/// Requires the live enemy, model and four collision bodies. Releases the
/// battle reference and hit contacts before unlinking the bodies. A blast
/// selects its hide state; otherwise an exact spawn argument of 7 retains
/// the wounded floating pose, and other spawns request surfaced clip 1 at
/// triple rate. A root less than 800 units below water aims for the surface.
static void _actor00400SwimDeathEnter(Task* task)
{
    enum { ACTOR_00400_SWIM_DEATH_SURFACE_DEPTH = 800 };
    _Actor00400Work* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (task->extra.tmd->coords->coord.t[1] - work->waterLevel < ACTOR_00400_SWIM_DEATH_SURFACE_DEPTH) {
        work->goalY = work->waterLevel;
    }
    // Retire combat membership before choosing the visual death sequence.
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs = NULL;
    worldCollisionUnlinkBody(&work->trunkBody);
    worldCollisionUnlinkBody(&work->headBody);
    worldCollisionUnlinkBody(&work->attackBody);
    worldCollisionUnlinkBody(&work->gridBody);
    work->stateFrames = 0;
    if (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST) {
        _Actor00400Work* nextWork = task->work;
        nextWork->state           = ACTOR_00400_SWIM_DEATH_BLAST_HIDE;
        nextWork->subState        = 0;
        return;
    }
    if (task->spawnArg1.value != ACTOR_00400_SPAWN_WOUNDED_FLOAT) {
        _Actor00400Work* nextWork;
        _actor00400PlayHitSound(task);
        nextWork = task->work;
        _diverRequestClipBlend(nextWork, ACTOR_00400_ANIM_SURFACED, ANIMATION_RATE_ONE * 3, 4);
    }
    work->state++;
}

/// Advances the swimming death pose until its boundary, jump or settled status.
///
/// Requires the initialized body rig and published slot-1 status. Ticks slots
/// 1..14, then tests the published status from the preceding frame. Normal
/// spawns request clip 14 before entering the rest state; an exact spawn
/// argument of 7 enters rest after one tick without waiting or changing clip.
static void _actor00400SwimDeathFallWait(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work = task->work;
    _actor00400AdvanceAnimation(task);
    if (task->spawnArg1.value != ACTOR_00400_SPAWN_WOUNDED_FLOAT) {
        if (!_diverClipHasBoundaryOrJump(task)) {
            return;
        }
        requestWork = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_ENTER, ANIMATION_RATE_ONE, 4);
    }
    work->state++;
}

/// States `_actor00400StrandedTask` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_STRANDED_STATE_*` and the three recoil states.
static const TaskFuncTable10 Actor00400_D000A8 = { {
    _actor00400StrandedStart,
    _actor00400StrandedIdleWait,
    _actor00400StrandedDecision,
    _actor00400StrandedState3,
    _actor00400StrandedCrawl,
    _actor00400StrandedDischarge,
    _actor00400StrandedUnusedState6,
    _actor00400StrandedLightRecoil,
    _actor00400StrandedHeavyRecoil,
    _actor00400StrandedStatusHold,
} };

/// Replaces the diver root's basis with its stored roll followed by heading.
///
/// Requires live work and root. Angles use 4096 units per turn; pitch is
/// ignored. Starts from Q12 identity and applies Z then Y rotation, discarding
/// prior scale and pitch while preserving translation and alignment bytes.
/// Marks composition dirty without recomposing the coordinate.
static inline void _actor00400ApplyRootRotation(Task* task)
{
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    MATRIX           rotationMatrix;
    MATRIX*          rootBasis;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    gfxSetRotIdentity(&rotationMatrix);
    RotMatrixZ(work->rotation.vz, &rotationMatrix);
    RotMatrixY(work->rotation.vy, &rotationMatrix);
    rootBasis               = &rootCoord->coord;
    rootBasis->m[0][0]      = rotationMatrix.m[0][0];
    rootBasis->m[0][1]      = rotationMatrix.m[0][1];
    rootBasis->m[0][2]      = rotationMatrix.m[0][2];
    rootBasis->m[1][0]      = rotationMatrix.m[1][0];
    rootBasis->m[1][1]      = rotationMatrix.m[1][1];
    rootBasis->m[1][2]      = rotationMatrix.m[1][2];
    rootBasis->m[2][0]      = rotationMatrix.m[2][0];
    rootBasis->m[2][1]      = rotationMatrix.m[2][1];
    rootBasis->m[2][2]      = rotationMatrix.m[2][2];
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Drives the Bog Diver's living stranded task state.
///
/// Requires live work, enemy and model; state indexes 10 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// Running, unsuspended ticks select targets, dispatch behavior, step discharge
/// and animation, then publish slot-1 status. Look, root rotation and contacts
/// follow; nonpositive signed HP selects stranded death at state/substate zero.
/// Running or paused frames update lighting and draw full-strength limb shadows.
static void _actor00400StrandedTask(Task* task)
{
    _Actor00400Work* work  = task->work;
    Enemy*           enemy = task->spawnArg2.pointer;
    TmdObject*       model = task->extra.tmd;
    TaskFuncTable10  states;
    _Actor00400Work* deathWork;
    _Actor00400Work* lightingWork;
    TmdObject*       lightingModel;

    states = Actor00400_D000A8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            work->bobPhase++;
            work->frameCount++;
            _actor00400UpdatePartyTarget(task);
            states.funcs[work->state](task);
            _actor00400TickDischarge(task);
            _actor00400AdvanceAnimation(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor00400UpdateStrandedLook(task, work->lookDisabled);
            _actor00400ApplyRootRotation(task);
            _actor00400ApplyContacts(task);
            if ((s16)enemy->hp <= 0) {
                deathWork           = task->work;
                task->state         = ACTOR_00400_TASK_STRANDED_DEATH;
                deathWork->state    = ACTOR_00400_STRANDED_DEATH_START;
                deathWork->subState = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            _actor00400UpdateModelColor(task, &lightingModel->coords[1], lightingWork, lightingModel);
            _actor00400DrawLimbShadows(task, task->extra.tmd->coords->coord.t[1], ACTOR_00400_LIVING_SHADOW_SHADE);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// Holds the stranded light recoil, restarting it on another light hit.
///
/// Requires the live rig and enemy. A repeated light hit queues a normal-rate
/// restart and its sound, preserving the pending reaction on that early return.
/// Otherwise consumes hit reactions; without a state change, a clip boundary,
/// jump or settled pose selects the stranded decision state.
static void _actor00400StrandedLightRecoilWait(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;
    s32              soundId;

    work = task->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = ACTOR_00400_ANIM_STRANDED_RECOIL_LIGHT;
        work->animRequest = DIVER_ANIM_REQUEST_RESET;
        soundId           = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_HIT;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        return;
    }
    if ((_actor00400ApplyHitReaction(task) << 0x10) == 0) {
        if (_diverClipHasBoundaryOrJump(task)) {
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            nextStateWork->subState = 0;
        }
    }
}

/// Holds the stranded heavy recoil, blending back into it on another heavy hit.
///
/// Requires the live rig and enemy. A repeated heavy hit plays its sound and
/// queues a six-frame normal-rate blend, preserving the reaction on return.
/// Other reactions may change state; otherwise a clip boundary, jump or settled
/// pose returns to the stranded decision state.
static void _actor00400StrandedHeavyRecoilWait(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* animWork;
    _Actor00400Work* nextStateWork;
    s32              soundId;

    work = task->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_HEAVY) {
        soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_HIT;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        animWork = task->work;
        _diverRequestClipBlend(animWork, ACTOR_00400_ANIM_STRANDED_RECOIL_HEAVY, ANIMATION_RATE_ONE, 6);
        return;
    }
    if ((_actor00400ApplyHitReaction(task) << 0x10) == 0) {
        if (_diverClipHasBoundaryOrJump(task)) {
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            nextStateWork->subState = 0;
        }
    }
}

/// States `_actor00400StrandedDeathTask` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_STRANDED_DEATH_*`.
static const TaskFuncTable10 Actor00400_D000D0 = { {
    _actor00400StrandedDeathEnter,
    _actor00400StrandedDeathFallWait,
    _actor00400StrandedDeathWeigh,
    _actor00400StrandedDeathFadeWait,
    _actor00400StrandedDeathShrink,
    _actor00400StrandedDeathEnd,
    _actor00400StrandedDeathBlastHide,
    _actor00400StrandedDeathBlastWait,
    _actor00400StrandedDeathBlastBurst,
    _actor00400StrandedDeathBlastEnd,
} };

/// Drives the Bog Diver's stranded death task state.
///
/// Requires live work, enemy and model; state indexes 10 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// Running, unsuspended ticks publish slot-1 status after death dispatch.
/// Running or paused frames update model lighting and draw fading limb shadows.
static void _actor00400StrandedDeathTask(Task* task)
{
    TaskFuncTable10  states;
    _Actor00400Work* work;
    TmdObject*       model;
    TmdObject*       lightingModel;
    _Actor00400Work* lightingWork;
    GfxCoord*        trunkCoord;

    work   = task->work;
    model  = task->extra.tmd;
    states = Actor00400_D000D0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            states.funcs[work->state](task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            trunkCoord    = &lightingModel->coords[1];
            _actor00400UpdateModelColor(task, trunkCoord, lightingWork, lightingModel);
            _actor00400DrawLimbShadows(task, task->extra.tmd->coords->coord.t[1], work->shadowShade);
            break;
    }
}

/// Unlinks the stranded diver from combat and starts its death or blast sequence.
///
/// Requires the live enemy, model and four collision bodies. Clears the hit
/// records, unlinks the bodies and target, and releases the battle reference.
/// A blast selects the hide state; otherwise queues normal-rate clip 15 with
/// an eight-frame blend, clears elapsed frames and plays the hit sound.
static void _actor00400StrandedDeathEnter(Task* task)
{
    enum { ACTOR_00400_LIVING_SHADOW_SHADE = 128 };
    _Actor00400Work* work;
    Enemy*           enemy;
    _Actor00400Work* nextWork;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Retire combat membership before choosing the visual death sequence.
    enemy->recs = NULL;
    worldCollisionUnlinkBody(&work->gridBody);
    worldCollisionUnlinkBody(&work->trunkBody);
    worldCollisionUnlinkBody(&work->headBody);
    worldCollisionUnlinkBody(&work->attackBody);
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    work->shadowShade = ACTOR_00400_LIVING_SHADOW_SHADE;
    if (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST) {
        nextWork           = task->work;
        nextWork->state    = ACTOR_00400_STRANDED_DEATH_BLAST_HIDE;
        nextWork->subState = 0;
        return;
    }
    nextWork = task->work;
    _diverRequestClipBlend(nextWork, ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_ENTER, ANIMATION_RATE_ONE, 8);
    work->stateFrames = 0;
    _actor00400PlayHitSound(task);
    work->state++;
}

/// States `_actor00400SwimTask` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_SWIM_STATE_*` and the three `ACTOR_00400_STATE_*`.
static const _Actor00400SwimStateTable Actor00400_D000F8 = { {
    _actor00400SwimStart,
    _actor00400SwimPatrol,
    _actor00400SwimDecide,
    Actor00400_Fn078C8,
    _actor00400Dive,
    _actor00400SwimUnusedState5,
    _actor00400SwimUnusedState6,
    _actor00400SwimLightRecoil,
    Actor00400_Fn079FC,
    _actor00400SwimStatusHold,
    _actor00400SwimAttack,
    _actor00400TunnelPatrol,
    _actor00400TunnelIntro,
    _actor00400RoomIntro,
    _actor00400AwaitFight,
} };

/// Sets the swimming diver's lock eligibility from its target part's depth.
///
/// Requires live work, enemy and model, target part 1 or 4 and a complete
/// coordinate chain to the view. The part origin and borrowed room water
/// height use world game-coordinate units, positive Y downward. More than
/// 400 units below water writes NOT_LOCKABLE; equality or above writes zero.
/// Replaces the entire flags byte and preserves target-list membership.
static inline void _actor00400UpdateLockable(Task* task)
{
    enum { ACTOR_00400_LOCKABLE_MAX_DEPTH = 400 };
    _Actor00400Work* work;
    TmdObject*       model;
    Enemy*           enemy;
    GfxCoord*        targetCoord;
    SVECTOR          targetPosition;

    work              = task->work;
    model             = task->extra.tmd;
    enemy             = task->spawnArg2.pointer;
    targetCoord       = &model->coords[work->targetPart];
    targetPosition.vx = 0;
    targetPosition.vy = 0;
    targetPosition.vz = 0;
    _actorRenderTransformPointToWorld(targetCoord, &targetPosition);
    if (work->waterLevel + ACTOR_00400_LOCKABLE_MAX_DEPTH < targetPosition.vy) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        enemy->node.state.parts.flags = 0;
    }
}

/// Drives the Bog Diver's living swimming task state.
///
/// Requires live work, enemy and model; state indexes 15 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// Running, unsuspended ticks dispatch behavior, discharge and animation before
/// publishing slot-1 status; neck/root pose and contacts follow. Signed HP <= 0
/// selects swimming death. Goal Y eases by 1/16, with patrol bob and hit shake;
/// target placement and lock eligibility follow the pose. Paused frames relight.
/// Two corridor views hide the model after either gate; suspension returns early.
static void _actor00400SwimTask(Task* task)
{
    enum {
        ACTOR_00400_SWIM_BOB_PHASE_SHIFT     = 6,
        ACTOR_00400_SWIM_BOB_AMPLITUDE       = 16,
        ACTOR_00400_SWIM_SHAKE_PHASE_SHIFT   = 10,
        ACTOR_00400_SWIM_SHAKE_SCALE         = 16,
        ACTOR_00400_CORRIDOR_HIDE_FIRST_VIEW = 10,
        ACTOR_00400_CORRIDOR_HIDE_VIEW_COUNT = 2U
    };
    _Actor00400Work*          work      = task->work;
    GfxCoord*                 rootCoord = task->extra.tmd->coords;
    Enemy*                    enemy     = task->spawnArg2.pointer;
    TmdObject*                model     = task->extra.tmd;
    _Actor00400SwimStateTable states;
    _Actor00400Work*          cooldownWork;
    _Actor00400Work*          deathWork;
    _Actor00400Work*          lightingWork;
    TmdObject*                lightingModel;
    TmdObject*                visibilityModel;
    GameLocationKey*          location;

    states = Actor00400_D000F8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                return;
            }
            work->bobPhase++;
            work->frameCount++;
            _actor00400UpdatePartyTarget(task);
            states.funcs[work->state](task);
            _actor00400TickDischarge(task);
            cooldownWork = task->work;
            if (cooldownWork->emergeCooldown != 0) {
                cooldownWork->emergeCooldown--;
            }
            _actor00400AdvanceAnimation(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor00400UpdateNeckRetraction(task, work->neckRetracted);
            _actor00400ApplyRootRotation(task);
            _actor00400ApplyContacts(task);
            if ((s16)enemy->hp <= 0) {
                deathWork           = task->work;
                task->state         = ACTOR_00400_TASK_SWIM_DEATH;
                deathWork->state    = ACTOR_00400_SWIM_DEATH_START;
                deathWork->subState = 0;
            }
            rootCoord->coord.t[1] += (work->goalY - rootCoord->coord.t[1]) >> 4;
            if (work->state < ACTOR_00400_SWIM_STATE_TUNNEL_PATROL) {
                rootCoord->coord.t[1] += (rsin(work->bobPhase << ACTOR_00400_SWIM_BOB_PHASE_SHIFT) * ACTOR_00400_SWIM_BOB_AMPLITUDE) >> 12;
            }
            if (work->shakeFrames != 0) {
                work->shakeFrames--;
                rootCoord->coord.t[1] += (rsin(work->frameCount << ACTOR_00400_SWIM_SHAKE_PHASE_SHIFT) * ACTOR_00400_SWIM_SHAKE_SCALE) >> ACTOR_00400_SWIM_SHAKE_PHASE_SHIFT;
            }
            enemy->coord = &task->extra.tmd->coords[work->targetPart];
            if (work->state < ACTOR_00400_SWIM_STATE_TUNNEL_PATROL) {
                _actor00400UpdateLockable(task);
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            _actor00400UpdateModelColor(task, &lightingModel->coords[1], lightingWork, lightingModel);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    location        = &gGameSession->location.loc;
    visibilityModel = task->extra.tmd;
    if (location->stage == GAME_STAGE_MINE_SHELTER && location->area == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR && (u32)(gGameSession->location.loc.view - ACTOR_00400_CORRIDOR_HIDE_FIRST_VIEW) < ACTOR_00400_CORRIDOR_HIDE_VIEW_COUNT) {
        visibilityModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Steps the diver's heading toward a point outside a yaw deadband.
///
/// The target's XZ is in the root's parent space. Angles are 4096ths of a
/// turn; the shortest signed difference is in -2048..2047. Callers pass
/// nonnegative `yawStep` and `deadband`; a turn uses that fixed increment,
/// even if it overshoots the target. Dirties the root even without a turn
/// and changes the GTE state while normalizing the horizontal direction.
static inline void _actor00400TurnTowardPoint(Task* task, const SVECTOR* target, s32 yawStep, s32 deadband)
{
    _Actor00400Work* work = task->work;
    GfxCoord*        rootCoord;
    SVECTOR          direction;
    s32              yawDifference;
    s32              targetYaw;
    u16              currentYaw;

    rootCoord               = task->extra.tmd->coords;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    direction.vx            = target->vx - rootCoord->coord.t[0];
    direction.vy            = 0;
    direction.vz            = target->vz - rootCoord->coord.t[2];
    VectorNormalSS(&direction, &direction);
    targetYaw     = ratan2(direction.vx, direction.vz);
    currentYaw    = work->rotation.vy;
    yawDifference = ((currentYaw - targetYaw) << 20) >> 20;
    if (yawDifference > deadband) {
        work->rotation.vy = currentYaw - yawStep;
    } else if (yawDifference < -deadband) {
        work->rotation.vy = currentYaw + yawStep;
    }
}

/// Emits sixteen water-spray particles around the diver's surface crossing.
///
/// Borrows live work and root; both use the view-parent coordinate frame.
/// Offsets form a 512-unit XZ ring with signed-halfword Y at waterLevel minus
/// root Y plus 250. The root's current basis transforms these offsets.
/// The room selects the particle task; its recipe requests size 328, two
/// running ticks per cell, speed 32 and upward-burst velocity. Effects snapshot
/// XYZ during the spawn; the temporary vector is reused for each particle.
static inline void _actor00400SpawnSurfaceSprayRing(Task* task, const _Actor00400Work* work, const GfxCoord* rootCoord)
{
    enum {
        ACTOR_00400_SURFACE_SPRAY_COUNT      = 16,
        ACTOR_00400_SURFACE_SPRAY_ANGLE_STEP = 256,
        ACTOR_00400_SURFACE_SPRAY_Y_OFFSET   = 250,
        // Size 328, two ticks/cell, speed 32, upward-burst velocity kind.
        ACTOR_00400_SURFACE_SPRAY_RECIPE = 0x01202148
    };
    GfxCoord* spawnCoord;
    SVECTOR   offset;
    s32       particleIndex;
    s16       surfaceOffsetY;

    particleIndex  = 0;
    surfaceOffsetY = work->waterLevel - rootCoord->coord.t[1] + ACTOR_00400_SURFACE_SPRAY_Y_OFFSET;
    spawnCoord     = task->extra.tmd->coords;
    do {
        offset.vx = (u32)rsin(particleIndex * ACTOR_00400_SURFACE_SPRAY_ANGLE_STEP) >> 3;
        offset.vy = surfaceOffsetY;
        offset.vz = (u32)rcos(particleIndex * ACTOR_00400_SURFACE_SPRAY_ANGLE_STEP) >> 3;
        effectSpawn(gRoomEffectWaterSprayId, spawnCoord, ACTOR_00400_SURFACE_SPRAY_RECIPE, &offset);
        particleIndex++;
    } while (particleIndex < ACTOR_00400_SURFACE_SPRAY_COUNT);
}

/// Finishes the patrol's surface cycle, or dives early when a nearby target alerts it.
///
/// Requires live work/root, an eight-point patrol ring and waypointIndex 0..7.
/// Starting at tick 20, a target within 3500 units outside the rear 1024-unit
/// bearing arc engages battle and returns before surface cues or clip handling.
/// Otherwise releases the neck at 8, emits spray at 12 and plays the surface
/// cue at 20. A published clip boundary/jump emits another ring, restores the
/// unsigned waypoint depth plus water level, requests normal-rate surfaced
/// clip 1 with four-frame blending and advances to the clip-wait substate.
static void _actor00400SwimPatrolSurface(Task* task)
{
    enum {
        ACTOR_00400_PATROL_SURFACE_NECK_FRAME  = 8,
        ACTOR_00400_PATROL_SURFACE_SPRAY_FRAME = 12,
        ACTOR_00400_PATROL_SURFACE_ALERT_FRAME = 20,
        ACTOR_00400_PATROL_ALERT_DISTANCE      = 3500,
        ACTOR_00400_PATROL_REAR_ARC_START      = 1536,
        ACTOR_00400_PATROL_REAR_ARC_WIDTH      = 1024U,
        ACTOR_00400_PATROL_YAW_STEP            = 32,
        ACTOR_00400_PATROL_YAW_DEADBAND        = 48,
        ACTOR_00400_PATROL_WAYPOINT_MASK       = 7
    };
    _Actor00400Work* work;
    _Actor00400Work* targetWork;
    _Actor00400Work* diveWork;
    _Actor00400Work* requestWork;
    GfxCoord*        rootCoord;
    s8               battleStarted;
    s32              neckSoundId;
    s32              neckPan;
    s32              surfaceSoundId;
    s32              surfacePan;
    s32              diveSoundId;
    s32              divePan;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= ACTOR_00400_PATROL_SURFACE_ALERT_FRAME) {
        targetWork    = task->work;
        battleStarted = 0;
        if (targetWork->targetDistance < ACTOR_00400_PATROL_ALERT_DISTANCE && (u32)(targetWork->targetBearing - ACTOR_00400_PATROL_REAR_ARC_START) >= ACTOR_00400_PATROL_REAR_ARC_WIDTH) {
            gSceneCombatState.signals.bytes.enemyAlert = 1;
            sceneEngageBattle(1);
            battleStarted      = 1;
            diveWork           = task->work;
            diveWork->state    = ACTOR_00400_SWIM_STATE_DIVE;
            diveWork->subState = 0;
        }
        if (battleStarted) {
            return;
        }
        _actor00400TurnTowardPoint(task, &work->waypoints[work->waypointIndex & ACTOR_00400_PATROL_WAYPOINT_MASK], ACTOR_00400_PATROL_YAW_STEP, ACTOR_00400_PATROL_YAW_DEADBAND);
    }
    if (work->stateFrames == ACTOR_00400_PATROL_SURFACE_NECK_FRAME) {
        work->neckRetracted = 0;
        neckSoundId         = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_NECK_RELEASE;
        neckPan             = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(neckSoundId, neckPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->stateFrames == ACTOR_00400_PATROL_SURFACE_SPRAY_FRAME) {
        _actor00400SpawnSurfaceSprayRing(task, work, rootCoord);
    }
    if (work->stateFrames == ACTOR_00400_PATROL_SURFACE_ALERT_FRAME) {
        surfaceSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_SURFACE_CUE;
        surfacePan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(surfaceSoundId, surfacePan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        _actor00400SpawnSurfaceSprayRing(task, work, rootCoord);
        diveSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_DIVE;
        divePan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(diveSoundId, divePan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->goalY = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
        requestWork = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SURFACED, ANIMATION_RATE_ONE, 4);
        work->subState++;
    }
}

/// Advances the decision-history write cursor, wrapping after its third entry.
///
/// Requires a writable work block and an index in 0..2. The caller has already
/// stored the newest choice; the byte-sized increment selects its successor.
static inline void _actor00400AdvanceDecisionHistory(_Actor00400Work* work)
{
    u8 nextHistoryIndex;

    nextHistoryIndex        = work->stateHistoryIndex + 1;
    work->stateHistoryIndex = nextHistoryIndex;
    if (nextHistoryIndex >= (u32)ARRAY_SIZE(work->stateHistory)) {
        work->stateHistoryIndex = 0;
    }
}

/// Chooses a swimming attack or dive and limits runs of attacks to two.
///
/// Requires current target distance/bearing and a history index in 0..2.
/// Distances are signed halfword horizontal coordinate units; bearings are
/// 0..4095 per turn. Within 10000 units and 191 angle units either side of
/// forward, one LCG bit chooses attack or a dive with a 90-frame emerge delay.
/// Three recorded attacks replace the newest with a delayed dive. A random
/// refusal leaves history alone; an out-of-reach dive records its choice.
static void _actor00400SwimDecide(Task* task)
{
    enum {
        ACTOR_00400_SWIM_ATTACK_DISTANCE       = 10000,
        ACTOR_00400_SWIM_ATTACK_EXCLUDED_START = 192,
        ACTOR_00400_SWIM_ATTACK_EXCLUDED_WIDTH = 3713U,
        ACTOR_00400_SWIM_DECISION_EMERGE_DELAY = 90
    };
    _Actor00400Work* work;
    _Actor00400Work* attackWork;
    _Actor00400Work* fallbackWork;
    _Actor00400Work* declinedWork;
    _Actor00400Work* outOfReachWork;
    u32              randomValue;
    s16              fallbackState;

    work = task->work;
    if ((_actor00400ApplyHitReaction(task) << 0x10) == 0) {
        if (work->targetDistance < ACTOR_00400_SWIM_ATTACK_DISTANCE && (u32)(work->targetBearing - ACTOR_00400_SWIM_ATTACK_EXCLUDED_START) >= ACTOR_00400_SWIM_ATTACK_EXCLUDED_WIDTH) {
            randomValue     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomValue;
            if ((randomValue >> 16) & 1) {
                attackWork                                              = task->work;
                attackWork->state                                       = ACTOR_00400_SWIM_STATE_ATTACK;
                attackWork->subState                                    = 0;
                attackWork->stateHistory[attackWork->stateHistoryIndex] = attackWork->state;
                fallbackState                                           = ACTOR_00400_SWIM_STATE_DIVE;
                // Replace the newest choice when it completes three consecutive attacks.
                if (attackWork->stateHistory[0] == attackWork->stateHistory[1] &&
                    attackWork->stateHistory[0] == attackWork->stateHistory[2] && attackWork->stateHistory[0] == ACTOR_00400_SWIM_STATE_ATTACK) {
                    fallbackWork                                            = task->work;
                    fallbackWork->state                                     = fallbackState;
                    fallbackWork->subState                                  = 0;
                    attackWork->stateHistory[attackWork->stateHistoryIndex] = fallbackState;
                    attackWork->emergeCooldown                              = ACTOR_00400_SWIM_DECISION_EMERGE_DELAY;
                }
                _actor00400AdvanceDecisionHistory(attackWork);
            } else {
                work->emergeCooldown   = ACTOR_00400_SWIM_DECISION_EMERGE_DELAY;
                declinedWork           = task->work;
                declinedWork->state    = ACTOR_00400_SWIM_STATE_DIVE;
                declinedWork->subState = 0;
            }
        } else {
            outOfReachWork                              = task->work;
            outOfReachWork->state                       = ACTOR_00400_SWIM_STATE_DIVE;
            outOfReachWork->subState                    = 0;
            work->stateHistory[work->stateHistoryIndex] = work->state;
            _actor00400AdvanceDecisionHistory(work);
        }
    }
}

static void Actor00400_Fn058C4(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    _Actor00400Work* w;
    _Actor00400Work* w2;
    GfxCoord*        coord;
    SVECTOR          nearestSpotOffset;
    SVECTOR          nearestSurfaceSpot;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s16              next;
    u8               idx;
    u8               idx2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    coord->coord.t[0] += (work->emergePos.vx - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += (work->emergePos.vz - coord->coord.t[2]) >> 4;
    work->goalY        = work->waterLevel + 0x64;
    if (work->stateFrames == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0xC) {
        _actor00400SpawnSurfaceSprayRing(arg0, work, coord);
    }
    if (work->stateFrames == 0x14) {
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040004;
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames < 0x15) {
        return;
    }
    _actor00400TurnTowardPoint(arg0, &work->targetPos, 0x18, 0x30);
    if (work->stateFrames < 0x1F) {
        return;
    }
    if (work->targetDistance < 0x2710 && (u32)(work->targetBearing - 0xC0) >= 0xE81U) {
        work2                                         = arg0->work;
        work2->state                                  = ACTOR_00400_SWIM_STATE_ATTACK;
        work2->subState                               = 0;
        work2->stateHistory[work2->stateHistoryIndex] = work2->state;
        next                                          = ACTOR_00400_SWIM_STATE_DIVE;
        if (work2->stateHistory[0] == work2->stateHistory[1] &&
            work2->stateHistory[0] == work2->stateHistory[2] && work2->stateHistory[0] == ACTOR_00400_SWIM_STATE_ATTACK) {
            w                                             = arg0->work;
            w->state                                      = next;
            w->subState                                   = 0;
            work2->stateHistory[work2->stateHistoryIndex] = next;
            work2->emergeCooldown                         = 90;
        }
        idx                      = work2->stateHistoryIndex + 1;
        work2->stateHistoryIndex = idx;
        if (idx >= (u32)ARRAY_SIZE(work2->stateHistory)) {
            work2->stateHistoryIndex = 0;
        }
    } else {
        nearestSurfaceSpot.vx = nearestSurfaceSpot.vy = nearestSurfaceSpot.vz = 0;
        _actor00400FindNearestSurfaceSpot(arg0, &nearestSurfaceSpot);
        nearestSpotOffset.vx = nearestSurfaceSpot.vx - coord->coord.t[0];
        nearestSpotOffset.vy = 0;
        nearestSpotOffset.vz = nearestSurfaceSpot.vz - coord->coord.t[2];
        if ((s16)SquareRoot0(nearestSpotOffset.vx * nearestSpotOffset.vx + nearestSpotOffset.vz * nearestSpotOffset.vz) >= 0xDAC) {
            _actor00400ClaimNearestSurfaceSpot(arg0);
            w2           = arg0->work;
            w2->state    = ACTOR_00400_SWIM_STATE_DIVE;
            w2->subState = 0;
        }
        work->stateHistory[work->stateHistoryIndex] = work->state;
        idx2                                        = work->stateHistoryIndex + 1;
        work->stateHistoryIndex                     = idx2;
        if (idx2 >= (u32)ARRAY_SIZE(work->stateHistory)) {
            work->stateHistoryIndex = 0;
        }
    }
}

/// Starts a dive by retracting the neck and selecting the waypoint's submerged height.
///
/// Requires live work/root and waypointIndex 0..7. Uses the waypoint's unsigned
/// low-halfword depth plus water level and selects black colour. If clip 3 is
/// already requested, advances twice to skip the clip wait. Otherwise emits a
/// surface spray ring and dive sound, requests clip 1 at normal rate with a
/// four-frame blend, and advances once into that wait.
static void _actor00400DiveEnter(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    GfxCoord*        rootCoord;
    s32              soundId;
    s32              pan;

    rootCoord           = task->extra.tmd->coords;
    work                = task->work;
    work->neckRetracted = 1;
    work->goalY         = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    if (work->animClip != ACTOR_00400_ANIM_SWIM) {
        _actor00400SpawnSurfaceSprayRing(task, work, rootCoord);
        soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_DIVE;
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        requestWork = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SURFACED, ANIMATION_RATE_ONE, 4);
    } else {
        work->subState++;
    }
    work->subState++;
}

/// Swims toward a claimed surface spot until it is near enough to emerge.
///
/// Requires the live root, enemy, eight-waypoint ring (index 0..7), writable
/// room-owned surface spot list and initialized scratch stack for its search.
/// Searches for the eligible spot nearest the party target each tick; with
/// none, keeps the previous spot position and uses the list's entry-0 fallback.
/// With a signed-halfword XZ distance below 800 and no emerge delay, restores
/// normal colour and enters emerge; otherwise swims 96 units per tick, with
/// a 48-unit yaw step and 256-unit deadband (4096 per turn). The swim sound
/// plays when the frame counter's low four bits are zero.
static void _actor00400DiveSwimToSurfaceSpot(Task* task)
{
    enum {
        ACTOR_00400_DIVE_ARRIVAL_DISTANCE = 800,
        ACTOR_00400_DIVE_STEP_DISTANCE    = 96,
        ACTOR_00400_DIVE_SOUND_PERIOD     = 16
    };
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          spotOffset;
    s32              soundId;
    s32              panOffset;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _actor00400ClaimNearestSurfaceSpot(task);
    // Measure the XZ displacement after narrowing it to signed halfwords.
    spotOffset.vx = work->surfaceSpot.vx - rootCoord->coord.t[0];
    spotOffset.vy = 0;
    spotOffset.vz = work->surfaceSpot.vz - rootCoord->coord.t[2];
    work->goalY   = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    if ((s16)SquareRoot0(spotOffset.vx * spotOffset.vx + spotOffset.vz * spotOffset.vz) < ACTOR_00400_DIVE_ARRIVAL_DISTANCE && work->emergeCooldown == 0) {
        _Actor00400Work* nextWork;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->stateFrames  = 0;
        nextWork           = task->work;
        nextWork->state    = ACTOR_00400_SWIM_STATE_EMERGE;
        nextWork->subState = 0;
        return;
    }
    if (work->animClip != ACTOR_00400_ANIM_SWIM) {
        _Actor00400Work* nextWork;
        nextWork = task->work;
        _diverRequestClipBlend(nextWork, ACTOR_00400_ANIM_SWIM, ANIMATION_RATE_ONE, 10);
    }
    _actor00400TurnTowardPoint(task, &work->surfaceSpot, 0x30, 0x100);
    _diverStepForward(task, ACTOR_00400_DIVE_STEP_DISTANCE, work->rotation.vy);
    if (!(work->frameCount & (ACTOR_00400_DIVE_SOUND_PERIOD - 1))) {
        const Enemy* enemy;

        enemy     = task->spawnArg2.pointer;
        soundId   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_SWIM;
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Holds the swimming light recoil, restarting it on another light hit.
///
/// Requires the live rig and enemy. A repeated light hit queues a double-rate
/// restart and its sound, preserving the pending reaction on that early return.
/// Other reactions may change state; otherwise a clip boundary, jump or settled
/// pose returns to the swimming decision state.
static void _actor00400SwimLightRecoilWait(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;
    s32              soundId;

    work = task->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animStep    = ANIMATION_RATE_ONE * 2;
        work->animClip    = ACTOR_00400_ANIM_SWIM_RECOIL_LIGHT;
        work->animRequest = DIVER_ANIM_REQUEST_RESET;
        soundId           = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_HIT;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        return;
    }
    if ((_actor00400ApplyHitReaction(task) << 0x10) == 0) {
        if (_diverClipHasBoundaryOrJump(task)) {
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_SWIM_STATE_DECIDE;
            nextStateWork->subState = 0;
        }
    }
}

static void Actor00400_Fn061E8(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    GfxCoord*        coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              i;
    s16              y;

    work              = arg0->work;
    coord             = arg0->extra.tmd->coords;
    work->animBlend   = 3;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0xB;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    sound             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    i                 = 0;
    pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    y      = work->waterLevel - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        effectSpawn(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
    sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
    pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}

/// Aims a 36-frame swimming attack windup around a 24-frame spark discharge.
///
/// Requires the live rig and current target position. Starts the discharge
/// countdown on elapsed frame 1; the frame driver emits its sparks separately.
/// Turns at 16 angle units per tick with a 32-unit deadband (4096 per turn).
/// At frame 36, clears elapsed frames, queues normal-rate discharge clip 7
/// with an eight-frame blend and enters the shot-firing wait.
static void _actor00400SwimAttackWindup(Task* task)
{
    enum {
        ACTOR_00400_SWIM_DISCHARGE_FRAMES = 24,
        ACTOR_00400_SWIM_WINDUP_FRAMES    = 36
    };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work = task->work;
    work->stateFrames++;
    _actor00400TurnTowardPoint(task, &work->targetPos, 0x10, 0x20);
    if (work->stateFrames == 1) {
        work->attackFrames = ACTOR_00400_SWIM_DISCHARGE_FRAMES;
    }
    if (work->stateFrames >= ACTOR_00400_SWIM_WINDUP_FRAMES) {
        work->stateFrames = 0;
        requestWork       = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_DISCHARGE, ANIMATION_RATE_ONE, 8);
        work->subState++;
    }
}

/// Spawns the shot's task from `Actor00400_D16028[1]` and hands it a zeroed
/// `_Actor00400ShotWork`: coordinate 5 gives the task's root translation, and
/// the span from coordinate 4's origin to the point `height` along its Z axis -
/// the speed `Actor00400_D1609C` holds for the enemy's placement row - becomes
/// the shot's `velocity`.
static inline void Actor00400_SpawnMarker(Task* arg0)
{
    AreaPlacement*       params;
    _Actor00400ShotWork* shot;
    GfxCoord*            coords;
    GfxCoord*            origin;
    GfxCoord*            span;
    GfxCoord*            dst;
    Task*                task;
    SVECTOR              pos;
    SVECTOR              base;
    SVECTOR              tip;
    u16                  height;

    params = ((Enemy*)arg0->spawnArg2.pointer)->place;
    if (params != NULL) {
        height = Actor00400_D1609C[params->rowIndex & 7];
    } else {
        height = 0xBE;
    }
    coords = arg0->extra.tmd->coords;
    origin = &coords[5];
    span   = &coords[4];
    task   = taskSpawnFromTable(Actor00400_D16028, 1, 0, 0);
    if (task != NULL) {
        shot = memCalloc(sizeof(_Actor00400ShotWork), false);
        if (shot == NULL) {
            taskKill(task);
        } else {
            base.vx = 0;
            base.vy = 0;
            base.vz = 0;
            tip.vx  = 0;
            tip.vy  = 0;
            tip.vz  = height;
            _actorRenderTransformPointToWorld(span, &base);
            _actorRenderTransformPointToWorld(span, &tip);
            task->work = shot;
            dst        = task->extra.tmd->coords;
            pos.vx     = 0;
            pos.vy     = 0;
            pos.vz     = 0;
            _actorRenderTransformPointToWorld(origin, &pos);
            dst->coord.t[0]   = pos.vx;
            dst->coord.t[1]   = pos.vy;
            dst->coord.t[2]   = pos.vz;
            shot->velocity.vx = tip.vx - base.vx;
            shot->velocity.vy = tip.vy - base.vy;
            shot->velocity.vz = tip.vz - base.vz;
        }
    }
}

static void Actor00400_Fn064B0(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              id;
    s32              pan;

    work = arg0->work;
    work->stateFrames++;
    _actor00400TurnTowardPoint(arg0, &work->targetPos, 0x10, 0x20);
    if (work->stateFrames == 0x29) {
        id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000A;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0x2B) {
        Actor00400_SpawnMarker(arg0);
    }
    if (_diverClipHasBoundaryOrJump(arg0)) {
        work2           = arg0->work;
        work2->state    = ACTOR_00400_SWIM_STATE_DECIDE;
        work2->subState = 0;
    }
}

/// Circles the tunnel's eight patrol waypoints without entering combat.
///
/// Requires the live diver rig and borrowed waypoint ring, index 0..7. Sets
/// goal Y directly from the waypoint, without a water-level offset. A signed
/// halfword XZ distance below 1000 advances the ring and ends this tick.
/// Otherwise starts and immediately ticks the normal swim clip if needed,
/// turns toward the waypoint, steps 96 coordinate units and selects black colouring.
static void _actor00400TunnelPatrolSwim(Task* task)
{
    enum { ACTOR_00400_TUNNEL_PATROL_ARRIVAL_RADIUS = 1000,
           ACTOR_00400_WAYPOINT_INDEX_MASK          = 7 };
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          waypointOffset;

    work              = task->work;
    rootCoord         = task->extra.tmd->coords;
    waypointOffset.vx = (u16)work->waypoints[work->waypointIndex].vx - rootCoord->coord.t[0];
    waypointOffset.vy = (u16)work->waypoints[work->waypointIndex].vy - rootCoord->coord.t[1];
    waypointOffset.vz = (u16)work->waypoints[work->waypointIndex].vz - rootCoord->coord.t[2];

    work->goalY = (u16)work->waypoints[work->waypointIndex].vy;
    if ((s16)SquareRoot0(waypointOffset.vx * waypointOffset.vx + waypointOffset.vz * waypointOffset.vz) < ACTOR_00400_TUNNEL_PATROL_ARRIVAL_RADIUS) {
        work->waypointIndex = (work->waypointIndex + 1) & ACTOR_00400_WAYPOINT_INDEX_MASK;
        return;
    }
    if (work->animClip != ACTOR_00400_ANIM_SWIM) {
        _Actor00400Work* requestWork;

        requestWork = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SWIM, ANIMATION_RATE_ONE, 10);
        // Apply the new swim pose before turning and moving this frame.
        _actor00400AdvanceAnimation(task);
    }
    _actor00400TurnTowardPoint(task, &work->waypoints[work->waypointIndex], 0x2C, 0x100);
    _diverStepForward(task, 0x60, work->rotation.vy);
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
}

/// Queues a body-clip blend at the normal playback rate.
///
/// Borrows the live work block. `clipIndex` must select a loaded clip for slots
/// 1..14; `blendFrames` is in normal-rate frames, with 0..2047 keeping the blend
/// duration nonnegative. A later request replaces it; no slots are ticked here.
static inline void _actor00400RequestNormalClipBlend(_Actor00400Work* work, s16 clipIndex, s16 blendFrames)
{
    _diverRequestClipBlend(work, clipIndex, ANIMATION_RATE_ONE, blendFrames);
}

/// Starts the scripted room-introduction discharge on its timed cues.
///
/// Requires the live diver task, enemy and initialized model, with the step
/// counter initially zero. Queues the room cue on frame 1, releases the neck
/// on frame 8, then starts 24 discharge ticks and blends to the discharge clip
/// on frame 20. Resets the counter for the following wait step.
static void _actor00400RoomIntroBeginDischarge(Task* task)
{
    enum {
        ACTOR_00400_INTRO_DISCHARGE_CUE_FRAME   = 1,
        ACTOR_00400_INTRO_NECK_RELEASE_FRAME    = 8,
        ACTOR_00400_INTRO_DISCHARGE_START_FRAME = 20,
        ACTOR_00400_INTRO_DISCHARGE_FRAMES      = 24,
        ACTOR_00400_SOUND_INTRO_DISCHARGE_CUE   = 0x54220005,
        ACTOR_00400_SOUND_NECK_RELEASE          = 0x40040007
    };
    _Actor00400Work* work;
    _Actor00400Work* animWork;
    s32              soundId;
    s32              panOffset;

    work = task->work;
    work->stateFrames++;
    if (work->stateFrames == ACTOR_00400_INTRO_DISCHARGE_CUE_FRAME) {
        sndEvtRequestScriptStart(((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_INTRO_DISCHARGE_CUE, 0, 0);
    }
    if (work->stateFrames == ACTOR_00400_INTRO_NECK_RELEASE_FRAME) {
        work->neckRetracted = 0;
        soundId             = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_NECK_RELEASE;
        panOffset           = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // Start the spark window, then give the following wait its own counter.
    if (work->stateFrames == ACTOR_00400_INTRO_DISCHARGE_START_FRAME) {
        work->attackFrames = ACTOR_00400_INTRO_DISCHARGE_FRAMES;
        animWork           = task->work;
        _actor00400RequestNormalClipBlend(animWork, ACTOR_00400_ANIM_DISCHARGE, 4);
        work->stateFrames = 0;
        work->subState++;
    }
}

/// Drives the Bog Diver's wounded lying task state.
///
/// Requires live work, enemy and model; state indexes 2 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// Running ticks select targets and dispatch wounded idle/flinch, then tick
/// animation and publish slot-1 status. Root rotation and contacts follow;
/// signed HP <= 0 selects stranded death. Paused frames relight and draw shadows.
static void _actor00400WoundedGroundTask(Task* task)
{
    _Actor00400Work* work      = task->work;
    Enemy*           enemy     = task->spawnArg2.pointer;
    TmdObject*       model     = task->extra.tmd;
    TaskFunc         states[2] = { _actor00400WoundedGroundIdle, _actor00400WoundedGroundFlinch };
    _Actor00400Work* deathWork;
    _Actor00400Work* lightingWork;
    TmdObject*       lightingModel;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase++;
            work->frameCount++;
            _actor00400UpdatePartyTarget(task);
            states[work->state](task);
            _actor00400AdvanceAnimation(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor00400ApplyRootRotation(task);
            _actor00400ApplyContacts(task);
            if ((s16)enemy->hp <= 0) {
                deathWork           = task->work;
                task->state         = ACTOR_00400_TASK_STRANDED_DEATH;
                deathWork->state    = ACTOR_00400_STRANDED_DEATH_START;
                deathWork->subState = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            _actor00400UpdateModelColor(task, &lightingModel->coords[1], lightingWork, lightingModel);
            _actor00400DrawLimbShadows(task, task->extra.tmd->coords->coord.t[1], ACTOR_00400_LIVING_SHADOW_SHADE);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// Consumes the pending hit reaction while the wounded diver lies or floats.
///
/// Light, heavy, status and blast reactions restart wounded state 1 (flinch)
/// at substate 0. Other reactions leave the state alone. Returns 1 whenever
/// `hitTaken == 1`, including a reaction that selects no state, and clears
/// `hitReaction`; returns 0 without changes otherwise.
static inline s32 _actor00400ConsumeWoundedHitReaction(_Actor00400Work* work)
{
    enum { ACTOR_00400_WOUNDED_STATE_FLINCH = 1 };
    s16 reaction;

    if (work->hitTaken != 1) {
        return 0;
    }
    reaction = work->hitReaction;
    if (reaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->state    = ACTOR_00400_WOUNDED_STATE_FLINCH;
        work->subState = 0;
    } else if (reaction == ACTOR_00400_HIT_REACTION_HEAVY) {
        work->state    = ACTOR_00400_WOUNDED_STATE_FLINCH;
        work->subState = 0;
    } else if (reaction == ACTOR_00400_HIT_REACTION_STATUS) {
        work->state    = ACTOR_00400_WOUNDED_STATE_FLINCH;
        work->subState = 0;
    } else if (reaction == ACTOR_00400_HIT_REACTION_BLAST) {
        work->state    = ACTOR_00400_WOUNDED_STATE_FLINCH;
        work->subState = 0;
    }
    work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
    return 1;
}

/// Keeps the wounded diver lying still, restarting its pose at quarter rate.
///
/// Requires the live rig and pending hit fields. A consumed hit restarts the
/// wounded flinch state; otherwise a published clip boundary, jump or settled
/// status queues clip 15 with an eight-frame blend, without ticking playback.
static void _actor00400WoundedGroundIdleTick(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work = task->work;
    if (_actor00400ConsumeWoundedHitReaction(work) == 0) {
        if (_diverClipHasBoundaryOrJump(task)) {
            requestWork = task->work;
            _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_ENTER, ANIMATION_RATE_ONE / 4, 8);
        }
    }
}

/// Holds the lying wounded diver's flinch until its published clip status fires.
///
/// Requires the live rig and enemy. A repeated light hit queues normal-rate
/// clip 19 with a two-frame blend and sound, then leaves the reaction pending.
/// Other consumed hits restart flinch; without one, a boundary, jump or settled
/// pose selects wounded idle at substate zero.
static void _actor00400WoundedGroundFlinchWait(Task* task)
{
    enum { ACTOR_00400_ANIM_WOUNDED_GROUND_FLINCH = 19 };
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;

    work = task->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        _diverRequestClipBlend(work, ACTOR_00400_ANIM_WOUNDED_GROUND_FLINCH, ANIMATION_RATE_ONE, 2);
        _actor00400PlayHitSound(task);
        return;
    }
    if (_actor00400ConsumeWoundedHitReaction(work) == 0) {
        if (_diverClipHasBoundaryOrJump(task)) {
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_WOUNDED_STATE_IDLE;
            nextStateWork->subState = 0;
        }
    }
}

/// Drives the Bog Diver's wounded floating task state.
///
/// Requires live work, enemy and model; state indexes 2 handlers without a
/// bounds check. Hidden control only disables active drawing.
/// Running ticks select targets and dispatch wounded idle/flinch, then tick
/// animation and publish slot-1 status. Root rotation and contacts follow;
/// signed HP <= 0 selects swimming death, then Y eases by 1/16 toward goalY.
/// Paused frames update model lighting without advancing the height.
static void _actor00400WoundedFloatTask(Task* task)
{
    _Actor00400Work* work      = task->work;
    TmdObject*       model     = task->extra.tmd;
    Enemy*           enemy     = task->spawnArg2.pointer;
    GfxCoord*        rootCoord = model->coords;
    TaskFunc         states[2] = { _actor00400WoundedFloatIdle, _actor00400WoundedFloatFlinch };
    _Actor00400Work* deathWork;
    _Actor00400Work* lightingWork;
    TmdObject*       lightingModel;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase++;
            work->frameCount++;
            _actor00400UpdatePartyTarget(task);
            states[work->state](task);
            _actor00400AdvanceAnimation(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor00400ApplyRootRotation(task);
            _actor00400ApplyContacts(task);
            if ((s16)enemy->hp <= 0) {
                deathWork           = task->work;
                task->state         = ACTOR_00400_TASK_SWIM_DEATH;
                deathWork->state    = ACTOR_00400_SWIM_DEATH_START;
                deathWork->subState = 0;
            }
            rootCoord->coord.t[1] += (work->goalY - rootCoord->coord.t[1]) >> 4;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            lightingModel = task->extra.tmd;
            lightingWork  = task->work;
            _actor00400UpdateModelColor(task, &lightingModel->coords[1], lightingWork, lightingModel);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// Bobs the floating wounded diver and renews its pose at one-eighth rate.
///
/// Requires the live rig and room water height. A consumed hit pauses the bob
/// and light, heavy, status or blast reactions restart flinch; otherwise the elapsed halfword drives a 64-tick
/// sine cycle with 64-unit amplitude about waterLevel plus floatOffset. A
/// published boundary, jump or settled status queues clip 16 with an
/// eight-frame blend. The frame driver eases the root toward this goal height.
static void _actor00400WoundedFloatIdleTick(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    s32              bobFrame;

    work = task->work;
    if (_actor00400ConsumeWoundedHitReaction(work) == 0) {
        bobFrame          = (u16)work->stateFrames + 1;
        work->stateFrames = bobFrame;
        work->goalY       = work->floatOffset + ((u16)work->waterLevel + ((rsin(bobFrame << 16 >> 10) * ACTOR_00400_WOUNDED_FLOAT_BOB_SCALE) >> 10));
        if (_diverClipHasBoundaryOrJump(task)) {
            requestWork = task->work;
            _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_LOOP, ANIMATION_RATE_ONE / 8, 8);
        }
    }
}

/// Bobs the wounded floating flinch for 121 hit-free ticks before returning idle.
///
/// Requires the live rig and room water height. A repeated light hit queues
/// normal-rate clip 18 with a two-frame blend. A consumed hit pauses the timer;
/// light, heavy, status and blast reactions restart flinch. Other ticks advance
/// the halfword counter and a 32-tick, 128-unit sine about waterLevel plus
/// floatOffset.
/// At elapsed frame 121 selects wounded idle at substate zero.
static void _actor00400WoundedFloatFlinchTick(Task* task)
{
    enum { ACTOR_00400_WOUNDED_FLOAT_FLINCH_FRAMES = 121 };
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;
    s32              bobFrame;

    work = task->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        _diverRequestClipBlend(work, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_HIT, ANIMATION_RATE_ONE, 2);
    }
    if (_actor00400ConsumeWoundedHitReaction(task->work) == 0) {
        bobFrame          = (u16)work->stateFrames + 1;
        work->stateFrames = bobFrame;
        work->goalY       = work->floatOffset + ((u16)work->waterLevel + ((rsin(bobFrame << 16 >> 9) * ACTOR_00400_WOUNDED_FLOAT_BOB_SCALE) >> 9));
        if (work->stateFrames >= ACTOR_00400_WOUNDED_FLOAT_FLINCH_FRAMES) {
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_WOUNDED_STATE_IDLE;
            nextStateWork->subState = 0;
        }
    }
}

#include "../../shared/diver_step_forward.inc.c"

/// Draws the wounded diver's ground stain until its fade finishes.
///
/// `task->work` owns an `_Actor00400GroundStainWork` populated before the
/// first tick. Task state 0 holds its strength while the enemy lives; state
/// 1 fades and kills the task. Those are the only valid dispatcher indices.
/// The borrowed enemy must remain readable through the hold state.
static void _actor00400GroundStainTask(Task* task)
{
    TaskFunc states[] = {
        _actor00400GroundStainHold,
        _actor00400GroundStainFade,
    };

    states[task->state](task);
}

/// Acquires a battle reference and starts the swimming patrol with random phases.
///
/// Requires the live enemy, initialized rig and root coordinate. Advances the
/// LCG twice, narrowing its upper halves into bobPhase and frameCount. Requests
/// normal-rate surfaced clip 1 without blending, enters patrol at substate zero
/// and seeds the goal height from the root's current parent-space Y.
static void _actor00400SwimStart(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    _Actor00400Work* nextStateWork;
    GfxCoord*        rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    sceneAcquireBattleRef(0);
    gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->bobPhase           = gRandomLcgState >> 16;
    gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->frameCount         = gRandomLcgState >> 16;
    requestWork              = task->work;
    requestWork->animStep    = ANIMATION_RATE_ONE;
    requestWork->animClip    = ACTOR_00400_ANIM_SURFACED;
    requestWork->animRequest = DIVER_ANIM_REQUEST_RESET;
    nextStateWork            = task->work;
    nextStateWork->state     = ACTOR_00400_SWIM_STATE_PATROL;
    nextStateWork->subState  = 0;
    work->goalY              = rootCoord->coord.t[1];
}

/// States `_actor00400SwimPatrol` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable4 Actor00400_D00134 = { {
    _actor00400SwimPatrolEnter,
    _actor00400SwimPatrolTravel,
    _actor00400SwimPatrolSurface,
    _actor00400SwimPatrolWaitForClip,
} };

/// Dispatches the swimming patrol until a hit or combat alert interrupts it.
///
/// Requires live work, enemy and rig with substate 0..3 (enter, travel,
/// surface, clip wait). A state-changing hit raises the alert and engages
/// battle; an existing alert selects dive at substate zero. Otherwise copies
/// the four handlers and dispatches without a bounds check. The hit test
/// preserves the low-halfword result of the reaction handler.
static void _actor00400SwimPatrol(Task* task)
{
    _Actor00400Work* work;
    TaskFuncTable4   handlers;
    _Actor00400Work* nextStateWork;

    work     = task->work;
    handlers = Actor00400_D00134;
    if ((_actor00400ApplyHitReaction(task) << 0x10) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        sceneEngageBattle(1);
    } else if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
        nextStateWork           = task->work;
        nextStateWork->state    = ACTOR_00400_SWIM_STATE_DIVE;
        nextStateWork->subState = 0;
    } else {
        handlers.funcs[work->subState](task);
    }
}

static void Actor00400_Fn078C8(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        _actor00400SwimEmergeEnter,
        Actor00400_Fn058C4,
    };

    if ((_actor00400ApplyHitReaction(arg0) << 0x10) == 0) {
        states[work->subState](arg0);
    }
}

/// States `_actor00400Dive` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D00144 = { {
    _actor00400DiveEnter,
    _actor00400DiveWaitForClip,
    _actor00400DiveSwimToSurfaceSpot,
} };

/// Dispatches the dive's entry, transition wait or travel to a surface spot.
///
/// Requires live work and substate 0..2; no bounds check or hit reaction is
/// performed here. Entry selects the wait or skips to travel when already
/// swimming. The room's waypoint and surface-spot storage must remain live.
static void _actor00400Dive(Task* task)
{
    _Actor00400Work* work;
    TaskFuncTable3   handlers;

    work     = task->work;
    handlers = Actor00400_D00144;
    handlers.funcs[work->subState](task);
}

/// Leaves unused swimming state-table slot 5 inert.
///
/// The swimming table retains a distinct callback at this slot; no state
/// transition selects it. Accepts the dispatcher task without accessing it.
static void _actor00400SwimUnusedState5(Task* task)
{
}

/// Leaves unused swimming state-table slot 6 inert.
///
/// The swimming table retains a distinct callback at this slot; no state
/// transition selects it. Accepts the dispatcher task without accessing it.
static void _actor00400SwimUnusedState6(Task* task)
{
}

/// Dispatches the swimming light recoil's entry or animation wait.
///
/// Requires live work and subState 0 (enter) or 1 (wait); there is no bounds
/// check. The stack table calls the shared recoil entry and the swimming wait,
/// which handles new hits before returning to the decision state.
static void _actor00400SwimLightRecoil(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _diverEnterRecoil,
        _actor00400SwimLightRecoilWait,
    };

    states[work->subState](task);
}

/// Consumes pending hits while the swimming diver is in heavy recoil.
///
/// Requires live work. Only `hitTaken == 1` with a heavy or status reaction
/// restarts heavy recoil or enters status hold, clearing substate and
/// returning 1. All paths clear `hitReaction`, including light, blast and
/// flinch reactions and ticks without a hit; these return 0. Preserves
/// `hitTaken`, animation requests and counters.
static inline s32 _actor00400ConsumeSwimHeavyRecoilHitReaction(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->hitTaken == 1) {
        switch (work->hitReaction) {
            case ACTOR_00400_HIT_REACTION_HEAVY:
                work->state       = ACTOR_00400_STATE_RECOIL_HEAVY;
                work->subState    = 0;
                work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
                return 1;
            case ACTOR_00400_HIT_REACTION_STATUS:
                work->state       = ACTOR_00400_STATE_STATUS_HOLD;
                work->subState    = 0;
                work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
                return 1;
        }
    }
    work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
    return 0;
}

static void Actor00400_Fn079FC(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn061E8,
        _actor00400SwimHeavyRecoilWait,
    };
    s16 taken;

    taken = _actor00400ConsumeSwimHeavyRecoilHitReaction(arg0);
    if (taken == 0) {
        states[work->subState](arg0);
    }
}

/// Dispatches entry and ticking of the swimming status hold.
///
/// Requires live work and subState 0 (enter) or 1 (tick); there is no bounds
/// check. The stack table drives the temporary critical-chance multiplier,
/// held pose and status-buildup release of this swimming state.
static void _actor00400SwimStatusHold(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400SwimStatusHoldEnter,
        _actor00400SwimStatusHoldTick,
    };

    states[work->subState](task);
}

/// States `_actor00400SwimAttack` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D00150 = { {
    _actor00400SwimAttackEnter,
    _actor00400SwimAttackWindup,
    Actor00400_Fn064B0,
} };

/// Dispatches the swimming discharge and shot attack unless a hit changes state.
///
/// Requires live work, enemy and rig with substate 0..2 (enter, discharge
/// windup, shot wait). Copies all three handlers before testing the reaction
/// handler's low halfword; dispatches without a bounds check when it is zero.
static void _actor00400SwimAttack(Task* task)
{
    _Actor00400Work* work;
    TaskFuncTable3   handlers;

    work     = task->work;
    handlers = Actor00400_D00150;
    if ((_actor00400ApplyHitReaction(task) << 0x10) == 0) {
        handlers.funcs[work->subState](task);
    }
}

/// States `_actor00400TunnelPatrol` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D0015C = { {
    _actor00400TunnelPatrolEnter,
    _actor00400TunnelPatrolWaitForCue,
    _actor00400TunnelPatrolSwim,
} };

/// Dispatches the scripted tunnel patrol while keeping the diver untargetable.
///
/// Requires a live enemy and work block with substate 0..2. Retracts the neck
/// on every tick; the three steps enter, await the patrol cue and swim the ring.
/// The table is copied to the stack and has no bounds check.
static void _actor00400TunnelPatrol(Task* task)
{
    Enemy*           enemy;
    _Actor00400Work* work;
    TaskFuncTable3   handlers;

    enemy                         = task->spawnArg2.pointer;
    work                          = task->work;
    handlers                      = Actor00400_D0015C;
    work->neckRetracted           = 1;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    handlers.funcs[work->subState](task);
}

/// States `_actor00400TunnelIntro` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable4 Actor00400_D00168 = { {
    _actor00400TunnelIntroEnter,
    _actor00400TunnelIntroWaitForSwim,
    _actor00400TunnelIntroSwim,
    _actor00400TunnelIntroWaitForPatrol,
} };

/// Dispatches the tunnel introduction, or skips a completed introduction to patrol.
///
/// Requires a live enemy and work block. An unseen introduction uses substate
/// 0..3 without a bounds check, with the neck retracted and targeting disabled.
/// A nonzero saved event flag selects patrol entry without ticking it yet.
static void _actor00400TunnelIntro(Task* task)
{
    Enemy*           enemy;
    _Actor00400Work* work;
    TaskFuncTable4   handlers;

    enemy    = task->spawnArg2.pointer;
    work     = task->work;
    handlers = Actor00400_D00168;
    if (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        _diverSetState(task, ACTOR_00400_SWIM_STATE_TUNNEL_PATROL);
    } else {
        work->neckRetracted           = 1;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        handlers.funcs[work->subState](task);
    }
}

/// Advances the resting death pose for sixty ticks before weighted-colour fading.
///
/// Requires the live animation rig and the death counter cleared at entry.
/// Consumes pending clip requests and ticks slots 1..14; the outer death
/// driver publishes slot 1's status after this handler returns.
static void _actor00400SwimDeathRest(Task* task)
{
    enum { ACTOR_00400_SWIM_DEATH_REST_TICKS = 60 };
    _Actor00400Work* work;

    work = task->work;
    work->stateFrames++;
    _actor00400AdvanceAnimation(task);
    if (work->stateFrames >= ACTOR_00400_SWIM_DEATH_REST_TICKS) {
        work->state++;
    }
}

/// Starts the swimming corpse's weighted-colour fade and clears its phase timer.
///
/// Requires the live enemy and work block; advances to the fade state.
static void _actor00400SwimDeathBeginColorFade(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state       = (u16)work->state + 1;
}

/// Makes the swimming corpse translucent after twenty-four weighted-colour ticks.
///
/// Requires the live model and a cleared phase timer. Enables semi-transparency,
/// clears the timer again and advances to darkening without changing animation.
static void _actor00400SwimDeathFade(Task* task)
{
    enum { ACTOR_00400_SWIM_DEATH_FADE_TICKS = 24 };
    _Actor00400Work* work;
    TmdObject*       model;

    work  = task->work;
    model = task->extra.tmd;
    if (++work->stateFrames >= ACTOR_00400_SWIM_DEATH_FADE_TICKS) {
        model->flags     |= TMD_OBJECT_SEMI_TRANS;
        work->stateFrames = 0;
        work->state++;
    }
}

/// Turns the translucent swimming corpse black on tick sixteen and finishes on tick thirty-three.
///
/// Requires a cleared phase timer and the live enemy. Advances to hiding once
/// the timer exceeds thirty-two; counters retain their signed halfword behavior.
static void _actor00400SwimDeathDarken(Task* task)
{
    enum { ACTOR_00400_SWIM_DEATH_BLACK_TICK       = 16,
           ACTOR_00400_SWIM_DEATH_DARKEN_LAST_TICK = 32 };
    _Actor00400Work* work;

    work = task->work;
    if (++work->stateFrames == ACTOR_00400_SWIM_DEATH_BLACK_TICK) {
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->stateFrames > ACTOR_00400_SWIM_DEATH_DARKEN_LAST_TICK) {
        work->state++;
    }
}

/// Hides the faded swimming corpse and enters surface-claim release and delayed despawn.
///
/// Requires the live model and work block; resets both state indices for despawn.
static void _actor00400SwimDeathHide(Task* task)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model          = task->extra.tmd;
    model->flags  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work           = task->work;
    task->state    = ACTOR_00400_TASK_DESPAWN;
    work->state    = ACTOR_00400_DESPAWN_STATE_RELEASE_SPOT;
    work->subState = 0;
}

/// Hides the blast-killed swimming diver before the two-tick body-burst delay.
///
/// Requires the live model and work block; clears the timer and advances to the wait.
static void _actor00400SwimDeathBlastHide(Task* task)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model             = task->extra.tmd;
    work              = task->work;
    model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->state++;
}

/// Waits two ticks with the swimming model hidden before its body is burst apart.
///
/// Requires the live work block and a cleared timer; advances to the burst state.
static void _actor00400SwimDeathBlastWait(Task* task)
{
    enum { ACTOR_00400_SWIM_DEATH_BLAST_WAIT_TICKS = 2 };
    _Actor00400Work* work;

    work = task->work;
    if (++work->stateFrames >= ACTOR_00400_SWIM_DEATH_BLAST_WAIT_TICKS) {
        work->state++;
    }
}

/// Bursts the hidden swimming corpse into independently owned model chunks.
///
/// Requires live work and model after the two-tick blast delay. Releases the
/// body's primitive buffer and disables automatic recreation before spawning
/// the chunks. Retains the model coordinates through emission, then increments
/// the state with low-halfword wrap into the blast exit.
static void _actor00400SwimDeathBlastBurst(Task* task)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model = task->extra.tmd;
    work  = task->work;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    _actor00400SpawnBodyChunks(task);
    work->state = (u16)work->state + 1;
}

/// Enters surface-claim release and delayed despawn after the swimming body's burst.
///
/// Requires the live work block; the preceding blast phases have already hidden
/// the model and released its primitive buffers. Resets both despawn indices.
static void _actor00400SwimDeathBlastEnd(Task* task)
{
    _Actor00400Work* work;

    work           = task->work;
    task->state    = ACTOR_00400_TASK_DESPAWN;
    work->state    = ACTOR_00400_DESPAWN_STATE_RELEASE_SPOT;
    work->subState = 0;
}

/// Dispatches the shot's launch, flight and disabled-sphere linger states.
///
/// Requires the coordinate body and owned `_Actor00400ShotWork` supplied by
/// the spawner, with task state 0..2. The three-entry table has no bounds check.
/// Launch links the sphere; flight disables it; linger unlinks it before
/// destroying the task and releasing its work.
static void _actor00400ShotTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = Actor00400_D0002C;
    handlers.funcs[task->state](task);
}

/// States `_actor00400RoomIntro` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable7 Actor00400_D00178 = { {
    _actor00400RoomIntroEnter,
    _actor00400RoomIntroWaitForSwim,
    _actor00400RoomIntroSwim,
    _actor00400RoomIntroWaitForSurface,
    _actor00400RoomIntroBeginDischarge,
    _actor00400RoomIntroWaitAfterDischarge,
    _actor00400RoomIntroWaitForFight,
} };

/// Applies a borrowed room command to the diver's scripted or combat state.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with a live diver and a readable command.
/// Commands 1..5 latch introduction cues; 6 immediately strands the diver at
/// root Y zero and selects ground combat decision. Other commands are ignored,
/// as are the context, message ID and second payload. No pointer is retained.
/// No response is defined; callers must ignore the dispatched result.
static void _actor00400ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg)
{
    _Actor00400Work* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.tmd->coords;
    switch (request->command) {
        case ACTOR_00400_COMMAND_TUNNEL_SWIM:
            work->command = ACTOR_00400_COMMAND_TUNNEL_SWIM;
            break;
        case ACTOR_00400_COMMAND_TUNNEL_PATROL:
            work->command = ACTOR_00400_COMMAND_TUNNEL_PATROL;
            break;
        case ACTOR_00400_COMMAND_INTRO_SWIM:
            work->command = ACTOR_00400_COMMAND_INTRO_SWIM;
            break;
        case ACTOR_00400_COMMAND_INTRO_SURFACE:
            work->command = ACTOR_00400_COMMAND_INTRO_SURFACE;
            break;
        case ACTOR_00400_COMMAND_FIGHT:
            work->command = ACTOR_00400_COMMAND_FIGHT;
            break;
        case ACTOR_00400_COMMAND_STRAND:
            // Restore targeting and move immediately into the stranded state table.
            enemy->node.state.parts.flags = 0;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            work->inWater         = 0;
            work->command         = ACTOR_00400_COMMAND_STRAND;
            rootCoord->coord.t[1] = 0;
            task->state           = ACTOR_00400_TASK_STRANDED;
            // Keep the initial state selection and the subsequent work reload.
            _diverSetState(task, ACTOR_00400_STRANDED_STATE_START);
            _diverSetState(task, ACTOR_00400_STRANDED_STATE_DECIDE);
            break;
    }
}

/// Measures target pitch and yaw from a model part in the root's local basis.
///
/// Requires a live part index 0..14, current part/view composed matrices and a
/// target in the view-parent frame. Subtracts `heightOffset` in coordinate units
/// from target Y, then uses the root basis's transpose without normalization.
/// Writes signed twelve-bit pitch and yaw (-2048..2047) and zero roll to the
/// caller's vector, preserving `pad`. Changes GTE state and borrows no storage
/// beyond this call; the ordinary caller uses head part 4 and height offset 1536.
static void _actor00400GetTargetAnglesFromPart(Task* task, s16 partIndex, SVECTOR* targetAngles, s16 heightOffset)
{
    MATRIX           partInView;
    VECTOR           targetOffset;
    VECTOR           localOffset;
    GfxCoord*        coords;
    _Actor00400Work* work;

    coords = task->extra.tmd->coords;
    work   = task->work;
    // Measure from the part origin, then apply the root basis's transpose for aiming.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[partIndex].workm, &partInView);
    targetOffset.vx = work->targetPos.vx - partInView.t[0];
    targetOffset.vy = work->targetPos.vy - heightOffset - partInView.t[1];
    targetOffset.vz = work->targetPos.vz - partInView.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &targetOffset, &localOffset);
    targetAngles->vx = ratan2(-localOffset.vy, localOffset.vz) << 20 >> 20;
    targetAngles->vy = ratan2(localOffset.vx, localOffset.vz) << 20 >> 20;
    targetAngles->vz = 0;
}

/// Invalidates and recomposes two model parts' full parent-chain transforms.
///
/// Both coordinates and their acyclic ancestor chains must remain live and
/// writable; the same coordinate may be supplied twice. Marks both dirty
/// before composing either, so a changed ancestor cannot leave the other
/// part's cached transform valid. Changes GTE state; the two calls retain
/// the normal composition stamps.
static inline void _actor00400ComposePartPair(GfxCoord* firstCoord, GfxCoord* secondCoord)
{
    firstCoord->composeStamp  = GRAPHICS_COORD_DIRTY;
    secondCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(firstCoord);
    actorRenderComposeCoord(secondCoord);
}

/// Captures the horizontal midpoint of two model parts in the view coordinate's space.
///
/// Both indices must address live coordinates of the task's model. The caller
/// supplies writable `midpoint`; only XZ is written, preserving Y and `pad`.
/// The crawl uses arm tips 11 and 14 to capture the stride's fixed anchor.
/// Composes both parts, changes the GTE state and leaves their stamps dirty.
static void _actor00400CapturePartMidpointXZ(Task* task, s16 firstPartIndex, s16 secondPartIndex, SVECTOR* midpoint)
{
    MATRIX    firstTransform;
    MATRIX    secondTransform;
    GfxCoord* firstCoord;
    GfxCoord* secondCoord;
    GfxCoord* coords;

    coords                     = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    firstCoord                 = &coords[firstPartIndex];
    secondCoord                = &coords[secondPartIndex];
    actorRenderComposeCoord(&gGfxViewCoord);
    _actor00400ComposePartPair(firstCoord, secondCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &firstTransform);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &secondTransform);
    midpoint->vx              = (firstTransform.t[0] + secondTransform.t[0]) / 2;
    midpoint->vz              = (firstTransform.t[2] + secondTransform.t[2]) / 2;
    firstCoord->composeStamp  = GRAPHICS_COORD_DIRTY;
    secondCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Hides or resumes the diver while respecting death phases that keep its model hidden.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with the live model and work block.
/// Mode 0 hides and suspends state updates; mode 1 resumes them, keeping blast
/// deaths, shrunken stranded corpses, the water death's hide phase and despawning
/// models hidden. Other modes do nothing. Also updates the group hide request
/// that ends live shots. The message ID and second payload are unused.
/// No response is defined; callers must ignore the dispatched result.
static void _actor00400SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { ACTOR_00400_MODEL_HIDE = 0,
           ACTOR_00400_MODEL_SHOW = 1 };
    _Actor00400Work* work;
    TmdObject*       model;
    s32              taskState;

    work  = task->work;
    model = task->extra.tmd;
    switch (drawMode) {
        case ACTOR_00400_MODEL_HIDE:
            gSceneCombatState.actor00400HideRequested = 1;
            model->flags                             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->suspended                           = 1;
            break;
        case ACTOR_00400_MODEL_SHOW:
            gSceneCombatState.actor00400HideRequested = 0;
            // A show request resumes updates even when the death state retains hiding.
            taskState = task->state;
            if (((taskState == ACTOR_00400_TASK_STRANDED_DEATH) || (taskState == ACTOR_00400_TASK_SWIM_DEATH)) && (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST)) {
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((task->state == ACTOR_00400_TASK_STRANDED_DEATH) && ((work->state == ACTOR_00400_STRANDED_DEATH_SHRINK) || (work->state == ACTOR_00400_STRANDED_DEATH_END))) {
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((task->state == ACTOR_00400_TASK_SWIM_DEATH) && (work->state == ACTOR_00400_SWIM_DEATH_HIDE)) {
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if (task->state == ACTOR_00400_TASK_DESPAWN) {
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            work->suspended = 0;
            break;
    }
}

/// Translates the root in XZ to hold two model parts' midpoint at an anchor.
///
/// Both indices must address live coordinates in a model parented to the view
/// coordinate. `anchor` supplies readable XZ in that parent's space; its Y and
/// `pad` are ignored. Root height and rotation are preserved. The crawl holds
/// arm tips 11 and 14 at its captured anchor while the stride animates.
/// Changes the GTE state and recomposes both parts and the root.
static void _actor00400AlignPartMidpointXZ(Task* task, s16 firstPartIndex, s16 secondPartIndex, const SVECTOR* anchor)
{
    MATRIX    rootTransform;
    MATRIX    firstTransform;
    MATRIX    secondTransform;
    GfxCoord* firstCoord;
    GfxCoord* secondCoord;
    GfxCoord* coords;

    coords                     = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    firstCoord                 = &coords[firstPartIndex];
    secondCoord                = &coords[secondPartIndex];
    actorRenderComposeCoord(&gGfxViewCoord);
    _actor00400ComposePartPair(firstCoord, secondCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &rootTransform);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &firstTransform);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &secondTransform);
    coords[0].coord.t[0]   = anchor->vx - ((firstTransform.t[0] + secondTransform.t[0]) / 2 - rootTransform.t[0]);
    coords[0].coord.t[2]   = anchor->vz - ((firstTransform.t[2] + secondTransform.t[2]) / 2 - rootTransform.t[2]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    _actor00400ComposePartPair(firstCoord, secondCoord);
    actorRenderComposeCoord(coords);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Captures and blends driven body slots 1..14 toward the requested clip's track starts.
///
/// Requires the initialized rig bound to its live model, slots, pose buffers
/// and loaded clip bank. `animClip` selects a loaded non-null set (1..19 here)
/// supporting every body track. `animStep` is narrowed to the signed byte rate
/// in sixteenths of a frame before each capture tick. `animBlend` counts normal
/// frames (0..2047 keeps the signed remaining time nonnegative). Slot 0 and the
/// request fields are preserved. Each seek advances/captures the old pose and
/// changes the GTE state; the buffers stay live throughout the transition.
static inline void _actor00400SeekBodySlotsWithBlend(_Actor00400Work* work)
{
    s32 slotIndex;

    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animStep;
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animClip, 0, work->animBlend);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Seeks body slots 1..14 to the requested clip with the pending blend.
///
/// The diver's initialized rig and loaded animation bank must remain live;
/// `animClip` must select a valid clip for every driven slot. Each slot gets
/// `animStep`'s low byte as its rate in sixteenths of a frame, then seeks to
/// record 0 with `animBlend` normal-rate transition frames (normally 0..2047).
/// A changed clip clears `animBlend`; reseeking the same clip preserves it.
/// Updates `animPlaying` after every slot is sought; leaves `animRequest`
/// for the animation driver to acknowledge.
static void _actor00400BlendRequestedClip(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->animPlaying == work->animClip) {
        _actor00400SeekBodySlotsWithBlend(work);
    } else {
        _actor00400SeekBodySlotsWithBlend(work);
        work->animBlend = 0;
    }
    work->animPlaying = (u16)work->animClip;
}

/// Converts a frame count to the diver's requested animation playback rate.
///
/// `animStep` is in sixteenths of a frame per tick: 16 keeps the count, 32
/// halves it, and zero returns 0. Callers use this for clip-time thresholds
/// and when reseeking the playing clip. The signed divide and shifted
/// narrowing preserve the original rounding and 16-bit wrap behavior.
static s16 _actor00400ScaleFramesForAnimRate(Task* task, s16 frames)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->animStep == 0) {
        return 0;
    }
    return ((frames << 8) / work->animStep << 12) >> 16;
}

/// Steps the diver's heading toward a point outside a masked yaw deadband.
///
/// The target's XZ is in the root's parent space. Angles are 4096ths of a
/// turn; the shortest signed difference is in -2048..2047. `yawStep` is a
/// fixed increment that may overshoot the target. Only the low
/// 11 bits of `deadband` are used. Dirties the root even without a turn and
/// changes the GTE state while normalizing the horizontal direction.
static void _actor00400TurnTowardPointMaskedRange(Task* task, const SVECTOR* target, s32 yawStep, s32 deadband)
{
    s32              maskedDeadband;
    _Actor00400Work* work;
    GfxCoord*        coords;
    SVECTOR          direction;
    s32              yawDifference;
    s32              targetYaw;
    u16              currentYaw;

    work                 = task->work;
    coords               = task->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    maskedDeadband       = deadband & (ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1);
    direction.vx         = (u16)target->vx - coords->coord.t[0];
    direction.vy         = 0;
    direction.vz         = (u16)target->vz - coords->coord.t[2];
    VectorNormalSS(&direction, &direction);
    targetYaw     = ratan2(direction.vx, direction.vz);
    currentYaw    = work->rotation.vy;
    yawDifference = ((currentYaw - targetYaw) << 20) >> 20;
    if ((yawDifference > maskedDeadband) || (yawDifference < -maskedDeadband)) {
        work->rotation.vy = (yawDifference > maskedDeadband) ? (currentYaw - yawStep) : (currentYaw + yawStep);
    }
}

/// Applies the diver's clip request and advances its body animation once.
///
/// Requires the live initialized rig. BLEND seeks the requested clip (rescaling
/// elapsed frames when already playing); RESET restarts that clip from frame
/// zero; PLAYING counts a frame. All calls tick slots 1..14. Slot-1 status is
/// left in the rig; this helper does not publish `animStatus`.
static void _actor00400TickAnimation(Task* task)
{
    _actor00400AdvanceAnimation(task);
}

/// Queues a clip and playback rate for the next blended animation update.
///
/// `clipIndex` selects a loaded clip for body slots 1..14. `rate` is in
/// sixteenths of a frame per tick (16 normal), with its low byte consumed
/// when the slots are sought. `blendFrames` is in normal-rate frames,
/// normally 0..2047. A later request replaces these pending values; this
/// function does not seek or tick the rig.
static void _actor00400RequestClipBlend(Task* task, s16 clipIndex, s16 rate, s16 blendFrames)
{
    _Actor00400Work* work;

    work              = task->work;
    work->animBlend   = blendFrames;
    work->animStep    = rate;
    work->animClip    = clipIndex;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
}

/// Tests the diver's last animation tick for a boundary, jump or settled pose.
///
/// Uses the slot-1 result copied into `animStatus`. Returns 0 or 1 with the
/// same semantics as `_diverClipHasBoundaryOrJump`.
static s16 _actor00400ClipEnded(Task* task)
{
    return _diverClipHasBoundaryOrJump(task);
}

/// Dispatches the Bog Diver body's current task phase once per callback.
///
/// Requires task state 0..7: spawn, stranded, stranded death, swimming,
/// swimming death, despawn, wounded ground or wounded float. The eight-entry
/// table is copied whole to the stack, without a terminator or bounds check.
/// Spawn initializes the model, enemy and owned work used by later phases;
/// the selected phase controls its own frame gate and eventual destruction.
static void _actor00400BodyTask(Task* task)
{
    TaskFuncTable8 handlers;

    handlers = Actor00400_D00038;
    handlers.funcs[task->state](task);
}

/// Dispatches surface-spot release and the diver's delayed destruction.
///
/// Requires live work with state 0 (release) or 1 (wait), without a bounds
/// check. The room's surface-spot list must remain live through release.
/// The wait retires the enemy and task after 301 ticks.
static void _actor00400Despawn(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400ReleaseSurfaceSpot,
        _actor00400DespawnWait,
    };

    states[work->state](task);
}

/// Copies nine rotation/scale coefficients while preserving the destination translation.
///
/// Both matrices must be live; exact self-copy is allowed. Copies the signed
/// Q12 coefficients without normalizing them. The alignment halfword between
/// rotation and translation is preserved too.
static void _actor00400CopyRotation(const MATRIX* source, MATRIX* destination)
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

/// Dispatches entry and ticking of the lying wounded diver's idle pose.
///
/// Requires live work and rig with substate 0 (enter) or 1 (tick). The stack
/// table has no bounds check; the outer wounded driver applies clip requests.
static void _actor00400WoundedGroundIdle(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400WoundedGroundIdleEnter,
        _actor00400WoundedGroundIdleTick,
    };

    states[work->subState](task);
}

/// Starts the lying wounded pose at a random rate of three to six sixteenths of a frame.
///
/// Requires the live work block and initialized rig. Advances the shared LCG
/// once, queues clip 15 with an eight-frame blend, clears elapsed state ticks
/// and advances to idle ticking; slot playback is left to the outer driver.
static void _actor00400WoundedGroundIdleEnter(Task* task)
{
    enum { ACTOR_00400_WOUNDED_IDLE_MIN_RATE     = 3,
           ACTOR_00400_WOUNDED_IDLE_BLEND_FRAMES = 8 };
    _Actor00400Work* requestWork;
    _Actor00400Work* work;
    u32              randomValue;

    work            = task->work;
    randomValue     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = randomValue;
    requestWork     = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_ENTER,
                           ((randomValue >> 16) & 3) + ACTOR_00400_WOUNDED_IDLE_MIN_RATE,
                           ACTOR_00400_WOUNDED_IDLE_BLEND_FRAMES);
    work->stateFrames = 0;
    work->subState++;
}

/// Dispatches entry and waiting of the lying wounded diver's flinch.
///
/// Requires live work, enemy and rig with substate 0 (enter) or 1 (wait).
/// The stack table has no bounds check; the wait can restart on another hit.
static void _actor00400WoundedGroundFlinch(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400WoundedGroundFlinchEnter,
        _actor00400WoundedGroundFlinchWait,
    };

    states[work->subState](task);
}

/// Plays the hit sound and enters the lying wounded diver's flinch wait.
///
/// Requires live work, enemy and model. The placement-tagged sound precedes
/// the normal-rate clip-19 request with a two-frame blend. The outer wounded
/// driver applies the request; this entry only advances substate 0 to 1.
static void _actor00400WoundedGroundFlinchEnter(Task* task)
{
    enum { ACTOR_00400_ANIM_WOUNDED_GROUND_FLINCH  = 19,
           ACTOR_00400_WOUNDED_FLINCH_BLEND_FRAMES = 2 };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work = task->work;
    _actor00400PlayHitSound(task);
    requestWork = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_WOUNDED_GROUND_FLINCH,
                           ANIMATION_RATE_ONE, ACTOR_00400_WOUNDED_FLINCH_BLEND_FRAMES);
    work->subState++;
}

/// Advances the stranded death animation for thirty running ticks.
///
/// Requires a live initialized rig and the timer cleared by death entry.
/// Consumes clip requests and ticks body slots 1..14 without publishing their
/// status; the outer death driver publishes slot 1 afterwards. The timer,
/// rather than an animation boundary, selects the weighted-colour phase.
static void _actor00400StrandedDeathFallWait(Task* task)
{
    enum { ACTOR_00400_STRANDED_DEATH_FALL_FRAMES = 30 };
    _Actor00400Work* work;

    work = task->work;
    work->stateFrames++;
    _actor00400AdvanceAnimation(task);
    if (work->stateFrames >= ACTOR_00400_STRANDED_DEATH_FALL_FRAMES) {
        work->state++;
    }
}

/// Saves the stranded corpse's root transform and starts its weighted-colour fade.
///
/// Requires live work, enemy and root coordinate. Initializes the later Y
/// shrink at Q12 unity and saves the full matrix before any scaling. Selects
/// weighted colouring, clears the phase timer and advances from state 2 to 3.
static void _actor00400StrandedDeathWeigh(Task* task)
{
    _Actor00400Work* work;
    GfxCoord*        rootCoord;

    work               = task->work;
    rootCoord          = task->extra.tmd->coords;
    work->shrinkScaleY = ONE;
    work->savedRootMtx = rootCoord->coord;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state++;
}

/// Waits twenty-four running ticks before making the stranded corpse translucent.
///
/// Requires live work and model with the phase timer initially zero. Enables
/// semitransparent drawing, clears the timer and enters the shrink phase.
static void _actor00400StrandedDeathFadeWait(Task* task)
{
    enum { ACTOR_00400_STRANDED_DEATH_FADE_FRAMES = 24 };
    TmdObject*       model;
    _Actor00400Work* work;

    work  = task->work;
    model = task->extra.tmd;
    if (++work->stateFrames >= ACTOR_00400_STRANDED_DEATH_FADE_FRAMES) {
        model->flags     |= TMD_OBJECT_SEMI_TRANS;
        work->stateFrames = 0;
        work->state++;
    }
}

/// Flattens and burns the stranded corpse while fading its limb shadows.
///
/// Requires the root matrix saved at death weigh-in and the current signed
/// halfword Q12 Y scale (ONE is unity). Each tick subtracts 64 from that scale,
/// restores the saved matrix before scaling, and fades shadow shade by seven
/// to zero. Starts the burn at 4, selects black at 16, and hides at 33 or later,
/// advancing to the exit. The burn requests four three-flame cycles and size 4.
static void _actor00400StrandedDeathShrink(Task* task)
{
    enum {
        ACTOR_00400_DEATH_SHADOW_FADE_STEP  = 7,
        ACTOR_00400_DEATH_SHRINK_SCALE_STEP = 64,
        ACTOR_00400_DEATH_BURN_FRAME        = 4,
        ACTOR_00400_DEATH_BLACK_FRAME       = 16,
        ACTOR_00400_DEATH_HIDE_FRAME        = 33,
        ACTOR_00400_DEATH_BURN_RECIPE       = 4
    };
    TmdObject*       model;
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    VECTOR           axisScale;
    SVECTOR          burnOffset;

    /// Restores the death snapshot before applying the current Q12 Y scale.
    ///
    /// Call inside a braced block with side-effect-free live work/root pointers
    /// and a writable VECTOR scratch lvalue. Arguments recur; translation and
    /// alignment bytes come from the snapshot, and composition becomes dirty.
#define ACTOR_00400_RESTORE_SHRUNK_ROOT(work, rootCoord, axisScale) \
    (axisScale).vx     = ONE;                                       \
    (axisScale).vy     = (work)->shrinkScaleY;                      \
    (axisScale).vz     = ONE;                                       \
    (rootCoord)->coord = (work)->savedRootMtx;                      \
    ScaleMatrix(&(rootCoord)->coord, &(axisScale));                 \
    (rootCoord)->composeStamp = GRAPHICS_COORD_DIRTY

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;

    work->shadowShade -= ACTOR_00400_DEATH_SHADOW_FADE_STEP;
    if (work->shadowShade < 0) {
        work->shadowShade = 0;
    }
    work->shrinkScaleY -= ACTOR_00400_DEATH_SHRINK_SCALE_STEP;
    ACTOR_00400_RESTORE_SHRUNK_ROOT(work, rootCoord, axisScale);
    if (++work->stateFrames == ACTOR_00400_DEATH_BURN_FRAME) {
        burnOffset.vx = 0;
        burnOffset.vy = 0;
        burnOffset.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, rootCoord, ACTOR_00400_DEATH_BURN_RECIPE, &burnOffset);
    }
    if (work->stateFrames == ACTOR_00400_DEATH_BLACK_FRAME) {
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->stateFrames >= ACTOR_00400_DEATH_HIDE_FRAME) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
#undef ACTOR_00400_RESTORE_SHRUNK_ROOT
}

/// Enters surface-spot release and delayed despawn after the stranded corpse shrinks.
///
/// Requires live work; the shrink phase has already hidden the model. Selects
/// the despawn task state at its release entry, resetting both inner indices.
static void _actor00400StrandedDeathEnd(Task* task)
{
    _Actor00400Work* work;

    work           = task->work;
    task->state    = ACTOR_00400_TASK_DESPAWN;
    work->state    = ACTOR_00400_DESPAWN_STATE_RELEASE_SPOT;
    work->subState = 0;
}

/// Hides the blast-killed stranded diver before the two-tick burst delay.
///
/// Requires live work and model. Disables active drawing, clears the phase
/// timer and advances to the blast wait; model buffers remain allocated.
static void _actor00400StrandedDeathBlastHide(Task* task)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model             = task->extra.tmd;
    work              = task->work;
    model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->state++;
}

/// Delays the stranded corpse burst for two running ticks after hiding it.
///
/// Requires live work and the zeroed phase timer. Advances to the burst state
/// on the second tick; the timer is retained for the following state.
static void _actor00400StrandedDeathBlastWait(Task* task)
{
    enum { ACTOR_00400_STRANDED_DEATH_BLAST_DELAY_FRAMES = 2 };
    _Actor00400Work* work;

    work = task->work;
    if (++work->stateFrames >= ACTOR_00400_STRANDED_DEATH_BLAST_DELAY_FRAMES) {
        work->state++;
    }
}

/// Bursts the hidden stranded corpse into independently owned model chunks.
///
/// Requires live work and model after the two-tick blast delay. Releases the
/// body's primitive buffer and disables automatic recreation before spawning
/// the chunks. Retains the model coordinates through emission, then increments
/// the state with low-halfword wrap into the blast exit.
static void _actor00400StrandedDeathBlastBurst(Task* task)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model = task->extra.tmd;
    work  = task->work;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    _actor00400SpawnBodyChunks(task);
    work->state = (u16)work->state + 1;
}

/// Enters surface-spot release and delayed despawn after the stranded corpse bursts.
///
/// Requires live work. The preceding burst has hidden the model and freed its
/// primitive buffers; this state resets both inner indices for despawn.
static void _actor00400StrandedDeathBlastEnd(Task* task)
{
    _Actor00400Work* work;

    work           = task->work;
    task->state    = ACTOR_00400_TASK_DESPAWN;
    work->state    = ACTOR_00400_DESPAWN_STATE_RELEASE_SPOT;
    work->subState = 0;
}

/// Makes the stranded diver lockable and starts its normal-rate idle animation.
///
/// Requires the live linked enemy and initialized rig. Acquires one battle
/// reference, clears bob phase, seeds the frame counter and requests clip 2
/// from its beginning. Selects stranded idle at substate zero; the outer driver
/// applies the animation request later in the same frame.
static void _actor00400StrandedStart(Task* task)
{
    enum { ACTOR_00400_ANIM_STRANDED_IDLE           = 2,
           ACTOR_00400_STRANDED_INITIAL_FRAME_COUNT = 0x174B };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    Enemy*           enemy;

    work                          = task->work;
    enemy                         = task->spawnArg2.pointer;
    enemy->node.state.parts.flags = 0;
    sceneAcquireBattleRef(0);
    work->bobPhase           = 0;
    work->frameCount         = ACTOR_00400_STRANDED_INITIAL_FRAME_COUNT;
    requestWork              = task->work;
    requestWork->animStep    = ANIMATION_RATE_ONE;
    requestWork->animClip    = ACTOR_00400_ANIM_STRANDED_IDLE;
    requestWork->animRequest = DIVER_ANIM_REQUEST_RESET;
    _diverSetState(task, ACTOR_00400_STRANDED_STATE_WAIT);
}

/// Engages combat and leaves stranded idle when a nearby target is outside the rear sector.
///
/// Requires the current signed-halfword horizontal distance and 0..4095
/// relative bearing. A distance below 3500 and bearing outside [1536, 2560)
/// raises enemy alert class 1 and selects the decision entry. Returns 0 or 1;
/// the scene battle reference count is unchanged.
static inline s16 _actor00400StrandedNoticeTarget(Task* task)
{
    enum {
        ACTOR_00400_STRANDED_NOTICE_DISTANCE   = 3500,
        ACTOR_00400_STRANDED_REAR_SECTOR_START = 1536,
        ACTOR_00400_STRANDED_REAR_SECTOR_WIDTH = 1024U,
        ACTOR_00400_STRANDED_NOTICE_ALERT      = 1
    };
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;
    s32              noticedTarget;

    work          = task->work;
    noticedTarget = 0;
    if (work->targetDistance < ACTOR_00400_STRANDED_NOTICE_DISTANCE) {
        if ((u32)(work->targetBearing - ACTOR_00400_STRANDED_REAR_SECTOR_START) >= ACTOR_00400_STRANDED_REAR_SECTOR_WIDTH) {
            gSceneCombatState.signals.bytes.enemyAlert = ACTOR_00400_STRANDED_NOTICE_ALERT;
            sceneEngageBattle(1);
            noticedTarget           = 1;
            nextStateWork           = task->work;
            nextStateWork->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            nextStateWork->subState = 0;
        }
    }
    return noticedTarget;
}

/// Leaves stranded idle on noticing a nearby target or taking a hit.
///
/// Requires the current target distance and bearing, live enemy and work.
/// Target notice takes precedence over hit consumption. A state-changing hit
/// engages battle; a hit without a state change selects decision. Neither path
/// acquires another battle reference or ticks the animation here.
static void _actor00400StrandedIdleWait(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (_actor00400StrandedNoticeTarget(task) == 0) {
        if (_actor00400ApplyHitReaction(task) != 0) {
            sceneEngageBattle(1);
            return;
        }
        if (work->hitTaken != 0) {
            _diverSetState(task, ACTOR_00400_STRANDED_STATE_DECIDE);
        }
    }
}

/// Handles pending hit reactions before dispatching the stranded attack decision.
///
/// Requires live work at substate 0; the one-entry stack table is indexed
/// without a bounds check. A state-changing hit skips the decision, otherwise
/// target distance and recent choices select a crawl or discharge.
static void _actor00400StrandedDecision(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[1] = {
        _actor00400StrandedDecide,
    };

    if (_actor00400ApplyHitReaction(task) == 0) {
        states[work->subState](task);
    }
}

/// Dispatches stranded state 3's clip entry or wait after handling hit reactions.
///
/// Requires live work and substate 0 (entry) or 1 (wait), without a bounds
/// check. Entry blends clip 6 at normal rate; wait returns to decision on a
/// published animation boundary, jump or settled pose. No living transition
/// selects this state, and the clip's visual role is unproven.
static void _actor00400StrandedState3(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedState3Enter,
        _actor00400StrandedState3Wait,
    };

    if (_actor00400ApplyHitReaction(task) == 0) {
        states[work->subState](task);
    }
}

/// Handles pending hit reactions before dispatching a stranded crawl step.
///
/// Requires live work, initialized rig, current target distance and live player.
/// Substate 0 enters and 1 ticks, without a bounds check. A state-changing hit
/// skips the stride; otherwise the crawl anchors the arms and turns toward the
/// player until the target is close or the published animation status fires.
static void _actor00400StrandedCrawl(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedCrawlEnter,
        _actor00400StrandedCrawlTick,
    };

    if (_actor00400ApplyHitReaction(task) == 0) {
        states[work->subState](task);
    }
}

/// Handles pending hit reactions before dispatching the stranded spark discharge.
///
/// Requires live work, initialized rig and the linked attack sphere. Substate
/// 0 enters and 1 waits, without a bounds check. A state-changing hit skips the
/// handler. The entry starts the 24-frame attack window; the outer frame driver
/// emits its sparks and enables the sphere independently of the clip wait.
static void _actor00400StrandedDischarge(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedDischargeEnter,
        _actor00400StrandedDischargeWait,
    };

    if (_actor00400ApplyHitReaction(task) == 0) {
        states[work->subState](task);
    }
}

/// Leaves unused stranded state-table slot 6 inert.
///
/// No living transition selects this slot. Accepts the dispatcher task without
/// accessing it; its distinct callback remains part of the ten-entry table.
static void _actor00400StrandedUnusedState6(Task* task)
{
}

/// Dispatches the stranded light recoil's entry or animation wait.
///
/// Requires live work, enemy and initialized rig at substate 0 (entry) or 1
/// (wait), without a bounds check. Entry plays the sound before requesting the
/// clip; wait handles repeated hits and returns to decision when the published
/// animation status fires. Hit consumption belongs to that wait.
static void _actor00400StrandedLightRecoil(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedLightRecoilEnter,
        _actor00400StrandedLightRecoilWait,
    };

    states[work->subState](task);
}

/// Dispatches the stranded heavy recoil's clip entry or hit-aware wait.
///
/// Requires live work and subState 0 (enter) or 1 (wait); no bounds check.
/// The two stack callbacks request the recoil and then return to decision
/// on a clip boundary, jump or settled pose, unless a hit changes state.
static void _actor00400StrandedHeavyRecoil(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedHeavyRecoilEnter,
        _actor00400StrandedHeavyRecoilWait,
    };

    states[work->subState](task);
}

/// Dispatches entry and ticking of the stranded status hold.
///
/// Requires live work and subState 0 (enter) or 1 (tick); no bounds check.
/// The callbacks suppress looking and boost critical chance until enemy
/// buildup expires, then restore looking and enter the decision state.
static void _actor00400StrandedStatusHold(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400StrandedStatusHoldEnter,
        _actor00400StrandedStatusHoldTick,
    };

    states[work->subState](task);
}

/// Starts the swimming patrol at waypoint zero and advances to travel.
///
/// Requires live work and a borrowed eight-point waypoint ring. Leaves the
/// animation and counters intact; later travel sets the height and clip.
static void _actor00400SwimPatrolEnter(Task* task)
{
    _Actor00400Work* work;

    work                = task->work;
    work->waypointIndex = 0;
    work->subState      = work->subState + 1;
}

/// Swims the patrol ring, starting a surface cycle on waypoint arrival.
///
/// Requires live work, enemy, root, rig and an eight-point waypoint ring with
/// index 0..7. Arrival advances the waypoint, requests clip 5 with a 16-frame
/// normal-rate blend when needed, restores colour, targets water-surface Y,
/// clears elapsed frames and enters the surface step. Travel retracts the
/// neck and queues the placement-tagged swim sound once per sixteen ticks.
static void _actor00400SwimPatrolTravel(Task* task)
{
    enum {
        ACTOR_00400_PATROL_SURFACE_CLIP = 5,
        ACTOR_00400_PATROL_SOUND_PERIOD = 16
    };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    s32              panOffset;
    s32              soundId;

    work = task->work;
    if ((s16)_actor00400SwimToNextWaypoint(task) != 0) {
        if (work->animClip != ACTOR_00400_PATROL_SURFACE_CLIP) {
            requestWork = task->work;
            _diverRequestClipBlend(requestWork, ACTOR_00400_PATROL_SURFACE_CLIP, ANIMATION_RATE_ONE, 16);
        }
        work->goalY = work->waterLevel;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
        return;
    }
    work->neckRetracted = 1;
    if (!(work->frameCount & (ACTOR_00400_PATROL_SOUND_PERIOD - 1))) {
        soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_SWIM;
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Retracts the neck and resumes patrol travel on the published clip status.
///
/// Requires live work and an initialized rig. A boundary, jump or settled
/// pose selects patrol substate 1; otherwise stays in this wait. This reads
/// the preceding tick's status without advancing playback.
static void _actor00400SwimPatrolWaitForClip(Task* task)
{
    enum { ACTOR_00400_PATROL_STEP_TRAVEL = 1 };
    _Actor00400Work* work;

    work                = task->work;
    work->neckRetracted = 1;
    if (_diverClipHasBoundaryOrJump(task)) {
        work->subState = ACTOR_00400_PATROL_STEP_TRAVEL;
    }
}

/// Captures the claimed surface spot and starts the diver's emergence.
///
/// Requires live work, root and rig at emerge substate zero. Copies XYZ
/// without the vector pad, clears elapsed ticks and moves root XZ a quarter
/// toward the signed-halfword spot in the root's parent frame. Queues surfaced
/// clip 1 at normal rate with a ten-frame blend, then advances to the emerge
/// tick. The captured position stays fixed if a later claim changes.
static void _actor00400SwimEmergeEnter(Task* task)
{
    enum { ACTOR_00400_EMERGE_BLEND_FRAMES = 10 };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    GfxCoord*        rootCoord;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    work->emergePos.vx     = work->surfaceSpot.vx;
    work->stateFrames      = 0;
    work->emergePos.vy     = work->surfaceSpot.vy;
    work->emergePos.vz     = work->surfaceSpot.vz;
    rootCoord->coord.t[0] += (work->emergePos.vx - rootCoord->coord.t[0]) >> 2;
    rootCoord->coord.t[2] += (work->emergePos.vz - rootCoord->coord.t[2]) >> 2;
    requestWork            = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SURFACED, ANIMATION_RATE_ONE, ACTOR_00400_EMERGE_BLEND_FRAMES);
    work->subState = work->subState + 1;
}

/// Advances from the dive's transition-clip wait to swimming toward a surface spot.
///
/// Requires live work and an initialized rig at dive substate 1. Advances one
/// substate on a published boundary, jump or settled pose, without ticking
/// playback or changing the room-owned surface claim.
static void _actor00400DiveWaitForClip(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (_diverClipHasBoundaryOrJump(task)) {
        work->subState = work->subState + 1;
    }
}

#include "../../shared/diver_state7_enter.inc.c"

/// Holds swimming heavy recoil, playing the hit sound on each damaged tick.
///
/// Requires live work, enemy and rig. Tests the previously published clip
/// status after the sound request; a boundary, jump or settled pose selects
/// swimming decision at substate zero. Animation playback stays with the
/// outer driver, and pending hit reactions stay with the recoil dispatcher.
static void _actor00400SwimHeavyRecoilWait(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;

    work = task->work;
    if (work->hitTaken != 0) {
        _actor00400PlayHitSound(task);
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        nextStateWork           = task->work;
        nextStateWork->state    = ACTOR_00400_SWIM_STATE_DECIDE;
        nextStateWork->subState = 0;
    }
}

/// Enters the swimming status hold with the trunk exposed as the target.
///
/// Requires the live diver task and initialized animation rig. Requests the
/// hold-entry clip, sets the goal 100 coordinate units below the water level,
/// restores default colouring, and applies the 100-fold critical-chance
/// multiplier until the status hold ends. Advances to the hold step.
static void _actor00400SwimStatusHoldEnter(Task* task)
{
    enum {
        ACTOR_00400_STATUS_HOLD_DEPTH          = 100,
        ACTOR_00400_STATUS_CRITICAL_MULTIPLIER = 100,
        ACTOR_00400_TARGET_PART_TRUNK          = 1
    };
    _Actor00400Work* work;

    work = task->work;
    _actor00400RequestNormalClipBlend(work, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_ENTER, 8);
    work->goalY = (u16)work->waterLevel + ACTOR_00400_STATUS_HOLD_DEPTH;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    work->critChanceScale = ACTOR_00400_STATUS_CRITICAL_MULTIPLIER;
    work->stateFrames     = 0;
    work->targetPart      = ACTOR_00400_TARGET_PART_TRUNK;
    work->subState        = work->subState + 1;
}

/// Keeps the swimming diver in its status hold until buildup expires.
///
/// Requires the live rig and enemy. Counts a tick, requests the hit clip when
/// `hitTaken` is exactly 1, otherwise blends to the hold loop on a clip boundary,
/// jump or settled pose. The signed hit value is reused as the BLEND request.
/// Buildup expiry clears the critical-chance multiplier, restores the head as
/// target part and selects the dive entry step.
static void _actor00400SwimStatusHoldTick(Task* task)
{
    enum { ACTOR_00400_STATUS_TARGET_HEAD = 4,
           ACTOR_00400_STATUS_HIT_TAKEN   = 1 };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    _Actor00400Work* nextStateWork;
    s32              hitTaken;

    work     = task->work;
    hitTaken = work->hitTaken;
    work->stateFrames++;
    if (hitTaken == ACTOR_00400_STATUS_HIT_TAKEN) {
        requestWork              = task->work;
        requestWork->animBlend   = 2;
        requestWork->animStep    = ANIMATION_RATE_ONE;
        requestWork->animClip    = ACTOR_00400_ANIM_SWIM_STATUS_HOLD_HIT;
        requestWork->animRequest = hitTaken;
    } else {
        if (_diverClipHasBoundaryOrJump(task)) {
            requestWork = task->work;
            _actor00400RequestNormalClipBlend(requestWork, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_LOOP, 8);
        }
    }
    // End the critical-chance boost only when the status buildup has expired.
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        work->critChanceScale   = 0;
        work->targetPart        = ACTOR_00400_STATUS_TARGET_HEAD;
        nextStateWork           = task->work;
        nextStateWork->state    = ACTOR_00400_SWIM_STATE_DIVE;
        nextStateWork->subState = 0;
    }
}

/// Releases neck retraction and enters the swimming attack's windup.
///
/// Requires live work at attack substate 0. Clears elapsed frames and sets
/// goal Y 100 coordinate units below the water surface before advancing.
/// Height motion and the windup's discharge are handled by later ticks.
static void _actor00400SwimAttackEnter(Task* task)
{
    enum { ACTOR_00400_ATTACK_DEPTH = 100 };
    _Actor00400Work* work;

    work                = task->work;
    work->neckRetracted = 0;
    work->stateFrames   = 0;
    work->goalY         = (u16)work->waterLevel + ACTOR_00400_ATTACK_DEPTH;
    work->subState      = work->subState + 1;
}

/// Prepares the tunnel patrol animation at the first waypoint.
///
/// Requires the live diver work block. Starts a ten-frame blend to the normal
/// swim clip and advances to the cue wait; it does not reset the frame counter.
static void _actor00400TunnelPatrolEnter(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* animWork;

    work                = task->work;
    work->waypointIndex = 0;
    animWork            = task->work;
    _actor00400RequestNormalClipBlend(animWork, ACTOR_00400_ANIM_SWIM, 0xA);
    work->subState = work->subState + 1;
}

/// Waits for the tunnel patrol cue or an already-seen tunnel introduction.
///
/// Advances to waypoint swimming when either condition holds. The command
/// remains latched in the live diver work block.
static void _actor00400TunnelPatrolWaitForCue(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_PATROL || gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        work->subState = work->subState + 1;
    }
}

/// Places the tunnel-introduction diver at the scripted swim start.
///
/// Requires the live diver model and work block. Positions and the goal height
/// are in the root's view-parent space; heading and roll start at zero. Restarts
/// the swim clip at normal rate, clears the counter and advances to the swim cue.
static void _actor00400TunnelIntroEnter(Task* task)
{
    enum {
        ACTOR_00400_TUNNEL_INTRO_X = 0x10E0,
        ACTOR_00400_TUNNEL_INTRO_Y = 0x178,
        ACTOR_00400_TUNNEL_INTRO_Z = -0xDAC
    };
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    _Actor00400Work* animWork;

    work                  = task->work;
    rootCoord             = task->extra.tmd->coords;
    rootCoord->coord.t[0] = ACTOR_00400_TUNNEL_INTRO_X;
    rootCoord->coord.t[1] = ACTOR_00400_TUNNEL_INTRO_Y;
    work->goalY           = ACTOR_00400_TUNNEL_INTRO_Y;
    rootCoord->coord.t[2] = ACTOR_00400_TUNNEL_INTRO_Z;
    work->rotation.vx     = 0;
    work->rotation.vy     = 0;
    work->rotation.vz     = 0;
    animWork              = task->work;
    animWork->animStep    = ANIMATION_RATE_ONE;
    animWork->animClip    = ACTOR_00400_ANIM_SWIM;
    animWork->animRequest = DIVER_ANIM_REQUEST_RESET;
    work->stateFrames     = 0;
    work->subState        = work->subState + 1;
}

/// Waits for the room command that starts the tunnel-introduction swim.
///
/// Advances one step on the latched swim cue without consuming the command
/// or resetting the frame counter.
static void _actor00400TunnelIntroWaitForSwim(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_SWIM) {
        work->subState = work->subState + 1;
    }
}

/// Runs the tunnel introduction's timed forward swim.
///
/// Requires a live root and a counter cleared by the entry step. Pre-increments
/// the signed halfword counter and steps 160 coordinate units while it is below
/// 48; tick 48 advances to the patrol cue without moving. Height is unchanged.
static void _actor00400TunnelIntroSwim(Task* task)
{
    enum { ACTOR_00400_TUNNEL_INTRO_SWIM_TICKS = 48 };
    _Actor00400Work* work;

    work = task->work;
    if (++work->stateFrames < ACTOR_00400_TUNNEL_INTRO_SWIM_TICKS) {
        _diverStepForward(task, 0xA0, work->rotation.vy);
        return;
    }
    work->subState++;
}

/// Hands the tunnel-introduction diver to the patrol state on the patrol cue.
///
/// Requires the live diver work block. Selects the patrol entry step; the room
/// command remains latched.
static void _actor00400TunnelIntroWaitForPatrol(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_PATROL) {
        work->state    = ACTOR_00400_SWIM_STATE_TUNNEL_PATROL;
        work->subState = 0;
    }
}

/// Dispatches the room introduction's seven scripted steps.
///
/// Requires live work and subState 0..6; no bounds check. Copies the callback
/// table to the stack, then runs placement, cued swimming/surfacing, discharge
/// and the fight-cue handoff. Room commands remain latched between steps.
static void _actor00400RoomIntro(Task* task)
{
    _Actor00400Work* work;
    TaskFuncTable7   stateHandlers;

    work          = task->work;
    stateHandlers = Actor00400_D00178;
    stateHandlers.funcs[work->subState](task);
}

/// Dispatches the waiting diver's single fight-cue handler.
///
/// Requires live work with subState 0; no bounds check. The callback keeps
/// waiting on the latched command, then enables targeting, claims a surface
/// spot, acquires a battle reference and selects the combat dive entry.
static void _actor00400AwaitFight(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[1] = {
        _actor00400AwaitFightCue,
    };

    states[work->subState](task);
}

/// Places the room-introduction diver at its submerged swim start.
///
/// Requires the live diver model and initialized rig. Sets the root in its
/// view-parent space facing a half turn (2048 angle units), retracts the neck,
/// and restarts the swim clip at normal rate. Clears the counter and advances
/// to the swim cue.
static void _actor00400RoomIntroEnter(Task* task)
{
    enum { ACTOR_00400_ROOM_INTRO_REVERSE_HEADING = 0x800 };
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    _Actor00400Work* animWork;

    work                  = task->work;
    rootCoord             = task->extra.tmd->coords;
    work->neckRetracted   = 1;
    rootCoord->coord.t[0] = ACTOR_00400_ROOM_INTRO_X;
    rootCoord->coord.t[1] = ACTOR_00400_ROOM_INTRO_DEPTH;
    work->goalY           = ACTOR_00400_ROOM_INTRO_DEPTH;
    rootCoord->coord.t[2] = ACTOR_00400_ROOM_INTRO_START_Z;
    work->rotation.vx     = 0;
    work->rotation.vy     = ACTOR_00400_ROOM_INTRO_REVERSE_HEADING;
    work->rotation.vz     = 0;
    animWork              = task->work;
    animWork->animStep    = ANIMATION_RATE_ONE;
    animWork->animClip    = ACTOR_00400_ANIM_SWIM;
    animWork->animRequest = DIVER_ANIM_REQUEST_RESET;
    work->stateFrames     = 0;
    work->subState        = work->subState + 1;
}

/// Waits for the swim cue unless encounter progress skips the introductory swim.
///
/// Progress nibble 1 goes directly to the surface-cue step; it takes precedence
/// over the latched swim command. Requires the live diver work block.
static void _actor00400RoomIntroWaitForSwim(Task* task)
{
    enum {
        ACTOR_00400_SEPTIC_TANK_SKIP_INTRO_SWIM      = 1,
        ACTOR_00400_ROOM_INTRO_STEP_WAIT_FOR_SURFACE = 3
    };
    _Actor00400Work* work;

    work = task->work;
    if (gameFlagGetNibble(GAME_FLAG_0EB) == ACTOR_00400_SEPTIC_TANK_SKIP_INTRO_SWIM) {
        work->subState = ACTOR_00400_ROOM_INTRO_STEP_WAIT_FOR_SURFACE;
    } else if (work->command == ACTOR_00400_COMMAND_INTRO_SWIM) {
        work->subState = work->subState + 1;
    }
}

/// Runs the room introduction's timed forward swim and periodic swim sound.
///
/// Requires the live root and enemy, with the entry counter initially zero.
/// Increments the halfword counter and steps 96 coordinate units while its
/// signed value is below 48; tick 48 advances without moving. Moving ticks
/// sound once per sixteen `frameCount` ticks with signed-byte audio pan/depth.
static void _actor00400RoomIntroSwim(Task* task)
{
    enum { ACTOR_00400_ROOM_INTRO_SWIM_TICKS = 48,
           ACTOR_00400_SWIM_SOUND_TICK_MASK  = 15 };
    u16              elapsedFrames;
    s32              soundId;
    s32              panOffset;
    _Actor00400Work* work;

    work              = task->work;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames < ACTOR_00400_ROOM_INTRO_SWIM_TICKS) {
        _diverStepForward(task, 0x60, work->rotation.vy);
        if (!(work->frameCount & ACTOR_00400_SWIM_SOUND_TICK_MASK)) {
            soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_SWIM;
            panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    } else {
        work->subState += 1;
    }
}

/// Fixes the room introduction's endpoint XZ and clears the stored rotation.
///
/// Borrows live work and root coordinates in the view-parent space. Preserves
/// root Y, the goal height and composition stamp; the enclosing frame rebuilds
/// and dirties the root matrix after this state step.
static inline void _actor00400PlaceRoomIntroEndpoint(_Actor00400Work* work, GfxCoord* rootCoord)
{
    rootCoord->coord.t[0] = ACTOR_00400_ROOM_INTRO_X;
    rootCoord->coord.t[2] = ACTOR_00400_ROOM_INTRO_STOP_Z;
    work->rotation.vx     = 0;
    work->rotation.vy     = 0;
    work->rotation.vz     = 0;
}

/// Holds the diver at the scripted endpoint until the room cues it to surface.
///
/// Requires the live diver model and initialized rig. Before the cue, resets
/// the submerged position, rotation and counter every frame. On the cue, keeps
/// the current root height, sets the goal to the water surface and requests an
/// eight-frame blend to the surfaced clip before advancing. Coordinates are
/// in the root's view-parent space.
static void _actor00400RoomIntroWaitForSurface(Task* task)
{
    _Actor00400Work* work;
    GfxCoord*        rootCoord;
    _Actor00400Work* animWork;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Hold the submerged pose until the cue changes the height goal.
    if (work->command == ACTOR_00400_COMMAND_INTRO_SURFACE) {
        work->stateFrames = 0;
        work->goalY       = work->waterLevel;
        _actor00400PlaceRoomIntroEndpoint(work, rootCoord);
        animWork = task->work;
        _actor00400RequestNormalClipBlend(animWork, ACTOR_00400_ANIM_SURFACED, 8);
        work->subState = work->subState + 1;
    } else {
        work->stateFrames = 0;
        _actor00400PlaceRoomIntroEndpoint(work, rootCoord);
        rootCoord->coord.t[1] = ACTOR_00400_ROOM_INTRO_DEPTH;
        work->goalY           = ACTOR_00400_ROOM_INTRO_DEPTH;
    }
}

/// Runs the timed pause after the introduction's discharge.
///
/// Requires the live diver task, enemy and model. Counts from zero, queues the
/// room end cue at frame 38 with the model's audio pan/depth, and advances at
/// frame 48. Preserves the signed 16-bit counter comparison and wrap behavior.
static void _actor00400RoomIntroWaitAfterDischarge(Task* task)
{
    enum {
        ACTOR_00400_INTRO_DISCHARGE_END_CUE_FRAME = 38,
        ACTOR_00400_INTRO_DISCHARGE_WAIT_FRAMES   = 48,
        ACTOR_00400_SOUND_INTRO_DISCHARGE_END_CUE = 0x54220006
    };
    u16              elapsedFrames;
    s32              soundId;
    s32              panOffset;
    _Actor00400Work* work;

    work              = task->work;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_00400_INTRO_DISCHARGE_END_CUE_FRAME) {
        soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_00400_SOUND_INSTANCE_SHIFT) | ACTOR_00400_SOUND_INTRO_DISCHARGE_END_CUE;
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if ((s16)work->stateFrames == ACTOR_00400_INTRO_DISCHARGE_WAIT_FRAMES) {
        work->subState += 1;
    }
}

/// Makes the introduction diver targetable and enters combat on the fight cue.
///
/// Requires the live diver task, linked enemy, model and surface-spot list.
/// Clears all target flags, claims the spot nearest the current target and
/// acquires one battle reference before selecting the dive entry step. The
/// state transition prevents a second acquisition on the still-latched cue.
static void _actor00400RoomIntroWaitForFight(Task* task)
{
    Enemy*                 enemy;
    const _Actor00400Work* commandWork;
    _Actor00400Work*       work;

    enemy       = task->spawnArg2.pointer;
    commandWork = task->work;
    if (commandWork->command == ACTOR_00400_COMMAND_FIGHT) {
        enemy->node.state.parts.flags = 0;
        _actor00400ClaimNearestSurfaceSpot(task);
        sceneAcquireBattleRef(0);
        work           = task->work;
        work->state    = ACTOR_00400_SWIM_STATE_DIVE;
        work->subState = 0;
    }
}

/// Makes the waiting diver targetable and enters combat on the fight cue.
///
/// Requires the live enemy and a terminated borrowed surface-spot list. Clears
/// target flags, claims the spot nearest the cached target and acquires one
/// battle reference before selecting the dive entry step. That transition
/// prevents repeated acquisition from the command, which remains latched.
static void _actor00400AwaitFightCue(Task* task)
{
    Enemy*                 enemy;
    const _Actor00400Work* commandWork;
    _Actor00400Work*       work;

    enemy       = task->spawnArg2.pointer;
    commandWork = task->work;
    if (commandWork->command == ACTOR_00400_COMMAND_FIGHT) {
        enemy->node.state.parts.flags = 0;
        _actor00400ClaimNearestSurfaceSpot(task);
        sceneAcquireBattleRef(0);
        work           = task->work;
        work->state    = ACTOR_00400_SWIM_STATE_DIVE;
        work->subState = 0;
    }
}

#include "../../shared/coord_math_local_to_world.inc.c"

/// Launches the spawned shot and links its attack sphere.
///
/// Requires the coordinate body and zeroed owned shot work from the spawner.
/// Parents the placed root to the view, resets both frame counters and links
/// the attack-table row-1 key on a radius-256 sphere with two fresh contacts.
/// Enables grid/pair tests, composes the root, replaces vertical speed with
/// -20 game units per tick and draws the launch burst before entering flight.
/// Horizontal speed and position are retained from the spawner.
static void _actor00400LaunchShot(Task* task)
{
    enum { ACTOR_00400_SHOT_ATTACK_INDEX = 1,
           ACTOR_00400_SHOT_RADIUS       = 256 };
    _Actor00400ShotWork* work;
    GfxCoord*            rootCoord;

    rootCoord                               = task->extra.coordBody->coord;
    work                                    = task->work;
    task->killCountdown                     = 0;
    work->frames                            = 0;
    rootCoord->parent                       = &gGfxViewCoord;
    rootCoord->composeStamp                 = GRAPHICS_COORD_DIRTY;
    work->child.attackBody.key              = damagePackAttackKey(Actor00400_D0FDC0, ACTOR_00400_SHOT_ATTACK_INDEX);
    work->child.attackBody.coord            = task->extra.coordBody->coord;
    work->child.attackBody.context.contacts = work->contacts;
    work->child.attackBody.pos.vx           = 0;
    work->child.attackBody.pos.vy           = 0;
    work->child.attackBody.pos.vz           = 0;
    work->child.attackBody.radius           = ACTOR_00400_SHOT_RADIUS;
    work->child.attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->child.attackBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->child.attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actorRenderComposeCoord(rootCoord);
    work->velocity.vy = ACTOR_00400_SHOT_LAUNCH_SPEED_Y;
    _diverImpactBurst(rootCoord, work->frames, DIVER_BURST_LAUNCH, ACTOR_00400_SHOT_BURST_SIZE_AND_SPRAY_BIAS);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

/// Draws the stain at its current strength and starts fading once the diver dies.
///
/// Requires the stain's work and borrowed enemy to be live. The final hold
/// frame is drawn before advancing task state 0 to state 1 on signed HP <= 0.
static void _actor00400GroundStainHold(Task* task)
{
    _Actor00400GroundStainWork* work;
    Enemy*                      enemy;

    work  = task->work;
    enemy = work->enemy;
    _actor00400DrawGroundStain(&work->vertices[0], &work->vertices[1],
                               &work->vertices[2], &work->vertices[3], work->intensity);
    if ((s16)enemy->hp <= 0) {
        task->state++;
    }
}

/// Draws and fades the ground stain by one intensity step per frame.
///
/// Enters with intensity 1..255. The current strength is drawn before the
/// decrement; reaching zero kills the task and releases its owned work.
static void _actor00400GroundStainFade(Task* task)
{
    _Actor00400GroundStainWork* work;
    u8                          intensity;

    work = task->work;
    _actor00400DrawGroundStain(&work->vertices[0], &work->vertices[1],
                               &work->vertices[2], &work->vertices[3], work->intensity);
    intensity       = work->intensity - 1;
    work->intensity = intensity;
    if (intensity == 0) {
        taskKill(task);
    }
}

/// Releases the diver's surface-spot claim before its delayed despawn.
///
/// Requires a live borrowed surface-spot list and a valid `surfaceSpotIndex`
/// (entry zero is the unclaimed default). Clears any nonzero claim halfword,
/// resets the delay counter and advances the despawn state to its timed wait.
/// The room owns the list; this callback does not free it.
static void _actor00400ReleaseSurfaceSpot(Task* task)
{
    enum { ACTOR_00400_SURFACE_SPOT_FREE = 0 };
    _Actor00400Work* work;

    work = task->work;
    if (work->surfaceSpots[work->surfaceSpotIndex].pad != ACTOR_00400_SURFACE_SPOT_FREE) {
        work->surfaceSpots[work->surfaceSpotIndex].pad = ACTOR_00400_SURFACE_SPOT_FREE;
    }
    work->stateFrames = 0;
    work->state       = (u16)work->state + 1;
}

/// Destroys the diver after 301 ticks of the despawn wait.
///
/// Enters with the signed-halfword counter cleared and the surface claim
/// released. The increment wraps through 16 bits before the signed comparison.
/// Destruction retires the enemy and its task; nothing accesses them afterward.
static void _actor00400DespawnWait(Task* task)
{
    enum { ACTOR_00400_DESPAWN_WAIT_FRAMES = 301 };
    _Actor00400Work* work;
    u16              elapsedFrames;

    work              = task->work;
    elapsedFrames     = (u16)work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= ACTOR_00400_DESPAWN_WAIT_FRAMES) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}

/// Dispatches entry and ticking of the floating wounded diver's idle pose.
///
/// Requires live work and rig with substate 0 (enter) or 1 (tick). The stack
/// table has no bounds check; the outer driver applies clip requests and height.
static void _actor00400WoundedFloatIdle(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400WoundedFloatIdleEnter,
        _actor00400WoundedFloatIdleTick,
    };

    states[work->subState](task);
}

/// Dispatches entry and timed ticking of the floating wounded diver's flinch.
///
/// Requires live work and rig with substate 0 (enter) or 1 (tick). The stack
/// table has no bounds check; the outer driver applies clip requests and height.
static void _actor00400WoundedFloatFlinch(Task* task)
{
    _Actor00400Work* work      = task->work;
    TaskFunc         states[2] = {
        _actor00400WoundedFloatFlinchEnter,
        _actor00400WoundedFloatFlinchTick,
    };

    states[work->subState](task);
}

/// Starts the wounded float's idle clip with a newly drawn slow playback rate.
///
/// Requires live work and an initialized rig at substate 0. Targets the water
/// surface, advances the shared LCG once, and blends clip 16 over eight frames
/// at a rate of 3..6 sixteenths of a frame per tick. Clears elapsed frames and
/// enters the idle tick. The animation request precedes publication of the
/// new random state.
static void _actor00400WoundedFloatIdleEnter(Task* task)
{
    enum {
        ACTOR_00400_WOUNDED_IDLE_RANDOM_SHIFT = 16,
        ACTOR_00400_WOUNDED_IDLE_RATE_MASK    = 3,
        ACTOR_00400_WOUNDED_IDLE_RATE_MIN     = 3
    };
    _Actor00400Work* requestWork;
    _Actor00400Work* work;
    u32              randomValue;

    work        = task->work;
    randomValue = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->goalY = work->waterLevel;
    requestWork = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_LOOP,
                           ((randomValue >> ACTOR_00400_WOUNDED_IDLE_RANDOM_SHIFT) & ACTOR_00400_WOUNDED_IDLE_RATE_MASK) + ACTOR_00400_WOUNDED_IDLE_RATE_MIN, 8);
    gRandomLcgState   = randomValue;
    work->stateFrames = 0;
    work->subState++;
}

/// Starts the wounded float's flinch clip and advances to its timed tick.
///
/// Requires live work and an initialized rig at substate 0. Requests clip 18
/// at normal rate with a two-frame blend, then clears elapsed frames.
/// Leaves the floating height goal and shared random state intact.
static void _actor00400WoundedFloatFlinchEnter(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    _diverRequestClipBlend(work, ACTOR_00400_ANIM_SWIM_STATUS_HOLD_HIT, ANIMATION_RATE_ONE, 2);
    work->stateFrames = 0;
    work->subState++;
}

/// Chooses a stranded discharge in reach, or a crawl, and records the choice.
///
/// Requires the current horizontal target distance and history index 0..2.
/// Below 1250 coordinate units chooses discharge unless that would make three
/// consecutive discharges; then crawl replaces the newest history entry.
/// Every choice clears the substate and advances the three-entry ring index.
static void _actor00400StrandedDecide(Task* task)
{
    enum { ACTOR_00400_STRANDED_DISCHARGE_DISTANCE = 1250 };
    _Actor00400Work* work;
    _Actor00400Work* nextStateWork;

    work = task->work;
    if (work->targetDistance < ACTOR_00400_STRANDED_DISCHARGE_DISTANCE) {
        work->state                                 = ACTOR_00400_STRANDED_STATE_DISCHARGE;
        work->subState                              = 0;
        work->stateHistory[work->stateHistoryIndex] = work->state;
        if (work->stateHistory[0] == work->stateHistory[1] &&
            work->stateHistory[0] == work->stateHistory[2] &&
            work->stateHistory[0] == ACTOR_00400_STRANDED_STATE_DISCHARGE) {
            // Break repeated discharges even while the target remains in reach.
            nextStateWork                               = task->work;
            nextStateWork->state                        = ACTOR_00400_STRANDED_STATE_CRAWL;
            nextStateWork->subState                     = 0;
            work->stateHistory[work->stateHistoryIndex] = work->state;
        }
    } else {
        work->state                                 = ACTOR_00400_STRANDED_STATE_CRAWL;
        work->subState                              = 0;
        work->stateHistory[work->stateHistoryIndex] = work->state;
    }
    work->stateHistoryIndex++;
    if (work->stateHistoryIndex >= (u32)ARRAY_SIZE(work->stateHistory)) {
        work->stateHistoryIndex = 0;
    }
}

/// Queues stranded state 3's clip and advances to its wait step.
///
/// Requires the live rig. Clip 6 plays at normal rate with a six-frame blend;
/// its visual role is unproven and the living state machine never selects this state.
static void _actor00400StrandedState3Enter(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    _actor00400RequestNormalClipBlend(work, ACTOR_00400_ANIM_STRANDED_STATE3, 6);
    work->subState++;
}

/// Returns stranded state 3 to decision on an animation boundary, jump or settled pose.
///
/// Tests the published slot-1 status without ticking the rig; otherwise leaves
/// the state unchanged. Selecting decision clears its substate.
static void _actor00400StrandedState3Wait(Task* task)
{
    _Actor00400Work* work;

    if (_diverClipHasBoundaryOrJump(task)) {
        work           = task->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

/// Starts a stranded crawl stride unless the target is already within 948 units.
///
/// Requires the live rig and current horizontal target distance. A nearby
/// target selects decision entry; otherwise advances the stride and enters
/// the crawl tick step. The stride helper turns toward the live player.
static void _actor00400StrandedCrawlEnter(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->targetDistance < ACTOR_00400_STRANDED_CRAWL_STOP_DISTANCE) {
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
        return;
    }
    _actor00400CrawlStride(task);
    work->subState++;
}

/// Advances a stranded crawl until the target is near or the animation reports a boundary.
///
/// Requires the live rig and current horizontal target distance. Below 948
/// units returns to decision before moving. Otherwise advances the stride,
/// then tests the published status for a boundary, jump or settled pose.
static void _actor00400StrandedCrawlTick(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    if (work->targetDistance < ACTOR_00400_STRANDED_CRAWL_STOP_DISTANCE) {
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
        return;
    }
    _actor00400CrawlStride(task);
    if (_diverClipHasBoundaryOrJump(task)) {
        work           = task->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

/// Starts the stranded discharge animation and its 24-frame spark attack window.
///
/// Requires the live rig and linked discharge sphere. Requests normal-rate
/// clip 9 with a six-frame blend and enters the animation wait. The frame
/// driver consumes `attackFrames` to emit sparks and enable the sphere.
static void _actor00400StrandedDischargeEnter(Task* task)
{
    enum { ACTOR_00400_STRANDED_DISCHARGE_FRAMES = 24 };
    _Actor00400Work* work;

    work = task->work;
    _actor00400RequestNormalClipBlend(work, ACTOR_00400_ANIM_STRANDED_DISCHARGE, 6);
    work->attackFrames = ACTOR_00400_STRANDED_DISCHARGE_FRAMES;
    work->subState++;
}

/// Returns the stranded discharge to decision on an animation boundary, jump or settled pose.
///
/// Tests the published slot-1 status without ticking the rig. The discharge
/// countdown continues in the frame driver independently of this transition.
static void _actor00400StrandedDischargeWait(Task* task)
{
    _Actor00400Work* work;

    if (_diverClipHasBoundaryOrJump(task)) {
        work           = task->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

/// Plays the placement-tagged hit sound and enters the stranded light-recoil wait.
///
/// Requires live work, enemy and model. The sound precedes the normal-rate
/// light-recoil request, whose blend lasts six frames.
static void _actor00400StrandedLightRecoilEnter(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work = task->work;
    _actor00400PlayHitSound(task);
    requestWork = task->work;
    _actor00400RequestNormalClipBlend(requestWork, ACTOR_00400_ANIM_STRANDED_RECOIL_LIGHT, 6);
    work->subState++;
}

/// Queues the stranded heavy-recoil clip, plays its hit sound and enters the wait.
///
/// Requires live work, enemy and model. The normal-rate request with a
/// four-frame blend precedes the placement-tagged sound.
static void _actor00400StrandedHeavyRecoilEnter(Task* task)
{
    _Actor00400Work* work;

    work = task->work;
    _actor00400RequestNormalClipBlend(work, ACTOR_00400_ANIM_STRANDED_RECOIL_HEAVY, 4);
    _actor00400PlayHitSound(task);
    work->subState++;
}

/// Enters the stranded status hold with looking suppressed and critical chance multiplied by 100.
///
/// Requires the live rig and enemy buildup reaction. Requests the normal-rate
/// entry clip with an eight-frame blend, clears elapsed ticks and enters the
/// hold tick step. The multiplier is a factor, rather than a percentage.
static void _actor00400StrandedStatusHoldEnter(Task* task)
{
    enum { ACTOR_00400_STRANDED_STATUS_CRITICAL_MULTIPLIER = 100 };
    _Actor00400Work* work;
    _Actor00400Work* requestWork;

    work               = task->work;
    work->lookDisabled = 1;
    requestWork        = task->work;
    _actor00400RequestNormalClipBlend(requestWork, ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_ENTER, 8);
    work->critChanceScale = ACTOR_00400_STRANDED_STATUS_CRITICAL_MULTIPLIER;
    work->stateFrames     = 0;
    work->subState++;
}

/// Keeps the stranded diver in its status hold until the enemy buildup reaction expires.
///
/// Requires the live rig and a started buildup reaction. Counts one tick and
/// requests the normal-rate hold loop on a boundary, jump or settled pose.
/// Buildup completion clears the critical multiplier, resumes looking and
/// selects the decision entry. The enemy reaction flag is not changed here.
static void _actor00400StrandedStatusHoldTick(Task* task)
{
    _Actor00400Work* work;
    _Actor00400Work* requestWork;
    _Actor00400Work* nextStateWork;

    work = task->work;
    work->stateFrames++;
    if (_diverClipHasBoundaryOrJump(task)) {
        requestWork = task->work;
        _actor00400RequestNormalClipBlend(requestWork, ACTOR_00400_ANIM_STRANDED_STATUS_HOLD_LOOP, 8);
    }
    if (damageTickEnemyBuildup(task->spawnArg2.pointer)) {
        work->critChanceScale   = 0;
        work->lookDisabled      = 0;
        nextStateWork           = task->work;
        nextStateWork->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        nextStateWork->subState = 0;
    }
}
