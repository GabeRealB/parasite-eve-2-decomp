#include "rooms/akropolis_plaza.h"

#include "config.h"
#include "../../room_ids.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/libcd.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_310100.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
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

#include "overlay.h"
#include "rooms/room_common.h"
#include "../../../../shared/screen/screen_fade.h"
#include "../../../../shared/screen/screen_fade_step_up.inc.c"
#include "../../../../shared/screen/screen_fade_tile_overlay.inc.c"

#include "assets/rooms/akropolis/plaza/declarations.inc"

/// Screen geometry shared by the plaza's fade and cinematic bars, in pixels.
enum {
    AKROPOLIS_PLAZA_SCREEN_WIDTH     = 320,
    AKROPOLIS_PLAZA_SCREEN_HEIGHT    = 240,
    AKROPOLIS_PLAZA_LETTERBOX_HEIGHT = 24
};

/// Projection, sweep and fan units of the plaza's placed lights.
enum {
    AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH                 = 17,  // Camera Z / 4
    AKROPOLIS_PLAZA_LIGHT_CULL_X                    = 192, // Absolute center X in screen pixels; exclusive
    AKROPOLIS_PLAZA_LIGHT_CULL_Y                    = 152, // Absolute center Y in screen pixels; exclusive
    AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT                = 12,  // Fan samples are Q12
    AKROPOLIS_PLAZA_LIGHT_TURN                      = 0x1000,
    AKROPOLIS_PLAZA_LIGHT_HALF_TURN                 = 0x800,
    AKROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT           = 11,
    AKROPOLIS_PLAZA_LIGHT_YAW_STEP                  = 0x80,
    AKROPOLIS_PLAZA_LIGHT_YAW_MASK                  = AKROPOLIS_PLAZA_LIGHT_TURN - 1,
    AKROPOLIS_PLAZA_LIGHT_SHORT_REACH               = 0x200, // Local +Z world units
    AKROPOLIS_PLAZA_LIGHT_LONG_REACH                = 0x800,
    AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z                = 0xE00,
    AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW       = 0x300,
    AKROPOLIS_PLAZA_LIGHT_FAN_SAMPLES               = 16, // One turn; each wedge spans two samples
    AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR = 0x10000,
    AKROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY    = 0x80,
    AKROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK    = 0x7F,
    AKROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD         = 0x80,
    AKROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK          = 0x7F,
};

/// Translates one rotated light vertex into world space, narrowing at each store.
#define AKROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(vertex, coordinate) \
    (vertex)->vx += (u16)(coordinate)->workm.t[0];                 \
    (vertex)->vy += (u16)(coordinate)->workm.t[1];                 \
    (vertex)->vz += (u16)(coordinate)->workm.t[2]

/// Queues the eight Gouraud wedges of one additive light glow.
#define AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(center, radius, depth, primitive, sampleIndex, red, green, blue)                                    \
    for ((sampleIndex) = 0; (sampleIndex) < AKROPOLIS_PLAZA_LIGHT_FAN_SAMPLES; (sampleIndex) += 2) {                                            \
        (primitive)    = gGpuPrimCursor;                                                                                                        \
        gGpuPrimCursor = (primitive) + 1;                                                                                                       \
        setPolyG4((primitive));                                                                                                                 \
        setRGB0((primitive), 0, 0, 0);                                                                                                          \
        setRGB1((primitive), 0, 0, 0);                                                                                                          \
        setRGB2((primitive), (red), (green), (blue));                                                                                           \
        setRGB3((primitive), 0, 0, 0);                                                                                                          \
        (primitive)->x0 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 4]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT); \
        (primitive)->y0 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex)]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);     \
        (primitive)->x1 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 5]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT); \
        (primitive)->y1 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 1]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT); \
        (primitive)->x2 = (center).vx;                                                                                                          \
        (primitive)->y2 = (center).vy;                                                                                                          \
        (primitive)->x3 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 6]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT); \
        (primitive)->y3 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 2]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT); \
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), (primitive));                                                                         \
        gpuSetPrimitiveBlendMode((primitive), GPU_BLEND_ADD, (depth));                                                                          \
    }

/// Index of one vertex in `_AkropolisPlazaBeamScratch`.
enum {
    AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE,    // Beacon origin and the beam's apex; replaced by the far glow's center
    AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT,  // Far edge of the beam, at the beam's current length
    AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT,
    AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT, // Widest point of the fringe beside the beam
    AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT,
    AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT, // Where the fringe leaves the apex
    AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT,
    AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT
};

/// Scratch-stack workspace for one frame of a rotating beacon's light beam.
typedef struct {
    s32     vertexScreenPos[AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT]; // Projected outline vertices as packed screen-XY words
    s32     otz;                                                // Projected depth (SZ3 / 4) of the center; the ordering-table depth, blend depth and divisor for `glowRadius`
    s32     glowRadius;                                         // On-screen radius in pixels of the wedge fan round the center: a world size divided by `otz`
    SVECTOR vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT];        // Beam vertices: staged in the beacon's frame, then the world positions supplied to the projection
    DVECTOR centerScreenPos;                                    // Projected center in screen pixels, stored as one GTE word
} _AkropolisPlazaBeamScratch;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaBeamScratch, 0x60);

/// Scratch-stack workspace for one frame of a light flare.
typedef struct {
    u8      unused0[0x14];  // Reserved with the block but never accessed; role unproven
    s32     otz;            // Projected depth (SZ3 / 4) of the center; the ordering-table depth, blend depth and divisor for both radii
    s32     outerRadius;    // On-screen radius in pixels of the wedge fan, then of a short ray's tip
    s32     innerRadius;    // On-screen distance in pixels from the center to a short ray's shoulders
    SVECTOR worldPos;       // Centre in world coordinates, the input to the projection; staged in the flare's frame for the far glow
    u8      unused28[0x20]; // Reserved with the block but never accessed; role unproven
    DVECTOR screenPos;      // Projected center in screen pixels, stored as one GTE word
} _AkropolisPlazaFlareScratch;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaFlareScratch, 0x4C);

/// Spawn argument of the plaza's streamed-scene task.
typedef struct {
    u16 startFrame;      // Scene frame to resume at; seeds both `CdCmdQueue::sceneFrame` and `movieFrame`
    u16 skipStreamReset; // 0 resets the stream to the frame before `startFrame`; non-zero only clears `movieAtEnd`
} _AkropolisPlazaSceneArg;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaSceneArg, 0x4);

/// `GameActor.movementMode` value this plaza treats as running.
enum { AKROPOLIS_PLAZA_MOVEMENT_RUNNING = 3 };

/// Edge flag on the streamed-scene work.
enum {
    AKROPOLIS_PLAZA_EDGE_NONE = 0,
    AKROPOLIS_PLAZA_EDGE_WALK = 1,
    AKROPOLIS_PLAZA_EDGE_RUN  = 2
};

/// `relX` margins, in world units, and the scene frame with no earlier shot.
enum {
    AKROPOLIS_PLAZA_FORWARD_EDGE      = 0xC9,
    AKROPOLIS_PLAZA_BACK_EDGE         = -0x14,
    AKROPOLIS_PLAZA_FIRST_SCENE_FRAME = 1
};

/// Seek scale stored in `frameStep`, and the stream frames one movie frame spans.
enum {
    AKROPOLIS_PLAZA_FRAME_STEP_FINE    = 1,
    AKROPOLIS_PLAZA_FRAME_STEP_COARSE  = 2,
    AKROPOLIS_PLAZA_SEEK_FRAMES_FINE   = 10,
    AKROPOLIS_PLAZA_SEEK_FRAMES_COARSE = 40
};

/// Work block for the plaza's streamed-scene task, kept in `Task::work`.
typedef struct {
    MATRIX*    playerMtx;    // Borrowed player root matrix; valid while the player actor is live
    Task*      playerTask;   // Borrowed player task, cached once the CD is idle
    GameActor* player;       // That task's actor; running is `AKROPOLIS_PLAZA_MOVEMENT_RUNNING`
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
} _AkropolisPlazaSceneWork;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaSceneWork, 0x34);

/// Steps of the plaza sequence's event handling, kept in `_AkropolisPlazaSequenceWork::step`.
enum {
    AKROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE = 0, // Before the first scene: accepts it or a caption
    AKROPOLIS_PLAZA_STEP_RUN_FIRST_SCENE   = 1, // First scene running; its trigger is unlinked when it ends
    AKROPOLIS_PLAZA_STEP_AWAIT_EVENT       = 2, // Accepts every event kind except the first scene
    AKROPOLIS_PLAZA_STEP_RUN_SCENE         = 3, // Stream or final scene running; the final one ends the sequence
    AKROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE  = 4, // Repeat scene running; advances `repeatVariant` when it ends
    AKROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION = 5, // Caption running; returns to AWAIT_FIRST_SCENE
    AKROPOLIS_PLAZA_STEP_RUN_CAPTION       = 6  // Caption running; returns to AWAIT_EVENT
};

/// Event kinds of the plaza sequence: `WorldCollisionTrigger::parameter0` of the room's action triggers, latched in
/// `_AkropolisPlazaSequenceWork::eventKind`.
enum {
    AKROPOLIS_PLAZA_EVENT_STREAM_SCENE  = 0, // Scene with its own movie stream; its trigger is unlinked afterwards
    AKROPOLIS_PLAZA_EVENT_FIRST_SCENE   = 1, // The only scene accepted first; its trigger is unlinked afterwards
    AKROPOLIS_PLAZA_EVENT_FINAL_SCENE   = 2, // Ends the sequence and leaves the room
    AKROPOLIS_PLAZA_EVENT_REPEAT_SCENE  = 3, // Runs one of three scripts, selected by `repeatVariant`
    AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION = 6  // This kind and every higher one is a CAP command ID
};

/// Work block of the plaza's sequence task, kept in `Task::work`.
typedef struct {
    byte                    field_0[4];       // No access established; role unproven
    Task*                   playerTask;       // Borrowed player task, cached on the first frame and not read back
    Task*                   sceneTask;        // Running streamed-scene task; an event task kills it at its stream handoff
    Task*                   eventTask;        // Task answering the latched event, polled until killed; first holds the unpolled display-setup task
    _AkropolisPlazaSceneArg sceneArg;         // Spawn argument of `sceneTask`, refilled before every spawn
    u16                     step;             // Event-handling step (`AKROPOLIS_PLAZA_STEP_*`)
    u16                     eventControl;     // Latched trigger `control` word; stored and not read back
    u8                      eventKind;        // Latched trigger `parameter0` (`AKROPOLIS_PLAZA_EVENT_*`); read back as a signed byte
    u8                      eventParameter1;  // Latched trigger `parameter1`; stored and not read back
    u16                     resumeFrame;      // Scene frame latched when the scene was interrupted; the respawned scene starts there
    u16                     repeatVariant;    // Script the next repeat scene runs (0..2); advances after each and stays at 2
    u16                     ambience5Playing; // Non-zero while area sound 5 is started; the three below likewise
    u16                     ambience1Playing; // Area sound 1
    u16                     ambience2Playing; // Area sound 2
    u16                     ambience4Playing; // Area sound 4
} _AkropolisPlazaSequenceWork;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaSequenceWork, 0x28);

