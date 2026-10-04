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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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

static void Actor00400_Fn0875C(Task* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
static void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
static void Actor00400_Fn02648(Task* arg0, s32 arg1);
static void Actor00400_Fn0237C(Task* arg0);
static void Actor00400_Fn02FF8(Task* arg0);
static void Actor00400_Fn0A190(Task* arg0);
static void Actor00400_Fn089C8(Task* arg0);
static void Actor00400_Fn03920(Task* arg0);
static void Actor00400_Fn04580(Task* arg0);
static void Actor00400_Fn04B48(Task* arg0);
static void Actor00400_Fn04E18(Task* arg0);
static void Actor00400_Fn040DC(Task* arg0);
static void Actor00400_Fn06B7C(Task* arg0);
static void Actor00400_Fn070C0(Task* arg0);
static void Actor00400_Fn08624(Task* arg0);
static s16  Actor00400_Fn086FC(Task* arg0, s16 arg1);
static void Actor00400_Fn08814(Task* arg0);
static s16  Actor00400_Fn08908(Task* arg0);
static void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
static void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
static void Actor00400_Fn060CC(Task* arg0);
static void Actor00400_Fn097C8(Task* arg0);
static void Actor00400_Fn06EA4(Task* arg0);
static void Actor00400_Fn08ADC(Task* arg0);
static void Actor00400_Fn08A88(Task* arg0);
static void Actor00400_Fn08B40(Task* arg0);
static void Actor00400_Fn08B94(Task* arg0);
static void Actor00400_Fn06F64(Task* arg0);
static void Actor00400_Fn0A880(Task* arg0);
static void Actor00400_Fn04900(Task* arg0);
static void Actor00400_Fn0A940(Task* arg0);
static void Actor00400_Fn04A1C(Task* arg0);
static void Actor00400_Fn0A9F4(Task* arg0);
static void Actor00400_Fn0AA40(Task* arg0);
static void Actor00400_Fn0A3D4(Task* arg0);
static void Actor00400_Fn0A414(Task* arg0);
static void Actor00400_Fn098A8(Task* arg0);
static void Actor00400_Fn09924(Task* arg0);
static s16  Actor00400_Fn02154(Task* arg0);
static void Actor00400_Fn0A5B8(Task* arg0);
static void Actor00400_Fn019B4(Task* arg0);
static void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3);
static void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst);

static s32  Actor00400_Fn02208(Task* arg0);
static void Actor00400_Fn0A680(Task* arg0);
static void Actor00400_Fn0A6B0(Task* arg0);
static void Actor00400_Fn0A704(Task* arg0);
static void Actor00400_Fn0A760(Task* arg0);
static void Actor00400_Fn0A7F0(Task* arg0);
static void Actor00400_Fn0A82C(Task* arg0);
static void Actor00400_Fn0A034(Task* arg0);
static void Actor00400_Fn0A510(Task* arg0);
static void Actor00400_Fn0A57C(Task* arg0);

/* States the dispatch tables name before their definitions. */
static void Actor00400_Fn042C0(Task* arg0);
static void Actor00400_Fn04414(Task* arg0);
static void Actor00400_Fn04CF8(Task* arg0);
static void Actor00400_Fn05728(Task* arg0);
static void Actor00400_Fn07738(Task* arg0);
static void Actor00400_Fn077F4(Task* arg0);
static void Actor00400_Fn078C8(Task* arg0);
static void Actor00400_Fn0793C(Task* arg0);
static void Actor00400_Fn07998(Task* task);
static void Actor00400_Fn079A0(Task* task);
static void Actor00400_Fn079A8(Task* arg0);
static void Actor00400_Fn079FC(Task* arg0);
static void Actor00400_Fn07ABC(Task* arg0);
static void Actor00400_Fn07B10(Task* arg0);
static void Actor00400_Fn07B98(Task* arg0);
static void Actor00400_Fn07C04(Task* arg0);
static void Actor00400_Fn07CC4(Task* arg0);
static void Actor00400_Fn07DE0(Task* arg0);
static void Actor00400_Fn07E20(Task* arg0);
static void Actor00400_Fn07E74(Task* arg0);
static void Actor00400_Fn07EE8(Task* arg0);
static void Actor00400_Fn07F18(Task* arg0);
static void Actor00400_Fn07F44(Task* arg0);
static void Actor00400_Fn07F88(Task* arg0);
static void Actor00400_Fn07FEC(Task* arg0);
static void Actor00400_Fn08C54(Task* arg0);
static void Actor00400_Fn08D70(Task* arg0);
static void Actor00400_Fn08DFC(Task* arg0);
static void Actor00400_Fn08E50(Task* arg0);
static void Actor00400_Fn08FB0(Task* arg0);
static void Actor00400_Fn08FC8(Task* arg0);
static void Actor00400_Fn08FF4(Task* arg0);
static void Actor00400_Fn09038(Task* arg0);
static void Actor00400_Fn0909C(Task* arg0);
static void Actor00400_Fn090B4(Task* arg0);
static void Actor00400_Fn09124(Task* arg0);
static void Actor00400_Fn091F8(Task* arg0);
static void Actor00400_Fn09260(Task* arg0);
static void Actor00400_Fn092D4(Task* arg0);
static void Actor00400_Fn09348(Task* arg0);
static void Actor00400_Fn093BC(Task* task);
static void Actor00400_Fn093C4(Task* arg0);
static void Actor00400_Fn09418(Task* arg0);
static void Actor00400_Fn0946C(Task* arg0);
static void Actor00400_Fn094C0(Task* arg0);
static void Actor00400_Fn094DC(Task* arg0);
static void Actor00400_Fn095D8(Task* arg0);
static void Actor00400_Fn096C0(Task* arg0);
static void Actor00400_Fn09A1C(Task* arg0);
static void Actor00400_Fn09A48(Task* arg0);
static void Actor00400_Fn09A8C(Task* arg0);
static void Actor00400_Fn09AE0(Task* arg0);
static void Actor00400_Fn09B44(Task* arg0);
static void Actor00400_Fn09B74(Task* arg0);
static void Actor00400_Fn09BDC(Task* arg0);
static void Actor00400_Fn09C04(Task* arg0);
static void Actor00400_Fn09C84(Task* arg0);
static void Actor00400_Fn09CCC(Task* arg0);
static void Actor00400_Fn09D3C(Task* arg0);
static void Actor00400_Fn09D98(Task* arg0);
static void Actor00400_Fn09E70(Task* arg0);
static void Actor00400_Fn09F18(Task* arg0);
static void Actor00400_Fn09FDC(Task* arg0);

