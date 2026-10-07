#include "rooms/acropolis_plaza.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libcd.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_310100.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"
#include "rooms/room_common.h"
#include "../../shared/screen_fade.h"

/// Screen geometry shared by the plaza's fade and cinematic bars, in pixels.
enum {
    ACROPOLIS_PLAZA_SCREEN_WIDTH     = 320,
    ACROPOLIS_PLAZA_SCREEN_HEIGHT    = 240,
    ACROPOLIS_PLAZA_LETTERBOX_HEIGHT = 24
};

/// Projection, sweep and fan units of the plaza's placed lights.
enum {
    ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH                 = 17,  // Camera Z / 4
    ACROPOLIS_PLAZA_LIGHT_CULL_X                    = 192, // Absolute centre X in screen pixels; exclusive
    ACROPOLIS_PLAZA_LIGHT_CULL_Y                    = 152, // Absolute centre Y in screen pixels; exclusive
    ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT                = 12,  // Fan samples are Q12
    ACROPOLIS_PLAZA_LIGHT_TURN                      = 0x1000,
    ACROPOLIS_PLAZA_LIGHT_HALF_TURN                 = 0x800,
    ACROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT           = 11,
    ACROPOLIS_PLAZA_LIGHT_YAW_STEP                  = 0x80,
    ACROPOLIS_PLAZA_LIGHT_YAW_MASK                  = ACROPOLIS_PLAZA_LIGHT_TURN - 1,
    ACROPOLIS_PLAZA_LIGHT_SHORT_REACH               = 0x200, // Local +Z world units
    ACROPOLIS_PLAZA_LIGHT_LONG_REACH                = 0x800,
    ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z                = 0xE00,
    ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW       = 0x300,
    ACROPOLIS_PLAZA_LIGHT_FAN_SAMPLES               = 16, // One turn; each wedge spans two samples
    ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR = 0x10000,
    ACROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY    = 0x80,
    ACROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK    = 0x7F,
    ACROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD         = 0x80,
    ACROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK          = 0x7F,
};

/// Translates one rotated light vertex into world space, narrowing at each store.
///
/// Arguments must be stable, side-effect-free `SVECTOR*` and `GfxCoord*`
/// expressions: each is evaluated three times. Low-halfword additions keep the
/// intermediate sums in range before the signed-halfword stores wrap them.
/// Expands to three statements; use only as a statement in a braced block.
#define ACROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(vertex, coordinate) \
    (vertex)->vx += (u16)(coordinate)->workm.t[0];                 \
    (vertex)->vy += (u16)(coordinate)->workm.t[1];                 \
    (vertex)->vz += (u16)(coordinate)->workm.t[2]

/// Queues the eight Gouraud wedges of one additive light glow.
///
/// `centre` is a `DVECTOR` lvalue, `radius` is in pixels, and `depth` is camera
/// Z / 4. RGB arguments supply byte intensities; all value expressions must be
/// stable and side-effect-free, since the loop evaluates them repeatedly.
/// `primitive` and `sampleIndex` must be writable pointer/integer locals; both
/// are overwritten. Samples 0..14 and offsets through +6 stay in the 32-entry
/// Q12 ring. Uses the current packet cursor, display depth shift and ordering
/// table, and queues eight quads plus eight blend commands without bounds checks.
/// Expands to a loop statement; invoke only in a braced block.
#define ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(centre, radius, depth, primitive, sampleIndex, red, green, blue)                                                       \
    for ((sampleIndex) = 0; (sampleIndex) < ACROPOLIS_PLAZA_LIGHT_FAN_SAMPLES; (sampleIndex) += 2) {                                                               \
        (primitive)    = gGpuPrimCursor;                                                                                                                           \
        gGpuPrimCursor = (primitive) + 1;                                                                                                                          \
        setPolyG4((primitive));                                                                                                                                    \
        setRGB0((primitive), 0, 0, 0);                                                                                                                             \
        setRGB1((primitive), 0, 0, 0);                                                                                                                             \
        setRGB2((primitive), (red), (green), (blue));                                                                                                              \
        setRGB3((primitive), 0, 0, 0);                                                                                                                             \
        (primitive)->x0 = (centre).vx + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex) + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                          \
        (primitive)->y0 = (centre).vy + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex)]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                              \
        (primitive)->x1 = (centre).vx + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex) + 5]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                          \
        (primitive)->y1 = (centre).vy + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex) + 1]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                          \
        (primitive)->x2 = (centre).vx;                                                                                                                             \
        (primitive)->y2 = (centre).vy;                                                                                                                             \
        (primitive)->x3 = (centre).vx + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex) + 6]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                          \
        (primitive)->y3 = (centre).vy + (((radius) * D_acropolis_plaza_801987E0[(sampleIndex) + 2]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                          \
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(depth) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), (primitive)); \
        gpuSetPrimitiveBlendMode((primitive), GPU_BLEND_ADD, (depth));                                                                                             \
    }

/// Index of one vertex in `_AcropolisPlazaBeamScratch`.
///
/// The beam is laid out in its beacon's own frame: flat in the XZ plane,
/// leaving the centre along +Z. Each left vertex is followed by its mirror
/// image, so a side index (0 left, 1 right) added to a left vertex selects
/// that side's.
enum {
    ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE,    // Beacon origin and the beam's apex; replaced by the far glow's centre
    ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT,  // Far edge of the beam, at the beam's current length
    ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT,
    ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT, // Widest point of the fringe beside the beam
    ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT,
    ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT, // Where the fringe leaves the apex
    ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT,
    ACROPOLIS_PLAZA_BEAM_VERTEX_COUNT
};

/// Scratch-stack workspace for one frame of a rotating beacon's light beam.
///
/// `vertices` is indexed by the `ACROPOLIS_PLAZA_BEAM_VERTEX_` constants. The
/// centre is projected first, with one perspective transform through
/// `GsWSMATRIX`: `centreScreenPos` and `otz` receive the result, and the beam
/// is drawn only when `otz` exceeds 0x10 and the centre is on screen. The six
/// outline vertices are staged in the beacon's frame, moved into world space
/// in place and projected into `vertexScreenPos`, which is indexed the same
/// way. Each entry there is one GTE screen-XY word, x in the low half and y in
/// the high half. The centre's entry is never written or read, since the
/// centre's screen position is `centreScreenPos`; that those four bytes are
/// entry 0 rather than a separate word rests on the shared index alone.
///
/// The beam triangle and the fringe either side of it all meet at
/// `centreScreenPos`, and `glowRadius` sizes the fan of wedges drawn round it.
/// A long beam ends in a second glow: its centre replaces the beacon origin in
/// the centre vertex, and `centreScreenPos`, `otz` and `glowRadius` are
/// written again for it. That pass uses no outline entry.
///
/// Reserve the complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    s32     vertexScreenPos[ACROPOLIS_PLAZA_BEAM_VERTEX_COUNT]; // Projected outline vertices as packed screen-XY words
    s32     otz;                                                // Projected depth (SZ3 / 4) of the centre; the ordering-table depth, blend depth and divisor for `glowRadius`
    s32     glowRadius;                                         // On-screen radius in pixels of the wedge fan round the centre: a world size divided by `otz`
    SVECTOR vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_COUNT];        // Beam vertices: staged in the beacon's frame, then the world positions supplied to the projection
    DVECTOR centreScreenPos;                                    // Projected centre in screen pixels, stored as one GTE word
} _AcropolisPlazaBeamScratch;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaBeamScratch, 0x60);

/// Scratch-stack workspace for one frame of a light flare.
///
/// The flare's centre is stored in `worldPos` and projected with one
/// perspective transform through `GsWSMATRIX`: `screenPos` and `otz` receive
/// the result, and the flare is drawn only when `otz` exceeds 0x10 and the
/// centre is on screen. Each radius is a world size divided by `otz`, so the
/// flare shrinks with distance. `outerRadius` first sizes the fan of wedges
/// drawn round the centre and is then written again, with `innerRadius`, for
/// the four rays laid over it: the two short rays have their tips at
/// `outerRadius` and their shoulders, a quarter turn either side, at
/// `innerRadius`, and the two long rays double both. A flare turned to its
/// long reach ends in a second glow: `worldPos`, `screenPos`, `otz` and
/// `outerRadius` are written again for a point ahead of the flare, and only a
/// fan is drawn there.
///
/// The accessed words are those of `RoomGlowRadiiScratch`, in the same order.
/// The two unused runs are exactly the size of five packed screen-XY words
/// ahead of `otz` and of four more `SVECTOR`s after `worldPos`, which is the
/// arrangement of `_AcropolisPlazaBeamScratch` with five vertices instead of
/// seven; no access confirms that the block was declared so.
///
/// Reserve the complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    u8      unused0[0x14];  // Reserved with the block but never accessed; role unproven
    s32     otz;            // Projected depth (SZ3 / 4) of the centre; the ordering-table depth, blend depth and divisor for both radii
    s32     outerRadius;    // On-screen radius in pixels of the wedge fan, then of a short ray's tip
    s32     innerRadius;    // On-screen distance in pixels from the centre to a short ray's shoulders
    SVECTOR worldPos;       // Centre in world coordinates, the input to the projection; staged in the flare's frame for the far glow
    u8      unused28[0x20]; // Reserved with the block but never accessed; role unproven
    DVECTOR screenPos;      // Projected centre in screen pixels, stored as one GTE word
} _AcropolisPlazaFlareScratch;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaFlareScratch, 0x4C);

/// Spawn argument of the plaza's streamed-scene task.
///
/// `Task::spawnArg2` points at one of these and the task reads it only while
/// initialising. The opening spawn passes frame 0 with `skipStreamReset`
/// clear, which also requests the stream. A respawn after an interruption
/// passes the scene frame latched when the scene stopped and sets
/// `skipStreamReset`: the task resumes its bookkeeping at that frame and
/// requests no stream until the player next crosses an edge.
///
/// The storage is the spawner's and need only outlive the task's first tick.
typedef struct {
    u16 startFrame;      // Scene frame to resume at; seeds both `CdCmdQueue::sceneFrame` and `movieFrame`
    u16 skipStreamReset; // 0 resets the stream to the frame before `startFrame`; non-zero only clears `movieAtEnd`
} _AcropolisPlazaSceneArg;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaSceneArg, 0x4);

/// `GameActor.movementMode` value this plaza treats as running.
enum { ACROPOLIS_PLAZA_MOVEMENT_RUNNING = 3 };

/// Edge flag on the streamed-scene work.
///
/// Zero means the player is still inside the shot. A walk across the edge
/// stores 1 and a run (`ACROPOLIS_PLAZA_MOVEMENT_RUNNING`) stores 2. The new
/// seek offset is computed only for a walk; a run still changes direction
/// and enqueues.
enum {
    ACROPOLIS_PLAZA_EDGE_NONE = 0,
    ACROPOLIS_PLAZA_EDGE_WALK = 1,
    ACROPOLIS_PLAZA_EDGE_RUN  = 2
};

/// `relX` margins, in world units, and the scene frame with no earlier shot.
///
/// `relX` is the shot position's X minus the player root X. Later frames of
/// this plaza move toward -X, so a large positive value is the forward edge.
/// The back test is strict: the margin itself does not fire.
enum {
    ACROPOLIS_PLAZA_FORWARD_EDGE      = 0xC9,
    ACROPOLIS_PLAZA_BACK_EDGE         = -0x14,
    ACROPOLIS_PLAZA_FIRST_SCENE_FRAME = 1
};

/// Seek scale stored in `frameStep`, and the stream frames one movie frame spans.
///
/// The scene task initialises the scale to the fine step and stores that same
/// value after every seek it computes. Nothing in this overlay stores the
/// coarse step, so the 40-frame arm is present and unreachable.
enum {
    ACROPOLIS_PLAZA_FRAME_STEP_FINE    = 1,
    ACROPOLIS_PLAZA_FRAME_STEP_COARSE  = 2,
    ACROPOLIS_PLAZA_SEEK_FRAMES_FINE   = 10,
    ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE = 40
};

/// Work block for the plaza's streamed-scene task, kept in `Task::work`.
///
/// The task allocates and zeroes one block of this size. While stream sub-id
/// 0..3 is selected it copies that scene frame's row from the 400-entry
/// shot-position table into `shotPos` and sets `relX` from the player's root
/// matrix. Crossing an edge re-seeks the movie: forward selects
/// `forwardSubId`, reverse selects the next sub-id, and the target is counted
/// in `ACROPOLIS_PLAZA_SEEK_FRAMES_FINE` or `ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE`
/// units. This block is distinct from the sequence task's `_AcropolisPlazaSequenceWork`.
typedef struct {
    MATRIX*    playerMtx;    // Borrowed player root matrix; valid while the player actor is live
    Task*      playerTask;   // Borrowed player task, cached once the CD is idle
    GameActor* player;       // That task's actor; running is `ACROPOLIS_PLAZA_MOVEMENT_RUNNING`
    VECTOR3    shotPos;      // Shot position for this scene frame (table row sceneFrame-1; the read is not clamped)
    s32        relX;         // Shot X minus the player root X; compared with the plaza edge margins
    byte       field_1C[2];  // No access established; role unproven
    s16        prevReverse;  // Previous traversal direction (0 forward, 1 reverse), latched when an edge fires
    s16        frameLimit;   // Current stream's playback stop frame, numbered from 1
    s16        fwd;          // Forward-edge flag (0 none, 1 walking, 2 running)
    s16        back;         // Back-edge flag (0 none, 1 walking, 2 running); stays clear on scene frame 1
    u16        forwardSubId; // Sub-id a forward crossing selects; reverse selects the next. Zeroed, then never stored
    byte       field_28[2];  // No access established; role unproven
    s16        prevFwd;      // `fwd` from the previous frame
    s16        prevBack;     // `back` from the previous frame
    s16        frameStep;    // Seek scale (1 = 10 stream frames, 2 = 40). This task only stores 1
    byte       field_30[4];  // No access established; role unproven
} _AcropolisPlazaSceneWork;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaSceneWork, 0x34);

/// Steps of the plaza sequence's event handling, kept in `_AcropolisPlazaSequenceWork::step`.
///
/// The two waiting steps take a latched trigger event and spawn the task that
/// answers it; each running step waits for that task to be killed, respawns
/// the streamed scene and returns to a waiting step. Only the first scene and
/// captions are accepted before the first scene has played. The code moves
/// from a waiting step to the running step after it, and back, by adding and
/// subtracting 1.
enum {
    ACROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE = 0, // Before the first scene: accepts it or a caption
    ACROPOLIS_PLAZA_STEP_RUN_FIRST_SCENE   = 1, // First scene running; its trigger is unlinked when it ends
    ACROPOLIS_PLAZA_STEP_AWAIT_EVENT       = 2, // Accepts every event kind except the first scene
    ACROPOLIS_PLAZA_STEP_RUN_SCENE         = 3, // Stream or final scene running; the final one ends the sequence
    ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE  = 4, // Repeat scene running; advances `repeatVariant` when it ends
    ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION = 5, // Caption running; returns to AWAIT_FIRST_SCENE
    ACROPOLIS_PLAZA_STEP_RUN_CAPTION       = 6  // Caption running; returns to AWAIT_EVENT
};

/// Event kinds of the plaza sequence: `WorldCollisionTrigger::parameter0` of
/// the room's action triggers, latched in `_AcropolisPlazaSequenceWork::eventKind`.
///
/// The three scenes below 3 fire on contact and each takes scripted control
/// of the player. The repeat scene and the captions need the interaction
/// button. A caption's kind is itself the CAP command to run.
enum {
    ACROPOLIS_PLAZA_EVENT_STREAM_SCENE  = 0, // Scene with its own movie stream; its trigger is unlinked afterwards
    ACROPOLIS_PLAZA_EVENT_FIRST_SCENE   = 1, // The only scene accepted first; its trigger is unlinked afterwards
    ACROPOLIS_PLAZA_EVENT_FINAL_SCENE   = 2, // Ends the sequence and leaves the room
    ACROPOLIS_PLAZA_EVENT_REPEAT_SCENE  = 3, // Runs one of three scripts, selected by `repeatVariant`
    ACROPOLIS_PLAZA_EVENT_FIRST_CAPTION = 6  // This kind and every higher one is a CAP command ID
};

/// Work block of the plaza's sequence task, kept in `Task::work`.
///
/// The sequence task owns the room's walk through the streamed plaza scene:
/// it starts the streamed-scene task, fades the ambience against the scene
/// frame, and answers each collision-trigger event by spawning one event task
/// and waiting for it. The task allocates and zeroes one block on its first
/// frame, and kills itself when the allocation fails.
///
/// Every event task receives this block as `Task::spawnArg2`. When its stream
/// handoff comes it stores the current scene frame in `resumeFrame` and kills
/// `sceneTask`; the sequence then respawns the scene from `sceneArg` once the
/// event task is gone. The final scene's task kills `sceneTask` without
/// storing a frame, since no scene follows it. The caption task also reads
/// `eventKind` as its CAP command.
typedef struct {
    byte                    field_0[4];       // No access established; role unproven
    Task*                   playerTask;       // Borrowed player task, cached on the first frame and not read back
    Task*                   sceneTask;        // Running streamed-scene task; an event task kills it at its stream handoff
    Task*                   eventTask;        // Task answering the latched event, polled until killed; first holds the unpolled display-setup task
    _AcropolisPlazaSceneArg sceneArg;         // Spawn argument of `sceneTask`, refilled before every spawn
    u16                     step;             // Event-handling step (`ACROPOLIS_PLAZA_STEP_*`)
    u16                     eventControl;     // Latched trigger `control` word; stored and not read back
    u8                      eventKind;        // Latched trigger `parameter0` (`ACROPOLIS_PLAZA_EVENT_*`); read back as a signed byte
    u8                      eventParameter1;  // Latched trigger `parameter1`; stored and not read back
    u16                     resumeFrame;      // Scene frame latched when the scene was interrupted; the respawned scene starts there
    u16                     repeatVariant;    // Script the next repeat scene runs (0..2); advances after each and stays at 2
    u16                     ambience5Playing; // Non-zero while area sound 5 is started; the three below likewise
    u16                     ambience1Playing; // Area sound 1
    u16                     ambience2Playing; // Area sound 2
    u16                     ambience4Playing; // Area sound 4
} _AcropolisPlazaSequenceWork;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaSequenceWork, 0x28);

/// Work block of the plaza's player-scripting event tasks, kept in `Task::work`.
///
/// The sequence task answers a collision-trigger event of kind 0, 1 or 2 by
/// spawning one of three tasks that take scripted control of the player: each
/// walks the player to a mark, turns them to a heading and then runs its
/// streamed scene. Each allocates and zeroes one block on its first frame, and
/// kills itself when the allocation fails. Only the kind-2 task has timed
/// waits; the other two leave `elapsedFrames` zero.
typedef struct {
    Task* playerTask;    // Borrowed player task the scripted-control messages are sent to; set once on the first frame
    s16   elapsedFrames; // Frames counted in the current timed wait; restarted from 0 when one begins
} _AcropolisPlazaEventWork;
STATIC_ASSERT_SIZEOF(_AcropolisPlazaEventWork, 0x8);

extern s16 D_acropolis_plaza_801987E0[];

/// Gate `func_acropolis_plaza_8017FB50` applies to a pending `WorldCollisionTrigger` event
/// whose id has the sign bit clear; a main-executable global with no module
/// header yet.

/// The three scene `WorldCollisionTrigger` nodes the plaza unlinks: `..._801991F0` when the
/// opening stream hands over, and `..._801991A4` / `..._8019923C` depending on
/// which event kind ended the scene.
extern WorldCollisionTrigger D_acropolis_plaza_801991A4;
extern WorldCollisionTrigger D_acropolis_plaza_801991F0;
extern WorldCollisionTrigger D_acropolis_plaza_8019923C[4];

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases that record uses.

/// Script block the plaza hands to slot 3 as msg 0x3F4 entry 0xB; it lives in
/// the main executable, not in this overlay.

/// The block `func_acropolis_plaza_8017E9A8` runs once its stream reports in.
extern EvsCommand D_acropolis_plaza_80182B24[];

/// The pair of blocks `func_acropolis_plaza_8017E7E4` hands to `evsStartScriptWithSkip`
/// once the streamed scene it waits on has finished.
extern EvsCommand D_acropolis_plaza_80182734[];
extern EvsCommand D_acropolis_plaza_80182A34[];

/// The pair of blocks the opening sequence hands to `evsStartScriptWithSkip` in state 4,
/// and the two it runs on its own with `evsStartScript` in states 10 and 14.
extern EvsCommand D_acropolis_plaza_80182C90[];
extern EvsCommand D_acropolis_plaza_80182F18[];
extern EvsCommand D_acropolis_plaza_801830DC[];
extern EvsCommand D_acropolis_plaza_801834B4[];

/// The three blocks `func_acropolis_plaza_8017F48C` picks between with
/// `Task::spawnArg1` before handing one to `evsStartScript`.
extern EvsCommand D_acropolis_plaza_80183554[];
extern EvsCommand D_acropolis_plaza_8018365C[];
extern EvsCommand D_acropolis_plaza_80183764[];

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.

/// The plaza's view tables, one per camera set. Each entry is a *pair* of
/// `ViewCamera`s -- the two shots the stream alternates between -- indexed by
/// `gCdCmdQueue.sceneFrame - 1`, so a table row is 0x48 bytes.
extern ViewCamera D_acropolis_plaza_801838B8[][2];
extern ViewCamera D_acropolis_plaza_8018A938[][2];
extern ViewCamera D_acropolis_plaza_8018CAFC[][2];
extern ViewCamera D_acropolis_plaza_8018F530[][2];
extern ViewCamera D_acropolis_plaza_8018F9B4[][2];

/// The plaza's camera table: one world position per stream view, indexed by
/// `gCdCmdQueue.sceneFrame - 1`.
extern VECTOR3 D_acropolis_plaza_801907C4[];

/// Ambient-effect anchor points, one `SVECTOR` per effect slot. The plaza's
/// three effect bursts index this table with the same slot number they pass to
/// `effectSpawn`, so entries 1-6, 7-0xA and 0xC-0x12 belong to the 0x60098,
/// 0x60099 and 0x60096 flavours respectively. The block runs well past entry
/// 0x12, so this declaration is left unsized.
extern SVECTOR D_acropolis_plaza_80198820[];

static void _acropolisPlazaFadeToWhiteTask(Task* task);
void        func_acropolis_plaza_8017DBFC(Task*);
void        func_acropolis_plaza_8017DFE0(Task*);
void        func_acropolis_plaza_8017E7E4(Task*);
void        func_acropolis_plaza_8017E9A8(Task*);
void        func_acropolis_plaza_8017ECF8(Task*);
void        func_acropolis_plaza_8017F48C(Task*);
void        func_acropolis_plaza_8017F620(Task*);
static void _acropolisPlazaLetterboxTask(Task* task);
void        func_acropolis_plaza_80180054(Task*);
void        func_acropolis_plaza_80180270(Task*);

extern WorldCollisionGrid   D_acropolis_plaza_80199180[1];
extern WorldCoordRoomLights D_acropolis_plaza_80199EE8[1];

static SVECTOR _gAcropolisPlazaCollision1BBC0Normals[30];
static SVECTOR _gAcropolisPlazaCollision1BBC0Verts[80];

AnimationPlayRequest D_acropolis_plaza_8018261C = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182630 = { { .index = 2 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182644 = { { .index = 2 }, 2, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_acropolis_plaza_80182658 = { { .index = 2 }, 3, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_8018266C = { { .index = 2 }, 4, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182680 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182694 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_801826A8 = { { .index = 2 }, 6, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_801826BC = { { .index = 2 }, 7, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_801826D0 = { { .index = 2 }, 8, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_801826E4 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_801826F8 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_8018270C = { 0 };

AnimationPlayRequest D_acropolis_plaza_80182720 = { { .index = 0 }, 0, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_plaza_80182734[32] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .message = { .pointer = &D_acropolis_plaza_80182630 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018270C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018261C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182630 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_801826F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182644 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018266C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182680 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182694 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_801826A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_801826BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_801826D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_801826E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_80182A34[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_80182B24[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_acropolis_plaza_80182BB4 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182BC8 = { { .index = 1 }, 2, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182BDC = { { .index = 1 }, 5, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182BF0 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C04 = { { .index = 1 }, 6, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C18 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C2C = { 0 };

AnimationPlayRequest D_acropolis_plaza_80182C40 = { { .index = 1 }, 3, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C54 = { { .index = 0 }, 0, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C68 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182C7C = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_plaza_80182C90[27] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .message = { .pointer = &D_acropolis_plaza_80182BC8 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182BB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182BDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182BF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 118 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182C54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_80182F18[6] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_acropolis_plaza_80182FA8 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182FBC = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182FD0 = { { .index = 1 }, 13, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182FE4 = { { .index = 1 }, 12, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80182FF8 = { { .index = 1 }, 14, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_8018300C = { { .index = 2 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183020 = { { .index = 2 }, 15, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183034 = { { .index = 2 }, 16, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183048 = { { .index = 2 }, 17, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_8018305C = { { .index = 2 }, 23, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183070 = { { .index = 2 }, 19, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183084 = { { .index = 2 }, 21, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183098 = { { .index = 2 }, 22, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_plaza_801830AC = { { 0x489E, 0, 4040, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_plaza_801830C4 = { { 0x4CAE, 0, 4250, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_acropolis_plaza_801830DC[41] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2002 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2002 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .message = { .pointer = &D_acropolis_plaza_80182FA8 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .message = { .pointer = &D_acropolis_plaza_8018300C } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_plaza_801830AC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_plaza_801830C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FA8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018300C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018305C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183070 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183084 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182FF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183098 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2007 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_801834B4[5] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_acropolis_plaza_8018352C = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_plaza_80183540 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_plaza_80183554[11] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .message = { .pointer = &D_acropolis_plaza_8018352C } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_8018352C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_8018365C[11] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .message = { .pointer = &D_acropolis_plaza_80183540 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80183540 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_plaza_80183764[8] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_plaza_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_acropolis_plaza_80183824[12] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_80180054, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017DFE0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017E9A8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017ECF8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017E7E4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_80180270, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017F48C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaFadeToWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017F620, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017DBFC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaLetterboxTask, { .value = 0 } },
};

s32 D_acropolis_plaza_801838B4 = 800;

ViewCamera D_acropolis_plaza_801838B8[400][2] = {
    { { { { { 3860, 0, 1369 }, { 1229, 1799, -3467 }, { -601, 3679, 1695 } }, { -0x5DD4, 6009, 1112 } }, 289 }, { { { { 3860, 0, 1368 }, { 1229, 1800, -3467 }, { -601, 3678, 1697 } }, { -0x5DD1, 6009, 1115 } }, 289 } },
    { { { { { 3860, 0, 1367 }, { 1228, 1802, -3466 }, { -602, 3677, 1699 } }, { -0x5DCB, 6009, 1119 } }, 289 }, { { { { 3861, 0, 1366 }, { 1226, 1806, -3465 }, { -602, 3676, 1702 } }, { -0x5DC3, 6009, 1125 } }, 289 } },
    { { { { { 3861, 0, 1365 }, { 1224, 1809, -3464 }, { -603, 3674, 1706 } }, { -0x5DB9, 6009, 1133 } }, 289 }, { { { { 3862, 0, 1363 }, { 1222, 1814, -3462 }, { -604, 3672, 1711 } }, { -0x5DAD, 6009, 1141 } }, 289 } },
    { { { { { 3863, 0, 1361 }, { 1219, 1819, -3460 }, { -604, 3669, 1716 } }, { -0x5D9F, 6009, 1151 } }, 289 }, { { { { 3864, 0, 1358 }, { 1216, 1825, -3458 }, { -605, 3666, 1722 } }, { -0x5D8F, 6008, 1162 } }, 289 } },
    { { { { { 3865, 0, 1355 }, { 1212, 1832, -3456 }, { -606, 3663, 1729 } }, { -0x5D7D, 6008, 1174 } }, 289 }, { { { { 3866, 0, 1352 }, { 1207, 1839, -3454 }, { -607, 3659, 1736 } }, { -0x5D69, 6007, 1187 } }, 289 } },
    { { { { { 3867, 0, 1347 }, { 1203, 1847, -3452 }, { -607, 3655, 1744 } }, { -0x5D53, 6006, 1201 } }, 289 }, { { { { 3869, 0, 1343 }, { 1197, 1855, -3449 }, { -608, 3651, 1752 } }, { -0x5D3B, 6004, 1215 } }, 289 } },
    { { { { { 3871, 0, 1338 }, { 1191, 1864, -3447 }, { -609, 3647, 1761 } }, { -0x5D21, 6003, 1230 } }, 289 }, { { { { 3873, 0, 1333 }, { 1185, 1873, -3444 }, { -609, 3642, 1771 } }, { -0x5D06, 6001, 1246 } }, 289 } },
    { { { { { 3875, 0, 1327 }, { 1178, 1882, -3441 }, { -609, 3637, 1780 } }, { -0x5CE9, 5999, 1262 } }, 289 }, { { { { 3877, 0, 1321 }, { 1171, 1892, -3438 }, { -610, 3632, 1791 } }, { -0x5CCB, 5997, 1279 } }, 289 } },
    { { { { { 3879, 0, 1314 }, { 1164, 1902, -3435 }, { -610, 3627, 1801 } }, { -0x5CAA, 5994, 1296 } }, 289 }, { { { { 3881, 0, 1307 }, { 1156, 1912, -3432 }, { -610, 3621, 1812 } }, { -0x5C89, 5992, 1313 } }, 289 } },
    { { { { { 3884, 0, 1300 }, { 1148, 1923, -3429 }, { -610, 3616, 1824 } }, { -0x5C66, 5989, 1331 } }, 289 }, { { { { 3886, 0, 1293 }, { 1139, 1934, -3425 }, { -610, 3610, 1835 } }, { -0x5C42, 5985, 1350 } }, 289 } },
    { { { { { 3889, 0, 1285 }, { 1131, 1945, -3422 }, { -610, 3604, 1847 } }, { -0x5C1D, 5982, 1368 } }, 289 }, { { { { 3891, 0, 1277 }, { 1122, 1957, -3418 }, { -610, 3598, 1859 } }, { -0x5BF7, 5978, 1387 } }, 289 } },
    { { { { { 3894, 0, 1269 }, { 1113, 1968, -3414 }, { -610, 3591, 1871 } }, { -0x5BCF, 5974, 1406 } }, 289 }, { { { { 3897, 0, 1261 }, { 1103, 1980, -3411 }, { -609, 3585, 1884 } }, { -0x5BA7, 5970, 1425 } }, 289 } },
    { { { { { 3899, 0, 1252 }, { 1094, 1992, -3407 }, { -609, 3578, 1897 } }, { -0x5B7E, 5966, 1444 } }, 289 }, { { { { 3902, 0, 1243 }, { 1084, 2004, -3403 }, { -608, 3572, 1909 } }, { -0x5B54, 5961, 1463 } }, 289 } },
    { { { { { 3905, 0, 1235 }, { 1075, 2016, -3399 }, { -608, 3565, 1922 } }, { -0x5B29, 5957, 1482 } }, 289 }, { { { { 3908, 0, 1226 }, { 1065, 2028, -3395 }, { -607, 3558, 1935 } }, { -0x5AFE, 5952, 1501 } }, 289 } },
    { { { { { 3910, 0, 1217 }, { 1055, 2040, -3390 }, { -606, 3551, 1948 } }, { -0x5AD2, 5947, 1520 } }, 289 }, { { { { 3913, 0, 1208 }, { 1045, 2052, -3386 }, { -605, 3544, 1961 } }, { -0x5AA6, 5941, 1539 } }, 289 } },
    { { { { { 3916, 0, 1199 }, { 1035, 2065, -3382 }, { -604, 3537, 1974 } }, { -0x5A79, 5936, 1558 } }, 289 }, { { { { 3919, 0, 1190 }, { 1025, 2077, -3377 }, { -603, 3530, 1987 } }, { -0x5A4C, 5931, 1577 } }, 289 } },
    { { { { { 3922, 0, 1180 }, { 1015, 2089, -3373 }, { -602, 3523, 2000 } }, { -0x5A1F, 5925, 1595 } }, 289 }, { { { { 3924, 0, 1171 }, { 1005, 2101, -3369 }, { -601, 3515, 2013 } }, { -0x59F2, 5919, 1614 } }, 289 } },
    { { { { { 3927, 0, 1162 }, { 995, 2113, -3364 }, { -599, 3508, 2026 } }, { -0x59C4, 5913, 1632 } }, 289 }, { { { { 3930, 0, 1153 }, { 985, 2124, -3360 }, { -598, 3501, 2038 } }, { -0x5997, 5907, 1649 } }, 289 } },
    { { { { { 3932, 0, 1144 }, { 976, 2136, -3355 }, { -596, 3494, 2051 } }, { -0x596A, 5901, 1667 } }, 289 }, { { { { 3935, 0, 1134 }, { 966, 2148, -3351 }, { -595, 3487, 2063 } }, { -0x593D, 5895, 1684 } }, 289 } },
    { { { { { 3938, 0, 1125 }, { 956, 2159, -3346 }, { -593, 3480, 2076 } }, { -0x5910, 5889, 1700 } }, 289 }, { { { { 3940, 0, 1116 }, { 947, 2170, -3342 }, { -591, 3473, 2088 } }, { -0x58E4, 5883, 1717 } }, 289 } },
    { { { { { 3943, 0, 1107 }, { 937, 2181, -3337 }, { -589, 3466, 2100 } }, { -0x58B8, 5876, 1733 } }, 289 }, { { { { 3945, 0, 1098 }, { 928, 2191, -3333 }, { -588, 3460, 2111 } }, { -0x588C, 5870, 1748 } }, 289 } },
    { { { { { 3948, 0, 1090 }, { 919, 2202, -3328 }, { -586, 3453, 2122 } }, { -0x5861, 5864, 1763 } }, 289 }, { { { { 3950, 0, 1081 }, { 909, 2212, -3324 }, { -584, 3447, 2134 } }, { -0x5837, 5857, 1777 } }, 289 } },
    { { { { { 3953, 0, 1072 }, { 901, 2222, -3320 }, { -582, 3440, 2144 } }, { -0x580E, 5851, 1790 } }, 289 }, { { { { 3955, 0, 1064 }, { 892, 2231, -3316 }, { -579, 3434, 2155 } }, { -0x57E5, 5844, 1804 } }, 289 } },
    { { { { { 3957, 0, 1055 }, { 883, 2241, -3312 }, { -577, 3428, 2165 } }, { -0x57BE, 5838, 1816 } }, 289 }, { { { { 3959, 0, 1047 }, { 875, 2250, -3308 }, { -575, 3422, 2175 } }, { -0x5797, 5832, 1828 } }, 289 } },
    { { { { { 3961, 0, 1039 }, { 867, 2258, -3305 }, { -573, 3416, 2184 } }, { -0x5772, 5826, 1839 } }, 289 }, { { { { 3964, 0, 1031 }, { 859, 2266, -3301 }, { -570, 3411, 2193 } }, { -0x574E, 5820, 1850 } }, 289 } },
    { { { { { 3966, 0, 1023 }, { 851, 2274, -3298 }, { -568, 3406, 2202 } }, { -0x572A, 5813, 1860 } }, 289 }, { { { { 3968, 0, 1015 }, { 843, 2282, -3294 }, { -566, 3400, 2211 } }, { -0x5706, 5807, 1870 } }, 289 } },
    { { { { { 3970, 0, 1007 }, { 835, 2290, -3291 }, { -563, 3395, 2220 } }, { -0x56E3, 5801, 1880 } }, 289 }, { { { { 3972, 0, 999 }, { 827, 2298, -3287 }, { -561, 3390, 2229 } }, { -0x56BF, 5795, 1889 } }, 289 } },
    { { { { { 3974, 0, 992 }, { 819, 2306, -3283 }, { -558, 3384, 2237 } }, { -0x569C, 5789, 1899 } }, 289 }, { { { { 3976, 0, 984 }, { 811, 2314, -3280 }, { -556, 3379, 2246 } }, { -0x5679, 5783, 1909 } }, 289 } },
    { { { { { 3977, 0, 976 }, { 804, 2322, -3276 }, { -553, 3373, 2255 } }, { -0x5655, 5776, 1918 } }, 289 }, { { { { 3979, 0, 968 }, { 796, 2330, -3273 }, { -550, 3368, 2264 } }, { -0x5632, 5770, 1928 } }, 289 } },
    { { { { { 3981, 0, 960 }, { 788, 2338, -3269 }, { -548, 3363, 2273 } }, { -0x560E, 5764, 1937 } }, 289 }, { { { { 3983, 0, 952 }, { 780, 2346, -3265 }, { -545, 3357, 2281 } }, { -0x55EB, 5757, 1946 } }, 289 } },
    { { { { { 3985, 0, 944 }, { 772, 2353, -3261 }, { -542, 3352, 2290 } }, { -0x55C8, 5751, 1955 } }, 289 }, { { { { 3987, 0, 936 }, { 765, 2361, -3257 }, { -539, 3346, 2299 } }, { -0x55A4, 5744, 1964 } }, 289 } },
    { { { { { 3989, 0, 928 }, { 757, 2369, -3254 }, { -537, 3341, 2307 } }, { -0x5581, 5738, 1972 } }, 289 }, { { { { 3991, 0, 920 }, { 749, 2377, -3250 }, { -534, 3335, 2316 } }, { -0x555D, 5731, 1981 } }, 289 } },
    { { { { { 3993, 0, 912 }, { 741, 2384, -3246 }, { -531, 3330, 2324 } }, { -0x553A, 5724, 1989 } }, 289 }, { { { { 3994, 0, 904 }, { 734, 2392, -3242 }, { -528, 3324, 2333 } }, { -0x5516, 5717, 1997 } }, 289 } },
    { { { { { 3996, 0, 896 }, { 726, 2400, -3238 }, { -525, 3319, 2341 } }, { -0x54F3, 5710, 2005 } }, 289 }, { { { { 3998, 0, 888 }, { 718, 2407, -3234 }, { -522, 3313, 2350 } }, { -0x54CF, 5703, 2013 } }, 289 } },
    { { { { { 4000, 0, 880 }, { 710, 2415, -3230 }, { -518, 3308, 2358 } }, { -0x54AB, 5696, 2021 } }, 289 }, { { { { 4002, 0, 871 }, { 703, 2422, -3226 }, { -515, 3302, 2367 } }, { -0x5487, 5689, 2028 } }, 289 } },
    { { { { { 4003, 0, 863 }, { 695, 2430, -3222 }, { -512, 3297, 2375 } }, { -0x5464, 5682, 2035 } }, 289 }, { { { { 4005, 0, 855 }, { 687, 2437, -3218 }, { -509, 3291, 2384 } }, { -0x5440, 5675, 2042 } }, 289 } },
    { { { { { 4007, 0, 847 }, { 679, 2445, -3214 }, { -505, 3286, 2392 } }, { -0x541C, 5667, 2049 } }, 289 }, { { { { 4009, 0, 839 }, { 671, 2452, -3210 }, { -502, 3280, 2400 } }, { -0x53F8, 5660, 2056 } }, 289 } },
    { { { { { 4010, 0, 830 }, { 664, 2459, -3206 }, { -498, 3275, 2408 } }, { -0x53D3, 5652, 2062 } }, 289 }, { { { { 4012, 0, 822 }, { 656, 2467, -3202 }, { -495, 3269, 2417 } }, { -0x53AF, 5644, 2068 } }, 289 } },
    { { { { { 4014, 0, 814 }, { 648, 2474, -3198 }, { -491, 3264, 2425 } }, { -0x538B, 5636, 2074 } }, 289 }, { { { { 4015, 0, 805 }, { 640, 2481, -3194 }, { -488, 3258, 2433 } }, { -0x5366, 5628, 2079 } }, 289 } },
    { { { { { 4017, 0, 797 }, { 633, 2488, -3190 }, { -484, 3253, 2441 } }, { -0x5342, 5620, 2084 } }, 289 }, { { { { 4019, 0, 788 }, { 625, 2496, -3186 }, { -480, 3247, 2449 } }, { -0x531D, 5612, 2089 } }, 289 } },
    { { { { { 4020, 0, 780 }, { 617, 2503, -3182 }, { -476, 3242, 2457 } }, { -0x52F8, 5603, 2094 } }, 289 }, { { { { 4022, 0, 771 }, { 609, 2510, -3178 }, { -472, 3236, 2465 } }, { -0x52D3, 5595, 2098 } }, 289 } },
    { { { { { 4024, 0, 763 }, { 602, 2517, -3174 }, { -469, 3231, 2472 } }, { -0x52AE, 5586, 2102 } }, 289 }, { { { { 4025, 0, 754 }, { 594, 2523, -3170 }, { -465, 3225, 2480 } }, { -0x5289, 5577, 2106 } }, 289 } },
    { { { { { 4027, 0, 746 }, { 586, 2530, -3166 }, { -461, 3220, 2488 } }, { -0x5264, 5568, 2109 } }, 289 }, { { { { 4029, 0, 737 }, { 579, 2537, -3162 }, { -456, 3215, 2496 } }, { -0x523F, 5559, 2112 } }, 289 } },
    { { { { { 4030, 0, 729 }, { 571, 2544, -3158 }, { -452, 3209, 2503 } }, { -0x5219, 5549, 2115 } }, 289 }, { { { { 4032, 0, 720 }, { 563, 2550, -3154 }, { -448, 3204, 2511 } }, { -0x51F4, 5540, 2117 } }, 289 } },
    { { { { { 4033, 0, 711 }, { 556, 2557, -3150 }, { -444, 3199, 2518 } }, { -0x51CE, 5530, 2119 } }, 289 }, { { { { 4035, 0, 703 }, { 548, 2563, -3146 }, { -440, 3194, 2525 } }, { -0x51A8, 5520, 2120 } }, 289 } },
    { { { { { 4036, 0, 694 }, { 540, 2570, -3142 }, { -435, 3189, 2533 } }, { -0x5182, 5510, 2121 } }, 289 }, { { { { 4038, 0, 686 }, { 533, 2576, -3138 }, { -431, 3183, 2540 } }, { -0x515C, 5500, 2122 } }, 289 } },
    { { { { { 4039, 0, 677 }, { 525, 2583, -3135 }, { -427, 3178, 2547 } }, { -0x5136, 5489, 2122 } }, 289 }, { { { { 4041, 0, 668 }, { 518, 2589, -3131 }, { -422, 3173, 2554 } }, { -0x5110, 5478, 2122 } }, 289 } },
    { { { { { 4042, 0, 660 }, { 510, 2595, -3127 }, { -418, 3168, 2561 } }, { -0x50E9, 5467, 2121 } }, 289 }, { { { { 4043, 0, 651 }, { 503, 2601, -3123 }, { -414, 3163, 2568 } }, { -0x50C3, 5456, 2120 } }, 289 } },
    { { { { { 4045, 0, 643 }, { 496, 2607, -3119 }, { -409, 3158, 2575 } }, { -0x509C, 5445, 2118 } }, 289 }, { { { { 4046, 0, 635 }, { 489, 2613, -3115 }, { -405, 3154, 2581 } }, { -0x5075, 5433, 2116 } }, 289 } },
    { { { { { 4047, 0, 626 }, { 481, 2619, -3112 }, { -400, 3149, 2588 } }, { -0x504E, 5422, 2113 } }, 289 }, { { { { 4049, 0, 618 }, { 474, 2624, -3108 }, { -396, 3144, 2594 } }, { -0x5028, 5410, 2110 } }, 289 } },
    { { { { { 4050, 0, 610 }, { 467, 2630, -3104 }, { -391, 3139, 2601 } }, { -0x5001, 5397, 2106 } }, 289 }, { { { { 4051, 0, 602 }, { 460, 2636, -3100 }, { -387, 3134, 2607 } }, { -0x4FDA, 5385, 2103 } }, 289 } },
    { { { { { 4052, 0, 593 }, { 453, 2641, -3096 }, { -383, 3130, 2614 } }, { -0x4FB3, 5373, 2099 } }, 289 }, { { { { 4053, 0, 585 }, { 447, 2647, -3093 }, { -378, 3125, 2620 } }, { -0x4F8C, 5361, 2096 } }, 289 } },
    { { { { { 4055, 0, 577 }, { 440, 2653, -3088 }, { -374, 3120, 2627 } }, { -0x4F65, 5350, 2093 } }, 289 }, { { { { 4056, 0, 570 }, { 433, 2659, -3084 }, { -370, 3115, 2633 } }, { -0x4F3E, 5338, 2090 } }, 289 } },
    { { { { { 4057, 0, 562 }, { 426, 2665, -3080 }, { -365, 3110, 2640 } }, { -0x4F17, 5326, 2087 } }, 289 }, { { { { 4058, 0, 554 }, { 420, 2671, -3076 }, { -361, 3105, 2646 } }, { -0x4EF0, 5314, 2084 } }, 289 } },
    { { { { { 4059, 0, 546 }, { 413, 2677, -3072 }, { -357, 3100, 2653 } }, { -0x4EC9, 5302, 2081 } }, 289 }, { { { { 4060, 0, 539 }, { 407, 2683, -3067 }, { -353, 3094, 2659 } }, { -0x4EA2, 5290, 2078 } }, 289 } },
    { { { { { 4061, 0, 531 }, { 401, 2688, -3063 }, { -349, 3089, 2666 } }, { -0x4E7A, 5278, 2075 } }, 289 }, { { { { 4062, 0, 524 }, { 394, 2694, -3059 }, { -344, 3084, 2672 } }, { -0x4E53, 5266, 2071 } }, 289 } },
    { { { { { 4063, 0, 516 }, { 388, 2700, -3054 }, { -340, 3079, 2679 } }, { -0x4E2C, 5254, 2068 } }, 289 }, { { { { 4064, 0, 508 }, { 382, 2706, -3050 }, { -336, 3074, 2685 } }, { -0x4E05, 5242, 2064 } }, 289 } },
    { { { { { 4065, 0, 501 }, { 375, 2712, -3045 }, { -332, 3069, 2692 } }, { -0x4DDE, 5230, 2060 } }, 289 }, { { { { 4066, 0, 493 }, { 369, 2718, -3041 }, { -327, 3063, 2698 } }, { -0x4DB6, 5217, 2056 } }, 289 } },
    { { { { { 4067, 0, 486 }, { 363, 2724, -3037 }, { -323, 3058, 2705 } }, { -0x4D8F, 5205, 2052 } }, 289 }, { { { { 4067, 0, 478 }, { 356, 2730, -3032 }, { -319, 3053, 2711 } }, { -0x4D68, 5193, 2047 } }, 289 } },
    { { { { { 4068, 0, 471 }, { 350, 2735, -3027 }, { -314, 3048, 2717 } }, { -0x4D40, 5180, 2042 } }, 289 }, { { { { 4069, 0, 463 }, { 344, 2741, -3023 }, { -310, 3043, 2724 } }, { -0x4D19, 5167, 2037 } }, 289 } },
    { { { { { 4070, 0, 455 }, { 338, 2747, -3018 }, { -305, 3037, 2730 } }, { -0x4CF1, 5154, 2031 } }, 289 }, { { { { 4071, 0, 448 }, { 331, 2753, -3014 }, { -301, 3032, 2736 } }, { -0x4CC9, 5141, 2026 } }, 289 } },
    { { { { { 4072, 0, 440 }, { 325, 2758, -3009 }, { -296, 3027, 2742 } }, { -0x4CA2, 5128, 2019 } }, 289 }, { { { { 4073, 0, 432 }, { 319, 2764, -3005 }, { -292, 3022, 2749 } }, { -0x4C7A, 5114, 2012 } }, 289 } },
    { { { { { 4073, 0, 424 }, { 312, 2770, -3000 }, { -287, 3017, 2755 } }, { -0x4C52, 5101, 2005 } }, 289 }, { { { { 4074, 0, 417 }, { 306, 2775, -2996 }, { -282, 3012, 2761 } }, { -0x4C2A, 5087, 1997 } }, 289 } },
    { { { { { 4075, 0, 409 }, { 300, 2781, -2991 }, { -277, 3006, 2767 } }, { -0x4C02, 5073, 1989 } }, 289 }, { { { { 4076, 0, 401 }, { 294, 2786, -2987 }, { -273, 3001, 2773 } }, { -0x4BDA, 5058, 1980 } }, 289 } },
    { { { { { 4077, 0, 393 }, { 287, 2791, -2983 }, { -268, 2997, 2779 } }, { -0x4BB2, 5044, 1971 } }, 289 }, { { { { 4077, 0, 385 }, { 281, 2797, -2978 }, { -263, 2992, 2784 } }, { -0x4B8A, 5029, 1961 } }, 289 } },
    { { { { { 4078, 0, 377 }, { 275, 2802, -2974 }, { -258, 2987, 2790 } }, { -0x4B62, 5014, 1950 } }, 289 }, { { { { 4079, 0, 369 }, { 269, 2807, -2970 }, { -253, 2982, 2795 } }, { -0x4B3A, 4999, 1938 } }, 289 } },
    { { { { { 4079, 0, 362 }, { 263, 2812, -2966 }, { -248, 2977, 2801 } }, { -0x4B12, 4983, 1925 } }, 289 }, { { { { 4080, 0, 354 }, { 257, 2817, -2962 }, { -243, 2973, 2806 } }, { -0x4AEB, 4967, 1912 } }, 289 } },
    { { { { { 4081, 0, 347 }, { 251, 2821, -2958 }, { -239, 2969, 2811 } }, { -0x4AC3, 4950, 1897 } }, 289 }, { { { { 4081, 0, 339 }, { 246, 2826, -2954 }, { -234, 2964, 2816 } }, { -0x4A9C, 4934, 1881 } }, 289 } },
    { { { { { 4082, 0, 332 }, { 240, 2830, -2951 }, { -230, 2960, 2820 } }, { -0x4A75, 4917, 1864 } }, 289 }, { { { { 4082, 0, 326 }, { 235, 2834, -2947 }, { -225, 2956, 2825 } }, { -0x4A4E, 4899, 1846 } }, 289 } },
    { { { { { 4083, 0, 320 }, { 230, 2838, -2944 }, { -221, 2953, 2829 } }, { -0x4A28, 4881, 1827 } }, 289 }, { { { { 4083, 0, 314 }, { 226, 2841, -2941 }, { -218, 2949, 2833 } }, { -0x4A02, 4863, 1806 } }, 289 } },
    { { { { { 4084, 0, 309 }, { 222, 2844, -2938 }, { -215, 2946, 2836 } }, { -0x49DD, 4844, 1783 } }, 289 }, { { { { 4084, 0, 305 }, { 219, 2847, -2935 }, { -212, 2944, 2839 } }, { -0x49B8, 4825, 1759 } }, 289 } },
    { { { { { 4084, 0, 302 }, { 217, 2850, -2933 }, { -210, 2941, 2842 } }, { -0x4995, 4806, 1734 } }, 289 }, { { { { 4084, 0, 300 }, { 215, 2852, -2931 }, { -208, 2939, 2844 } }, { -0x4973, 4786, 1706 } }, 289 } },
    { { { { { 4085, 0, 299 }, { 214, 2853, -2930 }, { -208, 2938, 2846 } }, { -0x4952, 4765, 1676 } }, 289 }, { { { { 4084, 0, 300 }, { 215, 2854, -2929 }, { -209, 2937, 2847 } }, { -0x4932, 4745, 1645 } }, 289 } },
    { { { { { 4084, 0, 302 }, { 217, 2855, -2928 }, { -211, 2936, 2847 } }, { -0x4914, 4724, 1612 } }, 289 }, { { { { 4084, 0, 307 }, { 220, 2855, -2927 }, { -214, 2936, 2847 } }, { -0x48F8, 4703, 1576 } }, 289 } },
    { { { { { 4083, 0, 314 }, { 225, 2855, -2927 }, { -219, 2936, 2846 } }, { -0x48DE, 4681, 1539 } }, 289 }, { { { { 4083, 0, 323 }, { 232, 2854, -2928 }, { -225, 2937, 2845 } }, { -0x48C6, 4660, 1500 } }, 289 } },
    { { { { { 4082, 0, 336 }, { 241, 2853, -2928 }, { -234, 2938, 2843 } }, { -0x48B0, 4638, 1458 } }, 289 }, { { { { 4080, 0, 350 }, { 251, 2851, -2929 }, { -244, 2940, 2840 } }, { -0x489C, 4616, 1415 } }, 289 } },
    { { { { { 4079, 0, 368 }, { 264, 2848, -2930 }, { -256, 2942, 2837 } }, { -0x488B, 4594, 1371 } }, 289 }, { { { { 4077, 0, 388 }, { 279, 2846, -2932 }, { -269, 2945, 2833 } }, { -0x487C, 4572, 1325 } }, 289 } },
    { { { { { 4075, 0, 410 }, { 295, 2843, -2933 }, { -285, 2948, 2829 } }, { -0x486F, 4550, 1277 } }, 289 }, { { { { 4072, 0, 435 }, { 313, 2840, -2934 }, { -301, 2950, 2824 } }, { -0x4863, 4529, 1229 } }, 289 } },
    { { { { { 4069, 0, 462 }, { 333, 2837, -2935 }, { -320, 2954, 2819 } }, { -0x4859, 4507, 1180 } }, 289 }, { { { { 4066, 0, 490 }, { 354, 2834, -2935 }, { -339, 2957, 2813 } }, { -0x4851, 4485, 1130 } }, 289 } },
    { { { { { 4062, 0, 521 }, { 377, 2830, -2936 }, { -360, 2960, 2807 } }, { -0x484A, 4464, 1079 } }, 289 }, { { { { 4058, 0, 554 }, { 401, 2826, -2936 }, { -382, 2964, 2800 } }, { -0x4844, 4443, 1027 } }, 289 } },
    { { { { { 4053, 0, 588 }, { 426, 2823, -2936 }, { -405, 2967, 2794 } }, { -0x483F, 4421, 975 } }, 289 }, { { { { 4048, 0, 624 }, { 453, 2819, -2936 }, { -430, 2970, 2786 } }, { -0x483B, 4400, 923 } }, 289 } },
    { { { { { 4042, 0, 662 }, { 481, 2816, -2935 }, { -455, 2974, 2778 } }, { -0x4839, 4379, 870 } }, 289 }, { { { { 4035, 0, 701 }, { 510, 2812, -2933 }, { -481, 2977, 2770 } }, { -0x4837, 4358, 816 } }, 289 } },
    { { { { { 4028, 0, 742 }, { 540, 2808, -2931 }, { -509, 2981, 2762 } }, { -0x4836, 4337, 762 } }, 289 }, { { { { 4020, 0, 784 }, { 571, 2805, -2929 }, { -537, 2984, 2753 } }, { -0x4836, 4316, 708 } }, 289 } },
    { { { { { 4011, 0, 828 }, { 604, 2801, -2926 }, { -566, 2988, 2743 } }, { -0x4837, 4295, 653 } }, 289 }, { { { { 4001, 0, 873 }, { 638, 2798, -2922 }, { -596, 2991, 2733 } }, { -0x4838, 4274, 598 } }, 289 } },
    { { { { { 3991, 0, 920 }, { 672, 2794, -2917 }, { -627, 2994, 2723 } }, { -0x483A, 4253, 543 } }, 289 }, { { { { 3979, 0, 967 }, { 708, 2791, -2912 }, { -659, 2997, 2712 } }, { -0x483C, 4232, 487 } }, 289 } },
    { { { { { 3967, 0, 1016 }, { 744, 2788, -2906 }, { -692, 3000, 2701 } }, { -0x483F, 4212, 431 } }, 289 }, { { { { 3954, 0, 1067 }, { 782, 2785, -2899 }, { -725, 3002, 2689 } }, { -0x4843, 4191, 375 } }, 289 } },
    { { { { { 3940, 0, 1118 }, { 820, 2782, -2891 }, { -759, 3005, 2677 } }, { -0x4847, 4170, 319 } }, 289 }, { { { { 3925, 0, 1170 }, { 859, 2780, -2882 }, { -794, 3007, 2664 } }, { -0x484B, 4150, 263 } }, 289 } },
    { { { { { 3908, 0, 1224 }, { 899, 2777, -2872 }, { -830, 3010, 2650 } }, { -0x484F, 4129, 206 } }, 289 }, { { { { 3891, 0, 1278 }, { 940, 2775, -2861 }, { -866, 3012, 2637 } }, { -0x4854, 4109, 149 } }, 289 } },
    { { { { { 3872, 0, 1334 }, { 981, 2773, -2849 }, { -903, 3013, 2622 } }, { -0x485A, 4089, 93 } }, 289 }, { { { { 3852, 0, 1390 }, { 1023, 2772, -2836 }, { -940, 3015, 2607 } }, { -0x485F, 4068, 36 } }, 289 } },
    { { { { { 3831, 0, 1447 }, { 1065, 2770, -2821 }, { -979, 3016, 2592 } }, { -0x4865, 4048, -20 } }, 289 }, { { { { 3809, 0, 1505 }, { 1108, 2769, -2806 }, { -1017, 3017, 2575 } }, { -0x486A, 4028, -77 } }, 289 } },
    { { { { { 3785, 0, 1563 }, { 1152, 2768, -2789 }, { -1057, 3018, 2559 } }, { -0x4870, 4007, -134 } }, 289 }, { { { { 3760, 0, 1622 }, { 1196, 2768, -2771 }, { -1096, 3018, 2541 } }, { -0x4876, 3987, -191 } }, 289 } },
    { { { { { 3734, 0, 1682 }, { 1240, 2768, -2752 }, { -1137, 3018, 2523 } }, { -0x487C, 3967, -248 } }, 289 }, { { { { 3706, 0, 1742 }, { 1284, 2768, -2731 }, { -1177, 3018, 2505 } }, { -0x4882, 3947, -305 } }, 289 } },
    { { { { { 3677, 0, 1803 }, { 1328, 2769, -2709 }, { -1219, 3018, 2486 } }, { -0x4889, 3927, -362 } }, 289 }, { { { { 3647, 0, 1864 }, { 1373, 2770, -2686 }, { -1260, 3017, 2466 } }, { -0x488F, 3907, -419 } }, 289 } },
    { { { { { 3615, 0, 1925 }, { 1417, 2771, -2662 }, { -1302, 3016, 2445 } }, { -0x4895, 3887, -476 } }, 289 }, { { { { 3581, 0, 1986 }, { 1462, 2773, -2636 }, { -1345, 3014, 2424 } }, { -0x489B, 3867, -533 } }, 289 } },
    { { { { { 3546, 0, 2048 }, { 1506, 2775, -2608 }, { -1387, 3012, 2403 } }, { -0x48A1, 3847, -589 } }, 289 }, { { { { 3510, 0, 2109 }, { 1550, 2777, -2580 }, { -1430, 3010, 2380 } }, { -0x48A7, 3827, -646 } }, 289 } },
    { { { { { 3473, 0, 2171 }, { 1594, 2780, -2550 }, { -1474, 3007, 2357 } }, { -0x48AC, 3807, -702 } }, 289 }, { { { { 3433, 0, 2232 }, { 1637, 2783, -2518 }, { -1517, 3004, 2333 } }, { -0x48B1, 3787, -758 } }, 289 } },
    { { { { { 3393, 0, 2293 }, { 1680, 2787, -2486 }, { -1560, 3001, 2309 } }, { -0x48B6, 3768, -814 } }, 289 }, { { { { 3351, 0, 2354 }, { 1722, 2791, -2452 }, { -1604, 2997, 2284 } }, { -0x48BB, 3748, -870 } }, 289 } },
    { { { { { 3308, 0, 2414 }, { 1764, 2796, -2417 }, { -1648, 2993, 2258 } }, { -0x48C0, 3728, -926 } }, 289 }, { { { { 3264, 0, 2474 }, { 1805, 2801, -2381 }, { -1692, 2988, 2232 } }, { -0x48C4, 3708, -981 } }, 289 } },
    { { { { { 3218, 0, 2533 }, { 1845, 2806, -2344 }, { -1735, 2983, 2205 } }, { -0x48C7, 3689, -1036 } }, 289 }, { { { { 3171, 0, 2591 }, { 1884, 2812, -2306 }, { -1779, 2978, 2177 } }, { -0x48CB, 3669, -1091 } }, 289 } },
    { { { { { 3123, 0, 2649 }, { 1922, 2818, -2266 }, { -1822, 2972, 2149 } }, { -0x48CD, 3649, -1145 } }, 289 }, { { { { 3074, 0, 2706 }, { 1959, 2824, -2226 }, { -1866, 2966, 2120 } }, { -0x48D0, 3629, -1200 } }, 289 } },
    { { { { { 3023, 0, 2762 }, { 1996, 2831, -2184 }, { -1909, 2959, 2090 } }, { -0x48D2, 3610, -1254 } }, 289 }, { { { { 2970, 0, 2819 }, { 2032, 2839, -2141 }, { -1954, 2952, 2059 } }, { -0x48D5, 3590, -1308 } }, 289 } },
    { { { { { 2915, 0, 2876 }, { 2067, 2847, -2096 }, { -1999, 2944, 2026 } }, { -0x48D9, 3571, -1362 } }, 289 }, { { { { 2858, 0, 2933 }, { 2102, 2856, -2049 }, { -2045, 2935, 1993 } }, { -0x48DD, 3552, -1417 } }, 289 } },
    { { { { { 2799, 0, 2989 }, { 2136, 2865, -2000 }, { -2091, 2926, 1958 } }, { -0x48E2, 3533, -1472 } }, 289 }, { { { { 2738, 0, 3045 }, { 2168, 2875, -1950 }, { -2138, 2916, 1922 } }, { -0x48E8, 3514, -1527 } }, 289 } },
    { { { { { 2676, 0, 3100 }, { 2199, 2886, -1898 }, { -2185, 2905, 1886 } }, { -0x48EE, 3495, -1582 } }, 289 }, { { { { 2611, 0, 3155 }, { 2229, 2898, -1845 }, { -2232, 2894, 1848 } }, { -0x48F5, 3476, -1637 } }, 289 } },
    { { { { { 2545, 0, 3208 }, { 2257, 2910, -1791 }, { -2280, 2881, 1809 } }, { -0x48FC, 3458, -1692 } }, 289 }, { { { { 2478, 0, 3260 }, { 2283, 2923, -1735 }, { -2327, 2868, 1769 } }, { -0x4903, 3439, -1747 } }, 289 } },
    { { { { { 2410, 0, 3311 }, { 2307, 2937, -1679 }, { -2375, 2854, 1728 } }, { -0x490B, 3420, -1803 } }, 289 }, { { { { 2340, 0, 3361 }, { 2330, 2952, -1622 }, { -2422, 2839, 1687 } }, { -0x4913, 3402, -1858 } }, 289 } },
    { { { { { 2270, 0, 3409 }, { 2350, 2966, -1565 }, { -2469, 2823, 1644 } }, { -0x491C, 3384, -1913 } }, 289 }, { { { { 2198, 0, 3455 }, { 2368, 2982, -1507 }, { -2516, 2807, 1601 } }, { -0x4924, 3365, -1969 } }, 289 } },
    { { { { { 2126, 0, 3500 }, { 2384, 2998, -1448 }, { -2562, 2790, 1557 } }, { -0x492D, 3347, -2024 } }, 289 }, { { { { 2054, 0, 3543 }, { 2398, 3015, -1390 }, { -2608, 2772, 1512 } }, { -0x4936, 3329, -2079 } }, 289 } },
    { { { { { 1981, 0, 3584 }, { 2409, 3032, -1331 }, { -2654, 2753, 1466 } }, { -0x493F, 3311, -2135 } }, 289 }, { { { { 1908, 0, 3624 }, { 2419, 3050, -1273 }, { -2698, 2733, 1421 } }, { -0x4948, 3293, -2190 } }, 289 } },
    { { { { { 1834, 0, 3662 }, { 2426, 3068, -1215 }, { -2743, 2713, 1374 } }, { -0x4951, 3275, -2245 } }, 289 }, { { { { 1761, 0, 3697 }, { 2430, 3086, -1157 }, { -2786, 2692, 1327 } }, { -0x495A, 3257, -2300 } }, 289 } },
    { { { { { 1688, 0, 3731 }, { 2433, 3105, -1100 }, { -2829, 2670, 1280 } }, { -0x4963, 3239, -2355 } }, 289 }, { { { { 1615, 0, 3763 }, { 2433, 3124, -1044 }, { -2871, 2648, 1232 } }, { -0x496C, 3221, -2410 } }, 289 } },
    { { { { { 1542, 0, 3794 }, { 2432, 3143, -989 }, { -2912, 2625, 1184 } }, { -0x4975, 3203, -2465 } }, 289 }, { { { { 1470, 0, 3822 }, { 2428, 3163, -934 }, { -2952, 2602, 1135 } }, { -0x497E, 3185, -2520 } }, 289 } },
    { { { { { 1399, 0, 3849 }, { 2422, 3182, -880 }, { -2991, 2577, 1087 } }, { -0x4986, 3168, -2575 } }, 289 }, { { { { 1328, 0, 3874 }, { 2415, 3202, -828 }, { -3029, 2553, 1038 } }, { -0x498F, 3150, -2629 } }, 289 } },
    { { { { { 1258, 0, 3897 }, { 2405, 3222, -776 }, { -3066, 2528, 990 } }, { -0x4997, 3132, -2684 } }, 289 }, { { { { 1189, 0, 3919 }, { 2394, 3242, -726 }, { -3102, 2502, 941 } }, { -0x499E, 3114, -2738 } }, 289 } },
    { { { { { 1120, 0, 3939 }, { 2382, 3262, -677 }, { -3137, 2476, 892 } }, { -0x49A5, 3096, -2792 } }, 289 }, { { { { 1053, 0, 3958 }, { 2368, 3282, -630 }, { -3171, 2450, 844 } }, { -0x49AC, 3078, -2846 } }, 289 } },
    { { { { { 986, 0, 3975 }, { 2352, 3301, -584 }, { -3204, 2423, 795 } }, { -0x49B3, 3060, -2900 } }, 289 }, { { { { 921, 0, 3990 }, { 2335, 3321, -539 }, { -3236, 2397, 747 } }, { -0x49B8, 3042, -2954 } }, 289 } },
    { { { { { 857, 0, 4005 }, { 2317, 3340, -496 }, { -3266, 2370, 699 } }, { -0x49BE, 3024, -3007 } }, 289 }, { { { { 794, 0, 4018 }, { 2298, 3359, -454 }, { -3296, 2342, 651 } }, { -0x49C2, 3006, -3060 } }, 289 } },
    { { { { { 732, 0, 4029 }, { 2278, 3378, -413 }, { -3324, 2315, 604 } }, { -0x49C6, 2988, -3113 } }, 289 }, { { { { 671, 0, 4040 }, { 2256, 3397, -375 }, { -3351, 2287, 557 } }, { -0x49CA, 2970, -3166 } }, 289 } },
    { { { { { 612, 0, 4049 }, { 2235, 3415, -337 }, { -3377, 2260, 510 } }, { -0x49CC, 2952, -3218 } }, 289 }, { { { { 553, 0, 4058 }, { 2212, 3433, -301 }, { -3402, 2232, 464 } }, { -0x49CE, 2934, -3270 } }, 289 } },
    { { { { { 496, 0, 4065 }, { 2189, 3451, -267 }, { -3426, 2205, 418 } }, { -0x49CE, 2916, -3322 } }, 289 }, { { { { 441, 0, 4072 }, { 2165, 3468, -234 }, { -3448, 2178, 373 } }, { -0x49CE, 2897, -3373 } }, 289 } },
    { { { { { 386, 0, 4077 }, { 2141, 3485, -203 }, { -3470, 2150, 329 } }, { -0x49CC, 2879, -3424 } }, 289 }, { { { { 333, 0, 4082 }, { 2116, 3502, -173 }, { -3490, 2124, 285 } }, { -0x49C9, 2860, -3474 } }, 289 } },
    { { { { { 282, 0, 4086 }, { 2092, 3518, -144 }, { -3509, 2097, 242 } }, { -0x49C5, 2842, -3524 } }, 289 }, { { { { 231, 0, 4089 }, { 2067, 3533, -117 }, { -3528, 2070, 200 } }, { -0x49C0, 2823, -3573 } }, 289 } },
    { { { { { 183, 0, 4091 }, { 2042, 3548, -91 }, { -3545, 2044, 158 } }, { -0x49B8, 2804, -3621 } }, 289 }, { { { { 135, 0, 4093 }, { 2018, 3563, -66 }, { -3561, 2019, 118 } }, { -0x49B0, 2785, -3669 } }, 289 } },
    { { { { { 90, 0, 4095 }, { 1993, 3577, -43 }, { -3576, 1994, 78 } }, { -0x49A5, 2766, -3716 } }, 289 }, { { { { 45, 0, 4095 }, { 1969, 3591, -22 }, { -3590, 1969, 40 } }, { -0x4999, 2747, -3762 } }, 289 } },
    { { { { { 3, 0, 4095 }, { 1946, 3604, -1 }, { -3604, 1946, 3 } }, { -0x498A, 2728, -3806 } }, 289 }, { { { { -36, 0, 4095 }, { 1922, 3616, 17 }, { -3616, 1923, -32 } }, { -0x497A, 2710, -3850 } }, 289 } },
    { { { { { -75, 0, 4095 }, { 1900, 3628, 35 }, { -3627, 1900, -67 } }, { -0x4967, 2691, -3891 } }, 289 }, { { { { -112, 0, 4094 }, { 1878, 3639, 51 }, { -3638, 1879, -100 } }, { -0x4952, 2672, -3932 } }, 289 } },
    { { { { { -148, 0, 4093 }, { 1856, 3650, 67 }, { -3647, 1858, -132 } }, { -0x493C, 2653, -3971 } }, 289 }, { { { { -182, 0, 4091 }, { 1835, 3660, 81 }, { -3656, 1837, -162 } }, { -0x4925, 2634, -4009 } }, 289 } },
    { { { { { -214, 0, 4090 }, { 1815, 3670, 95 }, { -3665, 1817, -192 } }, { -0x490B, 2616, -4046 } }, 289 }, { { { { -245, 0, 4088 }, { 1795, 3679, 107 }, { -3673, 1798, -220 } }, { -0x48F1, 2598, -4081 } }, 289 } },
    { { { { { -275, 0, 4086 }, { 1776, 3688, 119 }, { -3680, 1780, -248 } }, { -0x48D5, 2580, -4115 } }, 289 }, { { { { -303, 0, 4084 }, { 1757, 3697, 130 }, { -3687, 1762, -274 } }, { -0x48B8, 2562, -4148 } }, 289 } },
    { { { { { -330, 0, 4082 }, { 1738, 3705, 140 }, { -3693, 1744, -299 } }, { -0x489B, 2545, -4179 } }, 289 }, { { { { -356, 0, 4080 }, { 1720, 3713, 150 }, { -3699, 1727, -322 } }, { -0x487C, 2528, -4209 } }, 289 } },
    { { { { { -380, 0, 4078 }, { 1703, 3721, 158 }, { -3705, 1710, -345 } }, { -0x485D, 2511, -4238 } }, 289 }, { { { { -403, 0, 4076 }, { 1686, 3729, 167 }, { -3710, 1694, -367 } }, { -0x483C, 2495, -4266 } }, 289 } },
    { { { { { -426, 0, 4073 }, { 1669, 3736, 174 }, { -3716, 1678, -388 } }, { -0x481C, 2478, -4293 } }, 289 }, { { { { -447, 0, 4071 }, { 1652, 3743, 181 }, { -3720, 1662, -408 } }, { -0x47FA, 2462, -4318 } }, 289 } },
    { { { { { -467, 0, 4069 }, { 1636, 3749, 188 }, { -3725, 1647, -427 } }, { -0x47D9, 2446, -4343 } }, 289 }, { { { { -486, 0, 4066 }, { 1621, 3756, 193 }, { -3729, 1632, -446 } }, { -0x47B7, 2431, -4367 } }, 289 } },
    { { { { { -504, 0, 4064 }, { 1605, 3762, 199 }, { -3734, 1618, -463 } }, { -0x4794, 2415, -4389 } }, 289 }, { { { { -522, 0, 4062 }, { 1590, 3768, 204 }, { -3738, 1603, -480 } }, { -0x4771, 2400, -4411 } }, 289 } },
    { { { { { -539, 0, 4060 }, { 1575, 3774, 209 }, { -3742, 1589, -496 } }, { -0x474E, 2386, -4433 } }, 289 }, { { { { -555, 0, 4058 }, { 1561, 3780, 213 }, { -3745, 1575, -512 } }, { -0x472B, 2371, -4453 } }, 289 } },
    { { { { { -570, 0, 4056 }, { 1547, 3786, 217 }, { -3749, 1562, -527 } }, { -0x4707, 2357, -4473 } }, 289 }, { { { { -585, 0, 4053 }, { 1533, 3791, 221 }, { -3752, 1548, -541 } }, { -0x46E3, 2342, -4492 } }, 289 } },
    { { { { { -599, 0, 4051 }, { 1519, 3797, 224 }, { -3756, 1535, -555 } }, { -0x46BF, 2329, -4510 } }, 289 }, { { { { -612, 0, 4049 }, { 1505, 3802, 227 }, { -3759, 1522, -568 } }, { -0x469B, 2315, -4528 } }, 289 } },
    { { { { { -625, 0, 4047 }, { 1491, 3807, 230 }, { -3762, 1509, -581 } }, { -0x4677, 2301, -4545 } }, 289 }, { { { { -637, 0, 4046 }, { 1478, 3812, 233 }, { -3766, 1496, -593 } }, { -0x4653, 2288, -4561 } }, 289 } },
    { { { { { -649, 0, 4044 }, { 1465, 3817, 235 }, { -3769, 1484, -605 } }, { -0x462F, 2275, -4577 } }, 289 }, { { { { -661, 0, 4042 }, { 1452, 3822, 237 }, { -3772, 1471, -617 } }, { -0x460A, 2261, -4593 } }, 289 } },
    { { { { { -672, 0, 4040 }, { 1439, 3827, 239 }, { -3775, 1459, -628 } }, { -0x45E6, 2249, -4608 } }, 289 }, { { { { -682, 0, 4038 }, { 1427, 3831, 241 }, { -3778, 1447, -638 } }, { -0x45C1, 2236, -4623 } }, 289 } },
    { { { { { -692, 0, 4036 }, { 1414, 3836, 242 }, { -3781, 1435, -649 } }, { -0x459D, 2223, -4637 } }, 289 }, { { { { -702, 0, 4035 }, { 1401, 3840, 244 }, { -3783, 1423, -658 } }, { -0x4578, 2211, -4651 } }, 289 } },
    { { { { { -712, 0, 4033 }, { 1389, 3845, 245 }, { -3786, 1411, -668 } }, { -0x4553, 2198, -4665 } }, 289 }, { { { { -721, 0, 4031 }, { 1377, 3849, 246 }, { -3789, 1399, -677 } }, { -0x452F, 2186, -4678 } }, 289 } },
    { { { { { -730, 0, 4030 }, { 1365, 3853, 247 }, { -3792, 1387, -686 } }, { -0x450A, 2174, -4691 } }, 289 }, { { { { -738, 0, 4028 }, { 1353, 3858, 248 }, { -3794, 1375, -695 } }, { -0x44E5, 2162, -4703 } }, 289 } },
    { { { { { -746, 0, 4027 }, { 1341, 3862, 248 }, { -3797, 1364, -704 } }, { -0x44C0, 2150, -4715 } }, 289 }, { { { { -754, 0, 4025 }, { 1329, 3866, 249 }, { -3799, 1352, -712 } }, { -0x449C, 2138, -4728 } }, 289 } },
    { { { { { -762, 0, 4024 }, { 1317, 3870, 249 }, { -3802, 1341, -720 } }, { -0x4477, 2126, -4739 } }, 289 }, { { { { -769, 0, 4022 }, { 1306, 3874, 249 }, { -3805, 1329, -728 } }, { -0x4452, 2114, -4751 } }, 289 } },
    { { { { { -777, 0, 4021 }, { 1294, 3878, 250 }, { -3807, 1318, -735 } }, { -0x442D, 2103, -4762 } }, 289 }, { { { { -784, 0, 4020 }, { 1282, 3881, 250 }, { -3810, 1307, -743 } }, { -0x4408, 2091, -4774 } }, 289 } },
    { { { { { -791, 0, 4018 }, { 1271, 3885, 250 }, { -3812, 1295, -750 } }, { -0x43E3, 2080, -4785 } }, 289 }, { { { { -798, 0, 4017 }, { 1259, 3889, 250 }, { -3814, 1284, -757 } }, { -0x43BE, 2068, -4796 } }, 289 } },
    { { { { { -804, 0, 4016 }, { 1248, 3893, 250 }, { -3817, 1273, -764 } }, { -0x4399, 2057, -4806 } }, 289 }, { { { { -811, 0, 4014 }, { 1236, 3896, 249 }, { -3819, 1261, -771 } }, { -0x4374, 2045, -4817 } }, 289 } },
    { { { { { -817, 0, 4013 }, { 1225, 3900, 249 }, { -3821, 1250, -778 } }, { -0x434F, 2034, -4828 } }, 289 }, { { { { -824, 0, 4012 }, { 1213, 3904, 249 }, { -3824, 1239, -785 } }, { -0x432A, 2022, -4838 } }, 289 } },
    { { { { { -830, 0, 4010 }, { 1202, 3907, 248 }, { -3826, 1228, -792 } }, { -0x4305, 2011, -4849 } }, 289 }, { { { { -836, 0, 4009 }, { 1191, 3911, 248 }, { -3828, 1216, -798 } }, { -0x42E0, 2000, -4859 } }, 289 } },
    { { { { { -842, 0, 4008 }, { 1179, 3914, 247 }, { -3830, 1205, -805 } }, { -0x42BA, 1988, -4870 } }, 289 }, { { { { -847, 0, 4007 }, { 1168, 3917, 247 }, { -3833, 1194, -810 } }, { -0x4295, 1977, -4879 } }, 289 } },
    { { { { { -851, 0, 4006 }, { 1158, 3921, 246 }, { -3835, 1184, -814 } }, { -0x426F, 1966, -4887 } }, 289 }, { { { { -854, 0, 4005 }, { 1148, 3924, 244 }, { -3837, 1174, -818 } }, { -0x424A, 1956, -4895 } }, 289 } },
    { { { { { -856, 0, 4005 }, { 1138, 3927, 243 }, { -3840, 1164, -821 } }, { -0x4224, 1946, -4902 } }, 289 }, { { { { -858, 0, 4005 }, { 1129, 3929, 242 }, { -3842, 1154, -823 } }, { -0x41FE, 1936, -4908 } }, 289 } },
    { { { { { -859, 0, 4004 }, { 1120, 3932, 240 }, { -3845, 1145, -824 } }, { -0x41D9, 1926, -4913 } }, 289 }, { { { { -859, 0, 4004 }, { 1111, 3935, 238 }, { -3847, 1136, -825 } }, { -0x41B3, 1917, -4918 } }, 289 } },
    { { { { { -858, 0, 4005 }, { 1102, 3937, 236 }, { -3850, 1127, -825 } }, { -0x418D, 1907, -4922 } }, 289 }, { { { { -857, 0, 4005 }, { 1094, 3940, 234 }, { -3852, 1119, -824 } }, { -0x4168, 1898, -4926 } }, 289 } },
    { { { { { -855, 0, 4005 }, { 1086, 3942, 232 }, { -3855, 1111, -823 } }, { -0x4142, 1890, -4929 } }, 289 }, { { { { -852, 0, 4006 }, { 1079, 3944, 229 }, { -3858, 1103, -821 } }, { -0x411D, 1881, -4931 } }, 289 } },
    { { { { { -849, 0, 4006 }, { 1071, 3946, 227 }, { -3860, 1095, -818 } }, { -0x40F7, 1873, -4933 } }, 289 }, { { { { -845, 0, 4007 }, { 1064, 3948, 224 }, { -3863, 1087, -815 } }, { -0x40D2, 1864, -4934 } }, 289 } },
    { { { { { -841, 0, 4008 }, { 1057, 3950, 221 }, { -3866, 1080, -811 } }, { -0x40AD, 1856, -4935 } }, 289 }, { { { { -836, 0, 4009 }, { 1050, 3952, 219 }, { -3869, 1073, -807 } }, { -0x4087, 1849, -4935 } }, 289 } },
    { { { { { -831, 0, 4010 }, { 1043, 3954, 216 }, { -3872, 1066, -802 } }, { -0x4062, 1841, -4935 } }, 289 }, { { { { -825, 0, 4012 }, { 1037, 3956, 213 }, { -3875, 1059, -797 } }, { -0x403D, 1833, -4935 } }, 289 } },
    { { { { { -818, 0, 4013 }, { 1031, 3958, 210 }, { -3878, 1052, -791 } }, { -0x4018, 1826, -4934 } }, 289 }, { { { { -812, 0, 4014 }, { 1024, 3960, 207 }, { -3881, 1045, -785 } }, { -0x3FF3, 1819, -4933 } }, 289 } },
    { { { { { -805, 0, 4016 }, { 1018, 3962, 204 }, { -3884, 1039, -778 } }, { -0x3FCE, 1812, -4931 } }, 289 }, { { { { -797, 0, 4017 }, { 1012, 3963, 201 }, { -3887, 1032, -771 } }, { -0x3FAA, 1805, -4929 } }, 289 } },
    { { { { { -789, 0, 4019 }, { 1007, 3965, 197 }, { -3890, 1026, -764 } }, { -0x3F85, 1798, -4927 } }, 289 }, { { { { -781, 0, 4020 }, { 1001, 3966, 194 }, { -3894, 1019, -756 } }, { -0x3F60, 1791, -4925 } }, 289 } },
    { { { { { -772, 0, 4022 }, { 995, 3968, 191 }, { -3897, 1013, -748 } }, { -0x3F3C, 1785, -4922 } }, 289 }, { { { { -764, 0, 4024 }, { 989, 3970, 187 }, { -3900, 1007, -740 } }, { -0x3F17, 1778, -4920 } }, 289 } },
    { { { { { -755, 0, 4025 }, { 984, 3971, 184 }, { -3903, 1001, -732 } }, { -0x3EF3, 1772, -4917 } }, 289 }, { { { { -745, 0, 4027 }, { 978, 3973, 181 }, { -3906, 995, -723 } }, { -0x3ECF, 1765, -4914 } }, 289 } },
    { { { { { -736, 0, 4029 }, { 973, 3974, 177 }, { -3909, 989, -714 } }, { -0x3EAB, 1759, -4911 } }, 289 }, { { { { -726, 0, 4031 }, { 967, 3976, 174 }, { -3913, 983, -705 } }, { -0x3E86, 1753, -4908 } }, 289 } },
    { { { { { -716, 0, 4032 }, { 962, 3977, 170 }, { -3916, 977, -695 } }, { -0x3E62, 1747, -4904 } }, 289 }, { { { { -706, 0, 4034 }, { 956, 3979, 167 }, { -3919, 971, -686 } }, { -0x3E3E, 1740, -4901 } }, 289 } },
    { { { { { -696, 0, 4036 }, { 951, 3980, 164 }, { -3922, 965, -676 } }, { -0x3E1A, 1734, -4897 } }, 289 }, { { { { -685, 0, 4038 }, { 945, 3982, 160 }, { -3925, 959, -666 } }, { -0x3DF6, 1728, -4894 } }, 289 } },
    { { { { { -675, 0, 4039 }, { 940, 3983, 157 }, { -3928, 953, -657 } }, { -0x3DD2, 1722, -4890 } }, 289 }, { { { { -665, 0, 4041 }, { 934, 3984, 153 }, { -3932, 947, -647 } }, { -0x3DAF, 1716, -4887 } }, 289 } },
    { { { { { -654, 0, 4043 }, { 929, 3986, 150 }, { -3935, 941, -637 } }, { -0x3D8B, 1710, -4883 } }, 289 }, { { { { -643, 0, 4045 }, { 923, 3987, 147 }, { -3938, 935, -626 } }, { -0x3D67, 1704, -4880 } }, 289 } },
    { { { { { -633, 0, 4046 }, { 917, 3989, 143 }, { -3941, 928, -616 } }, { -0x3D43, 1698, -4876 } }, 289 }, { { { { -622, 0, 4048 }, { 911, 3990, 140 }, { -3944, 922, -606 } }, { -0x3D1F, 1692, -4873 } }, 289 } },
    { { { { { -612, 0, 4049 }, { 906, 3992, 136 }, { -3947, 916, -596 } }, { -0x3CFC, 1686, -4870 } }, 289 }, { { { { -601, 0, 4051 }, { 900, 3993, 133 }, { -3950, 909, -586 } }, { -0x3CD8, 1680, -4867 } }, 289 } },
    { { { { { -591, 0, 4053 }, { 894, 3995, 130 }, { -3953, 903, -576 } }, { -0x3CB4, 1674, -4864 } }, 289 }, { { { { -581, 0, 4054 }, { 887, 3996, 127 }, { -3956, 896, -567 } }, { -0x3C90, 1668, -4861 } }, 289 } },
    { { { { { -571, 0, 4056 }, { 881, 3998, 124 }, { -3959, 890, -557 } }, { -0x3C6C, 1662, -4859 } }, 289 }, { { { { -561, 0, 4057 }, { 875, 3999, 120 }, { -3961, 883, -547 } }, { -0x3C49, 1655, -4856 } }, 289 } },
    { { { { { -551, 0, 4058 }, { 868, 4001, 117 }, { -3964, 876, -538 } }, { -0x3C25, 1649, -4854 } }, 289 }, { { { { -541, 0, 4060 }, { 861, 4002, 114 }, { -3967, 869, -529 } }, { -0x3C01, 1643, -4852 } }, 289 } },
    { { { { { -532, 0, 4061 }, { 854, 4004, 112 }, { -3970, 862, -520 } }, { -0x3BDD, 1636, -4851 } }, 289 }, { { { { -523, 0, 4062 }, { 847, 4005, 109 }, { -3973, 854, -511 } }, { -0x3BBA, 1630, -4850 } }, 289 } },
    { { { { { -514, 0, 4063 }, { 840, 4007, 106 }, { -3975, 847, -503 } }, { -0x3B96, 1623, -4848 } }, 289 }, { { { { -505, 0, 4064 }, { 833, 4009, 103 }, { -3978, 839, -494 } }, { -0x3B72, 1616, -4847 } }, 289 } },
    { { { { { -495, 0, 4065 }, { 826, 4010, 100 }, { -3981, 832, -485 } }, { -0x3B4F, 1610, -4846 } }, 289 }, { { { { -486, 0, 4067 }, { 818, 4012, 97 }, { -3983, 824, -476 } }, { -0x3B2B, 1603, -4844 } }, 289 } },
    { { { { { -476, 0, 4068 }, { 811, 4013, 95 }, { -3986, 817, -466 } }, { -0x3B08, 1597, -4842 } }, 289 }, { { { { -466, 0, 4069 }, { 804, 4015, 92 }, { -3989, 809, -457 } }, { -0x3AE4, 1591, -4841 } }, 289 } },
    { { { { { -456, 0, 4070 }, { 797, 4016, 89 }, { -3991, 802, -447 } }, { -0x3AC1, 1584, -4839 } }, 289 }, { { { { -446, 0, 4071 }, { 790, 4018, 86 }, { -3994, 794, -438 } }, { -0x3A9E, 1578, -4837 } }, 289 } },
    { { { { { -436, 0, 4072 }, { 782, 4019, 83 }, { -3996, 787, -428 } }, { -0x3A7B, 1572, -4835 } }, 289 }, { { { { -426, 0, 4073 }, { 775, 4021, 81 }, { -3999, 779, -418 } }, { -0x3A58, 1566, -4832 } }, 289 } },
    { { { { { -415, 0, 4074 }, { 768, 4022, 78 }, { -4001, 772, -408 } }, { -0x3A35, 1560, -4830 } }, 289 }, { { { { -405, 0, 4075 }, { 761, 4023, 75 }, { -4004, 765, -397 } }, { -0x3A12, 1554, -4828 } }, 289 } },
    { { { { { -394, 0, 4076 }, { 754, 4025, 72 }, { -4006, 757, -387 } }, { -0x39F0, 1548, -4825 } }, 289 }, { { { { -383, 0, 4077 }, { 746, 4026, 70 }, { -4008, 750, -377 } }, { -0x39CD, 1542, -4823 } }, 289 } },
    { { { { { -372, 0, 4078 }, { 739, 4028, 67 }, { -4011, 742, -366 } }, { -0x39AA, 1536, -4820 } }, 289 }, { { { { -362, 0, 4079 }, { 732, 4029, 65 }, { -4013, 735, -356 } }, { -0x3988, 1530, -4818 } }, 289 } },
    { { { { { -350, 0, 4080 }, { 725, 4030, 62 }, { -4015, 728, -345 } }, { -0x3965, 1524, -4815 } }, 289 }, { { { { -339, 0, 4081 }, { 718, 4032, 59 }, { -4018, 720, -334 } }, { -0x3943, 1518, -4813 } }, 289 } },
    { { { { { -328, 0, 4082 }, { 711, 4033, 57 }, { -4020, 713, -323 } }, { -0x3920, 1512, -4810 } }, 289 }, { { { { -317, 0, 4083 }, { 703, 4034, 54 }, { -4022, 705, -312 } }, { -0x38FE, 1506, -4807 } }, 289 } },
    { { { { { -305, 0, 4084 }, { 696, 4035, 52 }, { -4024, 698, -301 } }, { -0x38DB, 1500, -4805 } }, 289 }, { { { { -294, 0, 4085 }, { 689, 4037, 49 }, { -4026, 691, -290 } }, { -0x38B9, 1495, -4802 } }, 289 } },
    { { { { { -282, 0, 4086 }, { 682, 4038, 47 }, { -4028, 683, -279 } }, { -0x3897, 1489, -4799 } }, 289 }, { { { { -271, 0, 4087 }, { 674, 4039, 44 }, { -4030, 676, -267 } }, { -0x3874, 1483, -4797 } }, 289 } },
    { { { { { -259, 0, 4087 }, { 667, 4040, 42 }, { -4032, 669, -256 } }, { -0x3852, 1478, -4794 } }, 289 }, { { { { -247, 0, 4088 }, { 660, 4042, 40 }, { -4034, 661, -244 } }, { -0x3830, 1472, -4791 } }, 289 } },
    { { { { { -236, 0, 4089 }, { 653, 4043, 37 }, { -4036, 654, -232 } }, { -0x380D, 1466, -4789 } }, 289 }, { { { { -224, 0, 4089 }, { 646, 4044, 35 }, { -4038, 647, -221 } }, { -0x37EB, 1461, -4786 } }, 289 } },
    { { { { { -212, 0, 4090 }, { 638, 4045, 33 }, { -4040, 639, -209 } }, { -0x37C9, 1455, -4783 } }, 289 }, { { { { -200, 0, 4091 }, { 631, 4046, 30 }, { -4042, 632, -197 } }, { -0x37A6, 1450, -4781 } }, 289 } },
    { { { { { -187, 0, 4091 }, { 624, 4048, 28 }, { -4043, 625, -185 } }, { -0x3784, 1444, -4778 } }, 289 }, { { { { -175, 0, 4092 }, { 617, 4049, 26 }, { -4045, 617, -173 } }, { -0x3761, 1438, -4776 } }, 289 } },
    { { { { { -163, 0, 4092 }, { 609, 4050, 24 }, { -4047, 610, -161 } }, { -0x373F, 1433, -4774 } }, 289 }, { { { { -151, 0, 4093 }, { 602, 4051, 22 }, { -4048, 603, -149 } }, { -0x371D, 1427, -4771 } }, 289 } },
    { { { { { -138, 0, 4093 }, { 595, 4052, 20 }, { -4050, 595, -137 } }, { -0x36FA, 1422, -4769 } }, 289 }, { { { { -126, 0, 4094 }, { 588, 4053, 18 }, { -4051, 588, -124 } }, { -0x36D7, 1416, -4767 } }, 289 } },
    { { { { { -113, 0, 4094 }, { 580, 4054, 16 }, { -4053, 581, -112 } }, { -0x36B5, 1411, -4765 } }, 289 }, { { { { -101, 0, 4094 }, { 573, 4055, 14 }, { -4054, 573, -100 } }, { -0x3692, 1405, -4763 } }, 289 } },
    { { { { { -88, 0, 4095 }, { 566, 4056, 12 }, { -4055, 566, -87 } }, { -0x3670, 1400, -4761 } }, 289 }, { { { { -76, 0, 4095 }, { 558, 4057, 10 }, { -4056, 558, -75 } }, { -0x364D, 1394, -4759 } }, 289 } },
    { { { { { -63, 0, 4095 }, { 551, 4058, 8 }, { -4058, 551, -62 } }, { -0x362A, 1389, -4758 } }, 289 }, { { { { -50, 0, 4095 }, { 544, 4059, 6 }, { -4059, 544, -50 } }, { -0x3607, 1383, -4756 } }, 289 } },
    { { { { { -37, 0, 4095 }, { 536, 4060, 4 }, { -4060, 536, -37 } }, { -0x35E4, 1378, -4755 } }, 289 }, { { { { -24, 0, 4095 }, { 529, 4061, 3 }, { -4061, 529, -24 } }, { -0x35C1, 1372, -4754 } }, 289 } },
    { { { { { -11, 0, 4095 }, { 522, 4062, 1 }, { -4062, 522, -11 } }, { -0x359E, 1367, -4753 } }, 289 }, { { { { 0, 0, 4096 }, { 514, 4063, 0 }, { -4063, 514, 0 } }, { -0x357B, 1361, -4752 } }, 289 } },
    { { { { { 13, 0, 4095 }, { 507, 4064, -1 }, { -4064, 507, 13 } }, { -0x3558, 1356, -4751 } }, 289 }, { { { { 26, 0, 4095 }, { 499, 4065, -3 }, { -4065, 499, 26 } }, { -0x3534, 1351, -4750 } }, 289 } },
    { { { { { 40, 0, 4095 }, { 492, 4066, -4 }, { -4066, 492, 39 } }, { -0x3511, 1345, -4750 } }, 289 }, { { { { 53, 0, 4095 }, { 484, 4067, -6 }, { -4066, 484, 52 } }, { -0x34EE, 1340, -4750 } }, 289 } },
    { { { { { 66, 0, 4095 }, { 477, 4068, -7 }, { -4067, 477, 65 } }, { -0x34CA, 1334, -4749 } }, 289 }, { { { { 79, 0, 4095 }, { 469, 4068, -9 }, { -4068, 469, 79 } }, { -0x34A6, 1328, -4749 } }, 289 } },
    { { { { { 93, 0, 4094 }, { 462, 4069, -10 }, { -4068, 462, 92 } }, { -0x3482, 1323, -4749 } }, 289 }, { { { { 107, 0, 4094 }, { 454, 4070, -11 }, { -4069, 454, 106 } }, { -0x345E, 1317, -4749 } }, 289 } },
    { { { { { 121, 0, 4094 }, { 446, 4071, -13 }, { -4069, 447, 120 } }, { -0x343A, 1312, -4749 } }, 289 }, { { { { 134, 0, 4093 }, { 439, 4072, -14 }, { -4070, 439, 134 } }, { -0x3416, 1306, -4749 } }, 289 } },
    { { { { { 149, 0, 4093 }, { 431, 4073, -15 }, { -4070, 431, 148 } }, { -0x33F2, 1301, -4750 } }, 289 }, { { { { 163, 0, 4092 }, { 423, 4073, -16 }, { -4070, 424, 162 } }, { -0x33CE, 1295, -4750 } }, 289 } },
    { { { { { 177, 0, 4092 }, { 416, 4074, -18 }, { -4070, 416, 176 } }, { -0x33A9, 1290, -4750 } }, 289 }, { { { { 191, 0, 4091 }, { 408, 4075, -19 }, { -4071, 408, 190 } }, { -0x3385, 1284, -4751 } }, 289 } },
    { { { { { 206, 0, 4090 }, { 400, 4076, -20 }, { -4071, 401, 205 } }, { -0x3360, 1278, -4751 } }, 289 }, { { { { 220, 0, 4090 }, { 392, 4077, -21 }, { -4071, 393, 219 } }, { -0x333C, 1273, -4752 } }, 289 } },
    { { { { { 235, 0, 4089 }, { 384, 4077, -22 }, { -4071, 385, 234 } }, { -0x3317, 1267, -4752 } }, 289 }, { { { { 249, 0, 4088 }, { 377, 4078, -23 }, { -4070, 377, 248 } }, { -0x32F2, 1262, -4753 } }, 289 } },
    { { { { { 264, 0, 4087 }, { 369, 4079, -23 }, { -4070, 370, 263 } }, { -0x32CE, 1256, -4754 } }, 289 }, { { { { 279, 0, 4086 }, { 361, 4079, -24 }, { -4070, 362, 278 } }, { -0x32A9, 1251, -4755 } }, 289 } },
    { { { { { 294, 0, 4085 }, { 353, 4080, -25 }, { -4070, 354, 293 } }, { -0x3284, 1245, -4756 } }, 289 }, { { { { 309, 0, 4084 }, { 345, 4081, -26 }, { -4069, 346, 308 } }, { -0x325F, 1240, -4757 } }, 289 } },
    { { { { { 324, 0, 4083 }, { 337, 4081, -26 }, { -4069, 338, 323 } }, { -0x323A, 1234, -4758 } }, 289 }, { { { { 339, 0, 4081 }, { 330, 4082, -27 }, { -4068, 331, 338 } }, { -0x3216, 1229, -4759 } }, 289 } },
    { { { { { 354, 0, 4080 }, { 322, 4083, -28 }, { -4067, 323, 353 } }, { -0x31F1, 1223, -4760 } }, 289 }, { { { { 369, 0, 4079 }, { 314, 4083, -28 }, { -4067, 315, 368 } }, { -0x31CC, 1218, -4761 } }, 289 } },
    { { { { { 384, 0, 4077 }, { 306, 4084, -28 }, { -4066, 308, 383 } }, { -0x31A7, 1212, -4763 } }, 289 }, { { { { 400, 0, 4076 }, { 299, 4084, -29 }, { -4065, 300, 399 } }, { -0x3182, 1207, -4764 } }, 289 } },
    { { { { { 415, 0, 4074 }, { 291, 4085, -29 }, { -4064, 292, 414 } }, { -0x315D, 1201, -4766 } }, 289 }, { { { { 430, 0, 4073 }, { 283, 4086, -29 }, { -4063, 285, 429 } }, { -0x3139, 1196, -4767 } }, 289 } },
    { { { { { 446, 0, 4071 }, { 275, 4086, -30 }, { -4062, 277, 445 } }, { -0x3114, 1191, -4769 } }, 289 }, { { { { 461, 0, 4069 }, { 268, 4087, -30 }, { -4061, 270, 460 } }, { -0x30EF, 1185, -4771 } }, 289 } },
    { { { { { 476, 0, 4068 }, { 260, 4087, -30 }, { -4059, 262, 475 } }, { -0x30CA, 1180, -4773 } }, 289 }, { { { { 492, 0, 4066 }, { 253, 4088, -30 }, { -4058, 254, 491 } }, { -0x30A6, 1175, -4774 } }, 289 } },
    { { { { { 507, 0, 4064 }, { 245, 4088, -30 }, { -4056, 247, 506 } }, { -0x3081, 1170, -4776 } }, 289 }, { { { { 523, 0, 4062 }, { 238, 4088, -30 }, { -4055, 240, 522 } }, { -0x305D, 1165, -4778 } }, 289 } },
    { { { { { 538, 0, 4060 }, { 230, 4089, -30 }, { -4053, 232, 537 } }, { -0x3038, 1159, -4781 } }, 289 }, { { { { 554, 0, 4058 }, { 223, 4089, -30 }, { -4052, 225, 553 } }, { -0x3014, 1154, -4783 } }, 289 } },
    { { { { { 569, 0, 4056 }, { 216, 4090, -30 }, { -4050, 218, 568 } }, { -0x2FEF, 1149, -4785 } }, 289 }, { { { { 585, 0, 4053 }, { 208, 4090, -30 }, { -4048, 210, 584 } }, { -0x2FCB, 1144, -4787 } }, 289 } },
    { { { { { 600, 0, 4051 }, { 201, 4090, -29 }, { -4046, 203, 599 } }, { -0x2FA7, 1139, -4790 } }, 289 }, { { { { 616, 0, 4049 }, { 194, 4091, -29 }, { -4044, 196, 615 } }, { -0x2F83, 1134, -4792 } }, 289 } },
    { { { { { 631, 0, 4047 }, { 187, 4091, -29 }, { -4042, 189, 630 } }, { -0x2F5F, 1130, -4795 } }, 289 }, { { { { 646, 0, 4044 }, { 180, 4091, -28 }, { -4040, 182, 646 } }, { -0x2F3B, 1125, -4798 } }, 289 } },
    { { { { { 662, 0, 4042 }, { 173, 4092, -28 }, { -4038, 175, 661 } }, { -0x2F17, 1120, -4801 } }, 289 }, { { { { 677, 0, 4039 }, { 166, 4092, -27 }, { -4036, 168, 677 } }, { -0x2EF4, 1115, -4803 } }, 289 } },
    { { { { { 693, 0, 4036 }, { 159, 4092, -27 }, { -4033, 162, 692 } }, { -0x2ED0, 1111, -4806 } }, 289 }, { { { { 708, 0, 4034 }, { 153, 4093, -26 }, { -4031, 155, 707 } }, { -0x2EAD, 1106, -4809 } }, 289 } },
    { { { { { 723, 0, 4031 }, { 146, 4093, -26 }, { -4028, 148, 723 } }, { -0x2E89, 1101, -4813 } }, 289 }, { { { { 738, 0, 4028 }, { 139, 4093, -25 }, { -4026, 142, 738 } }, { -0x2E66, 1097, -4816 } }, 289 } },
    { { { { { 754, 0, 4025 }, { 133, 4093, -25 }, { -4023, 135, 753 } }, { -0x2E43, 1092, -4819 } }, 289 }, { { { { 769, 0, 4023 }, { 127, 4093, -24 }, { -4021, 129, 768 } }, { -0x2E21, 1088, -4822 } }, 289 } },
    { { { { { 784, 0, 4020 }, { 120, 4094, -23 }, { -4018, 123, 784 } }, { -0x2DFE, 1084, -4826 } }, 289 }, { { { { 799, 0, 4017 }, { 114, 4094, -22 }, { -4015, 116, 799 } }, { -0x2DDB, 1079, -4830 } }, 289 } },
    { { { { { 814, 0, 4014 }, { 108, 4094, -22 }, { -4012, 110, 814 } }, { -0x2DB9, 1075, -4833 } }, 289 }, { { { { 829, 0, 4011 }, { 102, 4094, -21 }, { -4009, 104, 829 } }, { -0x2D97, 1071, -4837 } }, 289 } },
    { { { { { 844, 0, 4008 }, { 96, 4094, -20 }, { -4006, 98, 844 } }, { -0x2D75, 1067, -4841 } }, 289 }, { { { { 859, 0, 4004 }, { 90, 4094, -19 }, { -4003, 92, 858 } }, { -0x2D53, 1063, -4845 } }, 289 } },
    { { { { { 874, 0, 4001 }, { 85, 4095, -18 }, { -4000, 87, 873 } }, { -0x2D31, 1059, -4849 } }, 289 }, { { { { 888, 0, 3998 }, { 79, 4095, -17 }, { -3997, 81, 888 } }, { -0x2D0F, 1055, -4853 } }, 289 } },
    { { { { { 903, 0, 3995 }, { 73, 4095, -16 }, { -3994, 75, 903 } }, { -0x2CEE, 1051, -4857 } }, 289 }, { { { { 918, 0, 3991 }, { 68, 4095, -15 }, { -3991, 69, 918 } }, { -0x2CCC, 1047, -4862 } }, 289 } },
    { { { { { 933, 0, 3988 }, { 62, 4095, -14 }, { -3987, 64, 933 } }, { -0x2CAB, 1043, -4866 } }, 289 }, { { { { 947, 0, 3984 }, { 57, 4095, -13 }, { -3984, 58, 947 } }, { -0x2C89, 1040, -4871 } }, 289 } },
    { { { { { 962, 0, 3981 }, { 51, 4095, -12 }, { -3980, 53, 962 } }, { -0x2C68, 1036, -4876 } }, 289 }, { { { { 977, 0, 3977 }, { 46, 4095, -11 }, { -3977, 47, 977 } }, { -0x2C47, 1032, -4881 } }, 289 } },
    { { { { { 991, 0, 3974 }, { 41, 4095, -10 }, { -3973, 42, 991 } }, { -0x2C26, 1028, -4885 } }, 289 }, { { { { 1006, 0, 3970 }, { 35, 4095, -9 }, { -3970, 37, 1006 } }, { -0x2C05, 1025, -4890 } }, 289 } },
    { { { { { 1021, 0, 3966 }, { 30, 4095, -7 }, { -3966, 31, 1021 } }, { -0x2BE4, 1021, -4895 } }, 289 }, { { { { 1035, 0, 3962 }, { 25, 4095, -6 }, { -3962, 26, 1035 } }, { -0x2BC3, 1018, -4901 } }, 289 } },
    { { { { { 1050, 0, 3959 }, { 20, 4095, -5 }, { -3958, 21, 1050 } }, { -0x2BA2, 1014, -4906 } }, 289 }, { { { { 1064, 0, 3955 }, { 15, 4095, -4 }, { -3955, 16, 1064 } }, { -0x2B82, 1011, -4911 } }, 289 } },
    { { { { { 1079, 0, 3951 }, { 11, 4095, -3 }, { -3951, 11, 1079 } }, { -0x2B61, 1007, -4916 } }, 289 }, { { { { 1094, 0, 3947 }, { 6, 4095, -1 }, { -3947, 6, 1094 } }, { -0x2B41, 1004, -4922 } }, 289 } },
    { { { { { 1108, 0, 3943 }, { 1, 4096, 0 }, { -3943, 1, 1108 } }, { -0x2B20, 1001, -4927 } }, 289 }, { { { { 1123, 0, 3938 }, { -3, 4095, 0 }, { -3938, -3, 1123 } }, { -0x2B00, 997, -4933 } }, 289 } },
    { { { { { 1137, 0, 3934 }, { -7, 4095, 2 }, { -3934, -8, 1137 } }, { -0x2AE0, 994, -4939 } }, 289 }, { { { { 1152, 0, 3930 }, { -12, 4095, 3 }, { -3930, -12, 1152 } }, { -0x2AC0, 991, -4944 } }, 289 } },
    { { { { { 1167, 0, 3926 }, { -16, 4095, 4 }, { -3926, -17, 1167 } }, { -0x2AA0, 988, -4950 } }, 289 }, { { { { 1181, 0, 3921 }, { -21, 4095, 6 }, { -3921, -22, 1181 } }, { -0x2A7F, 984, -4956 } }, 289 } },
    { { { { { 1196, 0, 3917 }, { -25, 4095, 7 }, { -3917, -26, 1196 } }, { -0x2A5F, 981, -4962 } }, 289 }, { { { { 1210, 0, 3912 }, { -29, 4095, 9 }, { -3912, -31, 1210 } }, { -0x2A3F, 978, -4968 } }, 289 } },
    { { { { { 1225, 0, 3908 }, { -33, 4095, 10 }, { -3908, -35, 1225 } }, { -0x2A1F, 975, -4974 } }, 289 }, { { { { 1240, 0, 3903 }, { -38, 4095, 12 }, { -3903, -39, 1240 } }, { -0x2A00, 972, -4980 } }, 289 } },
    { { { { { 1254, 0, 3899 }, { -42, 4095, 13 }, { -3898, -44, 1254 } }, { -0x29E0, 969, -4986 } }, 289 }, { { { { 1269, 0, 3894 }, { -46, 4095, 15 }, { -3894, -48, 1269 } }, { -0x29C0, 966, -4992 } }, 289 } },
    { { { { { 1284, 0, 3889 }, { -50, 4095, 16 }, { -3889, -52, 1284 } }, { -0x29A0, 963, -4999 } }, 289 }, { { { { 1298, 0, 3884 }, { -54, 4095, 18 }, { -3884, -56, 1298 } }, { -0x2981, 961, -5005 } }, 289 } },
    { { { { { 1313, 0, 3879 }, { -57, 4095, 19 }, { -3879, -61, 1313 } }, { -0x2961, 958, -5011 } }, 289 }, { { { { 1328, 0, 3874 }, { -61, 4095, 21 }, { -3874, -65, 1328 } }, { -0x2941, 955, -5017 } }, 289 } },
    { { { { { 1343, 0, 3869 }, { -65, 4095, 22 }, { -3868, -69, 1342 } }, { -0x2922, 952, -5024 } }, 289 }, { { { { 1357, 0, 3864 }, { -68, 4095, 24 }, { -3863, -73, 1357 } }, { -0x2902, 949, -5030 } }, 289 } },
    { { { { { 1372, 0, 3859 }, { -72, 4095, 25 }, { -3858, -77, 1372 } }, { -0x28E2, 947, -5037 } }, 289 }, { { { { 1387, 0, 3853 }, { -76, 4095, 27 }, { -3853, -80, 1387 } }, { -0x28C3, 944, -5043 } }, 289 } },
    { { { { { 1402, 0, 3848 }, { -79, 4095, 28 }, { -3847, -84, 1402 } }, { -0x28A3, 942, -5050 } }, 289 }, { { { { 1417, 0, 3842 }, { -82, 4095, 30 }, { -3842, -88, 1417 } }, { -0x2884, 939, -5056 } }, 289 } },
    { { { { { 1432, 0, 3837 }, { -86, 4094, 32 }, { -3836, -92, 1431 } }, { -0x2864, 936, -5063 } }, 289 }, { { { { 1447, 0, 3831 }, { -89, 4094, 33 }, { -3830, -95, 1446 } }, { -0x2845, 934, -5069 } }, 289 } },
    { { { { { 1462, 0, 3826 }, { -92, 4094, 35 }, { -3824, -99, 1461 } }, { -0x2825, 931, -5076 } }, 289 }, { { { { 1477, 0, 3820 }, { -95, 4094, 37 }, { -3819, -102, 1476 } }, { -0x2806, 929, -5083 } }, 289 } },
    { { { { { 1492, 0, 3814 }, { -99, 4094, 38 }, { -3813, -106, 1492 } }, { -0x27E6, 927, -5089 } }, 289 }, { { { { 1507, 0, 3808 }, { -102, 4094, 40 }, { -3807, -109, 1507 } }, { -0x27C7, 924, -5096 } }, 289 } },
    { { { { { 1522, 0, 3802 }, { -105, 4094, 42 }, { -3800, -113, 1522 } }, { -0x27A7, 922, -5103 } }, 289 }, { { { { 1538, 0, 3796 }, { -108, 4094, 43 }, { -3794, -116, 1537 } }, { -0x2788, 920, -5110 } }, 289 } },
    { { { { { 1553, 0, 3790 }, { -110, 4094, 45 }, { -3788, -119, 1552 } }, { -0x2768, 917, -5116 } }, 289 }, { { { { 1568, 0, 3783 }, { -113, 4094, 47 }, { -3782, -123, 1567 } }, { -0x2749, 915, -5123 } }, 289 } },
    { { { { { 1583, 0, 3777 }, { -116, 4094, 48 }, { -3775, -126, 1583 } }, { -0x2729, 913, -5130 } }, 289 }, { { { { 1599, 0, 3770 }, { -118, 4093, 50 }, { -3768, -129, 1598 } }, { -9994, 911, -5136 } }, 289 } },
    { { { { { 1614, 0, 3764 }, { -121, 4093, 52 }, { -3762, -131, 1613 } }, { -9963, 909, -5143 } }, 289 }, { { { { 1630, 0, 3757 }, { -123, 4093, 53 }, { -3755, -134, 1629 } }, { -9932, 907, -5150 } }, 289 } },
    { { { { { 1645, 0, 3750 }, { -125, 4093, 55 }, { -3748, -137, 1644 } }, { -9901, 905, -5156 } }, 289 }, { { { { 1660, 0, 3744 }, { -128, 4093, 56 }, { -3741, -140, 1659 } }, { -9870, 903, -5163 } }, 289 } },
    { { { { { 1676, 0, 3737 }, { -130, 4093, 58 }, { -3734, -142, 1675 } }, { -9839, 901, -5170 } }, 289 }, { { { { 1691, 0, 3730 }, { -132, 4093, 60 }, { -3727, -145, 1690 } }, { -9808, 899, -5176 } }, 289 } },
    { { { { { 1707, 0, 3723 }, { -134, 4093, 61 }, { -3720, -147, 1706 } }, { -9777, 898, -5183 } }, 289 }, { { { { 1722, 0, 3716 }, { -136, 4093, 63 }, { -3713, -150, 1721 } }, { -9746, 896, -5190 } }, 289 } },
    { { { { { 1738, 0, 3708 }, { -137, 4093, 64 }, { -3706, -152, 1737 } }, { -9715, 894, -5197 } }, 289 }, { { { { 1753, 0, 3701 }, { -139, 4093, 66 }, { -3698, -154, 1752 } }, { -9685, 893, -5203 } }, 289 } },
    { { { { { 1769, 0, 3694 }, { -141, 4093, 67 }, { -3691, -156, 1767 } }, { -9654, 891, -5210 } }, 289 }, { { { { 1784, 0, 3686 }, { -142, 4092, 69 }, { -3683, -158, 1783 } }, { -9623, 890, -5217 } }, 289 } },
    { { { { { 1800, 0, 3679 }, { -144, 4092, 70 }, { -3676, -160, 1798 } }, { -9593, 888, -5224 } }, 289 }, { { { { 1815, 0, 3671 }, { -145, 4092, 72 }, { -3668, -162, 1814 } }, { -9562, 887, -5231 } }, 289 } },
    { { { { { 1831, 0, 3663 }, { -147, 4092, 73 }, { -3660, -164, 1829 } }, { -9532, 885, -5238 } }, 289 }, { { { { 1846, 0, 3656 }, { -148, 4092, 75 }, { -3653, -166, 1845 } }, { -9501, 884, -5245 } }, 289 } },
    { { { { { 1861, 0, 3648 }, { -150, 4092, 76 }, { -3645, -168, 1860 } }, { -9471, 883, -5252 } }, 289 }, { { { { 1877, 0, 3640 }, { -151, 4092, 78 }, { -3637, -170, 1875 } }, { -9441, 881, -5259 } }, 289 } },
    { { { { { 1892, 0, 3632 }, { -152, 4092, 79 }, { -3629, -172, 1891 } }, { -9410, 880, -5267 } }, 289 }, { { { { 1908, 0, 3624 }, { -153, 4092, 80 }, { -3621, -173, 1906 } }, { -9380, 879, -5274 } }, 289 } },
    { { { { { 1923, 0, 3616 }, { -154, 4092, 82 }, { -3613, -175, 1921 } }, { -9350, 878, -5281 } }, 289 }, { { { { 1938, 0, 3608 }, { -156, 4092, 83 }, { -3604, -177, 1936 } }, { -9319, 876, -5289 } }, 289 } },
    { { { { { 1953, 0, 3599 }, { -157, 4092, 85 }, { -3596, -178, 1952 } }, { -9289, 875, -5296 } }, 289 }, { { { { 1969, 0, 3591 }, { -158, 4092, 86 }, { -3588, -180, 1967 } }, { -9259, 874, -5304 } }, 289 } },
    { { { { { 1984, 0, 3583 }, { -159, 4091, 88 }, { -3579, -181, 1982 } }, { -9228, 873, -5311 } }, 289 }, { { { { 1999, 0, 3574 }, { -160, 4091, 89 }, { -3571, -183, 1997 } }, { -9198, 872, -5319 } }, 289 } },
    { { { { { 2014, 0, 3566 }, { -160, 4091, 90 }, { -3562, -184, 2012 } }, { -9168, 871, -5327 } }, 289 }, { { { { 2029, 0, 3557 }, { -161, 4091, 92 }, { -3554, -186, 2027 } }, { -9137, 870, -5335 } }, 289 } },
    { { { { { 2044, 0, 3549 }, { -162, 4091, 93 }, { -3545, -187, 2042 } }, { -9107, 868, -5343 } }, 289 }, { { { { 2059, 0, 3540 }, { -163, 4091, 95 }, { -3536, -189, 2057 } }, { -9077, 867, -5351 } }, 289 } },
    { { { { { 2074, 0, 3531 }, { -164, 4091, 96 }, { -3528, -190, 2071 } }, { -9047, 866, -5360 } }, 289 }, { { { { 2089, 0, 3523 }, { -165, 4091, 97 }, { -3519, -192, 2086 } }, { -9016, 865, -5368 } }, 289 } },
    { { { { { 2103, 0, 3514 }, { -166, 4091, 99 }, { -3510, -193, 2101 } }, { -8986, 864, -5377 } }, 289 }, { { { { 2118, 0, 3505 }, { -166, 4091, 100 }, { -3501, -194, 2116 } }, { -8956, 863, -5385 } }, 289 } },
    { { { { { 2133, 0, 3496 }, { -167, 4091, 102 }, { -3492, -196, 2130 } }, { -8925, 862, -5394 } }, 289 }, { { { { 2147, 0, 3487 }, { -168, 4091, 103 }, { -3483, -197, 2145 } }, { -8895, 861, -5403 } }, 289 } },
    { { { { { 2162, 0, 3478 }, { -169, 4091, 105 }, { -3474, -199, 2159 } }, { -8864, 860, -5412 } }, 289 }, { { { { 2176, 0, 3469 }, { -169, 4091, 106 }, { -3465, -200, 2174 } }, { -8834, 859, -5421 } }, 289 } },
    { { { { { 2191, 0, 3460 }, { -170, 4091, 108 }, { -3456, -202, 2188 } }, { -8804, 858, -5430 } }, 289 }, { { { { 2205, 0, 3451 }, { -171, 4090, 109 }, { -3447, -203, 2202 } }, { -8773, 856, -5440 } }, 289 } },
    { { { { { 2219, 0, 3442 }, { -172, 4090, 111 }, { -3438, -204, 2216 } }, { -8743, 855, -5449 } }, 289 }, { { { { 2233, 0, 3433 }, { -173, 4090, 112 }, { -3428, -206, 2230 } }, { -8712, 854, -5459 } }, 289 } },
    { { { { { 2247, 0, 3424 }, { -173, 4090, 114 }, { -3419, -207, 2244 } }, { -8681, 853, -5469 } }, 289 }, { { { { 2261, 0, 3414 }, { -174, 4090, 115 }, { -3410, -209, 2258 } }, { -8651, 852, -5479 } }, 289 } },
    { { { { { 2275, 0, 3405 }, { -175, 4090, 117 }, { -3401, -210, 2272 } }, { -8620, 851, -5489 } }, 289 }, { { { { 2289, 0, 3396 }, { -176, 4090, 118 }, { -3391, -212, 2286 } }, { -8590, 850, -5500 } }, 289 } },
    { { { { { 2303, 0, 3387 }, { -176, 4090, 120 }, { -3382, -214, 2300 } }, { -8559, 848, -5510 } }, 289 }, { { { { 2316, 0, 3377 }, { -177, 4090, 121 }, { -3373, -215, 2313 } }, { -8528, 847, -5521 } }, 289 } },
    { { { { { 2330, 0, 3368 }, { -178, 4090, 123 }, { -3363, -216, 2327 } }, { -8497, 846, -5531 } }, 289 }, { { { { 2344, 0, 3358 }, { -179, 4090, 124 }, { -3354, -218, 2340 } }, { -8466, 845, -5542 } }, 289 } },
    { { { { { 2357, 0, 3349 }, { -179, 4090, 126 }, { -3344, -219, 2354 } }, { -8435, 844, -5553 } }, 289 }, { { { { 2371, 0, 3339 }, { -180, 4090, 128 }, { -3334, -221, 2367 } }, { -8405, 843, -5564 } }, 289 } },
    { { { { { 2384, 0, 3330 }, { -180, 4089, 129 }, { -3325, -222, 2381 } }, { -8374, 842, -5576 } }, 289 }, { { { { 2398, 0, 3320 }, { -181, 4089, 131 }, { -3315, -223, 2394 } }, { -8343, 841, -5587 } }, 289 } },
    { { { { { 2411, 0, 3310 }, { -181, 4089, 132 }, { -3305, -225, 2407 } }, { -8312, 840, -5598 } }, 289 }, { { { { 2424, 0, 3301 }, { -182, 4089, 134 }, { -3296, -226, 2421 } }, { -8281, 839, -5610 } }, 289 } },
    { { { { { 2438, 0, 3291 }, { -182, 4089, 135 }, { -3286, -227, 2434 } }, { -8250, 838, -5622 } }, 289 }, { { { { 2451, 0, 3281 }, { -183, 4089, 136 }, { -3276, -228, 2447 } }, { -8219, 837, -5634 } }, 289 } },
    { { { { { 2464, 0, 3271 }, { -183, 4089, 138 }, { -3266, -230, 2460 } }, { -8188, 836, -5645 } }, 289 }, { { { { 2477, 0, 3261 }, { -184, 4089, 139 }, { -3256, -231, 2473 } }, { -8157, 835, -5657 } }, 289 } },
    { { { { { 2490, 0, 3251 }, { -184, 4089, 141 }, { -3246, -232, 2486 } }, { -8126, 834, -5670 } }, 289 }, { { { { 2503, 0, 3241 }, { -184, 4089, 142 }, { -3236, -233, 2499 } }, { -8095, 833, -5682 } }, 289 } },
    { { { { { 2516, 0, 3231 }, { -185, 4089, 144 }, { -3226, -234, 2512 } }, { -8063, 832, -5694 } }, 289 }, { { { { 2529, 0, 3221 }, { -185, 4089, 145 }, { -3216, -235, 2525 } }, { -8032, 831, -5707 } }, 289 } },
    { { { { { 2542, 0, 3211 }, { -185, 4089, 147 }, { -3205, -236, 2538 } }, { -8001, 830, -5719 } }, 289 }, { { { { 2555, 0, 3201 }, { -186, 4089, 148 }, { -3195, -237, 2550 } }, { -7970, 829, -5732 } }, 289 } },
    { { { { { 2568, 0, 3190 }, { -186, 4089, 149 }, { -3185, -239, 2563 } }, { -7939, 828, -5745 } }, 289 }, { { { { 2580, 0, 3180 }, { -186, 4088, 151 }, { -3175, -240, 2576 } }, { -7908, 827, -5757 } }, 289 } },
    { { { { { 2593, 0, 3170 }, { -186, 4088, 152 }, { -3164, -241, 2588 } }, { -7877, 826, -5770 } }, 289 }, { { { { 2605, 0, 3160 }, { -186, 4088, 154 }, { -3154, -242, 2601 } }, { -7847, 825, -5783 } }, 289 } },
    { { { { { 2618, 0, 3149 }, { -187, 4088, 155 }, { -3144, -243, 2613 } }, { -7816, 824, -5796 } }, 289 }, { { { { 2630, 0, 3139 }, { -187, 4088, 156 }, { -3133, -244, 2626 } }, { -7785, 824, -5810 } }, 289 } },
    { { { { { 2643, 0, 3129 }, { -187, 4088, 158 }, { -3123, -245, 2638 } }, { -7754, 823, -5823 } }, 289 }, { { { { 2655, 0, 3118 }, { -187, 4088, 159 }, { -3113, -246, 2650 } }, { -7723, 822, -5836 } }, 289 } },
    { { { { { 2667, 0, 3108 }, { -187, 4088, 161 }, { -3102, -247, 2662 } }, { -7692, 821, -5850 } }, 289 }, { { { { 2679, 0, 3097 }, { -188, 4088, 162 }, { -3092, -248, 2674 } }, { -7662, 820, -5863 } }, 289 } },
    { { { { { 2691, 0, 3087 }, { -188, 4088, 164 }, { -3081, -249, 2686 } }, { -7631, 819, -5877 } }, 289 }, { { { { 2703, 0, 3076 }, { -188, 4088, 165 }, { -3070, -250, 2698 } }, { -7600, 818, -5891 } }, 289 } },
    { { { { { 2715, 0, 3066 }, { -188, 4088, 166 }, { -3060, -251, 2710 } }, { -7570, 817, -5904 } }, 289 }, { { { { 2727, 0, 3055 }, { -188, 4088, 168 }, { -3049, -252, 2722 } }, { -7539, 816, -5918 } }, 289 } },
    { { { { { 2739, 0, 3045 }, { -188, 4088, 169 }, { -3039, -253, 2734 } }, { -7509, 815, -5932 } }, 289 }, { { { { 2751, 0, 3034 }, { -188, 4088, 171 }, { -3028, -255, 2745 } }, { -7478, 814, -5946 } }, 289 } },
    { { { { { 2762, 0, 3023 }, { -189, 4087, 172 }, { -3017, -256, 2757 } }, { -7448, 813, -5960 } }, 289 }, { { { { 2774, 0, 3013 }, { -189, 4087, 174 }, { -3007, -257, 2769 } }, { -7417, 812, -5974 } }, 289 } },
    { { { { { 2786, 0, 3002 }, { -189, 4087, 175 }, { -2996, -258, 2780 } }, { -7387, 811, -5989 } }, 289 }, { { { { 2797, 0, 2991 }, { -189, 4087, 177 }, { -2985, -259, 2791 } }, { -7357, 810, -6003 } }, 289 } },
    { { { { { 2808, 0, 2981 }, { -189, 4087, 178 }, { -2975, -260, 2803 } }, { -7327, 809, -6017 } }, 289 }, { { { { 2820, 0, 2970 }, { -189, 4087, 180 }, { -2964, -261, 2814 } }, { -7297, 808, -6032 } }, 289 } },
    { { { { { 2831, 0, 2959 }, { -190, 4087, 181 }, { -2953, -263, 2825 } }, { -7267, 807, -6046 } }, 289 }, { { { { 2842, 0, 2949 }, { -190, 4087, 183 }, { -2943, -264, 2836 } }, { -7237, 806, -6061 } }, 289 } },
    { { { { { 2853, 0, 2938 }, { -190, 4087, 184 }, { -2932, -265, 2847 } }, { -7207, 805, -6075 } }, 289 }, { { { { 2864, 0, 2927 }, { -190, 4087, 186 }, { -2921, -266, 2858 } }, { -7177, 804, -6090 } }, 289 } },
    { { { { { 2875, 0, 2917 }, { -190, 4087, 188 }, { -2910, -268, 2869 } }, { -7148, 803, -6105 } }, 289 }, { { { { 2886, 0, 2906 }, { -191, 4087, 189 }, { -2900, -269, 2879 } }, { -7118, 802, -6120 } }, 289 } },
    { { { { { 2896, 0, 2895 }, { -191, 4087, 191 }, { -2889, -270, 2890 } }, { -7089, 801, -6135 } }, 289 }, { { { { 2907, 0, 2884 }, { -191, 4086, 193 }, { -2878, -272, 2901 } }, { -7060, 800, -6150 } }, 289 } },
    { { { { { 2918, 0, 2874 }, { -191, 4086, 194 }, { -2867, -273, 2911 } }, { -7030, 798, -6165 } }, 289 }, { { { { 2928, 0, 2863 }, { -192, 4086, 196 }, { -2857, -274, 2922 } }, { -7001, 797, -6180 } }, 289 } },
    { { { { { 2939, 0, 2852 }, { -192, 4086, 198 }, { -2846, -276, 2932 } }, { -6972, 796, -6195 } }, 289 }, { { { { 2949, 0, 2842 }, { -192, 4086, 200 }, { -2835, -277, 2942 } }, { -6943, 794, -6210 } }, 289 } },
    { { { { { 2959, 0, 2831 }, { -193, 4086, 202 }, { -2824, -279, 2952 } }, { -6914, 793, -6226 } }, 289 }, { { { { 2969, 0, 2820 }, { -193, 4086, 203 }, { -2814, -281, 2962 } }, { -6885, 792, -6241 } }, 289 } },
    { { { { { 2979, 0, 2810 }, { -194, 4086, 205 }, { -2803, -282, 2972 } }, { -6856, 790, -6257 } }, 289 }, { { { { 2989, 0, 2799 }, { -194, 4086, 207 }, { -2792, -284, 2982 } }, { -6827, 789, -6273 } }, 289 } },
    { { { { { 2999, 0, 2788 }, { -194, 4085, 209 }, { -2782, -286, 2992 } }, { -6798, 787, -6289 } }, 289 }, { { { { 3009, 0, 2778 }, { -195, 4085, 211 }, { -2771, -288, 3002 } }, { -6769, 786, -6305 } }, 289 } },
    { { { { { 3019, 0, 2767 }, { -195, 4085, 213 }, { -2760, -289, 3012 } }, { -6741, 784, -6321 } }, 289 }, { { { { 3029, 0, 2756 }, { -196, 4085, 215 }, { -2749, -291, 3021 } }, { -6712, 783, -6337 } }, 289 } },
    { { { { { 3039, 0, 2745 }, { -196, 4085, 217 }, { -2738, -293, 3031 } }, { -6683, 781, -6353 } }, 289 }, { { { { 3048, 0, 2735 }, { -197, 4085, 219 }, { -2728, -295, 3040 } }, { -6655, 780, -6370 } }, 289 } },
    { { { { { 3058, 0, 2724 }, { -197, 4085, 221 }, { -2717, -297, 3050 } }, { -6626, 778, -6386 } }, 289 }, { { { { 3068, 0, 2713 }, { -198, 4085, 224 }, { -2706, -299, 3059 } }, { -6598, 777, -6403 } }, 289 } },
    { { { { { 3077, 0, 2702 }, { -198, 4084, 226 }, { -2695, -300, 3069 } }, { -6570, 775, -6419 } }, 289 }, { { { { 3087, 0, 2692 }, { -199, 4084, 228 }, { -2684, -302, 3078 } }, { -6541, 774, -6436 } }, 289 } },
    { { { { { 3096, 0, 2681 }, { -199, 4084, 230 }, { -2673, -304, 3087 } }, { -6513, 772, -6453 } }, 289 }, { { { { 3105, 0, 2670 }, { -199, 4084, 232 }, { -2663, -306, 3097 } }, { -6485, 771, -6469 } }, 289 } },
    { { { { { 3115, 0, 2659 }, { -200, 4084, 234 }, { -2652, -308, 3106 } }, { -6457, 769, -6486 } }, 289 }, { { { { 3124, 0, 2648 }, { -200, 4084, 236 }, { -2641, -310, 3115 } }, { -6429, 767, -6503 } }, 289 } },
    { { { { { 3133, 0, 2638 }, { -201, 4084, 238 }, { -2630, -312, 3124 } }, { -6401, 766, -6520 } }, 289 }, { { { { 3142, 0, 2627 }, { -201, 4083, 240 }, { -2619, -314, 3133 } }, { -6373, 764, -6537 } }, 289 } },
    { { { { { 3151, 0, 2616 }, { -201, 4083, 243 }, { -2608, -315, 3142 } }, { -6345, 763, -6554 } }, 289 }, { { { { 3160, 0, 2605 }, { -202, 4083, 245 }, { -2597, -317, 3151 } }, { -6317, 761, -6571 } }, 289 } },
    { { { { { 3169, 0, 2594 }, { -202, 4083, 247 }, { -2586, -319, 3159 } }, { -6290, 759, -6589 } }, 289 }, { { { { 3178, 0, 2583 }, { -202, 4083, 249 }, { -2575, -321, 3168 } }, { -6262, 758, -6606 } }, 289 } },
    { { { { { 3187, 0, 2572 }, { -202, 4083, 251 }, { -2564, -323, 3177 } }, { -6234, 756, -6623 } }, 289 }, { { { { 3195, 0, 2561 }, { -203, 4083, 253 }, { -2553, -324, 3185 } }, { -6207, 755, -6640 } }, 289 } },
    { { { { { 3204, 0, 2550 }, { -203, 4082, 255 }, { -2542, -326, 3194 } }, { -6179, 753, -6658 } }, 289 }, { { { { 3213, 0, 2539 }, { -203, 4082, 257 }, { -2531, -328, 3203 } }, { -6152, 752, -6675 } }, 289 } },
    { { { { { 3221, 0, 2529 }, { -203, 4082, 259 }, { -2520, -329, 3211 } }, { -6125, 750, -6692 } }, 289 }, { { { { 3230, 0, 2518 }, { -203, 4082, 261 }, { -2509, -331, 3219 } }, { -6097, 749, -6710 } }, 289 } },
    { { { { { 3239, 0, 2507 }, { -203, 4082, 263 }, { -2498, -332, 3228 } }, { -6070, 747, -6727 } }, 289 }, { { { { 3247, 0, 2496 }, { -203, 4082, 265 }, { -2487, -334, 3236 } }, { -6043, 746, -6745 } }, 289 } },
    { { { { { 3255, 0, 2485 }, { -203, 4082, 266 }, { -2476, -335, 3244 } }, { -6016, 745, -6762 } }, 289 }, { { { { 3264, 0, 2474 }, { -203, 4082, 268 }, { -2465, -337, 3253 } }, { -5989, 743, -6780 } }, 289 } },
    { { { { { 3272, 0, 2463 }, { -203, 4081, 270 }, { -2455, -338, 3261 } }, { -5962, 742, -6797 } }, 289 }, { { { { 3280, 0, 2452 }, { -203, 4081, 272 }, { -2444, -339, 3269 } }, { -5935, 741, -6815 } }, 289 } },
    { { { { { 3288, 0, 2441 }, { -203, 4081, 273 }, { -2433, -340, 3277 } }, { -5908, 739, -6832 } }, 289 }, { { { { 3296, 0, 2430 }, { -202, 4081, 275 }, { -2422, -342, 3285 } }, { -5881, 738, -6850 } }, 289 } },
    { { { { { 3304, 0, 2419 }, { -202, 4081, 276 }, { -2411, -343, 3293 } }, { -5855, 737, -6867 } }, 289 }, { { { { 3312, 0, 2408 }, { -202, 4081, 278 }, { -2400, -344, 3301 } }, { -5828, 736, -6885 } }, 289 } },
    { { { { { 3320, 0, 2397 }, { -202, 4081, 279 }, { -2389, -345, 3308 } }, { -5801, 735, -6902 } }, 289 }, { { { { 3328, 0, 2387 }, { -201, 4081, 281 }, { -2378, -345, 3316 } }, { -5775, 733, -6920 } }, 289 } },
    { { { { { 3336, 0, 2376 }, { -201, 4081, 282 }, { -2367, -346, 3324 } }, { -5749, 732, -6937 } }, 289 }, { { { { 3343, 0, 2365 }, { -200, 4081, 283 }, { -2356, -347, 3331 } }, { -5722, 731, -6955 } }, 289 } },
    { { { { { 3351, 0, 2354 }, { -200, 4081, 284 }, { -2346, -348, 3339 } }, { -5696, 730, -6972 } }, 289 }, { { { { 3359, 0, 2343 }, { -199, 4081, 285 }, { -2335, -348, 3346 } }, { -5670, 730, -6990 } }, 289 } },
    { { { { { 3366, 0, 2333 }, { -198, 4081, 286 }, { -2324, -349, 3354 } }, { -5643, 729, -7007 } }, 289 }, { { { { 3373, 0, 2322 }, { -198, 4081, 287 }, { -2313, -349, 3361 } }, { -5617, 728, -7024 } }, 289 } },
    { { { { { 3381, 0, 2311 }, { -197, 4081, 288 }, { -2303, -349, 3368 } }, { -5591, 727, -7042 } }, 289 }, { { { { 3388, 0, 2301 }, { -196, 4081, 289 }, { -2293, -349, 3375 } }, { -5566, 726, -7059 } }, 289 } },
    { { { { { 3395, 0, 2290 }, { -195, 4081, 290 }, { -2282, -349, 3382 } }, { -5540, 726, -7077 } }, 289 }, { { { { 3402, 0, 2280 }, { -194, 4081, 290 }, { -2272, -349, 3389 } }, { -5514, 725, -7094 } }, 289 } },
    { { { { { 3409, 0, 2270 }, { -193, 4081, 291 }, { -2261, -349, 3396 } }, { -5489, 724, -7112 } }, 289 }, { { { { 3416, 0, 2259 }, { -192, 4081, 291 }, { -2251, -349, 3403 } }, { -5463, 724, -7129 } }, 289 } },
    { { { { { 3422, 0, 2249 }, { -191, 4081, 292 }, { -2241, -349, 3410 } }, { -5438, 723, -7146 } }, 289 }, { { { { 3429, 0, 2239 }, { -190, 4081, 292 }, { -2231, -349, 3417 } }, { -5413, 723, -7164 } }, 289 } },
    { { { { { 3436, 0, 2229 }, { -189, 4081, 292 }, { -2221, -348, 3423 } }, { -5388, 722, -7181 } }, 289 }, { { { { 3442, 0, 2219 }, { -188, 4081, 292 }, { -2211, -348, 3430 } }, { -5363, 722, -7199 } }, 289 } },
    { { { { { 3449, 0, 2209 }, { -187, 4081, 293 }, { -2201, -348, 3436 } }, { -5338, 721, -7216 } }, 289 }, { { { { 3455, 0, 2199 }, { -186, 4081, 293 }, { -2191, -347, 3442 } }, { -5313, 721, -7234 } }, 289 } },
    { { { { { 3461, 0, 2189 }, { -185, 4081, 293 }, { -2181, -347, 3449 } }, { -5288, 720, -7252 } }, 289 }, { { { { 3467, 0, 2179 }, { -184, 4081, 293 }, { -2171, -346, 3455 } }, { -5263, 720, -7269 } }, 289 } },
    { { { { { 3474, 0, 2169 }, { -183, 4081, 293 }, { -2162, -345, 3461 } }, { -5239, 720, -7287 } }, 289 }, { { { { 3480, 0, 2160 }, { -182, 4081, 293 }, { -2152, -345, 3467 } }, { -5214, 719, -7304 } }, 289 } },
    { { { { { 3486, 0, 2150 }, { -180, 4081, 293 }, { -2142, -344, 3473 } }, { -5190, 719, -7322 } }, 289 }, { { { { 3491, 0, 2140 }, { -179, 4081, 293 }, { -2133, -343, 3479 } }, { -5165, 719, -7340 } }, 289 } },
    { { { { { 3497, 0, 2131 }, { -178, 4081, 293 }, { -2123, -343, 3485 } }, { -5141, 719, -7357 } }, 289 }, { { { { 3503, 0, 2121 }, { -177, 4081, 292 }, { -2114, -342, 3491 } }, { -5116, 718, -7375 } }, 289 } },
    { { { { { 3509, 0, 2112 }, { -176, 4081, 292 }, { -2104, -341, 3497 } }, { -5092, 718, -7393 } }, 289 }, { { { { 3515, 0, 2102 }, { -175, 4081, 292 }, { -2095, -341, 3502 } }, { -5068, 718, -7411 } }, 289 } },
    { { { { { 3520, 0, 2093 }, { -173, 4081, 292 }, { -2086, -340, 3508 } }, { -5044, 718, -7428 } }, 289 }, { { { { 3526, 0, 2083 }, { -172, 4081, 292 }, { -2076, -339, 3514 } }, { -5020, 717, -7446 } }, 289 } },
    { { { { { 3531, 0, 2074 }, { -171, 4081, 292 }, { -2067, -338, 3519 } }, { -4995, 717, -7464 } }, 289 }, { { { { 3537, 0, 2065 }, { -170, 4082, 291 }, { -2058, -338, 3525 } }, { -4971, 717, -7482 } }, 289 } },
    { { { { { 3542, 0, 2055 }, { -169, 4082, 291 }, { -2048, -337, 3530 } }, { -4947, 717, -7500 } }, 289 }, { { { { 3548, 0, 2046 }, { -168, 4082, 291 }, { -2039, -336, 3536 } }, { -4923, 717, -7518 } }, 289 } },
    { { { { { 3553, 0, 2037 }, { -167, 4082, 291 }, { -2030, -335, 3541 } }, { -4899, 716, -7536 } }, 289 }, { { { { 3558, 0, 2028 }, { -166, 4082, 291 }, { -2021, -335, 3546 } }, { -4875, 716, -7554 } }, 289 } },
    { { { { { 3563, 0, 2018 }, { -164, 4082, 291 }, { -2012, -334, 3551 } }, { -4851, 716, -7572 } }, 289 }, { { { { 3569, 0, 2009 }, { -163, 4082, 291 }, { -2003, -334, 3557 } }, { -4827, 716, -7591 } }, 289 } },
    { { { { { 3574, 0, 2000 }, { -162, 4082, 290 }, { -1993, -333, 3562 } }, { -4803, 715, -7609 } }, 289 }, { { { { 3579, 0, 1991 }, { -161, 4082, 290 }, { -1984, -332, 3567 } }, { -4779, 715, -7627 } }, 289 } },
    { { { { { 3584, 0, 1982 }, { -160, 4082, 290 }, { -1975, -332, 3572 } }, { -4755, 715, -7645 } }, 289 }, { { { { 3589, 0, 1972 }, { -159, 4082, 290 }, { -1966, -331, 3577 } }, { -4731, 715, -7664 } }, 289 } },
    { { { { { 3594, 0, 1963 }, { -158, 4082, 290 }, { -1957, -331, 3582 } }, { -4706, 714, -7682 } }, 289 }, { { { { 3599, 0, 1954 }, { -157, 4082, 290 }, { -1948, -331, 3587 } }, { -4682, 714, -7701 } }, 289 } },
    { { { { { 3604, 0, 1945 }, { -157, 4082, 290 }, { -1938, -330, 3592 } }, { -4658, 714, -7719 } }, 289 }, { { { { 3609, 0, 1935 }, { -156, 4082, 291 }, { -1929, -330, 3597 } }, { -4634, 713, -7738 } }, 289 } },
    { { { { { 3614, 0, 1926 }, { -155, 4082, 291 }, { -1920, -330, 3602 } }, { -4610, 713, -7757 } }, 289 }, { { { { 3619, 0, 1917 }, { -154, 4082, 291 }, { -1910, -329, 3607 } }, { -4586, 713, -7775 } }, 289 } },
    { { { { { 3624, 0, 1907 }, { -153, 4082, 291 }, { -1901, -329, 3612 } }, { -4561, 712, -7794 } }, 289 }, { { { { 3629, 0, 1898 }, { -152, 4082, 292 }, { -1892, -329, 3617 } }, { -4537, 712, -7813 } }, 289 } },
    { { { { { 3634, 0, 1888 }, { -152, 4082, 292 }, { -1882, -329, 3622 } }, { -4513, 711, -7832 } }, 289 }, { { { { 3639, 0, 1879 }, { -151, 4082, 292 }, { -1873, -329, 3627 } }, { -4488, 711, -7851 } }, 289 } },
    { { { { { 3644, 0, 1869 }, { -150, 4082, 293 }, { -1863, -329, 3632 } }, { -4464, 710, -7870 } }, 289 }, { { { { 3649, 0, 1860 }, { -149, 4082, 294 }, { -1854, -329, 3637 } }, { -4440, 710, -7890 } }, 289 } },
    { { { { { 3654, 0, 1850 }, { -149, 4082, 294 }, { -1844, -330, 3642 } }, { -4415, 709, -7909 } }, 289 }, { { { { 3659, 0, 1840 }, { -148, 4082, 295 }, { -1834, -330, 3647 } }, { -4390, 708, -7928 } }, 289 } },
    { { { { { 3664, 0, 1830 }, { -147, 4082, 296 }, { -1824, -330, 3652 } }, { -4366, 708, -7947 } }, 289 }, { { { { 3669, 0, 1820 }, { -147, 4082, 296 }, { -1814, -331, 3657 } }, { -4341, 707, -7967 } }, 289 } },
    { { { { { 3674, 0, 1810 }, { -146, 4082, 297 }, { -1804, -331, 3661 } }, { -4316, 706, -7986 } }, 289 }, { { { { 3679, 0, 1800 }, { -146, 4082, 298 }, { -1794, -332, 3666 } }, { -4291, 705, -8006 } }, 289 } },
    { { { { { 3683, 0, 1790 }, { -145, 4082, 299 }, { -1784, -333, 3671 } }, { -4266, 705, -8026 } }, 289 }, { { { { 3688, 0, 1780 }, { -145, 4082, 300 }, { -1774, -333, 3676 } }, { -4242, 704, -8045 } }, 289 } },
    { { { { { 3693, 0, 1770 }, { -144, 4082, 301 }, { -1764, -334, 3681 } }, { -4217, 703, -8065 } }, 289 }, { { { { 3698, 0, 1759 }, { -143, 4082, 302 }, { -1753, -335, 3686 } }, { -4192, 702, -8085 } }, 289 } },
    { { { { { 3703, 0, 1749 }, { -143, 4082, 303 }, { -1743, -335, 3691 } }, { -4167, 701, -8105 } }, 289 }, { { { { 3708, 0, 1738 }, { -142, 4082, 304 }, { -1733, -336, 3696 } }, { -4142, 700, -8124 } }, 289 } },
    { { { { { 3713, 0, 1728 }, { -142, 4082, 305 }, { -1722, -337, 3700 } }, { -4117, 700, -8144 } }, 289 }, { { { { 3718, 0, 1717 }, { -141, 4082, 307 }, { -1711, -338, 3705 } }, { -4092, 699, -8164 } }, 289 } },
    { { { { { 3723, 0, 1707 }, { -141, 4081, 308 }, { -1701, -339, 3710 } }, { -4067, 698, -8184 } }, 289 }, { { { { 3728, 0, 1696 }, { -140, 4081, 309 }, { -1690, -340, 3715 } }, { -4042, 697, -8204 } }, 289 } },
    { { { { { 3732, 0, 1685 }, { -140, 4081, 310 }, { -1680, -341, 3719 } }, { -4017, 696, -8225 } }, 289 }, { { { { 3737, 0, 1675 }, { -139, 4081, 312 }, { -1669, -342, 3724 } }, { -3992, 695, -8245 } }, 289 } },
    { { { { { 3742, 0, 1664 }, { -139, 4081, 313 }, { -1658, -343, 3729 } }, { -3967, 694, -8265 } }, 289 }, { { { { 3747, 0, 1653 }, { -138, 4081, 314 }, { -1647, -343, 3734 } }, { -3942, 693, -8285 } }, 289 } },
    { { { { { 3752, 0, 1642 }, { -138, 4081, 316 }, { -1636, -344, 3738 } }, { -3917, 692, -8306 } }, 289 }, { { { { 3756, 0, 1631 }, { -137, 4081, 317 }, { -1625, -346, 3743 } }, { -3892, 691, -8326 } }, 289 } },
    { { { { { 3761, 0, 1620 }, { -137, 4081, 318 }, { -1614, -347, 3748 } }, { -3867, 690, -8346 } }, 289 }, { { { { 3766, 0, 1609 }, { -136, 4081, 320 }, { -1603, -348, 3752 } }, { -3842, 689, -8367 } }, 289 } },
    { { { { { 3771, 0, 1598 }, { -136, 4081, 321 }, { -1592, -349, 3757 } }, { -3817, 688, -8387 } }, 289 }, { { { { 3775, 0, 1587 }, { -135, 4081, 322 }, { -1581, -350, 3762 } }, { -3792, 688, -8408 } }, 289 } },
    { { { { { 3780, 0, 1576 }, { -135, 4080, 324 }, { -1570, -351, 3766 } }, { -3767, 687, -8428 } }, 289 }, { { { { 3785, 0, 1565 }, { -134, 4080, 325 }, { -1559, -352, 3771 } }, { -3742, 686, -8449 } }, 289 } },
    { { { { { 3789, 0, 1554 }, { -133, 4080, 326 }, { -1548, -353, 3775 } }, { -3718, 685, -8470 } }, 289 }, { { { { 3794, 0, 1542 }, { -133, 4080, 328 }, { -1537, -354, 3780 } }, { -3693, 684, -8490 } }, 289 } },
    { { { { { 3798, 0, 1531 }, { -132, 4080, 329 }, { -1525, -355, 3784 } }, { -3669, 683, -8511 } }, 289 }, { { { { 3803, 0, 1520 }, { -132, 4080, 330 }, { -1514, -356, 3788 } }, { -3644, 682, -8532 } }, 289 } },
    { { { { { 3807, 0, 1509 }, { -131, 4080, 331 }, { -1503, -357, 3793 } }, { -3620, 681, -8552 } }, 289 }, { { { { 3812, 0, 1498 }, { -130, 4080, 333 }, { -1492, -357, 3797 } }, { -3595, 680, -8573 } }, 289 } },
    { { { { { 3816, 0, 1486 }, { -130, 4080, 334 }, { -1481, -358, 3801 } }, { -3571, 679, -8594 } }, 289 }, { { { { 3820, 0, 1475 }, { -129, 4080, 335 }, { -1470, -359, 3806 } }, { -3547, 679, -8615 } }, 289 } },
    { { { { { 3825, 0, 1464 }, { -128, 4080, 336 }, { -1458, -360, 3810 } }, { -3523, 678, -8636 } }, 289 }, { { { { 3829, 0, 1453 }, { -128, 4080, 337 }, { -1447, -361, 3814 } }, { -3499, 677, -8657 } }, 289 } },
    { { { { { 3833, 0, 1442 }, { -127, 4079, 339 }, { -1436, -362, 3818 } }, { -3475, 676, -8678 } }, 289 }, { { { { 3837, 0, 1431 }, { -126, 4079, 340 }, { -1425, -362, 3822 } }, { -3451, 675, -8699 } }, 289 } },
    { { { { { 3841, 0, 1420 }, { -126, 4079, 341 }, { -1414, -363, 3826 } }, { -3427, 675, -8720 } }, 289 }, { { { { 3846, 0, 1408 }, { -125, 4079, 342 }, { -1403, -364, 3830 } }, { -3404, 674, -8741 } }, 289 } },
    { { { { { 3850, 0, 1397 }, { -124, 4079, 342 }, { -1392, -364, 3834 } }, { -3380, 673, -8762 } }, 289 }, { { { { 3853, 0, 1387 }, { -123, 4079, 343 }, { -1381, -365, 3838 } }, { -3357, 673, -8783 } }, 289 } },
    { { { { { 3857, 0, 1376 }, { -122, 4079, 344 }, { -1370, -365, 3842 } }, { -3334, 672, -8804 } }, 289 }, { { { { 3861, 0, 1365 }, { -122, 4079, 345 }, { -1359, -366, 3846 } }, { -3311, 672, -8825 } }, 289 } },
    { { { { { 3865, 0, 1354 }, { -121, 4079, 346 }, { -1349, -366, 3849 } }, { -3288, 671, -8846 } }, 289 }, { { { { 3869, 0, 1344 }, { -120, 4079, 346 }, { -1338, -367, 3853 } }, { -3265, 670, -8867 } }, 289 } },
    { { { { { 3872, 0, 1333 }, { -119, 4079, 347 }, { -1328, -367, 3857 } }, { -3242, 670, -8888 } }, 289 }, { { { { 3876, 0, 1323 }, { -118, 4079, 347 }, { -1317, -367, 3860 } }, { -3220, 670, -8910 } }, 289 } },
    { { { { { 3880, 0, 1312 }, { -117, 4079, 348 }, { -1307, -367, 3864 } }, { -3197, 669, -8931 } }, 289 }, { { { { 3883, 0, 1301 }, { -116, 4079, 348 }, { -1296, -367, 3867 } }, { -3173, 669, -8953 } }, 289 } },
    { { { { { 3887, 0, 1290 }, { -115, 4079, 349 }, { -1285, -368, 3871 } }, { -3149, 668, -8977 } }, 289 }, { { { { 3891, 0, 1279 }, { -114, 4079, 349 }, { -1273, -368, 3875 } }, { -3124, 668, -9000 } }, 289 } },
    { { { { { 3894, 0, 1267 }, { -113, 4079, 350 }, { -1262, -368, 3879 } }, { -3099, 667, -9025 } }, 289 }, { { { { 3898, 0, 1255 }, { -112, 4079, 350 }, { -1250, -368, 3883 } }, { -3073, 667, -9050 } }, 289 } },
    { { { { { 3902, 0, 1243 }, { -111, 4079, 350 }, { -1238, -368, 3886 } }, { -3047, 667, -9076 } }, 289 }, { { { { 3906, 0, 1231 }, { -110, 4079, 350 }, { -1226, -368, 3890 } }, { -3021, 666, -9102 } }, 289 } },
    { { { { { 3910, 0, 1218 }, { -109, 4079, 351 }, { -1213, -367, 3894 } }, { -2994, 666, -9129 } }, 289 }, { { { { 3914, 0, 1206 }, { -108, 4079, 351 }, { -1201, -367, 3898 } }, { -2967, 666, -9156 } }, 289 } },
    { { { { { 3918, 0, 1193 }, { -107, 4079, 351 }, { -1188, -367, 3902 } }, { -2939, 665, -9183 } }, 289 }, { { { { 3922, 0, 1180 }, { -105, 4079, 351 }, { -1176, -367, 3906 } }, { -2912, 665, -9211 } }, 289 } },
    { { { { { 3925, 0, 1167 }, { -104, 4079, 351 }, { -1163, -367, 3910 } }, { -2884, 665, -9239 } }, 289 }, { { { { 3929, 0, 1155 }, { -103, 4079, 351 }, { -1150, -366, 3913 } }, { -2856, 665, -9268 } }, 289 } },
    { { { { { 3933, 0, 1142 }, { -102, 4079, 352 }, { -1137, -366, 3917 } }, { -2829, 664, -9296 } }, 289 }, { { { { 3937, 0, 1129 }, { -101, 4079, 352 }, { -1125, -366, 3921 } }, { -2801, 664, -9325 } }, 289 } },
    { { { { { 3940, 0, 1116 }, { -99, 4079, 352 }, { -1112, -365, 3925 } }, { -2773, 664, -9353 } }, 289 }, { { { { 3944, 0, 1104 }, { -98, 4079, 351 }, { -1099, -365, 3928 } }, { -2745, 664, -9382 } }, 289 } },
    { { { { { 3947, 0, 1091 }, { -97, 4079, 351 }, { -1087, -365, 3932 } }, { -2718, 664, -9411 } }, 289 }, { { { { 3951, 0, 1079 }, { -96, 4079, 351 }, { -1074, -364, 3935 } }, { -2691, 663, -9439 } }, 289 } },
    { { { { { 3954, 0, 1066 }, { -94, 4079, 351 }, { -1062, -364, 3939 } }, { -2664, 663, -9468 } }, 289 }, { { { { 3957, 0, 1054 }, { -93, 4079, 351 }, { -1050, -363, 3942 } }, { -2637, 663, -9496 } }, 289 } },
    { { { { { 3961, 0, 1042 }, { -92, 4079, 351 }, { -1038, -363, 3945 } }, { -2611, 663, -9524 } }, 289 }, { { { { 3964, 0, 1030 }, { -91, 4079, 351 }, { -1026, -362, 3948 } }, { -2585, 663, -9552 } }, 289 } },
    { { { { { 3967, 0, 1018 }, { -90, 4079, 350 }, { -1014, -362, 3951 } }, { -2560, 663, -9579 } }, 289 }, { { { { 3970, 0, 1007 }, { -88, 4079, 350 }, { -1003, -361, 3954 } }, { -2535, 663, -9607 } }, 289 } },
    { { { { { 3972, 0, 996 }, { -87, 4080, 350 }, { -992, -361, 3957 } }, { -2510, 663, -9633 } }, 289 }, { { { { 3975, 0, 985 }, { -86, 4080, 350 }, { -981, -360, 3960 } }, { -2486, 663, -9659 } }, 289 } },
    { { { { { 3978, 0, 974 }, { -85, 4080, 349 }, { -970, -360, 3962 } }, { -2463, 663, -9685 } }, 289 }, { { { { 3980, 0, 964 }, { -84, 4080, 349 }, { -960, -359, 3965 } }, { -2441, 663, -9710 } }, 289 } },
    { { { { { 3983, 0, 954 }, { -83, 4080, 349 }, { -950, -359, 3967 } }, { -2419, 663, -9734 } }, 289 }, { { { { 3985, 0, 944 }, { -82, 4080, 348 }, { -941, -358, 3970 } }, { -2398, 663, -9758 } }, 289 } },
    { { { { { 3987, 0, 935 }, { -81, 4080, 348 }, { -932, -357, 3972 } }, { -2378, 663, -9781 } }, 289 }, { { { { 3989, 0, 926 }, { -80, 4080, 348 }, { -923, -357, 3974 } }, { -2358, 663, -9803 } }, 289 } },
    { { { { { 3991, 0, 918 }, { -79, 4080, 347 }, { -914, -356, 3976 } }, { -2340, 664, -9824 } }, 289 }, { { { { 3993, 0, 910 }, { -79, 4080, 347 }, { -906, -356, 3978 } }, { -2323, 664, -9845 } }, 289 } },
    { { { { { 3995, 0, 902 }, { -78, 4080, 346 }, { -899, -355, 3980 } }, { -2306, 664, -9864 } }, 289 }, { { { { 3996, 0, 895 }, { -77, 4080, 346 }, { -892, -354, 3981 } }, { -2291, 664, -9883 } }, 289 } },
    { { { { { 3998, 0, 888 }, { -76, 4080, 345 }, { -885, -354, 3983 } }, { -2276, 664, -9900 } }, 289 }, { { { { 3999, 0, 882 }, { -76, 4080, 345 }, { -879, -353, 3984 } }, { -2263, 665, -9916 } }, 289 } },
    { { { { { 4000, 0, 877 }, { -75, 4080, 345 }, { -873, -353, 3986 } }, { -2251, 665, -9931 } }, 289 }, { { { { 4002, 0, 872 }, { -75, 4080, 344 }, { -868, -352, 3987 } }, { -2240, 665, -9945 } }, 289 } },
    { { { { { 4003, 0, 867 }, { -74, 4080, 344 }, { -864, -352, 3988 } }, { -2230, 665, -9957 } }, 289 }, { { { { 4003, 0, 863 }, { -74, 4080, 344 }, { -860, -352, 3989 } }, { -2222, 665, -9968 } }, 289 } },
    { { { { { 4004, 0, 860 }, { -73, 4080, 343 }, { -857, -351, 3989 } }, { -2215, 666, -9978 } }, 289 }, { { { { 4005, 0, 857 }, { -73, 4080, 343 }, { -854, -351, 3990 } }, { -2209, 666, -9986 } }, 289 } },
    { { { { { 4005, 0, 855 }, { -73, 4080, 343 }, { -852, -351, 3990 } }, { -2204, 666, -9992 } }, 289 }, { { { { 4006, 0, 853 }, { -73, 4080, 343 }, { -850, -350, 3991 } }, { -2201, 666, -9996 } }, 289 } },
    { { { { { 4006, 0, 852 }, { -73, 4080, 343 }, { -849, -350, 3991 } }, { -2199, 666, -9999 } }, 289 }, { { { { 4006, 0, 852 }, { -72, 4080, 342 }, { -849, -350, 3991 } }, { -2198, 666, -0x2710 } }, 289 } },
};

ViewCamera D_acropolis_plaza_8018A938[120][2] = {
    { { { { { 3256, 0, 2484 }, { -204, 4082, 267 }, { -2476, -336, 3245 } }, { -6007, 744, -6769 } }, 289 }, { { { { 3256, 0, 2484 }, { -204, 4082, 267 }, { -2476, -336, 3245 } }, { -6007, 744, -6769 } }, 289 } },
    { { { { { 3206, 0, 2548 }, { -216, 4081, 272 }, { -2539, -348, 3194 } }, { -6007, 745, -6772 } }, 289 }, { { { { 3206, 0, 2548 }, { -216, 4081, 272 }, { -2539, -348, 3194 } }, { -6006, 747, -6781 } }, 289 } },
    { { { { { 3049, 0, 2734 }, { -256, 4077, 285 }, { -2722, -383, 3036 } }, { -6006, 751, -6797 } }, 289 }, { { { { 3049, 0, 2734 }, { -256, 4077, 285 }, { -2722, -383, 3036 } }, { -6005, 757, -6818 } }, 289 } },
    { { { { { 2763, 0, 3023 }, { -324, 4072, 296 }, { -3006, -439, 2747 } }, { -6004, 763, -6845 } }, 289 }, { { { { 2763, 0, 3023 }, { -324, 4072, 296 }, { -3006, -439, 2747 } }, { -6003, 771, -6878 } }, 289 } },
    { { { { { 2317, 0, 3377 }, { -420, 4064, 288 }, { -3351, -509, 2299 } }, { -6002, 781, -6916 } }, 289 }, { { { { 2317, 0, 3377 }, { -420, 4064, 288 }, { -3351, -509, 2299 } }, { -6000, 791, -6958 } }, 289 } },
    { { { { { 1703, 0, 3724 }, { -529, 4054, 242 }, { -3687, -582, 1686 } }, { -5999, 803, -7006 } }, 289 }, { { { { 1703, 0, 3724 }, { -529, 4054, 242 }, { -3687, -582, 1686 } }, { -5997, 816, -7058 } }, 289 } },
    { { { { { 965, 0, 3980 }, { -624, 4045, 151 }, { -3931, -642, 953 } }, { -5995, 830, -7115 } }, 289 }, { { { { 965, 0, 3980 }, { -624, 4045, 151 }, { -3931, -642, 953 } }, { -5993, 845, -7176 } }, 289 } },
    { { { { { 202, 0, 4090 }, { -679, 4039, 33 }, { -4034, -679, 199 } }, { -5990, 861, -7241 } }, 289 }, { { { { 202, 0, 4090 }, { -679, 4039, 33 }, { -4034, -679, 199 } }, { -5988, 878, -7310 } }, 289 } },
    { { { { { -487, 0, 4066 }, { -688, 4036, -82 }, { -4008, -693, -480 } }, { -5986, 896, -7382 } }, 289 }, { { { { -487, 0, 4066 }, { -688, 4036, -82 }, { -4008, -693, -480 } }, { -5983, 915, -7457 } }, 289 } },
    { { { { { -1054, 0, 3957 }, { -666, 4037, -177 }, { -3901, -689, -1039 } }, { -5980, 934, -7536 } }, 289 }, { { { { -1054, 0, 3957 }, { -666, 4037, -177 }, { -3901, -689, -1039 } }, { -5977, 954, -7618 } }, 289 } },
    { { { { { -1495, 0, 3813 }, { -630, 4039, -247 }, { -3760, -676, -1474 } }, { -5974, 975, -7703 } }, 289 }, { { { { -1495, 0, 3813 }, { -630, 4039, -247 }, { -3760, -676, -1474 } }, { -5971, 997, -7790 } }, 289 } },
    { { { { { -1830, 0, 3664 }, { -591, 4042, -295 }, { -3616, -660, -1806 } }, { -5968, 1019, -7879 } }, 289 }, { { { { -1830, 0, 3664 }, { -591, 4042, -295 }, { -3616, -660, -1806 } }, { -5965, 1042, -7971 } }, 289 } },
    { { { { { -2085, 0, 3525 }, { -554, 4044, -328 }, { -3481, -644, -2059 } }, { -5962, 1065, -8064 } }, 289 }, { { { { -2085, 0, 3525 }, { -554, 4044, -328 }, { -3481, -644, -2059 } }, { -5958, 1088, -8160 } }, 289 } },
    { { { { { -2280, 0, 3402 }, { -522, 4047, -350 }, { -3362, -629, -2253 } }, { -5955, 1112, -8257 } }, 289 }, { { { { -2280, 0, 3402 }, { -522, 4047, -350 }, { -3362, -629, -2253 } }, { -5952, 1137, -8355 } }, 289 } },
    { { { { { -2431, 0, 3295 }, { -495, 4049, -365 }, { -3258, -615, -2404 } }, { -5948, 1161, -8454 } }, 289 }, { { { { -2431, 0, 3295 }, { -495, 4049, -365 }, { -3258, -615, -2404 } }, { -5945, 1186, -8554 } }, 289 } },
    { { { { { -2550, 0, 3204 }, { -472, 4051, -375 }, { -3169, -603, -2522 } }, { -5941, 1211, -8655 } }, 289 }, { { { { -2550, 0, 3204 }, { -472, 4051, -375 }, { -3169, -603, -2522 } }, { -5938, 1236, -8756 } }, 289 } },
    { { { { { -2645, 0, 3126 }, { -452, 4052, -383 }, { -3093, -593, -2617 } }, { -5934, 1261, -8858 } }, 289 }, { { { { -2645, 0, 3126 }, { -452, 4052, -383 }, { -3093, -593, -2617 } }, { -5931, 1286, -8960 } }, 289 } },
    { { { { { -2722, 0, 3060 }, { -436, 4054, -388 }, { -3029, -584, -2694 } }, { -5927, 1311, -9061 } }, 289 }, { { { { -2722, 0, 3060 }, { -436, 4054, -388 }, { -3029, -584, -2694 } }, { -5924, 1336, -9163 } }, 289 } },
    { { { { { -2784, 0, 3003 }, { -422, 4055, -391 }, { -2973, -576, -2757 } }, { -5920, 1361, -9263 } }, 289 }, { { { { -2784, 0, 3003 }, { -422, 4055, -391 }, { -2973, -576, -2757 } }, { -5917, 1386, -9363 } }, 289 } },
    { { { { { -2836, 0, 2955 }, { -411, 4056, -394 }, { -2926, -569, -2808 } }, { -5913, 1410, -9462 } }, 289 }, { { { { -2836, 0, 2955 }, { -411, 4056, -394 }, { -2926, -569, -2808 } }, { -5910, 1435, -9560 } }, 289 } },
    { { { { { -2878, 0, 2913 }, { -401, 4056, -396 }, { -2885, -563, -2851 } }, { -5906, 1458, -9657 } }, 289 }, { { { { -2878, 0, 2913 }, { -401, 4056, -396 }, { -2885, -563, -2851 } }, { -5903, 1482, -9752 } }, 289 } },
    { { { { { -2914, 0, 2877 }, { -392, 4057, -397 }, { -2850, -558, -2887 } }, { -5900, 1505, -9845 } }, 289 }, { { { { -2914, 0, 2877 }, { -392, 4057, -397 }, { -2850, -558, -2887 } }, { -5897, 1527, -9936 } }, 289 } },
    { { { { { -2944, 0, 2847 }, { -385, 4058, -398 }, { -2821, -554, -2917 } }, { -5894, 1549, -0x2729 } }, 289 }, { { { { -2944, 0, 2847 }, { -385, 4058, -398 }, { -2821, -554, -2917 } }, { -5891, 1571, -0x277F } }, 289 } },
    { { { { { -2969, 0, 2821 }, { -379, 4058, -399 }, { -2795, -550, -2942 } }, { -5888, 1592, -0x27D3 } }, 289 }, { { { { -2969, 0, 2821 }, { -379, 4058, -399 }, { -2795, -550, -2942 } }, { -5885, 1612, -0x2825 } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5882, 1631, -0x2873 } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1645, -0x28AB } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1647, -0x28B5 } }, 289 }, { { { { -2990, 0, 2799 }, { -374, 4059, -399 }, { -2773, -547, -2963 } }, { -5880, 1650, -0x28BE } }, 289 } },
    { { { { { -2980, 0, 2809 }, { -376, 4059, -399 }, { -2784, -549, -2953 } }, { -5881, 1641, -0x2899 } }, 289 }, { { { { -2980, 0, 2809 }, { -376, 4059, -399 }, { -2784, -549, -2953 } }, { -5882, 1631, -0x2874 } }, 289 } },
    { { { { { -2970, 0, 2820 }, { -379, 4058, -399 }, { -2795, -550, -2943 } }, { -5883, 1622, -0x284F } }, 289 }, { { { { -2970, 0, 2820 }, { -379, 4058, -399 }, { -2795, -550, -2943 } }, { -5885, 1613, -0x2828 } }, 289 } },
    { { { { { -2958, 0, 2832 }, { -382, 4058, -399 }, { -2806, -552, -2931 } }, { -5886, 1603, -0x2801 } }, 289 }, { { { { -2958, 0, 2832 }, { -382, 4058, -399 }, { -2806, -552, -2931 } }, { -5887, 1593, -0x27D9 } }, 289 } },
    { { { { { -2945, 0, 2845 }, { -385, 4058, -398 }, { -2819, -554, -2918 } }, { -5889, 1583, -0x27B1 } }, 289 }, { { { { -2945, 0, 2845 }, { -385, 4058, -398 }, { -2819, -554, -2918 } }, { -5890, 1573, -0x2788 } }, 289 } },
    { { { { { -2932, 0, 2860 }, { -388, 4058, -398 }, { -2833, -556, -2904 } }, { -5892, 1563, -0x275E } }, 289 }, { { { { -2932, 0, 2860 }, { -388, 4058, -398 }, { -2833, -556, -2904 } }, { -5893, 1552, -0x2734 } }, 289 } },
    { { { { { -2917, 0, 2875 }, { -392, 4057, -397 }, { -2848, -558, -2889 } }, { -5895, 1542, -9994 } }, 289 }, { { { { -2917, 0, 2875 }, { -392, 4057, -397 }, { -2848, -558, -2889 } }, { -5896, 1531, -9951 } }, 289 } },
    { { { { { -2901, 0, 2891 }, { -395, 4057, -397 }, { -2864, -560, -2873 } }, { -5898, 1520, -9907 } }, 289 }, { { { { -2901, 0, 2891 }, { -395, 4057, -397 }, { -2864, -560, -2873 } }, { -5899, 1509, -9863 } }, 289 } },
    { { { { { -2883, 0, 2909 }, { -400, 4057, -396 }, { -2881, -563, -2855 } }, { -5901, 1498, -9818 } }, 289 }, { { { { -2883, 0, 2909 }, { -400, 4057, -396 }, { -2881, -563, -2855 } }, { -5902, 1487, -9773 } }, 289 } },
    { { { { { -2864, 0, 2928 }, { -404, 4056, -395 }, { -2900, -565, -2836 } }, { -5904, 1476, -9728 } }, 289 }, { { { { -2864, 0, 2928 }, { -404, 4056, -395 }, { -2900, -565, -2836 } }, { -5906, 1465, -9682 } }, 289 } },
    { { { { { -2843, 0, 2948 }, { -409, 4056, -394 }, { -2919, -568, -2815 } }, { -5907, 1453, -9636 } }, 289 }, { { { { -2843, 0, 2948 }, { -409, 4056, -394 }, { -2919, -568, -2815 } }, { -5909, 1442, -9589 } }, 289 } },
    { { { { { -2820, 0, 2970 }, { -414, 4055, -393 }, { -2941, -571, -2792 } }, { -5910, 1430, -9543 } }, 289 }, { { { { -2820, 0, 2970 }, { -414, 4055, -393 }, { -2941, -571, -2792 } }, { -5912, 1419, -9496 } }, 289 } },
    { { { { { -2795, 0, 2994 }, { -420, 4055, -392 }, { -2964, -575, -2767 } }, { -5914, 1407, -9448 } }, 289 }, { { { { -2795, 0, 2994 }, { -420, 4055, -392 }, { -2964, -575, -2767 } }, { -5915, 1395, -9401 } }, 289 } },
    { { { { { -2767, 0, 3019 }, { -426, 4054, -390 }, { -2989, -578, -2739 } }, { -5917, 1383, -9353 } }, 289 }, { { { { -2767, 0, 3019 }, { -426, 4054, -390 }, { -2989, -578, -2739 } }, { -5919, 1371, -9305 } }, 289 } },
    { { { { { -2737, 0, 3046 }, { -433, 4054, -389 }, { -3015, -582, -2709 } }, { -5920, 1360, -9257 } }, 289 }, { { { { -2737, 0, 3046 }, { -433, 4054, -389 }, { -3015, -582, -2709 } }, { -5922, 1348, -9208 } }, 289 } },
    { { { { { -2704, 0, 3076 }, { -440, 4053, -387 }, { -3044, -586, -2676 } }, { -5924, 1336, -9160 } }, 289 }, { { { { -2704, 0, 3076 }, { -440, 4053, -387 }, { -3044, -586, -2676 } }, { -5925, 1324, -9111 } }, 289 } },
    { { { { { -2667, 0, 3108 }, { -448, 4053, -384 }, { -3075, -590, -2639 } }, { -5927, 1312, -9063 } }, 289 }, { { { { -2667, 0, 3108 }, { -448, 4053, -384 }, { -3075, -590, -2639 } }, { -5929, 1299, -9014 } }, 289 } },
    { { { { { -2627, 0, 3142 }, { -456, 4052, -381 }, { -3109, -595, -2599 } }, { -5930, 1287, -8965 } }, 289 }, { { { { -2627, 0, 3142 }, { -456, 4052, -381 }, { -3109, -595, -2599 } }, { -5932, 1275, -8916 } }, 289 } },
    { { { { { -2582, 0, 3179 }, { -465, 4051, -378 }, { -3145, -600, -2554 } }, { -5934, 1263, -8867 } }, 289 }, { { { { -2582, 0, 3179 }, { -465, 4051, -378 }, { -3145, -600, -2554 } }, { -5936, 1251, -8819 } }, 289 } },
    { { { { { -2532, 0, 3219 }, { -476, 4050, -374 }, { -3184, -605, -2504 } }, { -5937, 1239, -8770 } }, 289 }, { { { { -2532, 0, 3219 }, { -476, 4050, -374 }, { -3184, -605, -2504 } }, { -5939, 1227, -8721 } }, 289 } },
    { { { { { -2476, 0, 3262 }, { -486, 4050, -369 }, { -3226, -611, -2448 } }, { -5941, 1215, -8673 } }, 289 }, { { { { -2476, 0, 3262 }, { -486, 4050, -369 }, { -3226, -611, -2448 } }, { -5942, 1203, -8624 } }, 289 } },
    { { { { { -2413, 0, 3309 }, { -498, 4049, -363 }, { -3271, -617, -2386 } }, { -5944, 1191, -8576 } }, 289 }, { { { { -2413, 0, 3309 }, { -498, 4049, -363 }, { -3271, -617, -2386 } }, { -5946, 1179, -8528 } }, 289 } },
    { { { { { -2343, 0, 3359 }, { -511, 4048, -356 }, { -3319, -623, -2316 } }, { -5947, 1167, -8480 } }, 289 }, { { { { -2343, 0, 3359 }, { -511, 4048, -356 }, { -3319, -623, -2316 } }, { -5949, 1156, -8432 } }, 289 } },
    { { { { { -2264, 0, 3412 }, { -525, 4047, -348 }, { -3372, -630, -2237 } }, { -5951, 1144, -8384 } }, 289 }, { { { { -2264, 0, 3412 }, { -525, 4047, -348 }, { -3372, -630, -2237 } }, { -5952, 1132, -8337 } }, 289 } },
    { { { { { -2175, 0, 3470 }, { -540, 4046, -338 }, { -3428, -637, -2149 } }, { -5954, 1120, -8290 } }, 289 }, { { { { -2175, 0, 3470 }, { -540, 4046, -338 }, { -3428, -637, -2149 } }, { -5956, 1109, -8243 } }, 289 } },
    { { { { { -2074, 0, 3531 }, { -556, 4044, -326 }, { -3487, -645, -2048 } }, { -5957, 1097, -8197 } }, 289 }, { { { { -2074, 0, 3531 }, { -556, 4044, -326 }, { -3487, -645, -2048 } }, { -5959, 1086, -8150 } }, 289 } },
    { { { { { -1959, 0, 3596 }, { -573, 4043, -312 }, { -3550, -653, -1934 } }, { -5960, 1075, -8105 } }, 289 }, { { { { -1959, 0, 3596 }, { -573, 4043, -312 }, { -3550, -653, -1934 } }, { -5962, 1063, -8059 } }, 289 } },
    { { { { { -1828, 0, 3665 }, { -591, 4042, -295 }, { -3617, -660, -1804 } }, { -5964, 1052, -8014 } }, 289 }, { { { { -1828, 0, 3665 }, { -591, 4042, -295 }, { -3617, -660, -1804 } }, { -5965, 1041, -7970 } }, 289 } },
    { { { { { -1679, 0, 3735 }, { -609, 4041, -274 }, { -3685, -668, -1657 } }, { -5967, 1030, -7926 } }, 289 }, { { { { -1679, 0, 3735 }, { -609, 4041, -274 }, { -3685, -668, -1657 } }, { -5968, 1020, -7882 } }, 289 } },
    { { { { { -1509, 0, 3807 }, { -628, 4039, -249 }, { -3755, -676, -1488 } }, { -5970, 1009, -7839 } }, 289 }, { { { { -1509, 0, 3807 }, { -628, 4039, -249 }, { -3755, -676, -1488 } }, { -5971, 998, -7796 } }, 289 } },
    { { { { { -1314, 0, 3879 }, { -646, 4038, -219 }, { -3824, -683, -1296 } }, { -5973, 988, -7754 } }, 289 }, { { { { -1314, 0, 3879 }, { -646, 4038, -219 }, { -3824, -683, -1296 } }, { -5974, 978, -7713 } }, 289 } },
    { { { { { -1094, 0, 3947 }, { -663, 4037, -183 }, { -3890, -688, -1078 } }, { -5975, 968, -7672 } }, 289 }, { { { { -1094, 0, 3947 }, { -663, 4037, -183 }, { -3890, -688, -1078 } }, { -5977, 958, -7631 } }, 289 } },
    { { { { { -844, 0, 4007 }, { -677, 4037, -142 }, { -3950, -692, -832 } }, { -5978, 948, -7592 } }, 289 }, { { { { -844, 0, 4007 }, { -677, 4037, -142 }, { -3950, -692, -832 } }, { -5980, 938, -7553 } }, 289 } },
    { { { { { -564, 0, 4056 }, { -686, 4036, -95 }, { -3998, -693, -556 } }, { -5981, 929, -7514 } }, 289 }, { { { { -564, 0, 4056 }, { -686, 4036, -95 }, { -3998, -693, -556 } }, { -5982, 919, -7477 } }, 289 } },
    { { { { { -255, 0, 4088 }, { -689, 4037, -43 }, { -4029, -690, -252 } }, { -5984, 910, -7440 } }, 289 }, { { { { -255, 0, 4088 }, { -689, 4037, -43 }, { -4029, -690, -252 } }, { -5985, 901, -7403 } }, 289 } },
    { { { { { 79, 0, 4095 }, { -683, 4038, 13 }, { -4037, -683, 78 } }, { -5986, 893, -7368 } }, 289 }, { { { { 79, 0, 4095 }, { -683, 4038, 13 }, { -4037, -683, 78 } }, { -5987, 884, -7333 } }, 289 } },
    { { { { { 435, 0, 4072 }, { -667, 4040, 71 }, { -4017, -671, 429 } }, { -5988, 876, -7299 } }, 289 }, { { { { 435, 0, 4072 }, { -667, 4040, 71 }, { -4017, -671, 429 } }, { -5990, 867, -7266 } }, 289 } },
    { { { { { 802, 0, 4016 }, { -640, 4043, 127 }, { -3965, -652, 792 } }, { -5991, 859, -7234 } }, 289 }, { { { { 802, 0, 4016 }, { -640, 4043, 127 }, { -3965, -652, 792 } }, { -5992, 852, -7203 } }, 289 } },
    { { { { { 1169, 0, 3925 }, { -602, 4047, 179 }, { -3878, -628, 1155 } }, { -5993, 844, -7172 } }, 289 }, { { { { 1169, 0, 3925 }, { -602, 4047, 179 }, { -3878, -628, 1155 } }, { -5994, 837, -7143 } }, 289 } },
    { { { { { 1523, 0, 3802 }, { -556, 4051, 223 }, { -3761, -599, 1507 } }, { -5995, 830, -7114 } }, 289 }, { { { { 1523, 0, 3802 }, { -556, 4051, 223 }, { -3761, -599, 1507 } }, { -5996, 823, -7087 } }, 289 } },
    { { { { { 1853, 0, 3652 }, { -505, 4056, 256 }, { -3617, -566, 1835 } }, { -5997, 816, -7060 } }, 289 }, { { { { 1853, 0, 3652 }, { -505, 4056, 256 }, { -3617, -566, 1835 } }, { -5998, 810, -7035 } }, 289 } },
    { { { { { 2150, 0, 3486 }, { -452, 4061, 279 }, { -3456, -531, 2132 } }, { -5999, 804, -7010 } }, 289 }, { { { { 2150, 0, 3486 }, { -452, 4061, 279 }, { -3456, -531, 2132 } }, { -5999, 798, -6986 } }, 289 } },
    { { { { { 2409, 0, 3312 }, { -401, 4065, 292 }, { -3287, -496, 2391 } }, { -6000, 793, -6964 } }, 289 }, { { { { 2409, 0, 3312 }, { -401, 4065, 292 }, { -3287, -496, 2391 } }, { -6001, 787, -6943 } }, 289 } },
    { { { { { 2629, 0, 3140 }, { -354, 4069, 296 }, { -3120, -462, 2612 } }, { -6002, 782, -6922 } }, 289 }, { { { { 2629, 0, 3140 }, { -354, 4069, 296 }, { -3120, -462, 2612 } }, { -6002, 778, -6903 } }, 289 } },
    { { { { { 2810, 0, 2980 }, { -313, 4073, 295 }, { -2963, -430, 2794 } }, { -6003, 773, -6885 } }, 289 }, { { { { 2810, 0, 2980 }, { -313, 4073, 295 }, { -2963, -430, 2794 } }, { -6003, 769, -6869 } }, 289 } },
    { { { { { 2955, 0, 2836 }, { -279, 4076, 290 }, { -2822, -403, 2941 } }, { -6004, 765, -6853 } }, 289 }, { { { { 2955, 0, 2836 }, { -279, 4076, 290 }, { -2822, -403, 2941 } }, { -6004, 762, -6839 } }, 289 } },
    { { { { { 3068, 0, 2713 }, { -251, 4078, 284 }, { -2701, -379, 3055 } }, { -6005, 758, -6826 } }, 289 }, { { { { 3068, 0, 2713 }, { -251, 4078, 284 }, { -2701, -379, 3055 } }, { -6005, 756, -6814 } }, 289 } },
    { { { { { 3152, 0, 2615 }, { -230, 4080, 277 }, { -2604, -361, 3140 } }, { -6006, 753, -6804 } }, 289 }, { { { { 3152, 0, 2615 }, { -230, 4080, 277 }, { -2604, -361, 3140 } }, { -6006, 751, -6794 } }, 289 } },
    { { { { { 3210, 0, 2543 }, { -215, 4081, 272 }, { -2534, -347, 3199 } }, { -6006, 749, -6787 } }, 289 }, { { { { 3210, 0, 2543 }, { -215, 4081, 272 }, { -2534, -347, 3199 } }, { -6007, 747, -6780 } }, 289 } },
    { { { { { 3245, 0, 2499 }, { -207, 4081, 268 }, { -2490, -339, 3233 } }, { -6007, 746, -6775 } }, 289 }, { { { { 3245, 0, 2499 }, { -207, 4081, 268 }, { -2490, -339, 3233 } }, { -6007, 745, -6772 } }, 289 } },
    { { { { { 3256, 0, 2484 }, { -204, 4082, 267 }, { -2476, -336, 3245 } }, { -6007, 745, -6769 } }, 289 }, { { { { 3256, 0, 2484 }, { -204, 4082, 267 }, { -2476, -336, 3245 } }, { -6007, 744, -6769 } }, 289 } },
};

s32 D_acropolis_plaza_8018CAF8 = 300;

ViewCamera D_acropolis_plaza_8018CAFC[150][2] = {
    { { { { { 3808, 0, 1507 }, { -25, 4095, 63 }, { -1506, -68, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -25, 4095, 63 }, { -1506, -68, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -25, 4095, 63 }, { -1506, -68, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -24, 4095, 62 }, { -1506, -67, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -24, 4095, 62 }, { -1506, -66, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -25, 4095, 63 }, { -1506, -68, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -26, 4095, 66 }, { -1506, -71, 3808 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -27, 4095, 69 }, { -1506, -75, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -29, 4095, 73 }, { -1506, -79, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -30, 4095, 78 }, { -1506, -84, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -33, 4095, 83 }, { -1506, -89, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -35, 4094, 89 }, { -1506, -95, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -37, 4094, 95 }, { -1506, -102, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -40, 4094, 102 }, { -1506, -110, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -43, 4094, 109 }, { -1506, -118, 3807 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -46, 4094, 117 }, { -1506, -126, 3806 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -49, 4093, 125 }, { -1506, -135, 3806 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -53, 4093, 134 }, { -1506, -144, 3806 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -56, 4093, 143 }, { -1506, -154, 3805 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -60, 4092, 153 }, { -1505, -165, 3805 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -64, 4092, 163 }, { -1505, -175, 3805 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -68, 4091, 173 }, { -1505, -186, 3804 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -72, 4091, 184 }, { -1505, -198, 3804 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -77, 4090, 195 }, { -1505, -209, 3803 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -81, 4089, 206 }, { -1504, -221, 3803 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -86, 4089, 217 }, { -1504, -234, 3802 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -90, 4088, 229 }, { -1504, -246, 3801 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -95, 4087, 240 }, { -1504, -259, 3800 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -100, 4086, 252 }, { -1503, -272, 3800 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -104, 4086, 265 }, { -1503, -285, 3799 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -109, 4085, 277 }, { -1503, -298, 3798 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -114, 4084, 289 }, { -1502, -311, 3797 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -119, 4083, 302 }, { -1502, -325, 3796 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -124, 4081, 315 }, { -1502, -339, 3795 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -130, 4080, 329 }, { -1501, -354, 3794 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -135, 4079, 342 }, { -1501, -368, 3793 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -141, 4077, 356 }, { -1500, -383, 3791 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -146, 4076, 371 }, { -1500, -399, 3790 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -152, 4074, 385 }, { -1499, -414, 3789 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -158, 4073, 400 }, { -1498, -430, 3787 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -164, 4071, 415 }, { -1498, -446, 3785 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -170, 4069, 430 }, { -1497, -462, 3784 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -176, 4067, 445 }, { -1496, -479, 3782 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -182, 4065, 461 }, { -1496, -496, 3780 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -189, 4063, 477 }, { -1495, -513, 3778 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -195, 4061, 493 }, { -1494, -531, 3776 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -201, 4059, 510 }, { -1493, -548, 3774 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -208, 4056, 526 }, { -1492, -566, 3771 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -215, 4054, 543 }, { -1491, -584, 3769 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -221, 4051, 560 }, { -1490, -603, 3767 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -228, 4048, 578 }, { -1489, -621, 3764 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -235, 4045, 595 }, { -1488, -640, 3761 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -242, 4042, 613 }, { -1487, -659, 3758 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -249, 4039, 631 }, { -1486, -679, 3755 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -257, 4035, 649 }, { -1485, -698, 3752 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -264, 4032, 668 }, { -1483, -718, 3749 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -271, 4028, 686 }, { -1482, -738, 3746 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -279, 4025, 705 }, { -1481, -758, 3742 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -286, 4021, 724 }, { -1479, -778, 3739 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -294, 4017, 743 }, { -1478, -799, 3735 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -301, 4013, 762 }, { -1476, -820, 3731 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -309, 4008, 782 }, { -1475, -841, 3727 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -317, 4004, 802 }, { -1473, -862, 3723 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -325, 3999, 822 }, { -1471, -884, 3718 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -333, 3994, 842 }, { -1469, -905, 3714 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -341, 3989, 862 }, { -1468, -927, 3709 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -349, 3984, 883 }, { -1466, -950, 3704 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -357, 3978, 904 }, { -1464, -972, 3699 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -366, 3973, 925 }, { -1462, -995, 3694 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -374, 3967, 946 }, { -1459, -1017, 3689 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -382, 3961, 967 }, { -1457, -1040, 3683 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -391, 3955, 989 }, { -1455, -1063, 3677 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -399, 3949, 1010 }, { -1453, -1086, 3672 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -408, 3942, 1032 }, { -1450, -1110, 3666 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -417, 3936, 1053 }, { -1448, -1133, 3659 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -425, 3929, 1075 }, { -1445, -1156, 3653 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -434, 3922, 1097 }, { -1443, -1180, 3647 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -443, 3914, 1119 }, { -1440, -1204, 3640 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -451, 3907, 1141 }, { -1437, -1227, 3633 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -460, 3900, 1163 }, { -1435, -1251, 3626 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -469, 3892, 1185 }, { -1432, -1275, 3619 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -478, 3884, 1208 }, { -1429, -1299, 3611 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -486, 3876, 1230 }, { -1426, -1323, 3604 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -495, 3868, 1252 }, { -1423, -1346, 3596 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -504, 3859, 1274 }, { -1420, -1370, 3589 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -513, 3851, 1296 }, { -1417, -1394, 3581 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -521, 3842, 1318 }, { -1413, -1418, 3572 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -530, 3833, 1340 }, { -1410, -1442, 3564 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -539, 3824, 1362 }, { -1407, -1465, 3556 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -548, 3815, 1384 }, { -1404, -1489, 3547 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -556, 3806, 1406 }, { -1400, -1513, 3539 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -565, 3796, 1428 }, { -1397, -1536, 3530 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -574, 3787, 1450 }, { -1393, -1560, 3521 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -582, 3777, 1472 }, { -1389, -1583, 3512 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -591, 3767, 1494 }, { -1386, -1607, 3503 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -599, 3757, 1516 }, { -1382, -1630, 3493 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -608, 3747, 1537 }, { -1378, -1653, 3484 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -617, 3736, 1559 }, { -1375, -1677, 3474 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -625, 3726, 1581 }, { -1371, -1700, 3464 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -634, 3715, 1602 }, { -1367, -1723, 3454 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -642, 3704, 1624 }, { -1363, -1746, 3444 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -651, 3693, 1645 }, { -1359, -1770, 3434 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -659, 3682, 1667 }, { -1355, -1793, 3424 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -668, 3671, 1688 }, { -1350, -1816, 3413 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -676, 3659, 1710 }, { -1346, -1839, 3403 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -685, 3648, 1731 }, { -1342, -1862, 3392 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -693, 3636, 1752 }, { -1338, -1885, 3381 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -702, 3624, 1774 }, { -1333, -1907, 3370 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -710, 3612, 1795 }, { -1329, -1930, 3359 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -718, 3600, 1816 }, { -1324, -1953, 3347 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -727, 3587, 1837 }, { -1320, -1975, 3336 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -735, 3575, 1858 }, { -1315, -1998, 3324 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -743, 3562, 1879 }, { -1310, -2020, 3312 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -751, 3549, 1900 }, { -1306, -2043, 3300 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -760, 3536, 1920 }, { -1301, -2065, 3288 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -768, 3523, 1941 }, { -1296, -2087, 3276 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -776, 3510, 1962 }, { -1291, -2110, 3264 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -784, 3497, 1982 }, { -1286, -2132, 3251 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -792, 3483, 2003 }, { -1281, -2154, 3239 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -800, 3470, 2023 }, { -1276, -2176, 3226 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -808, 3456, 2043 }, { -1271, -2198, 3213 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -816, 3442, 2064 }, { -1266, -2219, 3200 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -824, 3428, 2084 }, { -1261, -2241, 3187 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -832, 3413, 2104 }, { -1256, -2263, 3174 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -840, 3399, 2124 }, { -1250, -2285, 3160 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -848, 3384, 2144 }, { -1245, -2306, 3147 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -856, 3370, 2164 }, { -1240, -2327, 3133 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -864, 3355, 2184 }, { -1234, -2349, 3119 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -872, 3340, 2204 }, { -1229, -2370, 3106 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -879, 3325, 2223 }, { -1223, -2391, 3092 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -887, 3310, 2243 }, { -1218, -2412, 3078 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -895, 3295, 2262 }, { -1212, -2432, 3063 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -902, 3279, 2281 }, { -1206, -2453, 3049 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -910, 3264, 2300 }, { -1201, -2473, 3035 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -917, 3249, 2319 }, { -1195, -2493, 3021 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -925, 3233, 2337 }, { -1189, -2513, 3006 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -932, 3218, 2355 }, { -1184, -2533, 2992 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -939, 3202, 2374 }, { -1178, -2553, 2978 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -946, 3187, 2392 }, { -1172, -2572, 2963 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -953, 3171, 2409 }, { -1167, -2591, 2949 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -960, 3156, 2427 }, { -1161, -2610, 2934 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -967, 3140, 2444 }, { -1155, -2629, 2920 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -974, 3125, 2461 }, { -1150, -2647, 2906 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -980, 3109, 2478 }, { -1144, -2665, 2891 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -987, 3094, 2495 }, { -1138, -2683, 2877 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -993, 3078, 2511 }, { -1132, -2701, 2862 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1000, 3063, 2527 }, { -1127, -2718, 2848 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1006, 3048, 2543 }, { -1121, -2735, 2834 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1012, 3033, 2559 }, { -1116, -2752, 2820 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1019, 3017, 2575 }, { -1110, -2769, 2806 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1025, 3002, 2590 }, { -1104, -2786, 2791 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1031, 2985, 2607 }, { -1098, -2804, 2776 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1038, 2968, 2624 }, { -1092, -2822, 2759 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1045, 2950, 2642 }, { -1085, -2841, 2743 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1052, 2931, 2660 }, { -1078, -2860, 2725 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1059, 2912, 2678 }, { -1071, -2880, 2707 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1067, 2892, 2696 }, { -1064, -2899, 2689 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1074, 2873, 2714 }, { -1057, -2919, 2671 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1081, 2853, 2732 }, { -1049, -2938, 2653 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1088, 2833, 2750 }, { -1042, -2957, 2634 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1095, 2813, 2767 }, { -1035, -2976, 2616 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1102, 2794, 2784 }, { -1028, -2994, 2598 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1108, 2775, 2801 }, { -1021, -3012, 2580 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1114, 2756, 2817 }, { -1014, -3029, 2562 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1120, 2738, 2832 }, { -1007, -3046, 2545 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1126, 2720, 2847 }, { -1001, -3062, 2529 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1132, 2703, 2861 }, { -994, -3077, 2513 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1137, 2687, 2874 }, { -988, -3091, 2498 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1142, 2671, 2886 }, { -983, -3104, 2484 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1146, 2657, 2898 }, { -977, -3116, 2471 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1151, 2644, 2908 }, { -972, -3128, 2458 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1154, 2631, 2918 }, { -968, -3138, 2447 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1158, 2620, 2927 }, { -964, -3147, 2436 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1161, 2610, 2934 }, { -960, -3156, 2427 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1163, 2602, 2941 }, { -957, -3163, 2419 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1166, 2594, 2946 }, { -954, -3169, 2412 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1167, 2589, 2951 }, { -952, -3173, 2407 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1169, 2584, 2954 }, { -951, -3177, 2403 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1169, 2582, 2956 }, { -950, -3179, 2401 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
    { { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 }, { { { { 3808, 0, 1507 }, { -1170, 2581, 2956 }, { -949, -3180, 2400 } }, { -0x4F38, 1740, -1350 } }, 289 } },
};

s32 D_acropolis_plaza_8018F52C = 32;

ViewCamera D_acropolis_plaza_8018F530[16][2] = {
    { { { { { 4006, 0, 852 }, { -72, 4080, 342 }, { -849, -350, 3991 } }, { -2198, 666, -0x2710 } }, 289 }, { { { { 4006, 0, 852 }, { -72, 4080, 342 }, { -849, -350, 3991 } }, { -2198, 666, -0x2710 } }, 289 } },
    { { { { { 4006, 0, 852 }, { -72, 4080, 342 }, { -849, -350, 3991 } }, { -2198, 666, -0x2710 } }, 289 }, { { { { 4006, 0, 852 }, { -72, 4080, 342 }, { -849, -350, 3991 } }, { -2198, 666, -0x2710 } }, 289 } },
    { { { { { 4008, 0, 842 }, { -72, 4081, 342 }, { -839, -350, 3993 } }, { -2185, 670, -0x271B } }, 289 }, { { { { 4014, 0, 813 }, { -69, 4081, 341 }, { -810, -348, 3999 } }, { -2148, 680, -0x273D } }, 289 } },
    { { { { { 4023, 0, 767 }, { -64, 4081, 339 }, { -765, -345, 4009 } }, { -2089, 696, -0x2771 } }, 289 }, { { { { 4034, 0, 705 }, { -58, 4081, 335 }, { -702, -340, 4020 } }, { -2012, 718, -0x27B6 } }, 289 } },
    { { { { { 4047, 0, 627 }, { -51, 4082, 330 }, { -624, -334, 4034 } }, { -1920, 744, -0x2808 } }, 289 }, { { { { 4061, 0, 533 }, { -42, 4083, 323 }, { -531, -326, 4048 } }, { -1816, 775, -0x2865 } }, 289 } },
    { { { { { 4073, 0, 425 }, { -32, 4083, 313 }, { -424, -315, 4061 } }, { -1702, 809, -0x28CA } }, 289 }, { { { { 4084, 0, 304 }, { -22, 4084, 302 }, { -303, -303, 4073 } }, { -1583, 845, -0x2935 } }, 289 } },
    { { { { { 4092, 0, 171 }, { -12, 4085, 288 }, { -170, -289, 4082 } }, { -1459, 882, -0x29A3 } }, 289 }, { { { { 4095, 0, 28 }, { -1, 4086, 272 }, { -28, -272, 4086 } }, { -1336, 920, -0x2A12 } }, 289 } },
    { { { { { 4094, 0, -119 }, { 7, 4088, 255 }, { 119, -255, 4086 } }, { -1215, 957, -0x2A7E } }, 289 }, { { { { 4087, 0, -270 }, { 15, 4089, 235 }, { 269, -236, 4080 } }, { -1100, 994, -0x2AE5 } }, 289 } },
    { { { { { 4072, 0, -439 }, { 22, 4090, 212 }, { 439, -213, 4066 } }, { -977, 1033, -0x2B52 } }, 289 }, { { { { 4044, 0, -646 }, { 28, 4091, 181 }, { 646, -183, 4040 } }, { -836, 1079, -0x2BCF } }, 289 } },
    { { { { { 3999, 0, -884 }, { 31, 4093, 143 }, { 884, -147, 3996 } }, { -682, 1129, -0x2C55 } }, 289 }, { { { { 3932, 0, -1144 }, { 29, 4094, 101 }, { 1144, -105, 3931 } }, { -523, 1182, -0x2CDF } }, 289 } },
    { { { { { 3844, 0, -1413 }, { 20, 4095, 55 }, { 1413, -59, 3843 } }, { -365, 1236, -0x2D6A } }, 289 }, { { { { 3736, 0, -1677 }, { 4, 4095, 10 }, { 1677, -11, 3736 } }, { -216, 1287, -0x2DF0 } }, 289 } },
    { { { { { 3618, 0, -1919 }, { -16, 4095, -31 }, { 1919, 35, 3618 } }, { -82, 1335, -0x2E6B } }, 289 }, { { { { 3500, 0, -2127 }, { -41, 4095, -67 }, { 2126, 79, 3499 } }, { 30, 1377, -0x2ED8 } }, 289 } },
    { { { { { 3382, 0, -2309 }, { -68, 4094, -99 }, { 2308, 120, 3381 } }, { 126, 1416, -0x2F3D } }, 289 }, { { { { 3258, 0, -2482 }, { -99, 4092, -130 }, { 2480, 163, 3255 } }, { 216, 1454, -0x2FA0 } }, 289 } },
    { { { { { 3132, 0, -2638 }, { -132, 4090, -157 }, { 2635, 206, 3128 } }, { 297, 1490, -0x2FFE } }, 289 }, { { { { 3012, 0, -2774 }, { -166, 4088, -181 }, { 2769, 246, 3007 } }, { 366, 1523, -0x3055 } }, 289 } },
    { { { { { 2906, 0, -2885 }, { -199, 4086, -200 }, { 2878, 282, 2899 } }, { 420, 1552, -0x30A1 } }, 289 }, { { { { 2823, 0, -2967 }, { -227, 4083, -216 }, { 2959, 313, 2814 } }, { 458, 1576, -0x30DE } }, 289 } },
    { { { { { 2771, 0, -3015 }, { -248, 4082, -228 }, { 3005, 337, 2762 } }, { 473, 1592, -0x3108 } }, 289 }, { { { { 2763, 0, -3023 }, { -256, 4081, -234 }, { 3012, 346, 2753 } }, { 468, 1598, -0x3116 } }, 289 } },
};

// Retained data: Retained word 100 between the camera table and script data; no reference found.
s32 D_acropolis_plaza_8018F9B0 = 100;

ViewCamera D_acropolis_plaza_8018F9B4[50][2] = {
    { { { { { 2763, 0, -3023 }, { -256, 4081, -234 }, { 3012, 346, 2753 } }, { 468, 1598, -0x3116 } }, 289 }, { { { { 2763, 0, -3023 }, { -256, 4081, -234 }, { 3012, 346, 2753 } }, { 468, 1598, -0x3116 } }, 289 } },
    { { { { { 2769, 0, -3017 }, { -255, 4081, -234 }, { 3006, 347, 2759 } }, { 466, 1598, -0x3115 } }, 289 }, { { { { 2787, 0, -3000 }, { -255, 4081, -237 }, { 2989, 348, 2777 } }, { 458, 1598, -0x3112 } }, 289 } },
    { { { { { 2816, 0, -2973 }, { -253, 4081, -240 }, { 2962, 349, 2806 } }, { 445, 1597, -0x310E } }, 289 }, { { { { 2855, 0, -2936 }, { -252, 4080, -245 }, { 2925, 352, 2844 } }, { 427, 1597, -0x3108 } }, 289 } },
    { { { { { 2902, 0, -2889 }, { -250, 4080, -251 }, { 2878, 354, 2892 } }, { 405, 1597, -0x3101 } }, 289 }, { { { { 2957, 0, -2833 }, { -247, 4080, -258 }, { 2822, 358, 2946 } }, { 378, 1596, -0x30F9 } }, 289 } },
    { { { { { 3018, 0, -2768 }, { -244, 4079, -266 }, { 2757, 362, 3007 } }, { 347, 1596, -0x30F0 } }, 289 }, { { { { 3084, 0, -2694 }, { -241, 4079, -276 }, { 2683, 366, 3072 } }, { 313, 1595, -0x30E7 } }, 289 } },
    { { { { { 3154, 0, -2612 }, { -236, 4079, -286 }, { 2601, 371, 3141 } }, { 275, 1594, -0x30DE } }, 289 }, { { { { 3226, 0, -2523 }, { -232, 4078, -296 }, { 2512, 376, 3212 } }, { 233, 1593, -0x30D6 } }, 289 } },
    { { { { { 3299, 0, -2426 }, { -226, 4078, -307 }, { 2416, 382, 3285 } }, { 189, 1592, -0x30CD } }, 289 }, { { { { 3373, 0, -2323 }, { -220, 4077, -319 }, { 2313, 387, 3357 } }, { 142, 1591, -0x30C5 } }, 289 } },
    { { { { { 3445, 0, -2215 }, { -212, 4077, -331 }, { 2204, 393, 3429 } }, { 93, 1590, -0x30BD } }, 289 }, { { { { 3515, 0, -2101 }, { -205, 4076, -343 }, { 2091, 399, 3498 } }, { 42, 1589, -0x30B6 } }, 289 } },
    { { { { { 3583, 0, -1984 }, { -196, 4075, -354 }, { 1974, 405, 3565 } }, { -10, 1588, -0x30B0 } }, 289 }, { { { { 3647, 0, -1864 }, { -187, 4075, -366 }, { 1854, 411, 3628 } }, { -65, 1587, -0x30AB } }, 289 } },
    { { { { { 3706, 0, -1742 }, { -177, 4074, -377 }, { 1733, 417, 3687 } }, { -120, 1586, -0x30A6 } }, 289 }, { { { { 3762, 0, -1619 }, { -167, 4074, -388 }, { 1610, 422, 3742 } }, { -177, 1585, -0x30A3 } }, 289 } },
    { { { { { 3812, 0, -1496 }, { -156, 4073, -398 }, { 1488, 428, 3791 } }, { -234, 1584, -0x30A0 } }, 289 }, { { { { 3858, 0, -1375 }, { -145, 4072, -408 }, { 1367, 433, 3836 } }, { -291, 1582, -0x309F } }, 289 } },
    { { { { { 3898, 0, -1255 }, { -134, 4072, -417 }, { 1248, 438, 3876 } }, { -348, 1581, -0x309F } }, 289 }, { { { { 3934, 0, -1138 }, { -123, 4071, -425 }, { 1131, 443, 3911 } }, { -404, 1580, -0x309F } }, 289 } },
    { { { { { 3965, 0, -1025 }, { -112, 4071, -433 }, { 1019, 448, 3941 } }, { -459, 1579, -0x30A1 } }, 289 }, { { { { 3992, 0, -916 }, { -101, 4070, -441 }, { 911, 452, 3967 } }, { -514, 1577, -0x30A4 } }, 289 } },
    { { { { { 4014, 0, -812 }, { -90, 4070, -447 }, { 807, 457, 3989 } }, { -567, 1576, -0x30A8 } }, 289 }, { { { { 4033, 0, -714 }, { -80, 4069, -454 }, { 709, 461, 4007 } }, { -618, 1575, -0x30AE } }, 289 } },
    { { { { { 4048, 0, -622 }, { -70, 4069, -460 }, { 618, 465, 4022 } }, { -666, 1574, -0x30B4 } }, 289 }, { { { { 4060, 0, -536 }, { -61, 4068, -465 }, { 532, 469, 4033 } }, { -713, 1573, -0x30BC } }, 289 } },
    { { { { { 4070, 0, -456 }, { -52, 4068, -470 }, { 453, 473, 4043 } }, { -756, 1572, -0x30C5 } }, 289 }, { { { { 4077, 0, -384 }, { -44, 4068, -475 }, { 381, 477, 4050 } }, { -797, 1571, -0x30CE } }, 289 } },
    { { { { { 4083, 0, -319 }, { -37, 4067, -480 }, { 317, 481, 4055 } }, { -833, 1570, -0x30D9 } }, 289 }, { { { { 4087, 0, -262 }, { -31, 4067, -484 }, { 260, 485, 4058 } }, { -866, 1569, -0x30E4 } }, 289 } },
    { { { { { 4090, 0, -211 }, { -25, 4066, -489 }, { 209, 489, 4061 } }, { -896, 1568, -0x30F1 } }, 289 }, { { { { 4092, 0, -163 }, { -19, 4066, -494 }, { 162, 494, 4062 } }, { -924, 1567, -0x30FF } }, 289 } },
    { { { { { 4094, 0, -118 }, { -14, 4065, -499 }, { 117, 500, 4063 } }, { -951, 1567, -0x310E } }, 289 }, { { { { 4095, 0, -77 }, { -9, 4064, -506 }, { 76, 506, 4063 } }, { -975, 1566, -0x311F } }, 289 } },
    { { { { { 4095, 0, -38 }, { -4, 4063, -512 }, { 38, 512, 4063 } }, { -998, 1565, -0x3130 } }, 289 }, { { { { 4095, 0, -2 }, { 0, 4062, -519 }, { 2, 519, 4062 } }, { -1020, 1565, -0x3143 } }, 289 } },
    { { { { { 4095, 0, 30 }, { 3, 4061, -526 }, { -30, 526, 4061 } }, { -1040, 1564, -0x3156 } }, 289 }, { { { { 4095, 0, 62 }, { 8, 4061, -534 }, { -61, 534, 4060 } }, { -1058, 1564, -0x3169 } }, 289 } },
    { { { { { 4094, 0, 91 }, { 12, 4059, -541 }, { -90, 541, 4058 } }, { -1076, 1563, -0x317D } }, 289 }, { { { { 4094, 0, 118 }, { 15, 4058, -549 }, { -117, 549, 4057 } }, { -1092, 1563, -0x3191 } }, 289 } },
    { { { { { 4093, 0, 144 }, { 19, 4057, -557 }, { -142, 557, 4055 } }, { -1107, 1563, -0x31A5 } }, 289 }, { { { { 4092, 0, 168 }, { 23, 4056, -565 }, { -166, 565, 4053 } }, { -1121, 1562, -0x31BA } }, 289 } },
    { { { { { 4091, 0, 191 }, { 26, 4055, -573 }, { -189, 574, 4051 } }, { -1134, 1562, -0x31CE } }, 289 }, { { { { 4090, 0, 212 }, { 30, 4054, -581 }, { -210, 582, 4048 } }, { -1146, 1561, -0x31E3 } }, 289 } },
    { { { { { 4089, 0, 233 }, { 33, 4053, -589 }, { -230, 590, 4046 } }, { -1157, 1561, -0x31F7 } }, 289 }, { { { { 4088, 0, 252 }, { 36, 4051, -597 }, { -250, 599, 4044 } }, { -1168, 1561, -0x320B } }, 289 } },
    { { { { { 4086, 0, 271 }, { 40, 4050, -606 }, { -268, 607, 4041 } }, { -1178, 1560, -0x3220 } }, 289 }, { { { { 4085, 0, 289 }, { 43, 4049, -614 }, { -286, 616, 4039 } }, { -1187, 1560, -0x3234 } }, 289 } },
    { { { { { 4084, 0, 306 }, { 46, 4048, -622 }, { -303, 624, 4036 } }, { -1196, 1560, -0x3248 } }, 289 }, { { { { 4083, 0, 323 }, { 49, 4046, -631 }, { -319, 633, 4034 } }, { -1205, 1559, -0x325C } }, 289 } },
    { { { { { 4081, 0, 338 }, { 53, 4045, -640 }, { -334, 642, 4031 } }, { -1213, 1559, -0x3271 } }, 289 }, { { { { 4080, 0, 354 }, { 56, 4043, -648 }, { -349, 651, 4028 } }, { -1221, 1559, -0x3285 } }, 289 } },
    { { { { { 4079, 0, 368 }, { 59, 4042, -657 }, { -363, 660, 4025 } }, { -1228, 1558, -0x3299 } }, 289 }, { { { { 4078, 0, 383 }, { 62, 4040, -666 }, { -377, 669, 4023 } }, { -1235, 1558, -0x32AD } }, 289 } },
    { { { { { 4076, 0, 396 }, { 65, 4039, -675 }, { -391, 678, 4020 } }, { -1242, 1557, -0x32C2 } }, 289 }, { { { { 4075, 0, 410 }, { 68, 4037, -683 }, { -404, 687, 4017 } }, { -1249, 1557, -0x32D6 } }, 289 } },
    { { { { { 4074, 0, 423 }, { 71, 4036, -692 }, { -416, 695, 4014 } }, { -1255, 1557, -0x32EB } }, 289 }, { { { { 4072, 0, 435 }, { 74, 4035, -700 }, { -429, 704, 4012 } }, { -1262, 1556, -0x3300 } }, 289 } },
    { { { { { 4071, 0, 447 }, { 77, 4033, -707 }, { -440, 712, 4009 } }, { -1268, 1556, -0x3315 } }, 289 }, { { { { 4070, 0, 459 }, { 80, 4032, -714 }, { -452, 719, 4006 } }, { -1274, 1555, -0x332A } }, 289 } },
    { { { { { 4068, 0, 471 }, { 83, 4031, -721 }, { -463, 726, 4004 } }, { -1280, 1555, -0x333F } }, 289 }, { { { { 4067, 0, 482 }, { 86, 4029, -727 }, { -475, 732, 4001 } }, { -1285, 1555, -0x3355 } }, 289 } },
    { { { { { 4066, 0, 494 }, { 89, 4028, -732 }, { -486, 737, 3999 } }, { -1291, 1554, -0x336B } }, 289 }, { { { { 4064, 0, 505 }, { 91, 4028, -736 }, { -496, 742, 3997 } }, { -1297, 1553, -0x3381 } }, 289 } },
    { { { { { 4063, 0, 516 }, { 94, 4027, -740 }, { -507, 746, 3995 } }, { -1303, 1553, -0x3398 } }, 289 }, { { { { 4061, 0, 527 }, { 96, 4026, -743 }, { -518, 749, 3993 } }, { -1309, 1552, -0x33AF } }, 289 } },
    { { { { { 4060, 0, 537 }, { 98, 4026, -745 }, { -528, 751, 3991 } }, { -1315, 1551, -0x33C7 } }, 289 }, { { { { 4059, 0, 546 }, { 100, 4026, -746 }, { -536, 752, 3990 } }, { -1320, 1551, -0x33E0 } }, 289 } },
    { { { { { 4058, 0, 553 }, { 101, 4026, -746 }, { -543, 753, 3989 } }, { -1325, 1550, -0x33FB } }, 289 }, { { { { 4057, 0, 558 }, { 102, 4026, -746 }, { -549, 753, 3988 } }, { -1329, 1549, -0x3418 } }, 289 } },
    { { { { { 4057, 0, 561 }, { 103, 4026, -745 }, { -552, 752, 3988 } }, { -1333, 1548, -0x3435 } }, 289 }, { { { { 4057, 0, 563 }, { 103, 4026, -743 }, { -554, 750, 3988 } }, { -1336, 1547, -0x3453 } }, 289 } },
    { { { { { 4057, 0, 563 }, { 102, 4027, -740 }, { -554, 748, 3988 } }, { -1338, 1546, -0x3471 } }, 289 }, { { { { 4057, 0, 561 }, { 102, 4027, -737 }, { -552, 744, 3989 } }, { -1339, 1545, -0x3491 } }, 289 } },
    { { { { { 4057, 0, 558 }, { 100, 4028, -734 }, { -549, 741, 3990 } }, { -1339, 1544, -0x34B0 } }, 289 }, { { { { 4058, 0, 553 }, { 99, 4029, -729 }, { -544, 736, 3992 } }, { -1339, 1543, -0x34D0 } }, 289 } },
    { { { { { 4059, 0, 546 }, { 97, 4030, -725 }, { -538, 731, 3993 } }, { -1338, 1541, -0x34EF } }, 289 }, { { { { 4060, 0, 539 }, { 95, 4031, -720 }, { -530, 726, 3995 } }, { -1336, 1540, -0x350E } }, 289 } },
    { { { { { 4061, 0, 530 }, { 93, 4032, -715 }, { -522, 721, 3998 } }, { -1333, 1539, -0x352D } }, 289 }, { { { { 4062, 0, 520 }, { 90, 4033, -709 }, { -512, 715, 4000 } }, { -1329, 1538, -0x354C } }, 289 } },
    { { { { { 4064, 0, 509 }, { 88, 4034, -703 }, { -501, 709, 4002 } }, { -1325, 1537, -0x3569 } }, 289 }, { { { { 4065, 0, 497 }, { 85, 4035, -697 }, { -490, 702, 4005 } }, { -1320, 1536, -0x3586 } }, 289 } },
    { { { { { 4067, 0, 484 }, { 82, 4036, -691 }, { -477, 696, 4007 } }, { -1315, 1535, -0x35A2 } }, 289 }, { { { { 4068, 0, 471 }, { 79, 4037, -685 }, { -464, 690, 4010 } }, { -1309, 1534, -0x35BC } }, 289 } },
    { { { { { 4070, 0, 457 }, { 76, 4038, -679 }, { -451, 683, 4013 } }, { -1302, 1533, -0x35D5 } }, 289 }, { { { { 4071, 0, 443 }, { 73, 4039, -673 }, { -437, 677, 4015 } }, { -1295, 1532, -0x35EC } }, 289 } },
    { { { { { 4073, 0, 429 }, { 70, 4040, -667 }, { -423, 671, 4018 } }, { -1287, 1531, -0x3602 } }, 289 }, { { { { 4074, 0, 415 }, { 67, 4041, -662 }, { -409, 665, 4020 } }, { -1280, 1530, -0x3615 } }, 289 } },
    { { { { { 4076, 0, 401 }, { 64, 4042, -656 }, { -395, 659, 4023 } }, { -1272, 1529, -0x3627 } }, 289 }, { { { { 4077, 0, 387 }, { 61, 4043, -651 }, { -382, 654, 4025 } }, { -1264, 1529, -0x3636 } }, 289 } },
    { { { { { 4078, 0, 374 }, { 59, 4044, -647 }, { -370, 649, 4027 } }, { -1256, 1528, -0x3643 } }, 289 }, { { { { 4079, 0, 363 }, { 57, 4044, -642 }, { -358, 645, 4028 } }, { -1249, 1527, -0x364D } }, 289 } },
    { { { { { 4080, 0, 353 }, { 55, 4045, -639 }, { -349, 641, 4030 } }, { -1244, 1527, -0x3655 } }, 289 }, { { { { 4081, 0, 346 }, { 54, 4045, -636 }, { -341, 639, 4031 } }, { -1239, 1527, -0x365B } }, 289 } },
    { { { { { 4081, 0, 341 }, { 53, 4046, -635 }, { -337, 637, 4032 } }, { -1236, 1527, -0x365E } }, 289 }, { { { { 4081, 0, 339 }, { 52, 4046, -634 }, { -335, 636, 4032 } }, { -1235, 1526, -0x3660 } }, 289 } },
};

VECTOR3 D_acropolis_plaza_801907C4[400] = {
    { 0x5A9D, -1000, 1195 },
    { 0x5A8E, -1000, 1194 },
    { 0x5A75, -1000, 1192 },
    { 0x5A54, -1000, 1190 },
    { 0x5A2A, -1000, 1188 },
    { 0x59F9, -1000, 1186 },
    { 0x59C1, -1000, 1185 },
    { 0x5983, -1000, 1184 },
    { 0x593F, -1000, 1184 },
    { 0x58F7, -1000, 1185 },
    { 0x58AA, -1000, 1186 },
    { 0x5859, -1000, 1187 },
    { 0x5806, -1000, 1189 },
    { 0x57B1, -1000, 1192 },
    { 0x5759, -1000, 1195 },
    { 0x5701, -1000, 1199 },
    { 0x56A9, -1000, 1203 },
    { 0x5650, -1000, 1207 },
    { 0x55F9, -1000, 1212 },
    { 0x55A4, -1000, 1218 },
    { 0x5550, -1000, 1224 },
    { 0x5500, -1000, 1230 },
    { 0x54B3, -1000, 1236 },
    { 0x546B, -1000, 1242 },
    { 0x5427, -1000, 1249 },
    { 0x53E6, -1000, 1256 },
    { 0x53A6, -1000, 1263 },
    { 0x5366, -1000, 1270 },
    { 0x5326, -1000, 1278 },
    { 0x52E6, -1000, 1286 },
    { 0x52A7, -1000, 1295 },
    { 0x5267, -1000, 1304 },
    { 0x5229, -1000, 1313 },
    { 0x51EA, -1000, 1323 },
    { 0x51AB, -1000, 1333 },
    { 0x516C, -1000, 1343 },
    { 0x512E, -1000, 1354 },
    { 0x50EF, -1000, 1365 },
    { 0x50B1, -1000, 1376 },
    { 0x5072, -1000, 1388 },
    { 0x5034, -1000, 1400 },
    { 0x4FF5, -1000, 1413 },
    { 0x4FB7, -1000, 1426 },
    { 0x4F78, -1000, 1439 },
    { 0x4F39, -1000, 1453 },
    { 0x4EFA, -1000, 1468 },
    { 0x4EBB, -1000, 1482 },
    { 0x4E7B, -1000, 1497 },
    { 0x4E3C, -1000, 1513 },
    { 0x4DFC, -1000, 1528 },
    { 0x4DBB, -1000, 1544 },
    { 0x4D7B, -1000, 1560 },
    { 0x4D3A, -1000, 1576 },
    { 0x4CF9, -1000, 1592 },
    { 0x4CB8, -1000, 1609 },
    { 0x4C76, -1000, 1625 },
    { 0x4C35, -1000, 1641 },
    { 0x4BF3, -1000, 1658 },
    { 0x4BB1, -1000, 1675 },
    { 0x4B70, -1000, 1693 },
    { 0x4B2E, -1000, 1711 },
    { 0x4AEC, -1000, 1729 },
    { 0x4AAA, -1000, 1749 },
    { 0x4A69, -1000, 1768 },
    { 0x4A27, -1000, 1789 },
    { 0x49E6, -1000, 1810 },
    { 0x49A5, -1000, 1832 },
    { 0x4964, -1000, 1855 },
    { 0x4924, -1000, 1879 },
    { 0x48E4, -1000, 1904 },
    { 0x48A5, -1000, 1930 },
    { 0x4866, -1000, 1957 },
    { 0x4827, -1000, 1985 },
    { 0x47E9, -1000, 2014 },
    { 0x47AC, -1000, 2045 },
    { 0x4770, -1000, 2077 },
    { 0x4735, -1000, 2112 },
    { 0x46FA, -1000, 2148 },
    { 0x46C0, -1000, 2186 },
    { 0x4687, -1000, 2225 },
    { 0x464F, -1000, 2266 },
    { 0x4618, -1000, 2307 },
    { 0x45E1, -1000, 2350 },
    { 0x45AB, -1000, 2394 },
    { 0x4575, -1000, 2438 },
    { 0x4540, -1000, 2482 },
    { 0x450B, -1000, 2527 },
    { 0x44D6, -1000, 2572 },
    { 0x44A1, -1000, 2617 },
    { 0x446D, -1000, 2662 },
    { 0x4439, -1000, 2707 },
    { 0x4404, -1000, 2752 },
    { 0x43D0, -1000, 2796 },
    { 0x439B, -1000, 2839 },
    { 0x4367, -1000, 2882 },
    { 0x4331, -1000, 2924 },
    { 0x42FC, -1000, 2965 },
    { 0x42C6, -1000, 3004 },
    { 0x4290, -1000, 3043 },
    { 0x4259, -1000, 3080 },
    { 0x4222, -1000, 3115 },
    { 0x41EB, -1000, 3150 },
    { 0x41B4, -1000, 3185 },
    { 0x417E, -1000, 3219 },
    { 0x4148, -1000, 3252 },
    { 0x4112, -1000, 3285 },
    { 0x40DC, -1000, 3318 },
    { 0x40A6, -1000, 3350 },
    { 0x4070, -1000, 3382 },
    { 0x403A, -1000, 3413 },
    { 0x4004, -1000, 3444 },
    { 0x3FCE, -1000, 3474 },
    { 0x3F97, -1000, 3504 },
    { 0x3F61, -1000, 3534 },
    { 0x3F2A, -1000, 3562 },
    { 0x3EF2, -1000, 3591 },
    { 0x3EBB, -1000, 3618 },
    { 0x3E83, -1000, 3646 },
    { 0x3E4A, -1000, 3672 },
    { 0x3E11, -1000, 3699 },
    { 0x3DD7, -1000, 3724 },
    { 0x3D9D, -1000, 3749 },
    { 0x3D62, -1000, 3774 },
    { 0x3D26, -1000, 3797 },
    { 0x3CEA, -1000, 3820 },
    { 0x3CAD, -1000, 3843 },
    { 0x3C6F, -1000, 3864 },
    { 0x3C30, -1000, 3885 },
    { 0x3BF2, -1000, 3905 },
    { 0x3BB2, -1000, 3923 },
    { 0x3B72, -1000, 3941 },
    { 0x3B32, -1000, 3959 },
    { 0x3AF1, -1000, 3975 },
    { 0x3AB0, -1000, 3991 },
    { 0x3A6F, -1000, 4007 },
    { 0x3A2D, -1000, 4022 },
    { 0x39EC, -1000, 4036 },
    { 0x39AA, -1000, 4050 },
    { 0x3968, -1000, 4064 },
    { 0x3926, -1000, 4077 },
    { 0x38E4, -1000, 4090 },
    { 0x38A2, -1000, 4103 },
    { 0x3860, -1000, 4116 },
    { 0x381E, -1000, 4128 },
    { 0x37DC, -1000, 4140 },
    { 0x379A, -1000, 4153 },
    { 0x3758, -1000, 4165 },
    { 0x3717, -1000, 4178 },
    { 0x36D6, -1000, 4190 },
    { 0x3695, -1000, 4203 },
    { 0x3654, -1000, 4216 },
    { 0x3614, -1000, 4228 },
    { 0x35D3, -1000, 4240 },
    { 0x3593, -1000, 4252 },
    { 0x3552, -1000, 4264 },
    { 0x3512, -1000, 4275 },
    { 0x34D2, -1000, 4286 },
    { 0x3492, -1000, 4296 },
    { 0x3452, -1000, 4307 },
    { 0x3412, -1000, 4317 },
    { 0x33D1, -1000, 4327 },
    { 0x3391, -1000, 4337 },
    { 0x3351, -1000, 4347 },
    { 0x3311, -1000, 4357 },
    { 0x32D1, -1000, 4367 },
    { 0x3290, -1000, 4377 },
    { 0x3250, -1000, 4387 },
    { 0x3210, -1000, 4397 },
    { 0x31CF, -1000, 4407 },
    { 0x318E, -1000, 4418 },
    { 0x314E, -1000, 4428 },
    { 0x310D, -1000, 4439 },
    { 0x30CC, -1000, 4450 },
    { 0x308A, -1000, 4461 },
    { 0x3049, -1000, 4472 },
    { 0x3007, -1000, 4484 },
    { 0x2FC6, -1000, 4495 },
    { 0x2F84, -1000, 4507 },
    { 0x2F42, -1000, 4518 },
    { 0x2F01, -1000, 4529 },
    { 0x2EBF, -1000, 4540 },
    { 0x2E7D, -1000, 4550 },
    { 0x2E3B, -1000, 4561 },
    { 0x2DF9, -1000, 4572 },
    { 0x2DB6, -1000, 4583 },
    { 0x2D74, -1000, 4594 },
    { 0x2D32, -1000, 4605 },
    { 0x2CEF, -1000, 4617 },
    { 0x2CAD, -1000, 4628 },
    { 0x2C6A, -1000, 4640 },
    { 0x2C27, -1000, 4653 },
    { 0x2BE4, -1000, 4665 },
    { 0x2BA1, -1000, 4678 },
    { 0x2B5E, -1000, 4692 },
    { 0x2B1B, -1000, 4706 },
    { 0x2AD8, -1000, 4721 },
    { 0x2A95, -1000, 4736 },
    { 0x2A51, -1000, 4752 },
    { 0x2A0E, -1000, 4769 },
    { 0x29CA, -1000, 4787 },
    { 0x2986, -1000, 4805 },
    { 0x2942, -1000, 4824 },
    { 0x28FD, -1000, 4843 },
    { 0x28B9, -1000, 4863 },
    { 0x2874, -1000, 4884 },
    { 0x282E, -1000, 4904 },
    { 0x27E9, -1000, 4926 },
    { 0x27A3, -1000, 4948 },
    { 0x275E, -1000, 4970 },
    { 0x2718, -1000, 4993 },
    { 9939, -1000, 5016 },
    { 9869, -1000, 5040 },
    { 9800, -1000, 5064 },
    { 9731, -1000, 5088 },
    { 9662, -1000, 5113 },
    { 9593, -1000, 5138 },
    { 9524, -1000, 5163 },
    { 9456, -1000, 5189 },
    { 9389, -1000, 5215 },
    { 9321, -1000, 5241 },
    { 9255, -1000, 5267 },
    { 9188, -1000, 5294 },
    { 9123, -1000, 5321 },
    { 9058, -1000, 5348 },
    { 8994, -1000, 5376 },
    { 8930, -1000, 5404 },
    { 8867, -1000, 5432 },
    { 8804, -1000, 5460 },
    { 8742, -1000, 5489 },
    { 8680, -1000, 5518 },
    { 8619, -1000, 5548 },
    { 8558, -1000, 5578 },
    { 8497, -1000, 5608 },
    { 8437, -1000, 5638 },
    { 8377, -1000, 5669 },
    { 8318, -1000, 5700 },
    { 8259, -1000, 5732 },
    { 8200, -1000, 5763 },
    { 8142, -1000, 5795 },
    { 8084, -1000, 5828 },
    { 8026, -1000, 5860 },
    { 7968, -1000, 5893 },
    { 7911, -1000, 5926 },
    { 7854, -1000, 5959 },
    { 7797, -1000, 5993 },
    { 7740, -1000, 6027 },
    { 7684, -1000, 6061 },
    { 7627, -1000, 6095 },
    { 7571, -1000, 6130 },
    { 7515, -1000, 6165 },
    { 7459, -1000, 6200 },
    { 7404, -1000, 6235 },
    { 7349, -1000, 6270 },
    { 7294, -1000, 6306 },
    { 7239, -1000, 6341 },
    { 7185, -1000, 6377 },
    { 7132, -1000, 6413 },
    { 7078, -1000, 6449 },
    { 7025, -1000, 6486 },
    { 6972, -1000, 6522 },
    { 6919, -1000, 6559 },
    { 6867, -1000, 6597 },
    { 6815, -1000, 6634 },
    { 6762, -1000, 6672 },
    { 6710, -1000, 6711 },
    { 6659, -1000, 6749 },
    { 6607, -1000, 6788 },
    { 6555, -1000, 6827 },
    { 6504, -1000, 6867 },
    { 6452, -1000, 6907 },
    { 6401, -1000, 6947 },
    { 6349, -1000, 6988 },
    { 6298, -1000, 7030 },
    { 6246, -1000, 7071 },
    { 6195, -1000, 7114 },
    { 6143, -1000, 7156 },
    { 6092, -1000, 7200 },
    { 6040, -1000, 7243 },
    { 5989, -1000, 7287 },
    { 5937, -1000, 7332 },
    { 5886, -1000, 7376 },
    { 5834, -1000, 7422 },
    { 5783, -1000, 7467 },
    { 5732, -1000, 7513 },
    { 5681, -1000, 7559 },
    { 5630, -1000, 7606 },
    { 5580, -1000, 7653 },
    { 5529, -1000, 7700 },
    { 5479, -1000, 7747 },
    { 5429, -1000, 7795 },
    { 5379, -1000, 7843 },
    { 5329, -1000, 7891 },
    { 5280, -1000, 7939 },
    { 5231, -1000, 7988 },
    { 5182, -1000, 8037 },
    { 5134, -1000, 8085 },
    { 5086, -1000, 8134 },
    { 5038, -1000, 8184 },
    { 4990, -1000, 8233 },
    { 4943, -1000, 8282 },
    { 4897, -1000, 8332 },
    { 4851, -1000, 8381 },
    { 4805, -1000, 8431 },
    { 4760, -1000, 8481 },
    { 4715, -1000, 8531 },
    { 4670, -1000, 8581 },
    { 4626, -1000, 8631 },
    { 4583, -1000, 8681 },
    { 4539, -1000, 8732 },
    { 4496, -1000, 8783 },
    { 4453, -1000, 8834 },
    { 4410, -1000, 8885 },
    { 4368, -1000, 8937 },
    { 4325, -1000, 8988 },
    { 4283, -1000, 9040 },
    { 4241, -1000, 9093 },
    { 4199, -1000, 9146 },
    { 4157, -1000, 9199 },
    { 4115, -1000, 9252 },
    { 4073, -1000, 9306 },
    { 4030, -1000, 9360 },
    { 3988, -1000, 9415 },
    { 3946, -1000, 9470 },
    { 3904, -1000, 9525 },
    { 3861, -1000, 9581 },
    { 3818, -1000, 9638 },
    { 3775, -1000, 9695 },
    { 3731, -1000, 9754 },
    { 3688, -1000, 9813 },
    { 3644, -1000, 9873 },
    { 3600, -1000, 9933 },
    { 3556, -1000, 9994 },
    { 3512, -1000, 0x2747 },
    { 3468, -1000, 0x2785 },
    { 3425, -1000, 0x27C3 },
    { 3381, -1000, 0x2801 },
    { 3338, -1000, 0x283F },
    { 3294, -1000, 0x287D },
    { 3251, -1000, 0x28BC },
    { 3209, -1000, 0x28FA },
    { 3167, -1000, 0x2938 },
    { 3125, -1000, 0x2975 },
    { 3083, -1000, 0x29B3 },
    { 3042, -1000, 0x29EF },
    { 3002, -1000, 0x2A2C },
    { 2962, -1000, 0x2A67 },
    { 2923, -1000, 0x2AA2 },
    { 2885, -1000, 0x2ADC },
    { 2847, -1000, 0x2B16 },
    { 2810, -1000, 0x2B4E },
    { 2774, -1000, 0x2B86 },
    { 2738, -1000, 0x2BBC },
    { 2704, -1000, 0x2BF2 },
    { 2669, -1000, 0x2C27 },
    { 2636, -1000, 0x2C5C },
    { 2602, -1000, 0x2C8F },
    { 2570, -1000, 0x2CC3 },
    { 2537, -1000, 0x2CF6 },
    { 2506, -1000, 0x2D28 },
    { 2474, -1000, 0x2D5A },
    { 2443, -1000, 0x2D8C },
    { 2413, -1000, 0x2DBE },
    { 2383, -1000, 0x2DF0 },
    { 2353, -1000, 0x2E22 },
    { 2323, -1000, 0x2E54 },
    { 2294, -1000, 0x2E85 },
    { 2265, -1000, 0x2EB7 },
    { 2236, -1000, 0x2EEA },
    { 2207, -1000, 0x2F1C },
    { 2178, -1000, 0x2F50 },
    { 2150, -1000, 0x2F83 },
    { 2122, -1000, 0x2FB7 },
    { 2093, -1000, 0x2FEC },
    { 2065, -1000, 0x3021 },
    { 2037, -1000, 0x3057 },
    { 2007, -1000, 0x308F },
    { 1977, -1000, 0x30CC },
    { 1944, -1000, 0x310D },
    { 1911, -1000, 0x3151 },
    { 1876, -1000, 0x3197 },
    { 1841, -1000, 0x31DF },
    { 1806, -1000, 0x3229 },
    { 1770, -1000, 0x3274 },
    { 1735, -1000, 0x32C0 },
    { 1700, -1000, 0x330B },
    { 1666, -1000, 0x3355 },
    { 1633, -1000, 0x339E },
    { 1601, -1000, 0x33E6 },
    { 1570, -1000, 0x342B },
    { 1542, -1000, 0x346D },
    { 1515, -1000, 0x34AB },
    { 1490, -1000, 0x34E6 },
    { 1468, -1000, 0x351C },
    { 1448, -1000, 0x354D },
    { 1431, -1000, 0x3577 },
    { 1417, -1000, 0x359C },
    { 1406, -1000, 0x35B9 },
    { 1398, -1000, 0x35CF },
    { 1393, -1000, 0x35DD },
    { 1391, -1000, 0x35E1 },
};

static AnimationPackedPose _gAcropolisPlazaAnimation148BCBank1[15] = {
#include "assets/acropolis_plaza_animation_148BC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation148BCBank4[79] = {
#include "assets/acropolis_plaza_animation_148BC_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation148BCRecords[120] = {
#include "assets/acropolis_plaza_animation_148BC_records.inc"
};

static u16 _gAcropolisPlazaAnimation148BCIndices[20] = {
#include "assets/acropolis_plaza_animation_148BC_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation148BC = {
    _gAcropolisPlazaAnimation148BCRecords,
    _gAcropolisPlazaAnimation148BCIndices,
    { NULL, _gAcropolisPlazaAnimation148BCBank1, NULL, NULL, _gAcropolisPlazaAnimation148BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation156B4Bank1[40] = {
#include "assets/acropolis_plaza_animation_156B4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation156B4Bank4[330] = {
#include "assets/acropolis_plaza_animation_156B4_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation156B4Records[424] = {
#include "assets/acropolis_plaza_animation_156B4_records.inc"
};

static u16 _gAcropolisPlazaAnimation156B4Indices[20] = {
#include "assets/acropolis_plaza_animation_156B4_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation156B4 = {
    _gAcropolisPlazaAnimation156B4Records,
    _gAcropolisPlazaAnimation156B4Indices,
    { NULL, _gAcropolisPlazaAnimation156B4Bank1, NULL, NULL, _gAcropolisPlazaAnimation156B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation15864Bank1[3] = {
#include "assets/acropolis_plaza_animation_15864_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation15864Bank4[22] = {
#include "assets/acropolis_plaza_animation_15864_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation15864Records[57] = {
#include "assets/acropolis_plaza_animation_15864_records.inc"
};

static u16 _gAcropolisPlazaAnimation15864Indices[20] = {
#include "assets/acropolis_plaza_animation_15864_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation15864 = {
    _gAcropolisPlazaAnimation15864Records,
    _gAcropolisPlazaAnimation15864Indices,
    { NULL, _gAcropolisPlazaAnimation15864Bank1, NULL, NULL, _gAcropolisPlazaAnimation15864Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation15CC8Bank1[3] = {
#include "assets/acropolis_plaza_animation_15CC8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation15CC8Bank4[111] = {
#include "assets/acropolis_plaza_animation_15CC8_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation15CC8Records[141] = {
#include "assets/acropolis_plaza_animation_15CC8_records.inc"
};

static u16 _gAcropolisPlazaAnimation15CC8Indices[20] = {
#include "assets/acropolis_plaza_animation_15CC8_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation15CC8 = {
    _gAcropolisPlazaAnimation15CC8Records,
    _gAcropolisPlazaAnimation15CC8Indices,
    { NULL, _gAcropolisPlazaAnimation15CC8Bank1, NULL, NULL, _gAcropolisPlazaAnimation15CC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation15F78Bank1[3] = {
#include "assets/acropolis_plaza_animation_15F78_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation15F78Bank4[57] = {
#include "assets/acropolis_plaza_animation_15F78_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation15F78Records[86] = {
#include "assets/acropolis_plaza_animation_15F78_records.inc"
};

static u16 _gAcropolisPlazaAnimation15F78Indices[20] = {
#include "assets/acropolis_plaza_animation_15F78_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation15F78 = {
    _gAcropolisPlazaAnimation15F78Records,
    _gAcropolisPlazaAnimation15F78Indices,
    { NULL, _gAcropolisPlazaAnimation15F78Bank1, NULL, NULL, _gAcropolisPlazaAnimation15F78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1622CBank1[9] = {
#include "assets/acropolis_plaza_animation_1622C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1622CBank4[42] = {
#include "assets/acropolis_plaza_animation_1622C_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1622CRecords[84] = {
#include "assets/acropolis_plaza_animation_1622C_records.inc"
};

static u16 _gAcropolisPlazaAnimation1622CIndices[20] = {
#include "assets/acropolis_plaza_animation_1622C_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1622C = {
    _gAcropolisPlazaAnimation1622CRecords,
    _gAcropolisPlazaAnimation1622CIndices,
    { NULL, _gAcropolisPlazaAnimation1622CBank1, NULL, NULL, _gAcropolisPlazaAnimation1622CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1643CBank1[3] = {
#include "assets/acropolis_plaza_animation_1643C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1643CBank4[33] = {
#include "assets/acropolis_plaza_animation_1643C_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1643CRecords[70] = {
#include "assets/acropolis_plaza_animation_1643C_records.inc"
};

static u16 _gAcropolisPlazaAnimation1643CIndices[20] = {
#include "assets/acropolis_plaza_animation_1643C_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1643C = {
    _gAcropolisPlazaAnimation1643CRecords,
    _gAcropolisPlazaAnimation1643CIndices,
    { NULL, _gAcropolisPlazaAnimation1643CBank1, NULL, NULL, _gAcropolisPlazaAnimation1643CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation16610Bank1[2] = {
#include "assets/acropolis_plaza_animation_16610_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation16610Bank4[26] = {
#include "assets/acropolis_plaza_animation_16610_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation16610Records[65] = {
#include "assets/acropolis_plaza_animation_16610_records.inc"
};

static u16 _gAcropolisPlazaAnimation16610Indices[20] = {
#include "assets/acropolis_plaza_animation_16610_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation16610 = {
    _gAcropolisPlazaAnimation16610Records,
    _gAcropolisPlazaAnimation16610Indices,
    { NULL, _gAcropolisPlazaAnimation16610Bank1, NULL, NULL, _gAcropolisPlazaAnimation16610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation168FCBank1[4] = {
#include "assets/acropolis_plaza_animation_168FC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation168FCBank4[63] = {
#include "assets/acropolis_plaza_animation_168FC_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation168FCRecords[92] = {
#include "assets/acropolis_plaza_animation_168FC_records.inc"
};

static u16 _gAcropolisPlazaAnimation168FCIndices[20] = {
#include "assets/acropolis_plaza_animation_168FC_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation168FC = {
    _gAcropolisPlazaAnimation168FCRecords,
    _gAcropolisPlazaAnimation168FCIndices,
    { NULL, _gAcropolisPlazaAnimation168FCBank1, NULL, NULL, _gAcropolisPlazaAnimation168FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation16D00Bank1[6] = {
#include "assets/acropolis_plaza_animation_16D00_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation16D00Bank4[92] = {
#include "assets/acropolis_plaza_animation_16D00_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation16D00Records[127] = {
#include "assets/acropolis_plaza_animation_16D00_records.inc"
};

static u16 _gAcropolisPlazaAnimation16D00Indices[20] = {
#include "assets/acropolis_plaza_animation_16D00_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation16D00 = {
    _gAcropolisPlazaAnimation16D00Records,
    _gAcropolisPlazaAnimation16D00Indices,
    { NULL, _gAcropolisPlazaAnimation16D00Bank1, NULL, NULL, _gAcropolisPlazaAnimation16D00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation16EE4Bank1[2] = {
#include "assets/acropolis_plaza_animation_16EE4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation16EE4Bank4[18] = {
#include "assets/acropolis_plaza_animation_16EE4_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation16EE4Records[77] = {
#include "assets/acropolis_plaza_animation_16EE4_records.inc"
};

static u16 _gAcropolisPlazaAnimation16EE4Indices[20] = {
#include "assets/acropolis_plaza_animation_16EE4_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation16EE4 = {
    _gAcropolisPlazaAnimation16EE4Records,
    _gAcropolisPlazaAnimation16EE4Indices,
    { NULL, _gAcropolisPlazaAnimation16EE4Bank1, NULL, NULL, _gAcropolisPlazaAnimation16EE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation170C0Bank1[3] = {
#include "assets/acropolis_plaza_animation_170C0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation170C0Bank4[29] = {
#include "assets/acropolis_plaza_animation_170C0_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation170C0Records[61] = {
#include "assets/acropolis_plaza_animation_170C0_records.inc"
};

static u16 _gAcropolisPlazaAnimation170C0Indices[20] = {
#include "assets/acropolis_plaza_animation_170C0_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation170C0 = {
    _gAcropolisPlazaAnimation170C0Records,
    _gAcropolisPlazaAnimation170C0Indices,
    { NULL, _gAcropolisPlazaAnimation170C0Bank1, NULL, NULL, _gAcropolisPlazaAnimation170C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17298Bank1[3] = {
#include "assets/acropolis_plaza_animation_17298_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17298Bank4[28] = {
#include "assets/acropolis_plaza_animation_17298_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17298Records[61] = {
#include "assets/acropolis_plaza_animation_17298_records.inc"
};

static u16 _gAcropolisPlazaAnimation17298Indices[20] = {
#include "assets/acropolis_plaza_animation_17298_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17298 = {
    _gAcropolisPlazaAnimation17298Records,
    _gAcropolisPlazaAnimation17298Indices,
    { NULL, _gAcropolisPlazaAnimation17298Bank1, NULL, NULL, _gAcropolisPlazaAnimation17298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1754CBank1[9] = {
#include "assets/acropolis_plaza_animation_1754C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1754CBank4[44] = {
#include "assets/acropolis_plaza_animation_1754C_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1754CRecords[82] = {
#include "assets/acropolis_plaza_animation_1754C_records.inc"
};

static u16 _gAcropolisPlazaAnimation1754CIndices[20] = {
#include "assets/acropolis_plaza_animation_1754C_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1754C = {
    _gAcropolisPlazaAnimation1754CRecords,
    _gAcropolisPlazaAnimation1754CIndices,
    { NULL, _gAcropolisPlazaAnimation1754CBank1, NULL, NULL, _gAcropolisPlazaAnimation1754CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17818Bank1[2] = {
#include "assets/acropolis_plaza_animation_17818_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17818Bank4[47] = {
#include "assets/acropolis_plaza_animation_17818_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17818Records[106] = {
#include "assets/acropolis_plaza_animation_17818_records.inc"
};

static u16 _gAcropolisPlazaAnimation17818Indices[20] = {
#include "assets/acropolis_plaza_animation_17818_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17818 = {
    _gAcropolisPlazaAnimation17818Records,
    _gAcropolisPlazaAnimation17818Indices,
    { NULL, _gAcropolisPlazaAnimation17818Bank1, NULL, NULL, _gAcropolisPlazaAnimation17818Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17A00Bank1[2] = {
#include "assets/acropolis_plaza_animation_17A00_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17A00Bank4[31] = {
#include "assets/acropolis_plaza_animation_17A00_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17A00Records[65] = {
#include "assets/acropolis_plaza_animation_17A00_records.inc"
};

static u16 _gAcropolisPlazaAnimation17A00Indices[20] = {
#include "assets/acropolis_plaza_animation_17A00_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17A00 = {
    _gAcropolisPlazaAnimation17A00Records,
    _gAcropolisPlazaAnimation17A00Indices,
    { NULL, _gAcropolisPlazaAnimation17A00Bank1, NULL, NULL, _gAcropolisPlazaAnimation17A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17C68Bank1[4] = {
#include "assets/acropolis_plaza_animation_17C68_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17C68Bank4[45] = {
#include "assets/acropolis_plaza_animation_17C68_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17C68Records[77] = {
#include "assets/acropolis_plaza_animation_17C68_records.inc"
};

static u16 _gAcropolisPlazaAnimation17C68Indices[20] = {
#include "assets/acropolis_plaza_animation_17C68_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17C68 = {
    _gAcropolisPlazaAnimation17C68Records,
    _gAcropolisPlazaAnimation17C68Indices,
    { NULL, _gAcropolisPlazaAnimation17C68Bank1, NULL, NULL, _gAcropolisPlazaAnimation17C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17E54Bank1[3] = {
#include "assets/acropolis_plaza_animation_17E54_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17E54Bank4[18] = {
#include "assets/acropolis_plaza_animation_17E54_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17E54Records[76] = {
#include "assets/acropolis_plaza_animation_17E54_records.inc"
};

static u16 _gAcropolisPlazaAnimation17E54Indices[20] = {
#include "assets/acropolis_plaza_animation_17E54_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17E54 = {
    _gAcropolisPlazaAnimation17E54Records,
    _gAcropolisPlazaAnimation17E54Indices,
    { NULL, _gAcropolisPlazaAnimation17E54Bank1, NULL, NULL, _gAcropolisPlazaAnimation17E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation17FF4Bank1[2] = {
#include "assets/acropolis_plaza_animation_17FF4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation17FF4Bank4[21] = {
#include "assets/acropolis_plaza_animation_17FF4_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation17FF4Records[57] = {
#include "assets/acropolis_plaza_animation_17FF4_records.inc"
};

static u16 _gAcropolisPlazaAnimation17FF4Indices[20] = {
#include "assets/acropolis_plaza_animation_17FF4_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation17FF4 = {
    _gAcropolisPlazaAnimation17FF4Records,
    _gAcropolisPlazaAnimation17FF4Indices,
    { NULL, _gAcropolisPlazaAnimation17FF4Bank1, NULL, NULL, _gAcropolisPlazaAnimation17FF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation181CCBank1[2] = {
#include "assets/acropolis_plaza_animation_181CC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation181CCBank4[16] = {
#include "assets/acropolis_plaza_animation_181CC_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation181CCRecords[76] = {
#include "assets/acropolis_plaza_animation_181CC_records.inc"
};

static u16 _gAcropolisPlazaAnimation181CCIndices[20] = {
#include "assets/acropolis_plaza_animation_181CC_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation181CC = {
    _gAcropolisPlazaAnimation181CCRecords,
    _gAcropolisPlazaAnimation181CCIndices,
    { NULL, _gAcropolisPlazaAnimation181CCBank1, NULL, NULL, _gAcropolisPlazaAnimation181CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation183B8Bank1[4] = {
#include "assets/acropolis_plaza_animation_183B8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation183B8Bank4[30] = {
#include "assets/acropolis_plaza_animation_183B8_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation183B8Records[61] = {
#include "assets/acropolis_plaza_animation_183B8_records.inc"
};

static u16 _gAcropolisPlazaAnimation183B8Indices[20] = {
#include "assets/acropolis_plaza_animation_183B8_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation183B8 = {
    _gAcropolisPlazaAnimation183B8Records,
    _gAcropolisPlazaAnimation183B8Indices,
    { NULL, _gAcropolisPlazaAnimation183B8Bank1, NULL, NULL, _gAcropolisPlazaAnimation183B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation18614Bank1[5] = {
#include "assets/acropolis_plaza_animation_18614_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation18614Bank4[44] = {
#include "assets/acropolis_plaza_animation_18614_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation18614Records[72] = {
#include "assets/acropolis_plaza_animation_18614_records.inc"
};

static u16 _gAcropolisPlazaAnimation18614Indices[20] = {
#include "assets/acropolis_plaza_animation_18614_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation18614 = {
    _gAcropolisPlazaAnimation18614Records,
    _gAcropolisPlazaAnimation18614Indices,
    { NULL, _gAcropolisPlazaAnimation18614Bank1, NULL, NULL, _gAcropolisPlazaAnimation18614Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation18904Bank1[2] = {
#include "assets/acropolis_plaza_animation_18904_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation18904Bank4[44] = {
#include "assets/acropolis_plaza_animation_18904_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation18904Records[118] = {
#include "assets/acropolis_plaza_animation_18904_records.inc"
};

static u16 _gAcropolisPlazaAnimation18904Indices[20] = {
#include "assets/acropolis_plaza_animation_18904_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation18904 = {
    _gAcropolisPlazaAnimation18904Records,
    _gAcropolisPlazaAnimation18904Indices,
    { NULL, _gAcropolisPlazaAnimation18904Bank1, NULL, NULL, _gAcropolisPlazaAnimation18904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation18DE0Bank1[9] = {
#include "assets/acropolis_plaza_animation_18DE0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation18DE0Bank4[85] = {
#include "assets/acropolis_plaza_animation_18DE0_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation18DE0Records[179] = {
#include "assets/acropolis_plaza_animation_18DE0_records.inc"
};

static u16 _gAcropolisPlazaAnimation18DE0Indices[20] = {
#include "assets/acropolis_plaza_animation_18DE0_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation18DE0 = {
    _gAcropolisPlazaAnimation18DE0Records,
    _gAcropolisPlazaAnimation18DE0Indices,
    { NULL, _gAcropolisPlazaAnimation18DE0Bank1, NULL, NULL, _gAcropolisPlazaAnimation18DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation18F98Bank1[2] = {
#include "assets/acropolis_plaza_animation_18F98_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation18F98Bank4[8] = {
#include "assets/acropolis_plaza_animation_18F98_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation18F98Records[76] = {
#include "assets/acropolis_plaza_animation_18F98_records.inc"
};

static u16 _gAcropolisPlazaAnimation18F98Indices[20] = {
#include "assets/acropolis_plaza_animation_18F98_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation18F98 = {
    _gAcropolisPlazaAnimation18F98Records,
    _gAcropolisPlazaAnimation18F98Indices,
    { NULL, _gAcropolisPlazaAnimation18F98Bank1, NULL, NULL, _gAcropolisPlazaAnimation18F98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation19610Bank1[5] = {
#include "assets/acropolis_plaza_animation_19610_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation19610Bank4[173] = {
#include "assets/acropolis_plaza_animation_19610_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation19610Records[206] = {
#include "assets/acropolis_plaza_animation_19610_records.inc"
};

static u16 _gAcropolisPlazaAnimation19610Indices[20] = {
#include "assets/acropolis_plaza_animation_19610_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation19610 = {
    _gAcropolisPlazaAnimation19610Records,
    _gAcropolisPlazaAnimation19610Indices,
    { NULL, _gAcropolisPlazaAnimation19610Bank1, NULL, NULL, _gAcropolisPlazaAnimation19610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation19AD8Bank1[7] = {
#include "assets/acropolis_plaza_animation_19AD8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation19AD8Bank4[115] = {
#include "assets/acropolis_plaza_animation_19AD8_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation19AD8Records[150] = {
#include "assets/acropolis_plaza_animation_19AD8_records.inc"
};

static u16 _gAcropolisPlazaAnimation19AD8Indices[20] = {
#include "assets/acropolis_plaza_animation_19AD8_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation19AD8 = {
    _gAcropolisPlazaAnimation19AD8Records,
    _gAcropolisPlazaAnimation19AD8Indices,
    { NULL, _gAcropolisPlazaAnimation19AD8Bank1, NULL, NULL, _gAcropolisPlazaAnimation19AD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation19D64Bank1[6] = {
#include "assets/acropolis_plaza_animation_19D64_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation19D64Bank4[47] = {
#include "assets/acropolis_plaza_animation_19D64_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation19D64Records[78] = {
#include "assets/acropolis_plaza_animation_19D64_records.inc"
};

static u16 _gAcropolisPlazaAnimation19D64Indices[20] = {
#include "assets/acropolis_plaza_animation_19D64_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation19D64 = {
    _gAcropolisPlazaAnimation19D64Records,
    _gAcropolisPlazaAnimation19D64Indices,
    { NULL, _gAcropolisPlazaAnimation19D64Bank1, NULL, NULL, _gAcropolisPlazaAnimation19D64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation19F7CBank1[2] = {
#include "assets/acropolis_plaza_animation_19F7C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation19F7CBank4[34] = {
#include "assets/acropolis_plaza_animation_19F7C_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation19F7CRecords[74] = {
#include "assets/acropolis_plaza_animation_19F7C_records.inc"
};

static u16 _gAcropolisPlazaAnimation19F7CIndices[20] = {
#include "assets/acropolis_plaza_animation_19F7C_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation19F7C = {
    _gAcropolisPlazaAnimation19F7CRecords,
    _gAcropolisPlazaAnimation19F7CIndices,
    { NULL, _gAcropolisPlazaAnimation19F7CBank1, NULL, NULL, _gAcropolisPlazaAnimation19F7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1A4F8Bank1[5] = {
#include "assets/acropolis_plaza_animation_1A4F8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1A4F8Bank4[137] = {
#include "assets/acropolis_plaza_animation_1A4F8_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1A4F8Records[179] = {
#include "assets/acropolis_plaza_animation_1A4F8_records.inc"
};

static u16 _gAcropolisPlazaAnimation1A4F8Indices[20] = {
#include "assets/acropolis_plaza_animation_1A4F8_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1A4F8 = {
    _gAcropolisPlazaAnimation1A4F8Records,
    _gAcropolisPlazaAnimation1A4F8Indices,
    { NULL, _gAcropolisPlazaAnimation1A4F8Bank1, NULL, NULL, _gAcropolisPlazaAnimation1A4F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1A784Bank1[7] = {
#include "assets/acropolis_plaza_animation_1A784_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1A784Bank4[45] = {
#include "assets/acropolis_plaza_animation_1A784_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1A784Records[77] = {
#include "assets/acropolis_plaza_animation_1A784_records.inc"
};

static u16 _gAcropolisPlazaAnimation1A784Indices[20] = {
#include "assets/acropolis_plaza_animation_1A784_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1A784 = {
    _gAcropolisPlazaAnimation1A784Records,
    _gAcropolisPlazaAnimation1A784Indices,
    { NULL, _gAcropolisPlazaAnimation1A784Bank1, NULL, NULL, _gAcropolisPlazaAnimation1A784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1AAACBank1[2] = {
#include "assets/acropolis_plaza_animation_1AAAC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1AAACBank4[66] = {
#include "assets/acropolis_plaza_animation_1AAAC_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1AAACRecords[110] = {
#include "assets/acropolis_plaza_animation_1AAAC_records.inc"
};

static u16 _gAcropolisPlazaAnimation1AAACIndices[20] = {
#include "assets/acropolis_plaza_animation_1AAAC_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1AAAC = {
    _gAcropolisPlazaAnimation1AAACRecords,
    _gAcropolisPlazaAnimation1AAACIndices,
    { NULL, _gAcropolisPlazaAnimation1AAACBank1, NULL, NULL, _gAcropolisPlazaAnimation1AAACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1AE04Bank1[2] = {
#include "assets/acropolis_plaza_animation_1AE04_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1AE04Bank4[78] = {
#include "assets/acropolis_plaza_animation_1AE04_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1AE04Records[110] = {
#include "assets/acropolis_plaza_animation_1AE04_records.inc"
};

static u16 _gAcropolisPlazaAnimation1AE04Indices[20] = {
#include "assets/acropolis_plaza_animation_1AE04_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1AE04 = {
    _gAcropolisPlazaAnimation1AE04Records,
    _gAcropolisPlazaAnimation1AE04Indices,
    { NULL, _gAcropolisPlazaAnimation1AE04Bank1, NULL, NULL, _gAcropolisPlazaAnimation1AE04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1AFA4Bank1[2] = {
#include "assets/acropolis_plaza_animation_1AFA4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1AFA4Bank4[19] = {
#include "assets/acropolis_plaza_animation_1AFA4_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1AFA4Records[59] = {
#include "assets/acropolis_plaza_animation_1AFA4_records.inc"
};

static u16 _gAcropolisPlazaAnimation1AFA4Indices[20] = {
#include "assets/acropolis_plaza_animation_1AFA4_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1AFA4 = {
    _gAcropolisPlazaAnimation1AFA4Records,
    _gAcropolisPlazaAnimation1AFA4Indices,
    { NULL, _gAcropolisPlazaAnimation1AFA4Bank1, NULL, NULL, _gAcropolisPlazaAnimation1AFA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPlazaAnimation1B1F8Bank1[5] = {
#include "assets/acropolis_plaza_animation_1B1F8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPlazaAnimation1B1F8Bank4[42] = {
#include "assets/acropolis_plaza_animation_1B1F8_bank4.inc"
};

static AnimationRecord _gAcropolisPlazaAnimation1B1F8Records[72] = {
#include "assets/acropolis_plaza_animation_1B1F8_records.inc"
};

static u16 _gAcropolisPlazaAnimation1B1F8Indices[20] = {
#include "assets/acropolis_plaza_animation_1B1F8_indices.inc"
};

AnimationSet gAcropolisPlazaAnimation1B1F8 = {
    _gAcropolisPlazaAnimation1B1F8Records,
    _gAcropolisPlazaAnimation1B1F8Indices,
    { NULL, _gAcropolisPlazaAnimation1B1F8Bank1, NULL, NULL, _gAcropolisPlazaAnimation1B1F8Bank4, NULL, NULL, NULL },
};

s16 D_acropolis_plaza_801987E0[32] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
};

SVECTOR D_acropolis_plaza_80198820[19] = {
    { 0x5F5A, -710, 450, 0 },
    { 0x6608, -1540, 2110, 0 },
    { 0x66A8, -1540, 1960, 0 },
    { 0x57D0, -1540, -310, 0 },
    { 0x57B2, -1540, -530, 0 },
    { 0x578A, -1540, -880, 0 },
    { 0x5776, -1540, -1090, 0 },
    { 0x3890, -1540, 8130, 0 },
    { 0x37D2, -1540, 8230, 0 },
    { 0x3692, -1540, 8390, 0 },
    { 0x35D4, -1540, 8490, 0 },
    { 0x4E8E, -690, 140, 0 },
    { 0x4308, -2020, -290, 0 },
    { 0x438A, -2020, 480, 0 },
    { 0x2C06, -1680, 2230, 0 },
    { 0x2990, -1680, 960, 0 },
    { 0x2B7A, -640, 3390, 0 },
    { 9580, -1070, 3620, 0 },
    { 7970, -640, 4980, 0 },
};

WorldCollisionRoomResources D_acropolis_plaza_801988B8[1] = {
    { D_acropolis_plaza_80199180, NULL, &D_acropolis_plaza_801991A4, NULL },
};

u8* D_acropolis_plaza_801988C8[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_plaza_801988CC[1] = { 8 };

WorldCoordRoomLighting D_acropolis_plaza_801988D0[1] = {
    { D_acropolis_plaza_80199EE8, NULL },
};

ViewCamera D_acropolis_plaza_801988D8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x61A8, 0 } }, 380 },
    { { { { 3907, 0, 1229 }, { 691, 3385, -2199 }, { -1015, 2305, 3229 } }, { -3330, 0x2EE0, 0x332C } }, 207 },
    { { { { 1614, 0, 3764 }, { 917, 3972, -393 }, { -3650, 998, 1565 } }, { -2710, 3765, 6480 } }, 230 },
    { { { { 3803, 0, -1520 }, { -188, 4064, -471 }, { 1508, 507, 3773 } }, { 5630, 3710, 0x2C7E } }, 257 },
    { { { { 3820, 0, 1476 }, { 178, 4066, -461 }, { -1465, 494, 3792 } }, { -5330, 3510, 0x2904 } }, 230 },
    { { { { -1334, 0, -3872 }, { -103, 4094, 35 }, { 3871, 109, -1334 } }, { 2600, 3765, -4090 } }, 246 },
    { { { { -920, 0, -3991 }, { 300, 4084, -69 }, { 3979, -308, -917 } }, { 6160, 3515, -3630 } }, 207 },
    { { { { 4051, 0, -604 }, { 18, 4094, 124 }, { 604, -126, 4049 } }, { 5040, 3595, 5660 } }, 225 },
};

SpriteBatch D_acropolis_plaza_801989F8[1] = {
    { SPRITE_BATCH_END, 1, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_plaza_80198A00[1] = {
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_plaza_80198A08[8] = {
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_801989F8, NULL },
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_80198A00, NULL },
    { { .elements = NULL }, D_acropolis_plaza_801989F8, NULL },
};

DirectionWarpEntry D_acropolis_plaza_80198A68[1] = {
    { { { .word = 3072 }, 0x5AA0, 0, 1800 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x5AA0, 0, 1800 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gAcropolisPlazaCollision1BBC0Normals[30] = {
#include "assets/acropolis_plaza_collision_1BBC0_normals.inc"
};

static SVECTOR _gAcropolisPlazaCollision1BBC0Verts[80] = {
#include "assets/acropolis_plaza_collision_1BBC0_verts.inc"
};

static WorldCollisionGridFace _gAcropolisPlazaCollision1BBC0Faces[32] = {
#include "assets/acropolis_plaza_collision_1BBC0_faces.inc"
};

static s16 _gAcropolisPlazaCollision1BBC0Cells[192] = {
#include "assets/acropolis_plaza_collision_1BBC0_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisPlazaCollision1BBC0Cells[i])
static s16* _gAcropolisPlazaCollision1BBC0Table[28] = {
#include "assets/acropolis_plaza_collision_1BBC0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_plaza_80199180[1] = {
    { NULL, _gAcropolisPlazaCollision1BBC0Normals, _gAcropolisPlazaCollision1BBC0Verts, _gAcropolisPlazaCollision1BBC0Faces, _gAcropolisPlazaCollision1BBC0Table, 0, 0, 7, 4, 4000, 32 },
};

WorldCollisionTrigger D_acropolis_plaza_801991A4 = { NULL, NULL, NULL, { 5744, -112, 8227, 0 }, { { 2736, 0, 3200, 0 }, { -3471, 0, -2304, 0 }, { 3471, 0, 2305, 0 }, { -2736, 0, -3199, 0 } }, { 0, 4113, 0, 0 }, { 2106, 0, -3513, 0 }, 4190, WORLD_COLLISION_TRIGGER_ACTION_CLEAR | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 119, WORLD_COLLISION_TRIGGER_QUAD, 0 };

WorldCollisionTrigger D_acropolis_plaza_801991F0 = { NULL, NULL, NULL, { 0x4336, -32, 3038, 0 }, { { 1967, 0, 3691, 0 }, { -3037, 0, -2925, 0 }, { 3036, 0, 2925, 0 }, { -1968, 0, -3691, 0 } }, { 0, 4098, 0, 0 }, { 2106, 0, -3513, 0 }, 4190, WORLD_COLLISION_TRIGGER_ACTION_CLEAR | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 };

WorldCollisionTrigger D_acropolis_plaza_8019923C[4] = {
    { NULL, NULL, NULL, { 928, -16, 0x3370, 0 }, { { 2575, 0, 3297, 0 }, { -3632, 0, -2207, 0 }, { 3632, 0, 2208, 0 }, { -2575, 0, -3296, 0 } }, { 0, 4107, 0, 0 }, { 2106, 0, -3513, 0 }, 4222, WORLD_COLLISION_TRIGGER_ACTION_CLEAR | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x352F, 0, 5313, 0 }, { { 573, 0, 1873, 0 }, { -1312, 0, -558, 0 }, { 1601, 0, 143, 0 }, { -860, 0, -1457, 0 } }, { 0, 4106, 0, 0 }, { 2910, 0, -2886, 0 }, 1958, WORLD_COLLISION_TRIGGER_ACTION_CLEAR, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4C3F, -32, 574, 0 }, { { 1781, 0, -112, 0 }, { -480, 0, 1609, 0 }, { 481, 0, -1607, 0 }, { -1748, 0, 1137, 0 } }, { 0, 4108, 0, 0 }, { 2106, 0, -3513, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CLEAR, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5AA0, -32, 2048, 0 }, { { 2418, 0, 652, 0 }, { -2420, 0, 505, 0 }, { 2419, 0, -506, 0 }, { -2419, 0, -653, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4092, 0 }, 2495, WORLD_COLLISION_TRIGGER_ACTION_CLEAR, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_plaza_8019936C[3] = {
    { 109, 101, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_310100_80179920 },
    { 108, 101, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_310100_801798FC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_plaza_80199390[3] = {
    { NULL, NULL },
    { D_map_akropolis_8017AFEC, D_acropolis_plaza_8019936C },
    { NULL, NULL },
};

/// Point lights for model shading throughout Acropolis Plaza.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is 1.0). Every entry admits all views.
/// The loaded plaza overlay owns this writable array; gameplay borrows it and
/// updates coordinate caches and attenuation in place while the room is active.
static WorldCoordPointLight _gAcropolisPlazaPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 26962, -10791, -9776 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2638, 1975, 1619 },
        },
        .inner = 10,
        .outer = 100000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 17901, -2152, 16 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2161, 2161, 1577 },
        },
        .inner = 6000,
        .outer = 7400,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 12032, -1488, 1096 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1507, 152, 27 },
        },
        .inner = 2500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10729, -1039, 3948 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 929, 261, 199 },
        },
        .inner = 1000,
        .outer = 2300,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9990, -1039, 5041 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1181, 332, 253 },
        },
        .inner = 2000,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 11445, -1565, -2016 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1130, 317, 242 },
        },
        .inner = 1000,
        .outer = 2300,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6234, -2606, -8 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 12287, 12288, 5503 },
        },
        .inner = 400,
        .outer = 600,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6742, -2680, -1938 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 12288, 1246, 226 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 26121, -1621, 2110 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 480, 1207, 3919 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 22486, -1617, -318 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 480, 1207, 3919 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 22459, -1617, -534 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 480, 1207, 3919 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 22417, -1617, -889 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3869, 1207, 603 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 22391, -1617, -1101 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3869, 1207, 603 },
        },
        .inner = 250,
        .outer = 500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -17126, -10791, -9776 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2638, 1975, 1619 },
        },
        .inner = 10,
        .outer = 100000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 23982, -856, 401 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2412, 2412, 1435 },
        },
        .inner = 2000,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1009, -1457, 10592 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1617, 2412, 2027 },
        },
        .inner = 4000,
        .outer = 5500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3234, -3771, 14314 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2467, 452 },
        },
        .inner = 600,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4397, -3771, 14314 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2467, 452 },
        },
        .inner = 600,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5654, -3771, 14314 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2467, 452 },
        },
        .inner = 600,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6816, -3771, 14314 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2467, 452 },
        },
        .inner = 600,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8999, -1457, 10592 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1617, 2412, 2039 },
        },
        .inner = 4000,
        .outer = 5500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9603, -3771, 14314 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2467, 452 },
        },
        .inner = 600,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 22810, -268, 4636 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2768, 1005 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8016, -994, 6602 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4095, 2434, 226 },
        },
        .inner = 800,
        .outer = 1200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -13098, -994, 6602 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4095, 2434, 226 },
        },
        .inner = 800,
        .outer = 1200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5017, -1457, 10592 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1617, 2412, 2039 },
        },
        .inner = 4000,
        .outer = 5500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8267, -268, 4788 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2768, 1005 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7195, -268, 5842 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 2768, 1005 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3017, -1457, 10592 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1617, 2412, 2027 },
        },
        .inner = 4000,
        .outer = 5500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7017, -1457, 10592 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 1617, 2412, 2039 },
        },
        .inner = 4000,
        .outer = 5500,
    },
};

WorldCoordRoomLights D_acropolis_plaza_80199EE8[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisPlazaPointLights), _gAcropolisPlazaPointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_acropolis_plaza_80199F00 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionFootstepSounds D_acropolis_plaza_80199F0C = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_acropolis_plaza_80199F18[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_plaza_80199F00 },
};

WorldCollisionSurfaceProperties D_acropolis_plaza_80199F20[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_plaza_80199F0C },
};

WorldCollisionSurfaceProperties* D_acropolis_plaza_80199F28[8] = {
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F20,
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F18,
    D_acropolis_plaza_80199F18,
};

static void            func_acropolis_plaza_8017DD90(Task* arg0);
static void            func_acropolis_plaza_8017DE24(s32 arg0);
static __inline__ void plaza_updateEdgeFlags(_AcropolisPlazaSceneWork* work);
static void            func_acropolis_plaza_8017F770(u16 fadeIn, u16 fadeOut, u16 hold, u16* state, s32 sndId, u16 mode);
static void            func_acropolis_plaza_8017F9EC(Task* task);
static u16             func_acropolis_plaza_8017FB50(Task* task);

/// Per-frame service step for the plaza's streamed cutscene commands.
///
/// Only runs while the slot `gCdCmdQueue.readIdx` selects holds one of the
/// stream opcodes 0x71..0x73; the entry packs the slot in `args.stream.slotIndex`
/// and a signed sector offset in `args.stream.sectorOffsetHigh:sectorOffsetLow`.
/// Step 0 waits for `CdCmd_PollStatus`: status
/// 0 keeps waiting, status 2 flushes the drive first, and status 1 (or 2)
/// promotes a 0x72 entry to 0x71 -- clearing the MDEC strip counters -- kicks
/// the decoder, primes `Stream_PollPlayback` and advances to step 1. Step 1 polls
/// `Stream_PollPlayback` every frame and retires the command once it reports done.
void func_acropolis_plaza_8017D6D4(void)
{
    CdCmdQueue* q;
    CdCmdEntry* entry;
    s16         slot;
    s16         sectorOffset;
    s32         cmd;

    q            = &gCdCmdQueue;
    entry        = &q->entries[q->readIdx];
    cmd          = entry->cmd;
    slot         = entry->args.stream.slotIndex;
    sectorOffset = entry->args.stream.sectorOffsetLow | (entry->args.stream.sectorOffsetHigh << 8);

    if (cmd != CD_COMMAND_EMPTY) {
        if (cmd >= 0) {
            if (cmd < CD_COMMAND_RESUME_STREAM_AT_POSITION + 1) {
                if (cmd >= CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                    switch (q->step) {
                        case 0:
                            switch ((s16)CdCmd_PollStatus(0, 0)) {
                                case 0:
                                    return;
                                case 2:
                                    CdFlush();
                                    /* fallthrough */
                                case 1:
                                    if (q->entries[q->readIdx].cmd == CD_COMMAND_RESET_STREAM_AT_OFFSET) {
                                        D_8005EAEC                 = 0;
                                        D_8005EAEE                 = 0;
                                        q->entries[q->readIdx].cmd = CD_COMMAND_PLAY_STREAM_AT_OFFSET;
                                    }
                                    Stream_KickDecode(slot & 0xFFFF);
                                    if (q->entries[q->readIdx].cmd == CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                                        Stream_PollPlayback(0, sectorOffset);
                                    } else if (q->entries[q->readIdx].cmd == CD_COMMAND_RESUME_STREAM_AT_POSITION) {
                                        Stream_PollPlayback(1, q->activeRequest.resumeSector);
                                    }
                                    q->step++;
                                    break;
                            }
                            /* fallthrough */
                        case 1:
                            if (q->entries[q->readIdx].cmd == CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                                if (Stream_PollPlayback(0, sectorOffset) != 0) {
                                    cdCmdCompleteHeadRequest();
                                }
                            } else if (q->entries[q->readIdx].cmd == CD_COMMAND_RESUME_STREAM_AT_POSITION) {
                                if (Stream_PollPlayback(1, q->activeRequest.resumeSector) != 0) {
                                    cdCmdCompleteHeadRequest();
                                }
                            }
                            break;
                    }
                }
            }
        }
    }
}

/// Fades the plaza to white, then blanks the display and ends the task.
///
/// `spawnArg1.value` supplies a positive intensity increment per frame (the
/// scene passes 9); its low halfword is added to signed-16-bit ramp channels.
/// The first tick owns a `ScreenFadeWork` allocation at `Task::work`, released
/// by task teardown. Allocation failure ends the task without blanking.
/// Requires the current frame's primitive arena and front ordering-table tags.
static void _acropolisPlazaFadeToWhiteTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_FADE_INIT               = 0,
        ACROPOLIS_PLAZA_FADE_DRAW               = 1,
        ACROPOLIS_PLAZA_FADE_COMPLETE_INTENSITY = 256,
        ACROPOLIS_PLAZA_FADE_FRONT_TAG_OFFSET   = -16,
        ACROPOLIS_PLAZA_FADE_ADDITIVE_DRAW_MODE = 0xE1000240
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* newFade;
    u8              red;
    u8              green;
    TILE*           tile;
    DR_TPAGE*       drawMode;

    fade = task->work;
    switch (task->state) {
        case ACROPOLIS_PLAZA_FADE_INIT:
            newFade    = memMalloc(sizeof(*newFade), false);
            task->work = newFade;
            if (newFade == NULL) {
                taskKill(task);
                break;
            }
            fade         = newFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            /* fallthrough */
        case ACROPOLIS_PLAZA_FADE_DRAW:
            red            = fade->r;
            green          = fade->g;
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setTile(tile);
            setSemiTrans(tile, 1);
            tile->r0 = red;
            tile->g0 = green;
            tile->b0 = red;
            tile->x0 = -ACROPOLIS_PLAZA_SCREEN_WIDTH / 2;
            tile->y0 = -ACROPOLIS_PLAZA_SCREEN_HEIGHT / 2;
            tile->w  = ACROPOLIS_PLAZA_SCREEN_WIDTH;
            tile->h  = ACROPOLIS_PLAZA_SCREEN_HEIGHT;
            addPrim(gGpuCurrentOt + ACROPOLIS_PLAZA_FADE_FRONT_TAG_OFFSET, tile);

            drawMode       = gGpuPrimCursor;
            gGpuPrimCursor = drawMode + 1;
            setlen(drawMode, 1);
            // Prepending the draw mode makes additive blending apply to the tile.
            drawMode->code[0] = ACROPOLIS_PLAZA_FADE_ADDITIVE_DRAW_MODE;
            addPrim(gGpuCurrentOt + ACROPOLIS_PLAZA_FADE_FRONT_TAG_OFFSET, drawMode);

            fade->r += (u16)task->spawnArg1.value;
            fade->g += (u16)task->spawnArg1.value;
            fade->b += (u16)task->spawnArg1.value;
            if (fade->r >= ACROPOLIS_PLAZA_FADE_COMPLETE_INTENSITY) {
                SetDispMask(0);
                taskKill(task);
            }
            break;
    }
}

#include "../../shared/screen_fade_in_tile.inc.c"

void func_acropolis_plaza_8017DBFC(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed() != 0) {
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case 4:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            }
            break;
        case 5:
            Stream_ResetRestoreState();
            task->state++;
            break;
        case 6:
            if (Stream_RestoreAfterLoad(1, 0) & 0xFFFF) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Rebuilds the eight box vertices in `_gAcropolisPlazaCollision1BBC0Verts` around
/// the scene work's `shotPos`.
static void func_acropolis_plaza_8017DD90(Task* arg0)
{
    _AcropolisPlazaSceneWork* work = (_AcropolisPlazaSceneWork*)arg0->work;
    s32                       x    = work->shotPos.vx;
    s32                       y    = work->shotPos.vy;
    s32                       z    = work->shotPos.vz;
    s16                       near = x - 0xBB8;
    s16                       top;
    s16                       left;
    s16                       right;
    s16                       far;

    _gAcropolisPlazaCollision1BBC0Verts[0].vx = near;
    _gAcropolisPlazaCollision1BBC0Verts[1].vx = near;
    _gAcropolisPlazaCollision1BBC0Verts[2].vx = near;
    _gAcropolisPlazaCollision1BBC0Verts[3].vx = near;

    top   = y + 0x3E8;
    left  = z - 0x1000;
    right = z + 0x3000;
    far   = x + 0x7D0;

    _gAcropolisPlazaCollision1BBC0Verts[0].vy = y;
    _gAcropolisPlazaCollision1BBC0Verts[1].vy = y;
    _gAcropolisPlazaCollision1BBC0Verts[2].vy = top;
    _gAcropolisPlazaCollision1BBC0Verts[3].vy = top;

    _gAcropolisPlazaCollision1BBC0Verts[0].vz = left;
    _gAcropolisPlazaCollision1BBC0Verts[1].vz = right;
    _gAcropolisPlazaCollision1BBC0Verts[2].vz = left;
    _gAcropolisPlazaCollision1BBC0Verts[3].vz = right;

    _gAcropolisPlazaCollision1BBC0Verts[4].vx = far;
    _gAcropolisPlazaCollision1BBC0Verts[5].vx = far;
    _gAcropolisPlazaCollision1BBC0Verts[6].vx = far;
    _gAcropolisPlazaCollision1BBC0Verts[7].vx = far;

    _gAcropolisPlazaCollision1BBC0Verts[4].vy = y;
    _gAcropolisPlazaCollision1BBC0Verts[5].vy = y;
    _gAcropolisPlazaCollision1BBC0Verts[6].vy = top;
    _gAcropolisPlazaCollision1BBC0Verts[7].vy = top;

    _gAcropolisPlazaCollision1BBC0Verts[4].vz = right;
    _gAcropolisPlazaCollision1BBC0Verts[5].vz = left;
    _gAcropolisPlazaCollision1BBC0Verts[6].vz = right;
    _gAcropolisPlazaCollision1BBC0Verts[7].vz = left;
}

/// Applies the plaza camera for view set `arg0`.
///
/// Sets 0..3 all share the opening table and pick within a row directly:
/// while `movieReady` is clear the row's first shot is used, otherwise
/// `movieFrameSubstep` steps forward or back from it depending on `reverseSceneFrames`.
/// Sets 4..7 (and any out-of-range value, which leaves the table whatever the
/// caller left in place) instead index the table flat, one shot per step, and
/// clamp the backwards walk at the start of the table.
static void func_acropolis_plaza_8017DE24(s32 arg0)
{
    CdCmdQueue* q = &gCdCmdQueue;
    ViewCamera(*tbl)[2];
    ViewCamera* view;
    s16         idx;

    switch ((u16)arg0) {
        case 0:
        case 1:
        case 2:
        case 3:
            tbl = D_acropolis_plaza_801838B8;
            if (q->movieReady == 0) {
                s32 pair = q->sceneFrame - 1;
                view     = tbl[pair];
            } else if (q->reverseSceneFrames == 0) {
                s32 pair = q->sceneFrame - 1;
                view     = tbl[pair] + q->movieFrameSubstep;
            } else {
                s32 pair = q->sceneFrame - 1;
                view     = tbl[pair] - q->movieFrameSubstep;
            }
            viewApplyCamera(view);
            return;
        case 4:
            tbl = D_acropolis_plaza_8018A938;
            break;
        case 5:
            tbl = D_acropolis_plaza_8018CAFC;
            break;
        case 6:
            tbl = D_acropolis_plaza_8018F530;
            break;
        case 7:
            tbl = D_acropolis_plaza_8018F9B4;
            break;
    }
    if (q->movieReady == 0) {
        s32 pair = q->sceneFrame - 1;
        view     = tbl[pair];
    } else {
        if (q->reverseSceneFrames == 0) {
            idx = ((q->sceneFrame - 1) * 2) + q->movieFrameSubstep + 1;
        } else {
            idx = ((q->sceneFrame - 1) * 2) - q->movieFrameSubstep - 1;
            if (idx < 0) {
                idx = 0;
            }
        }
        view = *tbl + idx;
    }
    viewApplyCamera(view);
}

/// Recomputes `fwd` and `back` from `relX`.
///
/// At or beyond the forward edge the player has walked toward later frames;
/// below the back edge, toward earlier ones. Scene frame 1 has no earlier
/// shot, so the back flag stays clear there. Running stores the run variant
/// of whichever flag fired.
static __inline__ void plaza_updateEdgeFlags(_AcropolisPlazaSceneWork* work)
{
    CdCmdQueue* cq   = &gCdCmdQueue;
    s32         dist = work->relX;

    work->fwd  = ACROPOLIS_PLAZA_EDGE_NONE;
    work->back = ACROPOLIS_PLAZA_EDGE_NONE;
    if (dist >= ACROPOLIS_PLAZA_FORWARD_EDGE) {
        if ((u16)work->player->movementMode == ACROPOLIS_PLAZA_MOVEMENT_RUNNING) {
            work->fwd = ACROPOLIS_PLAZA_EDGE_RUN;
        } else {
            work->fwd = ACROPOLIS_PLAZA_EDGE_WALK;
        }
    } else if (dist < ACROPOLIS_PLAZA_BACK_EDGE) {
        if (cq->sceneFrame != ACROPOLIS_PLAZA_FIRST_SCENE_FRAME) {
            if ((u16)work->player->movementMode == ACROPOLIS_PLAZA_MOVEMENT_RUNNING) {
                work->back = ACROPOLIS_PLAZA_EDGE_RUN;
            } else {
                work->back = ACROPOLIS_PLAZA_EDGE_WALK;
            }
        }
    }
}

/// The plaza's scene task: it plays the room's pre-rendered camera stream and
/// re-seeks it whenever the player walks past the end of the current shot.
///
/// State 0 allocates the work block, seeds `gCdCmdQueue` from the spawn
/// argument and, unless the argument suppresses it, asks for the opening
/// stream at the fine frame step. State 1 caches the player task once the CD
/// is idle and turns the display on. State 2 refreshes the edge flags, and
/// when one fires it enqueues a play-at-offset seek: forward selects
/// `forwardSubId` and reverse selects the next sub-id. Continuing counts from
/// the movie frame and reversing counts back from the frame limit, in fine or
/// coarse stream frames. States 3..5 wait for that seek to land and return to
/// state 2. State 6 parks while `movieAtEnd` is set, and leaves when the edge
/// flag opposite the current sub-id fires.
///
/// `loMask` holds 0xFF in a local on purpose: masking with a literal lets GCC
/// fold the `andi` into the byte store, and the original build keeps it.
void func_acropolis_plaza_8017DFE0(Task* task)
{
    u8                        slot[4];
    s32                       frameOfs;
    u32                       seekFrame;
    u32                       openFrame;
    s32                       loMask = 0xFF;
    s32                       side;
    _AcropolisPlazaSceneWork* block;
    CdCmdQueue*               q;
    _AcropolisPlazaSceneWork* work;
    _AcropolisPlazaSceneArg*  arg;
    Task*                     playerTask;
    u16                       startFrame;

    q    = &gCdCmdQueue;
    work = (_AcropolisPlazaSceneWork*)task->work;

    if (task->state != 0) {
        // Sub-ids 0..3 share the shot-position table, one row per scene frame.
        switch (q->plazaStreamSubId) {
            case 0:
            case 1:
            case 2:
            case 3:
                work->shotPos.vx = D_acropolis_plaza_801907C4[q->sceneFrame - 1].vx;
                work->shotPos.vy = D_acropolis_plaza_801907C4[q->sceneFrame - 1].vy;
                work->shotPos.vz = D_acropolis_plaza_801907C4[q->sceneFrame - 1].vz;
                work->relX       = work->shotPos.vx - work->playerMtx->t[0];
                break;
        }
    }

    switch (task->state) {
        case 0:
            block      = memMalloc(sizeof(*block), false);
            task->work = block;
            if (block == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(block, 0, sizeof(*block));
            arg                   = task->spawnArg2.pointer;
            work                  = (_AcropolisPlazaSceneWork*)task->work;
            startFrame            = arg->startFrame;
            q->plazaStreamSubId   = 0;
            q->sceneFrame         = startFrame;
            q->movieFrame         = startFrame;
            work->prevReverse     = 0;
            q->reverseSceneFrames = 0;
            q->movieReady         = 0;
            q->continueMovie      = 0;
            // The argument is fetched from the task again after the queue stores.
            arg = task->spawnArg2.pointer;
            if (arg->skipStreamReset == 0) {
                slot[0]   = streamFindMovieSlot(&gGameSession->location.loc, q->plazaStreamSubId, 0);
                frameOfs  = (q->movieFrame - 1) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                openFrame = frameOfs & 0xFFFF;
                slot[1]   = openFrame >> 8;
                slot[2]   = openFrame & loMask;
                cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, slot);
            } else {
                q->movieAtEnd = 0;
            }
            ((_AcropolisPlazaSceneWork*)task->work)->playerMtx = gPlayerStatus.coordMtx;
            work->frameStep                                    = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
            task->state                                        = task->state + 1;
            break;
        case 1:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            work->playerTask = playerTask;
            work->player     = (GameActor*)playerTask->work;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case 2:
        L_case2:
            work->frameLimit = streamGetFrameLimit(q->plazaStreamSubId);
            if (q->movieAtEnd != 0) {
                task->state = 6;
                break;
            }
            plaza_updateEdgeFlags((_AcropolisPlazaSceneWork*)task->work);
            if (work->fwd != ACROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse     = q->reverseSceneFrames;
                q->reverseSceneFrames = 0;
            }
            if (work->back != ACROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse     = q->reverseSceneFrames;
                q->reverseSceneFrames = 1;
            }
            if (work->fwd == ACROPOLIS_PLAZA_EDGE_NONE && work->back == ACROPOLIS_PLAZA_EDGE_NONE) {
                break;
            }
            // Continuing counts from the movie frame; reversing counts back from the frame limit.
            q->continueMovie = 1;
            side             = q->reverseSceneFrames;
            if (side == work->prevReverse) {
                if (side == 0) {
                    if (work->fwd == ACROPOLIS_PLAZA_EDGE_WALK) {
                        q->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            frameOfs = q->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            frameOfs = q->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (side == 1) {
                    if (work->back == side) {
                        q->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            frameOfs = q->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            frameOfs = q->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                }
            } else {
                if (side == 0) {
                    if (work->fwd == ACROPOLIS_PLAZA_EDGE_WALK) {
                        q->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            frameOfs = (work->frameLimit - q->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            frameOfs = (work->frameLimit - q->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (side == 1) {
                    if (work->back == side) {
                        q->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            frameOfs = (work->frameLimit - q->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            frameOfs = (work->frameLimit - q->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                }
            }
            slot[0]   = streamFindMovieSlot(&gGameSession->location.loc, q->plazaStreamSubId, 0);
            seekFrame = frameOfs & 0xFFFF;
            slot[1]   = seekFrame >> 8;
            slot[2]   = seekFrame;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM_AT_OFFSET, 0, slot);
            q->movieReady = 0;
            task->state   = task->state + 1;
            break;
        case 3:
            if (q->movieReady != 0) {
                task->state = task->state + 1;
            }
            break;
        case 4:
            plaza_updateEdgeFlags((_AcropolisPlazaSceneWork*)task->work);
            if (q->movieAtEnd != 0) {
                task->state = 6;
                break;
            }
            if ((work->fwd == ACROPOLIS_PLAZA_EDGE_NONE && q->reverseSceneFrames == 0) ||
                (work->back == ACROPOLIS_PLAZA_EDGE_NONE && q->reverseSceneFrames == 1) ||
                work->fwd != work->prevFwd || work->back != work->prevBack) {
                q->continueMovie = 0;
                task->state      = task->state + 1;
            }
            break;
        case 5:
            plaza_updateEdgeFlags((_AcropolisPlazaSceneWork*)task->work);
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            task->state = 2;
            goto L_case2;
        case 6:
            plaza_updateEdgeFlags((_AcropolisPlazaSceneWork*)task->work);
            switch (q->plazaStreamSubId) {
                case 0:
                case 2:
                    if (work->back != ACROPOLIS_PLAZA_EDGE_NONE) {
                        q->movieAtEnd = 0;
                        task->state   = 2;
                    }
                    break;
                case 1:
                case 3:
                    if (work->fwd != ACROPOLIS_PLAZA_EDGE_NONE) {
                        q->movieAtEnd = 0;
                        task->state   = 2;
                    }
                    break;
            }
            break;
    }

    if (q->movieReady != 0) {
        func_acropolis_plaza_8017DD90(task);
        func_acropolis_plaza_8017DE24(q->plazaStreamSubId);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Five-state warp sequence. State 0 allocates the work block, caches the
/// slot-3 task in it and places the player at (0x3804, 0, 0xFC8) with msg
/// 0x3F2; states 1 and 2 wait for slot 3 to go idle (msg 0x3F0), state 1
/// following up with the 0xD55 warp (msg 0x3EE). State 3 waits for the stream
/// to finish, latches `gCdCmdQueue.sceneFrame` into the sequence work block's
/// `resumeFrame`, kills its `sceneTask` and runs `evsStartScriptWithSkip`; state 4 kills
/// this task once the session is out of its transition.
void func_acropolis_plaza_8017E7E4(Task* task)
{
    ActorTransform            place;
    ActorTransform            warp;
    CdCmdQueue*               q    = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work = (_AcropolisPlazaEventWork*)task->work;
    _AcropolisPlazaEventWork* newWork;

    switch (task->state) {
        case 0:
            newWork    = memMalloc(sizeof(*newWork), false);
            task->work = newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(newWork, 0, sizeof(*newWork));
            ((_AcropolisPlazaEventWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            place.pos.vx                                        = 0x3804;
            place.pos.vy                                        = 0;
            place.pos.vz                                        = 0xFC8;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = q->sceneFrame;
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            evsStartScriptWithSkip(D_acropolis_plaza_80182734, EVENT_SCRIPT_HUD_KEEP, D_acropolis_plaza_80182A34);
            task->state = task->state + 1;
            return;
        case 4:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

/// Queues the plaza movie `subId` of the current room to restart from its
/// first frame.
///
/// The command's argument is the stream slot followed by a big-endian frame
/// offset to seek to; the queue copies it before this returns.
static inline void _acropolisPlazaRestartStream(u8 subId)
{
    u8 streamAt[4];

    streamAt[0] = streamFindMovieSlot(&gGameSession->location.loc, subId, 0);
    streamAt[1] = 0;
    streamAt[2] = 0;
    cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamAt);
}

/// Queues the plaza movie `subId` of the current room to play from its first
/// frame.
///
/// The argument has the layout `_acropolisPlazaRestartStream` describes, and
/// is likewise copied by the queue.
static inline void _acropolisPlazaPlayStream(u8 subId)
{
    u8 streamAt[4];

    streamAt[0] = streamFindMovieSlot(&gGameSession->location.loc, subId, 0);
    streamAt[1] = 0;
    streamAt[2] = 0;
    cdCmdEnqueue(CD_COMMAND_PLAY_STREAM_AT_OFFSET, 0, streamAt);
}

/// Finds the live enemy spawned from the current area's placement of resource
/// entry `entryId`.
///
/// The placements searched are those of the session's stage, area and layout
/// variant. An enemy is identified by its position in that table, so the
/// result is NULL when the placement was never spawned or its enemy is gone.
/// An `entryId` the table does not hold selects the position one past its last
/// placement.
static inline Enemy* _acropolisPlazaFindPlacedEnemy(u8 entryId)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    AreaPlacement*   placement;
    s32              index;

    sessionKey  = &gGameSession->location.loc;
    key.stage   = sessionKey->stage;
    key.area    = sessionKey->area;
    key.room    = gGameSession->spriteVariant;
    key.view    = gGameSession->location.loc.view;
    key.variant = sessionKey->variant;
    placement   = Gp_GetNestedAreaRec(&key)->placements;
    index       = 0;
    // Count the table entries ahead of the one sought. Spelled with `goto`:
    // the `while`, `do`/`break` and `for`/`break` forms all compile differently.
    if (placement->entryId != AREA_PLACEMENT_END) {
        for (;;) {
            if (placement->entryId == entryId) {
                goto found;
            }
            placement++;
            index++;
            if (placement->entryId == AREA_PLACEMENT_END) {
                goto found;
            }
        }
    }
found:
    return sceneFindEnemyByPlaceKey((index << ENEMY_PLACE_INDEX_SHIFT) | (sessionKey->stage << ENEMY_PLACE_STAGE_SHIFT) | sessionKey->area);
}

/// Plays `animationId` from the player's bank for the equipped weapon, off the
/// collision grid.
///
/// `blend` is an `ANIMATION_BLEND_*` choice and `blendFrames` the length of the
/// transition in frames. The request is consumed by the dispatch.
static inline void _acropolisPlazaPlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{
    AnimationPlayRequest request;
    s32                  weapon;

    // Each character has a bank per weapon slot: the primary's start at 1, the alternate's at 0x22.
    weapon                       = gPlayerStatus.weapon;
    request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weapon + 1 : weapon + 0x22;
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Re-places the player where its model's root transform currently stands,
/// upright and facing `yaw` (4096 units per turn).
///
/// `task` is the event task whose `_AcropolisPlazaEventWork` holds the
/// player. The transform is consumed by the dispatch.
static inline void _acropolisPlazaPlacePlayerAtModelRoot(Task* task, s32 yaw)
{
    ActorTransform place;
    GfxCoord*      root;

    root         = ((_AcropolisPlazaEventWork*)task->work)->playerTask->extra.tmd->coords;
    place.pos.vx = root->coord.t[0];
    place.pos.vy = root->coord.t[1];
    place.pos.vz = root->coord.t[2];
    place.rot.vz = 0;
    place.rot.vx = 0;
    place.rot.vy = yaw;
    TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &place, 0);
}

/// Seven-state opening sequence for the plaza's streamed scene. State 0 allocates
/// the work block, caches the slot-3 task in it and places the player at
/// (0xF6E, 0, 0x2328) with msg 0x3F2; states 1 and 2 wait for slot 3 to go idle
/// (msg 0x3F0), following up with the 0xD55 warp (msg 0x3EE) and then the
/// `D_actor_310100_801797FC` script (msg 0x3F4). State 3 waits for the CD queue, latches
/// `gCdCmdQueue.sceneFrame` into the sequence work block's `resumeFrame`, kills
/// its `sceneTask` and starts the scene's stream (`cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, ...)`); state 4
/// waits for the stream to report in and runs `D_acropolis_plaza_80182B24`.
/// State 5 waits out 0x60 frames, republishes the player's weapon to slot 3
/// (msg 0x3E8) and warps the player onto the slot-3 model's own coordinate
/// frame with a 0x3E9 placement; state 6 releases slot 3 (msg 0x3F1) and asks
/// to be killed. States 5 and 6 also step the room's per-frame work
/// (`func_acropolis_plaza_8017DE24(4)`), which the earlier states skip.
void func_acropolis_plaza_8017E9A8(Task* task)
{
    ActorTransform            place;
    ActorTransform            warp;
    AnimationPlayRequest      script;
    CdCmdQueue*               q    = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work = (_AcropolisPlazaEventWork*)task->work;
    _AcropolisPlazaEventWork* newWork;

    switch (task->state) {
        case 0:
            newWork    = memMalloc(sizeof(*newWork), false);
            task->work = newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(newWork, 0, sizeof(*newWork));
            ((_AcropolisPlazaEventWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            place.pos.vx                                        = 0xF6E;
            place.pos.vy                                        = 0;
            place.pos.vz                                        = 0x2328;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            script.source.sets          = D_actor_310100_801797FC;
            script.animationId          = 0xB;
            script.blend                = ANIMATION_BLEND_RESET;
            script.blendFrames          = 0;
            script.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &script, 0);
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = q->sceneFrame;
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            q->sceneFrame       = 1;
            q->movieFrame       = 1;
            q->plazaStreamSubId = 2;
            _acropolisPlazaRestartStream(2);
            q->continueMovie = 1;
            task->state      = task->state + 1;
            return;
        case 4:
            if (q->movieReady == 0) {
                return;
            }
            evsStartScript(D_acropolis_plaza_80182B24, EVENT_SCRIPT_HUD_KEEP);
            task->state = task->state + 1;
            return;
        case 5:
            if (q->movieFrame >= 0x60) {
                _acropolisPlazaPlayPlayerAnimation(1, ANIMATION_BLEND_RESET, 10);
                _acropolisPlazaPlacePlayerAtModelRoot(task, 0xEAA);
                task->state = task->state + 1;
            }
            break;
        case 6:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskRequestKill(task, 0);
            }
            break;
        default:
            return;
    }
    func_acropolis_plaza_8017DE24(4);
}

/// Sixteen-state opening sequence for the plaza's long streamed scene, and the
/// counterpart to `func_acropolis_plaza_8017E9A8` for the rest of it. States 0
/// to 2 allocate the work block, cache the slot-3 task in it, place the player
/// at (0x3DE, 0, 0x33FE) with msg 0x3F2 and warp them with a 0x1000 heading
/// (msg 0x3EE), waiting on msg 0x3F0 in between. State 3 kills the sequence
/// work block's `sceneTask` and starts stream slot 4; state 4 runs
/// `D_acropolis_plaza_80182C90` / `..._80182F18` once the CD queue reports in.
/// State 5 waits out the session transition and starts stream slot 5, unless
/// `GameSession::evtSkipped` says to skip the scene, in which case it blanks the
/// display and jumps straight to state 8.
///
/// States 6 and 8 both address the enemy placed from resource entry 0x6C
/// (`_acropolisPlazaFindPlacedEnemy`).
/// State 6 releases slot 3 (msg 0x3F1), re-places the player at
/// (0x3DE, 0, 0x439E) and has that enemy play an animation; state 8 sends it 0x7D7
/// and rebuilds the graphics state (`gpuResetAndInvalidateModelBuffers`, the aux heap from
/// `GameSession::location.loc.stage` / `location.loc.area`, `tmdResetAuxHeapAndRestoreBuffers`). State 7
/// waits 0x3D frames, playing 0x51050003 at frame 0x1E and spawning table entry
/// 7 at the end.
///
/// States 9 to 12 restart stream slot 3, run `D_acropolis_plaza_801830DC`,
/// spawn table entry 8 after 0xB frames and re-enable the display. State 13 is
/// the exit: a Start press (`padIsStartPressed`) skips to state 15, otherwise
/// it requests the map's own MIDI and starts the closing stream, state 14 runs
/// `D_acropolis_plaza_801834B4`, and state 15 releases slot 3 and kills the
/// task. Every state from 7 on also steps the room's per-frame work.
void func_acropolis_plaza_8017ECF8(Task* task)
{
    ActorTransform            place;
    ActorTransform            warp;
    u8                        slot[4];
    ActorTransform            placeBack;
    AnimationPlayRequest      roomRec;
    CdCmdQueue*               q    = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work = (_AcropolisPlazaEventWork*)task->work;
    _AcropolisPlazaEventWork* newWork;

    switch (task->state) {
        case 0:
            newWork    = memMalloc(sizeof(*newWork), false);
            task->work = newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(newWork, 0, sizeof(*newWork));
            ((_AcropolisPlazaEventWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            place.pos.vx                                        = 0x3DE;
            place.pos.vy                                        = 0;
            place.pos.vz                                        = 0x33FE;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0x1000;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            q->sceneFrame       = 1;
            q->movieFrame       = 1;
            q->plazaStreamSubId = 4;
            slot[0]             = streamFindMovieSlot(&gGameSession->location.loc, 4, 0);
            slot[1]             = 0;
            slot[2]             = 0;
            cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, slot);
            q->continueMovie = 1;
            task->state      = task->state + 1;
            return;
        case 4:
            if (cdCmdIsIdle() != 0) {
                evsStartScriptWithSkip(D_acropolis_plaza_80182C90, EVENT_SCRIPT_HUD_KEEP, D_acropolis_plaza_80182F18);
                task->state = task->state + 1;
                return;
            }
            func_acropolis_plaza_8017DE24(6);
            return;
        case 5:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (gGameSession->evtSkipped != 0) {
                SetDispMask(0);
                task->state = 8;
                return;
            }
            q->sceneFrame       = 1;
            q->movieFrame       = 1;
            q->plazaStreamSubId = 5;
            slot[0]             = streamFindMovieSlot(&gGameSession->location.loc, 5, 0);
            slot[1]             = 0;
            slot[2]             = 0;
            cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, slot);
            q->continueMovie = 1;
            task->state      = task->state + 1;
            return;
        case 6:
            if (q->movieReady == 0) {
                return;
            }
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            placeBack.pos.vx = 0x3DE;
            placeBack.pos.vy = 0;
            placeBack.pos.vz = 0x439E;
            TASK_MESSAGE_DISPATCH_POINTER(
                ((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &placeBack, 0);
            roomRec.source.index         = 1;
            roomRec.animationId          = 8;
            roomRec.blend                = ANIMATION_BLEND_RESET;
            roomRec.blendFrames          = 0xA;
            roomRec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(_acropolisPlazaFindPlacedEnemy(0x6C)->task, ACTOR_MESSAGE_PLAY_ANIMATION, &roomRec, 0);
            task->state         = task->state + 1;
            work->elapsedFrames = 0;
            return;
        case 7:
            if (work->elapsedFrames == 0x1E) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 3), 0, 0);
            }
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 0x3D) {
                taskSpawnFromTable(D_acropolis_plaza_80183824, 7, 9, 0);
                task->state = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(7);
            return;
        case 8:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(_acropolisPlazaFindPlacedEnemy(0x6C)->task, 0x7D7, 1, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                gpuResetAndInvalidateModelBuffers();
                memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
                memSelectAuxHeapRegion(true);
                tmdResetAuxHeapAndRestoreBuffers();
                SndEvt_EnqueueTypeB(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 5), 0x26);
                task->state = task->state + 1;
                return;
            }
            func_acropolis_plaza_8017DE24(7);
            return;
        case 9:
            q->sceneFrame       = 1;
            q->movieFrame       = 1;
            q->plazaStreamSubId = 3;
            slot[0]             = streamFindMovieSlot(&gGameSession->location.loc, 3, 0);
            slot[1]             = 0;
            slot[2]             = 0;
            cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, slot);
            q->continueMovie = 0;
            task->state      = task->state + 1;
            /* fallthrough */
        case 10:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            evsStartScript(D_acropolis_plaza_801830DC, EVENT_SCRIPT_HUD_KEEP);
            work->elapsedFrames = 0;
            task->state         = task->state + 1;
            return;
        case 11:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 0xB) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 0x0B), 0, 0);
                taskSpawnFromTable(D_acropolis_plaza_80183824, 8, 8, 0);
                work->elapsedFrames = 0;
                task->state         = task->state + 1;
            }
            return;
        case 12:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 2) {
                SetDispMask(1);
                task->state = task->state + 1;
            }
            return;
        case 13:
            if (padIsStartPressed() != 0) {
                Stage_RequestMidiFromMap(0xA);
                cdCmdRequestCancel();
                task->state = 0xF;
            } else if (gGameSession->eventState == 0) {
                Stage_RequestMidiFromMap(0x1E0);
                q->sceneFrame       = 1;
                q->movieFrame       = 1;
                q->plazaStreamSubId = 3;
                _acropolisPlazaPlayStream(3);
                q->continueMovie = 1;
                task->state      = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        case 14:
            if (q->movieReady != 0) {
                evsStartScript(D_acropolis_plaza_801834B4, EVENT_SCRIPT_HUD_KEEP);
                task->state = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        case 15:
            if (cdCmdIsIdle() != 0) {
                sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 2), 0xB4);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
                taskRequestKill(task, 0);
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        default:
            return;
    }
}

/// Three-state cutscene tail: state 0 republishes the player's weapon to slot
/// 3 (msg 0x3E8), state 1 waits for the streamed scene to finish -- latching
/// `gCdCmdQueue.sceneFrame` into the sequence work block's `resumeFrame`, killing
/// its `sceneTask` and running the block `spawnArg1` names -- and state 2 kills
/// this task once the session is out of its transition.
void func_acropolis_plaza_8017F48C(Task* task)
{
    AnimationPlayRequest rec;
    CdCmdQueue*          q = &gCdCmdQueue;
    s32                  state;
    s32                  weaponId;
    s32                  id;

    state = task->state;
    switch (state) {
        case 0:
            weaponId                 = gPlayerStatus.weapon;
            id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.source.index         = id;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0xA;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (cdCmdIsIdle() != 0) {
                ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = q->sceneFrame;
                taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
                switch (task->spawnArg1.value) {
                    case 0:
                        evsStartScript(D_acropolis_plaza_80183554, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 1:
                        evsStartScript(D_acropolis_plaza_8018365C, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 2:
                        evsStartScript(D_acropolis_plaza_80183764, EVENT_SCRIPT_HUD_KEEP);
                        break;
                }
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// Three-state cutscene tail: state 0 republishes the player's weapon to slot
/// 3 (msg 0x3E8), state 1 waits for the streamed scene to finish and hands
/// control back -- latching `gCdCmdQueue.sceneFrame` into the sequence work
/// block's `resumeFrame`, killing its `sceneTask` and running the CAP command
/// its `eventKind` names -- and state 2 releases
/// slot 3 (msg 0x3F1) and kills itself.
void func_acropolis_plaza_8017F620(Task* task)
{
    AnimationPlayRequest rec;
    CdCmdQueue*          q = &gCdCmdQueue;
    s32                  weaponId;
    s32                  id;

    switch (task->state) {
        case 0:
            weaponId                 = gPlayerStatus.weapon;
            id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.source.index         = id;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0xA;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (cdCmdIsIdle() != 0) {
                ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = q->sceneFrame;
                taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
                Gp_RunCapCmd1((s8)((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->eventKind);
                task->state = task->state + 1;
            }
            break;
        case 2:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            taskRequestKill(task, 0);
            break;
    }
}

/// Start an idle voice silently and finish this tick; its volume envelope
/// begins on the next call. Already-active voices continue to the update.
#define _acropolisPlazaStartAmbience(state, sndId)      \
    do {                                                \
        if (*(state) == 0) {                            \
            sndEvtRequestScriptStart((sndId), 0, 0x7F); \
            SndEvt_EnqueueTypeB((sndId), 0);            \
            *(state) = 1;                               \
            return;                                     \
        }                                               \
    } while (0)

/// Ambience voice driver: starts the voice named by `sndId` the first time
/// `state` is clear, then tracks `gCdCmdQueue.sceneFrame` between `fadeIn` and
/// `fadeOut` to ramp its volume.
static void func_acropolis_plaza_8017F770(u16 fadeIn, u16 fadeOut, u16 hold, u16* state, s32 sndId, u16 mode)
{
    CdCmdQueue* q = &gCdCmdQueue;
    u32         pos;
    u8          vol;

    pos = q->sceneFrame;
    if (pos >= fadeIn && pos <= fadeOut) {
        _acropolisPlazaStartAmbience(state, sndId);
        if (mode == 0) {
            if (pos < hold) {
                vol = ((q->sceneFrame - fadeIn) * 0x7F) / (hold - fadeIn);
            } else {
                vol = ((fadeOut - q->sceneFrame) * 0x7F) / (fadeOut - hold);
            }
        } else if (mode == 1) {
            if (pos < hold) {
                vol = ((q->sceneFrame - fadeIn) * 0x17D) / ((hold - fadeIn) * 4);
            } else {
                vol = ((fadeOut - q->sceneFrame) * 0x17D) / ((fadeOut - hold) * 4);
            }
        } else if (pos < 0x54U) {
            vol = (((0x54 - q->sceneFrame) * 0x7F) / 332) + 0x5F;
        } else {
            vol = ((0x82 - q->sceneFrame) * 0x17D) / 184;
        }
        SndEvt_EnqueueTypeB(sndId, vol);
        return;
    }
    if (*state != 0) {
        sndEvtRequestScriptStop(sndId, SOUND_SCRIPT_STOP_NO_FADE);
        *state = 0;
    }
}

#undef _acropolisPlazaStartAmbience

/// Ambience driver for the plaza's streamed scene, stepped by
/// `gCdCmdQueue.plazaStreamSubId`. While the stream is at 0/1 it keeps the four
/// looping voices alive (`func_acropolis_plaza_8017F770` starts a voice the
/// first time its slot flag is clear and ramps it afterwards); at 2 it fades
/// the crowd loop out against the stream frame counter, holding full volume
/// (0x7F) over frames 0x1F..0x54 and sliding down over 127/120ths of the
/// distance to the nearer end outside that window.
static void func_acropolis_plaza_8017F9EC(Task* task)
{
    CdCmdQueue*                  q     = &gCdCmdQueue;
    volatile u16*                frame = &gCdCmdQueue.sceneFrame;
    _AcropolisPlazaSequenceWork* work  = (_AcropolisPlazaSequenceWork*)task->work;
    s32                          pos;
    s32                          vol;

    switch (gCdCmdQueue.plazaStreamSubId) {
        case 0:
        case 1:
            func_acropolis_plaza_8017F770(1, 0x320, 1, &work->ambience5Playing, 0x51050005, 0);
            func_acropolis_plaza_8017F770(1, 0x82, 1, &work->ambience2Playing, 0x51050002, 2);
            func_acropolis_plaza_8017F770(0xF, 0x8E, 0x50, &work->ambience4Playing, 0x51050004, 0);
            func_acropolis_plaza_8017F770(0xC8, 0x172, 0x140, &work->ambience1Playing, 0x51050001, 1);
            break;
        case 2:
            pos = q->sceneFrame;
            if ((u32)(pos - 0x1F) < 0x36U) {
                vol = 0x7F;
            } else if (pos < 0x1EU) {
                vol = ((*frame * 0x7F) / 120) + 0x5F;
            } else {
                vol = (((0x73 - *frame) * 0x7F) / 120) + 0x5F;
            }
            SndEvt_EnqueueTypeB(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 1), vol & 0xFF);
            break;
    }
}

/// Steps the plaza's streamed scene, returning zero while it is still running.
///
/// Seven steps driven by the pending `WorldCollisionTrigger` event `worldCollisionReadActionHit`
/// reports. `ready` is that event's "take it" flag, qualified by `gPlayerStatus.interactionPressed`
/// so an event that arrives with the id's sign bit clear is only acted on when
/// that global is set. Steps 0 and 2 latch the event into the work block and
/// pick a table entry from its kind byte; steps 1, 3 and 4..6 wait on the task
/// the previous step spawned (`taskPollKill`) and respawn the entry-1 stream
/// watcher over `sceneArg`. Step 3 is the only exit: it unlinks the scene's
/// `WorldCollisionTrigger` and returns 1 when the latched kind is 2.
static u16 func_acropolis_plaza_8017FB50(Task* task)
{
    CdCmdQueue*                  q    = &gCdCmdQueue;
    _AcropolisPlazaSequenceWork* work = (_AcropolisPlazaSequenceWork*)task->work;
    u16                          evtId;
    u8                           evtKind;
    u8                           evtSub;
    s32                          killed0;
    s32                          killed1;
    s32                          killed2;
    s16                          ready;
    u16                          step;
    s32                          kind;
    u32                          latchedKind;
    u16                          latchedKind16;

    ready = worldCollisionReadActionHit(&evtId, &evtKind, &evtSub);
    if (!((s16)evtId & WORLD_COLLISION_TRIGGER_AUTOMATIC) && (ready != 0)) {
        ready = gPlayerStatus.interactionPressed != 0;
    }

    switch (work->step) {
        case ACROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE:
            if (ready != 0) {
                work->eventControl    = evtId;
                work->eventKind       = evtKind;
                work->eventParameter1 = evtSub;
                if ((s8)evtKind == ACROPOLIS_PLAZA_EVENT_FIRST_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(D_acropolis_plaza_80183824, 4, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if ((s8)evtKind >= ACROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(D_acropolis_plaza_80183824, 9, 0, work);
                    work->step = ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION;
                }
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_FIRST_SCENE:
            if (taskPollKill(work->eventTask, &killed0) != 0) {
                work->sceneArg.skipStreamReset = 1;
                work->sceneArg.startFrame      = work->resumeFrame;
                work->sceneTask =
                    taskSpawnFromTable(D_acropolis_plaza_80183824, 1, 0, &work->sceneArg);
                worldCollisionUnlinkTrigger(0, &D_acropolis_plaza_801991F0);
                work->step = work->step + 1;
            }
            break;
        case ACROPOLIS_PLAZA_STEP_AWAIT_EVENT:
            if (ready != 0) {
                work->eventControl    = evtId;
                work->eventKind       = evtKind;
                work->eventParameter1 = evtSub;
                work->resumeFrame     = q->sceneFrame;
                kind                  = (s8)evtKind;
                if (kind == ACROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(D_acropolis_plaza_80183824, 2, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (kind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(D_acropolis_plaza_80183824, 3, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (kind == ACROPOLIS_PLAZA_EVENT_REPEAT_SCENE) {
                    if (work->repeatVariant == 0) {
                        work->eventTask =
                            taskSpawnFromTable(D_acropolis_plaza_80183824, 6, 0, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else if (work->repeatVariant == 1) {
                        work->eventTask =
                            taskSpawnFromTable(D_acropolis_plaza_80183824, 6, 1, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else {
                        work->eventTask =
                            taskSpawnFromTable(D_acropolis_plaza_80183824, 6, 2, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    }
                } else if (kind >= ACROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(D_acropolis_plaza_80183824, 9, 0, work);
                    work->step = ACROPOLIS_PLAZA_STEP_RUN_CAPTION;
                }
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_SCENE:
            if (taskPollKill(work->eventTask, &killed1) != 0) {
                /* The kind byte is tested as an unsigned short, so it is
                   sign-extended and narrowed again at each comparison; routing
                   both tests through one variable folds the pair away. */
                latchedKind   = work->eventKind;
                latchedKind16 = (s8)latchedKind;
                if (latchedKind16 == ACROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    worldCollisionUnlinkTrigger(0, &D_acropolis_plaza_801991A4);
                } else if ((u16)(s8)latchedKind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    worldCollisionUnlinkTrigger(0, D_acropolis_plaza_8019923C);
                }
                work->step = work->step - 1;
                if ((s8)work->eventKind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    return 1;
                }
                work->sceneArg.skipStreamReset = 1;
                work->sceneArg.startFrame      = work->resumeFrame;
                work->sceneTask =
                    taskSpawnFromTable(D_acropolis_plaza_80183824, 1, 0, &work->sceneArg);
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE:
        case ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION:
        case ACROPOLIS_PLAZA_STEP_RUN_CAPTION:
            if (taskPollKill(work->eventTask, &killed2) != 0) {
                work->sceneArg.skipStreamReset = 1;
                work->sceneArg.startFrame      = work->resumeFrame;
                work->sceneTask =
                    taskSpawnFromTable(D_acropolis_plaza_80183824, 1, 0, &work->sceneArg);
                step = work->step;
                if (step == ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION) {
                    work->step = ACROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE;
                } else {
                    if (step != ACROPOLIS_PLAZA_STEP_RUN_CAPTION) {
                        if (work->repeatVariant < 2) {
                            work->repeatVariant = work->repeatVariant + 1;
                        }
                    }
                    work->step = ACROPOLIS_PLAZA_STEP_AWAIT_EVENT;
                }
            }
            break;
    }
    return 0;
}

/// Draws black bars over the top and bottom 24 pixels of the cinematic view.
///
/// The task argument is unused. Each tick queues two opaque tiles in tag 3 of
/// the current frame's ordering table; the frame arena must have room for both.
static void _acropolisPlazaLetterboxTask(Task* task)
{
    enum { ACROPOLIS_PLAZA_LETTERBOX_TAG = 3 };
    TILE* tile;

    /// Queues one cinematic bar, overwriting this callback's `tile` local.
    ///
    /// `topY` is evaluated once, in screen pixels. Expands to several statements;
    /// invoke only in this braced function body. Captures its local tag constant
    /// and the current frame's packet cursor and ordering table.
#define ACROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(topY)  \
    tile           = gGpuPrimCursor;              \
    gGpuPrimCursor = tile + 1;                    \
    SetTile(tile);                                \
    tile->b0 = 0;                                 \
    tile->g0 = 0;                                 \
    tile->r0 = 0;                                 \
    tile->x0 = -ACROPOLIS_PLAZA_SCREEN_WIDTH / 2; \
    tile->y0 = (topY);                            \
    tile->w  = ACROPOLIS_PLAZA_SCREEN_WIDTH;      \
    tile->h  = ACROPOLIS_PLAZA_LETTERBOX_HEIGHT;  \
    addPrim(gGpuCurrentOt + ACROPOLIS_PLAZA_LETTERBOX_TAG, tile);

    ACROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(-ACROPOLIS_PLAZA_SCREEN_HEIGHT / 2);
    ACROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(ACROPOLIS_PLAZA_SCREEN_HEIGHT / 2 - ACROPOLIS_PLAZA_LETTERBOX_HEIGHT);
#undef ACROPOLIS_PLAZA_DRAW_LETTERBOX_BAR
}

/// Six-state opening sequence for the plaza. State 0 fades in the room
/// (`func_800E9BDC`), applies the plaza view, allocates the sequence work block
/// and spawns entries 5 and 0xB of the room's table around
/// `playerActorRemoveEquipment`; states 1 and 2 idle. State 3 pins the camera override
/// to (0x370, 0x370, 0x370), tells slot 6 to start (msg 0xFA4), spawns the
/// stream watcher (entry 1) and the entry-8 actor, and arms
/// `gCdCmdQueue.blockGamePause`. State 4 runs the ambience driver until
/// `func_acropolis_plaza_8017FB50` reports the scene is over; state 5 records
/// the next stage in the save block, disarms `blockGamePause` and hands off to the
/// stage-load task.
void func_acropolis_plaza_80180054(Task* task)
{
    CdCmdQueue*                  q    = &gCdCmdQueue;
    _AcropolisPlazaSequenceWork* work = (_AcropolisPlazaSequenceWork*)task->work;
    _AcropolisPlazaSequenceWork* newWork;
    SVECTOR                      vec;

    switch (task->state) {
        case 0:
            func_800E9BDC(3, 0x9DF);
            viewApplyCamera(D_acropolis_plaza_801838B8[0]);
            newWork    = memMalloc(sizeof(*newWork), false);
            task->work = newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(newWork, 0, sizeof(*newWork));
            ((_AcropolisPlazaSequenceWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((_AcropolisPlazaSequenceWork*)task->work)->eventTask =
                taskSpawnFromTable(D_acropolis_plaza_80183824, 5, 0, 0);
            playerActorRemoveEquipment();
            taskSpawnFromTable(D_acropolis_plaza_80183824, 0xB, 0, 0);
            task->state = task->state + 1;
            return;
        case 1:
        case 2:
            task->state = task->state + 1;
            return;
        case 3:
            vec.vx = 0x370;
            vec.vy = 0x370;
            vec.vz = 0x370;
            worldCoordSetAmbientColorOverride(&vec);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->sceneArg.skipStreamReset = 0;
            work->sceneArg.startFrame      = 0;
            work->sceneTask                = taskSpawnFromTable(D_acropolis_plaza_80183824, 1, 0, &work->sceneArg);
            Stage_RequestFromAreaTable(0);
            taskSpawnFromTable(D_acropolis_plaza_80183824, 8, 6, 0);
            q->blockGamePause = 1;
            task->state       = task->state + 1;
            return;
        case 4:
            func_acropolis_plaza_8017F9EC(task);
            if (func_acropolis_plaza_8017FB50(task) == 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 5:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            Gp_EnqueueHeldWeaponCd();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            taskSpawn(0, 0x11, 0, 0);
            q->blockGamePause = 0;
            taskKill(task);
            return;
    }
}

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_plaza_80183824, 0xA, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void acropolisPlazaSirenLightTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW             = 0x200,
        ACROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT        = 5,
        ACROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS           = 0x600,
        ACROPOLIS_PLAZA_LIGHT_REFRESH_FRAMES               = 2,
        ACROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT            = 4,
        ACROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH              = 0x200,
        ACROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH            = 0x400,
        ACROPOLIS_PLAZA_SIREN_FRINGE_Z                     = 0x400,
        ACROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z                = 0x100,
        ACROPOLIS_PLAZA_SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR = 0xC000
    };
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    GfxCoord*                      coord;
    GfxCoord*                      lightCoord;
    EffectWork*                    work;
    _AcropolisPlazaBeamScratch*    beam;
    POLY_G3*                       beamTriangle;
    POLY_G4*                       primitive;
    s32                            index;
    u32                            brightness;
    u16                            red, green, blue;
    s32                            slot, packedBlueBrightness;
    u32                            packedRedBrightness;
    s16                            falloffWidth, beamLength;
    u16                            yaw;

    slot       = task->spawnArg1.value;
    lightSlot  = &gWorldCoordTransientPointLights[slot & (ARRAY_SIZE(gWorldCoordTransientPointLights) - 1)];
    light      = &lightSlot->light;
    coord      = task->extra.coordBody->coord;
    work       = task->spawnArg2.pointer;
    lightCoord = &light->head.transform.coord;
    if (task->state == 0) {
        work->scale = (slot & 1) << ACROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT;
        task->state = task->state + 1;
    }
    // Compose the sweeping beacon before projecting its origin and outline.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    beam                                                  = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisPlazaBeamScratch);
    beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vx = coord->workm.t[0];
    beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vy = coord->workm.t[1];
    beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
    gte_rtps();
    gte_stsxy(&beam->centreScreenPos);
    gte_stszotz(&beam->otz);
    // A culled origin leaves this slot inactive for the frame.
    lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
    if (beam->otz >= ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(beam->centreScreenPos.vx) < ACROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(beam->centreScreenPos.vy) < ACROPOLIS_PLAZA_LIGHT_CULL_Y) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            brightness      = ((gRandomLcgState >> 16) & ACROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | ACROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
            if (task->spawnArg1.value < ACROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT) {
                work->angle = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) > ACROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
                red         = brightness >> 1;
                green       = brightness >> 1;
                blue        = brightness;
            } else {
                work->angle = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) < ACROPOLIS_PLAZA_LIGHT_HALF_TURN - ACROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
                red         = brightness;
                green       = red >> 1;
                blue        = red >> 1;
            }
            beamLength = work->angle;
            // Refresh the point light; only a long beam adds yaw-dependent falloff.
            falloffWidth                                              = beamLength != ACROPOLIS_PLAZA_LIGHT_LONG_REACH ? 0 : (yaw = work->scale, work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN >= 0 ? (yaw - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) * 4 : (beamLength - yaw) * 4);
            work->period                                              = falloffWidth;
            lightCoord->coord.t[0]                                    = coord->coord.t[0];
            lightCoord->coord.t[1]                                    = coord->coord.t[1];
            lightCoord->coord.t[2]                                    = coord->coord.t[2];
            lightCoord->composeStamp                                  = GRAPHICS_COORD_DIRTY;
            lightSlot->framesLeft                                     = ACROPOLIS_PLAZA_LIGHT_REFRESH_FRAMES;
            light->inner                                              = ACROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS;
            light->outer                                              = work->period + ACROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS;
            light->head.color.r                                       = red << ACROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            light->head.color.g                                       = green << ACROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            light->head.color.b                                       = blue << ACROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vx   = -ACROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vy   = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vz   = work->angle;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vx  = ACROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vy  = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vz  = work->angle;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vx  = -ACROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vy  = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vz  = ACROPOLIS_PLAZA_SIREN_FRINGE_Z;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vx = ACROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vy = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vz = ACROPOLIS_PLAZA_SIREN_FRINGE_Z;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vx  = -ACROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vy  = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vz  = ACROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vx = ACROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vy = 0;
            beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vz = ACROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z;
            for (index = ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT; index < ACROPOLIS_PLAZA_BEAM_VERTEX_COUNT; index++) {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&beam->vertices[index]);
                gte_rtv0();
                gte_stsv(&beam->vertices[index]);
                // Move the rotated outline vertex into world coordinates.
                ACROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&beam->vertices[index], coord);
            }
            gte_SetRotMatrix(&GsWSMATRIX);
            for (index = ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT; index < ACROPOLIS_PLAZA_BEAM_VERTEX_COUNT; index++) {
                gte_ldv0(&beam->vertices[index]);
                gte_rtps();
                gte_stsxy(&beam->vertexScreenPos[index]);
            }
            // Draw the beam triangle and the fringe on each side.
            index          = 0;
            red            = (s32)(red << 16) >> 18;
            green          = (s32)(green << 16) >> 18;
            blue           = (s32)(blue << 16) >> 18;
            beamTriangle   = gGpuPrimCursor;
            gGpuPrimCursor = beamTriangle + 1;
            setPolyG3(beamTriangle);
            setRGB0(beamTriangle, (s16)red * 3, (s16)green * 3, (s16)blue * 3);
            setRGB1(beamTriangle, 0, 0, 0);
            setRGB2(beamTriangle, 0, 0, 0);
            beamTriangle->x0 = beam->centreScreenPos.vx;
            beamTriangle->y0 = beam->centreScreenPos.vy;
            beamTriangle->x1 = (u16)beam->vertexScreenPos[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT];
            beamTriangle->y1 = (beam->vertexScreenPos[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT] >> 16);
            beamTriangle->x2 = (u16)beam->vertexScreenPos[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT];
            beamTriangle->y2 = (beam->vertexScreenPos[ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT] >> 16);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)beam->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), beamTriangle);
            gpuSetPrimitiveBlendMode(beamTriangle, GPU_BLEND_ADD, beam->otz);
            for (; index < 2; index++) {
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, (s16)red * 2, (s16)green * 2, (s16)blue * 2);
                setRGB2(primitive, 0, 0, 0);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = (u16)beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT];
                primitive->y0 = (beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT] >> 16);
                primitive->x1 = beam->centreScreenPos.vx;
                primitive->y1 = beam->centreScreenPos.vy;
                primitive->x2 = (u16)beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT];
                primitive->y2 = (beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT] >> 16);
                primitive->x3 = (u16)beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT];
                primitive->y3 = (beam->vertexScreenPos[index + ACROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT] >> 16);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)beam->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, beam->otz);
            }
            beam->glowRadius = ACROPOLIS_PLAZA_SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR / beam->otz;
            red            <<= 1;
            green          <<= 1;
            blue           <<= 1;
            ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(beam->centreScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
        }
    }
    // The far glow has its own flicker and visibility test, even if the origin was culled.
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    brightness      = ((gRandomLcgState >> 16) & ACROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | ACROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < ACROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT) {
        work->angle          = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) > ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = (u32)packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) < ACROPOLIS_PLAZA_LIGHT_HALF_TURN - ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (u32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == ACROPOLIS_PLAZA_LIGHT_LONG_REACH) {
        beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vx = 0;
        beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vy = 0;
        beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vz = ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        gte_rtv0();
        gte_stsv(&beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        ACROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE], coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&beam->vertices[ACROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        gte_rtps();
        gte_stsxy(&beam->centreScreenPos);
        gte_stszotz(&beam->otz);
        if (beam->otz >= ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
            if (__builtin_abs(beam->centreScreenPos.vx) < ACROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(beam->centreScreenPos.vy) < ACROPOLIS_PLAZA_LIGHT_CULL_Y) {
                beam->glowRadius = ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR / beam->otz;
                ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(beam->centreScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - ACROPOLIS_PLAZA_LIGHT_YAW_STEP) & ACROPOLIS_PLAZA_LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisPlazaBeamScratch);
}

void acropolisPlazaLightFlareTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT         = 9,
        ACROPOLIS_PLAZA_FLARE_PULSE_STEP                    = 8,
        ACROPOLIS_PLAZA_FLARE_SLOT_PHASE_STEP               = 0xC0,
        ACROPOLIS_PLAZA_FLARE_RISING_LEVEL_MASK             = 0x78,
        ACROPOLIS_PLAZA_FLARE_ORIGIN_RADIUS_NUMERATOR       = 0xA000,
        ACROPOLIS_PLAZA_FLARE_RAY_TIP_RADIUS_NUMERATOR      = 0x8000,
        ACROPOLIS_PLAZA_FLARE_RAY_SHOULDER_RADIUS_NUMERATOR = 0x1800,
        ACROPOLIS_PLAZA_FLARE_FIRST_RAY_SAMPLE              = 3,
        ACROPOLIS_PLAZA_FLARE_OPPOSITE_RAY_SAMPLE_STEP      = 8
    };
    GfxCoord*                    coord;
    EffectWork*                  work;
    _AcropolisPlazaFlareScratch* flare;
    POLY_G4*                     primitive;
    s32                          index;
    s16                          brightness;
    u16                          red, green, blue;
    s32                          initialYaw;
    s32                          pulsePhase, packedBlueBrightness;
    u32                          packedRedBrightness;
    s16                          pulseLevel;
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    if (task->state == 0) {
        initialYaw  = (task->spawnArg1.value & 1) << ACROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT;
        work->scale = initialYaw;
        task->state = task->state + 1;
    }
    // Sweep the placement frame; the origin glow remains at its world translation.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    flare              = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisPlazaFlareScratch);
    flare->worldPos.vx = coord->workm.t[0];
    flare->worldPos.vy = coord->workm.t[1];
    flare->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&flare->worldPos);
    gte_rtps();
    gte_stsxy(&flare->screenPos);
    gte_stszotz(&flare->otz);
    if (flare->otz >= ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(flare->screenPos.vx) < ACROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < ACROPOLIS_PLAZA_LIGHT_CULL_Y) {
            pulsePhase = gDisplayState.animFrame * ACROPOLIS_PLAZA_FLARE_PULSE_STEP + task->spawnArg1.value * ACROPOLIS_PLAZA_FLARE_SLOT_PHASE_STEP;
            if (pulsePhase & ACROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD) {
                pulseLevel = ACROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK - (pulsePhase & ACROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK);
            } else {
                pulseLevel = pulsePhase & ACROPOLIS_PLAZA_FLARE_RISING_LEVEL_MASK;
            }
            brightness = pulseLevel;
            if (task->spawnArg1.value < ACROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT) {
                red   = brightness >> 2;
                green = brightness >> 2;
                blue  = brightness;
            } else {
                red   = brightness;
                green = (s16)red >> 2;
                blue  = (s16)red >> 2;
            }
            flare->outerRadius = ACROPOLIS_PLAZA_FLARE_ORIGIN_RADIUS_NUMERATOR / flare->otz;
            ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            // Overlay four diagonal rays, alternating double and single radii.
            flare->outerRadius = ACROPOLIS_PLAZA_FLARE_RAY_TIP_RADIUS_NUMERATOR / flare->otz;
            flare->innerRadius = ACROPOLIS_PLAZA_FLARE_RAY_SHOULDER_RADIUS_NUMERATOR / flare->otz;
            red              <<= 1;
            green            <<= 1;
            blue             <<= 1;
            for (index = ACROPOLIS_PLAZA_FLARE_FIRST_RAY_SAMPLE; index < ACROPOLIS_PLAZA_LIGHT_FAN_SAMPLES; index += ACROPOLIS_PLAZA_FLARE_OPPOSITE_RAY_SAMPLE_STEP) {
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * D_acropolis_plaza_801987E0[index]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 12]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * D_acropolis_plaza_801987E0[index + 4]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * D_acropolis_plaza_801987E0[index]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 8]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 4]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)flare->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * D_acropolis_plaza_801987E0[index]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * D_acropolis_plaza_801987E0[index + 8]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * D_acropolis_plaza_801987E0[index + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 12]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * D_acropolis_plaza_801987E0[index + 8]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)flare->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
            }
        }
    }
    // Gate an independently flickering glow ahead of the flare by the sweep yaw.
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    brightness      = ((gRandomLcgState >> 16) & ACROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | ACROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < ACROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT) {
        work->angle          = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) > ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - ACROPOLIS_PLAZA_LIGHT_HALF_TURN) < ACROPOLIS_PLAZA_LIGHT_HALF_TURN - ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? ACROPOLIS_PLAZA_LIGHT_LONG_REACH : ACROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (s32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == ACROPOLIS_PLAZA_LIGHT_LONG_REACH) {
        flare->worldPos.vx = 0;
        flare->worldPos.vy = 0;
        flare->worldPos.vz = ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&flare->worldPos);
        gte_rtv0();
        gte_stsv(&flare->worldPos);
        ACROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&flare->worldPos, coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&flare->worldPos);
        gte_rtps();
        gte_stsxy(&flare->screenPos);
        gte_stszotz(&flare->otz);
        if (flare->otz >= ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
            if (__builtin_abs(flare->screenPos.vx) < ACROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < ACROPOLIS_PLAZA_LIGHT_CULL_Y) {
                flare->outerRadius = ACROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR / flare->otz;
                ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - ACROPOLIS_PLAZA_LIGHT_YAW_STEP) & ACROPOLIS_PLAZA_LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisPlazaFlareScratch);
}

#undef ACROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX

void acropolisPlazaLightGlowTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_LIGHT_RED_GLOW_SLOT_START    = 16,
        ACROPOLIS_PLAZA_GLOW_YELLOW_FLICKER_MASK     = 0x3F,
        ACROPOLIS_PLAZA_GLOW_YELLOW_RADIUS_NUMERATOR = 0xC000,
        ACROPOLIS_PLAZA_GLOW_RED_RADIUS_NUMERATOR    = 0x8000,
        ACROPOLIS_PLAZA_GLOW_RED_PULSE_STEP          = 6,
        ACROPOLIS_PLAZA_GLOW_RED_RISING_LEVEL_MASK   = 0x7E
    };
    GfxCoord*              coord;
    RoomGlowSpriteScratch* glow;
    POLY_G4*               primitive;
    s32                    fanSample, pulsePhase;
    s16                    pulseLevel;
    s32                    packedBrightness, yellowIntensity, redAccentIntensity;
    s16                    red, green, blue;

    coord = task->extra.coordBody->coord;
    // Project the placement origin; size the screen-space glow by its depth.
    actorRenderComposeCoord(coord);
    glow              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    glow->worldPos.vx = coord->workm.t[0];
    glow->worldPos.vy = coord->workm.t[1];
    glow->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&glow->worldPos);
    gte_rtps();
    gte_stsxy(&glow->screenPos);
    gte_stszotz(&glow->otz);
    if (glow->otz >= ACROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(glow->screenPos.vx) < ACROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(glow->screenPos.vy) < ACROPOLIS_PLAZA_LIGHT_CULL_Y) {
            if (task->spawnArg1.value < ACROPOLIS_PLAZA_LIGHT_RED_GLOW_SLOT_START) {
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                packedBrightness = (((gRandomLcgState >> 16) & ACROPOLIS_PLAZA_GLOW_YELLOW_FLICKER_MASK) + ACROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY) << 16;
                yellowIntensity  = packedBrightness >> 17;
                red              = yellowIntensity;
                green            = yellowIntensity;
                blue             = (u32)packedBrightness >> 18;
                glow->halfExtent = ACROPOLIS_PLAZA_GLOW_YELLOW_RADIUS_NUMERATOR / glow->otz;
            } else {
                pulsePhase = gDisplayState.animFrame * ACROPOLIS_PLAZA_GLOW_RED_PULSE_STEP;
                if (pulsePhase & ACROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD) {
                    pulseLevel = ACROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK - (pulsePhase & ACROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK);
                } else {
                    pulseLevel = pulsePhase & ACROPOLIS_PLAZA_GLOW_RED_RISING_LEVEL_MASK;
                }
                red                = pulseLevel;
                redAccentIntensity = (s32)(red << 16) >> 18;
                green              = redAccentIntensity;
                blue               = redAccentIntensity;
                glow->halfExtent   = ACROPOLIS_PLAZA_GLOW_RED_RADIUS_NUMERATOR / glow->otz;
            }
            ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(glow->screenPos, glow->halfExtent, glow->otz, primitive, fanSample, red, green, blue);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#undef ACROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN

/// Plaza ambient-effect spawner. On its first frame only, it fires three bursts
/// of `effectSpawn` against the task's own coordinate frame - seven 0x60096
/// effects on slots 0xC-0x12, four 0x60099 on slots 7-0xA and six 0x60098 on
/// slots 1-6 - each anchored at the matching entry of
/// `D_acropolis_plaza_80198820`. Every later frame is a no-op.
void func_acropolis_plaza_8018251C(Task* task)
{
    GfxCoord* coord;
    s32       i;

    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        for (i = 0xC; i < 0x13; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_LIGHT_GLOW, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        for (i = 7; i < 0xB; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_LIGHT_FLARE, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        for (i = 1; i < 7; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_SIREN_LIGHT, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        task->state = task->state + 1;
    }
}