/// Work block of the plaza's player-scripting event tasks, kept in `Task::work`.
typedef struct {
    Task* playerTask;    // Borrowed player task the scripted-control messages are sent to; set once on the first frame
    s16   elapsedFrames; // Frames counted in the current timed wait; restarted from 0 when one begins
} _AkropolisPlazaEventWork;
STATIC_ASSERT_SIZEOF(_AkropolisPlazaEventWork, 0x8);

static void _akropolisPlazaFadeOutTask(Task* task);
static void _akropolisPlazaMovieDisplayTask(Task* task);
static void _akropolisPlazaStreamedSceneTask(Task* task);
static void _akropolisPlazaFirstSceneTask(Task* task);
static void _akropolisPlazaStreamSceneTask(Task* task);
static void _akropolisPlazaFinalSceneTask(Task* task);
static void _akropolisPlazaRepeatSceneTask(Task* task);
static void _akropolisPlazaCaptionEventTask(Task* task);
static void _akropolisPlazaLetterboxTask(Task* task);
static void _akropolisPlazaSequenceTask(Task* task);
static void _akropolisPlazaStartMovieDisplayTask(Task* task);

enum { AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY = 192 };

#include "assets/rooms/akropolis/plaza/events.inc"

TaskDesc gAkropolisPlazaSequenceTaskDescriptors[AKROPOLIS_PLAZA_TASK_COUNT] = {
    [AKROPOLIS_PLAZA_TASK_SEQUENCE]       = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaSequenceTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_STREAMED_SCENE] = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaStreamedSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_STREAM_SCENE]   = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaStreamSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FINAL_SCENE]    = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaFinalSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FIRST_SCENE]    = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaFirstSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_DISPLAY_SETUP]  = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaStartMovieDisplayTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_REPEAT_SCENE]   = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaRepeatSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FADE_OUT]       = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaFadeOutTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FADE_IN]        = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, screenFadeInTileTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_CAPTION]        = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaCaptionEventTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_MOVIE_DISPLAY]  = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaMovieDisplayTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_LETTERBOX]      = { { { TASK_BODY_NONE, AKROPOLIS_PLAZA_SEQUENCE_TASK_PRIORITY } }, _akropolisPlazaLetterboxTask, { .value = 0 } },
};

/// Stored camera-path lengths in scene frames; each frame has two consecutive samples.
enum {
    AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME         = 2,
    AKROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT     = 400,
    AKROPOLIS_PLAZA_CAMERA_STREAM_SCENE_FRAME_COUNT  = 120,
    AKROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT = 150,
    AKROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT = 16,
    AKROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT = 50
};

/// Retained sample-count header of the traversal camera path.
static s32 _gAkropolisPlazaTraversalCameraSampleCount = AKROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Traversal camera path shared by selectors 0..3.
static ViewCamera _gAkropolisPlazaTraversalCameraSamples[AKROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/akropolis_plaza_path_062F8.inc"
};

/// Camera path for the stream-scene event (camera selector 4).
static ViewCamera _gAkropolisPlazaStreamSceneCameraSamples[AKROPOLIS_PLAZA_CAMERA_STREAM_SCENE_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/akropolis_plaza_path_0D378.inc"
};

/// Retained sample-count header of the closing-scene camera path.
static s32 _gAkropolisPlazaClosingSceneCameraSampleCount = AKROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Closing camera path of the final scene (camera selector 5).
static ViewCamera _gAkropolisPlazaClosingSceneCameraSamples[AKROPOLIS_PLAZA_CAMERA_CLOSING_SCENE_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/akropolis_plaza_path_0F53C.inc"
};

/// Retained sample-count header of the final lead-in camera path.
static s32 _gAkropolisPlazaFinalSceneLeadInCameraSampleCount = AKROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Lead-in camera path of the final scene (camera selector 6).
static ViewCamera _gAkropolisPlazaFinalSceneLeadInCameraSamples[AKROPOLIS_PLAZA_CAMERA_FINAL_LEAD_IN_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/akropolis_plaza_path_11F70.inc"
};

/// Retained sample-count header of the officer-scene camera path.
static s32 _gAkropolisPlazaOfficerSceneCameraSampleCount = AKROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;

/// Officer camera path of the final scene (camera selector 7).
static ViewCamera _gAkropolisPlazaOfficerSceneCameraSamples[AKROPOLIS_PLAZA_CAMERA_OFFICER_SCENE_FRAME_COUNT * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/akropolis_plaza_path_123F4.inc"
};

/// World-space shot reference points for the streamed traversal's collision walls and edge tests.
static VECTOR3 _gAkropolisPlazaTraversalShotPositions[AKROPOLIS_PLAZA_CAMERA_TRAVERSAL_FRAME_COUNT] = {
#include "assets/akropolis_plaza_path_13204.inc"
};

#include "assets/rooms/akropolis/plaza/clips.inc"
#include "assets/rooms/akropolis/plaza/samples.inc"
#include "assets/rooms/akropolis/plaza/markers.inc"
#define ROOM_PART gAkropolisPlazaRoomCollisionResources
#include "assets/rooms/akropolis/plaza/header.inc"
#include "assets/rooms/akropolis/plaza/cameras.inc"
#include "assets/rooms/akropolis/plaza/sprites.inc"
#define ROOM_PART gAkropolisPlazaWarpEntries
#include "assets/rooms/akropolis/plaza/header.inc"
#include "assets/rooms/akropolis/plaza/geometry.inc"
#include "assets/rooms/akropolis/plaza/action_triggers.inc"
#include "assets/rooms/akropolis/plaza/placements.inc"
#include "assets/rooms/akropolis/plaza/lights.inc"
#include "assets/rooms/akropolis/plaza/surfaces.inc"

/// Envelope selectors and byte-volume units of the plaza ambience driver.
enum {
    AKROPOLIS_PLAZA_AMBIENCE_TRIANGLE        = 0,
    AKROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE  = 1,
    AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE      = 2,
    AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME     = 127,
    AKROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME     = 95,
    AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE = 84,
    AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END  = 130
};

static void        _akropolisPlazaUpdateSceneCollisionWalls(Task* sceneTask);
static void        _akropolisPlazaApplyStreamCamera(s32 subId);
static inline void _akropolisPlazaUpdateSceneEdgeFlags(_AkropolisPlazaSceneWork* work);
static void        _akropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope);
static void        _akropolisPlazaUpdateSceneAmbience(Task* task);

void akropolisPlazaPollStreamCommands(void)
{
    enum {
        AKROPOLIS_PLAZA_STREAM_COMMAND_INIT = 0,
        AKROPOLIS_PLAZA_STREAM_COMMAND_POLL = 1,
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
                        case AKROPOLIS_PLAZA_STREAM_COMMAND_INIT:
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
                        case AKROPOLIS_PLAZA_STREAM_COMMAND_POLL:
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

/// Fades the screen to black with a rising subtractive tile, then blanks the display. `spawnArg1` is the step per
/// update.
static void _akropolisPlazaFadeOutTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_FADE_OUT_INITIALIZE    = 0,
        AKROPOLIS_PLAZA_FADE_OUT_RAMP          = 1,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case AKROPOLIS_PLAZA_FADE_OUT_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                break;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            /* fallthrough */
        case AKROPOLIS_PLAZA_FADE_OUT_RAMP:
            _screenFadeDrawTileOverlay(fade);
            _screenFadeStepUp(fade, task);
            if (fade->r >= SCREEN_FADE_OUT_COMPLETE) {
                SetDispMask(0);
                taskKill(task);
            }
            break;
    }
}

#include "../../../../shared/screen/screen_fade_in_tile.inc.c"

/// Plays the plaza display movie, allowing Start to cancel, then restores game graphics.
static void _akropolisPlazaMovieDisplayTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_MOVIE_SAVE_GRAPHICS     = 0,
        AKROPOLIS_PLAZA_MOVIE_START             = 1,
        AKROPOLIS_PLAZA_MOVIE_WAIT_READY        = 2,
        AKROPOLIS_PLAZA_MOVIE_WAIT_END          = 3,
        AKROPOLIS_PLAZA_MOVIE_WAIT_CANCEL       = 4,
        AKROPOLIS_PLAZA_MOVIE_BEGIN_RESTORE     = 5,
        AKROPOLIS_PLAZA_MOVIE_WAIT_RESTORE      = 6,
        AKROPOLIS_PLAZA_DISPLAY_MOVIE_STREAM_ID = 100,
    };
    u8              streamArgs[4];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case AKROPOLIS_PLAZA_MOVIE_SAVE_GRAPHICS:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case AKROPOLIS_PLAZA_MOVIE_START:
            movieLocation      = gGameSession->location;
            movieLocation.view = AKROPOLIS_PLAZA_DISPLAY_MOVIE_STREAM_ID;
            streamArgs[0]      = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case AKROPOLIS_PLAZA_MOVIE_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case AKROPOLIS_PLAZA_MOVIE_WAIT_END:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed() != 0) {
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case AKROPOLIS_PLAZA_MOVIE_WAIT_CANCEL:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            }
            break;
        case AKROPOLIS_PLAZA_MOVIE_BEGIN_RESTORE:
            streamResetGameRestore();
            task->state++;
            break;
        case AKROPOLIS_PLAZA_MOVIE_WAIT_RESTORE:
            if (streamPollGameRestore(1, 0) & 0xFFFF) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Moves the two opposing collision walls with the streamed scene's shot position.
