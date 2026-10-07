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

/// The block `_acropolisPlazaStreamSceneTask` runs once its stream reports in.
extern EvsCommand D_acropolis_plaza_80182B24[];

/// The pair of blocks `_acropolisPlazaFirstSceneTask` hands to `evsStartScriptWithSkip`
/// once the streamed scene it waits on has finished.
extern EvsCommand D_acropolis_plaza_80182734[];
extern EvsCommand D_acropolis_plaza_80182A34[];

/// The pair of blocks the opening sequence hands to `evsStartScriptWithSkip` in state 4,
/// and the two it runs on its own with `evsStartScript` in states 10 and 14.
extern EvsCommand D_acropolis_plaza_80182C90[];
extern EvsCommand D_acropolis_plaza_80182F18[];
extern EvsCommand D_acropolis_plaza_801830DC[];
extern EvsCommand D_acropolis_plaza_801834B4[];

/// The three blocks `_acropolisPlazaRepeatSceneTask` picks between with
/// `Task::spawnArg1` before handing one to `evsStartScript`.
extern EvsCommand D_acropolis_plaza_80183554[];
extern EvsCommand D_acropolis_plaza_8018365C[];
extern EvsCommand D_acropolis_plaza_80183764[];

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
static void _acropolisPlazaMovieDisplayTask(Task* task);
static void _acropolisPlazaStreamedSceneTask(Task* task);
static void _acropolisPlazaFirstSceneTask(Task* task);
static void _acropolisPlazaStreamSceneTask(Task* task);
void        func_acropolis_plaza_8017ECF8(Task*);
static void _acropolisPlazaRepeatSceneTask(Task* task);
void        func_acropolis_plaza_8017F620(Task*);
static void _acropolisPlazaLetterboxTask(Task* task);
void        func_acropolis_plaza_80180054(Task*);
static void _acropolisPlazaStartMovieDisplayTask(Task* task);

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
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaStreamedSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaStreamSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017ECF8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaFirstSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaStartMovieDisplayTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaRepeatSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaFadeToWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_plaza_8017F620, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaMovieDisplayTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPlazaLetterboxTask, { .value = 0 } },
};

s32 D_acropolis_plaza_801838B4 = 800;

ViewCamera D_acropolis_plaza_801838B8[400][2] = {
#include "assets/acropolis_plaza_path_062F8.inc"
};

ViewCamera D_acropolis_plaza_8018A938[120][2] = {
#include "assets/acropolis_plaza_path_0D378.inc"
};

s32 D_acropolis_plaza_8018CAF8 = 300;

ViewCamera D_acropolis_plaza_8018CAFC[150][2] = {
#include "assets/acropolis_plaza_path_0F53C.inc"
};

s32 D_acropolis_plaza_8018F52C = 32;

ViewCamera D_acropolis_plaza_8018F530[16][2] = {
#include "assets/acropolis_plaza_path_11F70.inc"
};

// Retained data: Retained word 100 between the camera table and script data; no reference found.
s32 D_acropolis_plaza_8018F9B0 = 100;

ViewCamera D_acropolis_plaza_8018F9B4[50][2] = {
#include "assets/acropolis_plaza_path_123F4.inc"
};

VECTOR3 D_acropolis_plaza_801907C4[400] = {
#include "assets/acropolis_plaza_path_13204.inc"
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

/// Envelope selectors and byte-volume units of the plaza ambience driver.
enum {
    ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE        = 0,
    ACROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE  = 1,
    ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE      = 2,
    ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME     = 127,
    ACROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME     = 95,
    ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE = 84,
    ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END  = 130
};

static void            _acropolisPlazaUpdateSceneCollisionWalls(Task* sceneTask);
static void            _acropolisPlazaApplyStreamCamera(s32 subId);
static __inline__ void _acropolisPlazaUpdateSceneEdgeFlags(_AcropolisPlazaSceneWork* work);
static void            _acropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope);
static void            _acropolisPlazaUpdateSceneAmbience(Task* task);
static u16             func_acropolis_plaza_8017FB50(Task* task);

