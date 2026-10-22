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

#include "gameplay/companion_load.h"
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
#include "../../shared/screen/screen_fade.h"

#include "assets/rooms/akropolis/plaza/declarations.inc"

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
        (primitive)->x0 = (centre).vx + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex) + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                    \
        (primitive)->y0 = (centre).vy + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex)]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                        \
        (primitive)->x1 = (centre).vx + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex) + 5]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                    \
        (primitive)->y1 = (centre).vy + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex) + 1]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                    \
        (primitive)->x2 = (centre).vx;                                                                                                                             \
        (primitive)->y2 = (centre).vy;                                                                                                                             \
        (primitive)->x3 = (centre).vx + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex) + 6]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                    \
        (primitive)->y3 = (centre).vy + (((radius) * _gAcropolisPlazaLightRingSamples[(sampleIndex) + 2]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);                    \
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

static void _acropolisPlazaFadeToWhiteTask(Task* task);
static void _acropolisPlazaMovieDisplayTask(Task* task);
static void _acropolisPlazaStreamedSceneTask(Task* task);
static void _acropolisPlazaFirstSceneTask(Task* task);
static void _acropolisPlazaStreamSceneTask(Task* task);
static void _acropolisPlazaFinalSceneTask(Task* task);
static void _acropolisPlazaRepeatSceneTask(Task* task);
static void _acropolisPlazaCaptionEventTask(Task* task);
static void _acropolisPlazaLetterboxTask(Task* task);
static void _acropolisPlazaSequenceTask(Task* task);
static void _acropolisPlazaStartMovieDisplayTask(Task* task);

/// Player selection and transition duration in the officer controller's requests.
enum {
    ACROPOLIS_PLAZA_ANIMATION_TARGET_PLAYER       = 0,
    ACROPOLIS_PLAZA_PLAYER_ANIMATION_BLEND_FRAMES = 10
};

/// Officer selectors in animation requests sent to the plaza controllers.
enum {
    ACROPOLIS_PLAZA_ANIMATION_TARGET_OFFICER_1 = 1,
    ACROPOLIS_PLAZA_ANIMATION_TARGET_OFFICER_2 = 2
};

/// Scene-child selectors and body modes used by the plaza's event scripts.
///
/// Child 0 is officer 2 and child 1 is officer 1; body-swap selectors use
/// the same order. The culled-body message also accepts mode 3 to freeze
/// the current model. These messages borrow their payloads synchronously.
enum {
    ACROPOLIS_PLAZA_SCRIPT_OFFICER_2            = 0,
    ACROPOLIS_PLAZA_SCRIPT_OFFICER_1            = 1,
    ACROPOLIS_PLAZA_OFFICER_MESSAGE_STOP        = 2002,
    ACROPOLIS_PLAZA_OFFICER_MESSAGE_CULLED_BODY = 2007,
    ACROPOLIS_PLAZA_OFFICER_BODY_2              = 0,
    ACROPOLIS_PLAZA_OFFICER_BODY_1              = 1,
    ACROPOLIS_PLAZA_OFFICER_BODY_FREEZE         = 3,
};

/// CAP command indices of the plaza's scripted conversations and caption triggers.
enum {
    ACROPOLIS_PLAZA_CAP_FIRST_SCENE             = 1,
    ACROPOLIS_PLAZA_CAP_FIRST_REPEAT_SCENE      = 2,
    ACROPOLIS_PLAZA_CAP_SECOND_REPEAT_SCENE     = 3,
    ACROPOLIS_PLAZA_CAP_FINAL_SCENE_LEAD_IN     = 4,
    ACROPOLIS_PLAZA_CAP_FINAL_SCENE_OFFICERS    = 5,
    ACROPOLIS_PLAZA_CAP_FIRST_INTERACTION       = 6,
    ACROPOLIS_PLAZA_CAP_SECOND_INTERACTION      = 7,
    ACROPOLIS_PLAZA_CAP_SUBSEQUENT_REPEAT_SCENE = 8,
    ACROPOLIS_PLAZA_CAP_STREAM_SCENE            = 9,
    ACROPOLIS_PLAZA_CAP_FINAL_SCENE_CLOSING     = 10,
};

enum { ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY = 192 };

#include "assets/rooms/akropolis/plaza/events.inc"
TaskDesc gAcropolisPlazaSequenceTaskDescriptors[ACROPOLIS_PLAZA_TASK_COUNT] = {
    [ACROPOLIS_PLAZA_TASK_SEQUENCE]       = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaSequenceTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_STREAMED_SCENE] = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaStreamedSceneTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_STREAM_SCENE]   = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaStreamSceneTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_FINAL_SCENE]    = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaFinalSceneTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_FIRST_SCENE]    = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaFirstSceneTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_DISPLAY_SETUP]  = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaStartMovieDisplayTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_REPEAT_SCENE]   = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaRepeatSceneTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_FADE_TO_WHITE]  = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaFadeToWhiteTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_FADE_IN]        = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, screenFadeInTileTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_CAPTION]        = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaCaptionEventTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_MOVIE_DISPLAY]  = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaMovieDisplayTask, { .value = 0 } },
    [ACROPOLIS_PLAZA_TASK_LETTERBOX]      = { { { TASK_BODY_NONE, ACROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _acropolisPlazaLetterboxTask, { .value = 0 } },
};

/// Stored camera-path lengths in scene frames; each frame has two consecutive samples.
enum {
    ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME         = 2,
    ACROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT     = 400,
    ACROPOLIS_PLAZA_CAMERA_STREAM_SCENE_FRAME_COUNT  = 120,
    ACROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT = 150,
    ACROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT = 16,
    ACROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT = 50
};

/// Retained sample-count header of the traversal camera path.
///
/// Counts complete camera samples, with two per scene frame.
/// Playback indexes the following path without reading this word.
static s32 _gAcropolisPlazaTraversalCameraSampleCount = ACROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Traversal camera path shared by selectors 0..3.
///
/// Samples are complete world-to-view transforms and projection distances.
/// Select sample 2*(sceneFrame-1), then add or subtract the presentation
/// substep when ready. The selected sample must remain within the array.
static ViewCamera _gAcropolisPlazaTraversalCameraSamples[ACROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/acropolis_plaza_path_062F8.inc"
};

/// Camera path for the stream-scene event (camera selector 4).
///
/// Samples are complete world-to-view transforms and projection distances.
/// The selector is independent of the movie sub-ID. Before readiness use
/// 2*(sceneFrame-1); ready forward playback adds substep+1, reverse subtracts
/// substep+1 and clamps at zero. Forward sampling must fit the stored extent.
static ViewCamera _gAcropolisPlazaStreamSceneCameraSamples[ACROPOLIS_PLAZA_CAMERA_STREAM_SCENE_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/acropolis_plaza_path_0D378.inc"
};

/// Retained sample-count header of the closing-scene camera path.
///
/// Counts complete camera samples, with two per scene frame.
/// Playback indexes the following path without reading this word.
static s32 _gAcropolisPlazaClosingSceneCameraSampleCount = ACROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Closing camera path of the final scene (camera selector 5).
///
/// Samples are complete world-to-view transforms and projection distances.
/// The selector is independent of the movie sub-ID. Before readiness use
/// 2*(sceneFrame-1); ready forward playback adds substep+1, reverse subtracts
/// substep+1 and clamps at zero. Forward sampling must fit the stored extent.
static ViewCamera _gAcropolisPlazaClosingSceneCameraSamples[ACROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/acropolis_plaza_path_0F53C.inc"
};

/// Retained sample-count header of the final lead-in camera path.
///
/// Counts complete camera samples, with two per scene frame.
/// Playback indexes the following path without reading this word.
static s32 _gAcropolisPlazaFinalSceneLeadInCameraSampleCount = ACROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Lead-in camera path of the final scene (camera selector 6).
///
/// Samples are complete world-to-view transforms and projection distances.
/// The selector is independent of the movie sub-ID. Before readiness use
/// 2*(sceneFrame-1); ready forward playback adds substep+1, reverse subtracts
/// substep+1 and clamps at zero. Forward sampling must fit the stored extent.
static ViewCamera _gAcropolisPlazaFinalSceneLeadInCameraSamples[ACROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/acropolis_plaza_path_11F70.inc"
};

/// Retained sample-count header of the officer-scene camera path.
///
/// Counts complete camera samples, with two per scene frame.
/// Playback indexes the following path without reading this word.
static s32 _gAcropolisPlazaOfficerSceneCameraSampleCount = ACROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Officer camera path of the final scene (camera selector 7).
///
/// Samples are complete world-to-view transforms and projection distances.
/// The selector is independent of the movie sub-ID. Before readiness use
/// 2*(sceneFrame-1); ready forward playback adds substep+1, reverse subtracts
/// substep+1 and clamps at zero. Forward sampling must fit the stored extent.
static ViewCamera _gAcropolisPlazaOfficerSceneCameraSamples[ACROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/acropolis_plaza_path_123F4.inc"
};

/// World-space shot reference points for the streamed traversal's collision walls and edge tests.
///
/// One XYZ position in whole game units per scene frame, shared by sub-IDs
/// 0..3. The caller must supply sceneFrame 1..400; indexing is not clamped.
static VECTOR3 _gAcropolisPlazaTraversalShotPositions[ACROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT] = {
#include "assets/acropolis_plaza_path_13204.inc"
};

#include "assets/rooms/akropolis/plaza/clips.inc"
#include "assets/rooms/akropolis/plaza/samples.inc"
#include "assets/rooms/akropolis/plaza/markers.inc"
#define ROOM_PART gAcropolisPlazaRoomCollisionResources
#include "assets/rooms/akropolis/plaza/header.inc"
#include "assets/rooms/akropolis/plaza/cameras.inc"
#include "assets/rooms/akropolis/plaza/sprites.inc"
#define ROOM_PART gAcropolisPlazaWarpEntries
#include "assets/rooms/akropolis/plaza/header.inc"
#include "assets/rooms/akropolis/plaza/geometry.inc"
#include "assets/rooms/akropolis/plaza/action_triggers.inc"
#include "assets/rooms/akropolis/plaza/placements.inc"
#include "assets/rooms/akropolis/plaza/lights.inc"
#include "assets/rooms/akropolis/plaza/surfaces.inc"

/// Presentation choices carried by a terminal first batch's count.
enum {
    ACROPOLIS_PLAZA_SPRITE_SHOW_IMAGE = 0,
    ACROPOLIS_PLAZA_SPRITE_HIDE_IMAGE = 1
};

/// Resource-entry keys and controller recipe used by the plaza's two officers.
enum {
    ACROPOLIS_PLAZA_RESOURCE_ENTRY_OFFICER_1 = 108,
    ACROPOLIS_PLAZA_RESOURCE_ENTRY_OFFICER_2 = 109,
    ACROPOLIS_PLAZA_OFFICER_FILE_NUMBER      = 101,
    ACROPOLIS_PLAZA_OFFICER_CONTROLLER_TASK  = 0
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    ACROPOLIS_PLAZA_DEFAULT_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_PLAZA_DEFAULT_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
    ACROPOLIS_PLAZA_CLASS1_WALK_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    ACROPOLIS_PLAZA_CLASS1_RUN_SOUND   = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F
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

static void        _acropolisPlazaUpdateSceneCollisionWalls(Task* sceneTask);
static void        _acropolisPlazaApplyStreamCamera(s32 subId);
static inline void _acropolisPlazaUpdateSceneEdgeFlags(_AcropolisPlazaSceneWork* work);
static void        _acropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope);
static void        _acropolisPlazaUpdateSceneAmbience(Task* task);

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
                                        gMdecMovieInputSlot                = 0;
                                        gMdecMovieOutputSlot               = 0;
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

#include "../../shared/screen/screen_fade_in_tile.inc.c"

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
    u8              streamArgs[4];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case ACROPOLIS_PLAZA_MOVIE_SAVE_GRAPHICS:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case ACROPOLIS_PLAZA_MOVIE_START:
            movieLocation      = gGameSession->location;
            movieLocation.view = ACROPOLIS_PLAZA_DISPLAY_MOVIE_STREAM_ID;
            streamArgs[0]      = streamFindMovieSlot(&movieLocation, 0, 0);
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

    _gAcropolisPlazaCollisionGridVerts[0].vx = leftX;
    _gAcropolisPlazaCollisionGridVerts[1].vx = leftX;
    _gAcropolisPlazaCollisionGridVerts[2].vx = leftX;
    _gAcropolisPlazaCollisionGridVerts[3].vx = leftX;

    topY   = shotY + 0x3E8;
    minZ   = shotZ - 0x1000;
    maxZ   = shotZ + 0x3000;
    rightX = shotX + 0x7D0;

    _gAcropolisPlazaCollisionGridVerts[0].vy = shotY;
    _gAcropolisPlazaCollisionGridVerts[1].vy = shotY;
    _gAcropolisPlazaCollisionGridVerts[2].vy = topY;
    _gAcropolisPlazaCollisionGridVerts[3].vy = topY;

    _gAcropolisPlazaCollisionGridVerts[0].vz = minZ;
    _gAcropolisPlazaCollisionGridVerts[1].vz = maxZ;
    _gAcropolisPlazaCollisionGridVerts[2].vz = minZ;
    _gAcropolisPlazaCollisionGridVerts[3].vz = maxZ;

    _gAcropolisPlazaCollisionGridVerts[4].vx = rightX;
    _gAcropolisPlazaCollisionGridVerts[5].vx = rightX;
    _gAcropolisPlazaCollisionGridVerts[6].vx = rightX;
    _gAcropolisPlazaCollisionGridVerts[7].vx = rightX;

    _gAcropolisPlazaCollisionGridVerts[4].vy = shotY;
    _gAcropolisPlazaCollisionGridVerts[5].vy = shotY;
    _gAcropolisPlazaCollisionGridVerts[6].vy = topY;
    _gAcropolisPlazaCollisionGridVerts[7].vy = topY;

    _gAcropolisPlazaCollisionGridVerts[4].vz = maxZ;
    _gAcropolisPlazaCollisionGridVerts[5].vz = minZ;
    _gAcropolisPlazaCollisionGridVerts[6].vz = maxZ;
    _gAcropolisPlazaCollisionGridVerts[7].vz = minZ;
}