static void _akropolisPlazaUpdateSceneCollisionWalls(Task* sceneTask)
{
    _AkropolisPlazaSceneWork* work  = sceneTask->work;
    s32                       shotX = work->shotPos.vx;
    s32                       shotY = work->shotPos.vy;
    s32                       shotZ = work->shotPos.vz;
    s16                       leftX = shotX - 0xBB8;
    s16                       topY;
    s16                       minZ;
    s16                       maxZ;
    s16                       rightX;

    _gAkropolisPlazaCollisionGridVerts[0].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[1].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[2].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[3].vx = leftX;

    topY   = shotY + 0x3E8;
    minZ   = shotZ - 0x1000;
    maxZ   = shotZ + 0x3000;
    rightX = shotX + 0x7D0;

    _gAkropolisPlazaCollisionGridVerts[0].vy = shotY;
    _gAkropolisPlazaCollisionGridVerts[1].vy = shotY;
    _gAkropolisPlazaCollisionGridVerts[2].vy = topY;
    _gAkropolisPlazaCollisionGridVerts[3].vy = topY;

    _gAkropolisPlazaCollisionGridVerts[0].vz = minZ;
    _gAkropolisPlazaCollisionGridVerts[1].vz = maxZ;
    _gAkropolisPlazaCollisionGridVerts[2].vz = minZ;
    _gAkropolisPlazaCollisionGridVerts[3].vz = maxZ;

    _gAkropolisPlazaCollisionGridVerts[4].vx = rightX;
    _gAkropolisPlazaCollisionGridVerts[5].vx = rightX;
    _gAkropolisPlazaCollisionGridVerts[6].vx = rightX;
    _gAkropolisPlazaCollisionGridVerts[7].vx = rightX;

    _gAkropolisPlazaCollisionGridVerts[4].vy = shotY;
    _gAkropolisPlazaCollisionGridVerts[5].vy = shotY;
    _gAkropolisPlazaCollisionGridVerts[6].vy = topY;
    _gAkropolisPlazaCollisionGridVerts[7].vy = topY;

    _gAkropolisPlazaCollisionGridVerts[4].vz = maxZ;
    _gAkropolisPlazaCollisionGridVerts[5].vz = minZ;
    _gAkropolisPlazaCollisionGridVerts[6].vz = maxZ;
    _gAkropolisPlazaCollisionGridVerts[7].vz = minZ;
}