void acropolisPlazaPollStreamCommands(void)
{
    enum {
        ACROPOLIS_PLAZA_STREAM_COMMAND_INIT = 0,
        ACROPOLIS_PLAZA_STREAM_COMMAND_POLL = 1,
    };
    CdCmdQueue* queue;
    CdCmdEntry* entry;
    s16         slotIndex;
    s16         sectorOffset;
    s32         command;

    queue        = &gCdCmdQueue;
    entry        = &queue->entries[queue->readIdx];
    command      = entry->cmd;
    slotIndex    = entry->args.stream.slotIndex;
    sectorOffset = entry->args.stream.sectorOffsetLow | (entry->args.stream.sectorOffsetHigh << 8);

    if (command != CD_COMMAND_EMPTY) {
        if (command >= 0) {
            if (command < CD_COMMAND_RESUME_STREAM_AT_POSITION + 1) {
                if (command >= CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                    switch (queue->step) {
                        case ACROPOLIS_PLAZA_STREAM_COMMAND_INIT:
                            switch (cdSyncPollCommand(0, 0)) {
                                case CD_SYNC_PENDING:
                                    return;
                                case CD_SYNC_RETRY:
                                    CdFlush();
                                    /* fallthrough */
                                case CD_SYNC_COMPLETE:
                                    if (queue->entries[queue->readIdx].cmd == CD_COMMAND_RESET_STREAM_AT_OFFSET) {
                                        D_8005EAEC                         = 0;
                                        D_8005EAEE                         = 0;
                                        queue->entries[queue->readIdx].cmd = CD_COMMAND_PLAY_STREAM_AT_OFFSET;
                                    }
                                    streamInitMoviePlayback(slotIndex & 0xFFFF);
                                    if (queue->entries[queue->readIdx].cmd == CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                                        streamPollMoviePlayback(0, sectorOffset);
                                    } else if (queue->entries[queue->readIdx].cmd == CD_COMMAND_RESUME_STREAM_AT_POSITION) {
                                        streamPollMoviePlayback(1, queue->activeRequest.resumeSector);
                                    }
                                    queue->step++;
                                    break;
                            }
                            /* fallthrough */
                        case ACROPOLIS_PLAZA_STREAM_COMMAND_POLL:
                            if (queue->entries[queue->readIdx].cmd == CD_COMMAND_PLAY_STREAM_AT_OFFSET) {
                                if (streamPollMoviePlayback(0, sectorOffset) != 0) {
                                    cdCmdCompleteHeadRequest();
                                }
                            } else if (queue->entries[queue->readIdx].cmd == CD_COMMAND_RESUME_STREAM_AT_POSITION) {
                                if (streamPollMoviePlayback(1, queue->activeRequest.resumeSector) != 0) {
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

/// Plays the plaza display movie, allowing Start to cancel, then restores game graphics.
///
/// The current room must have movie stream ID 100, sub-ID 0 loaded. This task
/// owns the saved VRAM/workspace interval through restoration and resumes the
/// game display loop only after the CD queue is idle.
static void _acropolisPlazaMovieDisplayTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_MOVIE_SAVE_GRAPHICS     = 0,
        ACROPOLIS_PLAZA_MOVIE_START             = 1,
        ACROPOLIS_PLAZA_MOVIE_WAIT_READY        = 2,
        ACROPOLIS_PLAZA_MOVIE_WAIT_END          = 3,
        ACROPOLIS_PLAZA_MOVIE_WAIT_CANCEL       = 4,
        ACROPOLIS_PLAZA_MOVIE_BEGIN_RESTORE     = 5,
        ACROPOLIS_PLAZA_MOVIE_WAIT_RESTORE      = 6,
        ACROPOLIS_PLAZA_DISPLAY_MOVIE_STREAM_ID = 100,
    };
    u8          streamArgs[4];
    GameLoc     movieLocation;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case ACROPOLIS_PLAZA_MOVIE_SAVE_GRAPHICS:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case ACROPOLIS_PLAZA_MOVIE_START:
            movieLocation          = gGameSession->location;
            movieLocation.loc.view = ACROPOLIS_PLAZA_DISPLAY_MOVIE_STREAM_ID;
            streamArgs[0]          = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case ACROPOLIS_PLAZA_MOVIE_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case ACROPOLIS_PLAZA_MOVIE_WAIT_END:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed() != 0) {
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case ACROPOLIS_PLAZA_MOVIE_WAIT_CANCEL:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            }
            break;
        case ACROPOLIS_PLAZA_MOVIE_BEGIN_RESTORE:
            streamResetGameRestore();
            task->state++;
            break;
        case ACROPOLIS_PLAZA_MOVIE_WAIT_RESTORE:
            if (streamPollGameRestore(1, 0) & 0xFFFF) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Moves the two opposing collision walls with the streamed scene's shot position.
///
/// The eight vertices belong to the live plaza collision grid. The walls are
/// 3000 units left and 2000 right of the shot, 1000 units high, and span
/// shot Z - 4096 through Z + 12288. Stores wrap to signed 16-bit coordinates.
static void _acropolisPlazaUpdateSceneCollisionWalls(Task* sceneTask)
{
    _AcropolisPlazaSceneWork* work  = sceneTask->work;
    s32                       shotX = work->shotPos.vx;
    s32                       shotY = work->shotPos.vy;
    s32                       shotZ = work->shotPos.vz;
    s16                       leftX = shotX - 0xBB8;
    s16                       topY;
    s16                       minZ;
    s16                       maxZ;
    s16                       rightX;

    _gAcropolisPlazaCollision1BBC0Verts[0].vx = leftX;
    _gAcropolisPlazaCollision1BBC0Verts[1].vx = leftX;
    _gAcropolisPlazaCollision1BBC0Verts[2].vx = leftX;
    _gAcropolisPlazaCollision1BBC0Verts[3].vx = leftX;

    topY   = shotY + 0x3E8;
    minZ   = shotZ - 0x1000;
    maxZ   = shotZ + 0x3000;
    rightX = shotX + 0x7D0;

    _gAcropolisPlazaCollision1BBC0Verts[0].vy = shotY;
    _gAcropolisPlazaCollision1BBC0Verts[1].vy = shotY;
    _gAcropolisPlazaCollision1BBC0Verts[2].vy = topY;
    _gAcropolisPlazaCollision1BBC0Verts[3].vy = topY;

    _gAcropolisPlazaCollision1BBC0Verts[0].vz = minZ;
    _gAcropolisPlazaCollision1BBC0Verts[1].vz = maxZ;
    _gAcropolisPlazaCollision1BBC0Verts[2].vz = minZ;
    _gAcropolisPlazaCollision1BBC0Verts[3].vz = maxZ;

    _gAcropolisPlazaCollision1BBC0Verts[4].vx = rightX;
    _gAcropolisPlazaCollision1BBC0Verts[5].vx = rightX;
    _gAcropolisPlazaCollision1BBC0Verts[6].vx = rightX;
    _gAcropolisPlazaCollision1BBC0Verts[7].vx = rightX;

    _gAcropolisPlazaCollision1BBC0Verts[4].vy = shotY;
    _gAcropolisPlazaCollision1BBC0Verts[5].vy = shotY;
    _gAcropolisPlazaCollision1BBC0Verts[6].vy = topY;
    _gAcropolisPlazaCollision1BBC0Verts[7].vy = topY;

    _gAcropolisPlazaCollision1BBC0Verts[4].vz = maxZ;
    _gAcropolisPlazaCollision1BBC0Verts[5].vz = minZ;
    _gAcropolisPlazaCollision1BBC0Verts[6].vz = maxZ;
    _gAcropolisPlazaCollision1BBC0Verts[7].vz = minZ;
}

/// Applies the camera sample for the plaza stream sub-ID in the low halfword.
///
/// Sub-IDs 0..3 use the 400-row traversal table; 4..7 select the four event
/// tables. Scene frames are numbered from 1 and each row holds two samples.
/// While playback is ready, direction and the 0/1 presentation substep select
/// the neighbouring sample. Event tables clamp reverse sampling at zero.
/// The selected sample must fit the table; forward sampling and the traversal
/// table's reverse sampling have no clamp. Requires a valid sub-ID and loaded
/// view data.
static void _acropolisPlazaApplyStreamCamera(s32 subId)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    ViewCamera(*cameraPairs)[2];
    ViewCamera* camera;
    s16         cameraIndex;

    switch ((u16)subId) {
        case 0:
        case 1:
        case 2:
        case 3:
            cameraPairs = D_acropolis_plaza_801838B8;
            if (queue->movieReady == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraPairs[sceneIndex];
            } else if (queue->reverseSceneFrames == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraPairs[sceneIndex] + queue->movieFrameSubstep;
            } else {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraPairs[sceneIndex] - queue->movieFrameSubstep;
            }
            viewApplyCamera(camera);
            return;
        case 4:
            cameraPairs = D_acropolis_plaza_8018A938;
            break;
        case 5:
            cameraPairs = D_acropolis_plaza_8018CAFC;
            break;
        case 6:
            cameraPairs = D_acropolis_plaza_8018F530;
            break;
        case 7:
            cameraPairs = D_acropolis_plaza_8018F9B4;
            break;
    }
    if (queue->movieReady == 0) {
        s32 sceneIndex = queue->sceneFrame - 1;
        camera         = cameraPairs[sceneIndex];
    } else {
        if (queue->reverseSceneFrames == 0) {
            cameraIndex = ((queue->sceneFrame - 1) * 2) + queue->movieFrameSubstep + 1;
        } else {
            cameraIndex = ((queue->sceneFrame - 1) * 2) - queue->movieFrameSubstep - 1;
            if (cameraIndex < 0) {
                cameraIndex = 0;
            }
        }
        camera = *cameraPairs + cameraIndex;
    }
    viewApplyCamera(camera);
}

/// Classifies the player's forward or backward crossing of the current scene shot.
///
/// Uses shot X minus player root X in world units. A forward crossing begins
/// at 201; a backward crossing is strictly below -20 and is suppressed at
/// scene frame 1. Each flag distinguishes no crossing, walking and running.
/// The scene work and its borrowed player must be live.
static __inline__ void _acropolisPlazaUpdateSceneEdgeFlags(_AcropolisPlazaSceneWork* work)
{
    CdCmdQueue* queue     = &gCdCmdQueue;
    s32         relativeX = work->relX;

    work->fwd  = ACROPOLIS_PLAZA_EDGE_NONE;
    work->back = ACROPOLIS_PLAZA_EDGE_NONE;
    if (relativeX >= ACROPOLIS_PLAZA_FORWARD_EDGE) {
        if ((u16)work->player->movementMode == ACROPOLIS_PLAZA_MOVEMENT_RUNNING) {
            work->fwd = ACROPOLIS_PLAZA_EDGE_RUN;
        } else {
            work->fwd = ACROPOLIS_PLAZA_EDGE_WALK;
        }
    } else if (relativeX < ACROPOLIS_PLAZA_BACK_EDGE) {
        if (queue->sceneFrame != ACROPOLIS_PLAZA_FIRST_SCENE_FRAME) {
            if ((u16)work->player->movementMode == ACROPOLIS_PLAZA_MOVEMENT_RUNNING) {
                work->back = ACROPOLIS_PLAZA_EDGE_RUN;
            } else {
                work->back = ACROPOLIS_PLAZA_EDGE_WALK;
            }
        }
    }
}

/// Tracks the player through the streamed plaza scene and seeks when a shot edge is crossed.
///
/// `spawnArg2.pointer` borrows an `_AcropolisPlazaSceneArg` until the first
/// tick. The task owns its allocated scene work; player task, actor and root
/// matrix are borrowed. Its scene frame must select the loaded shot-position
/// and camera tables. Seeks encode a signed 16-bit sector offset, using 10 or
/// 40 sectors per scene frame. Frame 0 requests offset -10; it is not clamped.
///
/// Walking computes a new offset; running still queues a request without
/// assigning that local. The binary's running-path offset is unproven.
static void _acropolisPlazaStreamedSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_SCENE_INIT          = 0,
        ACROPOLIS_PLAZA_SCENE_WAIT_IDLE     = 1,
        ACROPOLIS_PLAZA_SCENE_CHECK_EDGES   = 2,
        ACROPOLIS_PLAZA_SCENE_WAIT_READY    = 3,
        ACROPOLIS_PLAZA_SCENE_FOLLOW_EDGE   = 4,
        ACROPOLIS_PLAZA_SCENE_WAIT_SEEK_END = 5,
        ACROPOLIS_PLAZA_SCENE_AT_END        = 6,
    };
    u8                        streamArgs[4];
    s32                       sectorOffset;
    u32                       packedSeekOffset;
    u32                       packedOpeningOffset;
    s32                       lowByteMask = 0xFF;
    s32                       reverse;
    _AcropolisPlazaSceneWork* allocatedWork;
    CdCmdQueue*               queue;
    _AcropolisPlazaSceneWork* work;
    _AcropolisPlazaSceneArg*  spawnArg;
    Task*                     playerTask;
    u16                       startFrame;

    queue = &gCdCmdQueue;
    work  = task->work;

    if (task->state != ACROPOLIS_PLAZA_SCENE_INIT) {
        // Sub-ids 0..3 share the shot-position table, one row per scene frame.
        switch (queue->plazaStreamSubId) {
            case 0:
            case 1:
            case 2:
            case 3:
                work->shotPos.vx = D_acropolis_plaza_801907C4[queue->sceneFrame - 1].vx;
                work->shotPos.vy = D_acropolis_plaza_801907C4[queue->sceneFrame - 1].vy;
                work->shotPos.vz = D_acropolis_plaza_801907C4[queue->sceneFrame - 1].vz;
                work->relX       = work->shotPos.vx - work->playerMtx->t[0];
                break;
        }
    }

    switch (task->state) {
        case ACROPOLIS_PLAZA_SCENE_INIT:
            allocatedWork = memMalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
            spawnArg                  = task->spawnArg2.pointer;
            work                      = task->work;
            startFrame                = spawnArg->startFrame;
            queue->plazaStreamSubId   = 0;
            queue->sceneFrame         = startFrame;
            queue->movieFrame         = startFrame;
            work->prevReverse         = 0;
            queue->reverseSceneFrames = 0;
            queue->movieReady         = 0;
            queue->continueMovie      = 0;
            // The argument is fetched from the task again after the queue stores.
            spawnArg = task->spawnArg2.pointer;
            if (spawnArg->skipStreamReset == 0) {
                streamArgs[0]       = streamFindMovieSlot(&gGameSession->location.loc, queue->plazaStreamSubId, 0);
                sectorOffset        = (queue->movieFrame - 1) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                packedOpeningOffset = sectorOffset & 0xFFFF;
                streamArgs[1]       = packedOpeningOffset >> 8;
                streamArgs[2]       = packedOpeningOffset & lowByteMask;
                cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
            } else {
                queue->movieAtEnd = 0;
            }
            ((_AcropolisPlazaSceneWork*)task->work)->playerMtx = gPlayerStatus.coordMtx;
            work->frameStep                                    = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
            task->state                                        = task->state + 1;
            break;
        case ACROPOLIS_PLAZA_SCENE_WAIT_IDLE:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            work->playerTask = playerTask;
            work->player     = playerTask->work;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case ACROPOLIS_PLAZA_SCENE_CHECK_EDGES:
        checkEdges:
            work->frameLimit = streamGetFrameLimit(queue->plazaStreamSubId);
            if (queue->movieAtEnd != 0) {
                task->state = ACROPOLIS_PLAZA_SCENE_AT_END;
                break;
            }
            _acropolisPlazaUpdateSceneEdgeFlags(task->work);
            if (work->fwd != ACROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 0;
            }
            if (work->back != ACROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 1;
            }
            if (work->fwd == ACROPOLIS_PLAZA_EDGE_NONE && work->back == ACROPOLIS_PLAZA_EDGE_NONE) {
                break;
            }
            // Continuing counts from the movie frame; reversing counts back from the frame limit.
            queue->continueMovie = 1;
            reverse              = queue->reverseSceneFrames;
            if (reverse == work->prevReverse) {
                if (reverse == 0) {
                    if (work->fwd == ACROPOLIS_PLAZA_EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                }
            } else {
                if (reverse == 0) {
                    if (work->fwd == ACROPOLIS_PLAZA_EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == ACROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * ACROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = ACROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                }
            }
            streamArgs[0]    = streamFindMovieSlot(&gGameSession->location.loc, queue->plazaStreamSubId, 0);
            packedSeekOffset = sectorOffset & 0xFFFF;
            streamArgs[1]    = packedSeekOffset >> 8;
            streamArgs[2]    = packedSeekOffset;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM_AT_OFFSET, 0, streamArgs);
            queue->movieReady = 0;
            task->state       = task->state + 1;
            break;
        case ACROPOLIS_PLAZA_SCENE_WAIT_READY:
            if (queue->movieReady != 0) {
                task->state = task->state + 1;
            }
            break;
        case ACROPOLIS_PLAZA_SCENE_FOLLOW_EDGE:
            _acropolisPlazaUpdateSceneEdgeFlags(task->work);
            if (queue->movieAtEnd != 0) {
                task->state = ACROPOLIS_PLAZA_SCENE_AT_END;
                break;
            }
            if ((work->fwd == ACROPOLIS_PLAZA_EDGE_NONE && queue->reverseSceneFrames == 0) ||
                (work->back == ACROPOLIS_PLAZA_EDGE_NONE && queue->reverseSceneFrames == 1) ||
                work->fwd != work->prevFwd || work->back != work->prevBack) {
                queue->continueMovie = 0;
                task->state          = task->state + 1;
            }
            break;
        case ACROPOLIS_PLAZA_SCENE_WAIT_SEEK_END:
            _acropolisPlazaUpdateSceneEdgeFlags(task->work);
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            task->state = ACROPOLIS_PLAZA_SCENE_CHECK_EDGES;
            goto checkEdges;
        case ACROPOLIS_PLAZA_SCENE_AT_END:
            _acropolisPlazaUpdateSceneEdgeFlags(task->work);
            switch (queue->plazaStreamSubId) {
                case 0:
                case 2:
                    if (work->back != ACROPOLIS_PLAZA_EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = ACROPOLIS_PLAZA_SCENE_CHECK_EDGES;
                    }
                    break;
                case 1:
                case 3:
                    if (work->fwd != ACROPOLIS_PLAZA_EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = ACROPOLIS_PLAZA_SCENE_CHECK_EDGES;
                    }
                    break;
            }
            break;
    }

    if (queue->movieReady != 0) {
        _acropolisPlazaUpdateSceneCollisionWalls(task);
        _acropolisPlazaApplyStreamCamera(queue->plazaStreamSubId);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Allocates an event task's work, caches the player, or kills it and returns.
///
/// `eventTask` must be a stable, side-effect-free live Task* expression; it
/// is evaluated repeatedly. On allocation failure this returns from the
/// enclosing void callback. The successful work belongs to task teardown.
#define ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(eventTask)                                                     \
    {                                                                                                        \
        _AcropolisPlazaEventWork* allocatedWork;                                                             \
        allocatedWork     = memMalloc(sizeof(*allocatedWork), false);                                        \
        (eventTask)->work = allocatedWork;                                                                   \
        if (allocatedWork == NULL) {                                                                         \
            taskKill(eventTask);                                                                             \
            return;                                                                                          \
        }                                                                                                    \
        memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));                                              \
        ((_AcropolisPlazaEventWork*)(eventTask)->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER); \
    }

/// Handles the first plaza scene event by walking and turning the player, then running its script.
///
/// `spawnArg2.pointer` borrows the live sequence work. This task owns an
/// allocated event work block. After scripted motion and the current stream
/// finish, it saves the resume frame, kills the traversal task and runs the
/// event script with a skip alternative. The sequence work must outlive it.
static void _acropolisPlazaFirstSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_FIRST_SCENE_INIT        = 0,
        ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_MOVE   = 1,
        ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_TURN   = 2,
        ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_STREAM = 3,
        ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_SCRIPT = 4,
    };
    ActorTransform            destination;
    ActorTransform            heading;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        case ACROPOLIS_PLAZA_FIRST_SCENE_INIT:
            ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = 0x3804;
            destination.pos.vy = 0;
            destination.pos.vz = 0xFC8;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_STREAM:
            // Preserve the traversal frame before handing control to the event script.
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            evsStartScriptWithSkip(D_acropolis_plaza_80182734, EVENT_SCRIPT_HUD_KEEP, D_acropolis_plaza_80182A34);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FIRST_SCENE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

/// Queues a plaza movie sub-ID at sector offset zero.
///
/// `command` must be `CD_COMMAND_PLAY_STREAM_AT_OFFSET` or
/// `CD_COMMAND_RESET_STREAM_AT_OFFSET`; reset
/// also clears both movie buffer selectors. Requires a loaded current-room
/// movie. The queue copies four argument bytes synchronously, interpreting
/// the slot and signed 16-bit big-endian sector offset and retaining the
/// uninterpreted fourth byte. No pointer to this local buffer survives.
static inline void _acropolisPlazaQueueStreamAtStart(u8 command, u8 subId)
{
    u8 streamArgs[4];

    streamArgs[0] = streamFindMovieSlot(&gGameSession->location.loc, subId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(command, 0, streamArgs);
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
    placement   = areaGetVariant(&key)->placements;
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

/// Plays a clip from the player's equipped-weapon bank with world collision disabled.
///
/// The current player must be live and the selected bank and clip loaded.
/// Character 1 selects weapon index + 1; other character IDs select index +
/// 34, whose validity is unproven for the resident 34-entry bank table.
/// `blend` is an `ANIMATION_BLEND_*` choice; `blendFrames` counts game frames
/// and is ignored by the player on reset. Dispatch consumes the stack request
/// synchronously, and the animation resources remain borrowed during playback.
static inline void _acropolisPlazaPlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{
    enum {
        ACROPOLIS_PLAZA_PRIMARY_CHARACTER          = 1,
        ACROPOLIS_PLAZA_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACROPOLIS_PLAZA_ALTERNATE_WEAPON_BANK_BASE = 34
    };
    AnimationPlayRequest request;
    s32                  weaponIndex;

    weaponIndex                  = gPlayerStatus.weapon;
    request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_PLAZA_PRIMARY_CHARACTER) ? weaponIndex + ACROPOLIS_PLAZA_PRIMARY_WEAPON_BANK_BASE : weaponIndex + ACROPOLIS_PLAZA_ALTERNATE_WEAPON_BANK_BASE;
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Places the player at the model root's local translation, upright at the requested yaw.
///
/// `task` owns live event work whose borrowed player has a coordinate-body
/// model. Translation retains signed 32-bit game coordinates; `yaw` narrows
/// to signed 16 bits, in 4096 units per turn. The root and player placement
/// must use the same parent frame. Dispatch consumes the transform
/// synchronously and retains no pointer to it.
static inline void _acropolisPlazaPlacePlayerAtModelRoot(Task* task, s32 yaw)
{
    ActorTransform placement;
    GfxCoord*      root;

    root             = ((_AcropolisPlazaEventWork*)task->work)->playerTask->extra.tmd->coords;
    placement.pos.vx = root->coord.t[0];
    placement.pos.vy = root->coord.t[1];
    placement.pos.vz = root->coord.t[2];
    placement.rot.vz = 0;
    placement.rot.vx = 0;
    placement.rot.vy = yaw;
    TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

/// Handles a plaza stream-scene event, returning the player from scene animation to play.
///
/// `spawnArg2.pointer` borrows live sequence work, which must outlive the
/// task. This task owns its event work. It walks and turns the player, installs
/// the scene animation, saves the traversal frame and replaces that stream.
/// At movie frame 96 it restores equipped-bank idle and places the player at
/// the model root; completion releases scripted control. Camera sampling starts
/// only after the event script has begun.
static void _acropolisPlazaStreamSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_STREAM_SCENE_INIT             = 0,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_MOVE        = 1,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_TURN        = 2,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_IDLE        = 3,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_READY       = 4,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_REJOIN      = 5,
        ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_END         = 6,
        ACROPOLIS_PLAZA_STREAM_SCENE_PLAYER_ANIMATION = 11,
        ACROPOLIS_PLAZA_STREAM_SCENE_REJOIN_FRAME     = 96,
    };
    ActorTransform            destination;
    ActorTransform            heading;
    AnimationPlayRequest      animationRequest;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        case ACROPOLIS_PLAZA_STREAM_SCENE_INIT:
            ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = 0xF6E;
            destination.pos.vy = 0;
            destination.pos.vz = 0x2328;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            animationRequest.source.sets          = D_actor_310100_801797FC;
            animationRequest.animationId          = ACROPOLIS_PLAZA_STREAM_SCENE_PLAYER_ANIMATION;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = 2;
            _acropolisPlazaQueueStreamAtStart(CD_COMMAND_RESET_STREAM_AT_OFFSET, 2);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            evsStartScript(D_acropolis_plaza_80182B24, EVENT_SCRIPT_HUD_KEEP);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_REJOIN:
            // Fold the scene pose back into placement before ordinary control resumes.
            if (queue->movieFrame >= ACROPOLIS_PLAZA_STREAM_SCENE_REJOIN_FRAME) {
                _acropolisPlazaPlayPlayerAnimation(1, ANIMATION_BLEND_RESET, 10);
                _acropolisPlazaPlacePlayerAtModelRoot(task, 0xEAA);
                task->state = task->state + 1;
            }
            break;
        case ACROPOLIS_PLAZA_STREAM_SCENE_WAIT_END:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskRequestKill(task, 0);
            }
            break;
        default:
            return;
    }
    _acropolisPlazaApplyStreamCamera(4);
}

#undef ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK

/// Sixteen-state opening sequence for the plaza's long streamed scene, and the
/// counterpart to `_acropolisPlazaStreamSceneTask` for the rest of it. States 0
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
            _acropolisPlazaApplyStreamCamera(6);
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
            _acropolisPlazaApplyStreamCamera(7);
            return;
        case 8:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(_acropolisPlazaFindPlacedEnemy(0x6C)->task, 0x7D7, 1, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                gpuResetAndInvalidateModelBuffers();
                memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
                memSelectAuxHeapRegion(true);
                tmdResetAuxHeapAndRestoreBuffers();
                sndEvtRequestScriptVolume(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 5), 0x26);
                task->state = task->state + 1;
                return;
            }
            _acropolisPlazaApplyStreamCamera(7);
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
                stageMusicRequestAreaStop(0xA);
                cdCmdRequestCancel();
                task->state = 0xF;
            } else if (gGameSession->eventState == 0) {
                stageMusicRequestAreaStop(0x1E0);
                q->sceneFrame       = 1;
                q->movieFrame       = 1;
                q->plazaStreamSubId = 3;
                _acropolisPlazaQueueStreamAtStart(CD_COMMAND_PLAY_STREAM_AT_OFFSET, 3);
                q->continueMovie = 1;
                task->state      = task->state + 1;
            }
            _acropolisPlazaApplyStreamCamera(5);
            return;
        case 14:
            if (q->movieReady != 0) {
                evsStartScript(D_acropolis_plaza_801834B4, EVENT_SCRIPT_HUD_KEEP);
                task->state = task->state + 1;
            }
            _acropolisPlazaApplyStreamCamera(5);
            return;
        case 15:
            if (cdCmdIsIdle() != 0) {
                sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 2), 0xB4);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
                taskRequestKill(task, 0);
            }
            _acropolisPlazaApplyStreamCamera(5);
            return;
        default:
            return;
    }
}