/// Applies one sample from the selected plaza camera path.
///
/// Selectors 0..3 use the traversal path; 4..7 select the four event
/// paths independently of the movie sub-ID. Scene frames start at 1 and
/// each frame has two consecutive samples.
/// While playback is ready, direction and the 0/1 presentation substep select
/// the neighbouring sample. Event tables clamp reverse sampling at zero.
/// The selected sample must fit the table; forward sampling and the traversal
/// table's reverse sampling have no clamp. Requires a valid selector and loaded
/// view data.
static void _acropolisPlazaApplyStreamCamera(s32 subId)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    ViewCamera* cameraSamples;
    ViewCamera* camera;
    s16         cameraIndex;

    switch ((u16)subId) {
        case 0:
        case 1:
        case 2:
        case 3:
            cameraSamples = _gAcropolisPlazaTraversalCameraSamples;
            if (queue->movieReady == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;
            } else if (queue->reverseSceneFrames == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME + queue->movieFrameSubstep;
            } else {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME - queue->movieFrameSubstep;
            }
            viewApplyCamera(camera);
            return;
        case 4:
            cameraSamples = _gAcropolisPlazaStreamSceneCameraSamples;
            break;
        case 5:
            cameraSamples = _gAcropolisPlazaClosingSceneCameraSamples;
            break;
        case 6:
            cameraSamples = _gAcropolisPlazaFinalSceneLeadInCameraSamples;
            break;
        case 7:
            cameraSamples = _gAcropolisPlazaOfficerSceneCameraSamples;
            break;
    }
    if (queue->movieReady == 0) {
        s32 sceneIndex = queue->sceneFrame - 1;
        camera         = cameraSamples + sceneIndex * ACROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;
    } else {
        if (queue->reverseSceneFrames == 0) {
            cameraIndex = ((queue->sceneFrame - 1) * 2) + queue->movieFrameSubstep + 1;
        } else {
            cameraIndex = ((queue->sceneFrame - 1) * 2) - queue->movieFrameSubstep - 1;
            if (cameraIndex < 0) {
                cameraIndex = 0;
            }
        }
        camera = cameraSamples + cameraIndex;
    }
    viewApplyCamera(camera);
}