extern EnemyParams Actor00400_D0FDC8;
/// Pair table `Actor00400_Fn0A190` packs, at index 1, into the `key` of the
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
void                Actor00400_Fn076E8(Task*);
void                Actor00400_Fn08004(Task*);
void                Actor00400_Fn0805C(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
void                Actor00400_Fn08354(Task*, s32, s32, s32);
void                Actor00400_Fn08948(Task*);

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
    { D_shelter_b2_septic_tank_801836A4, D_shelter_b2_septic_tank_801836B4, &D_shelter_b2_septic_tank_801832BC, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_shelter_b4_upper_sewer_801866F8, D_shelter_b4_upper_sewer_80186708, &D_shelter_b4_upper_sewer_80186438, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_neo_ark_bridge_801820BC, D_neo_ark_bridge_801820CC, &D_neo_ark_bridge_80181FF8, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 0 },
    { D_neo_ark_submarine_gallery_80181B0C, D_neo_ark_submarine_gallery_80181B1C, &D_neo_ark_submarine_gallery_80181A48, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { D_neo_ark_submarine_tunnel_80181F94, NULL, &D_neo_ark_submarine_tunnel_80181E90, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 0 },
    { D_neo_ark_pavilion_80183A64, D_neo_ark_pavilion_80183A74, &D_neo_ark_pavilion_801839A0, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 0 },
    { D_neo_ark_island_80181CE8, D_neo_ark_island_80181CF8, &D_neo_ark_island_80181C24, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 0 },
    { D_shelter_b2_main_corridor_80182EEC, D_shelter_b2_main_corridor_80182EFC, &D_shelter_b2_main_corridor_80182E28, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 0 },
    { D_shelter_b4_lower_sewer_8018210C, D_shelter_b4_lower_sewer_8018211C, &D_shelter_b4_lower_sewer_80181E6C, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, ACTOR_00400_AREA_CONFIG_GRID_COLLISION },
    { NULL, NULL, NULL, ACTOR_00400_AREA_CONFIG_END, 0, 0 },
};

TaskMessageEntry Actor00400_D16010[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor00400_Fn0805C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor00400_Fn08354 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor00400_D16028[3] = {
    { { { TASK_BODY_TMD, 96 } }, Actor00400_Fn08948, { .model = &_gActor00400DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor00400_Fn08004, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, Actor00400_Fn076E8, { .value = 0 } },
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

static void Actor00400_Fn0A468(Task* arg0);

static void Actor00400_Fn0A4BC(Task* arg0);

static void Actor00400_Fn0A2F4(Task* arg0);

static void Actor00400_Fn0A364(Task* arg0);

static void Actor00400_Fn0962C(Task* arg0);

static void Actor00400_Fn058C4(Task* arg0);

static void            Actor00400_Fn00A14(Task* arg0);
static void            Actor00400_Fn00B48(Task* arg0);
static void            Actor00400_Fn00C84(Task* arg0);
static void            Actor00400_Fn012B0(Task* arg0, s16 arg1, s32 arg2);
static void            Actor00400_Fn01454(Task* arg0);
static void            Actor00400_Fn016A4(Task* arg0, s32 arg1);
static void            Actor00400_Fn01B90(Task* arg0);
static void            Actor00400_Fn02D48(Task* arg0);
static void            Actor00400_Fn031A4(Task* arg0, SVECTOR* arg1);
static void            Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade);
static __inline__ s32  Actor00400_ApplyAreaConfig(Task* arg0);
static __inline__ void Actor00400_AttachHead(Task* arg0, Enemy* obj,
                                             _Actor00400Work* work, s32 hide);
static __inline__ void Actor00400_UpdateColor(Task* arg0, GfxCoord* coord,
                                              _Actor00400Work* work, TmdObject* ctx);
static inline void     Actor00400_TurnToward(Task* arg0, SVECTOR* target, s32 step, s32 range);
static inline void     Actor00400_SpawnRing(Task* arg0, _Actor00400Work* work, GfxCoord* coord);
static void            Actor00400_Fn05320(Task* arg0);
static void            Actor00400_Fn05D00(Task* arg0);
static void            Actor00400_Fn05EA4(Task* arg0);
static void            Actor00400_Fn061E8(Task* arg0);
static void            Actor00400_Fn06380(Task* arg0);
static inline void     Actor00400_SpawnMarker(Task* arg0);
static void            Actor00400_Fn064B0(Task* arg0);
static void            Actor00400_Fn06798(Task* arg0);
static void            Actor00400_Fn06A44(Task* arg0);
static inline s32      Actor00400_ConsumeStateRequest(_Actor00400Work* work);
static void            Actor00400_Fn07400(Task* arg0);
static void            Actor00400_Fn07518(Task* arg0);
static inline s32      Actor00400_TakeStateRequest(Task* arg0);

#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_impact_burst.inc.c"

#include "../../shared/diver_draw_spark.inc.c"

static void Actor00400_Fn00A14(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->attackFrames != 0) {
        if (!(work->attackFrames & 7)) {
            s32 id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000B;
            s32 pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        if (work->attackFrames == 0x18 || work->attackFrames == 0x30) {
            func_800FDB18(7, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
        }
        if (work->attackFrames == 0x16 && work->inWater == 0) {
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        }
        if (--work->attackFrames == 0) {
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
}

static void Actor00400_Fn00B48(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;
    Enemy*           obj;
    GfxCoord*        coord;
    GfxCoord*        coords;
    u16              hp;
    u8               slot;

    ctx                        = arg0->extra.tmd;
    work                       = arg0->work;
    obj                        = arg0->spawnArg2.pointer;
    ctx->lightMtx              = &work->lightMtx;
    ctx->flags                 = 0;
    ctx->colorMtx              = &work->colorMtx;
    coords                     = arg0->extra.tmd->coords;
    coord                      = ctx->coords;
    work->effectArg.spawnArgLo = 0x600;
    work->effectArg.spawnArgHi = 3;
    work->targetPart           = 4;
    work->effectArg.coord      = &coords[1];
    obj->field_4               = &coord->coord;
    obj->field_48              = 0;
    obj->bodyPos.vx            = 0;
    obj->bodyPos.vy            = 0;
    obj->bodyPos.vz            = 0;
    slot                       = work->targetPart;
    obj->coord                 = &arg0->extra.tmd->coords[slot];
    Gp_LinkNode(&obj->node);
    obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->recs                   = work->hitContacts;
    obj->param                  = &Actor00400_D0FDC8;
    hp                          = Actor00400_D0FDC8.hpMax;
    obj->hpMax                  = hp;
    obj->hp                     = hp;
    coord->parent               = &gGfxViewCoord;
    animationInitContext(&work->rig.anim, Actor00400_D1604C, ctx, work->rig.poses, work->rig.slots);
    Actor00400_Fn019B4(arg0);
    work->rotation.vy = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

static void Actor00400_Fn00C84(Task* arg0)
{
    Task*            actor;
    Task*            player;
    _Actor00400Work* work;
    GfxCoord*        coord;
    GfxCoord*        pc;
    SVECTOR          pos;
    u8               frame;

    actor  = arg0;
    player = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work   = actor->work;
    coord  = actor->extra.tmd->coords;
    if (work->animClip != 4) {
        Actor00400_Fn088EC(actor, 4, 0x20, 0xA);
        Actor00400_Fn08814(actor);
    }
    frame = Actor00400_Fn086FC(actor, 0x39);
    if (Actor00400_Fn08908(actor)) {
        work->animFrames = 0;
    }
    if (work->animFrames == 0) {
        Actor00400_Fn0824C(actor, 0xB, 0xE, &work->armAnchor);
        if (work->strideCount & 1) {
            s32 id  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040002;
            s32 pan = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        } else {
            s32 id  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040003;
            s32 pan = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        }
        work->strideCount++;
    }
    if (work->animFrames >= 0 && frame >= work->animFrames) {
        pc     = player->extra.tmd->coords;
        pos.vx = pc->coord.t[0];
        pos.vy = pc->coord.t[1];
        pos.vz = pc->coord.t[2];
        Actor00400_Fn0875C(actor, &pos, 8, 0x100);
        Actor00400_Fn08464(actor, 0xB, 0xE, &work->armAnchor);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/limb_shadows_segment.inc.c"

static void Actor00400_Fn012B0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    limbShadowDrawSegment(arg0, 1, 2, 0x258, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 2, 3, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 3, 4, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 4, 5, 0x1F4, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 6, 0x320, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 6, 7, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 7, 8, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 0xC, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xC, 0xD, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xD, 0xE, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 9, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 9, 0xA, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xA, 0xB, 0x12C, arg1, temp_s2);
}

/* Tracks the nearer of the two party members and stores the result in the
   actor's work block.

   `prevRootPos` snapshots the actor's own root translation. The
   second coordinate of the model (`coord[1]`) is taken into view space and
   each slot's root translation measured against it; the closer of the two
   lands in `targetPos` with its XZ distance in `targetDistance`. The chosen
   offset is then normalised and turned into a yaw relative to the actor's
   own heading (`rotation.vy`) in `targetBearing`. */
static void Actor00400_Fn01454(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    GfxCoord*        c0;
    GfxCoord*        c1;
    Task*            player;
    GfxCoord*        joint;
    SVECTOR          delta0;
    SVECTOR          delta1;
    SVECTOR          view;
    s32              dist0;
    s32              dist1;

    work                 = arg0->work;
    coord                = arg0->extra.tmd->coords;
    player               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    joint                = &coord[1];
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    if (player != NULL) {
        c0      = player->extra.tmd->coords;
        view.vx = 0;
        view.vy = 0;
        view.vz = 0;
        coordLocalToWorld(joint, &view);
        delta0.vx = c0->coord.t[0] - view.vx;
        delta0.vy = c0->coord.t[1] - view.vy;
        delta0.vz = c0->coord.t[2] - view.vz;
        dist0     = SquareRoot0(delta0.vx * delta0.vx + delta0.vz * delta0.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->targetPos.vx   = c0->coord.t[0];
            work->targetPos.vy   = c0->coord.t[1];
            work->targetPos.vz   = c0->coord.t[2];
            work->targetDistance = dist0;
        } else {
            c1        = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            delta1.vx = c1->coord.t[0] - view.vx;
            delta1.vy = c1->coord.t[1] - view.vy;
            delta1.vz = c1->coord.t[2] - view.vz;
            dist1     = SquareRoot0(delta1.vx * delta1.vx + delta1.vz * delta1.vz);
            if (dist1 < dist0) {
                work->targetPos.vx = c1->coord.t[0];
                work->targetPos.vy = c1->coord.t[1];
                work->targetPos.vz = c1->coord.t[2];
                delta0             = delta1;
                dist0              = dist1;
            } else {
                work->targetPos.vx = c0->coord.t[0];
                work->targetPos.vy = c0->coord.t[1];
                work->targetPos.vz = c0->coord.t[2];
            }
            work->targetDistance = dist0;
        }
        VectorNormalSS(&delta0, &delta0);
        work->targetBearing = (ratan2(delta0.vx, delta0.vz) - work->rotation.vy) & 0xFFF;
    }
}

/* Re-aims the two upper body coordinates at the target yaw held in
   `lookYaw` and folds the result back into the model root.

   With `arg1 == 0` the yaw chases the heading `Actor00400_Fn0814C` reports:
   it steps 0x18 per frame while the error is more than 0x20, and decays
   toward zero once the heading leaves +/-0x5FF. A non-zero `arg1` only
   decays, twice as fast. Each of the two coordinates then gets its pitch
   re-applied about X and a third of the yaw about Y, and the composed
   inverse of all three lands in `c4`. */
static void Actor00400_Fn016A4(Task* arg0, s32 arg1)
{
    SVECTOR           euler;
    SVECTOR           rot1;
    SVECTOR           rot2;
    MATRIX            t1;
    MATRIX            t2;
    MATRIX            t3;
    GfxMatrix         ma;
    GfxMatrix         mb;
    GfxMatrix         mc;
    GfxRotationWords* ia;
    GfxRotationWords* ib;
    GfxRotationWords* ic;
    GfxCoord*         base;
    GfxCoord*         c1;
    GfxCoord*         c2;
    GfxCoord*         c3;
    GfxCoord*         c4;
    _Actor00400Work*  work;
    MATRIX*           m2;
    MATRIX*           m3;

    base = arg0->extra.tmd->coords;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    work = arg0->work;
    Actor00400_Fn0814C(arg0, 4, &euler, 0x600);
    if ((arg1 & 0xFF) == 0) {
        if ((u16)(euler.vy + 0x5FF) < 0xBFF) {
            if ((u32)((euler.vy - (s16)work->lookYaw) + 0x20) >= 0x41) {
                if ((s16)work->lookYaw < euler.vy) {
                    work->lookYaw = work->lookYaw + 0x18;
                } else {
                    work->lookYaw = work->lookYaw - 0x18;
                }
            }
        } else {
            work->lookYaw = work->lookYaw + ((s32) - (s16)(work->lookYaw * 0x10) >> 8);
        }
    } else {
        work->lookYaw = work->lookYaw + ((s32) - (s16)(work->lookYaw * 0x10) >> 7);
    }

    m2 = &c2->coord;
    ia = &ma.rotationWords;
    ib = &mb.rotationWords;
    ic = &mc.rotationWords;

    ma.rotationWords.m00M01 = ONE;
    ma.rotationWords.m02M10 = 0;
    ia->m11M12              = ONE;
    ma.rotationWords.m20M21 = 0;
    ia->m22                 = ONE;
    mb.rotationWords.m00M01 = ONE;
    mb.rotationWords.m02M10 = 0;
    ib->m11M12              = ONE;
    mb.rotationWords.m20M21 = 0;
    ib->m22                 = ONE;
    mc.rotationWords.m00M01 = ONE;
    mc.rotationWords.m02M10 = 0;
    ic->m11M12              = ONE;
    mc.rotationWords.m20M21 = 0;
    ic->m22                 = ONE;

    Gp_MtxToEuler(m2, &rot1);
    m3 = &c3->coord;
    Gp_MtxToEuler(m3, &rot2);
    RotMatrixX(rot1.vx, &ma.mat);
    RotMatrixX(rot2.vx, &mb.mat);
    Actor00400_Fn08A1C(&ma.mat, m2);
    Actor00400_Fn08A1C(&mb.mat, m3);
    actorRenderComposeCoord(c1);
    actorRenderComposeCoord(c2);
    actorRenderComposeCoord(c3);
    diverTurnJoint(c2, (s16)work->lookYaw / 3);
    diverTurnJoint(c3, (s16)work->lookYaw / 3);

    mc.rotationWords.m00M01 = ONE;
    mc.rotationWords.m02M10 = 0;
    ic->m11M12              = ONE;
    mc.rotationWords.m20M21 = 0;
    ic->m22                 = ONE;

    RotMatrixY((s16)work->lookYaw / 3, &mc.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(m2, &t2);
    TransposeMatrix(m3, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.mat);
    Actor00400_Fn08A1C(&t1, &c4->coord);
}

/* Links the actor's four collision objects and clears their record tables;
   `gridBody` participates in grid tests when `gridCollision` is nonzero. */
static void Actor00400_Fn019B4(Task* arg0)
{
    _Actor00400Work* work = arg0->work;

    work->trunkBody.coord            = &arg0->extra.tmd->coords[1];
    work->trunkBody.context.contacts = work->hitContacts;
    work->trunkBody.pos.vx           = 0;
    work->trunkBody.pos.vy           = 0;
    work->trunkBody.pos.vz           = 0;
    work->trunkBody.key              = 0x30004;
    work->trunkBody.radius           = 0x300;
    work->trunkBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->trunkBody);
    Gp_InitRec18Table(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->trunkBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->headBody.coord            = &arg0->extra.tmd->coords[4];
    work->headBody.context.contacts = work->hitContacts;
    work->headBody.pos.vx           = 0;
    work->headBody.pos.vy           = 0;
    work->headBody.pos.vz           = 0;
    work->headBody.key              = 0x30004;
    work->headBody.radius           = 0xC0;
    work->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->headBody);
    work->headBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->attackBody.coord            = &arg0->extra.tmd->coords[1];
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->attackBody.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->attackBody.radius           = 0x480;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
    Gp_InitRec18Table(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->gridBody.coord            = arg0->extra.tmd->coords;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30004;
    work->gridBody.radius           = 0x380;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->gridBody);
    Gp_InitRec18Table(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    if (work->gridCollision != 0) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else {
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

/* Damage / knock-back tick: walks the six contact records, applies the hit
   the first one carries, then folds the accumulated push-back into the work
   position and the actor's coordinate. */
static void Actor00400_Fn01B90(Task* arg0)
{
    _Actor00400Work*    work;
    Enemy*              obj;
    GfxCoord*           coord;
    WorldCollisionDelta delta;
    s32                 kind;
    s16                 amount;
    s32                 dmg;
    s32                 tmp;
    s32                 tick;
    s32                 i;

    kind           = 0;
    coord          = arg0->extra.tmd->coords;
    work           = arg0->work;
    obj            = arg0->spawnArg2.pointer;
    work->hitTaken = 0;
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        if ((work->hitContacts[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = 1;
                work->wasHit      = 1;
                dmg               = Gp_ComputeDamage(work->hitContacts[i].key.value, work->targetDistance, 0, 0);
                amount            = dmg;
                work->hitCooldown = Gp_GetIdParam2(work->hitContacts[i].key.value);
                if (Gp_RollEnemyChance(obj, work->hitContacts[i].key.value, work->critChanceScale) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->hitContacts[i].key.value) & 0xFFFF,
                              &arg0->extra.tmd->coords[work->targetPart], 0, &work->effectArg);
                work->hitReaction = (amount < 0x3C) ? ACTOR_00400_HIT_REACTION_FLINCH : ACTOR_00400_HIT_REACTION_HEAVY;
                switch (Gp_GetIdParam0(work->hitContacts[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(obj);
                        break;
                    case 2:
                        Gp_SetObjFlag2(obj, work->hitContacts[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(obj, work->hitContacts[i].key.value, 0);
                        break;
                    case 4:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_BLAST;
                        break;
                    case 5:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_HEAVY;
                        break;
                    case 6:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_BLAST;
                        break;
                    case 7:
                        kind              = 2;
                        work->hitReaction = ACTOR_00400_HIT_REACTION_HEAVY;
                        amount           += amount;
                        break;
                    case 8:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
                        work->hitTaken    = 0;
                        break;
                    case 9:
                        work->hitReaction = ACTOR_00400_HIT_REACTION_LIGHT;
                        break;
                }
                if ((work->hitContacts[i].key.value & 0x7F) == 0x1C && (work->hitContacts[i].key.value & 0x8000) == 0) {
                    obj->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->hitReaction   = ACTOR_00400_HIT_REACTION_FLINCH;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[work->targetPart], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[work->targetPart], 2, 0);
                        break;
                }
                func_800E2C78(obj, work->hitContacts[i].key.value, amount, 0);
                func_800DA6E8(&obj->node, amount, 0);
                obj->hp -= amount;
                if ((s16)obj->hp < 0) {
                    obj->hp = 0;
                }
            } else if ((Gp_GetIdParam1(work->hitContacts[i].key.value) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], 0, &work->effectArg);
            }
        }
        if (work->hitTaken != 0) {
            break;
        }
    }

    if (obj->reactionFlags & ENEMY_REACTION_STAGGER) {
        obj->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction   = ACTOR_00400_HIT_REACTION_HEAVY;
    }
    if (obj->reactionFlags & ENEMY_REACTION_BUILDUP) {
        obj->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction   = ACTOR_00400_HIT_REACTION_STATUS;
    }
    if (obj->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tmp  = Gp_TickObjFlag4(obj);
        tick = (s16)tmp;
        if (tick != 0) {
            obj->hp -= tmp;
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            func_800DA6E8(&obj->node, tick, 0);
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        }
        if (Gp_ObjFlag4Expired(obj) != 0) {
            obj->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    switch (func_800E0C10(work->gridContacts, &delta, ARRAY_SIZE(work->gridContacts), 0)) {
        case 0:
            break;
        case 1:
            tmp                 = delta.fixed.vx.halves.integer;
            work->armAnchor.vx += tmp;
            tmp                 = delta.fixed.vz.halves.integer;
            work->armAnchor.vz += tmp;
            if ((delta.fixed.vx.word & 0xFFFF) != 0) {
                if (delta.fixed.vx.word > 0) {
                    work->armAnchor.vx++;
                } else {
                    work->armAnchor.vx--;
                }
            }
            if ((delta.fixed.vz.word & 0xFFFF) != 0) {
                if (delta.fixed.vz.word > 0) {
                    work->armAnchor.vz++;
                } else {
                    work->armAnchor.vz--;
                }
            }
            tmp                = delta.fixed.vx.halves.integer;
            coord->coord.t[0] += tmp;
            tmp                = delta.fixed.vz.halves.integer;
            coord->coord.t[2] += tmp;
            if ((delta.fixed.vx.word & 0xFFFF) != 0) {
                if (delta.fixed.vx.word > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            if ((delta.fixed.vz.word & 0xFFFF) != 0) {
                if (delta.fixed.vz.word > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }

    Gp_ClearRec18Occupied(work->hitContacts);
    Gp_ClearRec18Occupied(work->gridContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
}

static s16 Actor00400_Fn02154(Task* arg0)
{
    _Actor00400Work* work;
    s16              state;
    s16              req;

    work = arg0->work;
    if (work->hitTaken != 1) {
        goto fail;
    }
    req = work->hitReaction;
    if (req == ACTOR_00400_HIT_REACTION_LIGHT) {
        state = ACTOR_00400_STATE_RECOIL_LIGHT;
    } else if (req == ACTOR_00400_HIT_REACTION_HEAVY) {
        state = ACTOR_00400_STATE_RECOIL_HEAVY;
    } else if (req == ACTOR_00400_HIT_REACTION_STATUS) {
        state = ACTOR_00400_STATE_STATUS_HOLD;
    } else if (req == ACTOR_00400_HIT_REACTION_BLAST) {
        state = ACTOR_00400_STATE_RECOIL_HEAVY;
    } else {
        goto other;
    }
    work->state       = state;
    work->subState    = 0;
    work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
    goto ok;
other:
    if (req == ACTOR_00400_HIT_REACTION_FLINCH) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
        work->shakeFrames = 10;
        work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
        return 0;
    }
    work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
    goto fail;
ok:
    return 1;
fail:
    return 0;
}

static s32 Actor00400_Fn02208(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = (u16)work->waypoints[work->waypointIndex].vx - coord->coord.t[0];
    vec.vy = (u16)work->waypoints[work->waypointIndex].vy - coord->coord.t[1];
    vec.vz = (u16)work->waypoints[work->waypointIndex].vz - coord->coord.t[2];
    if (work->animClip != 3) {
        Actor00400_Fn088EC(arg0, 3, ANIMATION_RATE_ONE, 0xE);
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    work->goalY = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 400) {
        work->waypointIndex = (work->waypointIndex + 1) & 7;
        return 1;
    } else {
        Actor00400_Fn0875C(arg0, &work->waypoints[work->waypointIndex], 0x2C, 0x100);
        diverStepForward(arg0, 0x60, work->rotation.vy);
        return 0;
    }
}

/* The random pick spawns in both arms rather than after the `if`: jump2
   cross-jumps the identical tails, which is what leaves the 0x20010 argument
   load ahead of the `D_800678F0` store in each arm. */
static void Actor00400_Fn0237C(Task* arg0)
{
    EffectWork* eff1;
    TmdObject*  src1;
    TmdObject*  dst1;
    EffectWork* eff2;
    TmdObject*  src2;
    TmdObject*  dst2;
    EffectWork* eff3;
    TmdObject*  src3;
    TmdObject*  dst3;
    EffectWork* eff4;
    TmdObject*  src4;
    TmdObject*  dst4;
    EffectWork* eff5;
    TmdObject*  src5;
    TmdObject*  dst5;

    D_800678F0[0] = &_gActor00400DiverBurstHead;
    eff1          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[4], 0x200, NULL);
    if (eff1 != NULL) {
        src1                    = arg0->extra.tmd;
        dst1                    = eff1->task->extra.tmd;
        dst1->texturePageOffset = src1->texturePageOffset;
        dst1->clutRowOffset     = src1->clutRowOffset;
        if (dst1->buffer != NULL) {
            tmdProcessStream(dst1);
            tmdProcessStream(dst1);
        }
    }
    D_800678F0[0] = &_gActor00400DiverBurstArmRight;
    eff2          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[11], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &_gActor00400DiverBurstArmLeft1;
    eff3          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[14], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = arg0->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_800678F0[0] = &_gActor00400DiverBurstLegRight;
        eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = &_gActor00400DiverBurstArmLeft2;
        eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[8], 0x200, NULL);
    }
    if (eff4 != NULL) {
        src4                    = arg0->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    D_800678F0[0] = &_gActor00400DiverEnergyBall;
    eff5          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff5 != NULL) {
        src5                    = arg0->extra.tmd;
        dst5                    = eff5->task->extra.tmd;
        dst5->texturePageOffset = src5->texturePageOffset;
        dst5->clutRowOffset     = src5->clutRowOffset;
        if (dst5->buffer != NULL) {
            tmdProcessStream(dst5);
            tmdProcessStream(dst5);
        }
    }
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
}

/// Drives the two head/neck coordinates (`base[2]`, `base[3]`) and the aim
/// coordinate (`base[4]`) from `neckRetracted`.
///
/// While it is set, `neckPhase` walks a small state machine: state 0 snaps the
/// five part coordinates to their bind pose and records the current Euler
/// angles, state 1 eases those angles back towards zero (and promotes to
/// state 2 once all six components are inside 0x30), and state 2 scales the
/// parts by `neckScale` while the aim coordinate keeps its own rotation.
/// When the flag drops, `neckScale` is eased back to 0x1000 with the same
/// scaling pass until it passes 0xF80, after which the stored angles are
/// blended halfway towards the live ones and the state resets to 0.
///
/// `invScale` is one function-scope variable rather than a local per arm on
/// purpose: with two assignments the pseudo has two deaths, so local-alloc
/// skips it and never ties the `divmodsi4` result to its dividend. That is
/// what leaves the quotient in the divisor's register (`mflo $v1`).
static void Actor00400_Fn02648(Task* arg0, s32 arg1)
{
    VECTOR           scale;
    GfxMatrix        rot;
    SVECTOR          euler0;
    GfxMatrix        ma;
    SVECTOR          euler1;
    GfxMatrix        mb;
    SVECTOR          euler2;
    GfxMatrix        mc;
    _Actor00400Work* work;
    GfxCoord*        base;
    GfxCoord*        c2;
    GfxCoord*        c3;
    GfxCoord*        c4;
    s32              invScale;

    base = arg0->extra.tmd->coords;
    work = arg0->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    if (work->neckRetracted != 0) {
        switch (work->neckPhase) {
            case ACTOR_00400_NECK_FREE:
                base[0].composeStamp = GRAPHICS_COORD_DIRTY;
                base[1].composeStamp = GRAPHICS_COORD_DIRTY;
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(c4);
                Gp_MtxToEuler(&base[2].coord, &work->lowerNeckAngles);
                Gp_MtxToEuler(&base[3].coord, &work->upperNeckAngles);
                work->neckPhase = ACTOR_00400_NECK_STRAIGHTEN;
                work->neckScale = ONE;
                /* fallthrough */
            case ACTOR_00400_NECK_STRAIGHTEN: {
                GfxRotationWords* ir;

                ir                       = &rot.rotationWords;
                work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((s32) - (work->lowerNeckAngles.vx * 0x10) >> 6);
                work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((s32) - (work->lowerNeckAngles.vy * 0x10) >> 6);
                work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((s32) - (work->lowerNeckAngles.vz * 0x10) >> 6);
                work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((s32) - (work->upperNeckAngles.vx * 0x10) >> 6);
                work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((s32) - (work->upperNeckAngles.vy * 0x10) >> 6);
                work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((s32) - (work->upperNeckAngles.vz * 0x10) >> 6);
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                ir->m11M12               = ONE;
                rot.rotationWords.m20M21 = 0;
                ir->m22                  = ONE;
                RotMatrix(&work->lowerNeckAngles, &rot.mat);
                Actor00400_Fn08A1C(&rot.mat, &c2->coord);
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                ir->m11M12               = ONE;
                rot.rotationWords.m20M21 = 0;
                ir->m22                  = ONE;
                RotMatrix(&work->upperNeckAngles, &rot.mat);
                Actor00400_Fn08A1C(&rot.mat, &c3->coord);
                if ((abs(work->lowerNeckAngles.vx) < 0x30) && (abs(work->lowerNeckAngles.vy) < 0x30) && (abs(work->lowerNeckAngles.vz) < 0x30) &&
                    (abs(work->upperNeckAngles.vx) < 0x30) && (abs(work->upperNeckAngles.vy) < 0x30) && (abs(work->upperNeckAngles.vz) < 0x30)) {
                    work->neckPhase = ACTOR_00400_NECK_RETRACTED;
                }
                c2->composeStamp = GRAPHICS_COORD_DIRTY;
                c3->composeStamp = GRAPHICS_COORD_DIRTY;
                c4->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(c4);
                break;
            }
            case ACTOR_00400_NECK_RETRACTED: {
                GfxRotationWords* ia;
                GfxRotationWords* ib;
                GfxRotationWords* ic;
                GfxRotationWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler2);
                work->neckScale         = (u16)work->neckScale + ((0x2AA - work->neckScale) >> 3);
                ia                      = &ma.rotationWords;
                ma.rotationWords.m00M01 = ONE;
                ma.rotationWords.m02M10 = 0;
                ia->m11M12              = ONE;
                ma.rotationWords.m20M21 = 0;
                ia->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                scale.vz                = work->neckScale;
                ScaleMatrix(&ma.mat, &scale);
                Actor00400_Fn08A1C(&ma.mat, &base[2].coord);
                ib                      = &mb.rotationWords;
                mb.rotationWords.m00M01 = ONE;
                mb.rotationWords.m02M10 = 0;
                ib->m11M12              = ONE;
                mb.rotationWords.m20M21 = 0;
                ib->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                scale.vz                = 0x1000;
                ScaleMatrix(&mb.mat, &scale);
                Actor00400_Fn08A1C(&mb.mat, &base[3].coord);
                ic                      = &mc.rotationWords;
                mc.rotationWords.m00M01 = ONE;
                mc.rotationWords.m02M10 = 0;
                ic->m11M12              = ONE;
                mc.rotationWords.m20M21 = 0;
                ic->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                invScale                = 0x1000000 / work->neckScale;
                scale.vz                = invScale;
                ScaleMatrix(&mc.mat, &scale);
                ir                       = &rot.rotationWords;
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                ir->m11M12               = ONE;
                rot.rotationWords.m20M21 = 0;
                ir->m22                  = ONE;
                RotMatrix(&euler2, &rot.mat);
                MulMatrix(&mc.mat, &rot.mat);
                Actor00400_Fn08A1C(&mc.mat, &c4->coord);
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(c4);
                break;
            }
        }
    } else {
        base[0].composeStamp = GRAPHICS_COORD_DIRTY;
        base[1].composeStamp = GRAPHICS_COORD_DIRTY;
        base[2].composeStamp = GRAPHICS_COORD_DIRTY;
        base[3].composeStamp = GRAPHICS_COORD_DIRTY;
        base[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(c4);
        if (work->neckScale < 0xF80) {
            GfxRotationWords* ia;
            GfxRotationWords* ib;
            GfxRotationWords* ic;
            GfxRotationWords* ir;

            Gp_MtxToEuler(&c4->coord, &euler2);
            work->neckScale         = (u16)work->neckScale + ((0x1000 - work->neckScale) >> 3);
            ia                      = &ma.rotationWords;
            ma.rotationWords.m00M01 = ONE;
            ma.rotationWords.m02M10 = 0;
            ia->m11M12              = ONE;
            ma.rotationWords.m20M21 = 0;
            ia->m22                 = ONE;
            scale.vx                = 0x1000;
            scale.vy                = 0x1000;
            scale.vz                = work->neckScale;
            ScaleMatrix(&ma.mat, &scale);
            Actor00400_Fn08A1C(&ma.mat, &base[2].coord);
            ib                      = &mb.rotationWords;
            mb.rotationWords.m00M01 = ONE;
            mb.rotationWords.m02M10 = 0;
            ib->m11M12              = ONE;
            mb.rotationWords.m20M21 = 0;
            ib->m22                 = ONE;
            scale.vx                = 0x1000;
            scale.vy                = 0x1000;
            scale.vz                = 0x1000;
            ScaleMatrix(&mb.mat, &scale);
            Actor00400_Fn08A1C(&mb.mat, &base[3].coord);
            ic                      = &mc.rotationWords;
            mc.rotationWords.m00M01 = ONE;
            mc.rotationWords.m02M10 = 0;
            ic->m11M12              = ONE;
            mc.rotationWords.m20M21 = 0;
            ic->m22                 = ONE;
            scale.vx                = 0x1000;
            scale.vy                = 0x1000;
            invScale                = 0x1000000 / work->neckScale;
            scale.vz                = invScale;
            ScaleMatrix(&mc.mat, &scale);
            ir                       = &rot.rotationWords;
            rot.rotationWords.m00M01 = ONE;
            rot.rotationWords.m02M10 = 0;
            ir->m11M12               = ONE;
            rot.rotationWords.m20M21 = 0;
            ir->m22                  = ONE;
            RotMatrix(&euler2, &rot.mat);
            MulMatrix(&mc.mat, &rot.mat);
            Actor00400_Fn08A1C(&mc.mat, &c4->coord);
        } else {
            GfxRotationWords* ir;
            MATRIX*           m2;
            MATRIX*           m3;

            m2 = &base[2].coord;
            Gp_MtxToEuler(m2, &euler0);
            m3 = &base[3].coord;
            Gp_MtxToEuler(m3, &euler1);
            work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((euler0.vx - work->lowerNeckAngles.vx) >> 1);
            work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((euler0.vy - work->lowerNeckAngles.vy) >> 1);
            work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((euler0.vz - work->lowerNeckAngles.vz) >> 1);
            work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((euler1.vx - work->upperNeckAngles.vx) >> 1);
            work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((euler1.vy - work->upperNeckAngles.vy) >> 1);
            work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((euler1.vz - work->upperNeckAngles.vz) >> 1);
            ir                       = &rot.rotationWords;
            rot.rotationWords.m00M01 = ONE;
            rot.rotationWords.m02M10 = 0;
            ir->m11M12               = ONE;
            rot.rotationWords.m20M21 = 0;
            ir->m22                  = ONE;
            RotMatrix(&work->lowerNeckAngles, &rot.mat);
            Actor00400_Fn08A1C(&rot.mat, m2);
            rot.rotationWords.m00M01 = ONE;
            rot.rotationWords.m02M10 = 0;
            ir->m11M12               = ONE;
            rot.rotationWords.m20M21 = 0;
            ir->m22                  = ONE;
            RotMatrix(&work->upperNeckAngles, &rot.mat);
            Actor00400_Fn08A1C(&rot.mat, m3);
        }
        base[2].composeStamp = GRAPHICS_COORD_DIRTY;
        base[3].composeStamp = GRAPHICS_COORD_DIRTY;
        base[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(c4);
        work->neckPhase = ACTOR_00400_NECK_FREE;
    }
}

/// Flight state of the shot `Actor00400_SpawnMarker` starts: each frame it
/// adds `ACTOR_00400_SHOT_GRAVITY` to the shot's vertical speed, moves the
/// coordinate by `_Actor00400ShotWork::velocity`, then decides whether the
/// shot bursts.
///
/// `hidden` is raised when either of the shot's two contacts reports one of
/// the three kinds 1/3/5, or when `func_800E0C10` finds the sphere against the
/// room's grid and the current stage and area are not among the exceptions.
/// Once it is raised - or at `ACTOR_00400_SHOT_LIFETIME` frames, or when
/// `gSceneCombatState.actor00400HideRequested` is set - the sphere's grid and pair tests are
/// switched off, the task's state is bumped and the burst is spawned with kind
/// 2 instead of 1.
///
/// `composeStamp` is cleared through a scalar lvalue on purpose: written as a struct
/// member it is an in-struct MEM, and GCC 2.8.1's
/// `fixed_scalar_and_varying_struct_p` would then let the `gSceneCombatState.actorControl` load
/// hoist above the store. See DECOMPILATION_LEARNINGS.md, "Struct-typing a
/// body changes GCC 2.8.1's aliasing".
static void Actor00400_Fn02D48(Task* arg0)
{
    _Actor00400ShotWork* work;
    s32                  hidden;
    GfxCoord*            coord;
    WorldCollisionDelta  delta;
    s32                  mask;
    s32                  i;
    s32                  n;
    u16                  kind;

    hidden                = 0;
    work                  = arg0->work;
    coord                 = arg0->extra.tmd->coords;
    *&coord->composeStamp = GRAPHICS_COORD_DIRTY;
    kind                  = 1;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frames      += 1;
            work->velocity.vy += ACTOR_00400_SHOT_GRAVITY;
            coord->coord.t[0] += work->velocity.vx;
            coord->coord.t[1] += work->velocity.vy;
            coord->coord.t[2] += work->velocity.vz;
            if (Gp_FindRec18(work->contacts, 0) != 0) {
                for (i = 0; i < 2; i++) {
                    switch (work->contacts[i].key.value & 0xFFFF0000) {
                        case 0x10000:
                            hidden = 1;
                            break;
                        case 0x30000:
                            hidden = 1;
                            break;
                        case 0x50000:
                            hidden = 1;
                            break;
                    }
                }
            }
            n = func_800E0C10(work->contacts, &delta, 2, &mask);
            if (n < 3) {
                if (n > 0) {
                    if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER &&
                        (gGameSession->location.loc.area == 0x21 || gGameSession->location.loc.area == 0x2B ||
                         gGameSession->location.loc.area == 0x2C || gGameSession->location.loc.area == 0x2D ||
                         gGameSession->location.loc.area == 0x22)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK &&
                               (gGameSession->location.loc.area == 0xD || gGameSession->location.loc.area == 0xE ||
                                gGameSession->location.loc.area == 0x1B)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY && gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK) {
                        if ((mask & 8) == 0) {
                            hidden = 1;
                        }
                    } else {
                        hidden = 1;
                    }
                }
            }
            Gp_ClearRec18Occupied(work->contacts);
            if ((++arg0->killCountdown >= ACTOR_00400_SHOT_LIFETIME) || (gSceneCombatState.actor00400HideRequested != 0) || (hidden != 0)) {
                arg0->killCountdown           = 0;
                work->child.attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                kind                          = 2;
                arg0->state                  += 1;
            }
            diverImpactBurst(coord, work->frames, kind, 0x1300);
            break;
    }
}

/// Same nearest-waypoint search as `Actor00400_Fn031A4`, but the winner is
/// stored into `surfaceSpot` and then made current: the entry `surfaceSpotIndex`
/// used to point at is cleared to kind 0 and the new one is marked kind 1.
static void Actor00400_Fn02FF8(Task* arg0)
{
    _Actor00400NearestSurfaceSpotScratch* scratch;
    _Actor00400Work*                      work;
    SVECTOR*                              record;
    s16                                   index;
    s16                                   kind;
    s32                                   dx;
    s32                                   dz;
    s32                                   distance;

    scratch               = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
    work                  = arg0->work;
    scratch->spotIndex    = 1;
    scratch->nearestIndex = 0;
    scratch->bestDistance = ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE;
    for (;;) {
        index  = scratch->spotIndex;
        record = (SVECTOR*)(index * sizeof(SVECTOR) + (u32)work->surfaceSpots);
        kind   = record->pad;
        if (kind == -1) {
            goto done;
        }
        if ((kind != 1) || (index == work->surfaceSpotIndex)) {
            scratch->delta.vx = dx = work->targetPos.vx - record->vx;
            scratch->delta.vz = dz = work->targetPos.vz - work->surfaceSpots[scratch->spotIndex].vz;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->distance      = distance;
            if (distance < scratch->bestDistance) {
                work->surfaceSpot.vx  = work->surfaceSpots[scratch->spotIndex].vx;
                work->surfaceSpot.vz  = work->surfaceSpots[scratch->spotIndex].vz;
                scratch->bestDistance = scratch->distance;
                scratch->nearestIndex = scratch->spotIndex;
            }
        }
        scratch->spotIndex = scratch->spotIndex + 1;
    }
done:
    if (work->surfaceSpotIndex != scratch->nearestIndex) {
        work->surfaceSpots[work->surfaceSpotIndex].pad = 0;
        work->surfaceSpotIndex                         = scratch->nearestIndex;
        work->surfaceSpots[work->surfaceSpotIndex].pad = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
}

/// Finds the nearest eligible entry of `surfaceSpots` and returns its XZ in
/// `arg1`. Entries whose `pad` is 1 are only considered when they are the one
/// `surfaceSpotIndex` points at, and the walk ends at the `-1` terminator.
static void Actor00400_Fn031A4(Task* arg0, SVECTOR* arg1)
{
    _Actor00400NearestSurfaceSpotScratch* scratch;
    _Actor00400Work*                      work;
    SVECTOR*                              record;
    s16                                   index;
    s16                                   kind;
    s32                                   dx;
    s32                                   dz;
    s32                                   distance;

    scratch               = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
    work                  = arg0->work;
    scratch->spotIndex    = 1;
    scratch->nearestIndex = 0;
    scratch->bestDistance = ACTOR_00400_SURFACE_SPOT_DISTANCE_NONE;
loop:
    index  = scratch->spotIndex;
    record = (SVECTOR*)(index * sizeof(SVECTOR) + (u32)work->surfaceSpots);
    kind   = record->pad;
    if (kind != -1) {
        if ((kind != 1) || (index == work->surfaceSpotIndex)) {
            scratch->delta.vx = dx = work->targetPos.vx - record->vx;
            scratch->delta.vz = dz = work->targetPos.vz - work->surfaceSpots[scratch->spotIndex].vz;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->distance      = distance;
            if (distance < scratch->bestDistance) {
                arg1->vx              = work->surfaceSpots[scratch->spotIndex].vx;
                arg1->vz              = work->surfaceSpots[scratch->spotIndex].vz;
                scratch->bestDistance = scratch->distance;
                scratch->nearestIndex = scratch->spotIndex;
            }
        }
        scratch->spotIndex = scratch->spotIndex + 1;
        goto loop;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400NearestSurfaceSpotScratch);
}

/// Projects the four `corner` vertices through the view matrix and queues one
/// quad of the shadow blob texture, subtracted from the picture at strength
/// `shade` (half that on red). A quad the projection clips is not drawn.
static void Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade)
{
    _Actor00400GroundStainScratch* s;
    POLY_FT4*                      poly;

    s                          = SCRATCH_STACK_RESERVE_BLOCK(_Actor00400GroundStainScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    s->depth = RotTransPers4(corner0, corner1, corner2, corner3, &s->screenCorners[0], &s->screenCorners[1], &s->screenCorners[2], &s->screenCorners[3],
                             &s->depthCue, &s->flags);
    if (s->flags >= 0) {
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
        setRGB0(poly, shade >> 1, shade, shade);
        addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00400GroundStainScratch);
}

#include "../../shared/diver_turn_joint.inc.c"

/* The state tables below are defined among the functions, not with the other
   declarations, because `.rodata` follows source order: each sits between the
   jump tables of the functions around it. */

/// Kill-path states, indexed by `Task::state` in `Actor00400_Fn08004`.
static const TaskFuncTable3 Actor00400_D0002C = { {
    Actor00400_Fn0A190,
    Actor00400_Fn02D48,
    diverStrikeTeardown,
} };

/// The eight states `Actor00400_Fn08948` dispatches on `field_30`. The zero
/// word after it in the image is the alignment pad of
/// `Actor00400_Fn03920`'s jump table, not a terminator.
static const TaskFuncTable8 Actor00400_D00038 = { {
    Actor00400_Fn03920,
    Actor00400_Fn04580,
    Actor00400_Fn04B48,
    Actor00400_Fn04E18,
    Actor00400_Fn040DC,
    Actor00400_Fn089C8,
    Actor00400_Fn06B7C,
    Actor00400_Fn070C0,
} };

/// Applies the row of `Actor00400_D15F20` that matches the session's current
/// stage and area to the freshly allocated work block: the row can turn on the
/// grid collision, pick the patrol ring the spawn argument selects, hand the
/// actor the room's surface spots and seed its water level. Returns non-zero
/// when the actor does not belong in this room - no row matched, or the
/// matching row has `ACTOR_00400_AREA_CONFIG_NO_DIVER` - which makes the entry
/// state destroy the enemy instead.
static __inline__ s32 Actor00400_ApplyAreaConfig(Task* arg0)
{
    _Actor00400AreaConfig* cfg;
    _Actor00400Work*       work;
    GameLocationKey*       ses;
    u16                    flags;

    work = arg0->work;
    ses  = &gGameSession->location.loc;
    cfg  = Actor00400_D15F20;
    while (cfg->stage != ACTOR_00400_AREA_CONFIG_END) {
        if ((ses->stage == cfg->stage) && (ses->area == cfg->area)) {
            flags = cfg->flags;
            if (flags & ACTOR_00400_AREA_CONFIG_NO_DIVER) {
                return 1;
            }
            if (flags & ACTOR_00400_AREA_CONFIG_GRID_COLLISION) {
                work->gridCollision = 1;
            }
            if (cfg->waypointSets != NULL) {
                work->waypoints = cfg->waypointSets[(arg0->spawnArg1.value >> 4) & 3];
            }
            if (cfg->surfaceSpots != NULL) {
                work->surfaceSpots = cfg->surfaceSpots;
            }
            if (cfg->waterLevel != NULL) {
                work->waterLevel = (u16)*cfg->waterLevel;
            }
            return 0;
        }
        cfg++;
    }
    return 1;
}

/// Points the object at the second part coordinate of the model and clears its
/// pending-hit byte, then stores `hide` in `gridCollision`. Shared
/// by the two spawn states that attach the actor to its head.
///
/// `hide` is `s32` rather than `u8` so its literal lands in a different CSE mode
/// class from the `1` the callers store into `targetPart`, which is what makes
/// state 6 materialise the constant twice the way retail does.
static __inline__ void Actor00400_AttachHead(Task* arg0, Enemy* obj,
                                             _Actor00400Work* work, s32 hide)
{
    obj->coord                  = &arg0->extra.tmd->coords[1];
    obj->node.state.parts.flags = 0;
    work->gridCollision         = hide;
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
    s32                         failed;
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

    failed = Actor00400_ApplyAreaConfig(arg0);
    if (failed) {
        enemyDestroy(obj, arg0);
        return;
    }

    if ((arg0->spawnArg1.value & 0xF) == 0) {
        work->gridCollision = 1;
    }
    Actor00400_Fn00B48(arg0);
    arg0->msgTable = Actor00400_D16010;
    switch (arg0->spawnArg1.value & 0xF) {
        case 7:
            work->floatOffset = 200;
            work->inWater     = 1;
            work->targetPart  = 1;
            work->goalY       = (u16)work->waterLevel;
            Actor00400_AttachHead(arg0, obj, work, 0);
            w              = arg0->work;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 0x10;
            w->animRequest = DIVER_ANIM_REQUEST_RESET;
            w              = arg0->work;
            arg0->state    = 7;
            w->state       = 0;
            w->subState    = 0;
            Gp_IncStateF0Ref(0);
            obj->hp = (s16)obj->hpMax / 8;
            break;
        case 6:
            work->inWater    = 0;
            work->targetPart = 1;
            Actor00400_AttachHead(arg0, obj, work, 1);
            w              = arg0->work;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 0xF;
            w->animRequest = DIVER_ANIM_REQUEST_RESET;
            w              = arg0->work;
            arg0->state    = 6;
            w->state       = 0;
            w->subState    = 0;
            Gp_IncStateF0Ref(0);
            obj->hp    = (s16)obj->hpMax / 8;
            pos        = arg0->extra.tmd->coords;
            stainEnemy = arg0->spawnArg2.pointer;
            y          = (u16)pos->coord.t[1];
            task       = Task_SpawnFromTable(Actor00400_D16028, 2, 0, 0);
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
            nibble        = GameFlag_GetNibble(GAME_FLAG_0EB);
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
            nibble        = GameFlag_GetNibble(GAME_FLAG_0EB);
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
                if (GameFlag_GetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 0) {
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
            Actor00400_Fn02FF8(arg0);
            coord->coord.t[0] = work->surfaceSpot.vx;
            work->goalY       = (u16)work->waterLevel;
            coord->coord.t[1] = work->surfaceSpot.vy + work->waterLevel + 0x7D0;
            coord->coord.t[2] = work->surfaceSpot.vz;
            Gp_IncStateF0Ref(0);
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
            anim->animFrames = Actor00400_Fn086FC(arg0, anim->animFrames);
        }
        Actor00400_Fn08624(arg0);
        anim->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (anim->animRequest == DIVER_ANIM_REQUEST_RESET) {
        diverRestartClip(arg0);
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

/// Colours the actor from the second attach coordinate of its model through a
/// 0x10-byte `VECTOR` taken off the scratch stack, then zeroes the model's
/// background colour while `ambientOff` is set.
static __inline__ void Actor00400_UpdateColor(Task* arg0, GfxCoord* coord,
                                              _Actor00400Work* work, TmdObject* ctx)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    if (work->ambientOff != 0) {
        Gp_SetObjTrans(ctx, 0, 0, 0);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// States `Actor00400_Fn040DC` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_SWIM_DEATH_*`.
static const TaskFuncTable11 Actor00400_D0007C = { {
    Actor00400_Fn042C0,
    Actor00400_Fn04414,
    Actor00400_Fn07CC4,
    Actor00400_Fn07DE0,
    Actor00400_Fn07E20,
    Actor00400_Fn07E74,
    Actor00400_Fn07EE8,
    Actor00400_Fn07F18,
    Actor00400_Fn07F44,
    Actor00400_Fn07F88,
    Actor00400_Fn07FEC,
} };

/// Per-frame callback for the text actor's second task. Same frame gate as
/// `Actor00400_Fn04B48`: `gSceneCombatState.actorControl` 2 only flags the model hidden, 0 runs
/// this frame's state handler before falling through to the draw half, and 1
/// is the draw half on its own.
static void Actor00400_Fn040DC(Task* arg0)
{
    TaskFuncTable11  fns;
    _Actor00400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    _Actor00400Work* work2;
    GfxCoord*        coord;
    s32              y;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    fns   = Actor00400_D0007C;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            fns.funcs[work->state](arg0);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            if (work->hitReaction != 4) {
                work->neckRetracted = 1;
                Actor00400_Fn02648(arg0, 1);
            }
            y                 = coord->coord.t[1];
            coord->coord.t[1] = y + ((work->goalY + (s16)work->floatOffset - y) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            break;
    }
}

static void Actor00400_Fn042C0(Task* arg0)
{
    _Actor00400Work* work;
    Enemy*           obj;
    s32              id;

    work = arg0->work;
    obj  = arg0->spawnArg2.pointer;
    if (arg0->extra.tmd->coords->coord.t[1] - work->waterLevel < 0x320) {
        work->goalY = work->waterLevel;
    }
    worldTargetUnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    obj->recs = NULL;
    Gp_UnlinkObj(&work->trunkBody);
    Gp_UnlinkObj(&work->headBody);
    Gp_UnlinkObj(&work->attackBody);
    Gp_UnlinkObj(&work->gridBody);
    work->stateFrames = 0;
    if (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST) {
        _Actor00400Work* w = arg0->work;
        w->state           = ACTOR_00400_SWIM_DEATH_BLAST_HIDE;
        w->subState        = 0;
        return;
    }
    if (arg0->spawnArg1.value != 7) {
        _Actor00400Work* w;
        id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        w              = arg0->work;
        w->animStep    = 0x30;
        w->animBlend   = 4;
        w->animClip    = 1;
        w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    work->state++;
}

static void Actor00400_Fn04414(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    _Actor00400Work* w2;
    s32              i;

    work = arg0->work;
    w    = arg0->work;
    if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
        if (w->animPlaying != w->animClip) {
            w->animFrames = 0;
        } else {
            w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
        }
        Actor00400_Fn08624(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
        diverRestartClip(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        w->animFrames  = 0;
    } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
        w->animFrames++;
    }
    i = 1;
    do {
        animationTickSlot(&w->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(w->rig.slots));
    if (arg0->spawnArg1.value != 7) {
        if (!diverClipEnded(arg0)) {
            return;
        }
        w2              = arg0->work;
        w2->animBlend   = 4;
        w2->animStep    = ANIMATION_RATE_ONE;
        w2->animClip    = 0xE;
        w2->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    work->state++;
}

/// States `Actor00400_Fn04580` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_STRANDED_STATE_*` and the three recoil states.
static const TaskFuncTable10 Actor00400_D000A8 = { {
    Actor00400_Fn090B4,
    Actor00400_Fn09124,
    Actor00400_Fn091F8,
    Actor00400_Fn09260,
    Actor00400_Fn092D4,
    Actor00400_Fn09348,
    Actor00400_Fn093BC,
    Actor00400_Fn093C4,
    Actor00400_Fn09418,
    Actor00400_Fn0946C,
} };

static void Actor00400_Fn04580(Task* arg0)
{
    _Actor00400Work*  work = arg0->work;
    Enemy*            obj  = arg0->spawnArg2.pointer;
    TmdObject*        ctx  = arg0->extra.tmd;
    TaskFuncTable10   fns;
    GfxMatrix         m;
    GfxRotationWords* ia;
    _Actor00400Work*  w;
    _Actor00400Work*  w2;
    _Actor00400Work*  w3;
    _Actor00400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    fns = Actor00400_D000A8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            work->bobPhase++;
            work->frameCount++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->state](arg0);
            Actor00400_Fn00A14(arg0);
            w = arg0->work;
            if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
                if (w->animPlaying != w->animClip) {
                    w->animFrames = 0;
                } else {
                    w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
                diverRestartClip(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                w->animFrames  = 0;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
                w->animFrames++;
            }
            i = 1;
            do {
                animationTickSlot(&w->rig.anim, i);
                i++;
            } while (i < ARRAY_SIZE(w->rig.slots));
            work->animStatus = work->rig.slots[1].status.fields.flags;
            Actor00400_Fn016A4(arg0, work->lookDisabled);
            w2                     = arg0->work;
            coord                  = arg0->extra.tmd->coords;
            ia                     = &m.rotationWords;
            m.rotationWords.m00M01 = ONE;
            m.rotationWords.m02M10 = 0;
            ia->m11M12             = ONE;
            m.rotationWords.m20M21 = 0;
            ia->m22                = ONE;
            RotMatrixZ(w2->rotation.vz, &m.mat);
            RotMatrixY(w2->rotation.vy, &m.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.mat.m[0][0];
            dst->m[0][1]        = m.mat.m[0][1];
            dst->m[0][2]        = m.mat.m[0][2];
            dst->m[1][0]        = m.mat.m[1][0];
            dst->m[1][1]        = m.mat.m[1][1];
            dst->m[1][2]        = m.mat.m[1][2];
            dst->m[2][0]        = m.mat.m[2][0];
            dst->m[2][1]        = m.mat.m[2][1];
            dst->m[2][2]        = m.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3           = arg0->work;
                arg0->state  = 2;
                w3->state    = 0;
                w3->subState = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void Actor00400_Fn04900(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              id;

    work = arg0->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0xC;
        work->animRequest = DIVER_ANIM_REQUEST_RESET;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (diverClipEnded(arg0)) {
            work2           = arg0->work;
            work2->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            work2->subState = 0;
        }
    }
}

static void Actor00400_Fn04A1C(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    _Actor00400Work* work3;
    s32              id;

    work = arg0->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_HEAVY) {
        id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = arg0->work;
        work2->animBlend   = 6;
        work2->animStep    = ANIMATION_RATE_ONE;
        work2->animClip    = 0xD;
        work2->animRequest = DIVER_ANIM_REQUEST_BLEND;
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (diverClipEnded(arg0)) {
            work3           = arg0->work;
            work3->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            work3->subState = 0;
        }
    }
}

/// States `Actor00400_Fn04B48` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_STRANDED_DEATH_*`.
static const TaskFuncTable10 Actor00400_D000D0 = { {
    Actor00400_Fn04CF8,
    Actor00400_Fn08C54,
    Actor00400_Fn08D70,
    Actor00400_Fn08DFC,
    Actor00400_Fn08E50,
    Actor00400_Fn08FB0,
    Actor00400_Fn08FC8,
    Actor00400_Fn08FF4,
    Actor00400_Fn09038,
    Actor00400_Fn0909C,
} };

/// Per-frame callback for the main actor task. `gSceneCombatState.actorControl` gates the frame:
/// 2 only flags the model hidden, 0 runs this frame's state handler before
/// falling through to the draw half, and 1 is the draw half on its own.
static void Actor00400_Fn04B48(Task* arg0)
{
    TaskFuncTable10  fns;
    _Actor00400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    _Actor00400Work* work2;
    GfxCoord*        coord;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    fns  = Actor00400_D000D0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                break;
            }
            fns.funcs[work->state](arg0);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            coord = &ctx2->coords[1];
            Actor00400_UpdateColor(arg0, coord, work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], (u8)work->shadowShade);
            break;
    }
}

static void Actor00400_Fn04CF8(Task* arg0)
{
    _Actor00400Work* work;
    Enemy*           obj;
    s32              id;
    _Actor00400Work* w;

    work      = arg0->work;
    obj       = arg0->spawnArg2.pointer;
    obj->recs = NULL;
    Gp_UnlinkObj(&work->gridBody);
    Gp_UnlinkObj(&work->trunkBody);
    Gp_UnlinkObj(&work->headBody);
    Gp_UnlinkObj(&work->attackBody);
    worldTargetUnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    work->shadowShade = 0x80;
    if (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST) {
        w           = arg0->work;
        w->state    = ACTOR_00400_STRANDED_DEATH_BLAST_HIDE;
        w->subState = 0;
        return;
    }
    w                 = arg0->work;
    w->animBlend      = 8;
    w->animStep       = ANIMATION_RATE_ONE;
    w->animClip       = 0xF;
    w->animRequest    = DIVER_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                        (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->state++;
}

/// States `Actor00400_Fn04E18` dispatches on `_Actor00400Work::state`:
/// `ACTOR_00400_SWIM_STATE_*` and the three `ACTOR_00400_STATE_*`.
static const _Actor00400SwimStateTable Actor00400_D000F8 = { {
    Actor00400_Fn07738,
    Actor00400_Fn077F4,
    Actor00400_Fn05728,
    Actor00400_Fn078C8,
    Actor00400_Fn0793C,
    Actor00400_Fn07998,
    Actor00400_Fn079A0,
    Actor00400_Fn079A8,
    Actor00400_Fn079FC,
    Actor00400_Fn07ABC,
    Actor00400_Fn07B10,
    Actor00400_Fn07B98,
    Actor00400_Fn07C04,
    Actor00400_Fn09C04,
    Actor00400_Fn09C84,
} };

/// Rebuilds the root coordinate's rotation from `_Actor00400Work::rotation`:
/// the roll about Z, then the heading about Y.
static inline void _actor00400ApplyRootRotation(Task* task)
{
    _Actor00400Work*  work;
    GfxCoord*         coord;
    GfxMatrix         m;
    GfxRotationWords* ia;
    MATRIX*           dst;

    work                   = task->work;
    coord                  = task->extra.tmd->coords;
    ia                     = &m.rotationWords;
    m.rotationWords.m00M01 = ONE;
    m.rotationWords.m02M10 = 0;
    ia->m11M12             = ONE;
    m.rotationWords.m20M21 = 0;
    ia->m22                = ONE;
    RotMatrixZ(work->rotation.vz, &m.mat);
    RotMatrixY(work->rotation.vy, &m.mat);
    dst                 = &coord->coord;
    dst->m[0][0]        = m.mat.m[0][0];
    dst->m[0][1]        = m.mat.m[0][1];
    dst->m[0][2]        = m.mat.m[0][2];
    dst->m[1][0]        = m.mat.m[1][0];
    dst->m[1][1]        = m.mat.m[1][1];
    dst->m[1][2]        = m.mat.m[1][2];
    dst->m[2][0]        = m.mat.m[2][0];
    dst->m[2][1]        = m.mat.m[2][1];
    dst->m[2][2]        = m.mat.m[2][2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Takes the enemy off the lock-on list while its target part is more than
/// 0x190 below `_Actor00400Work::waterLevel`, and puts it back above that.
static inline void _actor00400UpdateLockable(Task* task)
{
    _Actor00400Work* work;
    TmdObject*       tmd;
    Enemy*           enemy;
    GfxCoord*        coord;
    SVECTOR          pos;

    work   = task->work;
    tmd    = task->extra.tmd;
    enemy  = task->spawnArg2.pointer;
    coord  = &tmd->coords[work->targetPart];
    pos.vx = 0;
    pos.vy = 0;
    pos.vz = 0;
    coordLocalToWorld(coord, &pos);
    if (work->waterLevel + 0x190 < pos.vy) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        enemy->node.state.parts.flags = 0;
    }
}

/// Per-frame callback for the boss task. Same `gSceneCombatState.actorControl` frame gate as
/// `Actor00400_Fn04580`, with the model's Y bobbed by two `rsin` terms and the
/// display object re-pointed at the part coordinate `targetPart` selects; the
/// tail hides the model again while the session sits in the two area-0xA/0xB
/// rooms of area 0x21.
static void Actor00400_Fn04E18(Task* arg0)
{
    _Actor00400Work*          work   = arg0->work;
    GfxCoord*                 coord0 = arg0->extra.tmd->coords;
    Enemy*                    obj    = arg0->spawnArg2.pointer;
    TmdObject*                ctx    = arg0->extra.tmd;
    _Actor00400SwimStateTable fns;
    _Actor00400Work*          w;
    _Actor00400Work*          wA;
    _Actor00400Work*          w3;
    _Actor00400Work*          work2;
    TmdObject*                ctx2;
    TmdObject*                ctx3;
    GameLocationKey*          sess;
    s32                       i;

    fns = Actor00400_D000F8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->suspended != 0) {
                return;
            }
            work->bobPhase++;
            work->frameCount++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->state](arg0);
            Actor00400_Fn00A14(arg0);
            wA = arg0->work;
            if (wA->emergeCooldown != 0) {
                wA->emergeCooldown--;
            }
            w = arg0->work;
            if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
                if (w->animPlaying != w->animClip) {
                    w->animFrames = 0;
                } else {
                    w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
                diverRestartClip(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                w->animFrames  = 0;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
                w->animFrames++;
            }
            i = 1;
            do {
                animationTickSlot(&w->rig.anim, i);
                i++;
            } while (i < ARRAY_SIZE(w->rig.slots));
            work->animStatus = work->rig.slots[1].status.fields.flags;
            Actor00400_Fn02648(arg0, work->neckRetracted);
            _actor00400ApplyRootRotation(arg0);
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3           = arg0->work;
                arg0->state  = 4;
                w3->state    = 0;
                w3->subState = 0;
            }
            coord0->coord.t[1] += (work->goalY - coord0->coord.t[1]) >> 4;
            if (work->state < ACTOR_00400_SWIM_STATE_TUNNEL_PATROL) {
                coord0->coord.t[1] += (rsin(work->bobPhase << 6) * 0x10) >> 12;
            }
            if (work->shakeFrames != 0) {
                work->shakeFrames--;
                coord0->coord.t[1] += (rsin(work->frameCount << 0xA) * 0x10) >> 0xA;
            }
            obj->coord = &arg0->extra.tmd->coords[work->targetPart];
            if (work->state < ACTOR_00400_SWIM_STATE_TUNNEL_PATROL) {
                _actor00400UpdateLockable(arg0);
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    sess = &gGameSession->location.loc;
    ctx3 = arg0->extra.tmd;
    if (sess->stage == GAME_STAGE_MINE_SHELTER && sess->area == 0x21 && (u32)(gGameSession->location.loc.view - 0xA) < 2U) {
        ctx3->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

static inline void Actor00400_TurnToward(Task* arg0, SVECTOR* target, s32 step, s32 range)
{
    _Actor00400Work* work = arg0->work;
    GfxCoord*        coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords               = arg0->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    vec.vx               = target->vx - coords->coord.t[0];
    vec.vy               = 0;
    vec.vz               = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->rotation.vy;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > range) {
        work->rotation.vy = angle - step;
    } else if (diff < -range) {
        work->rotation.vy = angle + step;
    }
}

/// Spawns the 16-way ring of `0x01202148` effects the boss uses when it lands
/// and when it is knocked down: one per 1/16 turn, at the height `waterLevel`
/// gives above the root coordinate.
static inline void Actor00400_SpawnRing(Task* arg0, _Actor00400Work* work, GfxCoord* coord)
{
    GfxCoord* coord2;
    SVECTOR   vec;
    s32       i;
    s16       y;

    i      = 0;
    y      = work->waterLevel - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
}

static void Actor00400_Fn05320(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w1;
    _Actor00400Work* w2;
    _Actor00400Work* w5;
    GfxCoord*        coord;
    s8               armed;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              sound3;
    s32              pan3;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->stateFrames++;
    if (work->stateFrames >= 0x14) {
        w1    = arg0->work;
        armed = 0;
        if (w1->targetDistance < 0xDAC && (u32)(w1->targetBearing - 0x600) >= 0x400U) {
            gSceneCombatState.signals.bytes.enemyAlert = 1;
            Gp_ArmStateF0(1);
            armed        = 1;
            w2           = arg0->work;
            w2->state    = ACTOR_00400_SWIM_STATE_DIVE;
            w2->subState = 0;
        }
        if (armed) {
            return;
        }
        Actor00400_TurnToward(arg0, &work->waypoints[work->waypointIndex & 7], 0x20, 0x30);
    }
    if (work->stateFrames == 8) {
        work->neckRetracted = 0;
        sound               = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->stateFrames == 0x14) {
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040004;
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (diverClipEnded(arg0)) {
        Actor00400_SpawnRing(arg0, work, coord);
        sound3 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
        pan3   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound3, pan3, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->goalY     = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
        w5              = arg0->work;
        w5->animBlend   = 4;
        w5->animStep    = ANIMATION_RATE_ONE;
        w5->animClip    = 1;
        w5->animRequest = DIVER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}

static void Actor00400_Fn05728(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    _Actor00400Work* w;
    _Actor00400Work* w2;
    _Actor00400Work* w3;
    u32              random;
    s16              next;
    u8               idx;
    u8               idx2;

    work = arg0->work;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (work->targetDistance < 0x2710 && (u32)(work->targetBearing - 0xC0) >= 0xE81U) {
            random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            if ((random >> 16) & 1) {
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
                work->emergeCooldown = 90;
                w2                   = arg0->work;
                w2->state            = ACTOR_00400_SWIM_STATE_DIVE;
                w2->subState         = 0;
            }
        } else {
            w3                                          = arg0->work;
            w3->state                                   = ACTOR_00400_SWIM_STATE_DIVE;
            w3->subState                                = 0;
            work->stateHistory[work->stateHistoryIndex] = work->state;
            idx2                                        = work->stateHistoryIndex + 1;
            work->stateHistoryIndex                     = idx2;
            if (idx2 >= (u32)ARRAY_SIZE(work->stateHistory)) {
                work->stateHistoryIndex = 0;
            }
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
    SVECTOR          delta;
    SVECTOR          vec2;
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
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->stateFrames == 0x14) {
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040004;
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames < 0x15) {
        return;
    }
    Actor00400_TurnToward(arg0, &work->targetPos, 0x18, 0x30);
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
        vec2.vx = vec2.vy = vec2.vz = 0;
        Actor00400_Fn031A4(arg0, &vec2);
        delta.vx = vec2.vx - coord->coord.t[0];
        delta.vy = 0;
        delta.vz = vec2.vz - coord->coord.t[2];
        if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) >= 0xDAC) {
            Actor00400_Fn02FF8(arg0);
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

static void Actor00400_Fn05D00(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    GfxCoord*        coord;
    GfxCoord*        coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    coord               = arg0->extra.tmd->coords;
    work                = arg0->work;
    work->neckRetracted = 1;
    work->goalY         = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    if (work->animClip != 3) {
        i      = 0;
        y      = work->waterLevel - coord->coord.t[1] + 0xFA;
        coord2 = arg0->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 8) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 8) >> 3;
            Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
            i++;
        } while (i < 16);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        w              = arg0->work;
        w->animBlend   = 4;
        w->animStep    = ANIMATION_RATE_ONE;
        w->animClip    = 1;
        w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    } else {
        work->subState++;
    }
    work->subState++;
}

static void Actor00400_Fn05EA4(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;
    s32              id;
    s32              pan;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Actor00400_Fn02FF8(arg0);
    vec.vx      = work->surfaceSpot.vx - coord->coord.t[0];
    vec.vy      = 0;
    vec.vz      = work->surfaceSpot.vz - coord->coord.t[2];
    work->goalY = (u16)work->waypoints[work->waypointIndex].vy + work->waterLevel;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 800 && work->emergeCooldown == 0) {
        _Actor00400Work* w;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->stateFrames = 0;
        w                 = arg0->work;
        w->state          = ACTOR_00400_SWIM_STATE_EMERGE;
        w->subState       = 0;
        return;
    }
    if (work->animClip != 3) {
        _Actor00400Work* w;
        w              = arg0->work;
        w->animBlend   = 10;
        w->animStep    = ANIMATION_RATE_ONE;
        w->animClip    = 3;
        w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    Actor00400_TurnToward(arg0, &work->surfaceSpot, 0x30, 0x100);
    diverStepForward(arg0, 0x60, work->rotation.vy);
    if (!(work->frameCount & 0xF)) {
        id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static void Actor00400_Fn060CC(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              id;

    work = arg0->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animStep    = 0x20;
        work->animClip    = 0xA;
        work->animRequest = DIVER_ANIM_REQUEST_RESET;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (diverClipEnded(arg0)) {
            work2           = arg0->work;
            work2->state    = ACTOR_00400_SWIM_STATE_DECIDE;
            work2->subState = 0;
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
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    y      = work->waterLevel - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
    sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
    pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}

static void Actor00400_Fn06380(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;

    work = arg0->work;
    work->stateFrames++;
    Actor00400_TurnToward(arg0, &work->targetPos, 0x10, 0x20);
    if (work->stateFrames == 1) {
        work->attackFrames = 0x18;
    }
    if (work->stateFrames >= 0x24) {
        work->stateFrames = 0;
        w                 = arg0->work;
        w->animBlend      = 8;
        w->animStep       = ANIMATION_RATE_ONE;
        w->animClip       = 7;
        w->animRequest    = DIVER_ANIM_REQUEST_BLEND;
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
    task   = Task_SpawnFromTable(Actor00400_D16028, 1, 0, 0);
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
            coordLocalToWorld(span, &base);
            coordLocalToWorld(span, &tip);
            task->work = shot;
            dst        = task->extra.tmd->coords;
            pos.vx     = 0;
            pos.vy     = 0;
            pos.vz     = 0;
            coordLocalToWorld(origin, &pos);
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
    Actor00400_TurnToward(arg0, &work->targetPos, 0x10, 0x20);
    if (work->stateFrames == 0x29) {
        id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000A;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0x2B) {
        Actor00400_SpawnMarker(arg0);
    }
    if (diverClipEnded(arg0)) {
        work2           = arg0->work;
        work2->state    = ACTOR_00400_SWIM_STATE_DECIDE;
        work2->subState = 0;
    }
}

static void Actor00400_Fn06798(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = (u16)work->waypoints[work->waypointIndex].vx - coord->coord.t[0];
    vec.vy = (u16)work->waypoints[work->waypointIndex].vy - coord->coord.t[1];
    vec.vz = (u16)work->waypoints[work->waypointIndex].vz - coord->coord.t[2];

    work->goalY = (u16)work->waypoints[work->waypointIndex].vy;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 1000) {
        work->waypointIndex = (work->waypointIndex + 1) & 7;
        return;
    }
    if (work->animClip != 3) {
        _Actor00400Work* w;
        _Actor00400Work* a;
        s32              i;

        w              = arg0->work;
        w->animBlend   = 10;
        w->animStep    = ANIMATION_RATE_ONE;
        w->animClip    = 3;
        w->animRequest = DIVER_ANIM_REQUEST_BLEND;

        a = arg0->work;
        if (a->animRequest == DIVER_ANIM_REQUEST_BLEND) {
            if (a->animPlaying != a->animClip) {
                a->animFrames = 0;
            } else {
                a->animFrames = Actor00400_Fn086FC(arg0, a->animFrames);
            }
            Actor00400_Fn08624(arg0);
            a->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        } else if (a->animRequest == DIVER_ANIM_REQUEST_RESET) {
            diverRestartClip(arg0);
            a->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            a->animFrames  = 0;
        } else if (a->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
            a->animFrames++;
        }
        i = 1;
        do {
            animationTickSlot(&a->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(a->rig.slots));
    }
    Actor00400_TurnToward(arg0, &work->waypoints[work->waypointIndex], 0x2C, 0x100);
    diverStepForward(arg0, 0x60, work->rotation.vy);
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
}

static void Actor00400_Fn06A44(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    s32              id;
    s32              pan;

    work = arg0->work;
    work->stateFrames++;
    if (work->stateFrames == 1) {
        SndEvt_EnqueueType6(((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54220005, 0, 0);
    }
    if (work->stateFrames == 8) {
        work->neckRetracted = 0;
        id                  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateFrames == 0x14) {
        work->attackFrames = 0x18;
        w                  = arg0->work;
        w->animBlend       = 4;
        w->animStep        = ANIMATION_RATE_ONE;
        w->animClip        = 7;
        w->animRequest     = DIVER_ANIM_REQUEST_BLEND;
        work->stateFrames  = 0;
        work->subState++;
    }
}

/// Per-frame callback for the text actor's third task, with the same
/// `gSceneCombatState.actorControl` frame gate as `Actor00400_Fn04B48`: 2 only flags the model
/// hidden, 0 runs this frame's state handler and rebuilds the root rotation
/// before falling through to the draw half, and 1 is the draw half on its own.
static void Actor00400_Fn06B7C(Task* arg0)
{
    _Actor00400Work*  work             = arg0->work;
    Enemy*            obj              = arg0->spawnArg2.pointer;
    TmdObject*        ctx              = arg0->extra.tmd;
    void              (*fns[2])(Task*) = { Actor00400_Fn08A88, Actor00400_Fn08B40 };
    GfxMatrix         m;
    GfxRotationWords* ia;
    _Actor00400Work*  w;
    _Actor00400Work*  w2;
    _Actor00400Work*  w3;
    _Actor00400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase++;
            work->frameCount++;
            Actor00400_Fn01454(arg0);
            fns[work->state](arg0);
            w = arg0->work;
            if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
                if (w->animPlaying != w->animClip) {
                    w->animFrames = 0;
                } else {
                    w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
                diverRestartClip(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                w->animFrames  = 0;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
                w->animFrames++;
            }
            i = 1;
            do {
                animationTickSlot(&w->rig.anim, i);
                i++;
            } while (i < ARRAY_SIZE(w->rig.slots));
            work->animStatus       = work->rig.slots[1].status.fields.flags;
            w2                     = arg0->work;
            coord                  = arg0->extra.tmd->coords;
            ia                     = &m.rotationWords;
            m.rotationWords.m00M01 = ONE;
            m.rotationWords.m02M10 = 0;
            ia->m11M12             = ONE;
            m.rotationWords.m20M21 = 0;
            ia->m22                = ONE;
            RotMatrixZ(w2->rotation.vz, &m.mat);
            RotMatrixY(w2->rotation.vy, &m.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.mat.m[0][0];
            dst->m[0][1]        = m.mat.m[0][1];
            dst->m[0][2]        = m.mat.m[0][2];
            dst->m[1][0]        = m.mat.m[1][0];
            dst->m[1][1]        = m.mat.m[1][1];
            dst->m[1][2]        = m.mat.m[1][2];
            dst->m[2][0]        = m.mat.m[2][0];
            dst->m[2][1]        = m.mat.m[2][1];
            dst->m[2][2]        = m.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3           = arg0->work;
                arg0->state  = 2;
                w3->state    = 0;
                w3->subState = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static inline s32 Actor00400_ConsumeStateRequest(_Actor00400Work* work)
{
    s16 req;
    s32 state;

    state = work->hitTaken;
    if (state != 1) {
        return 0;
    }
    req = work->hitReaction;
    if (req == ACTOR_00400_HIT_REACTION_LIGHT)
        goto set;
    if (req == ACTOR_00400_HIT_REACTION_HEAVY)
        goto set;
    if (req == ACTOR_00400_HIT_REACTION_STATUS)
        goto set;
    if (req != ACTOR_00400_HIT_REACTION_BLAST)
        goto other;
set:
    /* The do/while(0) is load-bearing: flow.c weights REG_N_REFS by loop
       depth, and the two extra references it buys `work` are what let the
       pointer outrank `req` in global.c's allocation order. */
    do {
        work->state    = state;
        work->subState = 0;
    } while (0);
other:
    work->hitReaction = ACTOR_00400_HIT_REACTION_NONE;
    return 1;
}

static void Actor00400_Fn06EA4(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        if (diverClipEnded(arg0)) {
            work2              = arg0->work;
            work2->animBlend   = 8;
            work2->animStep    = 4;
            work2->animClip    = 0xF;
            work2->animRequest = DIVER_ANIM_REQUEST_BLEND;
        }
    }
}

static void Actor00400_Fn06F64(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              id;

    work = arg0->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animBlend   = 2;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0x13;
        work->animRequest = DIVER_ANIM_REQUEST_BLEND;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        if (diverClipEnded(arg0)) {
            work2           = arg0->work;
            work2->state    = 0;
            work2->subState = 0;
        }
    }
}

/// Per-frame callback for the text actor's fourth task. Same frame gate as
/// `Actor00400_Fn06B7C`, but the draw half only recolours the actor: case 0
/// runs this frame's state handler, lerps the root coordinate's height a
/// sixteenth of the way towards `goalY` and falls through.
static void Actor00400_Fn070C0(Task* arg0)
{
    _Actor00400Work*  work             = arg0->work;
    TmdObject*        ctx              = arg0->extra.tmd;
    Enemy*            obj              = arg0->spawnArg2.pointer;
    GfxCoord*         coord0           = ctx->coords;
    void              (*fns[2])(Task*) = { Actor00400_Fn0A468, Actor00400_Fn0A4BC };
    GfxMatrix         m;
    GfxRotationWords* ia;
    _Actor00400Work*  w;
    _Actor00400Work*  w2;
    _Actor00400Work*  w3;
    _Actor00400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase++;
            work->frameCount++;
            Actor00400_Fn01454(arg0);
            fns[work->state](arg0);
            w = arg0->work;
            if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
                if (w->animPlaying != w->animClip) {
                    w->animFrames = 0;
                } else {
                    w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
                diverRestartClip(arg0);
                w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
                w->animFrames  = 0;
            } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
                w->animFrames++;
            }
            i = 1;
            do {
                animationTickSlot(&w->rig.anim, i);
                i++;
            } while (i < ARRAY_SIZE(w->rig.slots));
            work->animStatus       = work->rig.slots[1].status.fields.flags;
            w2                     = arg0->work;
            coord                  = arg0->extra.tmd->coords;
            ia                     = &m.rotationWords;
            m.rotationWords.m00M01 = ONE;
            m.rotationWords.m02M10 = 0;
            ia->m11M12             = ONE;
            m.rotationWords.m20M21 = 0;
            ia->m22                = ONE;
            RotMatrixZ(w2->rotation.vz, &m.mat);
            RotMatrixY(w2->rotation.vy, &m.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.mat.m[0][0];
            dst->m[0][1]        = m.mat.m[0][1];
            dst->m[0][2]        = m.mat.m[0][2];
            dst->m[1][0]        = m.mat.m[1][0];
            dst->m[1][1]        = m.mat.m[1][1];
            dst->m[1][2]        = m.mat.m[1][2];
            dst->m[2][0]        = m.mat.m[2][0];
            dst->m[2][1]        = m.mat.m[2][1];
            dst->m[2][2]        = m.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3           = arg0->work;
                arg0->state  = 4;
                w3->state    = 0;
                w3->subState = 0;
            }
            coord0->coord.t[1] += (work->goalY - coord0->coord.t[1]) >> 4;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void Actor00400_Fn07400(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work3;
    s32              phase;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        phase             = (u16)work->stateFrames + 1;
        work->stateFrames = phase;
        work->goalY       = work->floatOffset + ((u16)work->waterLevel + ((rsin(phase << 16 >> 10) * 0x10) >> 10));
        if (diverClipEnded(arg0)) {
            work3              = arg0->work;
            work3->animBlend   = 8;
            work3->animStep    = 2;
            work3->animClip    = 0x10;
            work3->animRequest = DIVER_ANIM_REQUEST_BLEND;
        }
    }
}

static void Actor00400_Fn07518(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              phase;

    work = arg0->work;
    if (work->hitTaken != 0 && work->hitReaction == ACTOR_00400_HIT_REACTION_LIGHT) {
        work->animBlend   = 2;
        work->animStep    = ANIMATION_RATE_ONE;
        work->animClip    = 0x12;
        work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    if (Actor00400_ConsumeStateRequest(arg0->work) == 0) {
        phase             = (u16)work->stateFrames + 1;
        work->stateFrames = phase;
        work->goalY       = work->floatOffset + ((u16)work->waterLevel + ((rsin(phase << 16 >> 9) * 0x10) >> 9));
        if (work->stateFrames >= 0x79) {
            work2           = arg0->work;
            work2->state    = 0;
            work2->subState = 0;
        }
    }
}

#include "../../shared/diver_step_forward.inc.c"

/// Two-state dispatcher over a handler table built on the stack.
void Actor00400_Fn076E8(Task* task)
{
    TaskFunc funcs[2] = {
        Actor00400_Fn0A2F4,
        Actor00400_Fn0A364,
    };

    funcs[task->state](task);
}

/// Draws two LCG values into the work's bob phase and `frameCount`, resets the
/// state counters and copies the root coordinate's `t[1]` into `goalY`.
static void Actor00400_Fn07738(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    _Actor00400Work* w2;
    GfxCoord*        coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Gp_IncStateF0Ref(0);
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->bobPhase   = gRandomLcgState >> 16;
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->frameCount = gRandomLcgState >> 16;
    w                = arg0->work;
    w->animStep      = ANIMATION_RATE_ONE;
    w->animClip      = 1;
    w->animRequest   = DIVER_ANIM_REQUEST_RESET;
    w2               = arg0->work;
    w2->state        = ACTOR_00400_SWIM_STATE_PATROL;
    w2->subState     = 0;
    work->goalY      = coord->coord.t[1];
}

/// States `Actor00400_Fn077F4` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable4 Actor00400_D00134 = { {
    Actor00400_Fn094C0,
    Actor00400_Fn094DC,
    Actor00400_Fn05320,
    Actor00400_Fn095D8,
} };

static void Actor00400_Fn077F4(Task* arg0)
{
    _Actor00400Work* work;
    TaskFuncTable4   handlers;
    _Actor00400Work* work2;

    work     = arg0->work;
    handlers = Actor00400_D00134;
    if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
    } else if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
        work2           = arg0->work;
        work2->state    = ACTOR_00400_SWIM_STATE_DIVE;
        work2->subState = 0;
    } else {
        handlers.funcs[work->subState](arg0);
    }
}

static void Actor00400_Fn078C8(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0962C,
        Actor00400_Fn058C4,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[work->subState](arg0);
    }
}

/// States `Actor00400_Fn0793C` and `Actor00400_Fn09C04` dispatch on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D00144 = { {
    Actor00400_Fn05D00,
    Actor00400_Fn096C0,
    Actor00400_Fn05EA4,
} };

static void Actor00400_Fn0793C(Task* arg0)
{
    _Actor00400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00144;
    fns.funcs[work->subState](arg0);
}

static void Actor00400_Fn07998(Task* task)
{
}

static void Actor00400_Fn079A0(Task* task)
{
}

static void Actor00400_Fn079A8(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        diverState7Enter,
        Actor00400_Fn060CC,
    };

    states[work->subState](arg0);
}

static inline s32 Actor00400_TakeStateRequest(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
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
        Actor00400_Fn097C8,
    };
    s16 taken;

    taken = Actor00400_TakeStateRequest(arg0);
    if (taken == 0) {
        states[work->subState](arg0);
    }
}

static void Actor00400_Fn07ABC(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn098A8,
        Actor00400_Fn09924,
    };

    states[work->subState](arg0);
}

/// States `Actor00400_Fn07B10` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D00150 = { {
    Actor00400_Fn09A1C,
    Actor00400_Fn06380,
    Actor00400_Fn064B0,
} };

static void Actor00400_Fn07B10(Task* arg0)
{
    _Actor00400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00150;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        fns.funcs[work->subState](arg0);
    }
}

/// States `Actor00400_Fn07B98` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable3 Actor00400_D0015C = { {
    Actor00400_Fn09A48,
    Actor00400_Fn09A8C,
    Actor00400_Fn06798,
} };

static void Actor00400_Fn07B98(Task* arg0)
{
    Enemy*           obj;
    _Actor00400Work* work;
    TaskFuncTable3   fns;

    obj                         = arg0->spawnArg2.pointer;
    work                        = arg0->work;
    fns                         = Actor00400_D0015C;
    work->neckRetracted         = 1;
    obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    fns.funcs[work->subState](arg0);
}

/// States `Actor00400_Fn07C04` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable4 Actor00400_D00168 = { {
    Actor00400_Fn09AE0,
    Actor00400_Fn09B44,
    Actor00400_Fn09B74,
    Actor00400_Fn09BDC,
} };

static void Actor00400_Fn07C04(Task* arg0)
{
    Enemy*           obj;
    _Actor00400Work* work;
    TaskFuncTable4   handlers;
    _Actor00400Work* work2;

    obj      = arg0->spawnArg2.pointer;
    work     = arg0->work;
    handlers = Actor00400_D00168;
    if (GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        work2           = arg0->work;
        work2->state    = ACTOR_00400_SWIM_STATE_TUNNEL_PATROL;
        work2->subState = 0;
    } else {
        work->neckRetracted         = 1;
        obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        handlers.funcs[work->subState](arg0);
    }
}

static void Actor00400_Fn07CC4(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    s32              i;

    work = arg0->work;
    work->stateFrames++;
    w = arg0->work;
    if (w->animRequest == DIVER_ANIM_REQUEST_BLEND) {
        if (w->animPlaying != w->animClip) {
            w->animFrames = 0;
        } else {
            w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
        }
        Actor00400_Fn08624(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (w->animRequest == DIVER_ANIM_REQUEST_RESET) {
        diverRestartClip(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        w->animFrames  = 0;
    } else if (w->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
        w->animFrames++;
    }
    i = 1;
    do {
        animationTickSlot(&w->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(w->rig.slots));
    if (work->stateFrames >= 0x3C) {
        work->state++;
    }
}

static void Actor00400_Fn07DE0(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state       = (u16)work->state + 1;
}

static void Actor00400_Fn07E20(Task* arg0)
{
    _Actor00400Work* work;
    TmdObject*       ctx;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->stateFrames >= 0x18) {
        ctx->flags       |= TMD_OBJECT_SEMI_TRANS;
        work->stateFrames = 0;
        work->state++;
    }
}

static void Actor00400_Fn07E74(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (++work->stateFrames == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->stateFrames > 0x20) {
        work->state++;
    }
}

static void Actor00400_Fn07EE8(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;

    ctx            = arg0->extra.tmd;
    ctx->flags    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work           = arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}

static void Actor00400_Fn07F18(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;

    ctx               = arg0->extra.tmd;
    work              = arg0->work;
    ctx->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->state++;
}

static void Actor00400_Fn07F44(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (++work->stateFrames >= 2) {
        work->state++;
    }
}

static void Actor00400_Fn07F88(Task* arg0)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor00400_Fn0237C(arg0);
    work->state = (u16)work->state + 1;
}

static void Actor00400_Fn07FEC(Task* arg0)
{
    _Actor00400Work* work;

    work           = arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}

void Actor00400_Fn08004(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Actor00400_D0002C;
    sp.funcs[arg0->state](arg0);
}

/// States `Actor00400_Fn09C04` dispatches on `_Actor00400Work.subState`.
static const TaskFuncTable7 Actor00400_D00178 = { {
    Actor00400_Fn09CCC,
    Actor00400_Fn09D3C,
    Actor00400_Fn09D98,
    Actor00400_Fn09E70,
    Actor00400_Fn06A44,
    Actor00400_Fn09F18,
    Actor00400_Fn09FDC,
} };

void Actor00400_Fn0805C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor00400Work* work;
    Enemy*           obj;
    GfxCoord*        coord;
    _Actor00400Work* w;

    work  = arg0->work;
    obj   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
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
            obj->node.state.parts.flags = 0;
            Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            work->inWater     = 0;
            work->command     = ACTOR_00400_COMMAND_STRAND;
            coord->coord.t[1] = 0;
            arg0->state       = 1;
            w                 = arg0->work;
            w->state          = 0;
            w->subState       = 0;
            w                 = arg0->work;
            w->state          = ACTOR_00400_STRANDED_STATE_DECIDE;
            w->subState       = 0;
            break;
    }
}

static void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3)
{
    MATRIX           m;
    VECTOR           d;
    VECTOR           r;
    GfxCoord*        coords;
    _Actor00400Work* work;

    coords = arg0->extra.tmd->coords;
    work   = arg0->work;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[arg1].workm, &m);
    d.vx = work->targetPos.vx - m.t[0];
    d.vy = work->targetPos.vy - arg3 - m.t[1];
    d.vz = work->targetPos.vz - m.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &d, &r);
    arg2->vx = ratan2(-r.vy, r.vz) << 20 >> 20;
    arg2->vy = ratan2(r.vx, r.vz) << 20 >> 20;
    arg2->vz = 0;
}

static void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX    a;
    MATRIX    b;
    GfxCoord* coordA;
    GfxCoord* coordB;
    GfxCoord* coords;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coordA                     = &coords[arg1];
    coordB                     = &coords[arg2];
    actorRenderComposeCoord(&gGfxViewCoord);
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coordA);
    actorRenderComposeCoord(coordB);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordA->workm, &a);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordB->workm, &b);
    arg3->vx             = (a.t[0] + b.t[0]) / 2;
    arg3->vz             = (a.t[2] + b.t[2]) / 2;
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Reaction to message 0x7D5: `arg2` toggles the "big" flag (0x80) on the
/// actor's context. Turning it on is unconditional; turning it off first checks
/// whether the current state / animation combination still wants it held.
///
/// GCC 2.8.1 decides the store to `gSceneCombatState`, at a fixed address, cannot
/// alias the struct fields reached through `ctx` / `work`, so without the
/// barrier the scheduler sinks this `sb` past the traffic that follows it.
void Actor00400_Fn08354(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    _Actor00400Work* work;
    TmdObject*       ctx;
    s32              state;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            gSceneCombatState.actor00400HideRequested = 1;
            ctx->flags                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->suspended                           = 1;
            break;
        case 1:
            gSceneCombatState.actor00400HideRequested = 0;
            state                                     = arg0->state;
            if (((state == 2) || (state == 4)) && (work->hitReaction == ACTOR_00400_HIT_REACTION_BLAST)) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((arg0->state == 2) && ((work->state == ACTOR_00400_STRANDED_DEATH_SHRINK) || (work->state == ACTOR_00400_STRANDED_DEATH_END))) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((arg0->state == 4) && (work->state == ACTOR_00400_SWIM_DEATH_HIDE)) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if (arg0->state == 5) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            work->suspended = 0;
            break;
    }
}

static void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX    root;
    MATRIX    a;
    MATRIX    b;
    GfxCoord* coordA;
    GfxCoord* coordB;
    GfxCoord* coords;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coordA                     = &coords[arg1];
    coordB                     = &coords[arg2];
    actorRenderComposeCoord(&gGfxViewCoord);
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coordA);
    actorRenderComposeCoord(coordB);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &root);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordA->workm, &a);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordB->workm, &b);
    coords[0].coord.t[0]   = arg3->vx - ((a.t[0] + b.t[0]) / 2 - root.t[0]);
    coords[0].coord.t[2]   = arg3->vz - ((a.t[2] + b.t[2]) / 2 - root.t[2]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coordA->composeStamp   = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coordA);
    actorRenderComposeCoord(coordB);
    actorRenderComposeCoord(coords);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Like `diverRestartClip`, but restarts every slot through `animationSeekSlotWithBlend`