/// Runs one of the plaza's three repeat-event scripts after the traversal stream stops.
///
/// `spawnArg1.value` selects script variant 0..2 and `spawnArg2.pointer`
/// borrows the live sequence work. The player and equipped animation bank
/// must be available. The task saves the resume frame and kills the traversal
/// task before starting the script; the sequence work must outlive it.
static void _acropolisPlazaRepeatSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_REPEAT_SCENE_INIT        = 0,
        ACROPOLIS_PLAZA_REPEAT_SCENE_WAIT_STREAM = 1,
        ACROPOLIS_PLAZA_REPEAT_SCENE_WAIT_SCRIPT = 2,
    };
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case ACROPOLIS_PLAZA_REPEAT_SCENE_INIT:
            _acropolisPlazaPlayPlayerAnimation(1, ANIMATION_BLEND_RESET, 10);
            task->state = task->state + 1;
            break;
        case ACROPOLIS_PLAZA_REPEAT_SCENE_WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
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
        case ACROPOLIS_PLAZA_REPEAT_SCENE_WAIT_SCRIPT:
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
                capRunCommandWithTransition((s8)((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->eventKind);
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
            sndEvtRequestScriptVolume((sndId), 0);      \
            *(state) = 1;                               \
            return;                                     \
        }                                               \
    } while (0)