/// Applies one sample from the selected plaza camera path.
static void _akropolisPlazaApplyStreamCamera(s32 subId)
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
            cameraSamples = _gAkropolisPlazaTraversalCameraSamples;
            if (queue->movieReady == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;
            } else if (queue->reverseSceneFrames == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME + queue->movieFrameSubstep;
            } else {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME - queue->movieFrameSubstep;
            }
            viewApplyCamera(camera);
            return;
        case 4:
            cameraSamples = _gAkropolisPlazaStreamSceneCameraSamples;
            break;
        case 5:
            cameraSamples = _gAkropolisPlazaClosingSceneCameraSamples;
            break;
        case 6:
            cameraSamples = _gAkropolisPlazaFinalSceneLeadInCameraSamples;
            break;
        case 7:
            cameraSamples = _gAkropolisPlazaOfficerSceneCameraSamples;
            break;
    }
    if (queue->movieReady == 0) {
        s32 sceneIndex = queue->sceneFrame - 1;
        camera         = cameraSamples + sceneIndex * AKROPOLIS_PLAZA_CAMERA_SAMPLES_PER_FRAME;
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
static inline void _akropolisPlazaUpdateSceneEdgeFlags(_AkropolisPlazaSceneWork* work)
{
    CdCmdQueue* queue     = &gCdCmdQueue;
    s32         relativeX = work->relX;

    work->fwd  = AKROPOLIS_PLAZA_EDGE_NONE;
    work->back = AKROPOLIS_PLAZA_EDGE_NONE;
    if (relativeX >= AKROPOLIS_PLAZA_FORWARD_EDGE) {
        if ((u16)work->player->movementMode == AKROPOLIS_PLAZA_MOVEMENT_RUNNING) {
            work->fwd = AKROPOLIS_PLAZA_EDGE_RUN;
        } else {
            work->fwd = AKROPOLIS_PLAZA_EDGE_WALK;
        }
    } else if (relativeX < AKROPOLIS_PLAZA_BACK_EDGE) {
        if (queue->sceneFrame != AKROPOLIS_PLAZA_FIRST_SCENE_FRAME) {
            if ((u16)work->player->movementMode == AKROPOLIS_PLAZA_MOVEMENT_RUNNING) {
                work->back = AKROPOLIS_PLAZA_EDGE_RUN;
            } else {
                work->back = AKROPOLIS_PLAZA_EDGE_WALK;
            }
        }
    }
}

/// Tracks the player through the streamed plaza scene and seeks when a shot edge is crossed.
static void _akropolisPlazaStreamedSceneTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_SCENE_INIT          = 0,
        AKROPOLIS_PLAZA_SCENE_WAIT_IDLE     = 1,
        AKROPOLIS_PLAZA_SCENE_CHECK_EDGES   = 2,
        AKROPOLIS_PLAZA_SCENE_WAIT_READY    = 3,
        AKROPOLIS_PLAZA_SCENE_FOLLOW_EDGE   = 4,
        AKROPOLIS_PLAZA_SCENE_WAIT_SEEK_END = 5,
        AKROPOLIS_PLAZA_SCENE_AT_END        = 6,
    };
    u8                        streamArgs[4];
    s32                       sectorOffset;
    u32                       packedSeekOffset;
    u32                       packedOpeningOffset;
    s32                       lowByteMask = 0xFF;
    s32                       reverse;
    _AkropolisPlazaSceneWork* allocatedWork;
    CdCmdQueue*               queue;
    _AkropolisPlazaSceneWork* work;
    _AkropolisPlazaSceneArg*  spawnArg;
    Task*                     playerTask;
    u16                       startFrame;

    queue = &gCdCmdQueue;
    work  = task->work;

    if (task->state != AKROPOLIS_PLAZA_SCENE_INIT) {
        // Sub-ids 0..3 share the shot-position table, one row per scene frame.
        switch (queue->plazaStreamSubId) {
            case 0:
            case 1:
            case 2:
            case 3:
                work->shotPos.vx = _gAkropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vx;
                work->shotPos.vy = _gAkropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vy;
                work->shotPos.vz = _gAkropolisPlazaTraversalShotPositions[queue->sceneFrame - 1].vz;
                work->relX       = work->shotPos.vx - work->playerMtx->t[0];
                break;
        }
    }

    switch (task->state) {
        case AKROPOLIS_PLAZA_SCENE_INIT:
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
                sectorOffset        = (queue->movieFrame - 1) * AKROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                packedOpeningOffset = sectorOffset & 0xFFFF;
                streamArgs[1]       = packedOpeningOffset >> 8;
                streamArgs[2]       = packedOpeningOffset & lowByteMask;
                cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
            } else {
                queue->movieAtEnd = 0;
            }
            ((_AkropolisPlazaSceneWork*)task->work)->playerMtx = gPlayerStatus.coordMtx;
            work->frameStep                                    = AKROPOLIS_PLAZA_FRAME_STEP_FINE;
            task->state                                        = task->state + 1;
            break;
        case AKROPOLIS_PLAZA_SCENE_WAIT_IDLE:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            work->playerTask = playerTask;
            work->player     = playerTask->work;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case AKROPOLIS_PLAZA_SCENE_CHECK_EDGES:
        checkEdges:
            work->frameLimit = streamGetFrameLimit(queue->plazaStreamSubId);
            if (queue->movieAtEnd != 0) {
                task->state = AKROPOLIS_PLAZA_SCENE_AT_END;
                break;
            }
            _akropolisPlazaUpdateSceneEdgeFlags(task->work);
            if (work->fwd != AKROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 0;
            }
            if (work->back != AKROPOLIS_PLAZA_EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 1;
            }
            if (work->fwd == AKROPOLIS_PLAZA_EDGE_NONE && work->back == AKROPOLIS_PLAZA_EDGE_NONE) {
                break;
            }
            // Continuing counts from the movie frame; reversing counts back from the frame limit.
            queue->continueMovie = 1;
            reverse              = queue->reverseSceneFrames;
            if (reverse == work->prevReverse) {
                if (reverse == 0) {
                    if (work->fwd == AKROPOLIS_PLAZA_EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == AKROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * AKROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * AKROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = AKROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == AKROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * AKROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * AKROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = AKROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                }
            } else {
                if (reverse == 0) {
                    if (work->fwd == AKROPOLIS_PLAZA_EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == AKROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * AKROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * AKROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = AKROPOLIS_PLAZA_FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == AKROPOLIS_PLAZA_FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * AKROPOLIS_PLAZA_SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * AKROPOLIS_PLAZA_SEEK_FRAMES_FINE;
                        }
                        work->frameStep = AKROPOLIS_PLAZA_FRAME_STEP_FINE;
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
        case AKROPOLIS_PLAZA_SCENE_WAIT_READY:
            if (queue->movieReady != 0) {
                task->state = task->state + 1;
            }
            break;
        case AKROPOLIS_PLAZA_SCENE_FOLLOW_EDGE:
            _akropolisPlazaUpdateSceneEdgeFlags(task->work);
            if (queue->movieAtEnd != 0) {
                task->state = AKROPOLIS_PLAZA_SCENE_AT_END;
                break;
            }
            if ((work->fwd == AKROPOLIS_PLAZA_EDGE_NONE && queue->reverseSceneFrames == 0) ||
                (work->back == AKROPOLIS_PLAZA_EDGE_NONE && queue->reverseSceneFrames == 1) ||
                work->fwd != work->prevFwd || work->back != work->prevBack) {
                queue->continueMovie = 0;
                task->state          = task->state + 1;
            }
            break;
        case AKROPOLIS_PLAZA_SCENE_WAIT_SEEK_END:
            _akropolisPlazaUpdateSceneEdgeFlags(task->work);
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            task->state = AKROPOLIS_PLAZA_SCENE_CHECK_EDGES;
            goto checkEdges;
        case AKROPOLIS_PLAZA_SCENE_AT_END:
            _akropolisPlazaUpdateSceneEdgeFlags(task->work);
            switch (queue->plazaStreamSubId) {
                case 0:
                case 2:
                    if (work->back != AKROPOLIS_PLAZA_EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = AKROPOLIS_PLAZA_SCENE_CHECK_EDGES;
                    }
                    break;
                case 1:
                case 3:
                    if (work->fwd != AKROPOLIS_PLAZA_EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = AKROPOLIS_PLAZA_SCENE_CHECK_EDGES;
                    }
                    break;
            }
            break;
    }

    if (queue->movieReady != 0) {
        _akropolisPlazaUpdateSceneCollisionWalls(task);
        _akropolisPlazaApplyStreamCamera(queue->plazaStreamSubId);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Allocates an event task's work, caches the player, or kills it and returns.
#define AKROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(eventTask)                                                     \
    {                                                                                                        \
        _AkropolisPlazaEventWork* allocatedWork;                                                             \
        allocatedWork     = memMalloc(sizeof(*allocatedWork), false);                                        \
        (eventTask)->work = allocatedWork;                                                                   \
        if (allocatedWork == NULL) {                                                                         \
            taskKill(eventTask);                                                                             \
            return;                                                                                          \
        }                                                                                                    \
        memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));                                              \
        ((_AkropolisPlazaEventWork*)(eventTask)->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER); \
    }

/// Handles the first plaza scene event by walking and turning the player, then running its script.
static void _akropolisPlazaFirstSceneTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_FIRST_SCENE_INIT        = 0,
        AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_MOVE   = 1,
        AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_TURN   = 2,
        AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_STREAM = 3,
        AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_SCRIPT = 4,
    };
    ActorTransform            destination;
    ActorTransform            heading;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AkropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        case AKROPOLIS_PLAZA_FIRST_SCENE_INIT:
            AKROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = 0x3804;
            destination.pos.vy = 0;
            destination.pos.vz = 0xFC8;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_STREAM:
            // Preserve the traversal frame before handing control to the event script.
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
            taskKill(((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            evsStartScriptWithSkip(_gAkropolisPlazaFirstSceneScript, EVENT_SCRIPT_HUD_KEEP, _gAkropolisPlazaFirstSceneSkipScript);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FIRST_SCENE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

/// Queues a plaza movie sub-ID at sector offset zero.
static inline void _akropolisPlazaQueueStreamAtStart(u8 command, u8 subId)
{
    u8 streamArgs[4];

    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, subId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(command, 0, streamArgs);
}

/// Finds a live enemy by the resource entry in the current area's placement table.
static inline Enemy* _akropolisPlazaFindPlacedEnemy(u8 entryId)
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
static inline void _akropolisPlazaPlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{
    AnimationPlayRequest request;
    s32                  weaponIndex;

    weaponIndex                  = gPlayerStatus.weapon;
    request.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponIndex);
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Places the player at the model root's local translation, upright at the requested yaw.
static inline void _akropolisPlazaPlacePlayerAtModelRoot(Task* task, s32 yaw)
{
    ActorTransform placement;
    GfxCoord*      root;

    root             = ((_AkropolisPlazaEventWork*)task->work)->playerTask->extra.tmd->coords;
    placement.pos.vx = root->coord.t[0];
    placement.pos.vy = root->coord.t[1];
    placement.pos.vz = root->coord.t[2];
    placement.rot.vz = 0;
    placement.rot.vx = 0;
    placement.rot.vy = yaw;
    TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

/// Handles a plaza stream-scene event, returning the player from scene animation to play.
static void _akropolisPlazaStreamSceneTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_STREAM_SCENE_INIT             = 0,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_MOVE        = 1,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_TURN        = 2,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_IDLE        = 3,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_READY       = 4,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_REJOIN      = 5,
        AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_END         = 6,
        AKROPOLIS_PLAZA_STREAM_SCENE_PLAYER_ANIMATION = 11,
        AKROPOLIS_PLAZA_STREAM_SCENE_REJOIN_FRAME     = 96,
    };
    ActorTransform            destination;
    ActorTransform            heading;
    AnimationPlayRequest      animationRequest;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AkropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        case AKROPOLIS_PLAZA_STREAM_SCENE_INIT:
            AKROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = 0xF6E;
            destination.pos.vy = 0;
            destination.pos.vz = 0x2328;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = 0xD55;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            animationRequest.source.sets          = gActor310100PlayerAnimationSets;
            animationRequest.animationId          = AKROPOLIS_PLAZA_STREAM_SCENE_PLAYER_ANIMATION;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            ((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
            taskKill(((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = 2;
            _akropolisPlazaQueueStreamAtStart(CD_COMMAND_RESET_STREAM_AT_OFFSET, 2);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            evsStartScript(_gAkropolisPlazaStreamSceneScript, EVENT_SCRIPT_HUD_KEEP);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_REJOIN:
            // Fold the scene pose back into placement before ordinary control resumes.
            if (queue->movieFrame >= AKROPOLIS_PLAZA_STREAM_SCENE_REJOIN_FRAME) {
                _akropolisPlazaPlayPlayerAnimation(1, ANIMATION_BLEND_RESET, 10);
                _akropolisPlazaPlacePlayerAtModelRoot(task, 0xEAA);
                task->state = task->state + 1;
            }
            break;
        case AKROPOLIS_PLAZA_STREAM_SCENE_WAIT_END:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskRequestKill(task, 0);
            }
            break;
        default:
            return;
    }
    _akropolisPlazaApplyStreamCamera(4);
}

/// Resets a plaza movie at sector zero using the caller's four-byte command buffer.
static inline void _akropolisPlazaResetStreamInBuffer(u8 streamArgs[4], u8 movieSubId)
{
    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, movieSubId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
}

/// Runs the plaza's final trigger scene, including stream handoffs and its skip path.
static void _akropolisPlazaFinalSceneTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_FINAL_INIT                    = 0,
        AKROPOLIS_PLAZA_FINAL_WAIT_WALK               = 1,
        AKROPOLIS_PLAZA_FINAL_WAIT_TURN               = 2,
        AKROPOLIS_PLAZA_FINAL_START_FIRST_STREAM      = 3,
        AKROPOLIS_PLAZA_FINAL_WAIT_FIRST_STREAM       = 4,
        AKROPOLIS_PLAZA_FINAL_WAIT_FIRST_SCRIPT       = 5,
        AKROPOLIS_PLAZA_FINAL_RESTORE_PLAYER          = 6,
        AKROPOLIS_PLAZA_FINAL_WAIT_OFFICER            = 7,
        AKROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS        = 8,
        AKROPOLIS_PLAZA_FINAL_START_SECOND_STREAM     = 9,
        AKROPOLIS_PLAZA_FINAL_WAIT_SECOND_STREAM      = 10,
        AKROPOLIS_PLAZA_FINAL_WAIT_TAIL_EFFECT        = 11,
        AKROPOLIS_PLAZA_FINAL_WAIT_DISPLAY            = 12,
        AKROPOLIS_PLAZA_FINAL_WAIT_TAIL_SCRIPT        = 13,
        AKROPOLIS_PLAZA_FINAL_WAIT_CLOSING_STREAM     = 14,
        AKROPOLIS_PLAZA_FINAL_WAIT_END                = 15,
        AKROPOLIS_PLAZA_FINAL_OFFICER_ENTRY           = 0x6C,
        AKROPOLIS_PLAZA_FINAL_SET_CULLED_BODY_MESSAGE = 2007,
        AKROPOLIS_PLAZA_FINAL_OFFICER_1               = 1,
        AKROPOLIS_PLAZA_FINAL_OFFICER_SOUND_FRAME     = 30,
        AKROPOLIS_PLAZA_FINAL_OFFICER_EFFECT_FRAME    = 61,
        AKROPOLIS_PLAZA_FINAL_TAIL_EFFECT_FRAME       = 11,
        AKROPOLIS_PLAZA_FINAL_FADE_OUT_TASK           = AKROPOLIS_PLAZA_TASK_FADE_OUT,
        AKROPOLIS_PLAZA_FINAL_FADE_IN_TASK            = AKROPOLIS_PLAZA_TASK_FADE_IN,
        AKROPOLIS_PLAZA_FINAL_OFFICER_ANIMATION       = 8,
        AKROPOLIS_PLAZA_FINAL_OFFICER_BLEND_FRAMES    = 10,
        AKROPOLIS_PLAZA_FINAL_FADE_OUT_STEP           = 9,
        AKROPOLIS_PLAZA_FINAL_FADE_IN_FRAMES          = 8,
        AKROPOLIS_PLAZA_FINAL_SKIP_MUSIC_FADE_TICKS   = 10,
        AKROPOLIS_PLAZA_FINAL_MUSIC_FADE_TICKS        = 480,
        AKROPOLIS_PLAZA_FINAL_AMBIENCE_STOP_TICKS     = 180,
        AKROPOLIS_PLAZA_FINAL_AMBIENCE_VOLUME         = 38,
        AKROPOLIS_PLAZA_FINAL_FIRST_STREAM            = 4,
        AKROPOLIS_PLAZA_FINAL_OFFICER_STREAM          = 5,
        AKROPOLIS_PLAZA_FINAL_CLOSING_STREAM          = 3,
        AKROPOLIS_PLAZA_FINAL_FIRST_CAMERA            = 6,
        AKROPOLIS_PLAZA_FINAL_OFFICER_CAMERA          = 7,
        AKROPOLIS_PLAZA_FINAL_CLOSING_CAMERA          = 5
    };

    ActorTransform            walkDestination;
    ActorTransform            heading;
    u8                        streamArgs[4];
    ActorTransform            returnDestination;
    AnimationPlayRequest      officerAnimation;
    CdCmdQueue*               queue = &gCdCmdQueue;
    _AkropolisPlazaEventWork* work  = task->work;

    switch (task->state) {
        // Scripted walking and turning finish before the traversal task is released.
        case AKROPOLIS_PLAZA_FINAL_INIT:
            AKROPOLIS_PLAZA_INITIALIZE_EVENT_WORK(task);
            walkDestination.pos.vx = 0x3DE;
            walkDestination.pos.vy = 0;
            walkDestination.pos.vz = 0x33FE;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &walkDestination, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_WALK:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = ACTOR_TRANSFORM_ANGLE_TURN;
            TASK_MESSAGE_DISPATCH_POINTER(((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_START_FIRST_STREAM:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            taskKill(((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = AKROPOLIS_PLAZA_FINAL_FIRST_STREAM;
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_FINAL_FIRST_STREAM);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_FIRST_STREAM:
            if (cdCmdIsIdle() != 0) {
                evsStartScriptWithSkip(_gAkropolisPlazaFinalSceneLeadInScript, EVENT_SCRIPT_HUD_KEEP, _gAkropolisPlazaFinalSceneLeadInSkipScript);
                task->state = task->state + 1;
                return;
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_FIRST_CAMERA);
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_FIRST_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (gGameSession->evtSkipped != 0) {
                SetDispMask(0);
                task->state = AKROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS;
                return;
            }
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = AKROPOLIS_PLAZA_FINAL_OFFICER_STREAM;
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_FINAL_OFFICER_STREAM);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_RESTORE_PLAYER:
            if (queue->movieReady == 0) {
                return;
            }
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            returnDestination.pos.vx = 0x3DE;
            returnDestination.pos.vy = 0;
            returnDestination.pos.vz = 0x439E;
            TASK_MESSAGE_DISPATCH_POINTER(
                ((_AkropolisPlazaEventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &returnDestination, 0);
            officerAnimation.source.index         = AKROPOLIS_PLAZA_FINAL_OFFICER_1;
            officerAnimation.animationId          = AKROPOLIS_PLAZA_FINAL_OFFICER_ANIMATION;
            officerAnimation.blend                = ANIMATION_BLEND_RESET;
            officerAnimation.blendFrames          = AKROPOLIS_PLAZA_FINAL_OFFICER_BLEND_FRAMES;
            officerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(_akropolisPlazaFindPlacedEnemy(AKROPOLIS_PLAZA_FINAL_OFFICER_ENTRY)->task, ACTOR_MESSAGE_PLAY_ANIMATION, &officerAnimation, 0);
            task->state         = task->state + 1;
            work->elapsedFrames = 0;
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_OFFICER:
            if (work->elapsedFrames == AKROPOLIS_PLAZA_FINAL_OFFICER_SOUND_FRAME) {
                sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_FINAL_SCENE_CUE), 0, 0);
            }
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= AKROPOLIS_PLAZA_FINAL_OFFICER_EFFECT_FRAME) {
                taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_FINAL_FADE_OUT_TASK, AKROPOLIS_PLAZA_FINAL_FADE_OUT_STEP, 0);
                task->state = task->state + 1;
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_OFFICER_CAMERA);
            return;
        // Replace the officer body and release player buffers before rebuilding image memory.
        case AKROPOLIS_PLAZA_FINAL_REBUILD_GRAPHICS:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(_akropolisPlazaFindPlacedEnemy(AKROPOLIS_PLAZA_FINAL_OFFICER_ENTRY)->task, AKROPOLIS_PLAZA_FINAL_SET_CULLED_BODY_MESSAGE, AKROPOLIS_PLAZA_FINAL_OFFICER_1, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE, 0);
                gpuResetAndInvalidateModelBuffers();
                memConfigureImageMemory(gGameSession->location.stage, gGameSession->location.area);
                memSelectAuxHeapRegion(true);
                tmdResetAuxHeapAndRestoreBuffers();
                sndEvtRequestScriptVolume(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE), AKROPOLIS_PLAZA_FINAL_AMBIENCE_VOLUME);
                task->state = task->state + 1;
                return;
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_OFFICER_CAMERA);
            return;
        case AKROPOLIS_PLAZA_FINAL_START_SECOND_STREAM:
            queue->sceneFrame       = 1;
            queue->movieFrame       = 1;
            queue->plazaStreamSubId = AKROPOLIS_PLAZA_FINAL_CLOSING_STREAM;
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_FINAL_CLOSING_STREAM);
            queue->continueMovie = 0;
            task->state          = task->state + 1;
            /* fallthrough */
        case AKROPOLIS_PLAZA_FINAL_WAIT_SECOND_STREAM:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            evsStartScript(_gAkropolisPlazaFinalSceneOfficerScript, EVENT_SCRIPT_HUD_KEEP);
            work->elapsedFrames = 0;
            task->state         = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_TAIL_EFFECT:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= AKROPOLIS_PLAZA_FINAL_TAIL_EFFECT_FRAME) {
                sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICERS_WALK_IN), 0, 0);
                taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_FINAL_FADE_IN_TASK, AKROPOLIS_PLAZA_FINAL_FADE_IN_FRAMES, 0);
                work->elapsedFrames = 0;
                task->state         = task->state + 1;
            }
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_DISPLAY:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 2) {
                SetDispMask(1);
                task->state = task->state + 1;
            }
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_TAIL_SCRIPT:
            if (padIsStartPressed() != 0) {
                stageMusicRequestAreaStop(AKROPOLIS_PLAZA_FINAL_SKIP_MUSIC_FADE_TICKS);
                cdCmdRequestCancel();
                task->state = AKROPOLIS_PLAZA_FINAL_WAIT_END;
            } else if (gGameSession->eventState == 0) {
                stageMusicRequestAreaStop(AKROPOLIS_PLAZA_FINAL_MUSIC_FADE_TICKS);
                queue->sceneFrame       = 1;
                queue->movieFrame       = 1;
                queue->plazaStreamSubId = AKROPOLIS_PLAZA_FINAL_CLOSING_STREAM;
                _akropolisPlazaQueueStreamAtStart(CD_COMMAND_PLAY_STREAM_AT_OFFSET, AKROPOLIS_PLAZA_FINAL_CLOSING_STREAM);
                queue->continueMovie = 1;
                task->state          = task->state + 1;
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_CLOSING_STREAM:
            if (queue->movieReady != 0) {
                evsStartScript(_gAkropolisPlazaFinalSceneClosingScript, EVENT_SCRIPT_HUD_KEEP);
                task->state = task->state + 1;
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        case AKROPOLIS_PLAZA_FINAL_WAIT_END:
            if (cdCmdIsIdle() != 0) {
                sndEvtRequestScriptStop(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE), AKROPOLIS_PLAZA_FINAL_AMBIENCE_STOP_TICKS);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
                taskRequestKill(task, 0);
            }
            _akropolisPlazaApplyStreamCamera(AKROPOLIS_PLAZA_FINAL_CLOSING_CAMERA);
            return;
        default:
            return;
    }
}