/// Classifies the player's forward or backward crossing of the current scene shot.
///
/// Uses shot X minus player root X in world units. A forward crossing begins
/// at 201; a backward crossing is strictly below -20 and is suppressed at
/// scene frame 1. Each flag distinguishes no crossing, walking and running.
/// The scene work and its borrowed player must be live.
static inline void _acropolisPlazaUpdateSceneEdgeFlags(_AcropolisPlazaSceneWork* work)
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
                work->shotPos.vx = _gAcropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vx;
                work->shotPos.vy = _gAcropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vy;
                work->shotPos.vz = _gAcropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vz;
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
                streamArgs[0]       = streamFindMovieSlot(&gGameSession->location, queue->plazaStreamSubId, 0);
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
            streamArgs[0]    = streamFindMovieSlot(&gGameSession->location, queue->plazaStreamSubId, 0);
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
            evsStartScriptWithSkip(_gAcropolisPlazaFirstSceneScript, EVENT_SCRIPT_HUD_KEEP, _gAcropolisPlazaFirstSceneSkipScript);
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

    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, subId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(command, 0, streamArgs);
}

/// Finds a live enemy by the resource entry in the current area's placement table.
///
/// Borrows the current stage/area/variant's sentinel-terminated placements and
/// returns the scene child's borrowed enemy, or NULL when that instance is gone.
/// A missing `entryId` uses the terminator index rather than failing the lookup.
/// Only the low sixteen bits of the stage/area/index key reach the scene search;
/// a placement index above 15 wraps. The scene and placement table must remain
/// loaded, and the caller must keep a returned enemy alive while using it.
static inline Enemy* _acropolisPlazaFindPlacedEnemy(u8 entryId)
{
    GameLocationKey        locationKey;
    const GameLocationKey* currentLocation;
    const AreaPlacement*   currentPlacement;
    s32                    placementIndex;

    currentLocation       = &gGameSession->location;
    locationKey.stage     = currentLocation->stage;
    locationKey.area      = currentLocation->area;
    locationKey.roomState = gGameSession->spriteVariant;
    locationKey.view      = gGameSession->location.view;
    locationKey.variant   = currentLocation->variant;
    currentPlacement      = areaGetVariant(&locationKey)->placements;
    placementIndex        = 0;
    // A missing resource entry uses the terminator position in the enemy key.
    if (currentPlacement->entryId != AREA_PLACEMENT_END) {
        for (;;) {
            if (currentPlacement->entryId == entryId) {
                goto found;
            }
            currentPlacement++;
            placementIndex++;
            if (currentPlacement->entryId == AREA_PLACEMENT_END) {
                goto found;
            }
        }
    }
found:
    return sceneFindEnemyByPlaceKey((placementIndex << ENEMY_PLACE_INDEX_SHIFT) | (currentLocation->stage << ENEMY_PLACE_STAGE_SHIFT) | currentLocation->area);
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
            animationRequest.source.sets          = gActor310100PlayerAnimationSets;
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
            evsStartScript(_gAcropolisPlazaStreamSceneScript, EVENT_SCRIPT_HUD_KEEP);
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

/// Resets a plaza movie at sector zero using the caller's four-byte command buffer.
///
/// Requires four writable bytes; writes slot and two big-endian offset bytes.
/// The fourth byte remains uninterpreted. Enqueue copies the buffer synchronously.
static inline void _acropolisPlazaResetStreamInBuffer(u8 streamArgs[4], u8 movieSubId)
{
    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, movieSubId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
}

/// Runs the plaza's final trigger scene, including stream handoffs and its skip path.
///
/// `spawnArg2.pointer` borrows live sequence work, whose traversal task is
/// killed after the player walks and turns. Owns allocated event work through
/// teardown. The player, officer placement entry and scene resources must stay
/// available across graphics-memory rebuilds. Movement and animation messages
/// consume stack payloads synchronously; animations borrow loaded banks.
/// Start cancels the closing stream, and completion releases scripted control
/// and requests task removal. Keep sequence work live through this child.
static void _acropolisPlazaFinalSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_FINAL_INIT                    = 0,
        ACROPOLIS_PLAZA_FINAL_WAIT_WALK               = 1,
        ACROPOLIS_PLAZA_FINAL_WAIT_TURN               = 2,
        ACROPOLIS_PLAZA_FINAL_START_FIRST_STREAM      = 3,
        ACROPOLIS_PLAZA_FINAL_WAIT_FIRST_STREAM       = 4,
        ACROPOLIS_PLAZA_FINAL_WAIT_FIRST_SCRIPT       = 5,
        ACROPOLIS_PLAZA_FINAL_RESTORE_PLAYER          = 6,
        ACROPOLIS_PLAZA_FINAL_WAIT_OFFICER            = 7,
        ACROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS        = 8,
        ACROPOLIS_PLAZA_FINAL_START_SECOND_STREAM     = 9,
        ACROPOLIS_PLAZA_FINAL_WAIT_SECOND_STREAM      = 10,
        ACROPOLIS_PLAZA_FINAL_WAIT_TAIL_EFFECT        = 11,
        ACROPOLIS_PLAZA_FINAL_WAIT_DISPLAY            = 12,
        ACROPOLIS_PLAZA_FINAL_WAIT_TAIL_SCRIPT        = 13,
        ACROPOLIS_PLAZA_FINAL_WAIT_CLOSING_STREAM     = 14,
        ACROPOLIS_PLAZA_FINAL_WAIT_END                = 15,
        ACROPOLIS_PLAZA_FINAL_OFFICER_ENTRY           = 0x6C,
        ACROPOLIS_PLAZA_FINAL_SET_CULLED_BODY_MESSAGE = 2007,
        ACROPOLIS_PLAZA_FINAL_OFFICER_1               = 1,
        ACROPOLIS_PLAZA_FINAL_OFFICER_SOUND_FRAME     = 30,
        ACROPOLIS_PLAZA_FINAL_OFFICER_EFFECT_FRAME    = 61,
        ACROPOLIS_PLAZA_FINAL_TAIL_EFFECT_FRAME       = 11,
        ACROPOLIS_PLAZA_FINAL_WHITE_FADE_TASK         = ACROPOLIS_PLAZA_TASK_FADE_TO_WHITE,
        ACROPOLIS_PLAZA_FINAL_FADE_IN_TASK            = ACROPOLIS_PLAZA_TASK_FADE_IN,
        ACROPOLIS_PLAZA_FINAL_OFFICER_ANIMATION       = 8,
        ACROPOLIS_PLAZA_FINAL_OFFICER_BLEND_FRAMES    = 10,
        ACROPOLIS_PLAZA_FINAL_WHITE_FADE_FRAMES       = 9,
        ACROPOLIS_PLAZA_FINAL_FADE_IN_FRAMES          = 8,
        ACROPOLIS_PLAZA_FINAL_SKIP_MUSIC_FADE_TICKS   = 10,
        ACROPOLIS_PLAZA_FINAL_MUSIC_FADE_TICKS        = 480,
        ACROPOLIS_PLAZA_FINAL_AMBIENCE_STOP_TICKS     = 180,
        ACROPOLIS_PLAZA_FINAL_AMBIENCE_VOLUME         = 38,
        ACROPOLIS_PLAZA_FINAL_FIRST_STREAM            = 4,
        ACROPOLIS_PLAZA_FINAL_OFFICER_STREAM          = 5,
        ACROPOLIS_PLAZA_FINAL_CLOSING_STREAM          = 3,
        ACROPOLIS_PLAZA_FINAL_FIRST_CAMERA            = 6,
        ACROPOLIS_PLAZA_FINAL_OFFICER_CAMERA          = 7,
        ACROPOLIS_PLAZA_FINAL_CLOSING_CAMERA          = 5
    };

    ActorTransform            walkDestination;
    ActorTransform            heading;
    u8                        streamArgs[4];
    ActorTransform            returnDestination;
    AnimationPlayRequest      officerAnimation;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AcropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        // Scripted walking and turning finish before the traversal task is released.
        case ACROPOLIS_PLAZA_FINAL_INIT:
            ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            walkDestination.pos.vx = 0x3DE;
            walkDestination.pos.vy = 0;
            walkDestination.pos.vz = 0x33FE;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &walkDestination, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_WALK:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = ACTOR_TRANSFORM_ANGLE_TURN;
            TASK_MESSAGE_DISPATCH_POINTER(((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_START_FIRST_STREAM:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = ACROPOLIS_PLAZA_FINAL_FIRST_STREAM;
            _acropolisPlazaResetStreamInBuffer(streamArgs, ACROPOLIS_PLAZA_FINAL_FIRST_STREAM);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_FIRST_STREAM:
            if (cdCmdIsIdle() != 0) {
                evsStartScriptWithSkip(_gAcropolisPlazaFinalSceneLeadInScript, EVENT_SCRIPT_HUD_KEEP, _gAcropolisPlazaFinalSceneLeadInSkipScript);
                task->state = task->state + 1;
                return;
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_FIRST_CAMERA);
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_FIRST_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (gGameSession->evtSkipped != 0) {
                SetDispMask(0);
                task->state = ACROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS;
                return;
            }
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = ACROPOLIS_PLAZA_FINAL_OFFICER_STREAM;
            _acropolisPlazaResetStreamInBuffer(streamArgs, ACROPOLIS_PLAZA_FINAL_OFFICER_STREAM);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_RESTORE_PLAYER:
            if (queue->movieReady == 0) {
                return;
            }
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            returnDestination.pos.vx = 0x3DE;
            returnDestination.pos.vy = 0;
            returnDestination.pos.vz = 0x439E;
            TASK_MESSAGE_DISPATCH_POINTER(
                ((_AcropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &returnDestination, 0);
            officerAnimation.source.index         = ACROPOLIS_PLAZA_FINAL_OFFICER_1;
            officerAnimation.animationId          = ACROPOLIS_PLAZA_FINAL_OFFICER_ANIMATION;
            officerAnimation.blend                = ANIMATION_BLEND_RESET;
            officerAnimation.blendFrames          = ACROPOLIS_PLAZA_FINAL_OFFICER_BLEND_FRAMES;
            officerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(_acropolisPlazaFindPlacedEnemy(ACROPOLIS_PLAZA_FINAL_OFFICER_ENTRY)->task, ACTOR_MESSAGE_PLAY_ANIMATION, &officerAnimation, 0);
            task->state         = task->state + 1;
            work->elapsedFrames = 0;
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_OFFICER:
            if (work->elapsedFrames == ACROPOLIS_PLAZA_FINAL_OFFICER_SOUND_FRAME) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 3), 0, 0);
            }
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= ACROPOLIS_PLAZA_FINAL_OFFICER_EFFECT_FRAME) {
                taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_FINAL_WHITE_FADE_TASK, ACROPOLIS_PLAZA_FINAL_WHITE_FADE_FRAMES, 0);
                task->state = task->state + 1;
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_OFFICER_CAMERA);
            return;
        // Replace the officer body and release player buffers before rebuilding image memory.
        case ACROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(_acropolisPlazaFindPlacedEnemy(ACROPOLIS_PLAZA_FINAL_OFFICER_ENTRY)->task, ACROPOLIS_PLAZA_FINAL_SET_CULLED_BODY_MESSAGE, ACROPOLIS_PLAZA_FINAL_OFFICER_1, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE, 0);
                gpuResetAndInvalidateModelBuffers();
                memConfigureImageMemory(gGameSession->location.stage, gGameSession->location.area);
                memSelectAuxHeapRegion(true);
                tmdResetAuxHeapAndRestoreBuffers();
                sndEvtRequestScriptVolume(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 5), ACROPOLIS_PLAZA_FINAL_AMBIENCE_VOLUME);
                task->state = task->state + 1;
                return;
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_OFFICER_CAMERA);
            return;
        case ACROPOLIS_PLAZA_FINAL_START_SECOND_STREAM:
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = ACROPOLIS_PLAZA_FINAL_CLOSING_STREAM;
            _acropolisPlazaResetStreamInBuffer(streamArgs, ACROPOLIS_PLAZA_FINAL_CLOSING_STREAM);
            queue->continueMovie = 0;
            task->state          = task->state + 1;
            /* fallthrough */
        case ACROPOLIS_PLAZA_FINAL_WAIT_SECOND_STREAM:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            evsStartScript(_gAcropolisPlazaFinalSceneOfficerScript, EVENT_SCRIPT_HUD_KEEP);
            work->elapsedFrames = 0;
            task->state         = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_TAIL_EFFECT:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= ACROPOLIS_PLAZA_FINAL_TAIL_EFFECT_FRAME) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 0x0B), 0, 0);
                taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_FINAL_FADE_IN_TASK, ACROPOLIS_PLAZA_FINAL_FADE_IN_FRAMES, 0);
                work->elapsedFrames = 0;
                task->state         = task->state + 1;
            }
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_DISPLAY:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 2) {
                SetDispMask(1);
                task->state = task->state + 1;
            }
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_TAIL_SCRIPT:
            if (padIsStartPressed() != 0) {
                stageMusicRequestAreaStop(ACROPOLIS_PLAZA_FINAL_SKIP_MUSIC_FADE_TICKS);
                cdCmdRequestCancel();
                task->state = ACROPOLIS_PLAZA_FINAL_WAIT_END;
            } else if (gGameSession->eventState == 0) {
                stageMusicRequestAreaStop(ACROPOLIS_PLAZA_FINAL_MUSIC_FADE_TICKS);
                queue->sceneFrame       = 1;
                queue->movieFrame       = 1;
                queue->plazaStreamSubId = ACROPOLIS_PLAZA_FINAL_CLOSING_STREAM;
                _acropolisPlazaQueueStreamAtStart(CD_COMMAND_PLAY_STREAM_AT_OFFSET, ACROPOLIS_PLAZA_FINAL_CLOSING_STREAM);
                queue->continueMovie = 1;
                task->state          = task->state + 1;
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_CLOSING_STREAM:
            if (queue->movieReady != 0) {
                evsStartScript(_gAcropolisPlazaFinalSceneClosingScript, EVENT_SCRIPT_HUD_KEEP);
                task->state = task->state + 1;
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        case ACROPOLIS_PLAZA_FINAL_WAIT_END:
            if (cdCmdIsIdle() != 0) {
                sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 2), ACROPOLIS_PLAZA_FINAL_AMBIENCE_STOP_TICKS);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
                taskRequestKill(task, 0);
            }
            _acropolisPlazaApplyStreamCamera(ACROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        default:
            return;
    }
}