/// Starts, envelopes or stops one plaza ambience script against the current scene frame.
///
/// `startRequested` addresses the sequence's writable voice flag; it must start at
/// zero and remain live. Within the inclusive first/last frame interval, the
/// first call queues a silent start and later calls set volume (low eight bits).
/// The flag records that request even if the sound queue rejects admission.
/// Envelope 0 peaks at 127, 1 at 3/4 of that, and 2 uses the early fade for
/// area sound 2. Triangular envelopes require a nonzero denominator on every
/// selected arm; a peak equal to the first frame is safe.
/// Leaving the interval stops a flagged voice without a release fade.
static void _acropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    u32         sceneFrame;
    u8          volume;

    sceneFrame = queue->sceneFrame;
    if (sceneFrame >= firstFrame && sceneFrame <= lastFrame) {
        _acropolisPlazaStartAmbience(startRequested, soundId);
        if (envelope == ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / (peakFrame - firstFrame);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / (lastFrame - peakFrame);
            }
        } else if (envelope == ACROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * (ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((peakFrame - firstFrame) * 4);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * (ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((lastFrame - peakFrame) * 4);
            }
        } else if (sceneFrame < (u32)ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) {
            volume = (((ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - queue->sceneFrame) * ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / ((ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - 1) * 4)) + ACROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
        } else {
            volume = ((ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - queue->sceneFrame) * (ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) * 4);
        }
        sndEvtRequestScriptVolume(soundId, volume);
        return;
    }
    if (*startRequested != 0) {
        sndEvtRequestScriptStop(soundId, SOUND_SCRIPT_STOP_NO_FADE);
        *startRequested = 0;
    }
}