#undef AKROPOLIS_PLAZA_INITIALIZE_EVENT_WORK

/// Runs one of the plaza's three repeat-event scripts after the traversal stream stops.
static void _akropolisPlazaRepeatSceneTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_REPEAT_SCENE_INIT        = 0,
        AKROPOLIS_PLAZA_REPEAT_SCENE_WAIT_STREAM = 1,
        AKROPOLIS_PLAZA_REPEAT_SCENE_WAIT_SCRIPT = 2,
    };
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case AKROPOLIS_PLAZA_REPEAT_SCENE_INIT:
            _akropolisPlazaPlayPlayerAnimation(1, ANIMATION_BLEND_RESET, 10);
            task->state = task->state + 1;
            break;
        case AKROPOLIS_PLAZA_REPEAT_SCENE_WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                ((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
                taskKill(((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
                switch (task->spawnArg1.value) {
                    case 0:
                        evsStartScript(_gAkropolisPlazaFirstRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 1:
                        evsStartScript(_gAkropolisPlazaSecondRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case 2:
                        evsStartScript(_gAkropolisPlazaSubsequentRepeatSceneScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                }
                task->state = task->state + 1;
            }
            break;
        case AKROPOLIS_PLAZA_REPEAT_SCENE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// Stops plaza traversal and starts the CAP command selected by a caption trigger.
static void _akropolisPlazaCaptionEventTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_CAPTION_INIT           = 0,
        AKROPOLIS_PLAZA_CAPTION_WAIT_STREAM    = 1,
        AKROPOLIS_PLAZA_CAPTION_END            = 2,
        AKROPOLIS_PLAZA_CAPTION_IDLE_ANIMATION = 1,
        AKROPOLIS_PLAZA_CAPTION_BLEND_FRAMES   = 10
    };

    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case AKROPOLIS_PLAZA_CAPTION_INIT:
            _akropolisPlazaPlayPlayerAnimation(AKROPOLIS_PLAZA_CAPTION_IDLE_ANIMATION, ANIMATION_BLEND_RESET, AKROPOLIS_PLAZA_CAPTION_BLEND_FRAMES);
            task->state = task->state + 1;
            break;
        case AKROPOLIS_PLAZA_CAPTION_WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                ((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
                taskKill(((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->sceneTask);
                capRunCommandWithTransition((s8)((_AkropolisPlazaSequenceWork*)task->spawnArg2.pointer)->eventKind);
                task->state = task->state + 1;
            }
            break;
        case AKROPOLIS_PLAZA_CAPTION_END:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            taskRequestKill(task, 0);
            break;
    }
}

/// Requests a silent ambience start and returns from the enclosing voice update.
#define AKROPOLIS_PLAZA_START_AMBIENCE(startFlag, soundId)                                       \
    do {                                                                                         \
        enum { AKROPOLIS_PLAZA_AMBIENCE_SILENT_ATTENUATION = 127 };                              \
        if (*(startFlag) == false) {                                                             \
            sndEvtRequestScriptStart((soundId), 0, AKROPOLIS_PLAZA_AMBIENCE_SILENT_ATTENUATION); \
            sndEvtRequestScriptVolume((soundId), 0);                                             \
            *(startFlag) = true;                                                                 \
            return;                                                                              \
        }                                                                                        \
    } while (0)

/// Starts, envelopes or stops one plaza ambience script against the current scene frame.
static void _akropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    u32         sceneFrame;
    u8          volume;

    sceneFrame = queue->sceneFrame;
    if (sceneFrame >= firstFrame && sceneFrame <= lastFrame) {
        AKROPOLIS_PLAZA_START_AMBIENCE(startRequested, soundId);
        if (envelope == AKROPOLIS_PLAZA_AMBIENCE_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / (peakFrame - firstFrame);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / (lastFrame - peakFrame);
            }
        } else if (envelope == AKROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * (AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((peakFrame - firstFrame) * 4);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * (AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((lastFrame - peakFrame) * 4);
            }
        } else if (sceneFrame < (u32)AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) {
            volume = (((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - queue->sceneFrame) * AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - 1) * 4)) + AKROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
        } else {
            volume = ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - queue->sceneFrame) * (AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME * 3)) / ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) * 4);
        }
        sndEvtRequestScriptVolume(soundId, volume);
        return;
    }
    if (*startRequested != 0) {
        sndEvtRequestScriptStop(soundId, SOUND_SCRIPT_STOP_NO_FADE);
        *startRequested = 0;
    }
}

#undef AKROPOLIS_PLAZA_START_AMBIENCE

/// Updates the plaza's four traversal ambience voices or the stream-scene voice envelope.
static void _akropolisPlazaUpdateSceneAmbience(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME      = 800,
        AKROPOLIS_PLAZA_AMBIENCE4_FIRST_FRAME             = 15,
        AKROPOLIS_PLAZA_AMBIENCE4_PEAK_FRAME              = 80,
        AKROPOLIS_PLAZA_AMBIENCE4_LAST_FRAME              = 142,
        AKROPOLIS_PLAZA_AMBIENCE1_FIRST_FRAME             = 200,
        AKROPOLIS_PLAZA_AMBIENCE1_PEAK_FRAME              = 320,
        AKROPOLIS_PLAZA_AMBIENCE1_LAST_FRAME              = 370,
        AKROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FIRST_FRAME  = 31,
        AKROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FRAME_COUNT  = 54,
        AKROPOLIS_PLAZA_AMBIENCE_STREAM_RISE_END_FRAME    = 30,
        AKROPOLIS_PLAZA_AMBIENCE_STREAM_FALL_ANCHOR_FRAME = 115,
        AKROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES       = 120
    };
    CdCmdQueue*                  queue            = &gCdCmdQueue;
    volatile u16*                sceneFrameSource = &gCdCmdQueue.sceneFrame;
    _AkropolisPlazaSequenceWork* work             = task->work;
    s32                          sceneFrame;
    s32                          volume;

    switch (gCdCmdQueue.plazaStreamSubId) {
        case 0:
        case 1:
            _akropolisPlazaUpdateAmbienceVoice(1, AKROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME, 1, &work->ambience5Playing, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE), AKROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _akropolisPlazaUpdateAmbienceVoice(1, AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END, 1, &work->ambience2Playing, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE), AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE);
            _akropolisPlazaUpdateAmbienceVoice(AKROPOLIS_PLAZA_AMBIENCE4_FIRST_FRAME, AKROPOLIS_PLAZA_AMBIENCE4_LAST_FRAME, AKROPOLIS_PLAZA_AMBIENCE4_PEAK_FRAME, &work->ambience4Playing, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_APPROACH_AMBIENCE), AKROPOLIS_PLAZA_AMBIENCE_TRIANGLE);
            _akropolisPlazaUpdateAmbienceVoice(AKROPOLIS_PLAZA_AMBIENCE1_FIRST_FRAME, AKROPOLIS_PLAZA_AMBIENCE1_LAST_FRAME, AKROPOLIS_PLAZA_AMBIENCE1_PEAK_FRAME, &work->ambience1Playing, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE), AKROPOLIS_PLAZA_AMBIENCE_QUIET_TRIANGLE);
            break;
        case 2:
            // Compare a snapshot, then reread the frame published by MDEC completion.
            sceneFrame = queue->sceneFrame;
            if ((u32)(sceneFrame - AKROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FIRST_FRAME) < (u32)AKROPOLIS_PLAZA_AMBIENCE_STREAM_FULL_FRAME_COUNT) {
                volume = AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME;
            } else if (sceneFrame < (u32)AKROPOLIS_PLAZA_AMBIENCE_STREAM_RISE_END_FRAME) {
                volume = ((*sceneFrameSource * AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / AKROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES) + AKROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
            } else {
                volume = (((AKROPOLIS_PLAZA_AMBIENCE_STREAM_FALL_ANCHOR_FRAME - *sceneFrameSource) * AKROPOLIS_PLAZA_AMBIENCE_FULL_VOLUME) / AKROPOLIS_PLAZA_AMBIENCE_STREAM_RAMP_FRAMES) + AKROPOLIS_PLAZA_AMBIENCE_BASE_VOLUME;
            }
            sndEvtRequestScriptVolume(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE), volume & 0xFF);
            break;
    }
}