#undef ACROPOLIS_PLAZA_INITIALIZE_EVENT_WORK

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
                        evsStartScript(_gAcropolisPlazaFirstRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 1:
                        evsStartScript(_gAcropolisPlazaSecondRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 2:
                        evsStartScript(_gAcropolisPlazaSubsequentRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
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

/// Stops plaza traversal and starts the CAP command selected by a caption trigger.
///
/// `spawnArg2.pointer` borrows live sequence work. Requires the player and its
/// equipped animation bank. After resetting the idle clip, waits for the CD
/// queue, saves the resume frame, kills traversal and requests CAP using the
/// signed low byte of `eventKind`. The next tick releases scripted control
/// and requests removal without waiting for CAP playback. The sequence work
/// must outlive this task; animation dispatch consumes its stack request.
static void _acropolisPlazaCaptionEventTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_CAPTION_INIT           = 0,
        ACROPOLIS_PLAZA_CAPTION_WAIT_STREAM    = 1,
        ACROPOLIS_PLAZA_CAPTION_END            = 2,
        ACROPOLIS_PLAZA_CAPTION_IDLE_ANIMATION = 1,
        ACROPOLIS_PLAZA_CAPTION_BLEND_FRAMES   = 10
    };

    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case ACROPOLIS_PLAZA_CAPTION_INIT:
            _acropolisPlazaPlayPlayerAnimation(ACROPOLIS_PLAZA_CAPTION_IDLE_ANIMATION, ANIMATION_BLEND_RESET, ACROPOLIS_PLAZA_CAPTION_BLEND_FRAMES);
            task->state = task->state + 1;
            break;
        case ACROPOLIS_PLAZA_CAPTION_WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                ((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
                taskKill(((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
                capRunCommandWithTransition((s8)((_AcropolisPlazaSequenceWork*)task->spawnArg2.pointer)->eventKind);
                task->state = task->state + 1;
            }
            break;
        case ACROPOLIS_PLAZA_CAPTION_END:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            taskRequestKill(task, 0);
            break;
    }
}

/// Requests a silent ambience start and returns from the enclosing voice update.
///
/// `startFlag` points to a writable u16 flag (0 idle, 1 start requested), evaluated
/// twice on startup. `soundId` is evaluated once per request; both arguments must be
/// stable and side-effect-free. Records the start even if queue admission fails.
/// A set flag continues the caller's envelope update on this invocation.
#define ACROPOLIS_PLAZA_START_AMBIENCE(startFlag, soundId)                                       \
    do {                                                                                         \
        enum { ACROPOLIS_PLAZA_AMBIENCE_SILENT_ATTENUATION = 127 };                              \
        if (*(startFlag) == false) {                                                             \
            sndEvtRequestScriptStart((soundId), 0, ACROPOLIS_PLAZA_AMBIENCE_SILENT_ATTENUATION); \
            sndEvtRequestScriptVolume((soundId), 0);                                             \
            *(startFlag) = true;                                                                 \
            return;                                                                              \
        }                                                                                        \
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
        ACROPOLIS_PLAZA_START_AMBIENCE(startRequested, soundId);
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

#undef ACROPOLIS_PLAZA_START_AMBIENCE

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
            _acropolisPlazaUpdateAmbienceVoice(1, ACROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME, 1, &work->ambience5Playing, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 5), ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _acropolisPlazaUpdateAmbienceVoice(1, ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END, 1, &work->ambience2Playing, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 2), ACROPOLIS_PLAZA_AMBIENCE_EARLY_FADE);
            _acropolisPlazaUpdateAmbienceVoice(ACROPOLIS_PLAZA_AMBIENCE4_FIRST_FRAME, ACROPOLIS_PLAZA_AMBIENCE4_LAST_FRAME, ACROPOLIS_PLAZA_AMBIENCE4_PEAK_FRAME, &work->ambience4Playing, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 4), ACROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _acropolisPlazaUpdateAmbienceVoice(ACROPOLIS_PLAZA_AMBIENCE1_FIRST_FRAME, ACROPOLIS_PLAZA_AMBIENCE1_LAST_FRAME, ACROPOLIS_PLAZA_AMBIENCE1_PEAK_FRAME, &work->ambience1Playing, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 1), ACROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE);
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
            sndEvtRequestScriptVolume(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 1), volume & 0xFF);
            break;
    }
}