#undef _acropolisPlazaStartAmbience

/// Updates the plaza's four traversal ambience voices or the stream-scene voice envelope.
///
/// Requires the live sequence task and loaded plaza area sound bank. Sub-IDs
/// 0/1 drive four independent envelopes; sub-ID 2 changes area sound 1
/// without starting or stopping it. That curve is full volume on frames
/// 31..84 inclusive; its other arms keep their byte truncation and do not clamp.
static void _acropolisPlazaUpdateSceneAmbience(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME      = 800,
        ACROPOLIS_PLAZA_AMBIENCE4_FIRST_FRAME             = 15,
        ACROPOLIS_PLAZA_AMBIENCE4_PEAK_FRAME              = 80,
        ACROPOLIS_PLAZA_AMBIENCE4_LAST_FRAME              = 142,
        ACROPOLIS_PLAZA_AMBIENCE1_FIRST_FRAME             = 200,
        ACROPOLIS_PLAZA_AMBIENCE1_PEAK_FRAME              = 320,
        ACROPOLIS_PLAZA_AMBIENCE1_LAST_FRAME              = 370,
        ACROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FIRST_FRAME  = 31,
        ACROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FRAME_COUNT  = 54,
        ACROPOLIS_PLAZA_AMBIENCE_STREAM_RISE_END_FRAME    = 30,
        ACROPOLIS_PLAZA_AMBIENCE_STREAM_FALL_ANCHOR_FRAME = 115,
        ACROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES       = 120
    };
    CdCmdQueue*                  queue            = &gCdCmdQueue;
    volatile u16*                sceneFrameSource = &gCdCmdQueue.sceneFrame;
    _AcropolisPlazaSequenceWork* work             = task->work;
    s32                          sceneFrame;
    s32                          volume;

    switch (gCdCmdQueue.plazaStreamSubId) {
        case 0:
        case 1:
            _acropolisPlazaUpdateAmbienceVoice(1, ACROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME, 1, &work->ambience5Playing, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 5), ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _acropolisPlazaUpdateAmbienceVoice(1, ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END, 1, &work->ambience2Playing, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 2), ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE);
            _acropolisPlazaUpdateAmbienceVoice(ACROPOLIS_PLAZA_AMBIENCE4_FIRST_FRAME, ACROPOLIS_PLAZA_AMBIENCE4_LAST_FRAME, ACROPOLIS_PLAZA_AMBIENCE4_PEAK_FRAME, &work->ambience4Playing, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 4), ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _acropolisPlazaUpdateAmbienceVoice(ACROPOLIS_PLAZA_AMBIENCE1_FIRST_FRAME, ACROPOLIS_PLAZA_AMBIENCE1_LAST_FRAME, ACROPOLIS_PLAZA_AMBIENCE1_PEAK_FRAME, &work->ambience1Playing, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 1), ACROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE);
            break;
        case 2:
            // Compare a snapshot, then reread the frame published by MDEC completion.
            sceneFrame = queue->sceneFrame;
            if ((u32)(sceneFrame - ACROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FIRST_FRAME) < (u32)ACROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FRAME_COUNT) {
                volume = ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME;
            } else if (sceneFrame < (u32)ACROPOLIS_PLAZA_AMBIENCE_STREAM_RISE_END_FRAME) {
                volume = ((*sceneFrameSource * ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / ACROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES) + ACROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
            } else {
                volume = (((ACROPOLIS_PLAZA_AMBIENCE_STREAM_FALL_ANCHOR_FRAME - *sceneFrameSource) * ACROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / ACROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES) + ACROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
            }
            sndEvtRequestScriptVolume(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 1), volume & 0xFF);
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

/// Six-state opening sequence for the plaza. State 0 suppresses action/menu input
/// except interaction (`padInputChangeSuppression`), applies the plaza view, allocates the sequence work block
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
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU & ~PAD_BUTTON_CIRCLE);
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
            stageMusicRequestAreaStart(0);
            taskSpawnFromTable(D_acropolis_plaza_80183824, 8, 6, 0);
            q->blockGamePause = 1;
            task->state       = task->state + 1;
            return;
        case 4:
            _acropolisPlazaUpdateSceneAmbience(task);
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
            loadingEnqueueEquippedWeaponResources();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            taskSpawn(0, 0x11, 0, 0);
            q->blockGamePause = 0;
            taskKill(task);
            return;
    }
}

/// Hands the display loop to the plaza movie task, then ends this setup task.
///
/// Requires the loaded plaza task table and current view/packet data. The
/// movie task owns presentation until it restores the ordinary game loop.
static void _acropolisPlazaStartMovieDisplayTask(Task* task)
{
    enum { ACROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX = 10 };
    displaySpawnTaskFromTable(D_acropolis_plaza_80183824, ACROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
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