/// Resumes the plaza scene task at the sequence's saved frame without seeking the stream.
static inline void _akropolisPlazaResumeSequenceStream(_AkropolisPlazaSequenceWork* work)
{
    enum { AKROPOLIS_PLAZA_RESUME_STREAM_TASK = AKROPOLIS_PLAZA_TASK_STREAMED_SCENE };

    work->sceneArg.skipStreamReset = 1;
    work->sceneArg.startFrame      = work->resumeFrame;
    work->sceneTask                = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_RESUME_STREAM_TASK, 0, &work->sceneArg);
}

/// Advances the plaza's trigger-driven streamed sequence and reports final-scene completion.
static u16 _akropolisPlazaUpdateSequenceEvent(Task* task)
{
    enum { AKROPOLIS_PLAZA_EVENT_TASK_STREAM_SCENE = AKROPOLIS_PLAZA_TASK_STREAM_SCENE,
           AKROPOLIS_PLAZA_EVENT_TASK_FINAL_SCENE  = AKROPOLIS_PLAZA_TASK_FINAL_SCENE,
           AKROPOLIS_PLAZA_EVENT_TASK_FIRST_SCENE  = AKROPOLIS_PLAZA_TASK_FIRST_SCENE,
           AKROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE = AKROPOLIS_PLAZA_TASK_REPEAT_SCENE,
           AKROPOLIS_PLAZA_EVENT_TASK_CAPTION      = AKROPOLIS_PLAZA_TASK_CAPTION,
           AKROPOLIS_PLAZA_REPEAT_FIRST_VARIANT    = 0,
           AKROPOLIS_PLAZA_REPEAT_SECOND_VARIANT   = 1,
           AKROPOLIS_PLAZA_REPEAT_LAST_VARIANT     = 2,
           AKROPOLIS_PLAZA_SCENE_RUNNING           = 0,
           AKROPOLIS_PLAZA_SCENE_COMPLETE          = 1 };

    CdCmdQueue*                  queue = &gCdCmdQueue;
    _AkropolisPlazaSequenceWork* work  = task->work;
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
        case AKROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                if ((s8)eventKind == AKROPOLIS_PLAZA_EVENT_FIRST_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_FIRST_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if ((s8)eventKind >= AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_CAPTION, 0, work);
                    work->step = AKROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION;
                }
            }
            break;
        case AKROPOLIS_PLAZA_STEP_RUN_FIRST_SCENE:
            if (taskPollKill(work->eventTask, &firstSceneResult) != 0) {
                _akropolisPlazaResumeSequenceStream(work);
                worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAkropolisPlazaFirstSceneTrigger);
                work->step = work->step + 1;
            }
            break;
        case AKROPOLIS_PLAZA_STEP_AWAIT_EVENT:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                work->resumeFrame     = queue->sceneFrame;
                signedEventKind       = (s8)eventKind;
                if (signedEventKind == AKROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_STREAM_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == AKROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_FINAL_SCENE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == AKROPOLIS_PLAZA_EVENT_REPEAT_SCENE) {
                    if (work->repeatVariant == AKROPOLIS_PLAZA_REPEAT_FIRST_VARIANT) {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, AKROPOLIS_PLAZA_REPEAT_FIRST_VARIANT, work);
                        work->step = AKROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else if (work->repeatVariant == AKROPOLIS_PLAZA_REPEAT_SECOND_VARIANT) {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, AKROPOLIS_PLAZA_REPEAT_SECOND_VARIANT, work);
                        work->step = AKROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    } else {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_REPEAT_SCENE, AKROPOLIS_PLAZA_REPEAT_LAST_VARIANT, work);
                        work->step = AKROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE;
                    }
                } else if (signedEventKind >= AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_EVENT_TASK_CAPTION, 0, work);
                    work->step = AKROPOLIS_PLAZA_STEP_RUN_CAPTION;
                }
            }
            break;
        case AKROPOLIS_PLAZA_STEP_RUN_SCENE:
            if (taskPollKill(work->eventTask, &sceneResult) != 0) {
                // Keep the signed-byte extension and unsigned-halfword comparison widths.
                latchedKind   = work->eventKind;
                latchedKind16 = (s8)latchedKind;
                if (latchedKind16 == AKROPOLIS_PLAZA_EVENT_STREAM_SCENE) {
                    worldCollisionUnlinkTrigger(0, &_gAkropolisPlazaStreamSceneTrigger);
                } else if ((u16)(s8)latchedKind == AKROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, _gAkropolisPlazaFinalSceneAndInteractionTriggers);
                }
                work->step = work->step - 1;
                if ((s8)work->eventKind == AKROPOLIS_PLAZA_EVENT_FINAL_SCENE) {
                    return AKROPOLIS_PLAZA_SCENE_COMPLETE;
                }
                _akropolisPlazaResumeSequenceStream(work);
            }
            break;
        case AKROPOLIS_PLAZA_STEP_RUN_REPEAT_SCENE:
        case AKROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION:
        case AKROPOLIS_PLAZA_STEP_RUN_CAPTION:
            if (taskPollKill(work->eventTask, &repeatOrCaptionResult) != 0) {
                _akropolisPlazaResumeSequenceStream(work);
                completedStep = work->step;
                if (completedStep == AKROPOLIS_PLAZA_STEP_RUN_FIRST_CAPTION) {
                    work->step = AKROPOLIS_PLAZA_STEP_AWAIT_FIRST_SCENE;
                } else {
                    if (completedStep != AKROPOLIS_PLAZA_STEP_RUN_CAPTION) {
                        if (work->repeatVariant < AKROPOLIS_PLAZA_REPEAT_LAST_VARIANT) {
                            work->repeatVariant = work->repeatVariant + 1;
                        }
                    }
                    work->step = AKROPOLIS_PLAZA_STEP_AWAIT_EVENT;
                }
            }
            break;
    }
    return AKROPOLIS_PLAZA_SCENE_RUNNING;
}

/// Draws black bars over the top and bottom 24 pixels of the cinematic view.
static void _akropolisPlazaLetterboxTask(Task* task)
{
    enum { AKROPOLIS_PLAZA_LETTERBOX_TAG = 3 };
    TILE* tile;

    /// Queues one cinematic bar, overwriting this callback's `tile` local.
#define AKROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(topY)  \
    tile           = gGpuPrimCursor;              \
    gGpuPrimCursor = tile + 1;                    \
    SetTile(tile);                                \
    tile->b0 = 0;                                 \
    tile->g0 = 0;                                 \
    tile->r0 = 0;                                 \
    tile->x0 = -AKROPOLIS_PLAZA_SCREEN_WIDTH / 2; \
    tile->y0 = (topY);                            \
    tile->w  = AKROPOLIS_PLAZA_SCREEN_WIDTH;      \
    tile->h  = AKROPOLIS_PLAZA_LETTERBOX_HEIGHT;  \
    addPrim(gGpuCurrentOt + AKROPOLIS_PLAZA_LETTERBOX_TAG, tile);

    AKROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(-AKROPOLIS_PLAZA_SCREEN_HEIGHT / 2);
    AKROPOLIS_PLAZA_DRAW_LETTERBOX_BAR(AKROPOLIS_PLAZA_SCREEN_HEIGHT / 2 - AKROPOLIS_PLAZA_LETTERBOX_HEIGHT);
#undef AKROPOLIS_PLAZA_DRAW_LETTERBOX_BAR
}