/// Resumes the plaza scene task at the sequence's saved frame without seeking the stream.
///
/// The previous scene task must have ended. Borrows live sequence work and
/// stores the new task handle, including NULL on allocation failure. The child
/// reads the embedded argument during initialization, seeds both scene/movie
/// frame counters and clears the stream's end latch. Keep the work and loaded
/// stream/room resources alive through that handoff; no CD seek is requested.
static inline void _acropolisPlazaResumeSequenceStream(_AcropolisPlazaSequenceWork* work)
{
    enum { ACROPOLIS_PLAZA_RESUME_STREAM_TASK = ACROPOLIS_PLAZA_TASK_STREAMED_SCENE };

    work->sceneArg.skipStreamReset = 1;
    work->sceneArg.startFrame      = work->resumeFrame;
    work->sceneTask                = taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_RESUME_STREAM_TASK, 0, &work->sceneArg);
}

/// Advances the plaza's trigger-driven streamed sequence and reports final-scene completion.
///
/// Borrows the sequence task's live work and retains it in spawned event tasks.
/// Automatic triggers bypass the interaction-button gate. First-scene and
/// ordinary-event waits latch the trigger control and parameters; completion
/// unlinks one-shot triggers and resumes the scene at the saved frame. Repeat
/// events saturate at variant 2; caption events return to their prior wait.
/// Returns 1 only after the final event task ends, otherwise 0. The room,
/// stream resources, trigger storage and sequence work must outlive all children.
static u16 _acropolisPlazaUpdateSequenceEvent(Task* task)
{
    enum { ACROPOLIS_PLAZA_EVENT_TASK_STREAM_SCENE = ACROPOLIS_PLAZA_TASK_STREAM_SCENE,
           ACROPOLIS_PLAZA_EVENT_TASK_FINAL_SCENE  = ACROPOLIS_PLAZA_TASK_FINAL_SCENE,
           ACROPOLIS_PLAZA_EVENT_TASK_FIRST_SCENE  = ACROPOLIS_PLAZA_TASK_FIRST_SCENE,
           ACROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE = ACROPOLIS_PLAZA_TASK_REPEAT_SCENE,
           ACROPOLIS_PLAZA_EVENT_TASK_CAPTION      = ACROPOLIS_PLAZA_TASK_CAPTION,
           ACROPOLIS_PLAZA_REPEAT_FIRST_VARIANT    = 0,
           ACROPOLIS_PLAZA_REPEAT_SECOND_VARIANT   = 1,
           ACROPOLIS_PLAZA_REPEAT_LAST_VARIANT     = 2,
           ACROPOLIS_PLAZA_SCENE_RUNNING           = 0,
           ACROPOLIS_PLAZA_SCENE_COMPLETE          = 1 };

    CdCmdQueue*                  queue = &gCdCmdQueue;
    _AcropolisPlazaSequenceWork* work  = task->work;
    u16                          eventControl;
    u8                           eventKind;
    u8                           eventParameter1;
    s32                          firstSceneResult;
    s32                          sceneResult;
    s32                          repeatOrCaptionResult;
    s16                          eventReady;
    u16                          completedStep;
    s32                          signedEventKind;
    u32                          latchedKind;
    u16                          latchedKind16;

    // Manual triggers require a fresh interaction press.
    // The target reads control even on a failed query; readiness still gates actions.
    eventReady = worldCollisionReadActionHit(&eventControl, &eventKind, &eventParameter1);
    if (!((s16)eventControl & WORLD_COLLISION_TRIGGER_AUTOMATIC) && (eventReady != 0)) {
        eventReady = gPlayerStatus.interactionPressed != 0;
    }

    switch (work->step) {
        case ACROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                if ((s8)eventKind == ACROPOLIS_PLAZA_EVENT_FIRST_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_FIRST_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if ((s8)eventKind >= ACROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_CAPTION, 0, work);
                    work->step = ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION;
                }
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_FIRST_SCENE:
            if (taskPollKill(work->eventTask, &firstSceneResult) != 0) {
                _acropolisPlazaResumeSequenceStream(work);
                worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAcropolisPlazaFirstSceneTrigger);
                work->step = work->step + 1;
            }
            break;
        case ACROPOLIS_PLAZA_STEP_AWAIT_EVENT:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                work->resumeFrame     = queue->sceneFrame;
                signedEventKind       = (s8)eventKind;
                if (signedEventKind == ACROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_STREAM_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_FINAL_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == ACROPOLIS_PLAZA_EVENT_REPEAT_SCENE) {
                    if (work->repeatVariant == ACROPOLIS_PLAZA_REPEAT_FIRST_VARIANT) {
                        work->eventTask =
                            taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, ACROPOLIS_PLAZA_REPEAT_FIRST_VARIANT, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else if (work->repeatVariant == ACROPOLIS_PLAZA_REPEAT_SECOND_VARIANT) {
                        work->eventTask =
                            taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, ACROPOLIS_PLAZA_REPEAT_SECOND_VARIANT, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else {
                        work->eventTask =
                            taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, ACROPOLIS_PLAZA_REPEAT_LAST_VARIANT, work);
                        work->step = ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    }
                } else if (signedEventKind >= ACROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_EVENT_TASK_CAPTION, 0, work);
                    work->step = ACROPOLIS_PLAZA_STEP_RUN_CAPTION;
                }
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_SCENE:
            if (taskPollKill(work->eventTask, &sceneResult) != 0) {
                // Keep the signed-byte extension and unsigned-halfword comparison widths.
                latchedKind   = work->eventKind;
                latchedKind16 = (s8)latchedKind;
                if (latchedKind16 == ACROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    worldCollisionUnlinkTrigger(0, &_gAcropolisPlazaStreamSceneTrigger);
                } else if ((u16)(s8)latchedKind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, _gAcropolisPlazaFinalSceneAndInteractionTriggers);
                }
                work->step = work->step - 1;
                if ((s8)work->eventKind == ACROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    return ACROPOLIS_PLAZA_SCENE_COMPLETE;
                }
                _acropolisPlazaResumeSequenceStream(work);
            }
            break;
        case ACROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE:
        case ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION:
        case ACROPOLIS_PLAZA_STEP_RUN_CAPTION:
            if (taskPollKill(work->eventTask, &repeatOrCaptionResult) != 0) {
                _acropolisPlazaResumeSequenceStream(work);
                completedStep = work->step;
                if (completedStep == ACROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION) {
                    work->step = ACROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE;
                } else {
                    if (completedStep != ACROPOLIS_PLAZA_STEP_RUN_CAPTION) {
                        if (work->repeatVariant < ACROPOLIS_PLAZA_REPEAT_LAST_VARIANT) {
                            work->repeatVariant = work->repeatVariant + 1;
                        }
                    }
                    work->step = ACROPOLIS_PLAZA_STEP_AWAIT_EVENT;
                }
            }
            break;
    }
    return ACROPOLIS_PLAZA_SCENE_RUNNING;
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