/// with the pending blend value `animBlend`, which is consumed (cleared) only
/// when the requested clip `animClip` differs from the current `animPlaying`.
static void Actor00400_Fn08624(Task* arg0)
{
    _Actor00400Work* work;
    s32              i;

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
    work->animPlaying = (u16)work->animClip;
}

/// Scales `arg1` (a 12-bit angle) by the ratio `work->animStep`, returning 0
/// while that field is unset.
static s16 Actor00400_Fn086FC(Task* arg0, s16 arg1)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->animStep == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->animStep << 12) >> 16;
}

/// Turns `work->rotation.vy` toward `arg1` by at most `arg2` per call, but only
/// once the shortest signed 12-bit angle difference leaves the deadband
/// `arg3 & 0x7FF`. The two conditions share one arm rather than nesting, which
/// collapses the arms into the entry block: `work` then lives over 32 insns,
/// exactly tying its allocator priority with `range`'s, and the tie falls
/// through to the declaration order - hence `range` is declared ahead of `work`.
static void Actor00400_Fn0875C(Task* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    s32              range;
    _Actor00400Work* work;
    GfxCoord*        coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    work                 = arg0->work;
    coords               = arg0->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    range                = arg3 & 0x7FF;
    vec.vx               = (u16)arg1->vx - coords->coord.t[0];
    vec.vy               = 0;
    vec.vz               = (u16)arg1->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->rotation.vy;
    diff  = ((angle - yaw) << 20) >> 20;
    if ((diff > range) || (diff < -range)) {
        work->rotation.vy = (diff > range) ? (angle - arg2) : (angle + arg2);
    }
}