/// Records the elevator arrival selectors and the destination's sprite variant.
static inline void _akropolisPlazaCommitElevatorDestination(void)
{
    enum {
        AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP   = 1,
        AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM   = 1,
        AKROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT = 1,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_WEST_ELEVATOR_HALL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                 = AKROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT;
}

/// Runs the plaza's streamed traversal and triggered scenes before leaving for the elevator hall.
static void _akropolisPlazaSequenceTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_SEQUENCE_INITIALIZE               = 0,
        AKROPOLIS_PLAZA_SEQUENCE_WAIT_FIRST_FRAME         = 1,
        AKROPOLIS_PLAZA_SEQUENCE_WAIT_SECOND_FRAME        = 2,
        AKROPOLIS_PLAZA_SEQUENCE_START_STREAM             = 3,
        AKROPOLIS_PLAZA_SEQUENCE_RUN                      = 4,
        AKROPOLIS_PLAZA_SEQUENCE_LEAVE                    = 5,
        AKROPOLIS_PLAZA_SEQUENCE_STREAM_TASK_INDEX        = AKROPOLIS_PLAZA_TASK_STREAMED_SCENE,
        AKROPOLIS_PLAZA_SEQUENCE_DISPLAY_SETUP_TASK_INDEX = AKROPOLIS_PLAZA_TASK_DISPLAY_SETUP,
        AKROPOLIS_PLAZA_SEQUENCE_FADE_TASK_INDEX          = AKROPOLIS_PLAZA_TASK_FADE_IN,
        AKROPOLIS_PLAZA_SEQUENCE_LETTERBOX_TASK_INDEX     = AKROPOLIS_PLAZA_TASK_LETTERBOX,
        AKROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL            = 0x370,
        AKROPOLIS_PLAZA_SEQUENCE_FADE_MODE                = 6,
    };
    CdCmdQueue*                  queue = &gCdCmdQueue;
    _AkropolisPlazaSequenceWork* work  = task->work;
    _AkropolisPlazaSequenceWork* allocatedWork;
    _AkropolisPlazaSequenceWork* initializedWork;
    Task*                        childTask;
    SVECTOR                      ambientColor;

    switch (task->state) {
        case AKROPOLIS_PLAZA_SEQUENCE_INITIALIZE:
            // Hold input while presentation and the owned work block are prepared.
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU & ~PAD_BUTTON_CIRCLE);
            viewApplyCamera(&_gAkropolisPlazaTraversalCameraSamples[0]);
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
            childTask                   = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_SEQUENCE_DISPLAY_SETUP_TASK_INDEX, 0, 0);
            initializedWork             = task->work;
            initializedWork->eventTask  = childTask;
            playerActorRemoveEquipment();
            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_SEQUENCE_LETTERBOX_TASK_INDEX, 0, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_SEQUENCE_WAIT_FIRST_FRAME:
        case AKROPOLIS_PLAZA_SEQUENCE_WAIT_SECOND_FRAME:
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_SEQUENCE_START_STREAM:
            // Children borrow sceneArg from this work until the sequence finishes.
            ambientColor.vx = AKROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            ambientColor.vy = AKROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            ambientColor.vz = AKROPOLIS_PLAZA_SEQUENCE_AMBIENT_LEVEL;
            worldCoordSetAmbientColorOverride(&ambientColor);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->sceneArg.skipStreamReset = 0;
            work->sceneArg.startFrame      = 0;
            work->sceneTask                = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_SEQUENCE_STREAM_TASK_INDEX, 0, &work->sceneArg);
            stageMusicRequestAreaStart(0);
            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_SEQUENCE_FADE_TASK_INDEX, AKROPOLIS_PLAZA_SEQUENCE_FADE_MODE, 0);
            queue->blockGamePause = 1;
            task->state           = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_SEQUENCE_RUN:
            _akropolisPlazaUpdateSceneAmbience(task);
            if (_akropolisPlazaUpdateSequenceEvent(task) == 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case AKROPOLIS_PLAZA_SEQUENCE_LEAVE:
            // Commit the elevator destination before queuing the gameplay reload.
            _akropolisPlazaCommitElevatorDestination();
            loadingEnqueueEquippedWeaponResources();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            queue->blockGamePause = 0;
            taskKill(task);
            return;
    }
}

/// Hands the display loop to the plaza movie task, then ends this setup task.
static void _akropolisPlazaStartMovieDisplayTask(Task* task)
{
    enum { AKROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX = AKROPOLIS_PLAZA_TASK_MOVIE_DISPLAY };
    displaySpawnTaskFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_MOVIE_DISPLAY_TASK_INDEX, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

void akropolisPlazaSirenLightTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW             = 0x200,
        AKROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT        = 5,
        AKROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS           = 0x600,
        AKROPOLIS_PLAZA_LIGHT_REFRESH_FRAMES               = 2,
        AKROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT            = 4,
        AKROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH              = 0x200,
        AKROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH            = 0x400,
        AKROPOLIS_PLAZA_SIREN_FRINGE_Z                     = 0x400,
        AKROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z                = 0x100,
        AKROPOLIS_PLAZA_SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR = 0xC000
    };
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    GfxCoord*                      coord;
    GfxCoord*                      lightCoord;
    EffectWork*                    work;
    _AkropolisPlazaBeamScratch*    beam;
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
        work->scale = (slot & 1) << AKROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT;
        task->state = task->state + 1;
    }
    // Compose the sweeping beacon before projecting its origin and outline.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    beam                                                  = SCRATCH_STACK_RESERVE_BLOCK(_AkropolisPlazaBeamScratch);
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vx = coord->workm.t[0];
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vy = coord->workm.t[1];
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
    gte_rtps();
    gte_stsxy(&beam->centerScreenPos);
    gte_stszotz(&beam->otz);
    // A culled origin leaves this slot inactive for the frame.
    lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
    if (beam->otz >= AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(beam->centerScreenPos.vx) < AKROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(beam->centerScreenPos.vy) < AKROPOLIS_PLAZA_LIGHT_CULL_Y) {
            RANDOM_LCG_ADVANCE();
            brightness = (RANDOM_LCG_DRAW() & AKROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | AKROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT) {
                work->angle = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) > AKROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
                red         = brightness >> 1;
                green       = brightness >> 1;
                blue        = brightness;
            } else {
                work->angle = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) < AKROPOLIS_PLAZA_LIGHT_HALF_TURN - AKROPOLIS_PLAZA_LIGHT_SIREN_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
                red         = brightness;
                green       = red >> 1;
                blue        = red >> 1;
            }
            beamLength = work->angle;
            // Refresh the point light; only a long beam adds yaw-dependent falloff.
            falloffWidth                                              = beamLength != AKROPOLIS_PLAZA_LIGHT_LONG_REACH ? 0 : (yaw = work->scale, work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN >= 0 ? (yaw - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) * 4 : (beamLength - yaw) * 4);
            work->period                                              = falloffWidth;
            lightCoord->coord.t[0]                                    = coord->coord.t[0];
            lightCoord->coord.t[1]                                    = coord->coord.t[1];
            lightCoord->coord.t[2]                                    = coord->coord.t[2];
            lightCoord->composeStamp                                  = GRAPHICS_COORD_DIRTY;
            lightSlot->framesLeft                                     = AKROPOLIS_PLAZA_LIGHT_REFRESH_FRAMES;
            light->inner                                              = AKROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS;
            light->outer                                              = work->period + AKROPOLIS_PLAZA_LIGHT_POINT_INNER_RADIUS;
            light->head.color.r                                       = red << AKROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            light->head.color.g                                       = green << AKROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            light->head.color.b                                       = blue << AKROPOLIS_PLAZA_LIGHT_POINT_COLOR_SHIFT;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vx   = -AKROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vy   = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vz   = work->angle;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vx  = AKROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vz  = work->angle;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vx  = -AKROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vz  = AKROPOLIS_PLAZA_SIREN_FRINGE_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vx = AKROPOLIS_PLAZA_SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vy = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vz = AKROPOLIS_PLAZA_SIREN_FRINGE_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vx  = -AKROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vz  = AKROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vx = AKROPOLIS_PLAZA_SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vy = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vz = AKROPOLIS_PLAZA_SIREN_FRINGE_NEAR_Z;
            for (index = AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT; index < AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT; index++) {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&beam->vertices[index]);
                gte_rtv0();
                gte_stsv(&beam->vertices[index]);
                // Move the rotated outline vertex into world coordinates.
                AKROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&beam->vertices[index], coord);
            }
            gte_SetRotMatrix(&GsWSMATRIX);
            for (index = AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT; index < AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT; index++) {
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
            beamTriangle->x0 = beam->centerScreenPos.vx;
            beamTriangle->y0 = beam->centerScreenPos.vy;
            beamTriangle->x1 = (u16)beam->vertexScreenPos[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT];
            beamTriangle->y1 = (beam->vertexScreenPos[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT] >> 16);
            beamTriangle->x2 = (u16)beam->vertexScreenPos[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT];
            beamTriangle->y2 = (beam->vertexScreenPos[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT] >> 16);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(beam->otz), beamTriangle);
            gpuSetPrimitiveBlendMode(beamTriangle, GPU_BLEND_ADD, beam->otz);
            for (; index < 2; index++) {
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, (s16)red * 2, (s16)green * 2, (s16)blue * 2);
                setRGB2(primitive, 0, 0, 0);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = (u16)beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT];
                primitive->y0 = (beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT] >> 16);
                primitive->x1 = beam->centerScreenPos.vx;
                primitive->y1 = beam->centerScreenPos.vy;
                primitive->x2 = (u16)beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT];
                primitive->y2 = (beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT] >> 16);
                primitive->x3 = (u16)beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT];
                primitive->y3 = (beam->vertexScreenPos[index + AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT] >> 16);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(beam->otz), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, beam->otz);
            }
            beam->glowRadius = AKROPOLIS_PLAZA_SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR / beam->otz;
            red            <<= 1;
            green          <<= 1;
            blue           <<= 1;
            AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(beam->centerScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
        }
    }
    // The far glow has its own flicker and visibility test, even if the origin was culled.
    RANDOM_LCG_ADVANCE();
    brightness = (RANDOM_LCG_DRAW() & AKROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | AKROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_BLUE_SIREN_SLOT_LIMIT) {
        work->angle          = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) > AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = (u32)packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) < AKROPOLIS_PLAZA_LIGHT_HALF_TURN - AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (u32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == AKROPOLIS_PLAZA_LIGHT_LONG_REACH) {
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vx = 0;
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vy = 0;
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE].vz = AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        gte_rtv0();
        gte_stsv(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        AKROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE], coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTRE]);
        gte_rtps();
        gte_stsxy(&beam->centerScreenPos);
        gte_stszotz(&beam->otz);
        if (beam->otz >= AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
            if (__builtin_abs(beam->centerScreenPos.vx) < AKROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(beam->centerScreenPos.vy) < AKROPOLIS_PLAZA_LIGHT_CULL_Y) {
                beam->glowRadius = AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR / beam->otz;
                AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(beam->centerScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - AKROPOLIS_PLAZA_LIGHT_YAW_STEP) & AKROPOLIS_PLAZA_LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_AkropolisPlazaBeamScratch);
}

void akropolisPlazaLightFlareTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT         = 9,
        AKROPOLIS_PLAZA_FLARE_PULSE_STEP                    = 8,
        AKROPOLIS_PLAZA_FLARE_SLOT_PHASE_STEP               = 0xC0,
        AKROPOLIS_PLAZA_FLARE_RISING_LEVEL_MASK             = 0x78,
        AKROPOLIS_PLAZA_FLARE_ORIGIN_RADIUS_NUMERATOR       = 0xA000,
        AKROPOLIS_PLAZA_FLARE_RAY_TIP_RADIUS_NUMERATOR      = 0x8000,
        AKROPOLIS_PLAZA_FLARE_RAY_SHOULDER_RADIUS_NUMERATOR = 0x1800,
        AKROPOLIS_PLAZA_FLARE_FIRST_RAY_SAMPLE              = 3,
        AKROPOLIS_PLAZA_FLARE_OPPOSITE_RAY_SAMPLE_STEP      = 8
    };
    GfxCoord*                    coord;
    EffectWork*                  work;
    _AkropolisPlazaFlareScratch* flare;
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
        initialYaw  = (task->spawnArg1.value & 1) << AKROPOLIS_PLAZA_LIGHT_HALF_TURN_SHIFT;
        work->scale = initialYaw;
        task->state = task->state + 1;
    }
    // Sweep the placement frame; the origin glow remains at its world translation.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    flare              = SCRATCH_STACK_RESERVE_BLOCK(_AkropolisPlazaFlareScratch);
    flare->worldPos.vx = coord->workm.t[0];
    flare->worldPos.vy = coord->workm.t[1];
    flare->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&flare->worldPos);
    gte_rtps();
    gte_stsxy(&flare->screenPos);
    gte_stszotz(&flare->otz);
    if (flare->otz >= AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(flare->screenPos.vx) < AKROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < AKROPOLIS_PLAZA_LIGHT_CULL_Y) {
            pulsePhase = gDisplayState.animFrame * AKROPOLIS_PLAZA_FLARE_PULSE_STEP + task->spawnArg1.value * AKROPOLIS_PLAZA_FLARE_SLOT_PHASE_STEP;
            if (pulsePhase & AKROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD) {
                pulseLevel = AKROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK - (pulsePhase & AKROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK);
            } else {
                pulseLevel = pulsePhase & AKROPOLIS_PLAZA_FLARE_RISING_LEVEL_MASK;
            }
            brightness = pulseLevel;
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT) {
                red   = brightness >> 2;
                green = brightness >> 2;
                blue  = brightness;
            } else {
                red   = brightness;
                green = (s16)red >> 2;
                blue  = (s16)red >> 2;
            }
            flare->outerRadius = AKROPOLIS_PLAZA_FLARE_ORIGIN_RADIUS_NUMERATOR / flare->otz;
            AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            // Overlay four diagonal rays, alternating double and single radii.
            flare->outerRadius = AKROPOLIS_PLAZA_FLARE_RAY_TIP_RADIUS_NUMERATOR / flare->otz;
            flare->innerRadius = AKROPOLIS_PLAZA_FLARE_RAY_SHOULDER_RADIUS_NUMERATOR / flare->otz;
            red              <<= 1;
            green            <<= 1;
            blue             <<= 1;
            for (index = AKROPOLIS_PLAZA_FLARE_FIRST_RAY_SAMPLE; index < AKROPOLIS_PLAZA_LIGHT_FAN_SAMPLES; index += AKROPOLIS_PLAZA_FLARE_OPPOSITE_RAY_SAMPLE_STEP) {
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 12]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> (AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT - 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(flare->otz), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 12]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> AKROPOLIS_PLAZA_LIGHT_TRIG_SHIFT);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(flare->otz), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
            }
        }
    }
    // Gate an independently flickering glow ahead of the flare by the sweep yaw.
    RANDOM_LCG_ADVANCE();
    brightness = (RANDOM_LCG_DRAW() & AKROPOLIS_PLAZA_LIGHT_FLICKER_INTENSITY_MASK) | AKROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_BLUE_FLARE_SLOT_LIMIT) {
        work->angle          = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) > AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - AKROPOLIS_PLAZA_LIGHT_HALF_TURN) < AKROPOLIS_PLAZA_LIGHT_HALF_TURN - AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_YAW_WINDOW ? AKROPOLIS_PLAZA_LIGHT_LONG_REACH : AKROPOLIS_PLAZA_LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (s32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == AKROPOLIS_PLAZA_LIGHT_LONG_REACH) {
        flare->worldPos.vx = 0;
        flare->worldPos.vy = 0;
        flare->worldPos.vz = AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&flare->worldPos);
        gte_rtv0();
        gte_stsv(&flare->worldPos);
        AKROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX(&flare->worldPos, coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&flare->worldPos);
        gte_rtps();
        gte_stsxy(&flare->screenPos);
        gte_stszotz(&flare->otz);
        if (flare->otz >= AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
            if (__builtin_abs(flare->screenPos.vx) < AKROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < AKROPOLIS_PLAZA_LIGHT_CULL_Y) {
                flare->outerRadius = AKROPOLIS_PLAZA_LIGHT_FAR_GLOW_RADIUS_NUMERATOR / flare->otz;
                AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - AKROPOLIS_PLAZA_LIGHT_YAW_STEP) & AKROPOLIS_PLAZA_LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_AkropolisPlazaFlareScratch);
}

#undef AKROPOLIS_PLAZA_TRANSLATE_LIGHT_VERTEX

void akropolisPlazaLightGlowTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_LIGHT_RED_GLOW_SLOT_START    = 16,
        AKROPOLIS_PLAZA_GLOW_YELLOW_FLICKER_MASK     = 0x3F,
        AKROPOLIS_PLAZA_GLOW_YELLOW_RADIUS_NUMERATOR = 0xC000,
        AKROPOLIS_PLAZA_GLOW_RED_RADIUS_NUMERATOR    = 0x8000,
        AKROPOLIS_PLAZA_GLOW_RED_PULSE_STEP          = 6,
        AKROPOLIS_PLAZA_GLOW_RED_RISING_LEVEL_MASK   = 0x7E
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
    if (glow->otz >= AKROPOLIS_PLAZA_LIGHT_MIN_DEPTH) {
        if (__builtin_abs(glow->screenPos.vx) < AKROPOLIS_PLAZA_LIGHT_CULL_X && __builtin_abs(glow->screenPos.vy) < AKROPOLIS_PLAZA_LIGHT_CULL_Y) {
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_RED_GLOW_SLOT_START) {
                RANDOM_LCG_ADVANCE();
                packedBrightness = ((RANDOM_LCG_DRAW() & AKROPOLIS_PLAZA_GLOW_YELLOW_FLICKER_MASK) + AKROPOLIS_PLAZA_LIGHT_FLICKER_BASE_INTENSITY) << 16;
                yellowIntensity  = packedBrightness >> 17;
                red              = yellowIntensity;
                green            = yellowIntensity;
                blue             = (u32)packedBrightness >> 18;
                glow->halfExtent = AKROPOLIS_PLAZA_GLOW_YELLOW_RADIUS_NUMERATOR / glow->otz;
            } else {
                pulsePhase = gDisplayState.animFrame * AKROPOLIS_PLAZA_GLOW_RED_PULSE_STEP;
                if (pulsePhase & AKROPOLIS_PLAZA_LIGHT_PULSE_HALF_PERIOD) {
                    pulseLevel = AKROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK - (pulsePhase & AKROPOLIS_PLAZA_LIGHT_PULSE_LEVEL_MASK);
                } else {
                    pulseLevel = pulsePhase & AKROPOLIS_PLAZA_GLOW_RED_RISING_LEVEL_MASK;
                }
                red                = pulseLevel;
                redAccentIntensity = (s32)(red << 16) >> 18;
                green              = redAccentIntensity;
                blue               = redAccentIntensity;
                glow->halfExtent   = AKROPOLIS_PLAZA_GLOW_RED_RADIUS_NUMERATOR / glow->otz;
            }
            AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN(glow->screenPos, glow->halfExtent, glow->otz, primitive, fanSample, red, green, blue);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#undef AKROPOLIS_PLAZA_DRAW_LIGHT_GLOW_FAN

void akropolisPlazaSpawnAmbientLightsTask(Task* task)
{
    enum {
        AKROPOLIS_PLAZA_AMBIENT_LIGHTS_INITIAL = 0,
        AKROPOLIS_PLAZA_AMBIENT_GLOW_FIRST     = 12,
        AKROPOLIS_PLAZA_AMBIENT_GLOW_END       = ARRAY_SIZE(_gAkropolisPlazaAmbientLightPlacements),
        AKROPOLIS_PLAZA_AMBIENT_FLARE_FIRST    = 7,
        AKROPOLIS_PLAZA_AMBIENT_FLARE_END      = 11,
        AKROPOLIS_PLAZA_AMBIENT_SIREN_FIRST    = 1,
        AKROPOLIS_PLAZA_AMBIENT_SIREN_END      = 7,
    };
    GfxCoord* roomCoord;
    s32       placementIndex;

    roomCoord = task->extra.coordBody->coord;
    if (task->state == AKROPOLIS_PLAZA_AMBIENT_LIGHTS_INITIAL) {
        for (placementIndex = AKROPOLIS_PLAZA_AMBIENT_GLOW_FIRST; placementIndex < AKROPOLIS_PLAZA_AMBIENT_GLOW_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_LIGHT_GLOW, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = AKROPOLIS_PLAZA_AMBIENT_FLARE_FIRST; placementIndex < AKROPOLIS_PLAZA_AMBIENT_FLARE_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_LIGHT_FLARE, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = AKROPOLIS_PLAZA_AMBIENT_SIREN_FIRST; placementIndex < AKROPOLIS_PLAZA_AMBIENT_SIREN_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_SIREN_LIGHT, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        task->state = task->state + 1;
    }
}