/// Records the elevator arrival selectors and the destination's sprite variant.
///
/// Requires the live save block and display state; the reload consumes them.
static inline void _acropolisPlazaCommitElevatorDestination(void)
{
    enum {
        ACROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP   = 1,
        ACROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM   = 1,
        ACROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT = 1,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                 = ACROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT;
}

/// Runs the plaza's streamed traversal and triggered scenes before leaving for the elevator hall.
///
/// Starts bodyless in state 0, allocates and clears owned sequence work, and
/// ends immediately on allocation failure. Two frame delays precede stream setup.
/// The stream and event children borrow the embedded scene arguments/work;
/// keep room resources loaded until they finish. The running state updates ambience
/// and events, then commits west-elevator-hall warp/room 1 and queues a reload.
/// Game pausing is blocked only from stream setup through the departure handoff.
static void _acropolisPlazaSequenceTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_SEQUENCE_INITIALIZE               = 0,
        ACROPOLIS_PLAZA_SEQUENCE_WAIT_FIRST_FRAME         = 1,
        ACROPOLIS_PLAZA_SEQUENCE_WAIT_SECOND_FRAME        = 2,
        ACROPOLIS_PLAZA_SEQUENCE_START_STREAM             = 3,
        ACROPOLIS_PLAZA_SEQUENCE_RUN                      = 4,
        ACROPOLIS_PLAZA_SEQUENCE_LEAVE                    = 5,
        ACROPOLIS_PLAZA_SEQUENCE_STREAM_TASK_INDEX        = ACROPOLIS_PLAZA_TASK_STREAMED_SCENE,
        ACROPOLIS_PLAZA_SEQUENCE_DISPLAY_SETUP_TASK_INDEX = ACROPOLIS_PLAZA_TASK_DISPLAY_SETUP,
        ACROPOLIS_PLAZA_SEQUENCE_FADE_TASK_INDEX          = ACROPOLIS_PLAZA_TASK_FADE_IN,
        ACROPOLIS_PLAZA_SEQUENCE_LETTERBOX_TASK_INDEX     = ACROPOLIS_PLAZA_TASK_LETTERBOX,
        ACROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL            = 0x370,
        ACROPOLIS_PLAZA_SEQUENCE_FADE_MODE                = 6,
    };
    CdCmdQueue*                  queue = &gCdCmdQueue;
    _AcropolisPlazaSequenceWork* work  = task->work;
    _AcropolisPlazaSequenceWork* allocatedWork;
    _AcropolisPlazaSequenceWork* initializedWork;
    Task*                        childTask;
    SVECTOR                      ambientColor;

    switch (task->state) {
        case ACROPOLIS_PLAZA_SEQUENCE_INITIALIZE:
            // Hold input while presentation and the owned work block are prepared.
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU & ~PAD_BUTTON_CIRCLE);
            viewApplyCamera(&_gAcropolisPlazaTraversalCameraSamples[0]);
            allocatedWork = memMalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
            childTask                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            initializedWork             = task->work;
            initializedWork->playerTask = childTask;
            childTask                   = taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_SEQUENCE_DISPLAY_SETUP_TASK_INDEX, 0, 0);
            initializedWork             = task->work;
            initializedWork->eventTask  = childTask;
            playerActorRemoveEquipment();
            taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_SEQUENCE_LETTERBOX_TASK_INDEX, 0, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_SEQUENCE_WAIT_FIRST_FRAME:
        case ACROPOLIS_PLAZA_SEQUENCE_WAIT_SECOND_FRAME:
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_SEQUENCE_START_STREAM:
            // Children borrow sceneArg from this work until the sequence finishes.
            ambientColor.vx = ACROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            ambientColor.vy = ACROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            ambientColor.vz = ACROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            worldCoordSetAmbientColorOverride(&ambientColor);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->sceneArg.skipStreamReset = 0;
            work->sceneArg.startFrame      = 0;
            work->sceneTask                = taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_SEQUENCE_STREAM_TASK_INDEX, 0, &work->sceneArg);
            stageMusicRequestAreaStart(0);
            taskSpawnFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_SEQUENCE_FADE_TASK_INDEX, ACROPOLIS_PLAZA_SEQUENCE_FADE_MODE, 0);
            queue->blockGamePause = 1;
            task->state           = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_SEQUENCE_RUN:
            _acropolisPlazaUpdateSceneAmbience(task);
            if (_acropolisPlazaUpdateSequenceEvent(task) == 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PLAZA_SEQUENCE_LEAVE:
            // Commit the elevator destination before queuing the gameplay reload.
            _acropolisPlazaCommitElevatorDestination();
            loadingEnqueueEquippedWeaponResources();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            queue->blockGamePause = 0;
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
    enum { ACROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX = ACROPOLIS_PLAZA_TASK_MOVIE_DISPLAY };
    displaySpawnTaskFromTable(gAcropolisPlazaSequenceTaskDescriptors, ACROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX, 0, 0);
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
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 12]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAcropolisPlazaLightRingSamples[index + 4]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAcropolisPlazaLightRingSamples[index]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 8]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 4]) >> (ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)flare->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAcropolisPlazaLightRingSamples[index + 8]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAcropolisPlazaLightRingSamples[index + 4]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 12]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAcropolisPlazaLightRingSamples[index + 8]) >> ACROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
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