/// One animation-step: state 1 starts the clip `animClip` (or advances the
/// current one through `Actor00400_Fn086FC` when it is already in place, and
/// resets `animFrames` when it is not), state 2 finishes the old clip and state
/// 3 counts `animFrames` up a frame at a time. All three land in state 3 and
/// then tick animation slots 1..14.
static void Actor00400_Fn08814(Task* arg0)
{
    _Actor00400Work* work;
    s32              i;

    work = arg0->work;
    if (work->animRequest == DIVER_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrames = 0;
        } else {
            work->animFrames = Actor00400_Fn086FC(arg0, work->animFrames);
        }
        Actor00400_Fn08624(arg0);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == DIVER_ANIM_REQUEST_RESET) {
        diverRestartClip(arg0);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (work->animRequest == DIVER_ANIM_REQUEST_PLAYING) {
        work->animFrames++;
    }
    i = 1;
    do {
        animationTickSlot(&work->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work->rig.slots));
}

static void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    _Actor00400Work* work;

    work              = arg0->work;
    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
}

/// Same body as src/lib/actors_shared_8016974c.c.
static s16 Actor00400_Fn08908(Task* arg0)
{
    _Actor00400Work* work = arg0->work;

    if ((work->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->animStatus & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

void Actor00400_Fn08948(Task* arg0)
{
    TaskFuncTable8 fns;

    fns = Actor00400_D00038;
    fns.funcs[arg0->state](arg0);
}

static void Actor00400_Fn089C8(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A3D4,
        Actor00400_Fn0A414,
    };

    states[work->state](arg0);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. Same body as src/lib/actors_shared_80132c4c.c.
static void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst)
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

static void Actor00400_Fn08A88(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08ADC,
        Actor00400_Fn06EA4,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn08ADC(Task* arg0)
{
    _Actor00400Work* w;
    _Actor00400Work* work;
    u32              random;

    work              = arg0->work;
    random            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState   = random;
    w                 = arg0->work;
    w->animBlend      = 8;
    w->animStep       = ((random >> 16) & 3) + 3;
    w->animClip       = 0xF;
    w->animRequest    = DIVER_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->subState++;
}

static void Actor00400_Fn08B40(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08B94,
        Actor00400_Fn06F64,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn08B94(Task* arg0)
{
    s32              sound;
    s32              pan;
    _Actor00400Work* work;
    _Actor00400Work* w;

    work  = arg0->work;
    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    w              = arg0->work;
    w->animBlend   = 2;
    w->animStep    = ANIMATION_RATE_ONE;
    w->animClip    = 0x13;
    w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->subState++;
}

static void Actor00400_Fn08C54(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    s16              mode;
    s32              i;

    work = arg0->work;
    work->stateFrames++;

    w    = arg0->work;
    mode = w->animRequest;
    if (mode == DIVER_ANIM_REQUEST_BLEND) {
        if (w->animPlaying != w->animClip) {
            w->animFrames = 0;
        } else {
            w->animFrames = Actor00400_Fn086FC(arg0, w->animFrames);
        }
        Actor00400_Fn08624(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (mode == DIVER_ANIM_REQUEST_RESET) {
        diverRestartClip(arg0);
        w->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        w->animFrames  = 0;
    } else if (mode == DIVER_ANIM_REQUEST_PLAYING) {
        w->animFrames++;
    }

    for (i = 1; i < ARRAY_SIZE(w->rig.slots); i++) {
        animationTickSlot(&w->rig.anim, i);
    }

    if (work->stateFrames >= 0x1E) {
        work->state++;
    }
}

static void Actor00400_Fn08D70(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->shrinkScaleY = ONE;
    work->savedRootMtx = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

static void Actor00400_Fn08DFC(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->stateFrames >= 0x18) {
        ctx->flags       |= TMD_OBJECT_SEMI_TRANS;
        work->stateFrames = 0;
        work->state++;
    }
}

static void Actor00400_Fn08E50(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;
    GfxCoord*        coord;
    VECTOR           scale;
    SVECTOR          pos;

    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    coord = ctx->coords;

    work->shadowShade -= 7;
    if (work->shadowShade < 0) {
        work->shadowShade = 0;
    }
    work->shrinkScaleY -= 0x40;
    scale.vx            = 0x1000;
    scale.vy            = work->shrinkScaleY;
    scale.vz            = 0x1000;
    coord->coord        = work->savedRootMtx;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (++work->stateFrames == 4) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 4, &pos);
    }
    if (work->stateFrames == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->stateFrames >= 0x21) {
        ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}

static void Actor00400_Fn08FB0(Task* arg0)
{
    _Actor00400Work* work;

    work           = arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}

static void Actor00400_Fn08FC8(Task* arg0)
{
    TmdObject*       ctx;
    _Actor00400Work* work;

    ctx               = arg0->extra.tmd;
    work              = arg0->work;
    ctx->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->state++;
}

static void Actor00400_Fn08FF4(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (++work->stateFrames >= 2) {
        work->state++;
    }
}

static void Actor00400_Fn09038(Task* arg0)
{
    TmdObject*       model;
    _Actor00400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor00400_Fn0237C(arg0);
    work->state = (u16)work->state + 1;
}

static void Actor00400_Fn0909C(Task* arg0)
{
    _Actor00400Work* work;

    work           = arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}

static void Actor00400_Fn090B4(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    _Actor00400Work* w2;

    work                                                      = arg0->work;
    ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
    Gp_IncStateF0Ref(0);
    work->bobPhase   = 0;
    work->frameCount = 0x174B;
    w                = arg0->work;
    w->animStep      = ANIMATION_RATE_ONE;
    w->animClip      = 2;
    w->animRequest   = DIVER_ANIM_REQUEST_RESET;
    w2               = arg0->work;
    w2->state        = ACTOR_00400_STRANDED_STATE_WAIT;
    w2->subState     = 0;
}

/// Every path out of the range test funnels through `set`, where the arm flag
/// is copied for the test below: the in-range edge arrives with the same value
/// (0), so the copy is redundant there and cse drops it, which leaves `done`
/// defined only here - and reorg then fills the branch's delay slot with a copy
/// of that one move.
static void Actor00400_Fn09124(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    _Actor00400Work* w2;
    s32              active;
    s32              done;

    work   = arg0->work;
    active = 0;
    if (work->targetDistance >= 0xDAC) {
        goto set;
    }
    done = 0;
    if ((u32)(work->targetBearing - 0x600) >= 0x400U) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
        active      = 1;
        w           = arg0->work;
        w->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        w->subState = 0;
    }
set:
    done = active;
    if (done == 0) {
        if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
            Gp_ArmStateF0(1);
            return;
        }
        if (work->hitTaken != 0) {
            w2           = arg0->work;
            w2->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
            w2->subState = 0;
        }
    }
}

static void Actor00400_Fn091F8(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A5B8,
    };

    if (Actor00400_Fn02154(arg0) == 0) {
        states[work->subState](arg0);
    }
}

static void Actor00400_Fn09260(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A680,
        Actor00400_Fn0A6B0,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[work->subState](arg0);
    }
}

static void Actor00400_Fn092D4(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A704,
        Actor00400_Fn0A760,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[work->subState](arg0);
    }
}