void acropolisPlazaSpawnAmbientLightsTask(Task* task)
{
    enum {
        ACROPOLIS_PLAZA_AMBIENT_LIGHTS_INITIAL = 0,
        ACROPOLIS_PLAZA_AMBIENT_GLOW_FIRST     = 12,
        ACROPOLIS_PLAZA_AMBIENT_GLOW_END       = ARRAY_SIZE(_gAcropolisPlazaAmbientLightPlacements),
        ACROPOLIS_PLAZA_AMBIENT_FLARE_FIRST    = 7,
        ACROPOLIS_PLAZA_AMBIENT_FLARE_END      = 11,
        ACROPOLIS_PLAZA_AMBIENT_SIREN_FIRST    = 1,
        ACROPOLIS_PLAZA_AMBIENT_SIREN_END      = 7,
    };
    GfxCoord* roomCoord;
    s32       placementIndex;

    roomCoord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_PLAZA_AMBIENT_LIGHTS_INITIAL) {
        for (placementIndex = ACROPOLIS_PLAZA_AMBIENT_GLOW_FIRST; placementIndex < ACROPOLIS_PLAZA_AMBIENT_GLOW_END; placementIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_LIGHT_GLOW, roomCoord, placementIndex, &_gAcropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = ACROPOLIS_PLAZA_AMBIENT_FLARE_FIRST; placementIndex < ACROPOLIS_PLAZA_AMBIENT_FLARE_END; placementIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_LIGHT_FLARE, roomCoord, placementIndex, &_gAcropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = ACROPOLIS_PLAZA_AMBIENT_SIREN_FIRST; placementIndex < ACROPOLIS_PLAZA_AMBIENT_SIREN_END; placementIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PLAZA_SIREN_LIGHT, roomCoord, placementIndex, &_gAcropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        task->state = task->state + 1;
    }
}