static void Actor00400_Fn09348(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A7F0,
        Actor00400_Fn0A82C,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[work->subState](arg0);
    }
}

static void Actor00400_Fn093BC(Task* task)
{
}

static void Actor00400_Fn093C4(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A880,
        Actor00400_Fn04900,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn09418(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A940,
        Actor00400_Fn04A1C,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn0946C(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A9F4,
        Actor00400_Fn0AA40,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn094C0(Task* arg0)
{
    _Actor00400Work* work;

    work                = arg0->work;
    work->waypointIndex = 0;
    work->subState      = work->subState + 1;
}

static void Actor00400_Fn094DC(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* work2;
    s32              pan;
    s32              sound;

    work = arg0->work;
    if ((Actor00400_Fn02208(arg0) << 0x10) != 0) {
        if (work->animClip != 5) {
            work2              = arg0->work;
            work2->animBlend   = 0x10;
            work2->animStep    = ANIMATION_RATE_ONE;
            work2->animClip    = 5;
            work2->animRequest = DIVER_ANIM_REQUEST_BLEND;
        }
        work->goalY = work->waterLevel;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
        return;
    }
    work->neckRetracted = 1;
    if (!(work->frameCount & 0xF)) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static void Actor00400_Fn095D8(Task* arg0)
{
    _Actor00400Work* work;

    work                = arg0->work;
    work->neckRetracted = 1;
    if (diverClipEnded(arg0)) {
        work->subState = 1;
    }
}

static void Actor00400_Fn0962C(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    GfxCoord*        coord;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->emergePos.vx = work->surfaceSpot.vx;
    work->stateFrames  = 0;
    work->emergePos.vy = work->surfaceSpot.vy;
    work->emergePos.vz = work->surfaceSpot.vz;
    coord->coord.t[0] += ((s16)work->emergePos.vx - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += ((s16)work->emergePos.vz - coord->coord.t[2]) >> 2;
    w                  = arg0->work;
    w->animBlend       = 0xA;
    w->animStep        = ANIMATION_RATE_ONE;
    w->animClip        = 1;
    w->animRequest     = DIVER_ANIM_REQUEST_BLEND;
    work->subState     = work->subState + 1;
}

static void Actor00400_Fn096C0(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (diverClipEnded(arg0)) {
        work->subState = work->subState + 1;
    }
}

#include "../../shared/diver_state7_enter.inc.c"

static void Actor00400_Fn097C8(Task* arg0)
{
    s32              pan;
    s32              sound;
    _Actor00400Work* work;
    _Actor00400Work* w;

    work = arg0->work;
    if (work->hitTaken != 0) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (diverClipEnded(arg0)) {
        w           = arg0->work;
        w->state    = ACTOR_00400_SWIM_STATE_DECIDE;
        w->subState = 0;
    }
}

static void Actor00400_Fn098A8(Task* arg0)
{
    _Actor00400Work* work;

    work              = arg0->work;
    work->animBlend   = 8;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0xE;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->goalY       = (u16)work->waterLevel + 0x64;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    work->critChanceScale = 100;
    work->stateFrames     = 0;
    work->targetPart      = 1;
    work->subState        = work->subState + 1;
}

static void Actor00400_Fn09924(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;
    s32              mode;

    work = arg0->work;
    mode = work->hitTaken;
    work->stateFrames++;
    if (mode == 1) {
        w              = arg0->work;
        w->animBlend   = 2;
        w->animStep    = ANIMATION_RATE_ONE;
        w->animClip    = 0x12;
        w->animRequest = mode;
    } else {
        if (diverClipEnded(arg0)) {
            w              = arg0->work;
            w->animBlend   = 8;
            w->animStep    = ANIMATION_RATE_ONE;
            w->animClip    = 0x10;
            w->animRequest = DIVER_ANIM_REQUEST_BLEND;
        }
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work->critChanceScale = 0;
        work->targetPart      = 4;
        w                     = arg0->work;
        w->state              = ACTOR_00400_SWIM_STATE_DIVE;
        w->subState           = 0;
    }
}

static void Actor00400_Fn09A1C(Task* arg0)
{
    _Actor00400Work* work;

    work                = arg0->work;
    work->neckRetracted = 0;
    work->stateFrames   = 0;
    work->goalY         = (u16)work->waterLevel + 0x64;
    work->subState      = work->subState + 1;
}

static void Actor00400_Fn09A48(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;

    work                = arg0->work;
    work->waypointIndex = 0;
    w                   = arg0->work;
    w->animBlend        = 0xA;
    w->animStep         = ANIMATION_RATE_ONE;
    w->animClip         = 3;
    w->animRequest      = DIVER_ANIM_REQUEST_BLEND;
    work->subState      = work->subState + 1;
}

static void Actor00400_Fn09A8C(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_PATROL || GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09AE0(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    _Actor00400Work* w;

    work              = arg0->work;
    coord             = arg0->extra.tmd->coords;
    coord->coord.t[0] = 0x10E0;
    coord->coord.t[1] = 0x178;
    work->goalY       = 0x178;
    coord->coord.t[2] = -0xDAC;
    work->rotation.vx = 0;
    work->rotation.vy = 0;
    work->rotation.vz = 0;
    w                 = arg0->work;
    w->animStep       = ANIMATION_RATE_ONE;
    w->animClip       = 3;
    w->animRequest    = DIVER_ANIM_REQUEST_RESET;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

static void Actor00400_Fn09B44(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_SWIM) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09B74(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (++work->stateFrames < 0x30) {
        diverStepForward(arg0, 0xA0, work->rotation.vy);
        return;
    }
    work->subState++;
}

static void Actor00400_Fn09BDC(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->command == ACTOR_00400_COMMAND_TUNNEL_PATROL) {
        work->state    = ACTOR_00400_SWIM_STATE_TUNNEL_PATROL;
        work->subState = 0;
    }
}

static void Actor00400_Fn09C04(Task* arg0)
{
    _Actor00400Work* work;
    TaskFuncTable7   fns;

    work = arg0->work;
    fns  = Actor00400_D00178;
    fns.funcs[work->subState](arg0);
}

static void Actor00400_Fn09C84(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A034,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn09CCC(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    _Actor00400Work* w;

    work                = arg0->work;
    coord               = arg0->extra.tmd->coords;
    work->neckRetracted = 1;
    coord->coord.t[0]   = -0x6C0;
    coord->coord.t[1]   = 0x3E8;
    work->goalY         = 0x3E8;
    coord->coord.t[2]   = -0xBB8;
    work->rotation.vx   = 0;
    work->rotation.vy   = 0x800;
    work->rotation.vz   = 0;
    w                   = arg0->work;
    w->animStep         = ANIMATION_RATE_ONE;
    w->animClip         = 3;
    w->animRequest      = DIVER_ANIM_REQUEST_RESET;
    work->stateFrames   = 0;
    work->subState      = work->subState + 1;
}

static void Actor00400_Fn09D3C(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (GameFlag_GetNibble(GAME_FLAG_0EB) == 1) {
        work->subState = 3;
    } else if (work->command == ACTOR_00400_COMMAND_INTRO_SWIM) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09D98(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    _Actor00400Work* work;

    work              = arg0->work;
    count             = work->stateFrames + 1;
    work->stateFrames = count;
    if ((s16)count < 0x30) {
        diverStepForward(arg0, 0x60, work->rotation.vy);
        if (!(work->frameCount & 0xF)) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->subState += 1;
    }
}

static void Actor00400_Fn09E70(Task* arg0)
{
    _Actor00400Work* work;
    GfxCoord*        coord;
    _Actor00400Work* w;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->command == ACTOR_00400_COMMAND_INTRO_SURFACE) {
        work->stateFrames = 0;
        work->goalY       = work->waterLevel;
        coord->coord.t[0] = -0x6C0;
        coord->coord.t[2] = -0x2008;
        work->rotation.vx = 0;
        work->rotation.vy = 0;
        work->rotation.vz = 0;
        w                 = arg0->work;
        w->animBlend      = 8;
        w->animStep       = ANIMATION_RATE_ONE;
        w->animClip       = 1;
        w->animRequest    = DIVER_ANIM_REQUEST_BLEND;
        work->subState    = work->subState + 1;
    } else {
        work->stateFrames = 0;
        coord->coord.t[0] = -0x6C0;
        coord->coord.t[2] = -0x2008;
        work->rotation.vx = 0;
        work->rotation.vy = 0;
        work->rotation.vz = 0;
        coord->coord.t[1] = 0x3E8;
        work->goalY       = 0x3E8;
    }
}

static void Actor00400_Fn09F18(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    _Actor00400Work* work;

    work              = arg0->work;
    count             = work->stateFrames + 1;
    work->stateFrames = count;
    if ((s16)count == 0x26) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54220006;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->stateFrames == 0x30) {
        work->subState += 1;
    }
}

static void Actor00400_Fn09FDC(Task* arg0)
{
    Enemy*           obj;
    _Actor00400Work* work;

    obj = arg0->spawnArg2.pointer;
    if (((_Actor00400Work*)arg0->work)->command == ACTOR_00400_COMMAND_FIGHT) {
        obj->node.state.parts.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work           = arg0->work;
        work->state    = ACTOR_00400_SWIM_STATE_DIVE;
        work->subState = 0;
    }
}

static void Actor00400_Fn0A034(Task* arg0)
{
    Enemy*           obj;
    _Actor00400Work* work;

    obj = arg0->spawnArg2.pointer;
    if (((_Actor00400Work*)arg0->work)->command == ACTOR_00400_COMMAND_FIGHT) {
        obj->node.state.parts.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work           = arg0->work;
        work->state    = ACTOR_00400_SWIM_STATE_DIVE;
        work->subState = 0;
    }
}

#include "../../shared/coord_math_local_to_world.inc.c"

/// First state of the shot's task, entered the frame the shot is spawned:
/// `Actor00400_Fn02D48` flies it afterwards and `diverStrikeTeardown` retires it.
///
/// `task->work` is the `_Actor00400ShotWork` block `Actor00400_SpawnMarker`
/// allocated, and `task->extra` the `TmdObject` whose `coords` is the
/// coordinate the shot is drawn at. That coordinate is re-parented to
/// `gGfxViewCoord` here, and the sphere is linked on it with its two contacts
/// cleared, so the state `Actor00400_Fn02D48` runs can report what the shot
/// touches. The sphere's radius is 0x100, and the vertical speed the spawner
/// stored is replaced by `ACTOR_00400_SHOT_LAUNCH_SPEED_Y`.
///
/// The `task->extra` walk is repeated for `work->child.attackBody.coord` rather than reusing
/// `coord`: the original re-reads it, which is what the second `lw` chain in
/// the target shows.
///
/// `coord` is assigned before `work` on purpose. sched1 emits each load where
/// its source order puts it, and that position is the quantity's `birth`:
/// writing `coord` second lands its `lw` one insn later, shortening its span
/// from 70 to 68 and raising its `QTY_CMP_PRI` from 1428 to 1470 — above the
/// task pointer's 1458 — so local-alloc hands the coordinate `$s1` and the task
/// pointer `$s2` instead of the reverse. See DECOMPILATION_LEARNINGS.md,
/// "A parameter competes in local-alloc on its raw span, not its doubled
/// `REG_LIVE_LENGTH`".
static void Actor00400_Fn0A190(Task* task)
{
    _Actor00400ShotWork* work;
    GfxCoord*            coord;

    coord                                   = task->extra.tmd->coords;
    work                                    = task->work;
    task->killCountdown                     = 0;
    work->frames                            = 0;
    coord->parent                           = &gGfxViewCoord;
    coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    work->child.attackBody.key              = Gp_PackPair(Actor00400_D0FDC0, 1);
    work->child.attackBody.coord            = task->extra.tmd->coords;
    work->child.attackBody.context.contacts = work->contacts;
    work->child.attackBody.pos.vx           = 0;
    work->child.attackBody.pos.vy           = 0;
    work->child.attackBody.pos.vz           = 0;
    work->child.attackBody.radius           = 0x100;
    work->child.attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->child.attackBody);
    Gp_InitRec18Table(work->contacts, 2, 0);
    work->child.attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actorRenderComposeCoord(coord);
    work->velocity.vy = ACTOR_00400_SHOT_LAUNCH_SPEED_Y;
    diverImpactBurst(coord, (u16)work->frames, 0, 0x1300);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

static void Actor00400_Fn0A2F4(Task* arg0)
{
    _Actor00400GroundStainWork* work;
    Enemy*                      enemy;

    work  = arg0->work;
    enemy = work->enemy;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    if ((s16)enemy->hp <= 0) {
        arg0->state++;
    }
}

static void Actor00400_Fn0A364(Task* arg0)
{
    _Actor00400GroundStainWork* work;
    u8                          intensity;

    work = arg0->work;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    intensity       = work->intensity - 1;
    work->intensity = intensity;
    if (intensity == 0) {
        taskKill(arg0);
    }
}

static void Actor00400_Fn0A3D4(Task* arg0)
{
    _Actor00400Work* work;
    SVECTOR*         record;

    work   = arg0->work;
    record = (SVECTOR*)(work->surfaceSpotIndex * sizeof(SVECTOR) + (u32)work->surfaceSpots);
    if (record->pad != 0) {
        record->pad = 0;
    }
    work->stateFrames = 0;
    work->state       = (u16)work->state + 1;
}

static void Actor00400_Fn0A414(Task* arg0)
{
    _Actor00400Work* work;
    u16              frame;

    work              = arg0->work;
    frame             = (u16)work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame >= 0x12D) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}

static void Actor00400_Fn0A468(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A510,
        Actor00400_Fn07400,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn0A4BC(Task* arg0)
{
    _Actor00400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A57C,
        Actor00400_Fn07518,
    };

    states[work->subState](arg0);
}

static void Actor00400_Fn0A510(Task* arg0)
{
    _Actor00400Work* w;
    _Actor00400Work* work;
    u32              random;

    work              = arg0->work;
    random            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->goalY       = work->waterLevel;
    w                 = arg0->work;
    w->animBlend      = 8;
    w->animStep       = ((random >> 16) & 3) + 3;
    w->animClip       = 0x10;
    w->animRequest    = DIVER_ANIM_REQUEST_BLEND;
    gRandomLcgState   = random;
    work->stateFrames = 0;
    work->subState++;
}

static void Actor00400_Fn0A57C(Task* arg0)
{
    _Actor00400Work* work;

    work              = arg0->work;
    work->animBlend   = 2;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0x12;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->stateFrames = 0;
    work->subState++;
}

static void Actor00400_Fn0A5B8(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;

    work = arg0->work;
    if (work->targetDistance < 0x4E2) {
        work->state                                 = ACTOR_00400_STRANDED_STATE_DISCHARGE;
        work->subState                              = 0;
        work->stateHistory[work->stateHistoryIndex] = work->state;
        if (work->stateHistory[0] == work->stateHistory[1] &&
            work->stateHistory[0] == work->stateHistory[2] &&
            work->stateHistory[0] == ACTOR_00400_STRANDED_STATE_DISCHARGE) {
            w                                           = arg0->work;
            w->state                                    = ACTOR_00400_STRANDED_STATE_CRAWL;
            w->subState                                 = 0;
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

static void Actor00400_Fn0A680(Task* arg0)
{
    _Actor00400Work* work;

    work              = arg0->work;
    work->animBlend   = 6;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 6;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->subState++;
}

static void Actor00400_Fn0A6B0(Task* arg0)
{
    _Actor00400Work* work;

    if (diverClipEnded(arg0)) {
        work           = arg0->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

static void Actor00400_Fn0A704(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->targetDistance < 0x3B4) {
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work->subState++;
}

static void Actor00400_Fn0A760(Task* arg0)
{
    _Actor00400Work* work;

    work = arg0->work;
    if (work->targetDistance < 0x3B4) {
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    if (diverClipEnded(arg0)) {
        work           = arg0->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

static void Actor00400_Fn0A7F0(Task* arg0)
{
    _Actor00400Work* work;

    work               = arg0->work;
    work->animBlend    = 6;
    work->animStep     = ANIMATION_RATE_ONE;
    work->animClip     = 9;
    work->animRequest  = DIVER_ANIM_REQUEST_BLEND;
    work->attackFrames = 0x18;
    work->subState++;
}

static void Actor00400_Fn0A82C(Task* arg0)
{
    _Actor00400Work* work;

    if (diverClipEnded(arg0)) {
        work           = arg0->work;
        work->state    = ACTOR_00400_STRANDED_STATE_DECIDE;
        work->subState = 0;
    }
}

static void Actor00400_Fn0A880(Task* arg0)
{
    s32              sound;
    s32              pan;
    _Actor00400Work* work;
    _Actor00400Work* w;

    work  = arg0->work;
    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    w              = arg0->work;
    w->animBlend   = 6;
    w->animStep    = ANIMATION_RATE_ONE;
    w->animClip    = 0xC;
    w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    work->subState++;
}

static void Actor00400_Fn0A940(Task* arg0)
{
    s32              sound;
    s32              pan;
    _Actor00400Work* work;

    work              = arg0->work;
    work->animBlend   = 4;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0xD;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    sound             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}

static void Actor00400_Fn0A9F4(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;

    work                  = arg0->work;
    work->lookDisabled    = 1;
    w                     = arg0->work;
    w->animBlend          = 8;
    w->animStep           = ANIMATION_RATE_ONE;
    w->animClip           = 0xF;
    w->animRequest        = DIVER_ANIM_REQUEST_BLEND;
    work->critChanceScale = 100;
    work->stateFrames     = 0;
    work->subState++;
}

static void Actor00400_Fn0AA40(Task* arg0)
{
    _Actor00400Work* work;
    _Actor00400Work* w;

    work = arg0->work;
    work->stateFrames++;
    if (diverClipEnded(arg0)) {
        w              = arg0->work;
        w->animBlend   = 8;
        w->animStep    = ANIMATION_RATE_ONE;
        w->animClip    = 0x11;
        w->animRequest = DIVER_ANIM_REQUEST_BLEND;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer)) {
        work->critChanceScale = 0;
        work->lookDisabled    = 0;
        w                     = arg0->work;
        w->state              = ACTOR_00400_STRANDED_STATE_DECIDE;
        w->subState           = 0;
    }
}
