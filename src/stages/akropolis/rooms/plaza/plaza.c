#include "stages/akropolis/rooms/plaza.h"

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

#include "stages/akropolis/actors/police_officers.h"

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
#include "../../../../shared/effects/glow_draw.h"
#include "../../../../shared/screen/screen_fade.h"
#include "../../../../shared/screen/screen_fade_step_up.inc.c"
#include "../../../../shared/screen/screen_fade_tile_overlay.inc.c"

#include "assets/engine/animation/player_schema.h"
#include "assets/rooms/akropolis/plaza/police_rookie_schema.h"
#include "assets/rooms/akropolis/plaza/police_scene_player_schema.h"

#include "assets/rooms/akropolis/plaza/declarations.inc"

/// Screen geometry shared by the plaza's fade and cinematic bars, in pixels.
enum {
    LETTERBOX_HEIGHT = 24
};

/// Sweep, reach, flicker and fan units of the plaza's lights, beside the glow library's projection and angle units.
enum {
    LIGHT_CULL_X                    = 192, // Absolute center X in screen pixels; exclusive
    LIGHT_CULL_Y                    = 152, // Absolute center Y in screen pixels; exclusive
    LIGHT_HALF_TURN_SHIFT           = 11,
    LIGHT_YAW_STEP                  = 0x80,
    LIGHT_YAW_MASK                  = GLOW_FULL_TURN - 1,
    LIGHT_SHORT_REACH               = 0x200, // Local +Z world units
    LIGHT_LONG_REACH                = 0x800,
    LIGHT_FAR_GLOW_Z                = 0xE00,
    LIGHT_FAR_GLOW_YAW_WINDOW       = 0x300,
    LIGHT_FAN_SAMPLES               = 16, // One turn; each wedge spans two samples
    LIGHT_FAR_GLOW_RADIUS_NUMERATOR = 0x10000,
    LIGHT_FLICKER_BASE_INTENSITY    = 0x80,
    LIGHT_FLICKER_INTENSITY_MASK    = 0x7F,
    LIGHT_PULSE_HALF_PERIOD         = 0x80,
    LIGHT_PULSE_LEVEL_MASK          = 0x7F,
};

/// Translates one rotated light vertex into world space, narrowing at each store.
#define TRANSLATE_LIGHT_VERTEX(vertex, coordinate) \
    (vertex)->vx += (u16)(coordinate)->workm.t[0]; \
    (vertex)->vy += (u16)(coordinate)->workm.t[1]; \
    (vertex)->vz += (u16)(coordinate)->workm.t[2]

/// Queues the eight Gouraud wedges of one additive light glow.
#define DRAW_LIGHT_GLOW_FAN(center, radius, depth, primitive, sampleIndex, red, green, blue)                                   \
    for ((sampleIndex) = 0; (sampleIndex) < LIGHT_FAN_SAMPLES; (sampleIndex) += 2) {                                           \
        (primitive)    = gGpuPrimCursor;                                                                                       \
        gGpuPrimCursor = (primitive) + 1;                                                                                      \
        setPolyG4((primitive));                                                                                                \
        setRGB0((primitive), 0, 0, 0);                                                                                         \
        setRGB1((primitive), 0, 0, 0);                                                                                         \
        setRGB2((primitive), (red), (green), (blue));                                                                          \
        setRGB3((primitive), 0, 0, 0);                                                                                         \
        (primitive)->x0 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 4]) >> GLOW_TRIG_SHIFT); \
        (primitive)->y0 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex)]) >> GLOW_TRIG_SHIFT);     \
        (primitive)->x1 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 5]) >> GLOW_TRIG_SHIFT); \
        (primitive)->y1 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 1]) >> GLOW_TRIG_SHIFT); \
        (primitive)->x2 = (center).vx;                                                                                         \
        (primitive)->y2 = (center).vy;                                                                                         \
        (primitive)->x3 = (center).vx + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 6]) >> GLOW_TRIG_SHIFT); \
        (primitive)->y3 = (center).vy + (((radius) * _gAkropolisPlazaLightRingSamples[(sampleIndex) + 2]) >> GLOW_TRIG_SHIFT); \
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), (primitive));                                                        \
        gpuSetPrimitiveBlendMode((primitive), GPU_BLEND_ADD, (depth));                                                         \
    }

/// Index of one vertex in `_BeamScratch`.
enum {
    AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER,    // Beacon origin and the beam's apex; replaced by the far glow's center
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
} _BeamScratch;
STATIC_ASSERT_SIZEOF(_BeamScratch, 0x60);

/// Scratch-stack workspace for one frame of a light flare.
typedef struct {
    u8      unused0[0x14];  // Reserved with the block but never accessed; role unproven
    s32     otz;            // Projected depth (SZ3 / 4) of the center; the ordering-table depth, blend depth and divisor for both radii
    s32     outerRadius;    // On-screen radius in pixels of the wedge fan, then of a short ray's tip
    s32     innerRadius;    // On-screen distance in pixels from the center to a short ray's shoulders
    SVECTOR worldPos;       // Center in world coordinates, the input to the projection; staged in the flare's frame for the far glow
    u8      unused28[0x20]; // Reserved with the block but never accessed; role unproven
    DVECTOR screenPos;      // Projected center in screen pixels, stored as one GTE word
} _FlareScratch;
STATIC_ASSERT_SIZEOF(_FlareScratch, 0x4C);

/// The camera paths `_akropolisPlazaApplyMovieCamera` selects, one sample per half movie frame. Selectors 0..3 all
/// take the traversal's path; the traversal uses 0 and 1, its two movies.
enum {
    CAMERA_TRAVERSAL             = 0,
    CAMERA_HELICOPTER_WRECK      = 4,
    CAMERA_OFFICERS_TALK         = 5,
    CAMERA_ENTRANCE_OFFICER_TALK = 6,
    CAMERA_ENTERING_TOWER        = 7,
};

/// Spawn argument of the traversal task.
typedef struct {
    u16 startFrame;      // Scene frame to resume at; seeds both `CdCmdQueue::sceneFrame` and `movieFrame`
    u16 skipStreamReset; // 0 resets the stream to the frame before `startFrame`; non-zero only clears `movieAtEnd`
} _TraversalArg;
STATIC_ASSERT_SIZEOF(_TraversalArg, 0x4);

/// Which way the player is crossing the current shot's edge, and how fast.
enum {
    EDGE_NONE = 0,
    EDGE_WALK = 1,
    EDGE_RUN  = 2
};

/// How far ahead of the current shot (along -X) or behind it the player may go, in world units, before the movie moves
/// on; and the traversal's first frame, which has no shot behind it.
enum {
    FORWARD_EDGE          = 201,
    BACK_EDGE             = -20,
    FIRST_TRAVERSAL_FRAME = 1
};

/// Seek scale stored in `frameStep`, and the stream frames one movie frame spans.
enum {
    FRAME_STEP_FINE    = 1,
    FRAME_STEP_COARSE  = 2,
    SEEK_FRAMES_FINE   = 10,
    SEEK_FRAMES_COARSE = 40
};

/// Work block of the traversal task, kept in `Task::work`.
typedef struct {
    MATRIX*    playerMtx;    // Borrowed player root matrix; valid while the player actor is live
    Task*      playerTask;   // Borrowed player task, cached once the CD is idle
    GameActor* player;       // That task's actor; running is `GAME_ACTOR_MOVEMENT_RUN`
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
} _TraversalWork;
STATIC_ASSERT_SIZEOF(_TraversalWork, 0x34);

/// Steps of the plaza sequence's event handling, kept in `_SequenceWork::step`.
enum {
    STEP_AWAIT_SERGEANT     = 0, // Before the barricade: accepts the sergeant or a caption
    STEP_RUN_SERGEANT       = 1, // The sergeant's scene; his trigger is unlinked when it ends
    STEP_AWAIT_EVENT        = 2, // Past the barricade: accepts every event but the sergeant's
    STEP_RUN_SCENE          = 3, // The helicopter wreck or the entrance; the entrance ends the sequence
    STEP_RUN_SERGEANT_AGAIN = 4, // Talking to the sergeant again; the next visit's talk follows
    STEP_RUN_FIRST_CAPTION  = 5, // A caption before the barricade; back to AWAIT_SERGEANT
    STEP_RUN_CAPTION        = 6  // A caption past it; back to AWAIT_EVENT
};

/// Which visit to the sergeant after the first; the last repeats, `_SequenceWork::sergeantAgainVisit`.
enum {
    SERGEANT_AGAIN_FIRST  = 0,
    SERGEANT_AGAIN_SECOND = 1,
    SERGEANT_AGAIN_LATER  = 2,
};

/// Work block of the plaza's sequence task, kept in `Task::work`.
typedef struct {
    byte          field_0[4];          // No access established; role unproven
    Task*         playerTask;          // Borrowed player task, cached on the first frame and not read back
    Task*         traversalTask;       // The traversal task; a scene kills it when its own movie takes over
    Task*         eventTask;           // The scene or caption answering the latched event, polled until killed; first the opening movie's setup
    _TraversalArg traversalArg;        // Spawn argument of `traversalTask`, refilled before every spawn
    u16           step;                // Event-handling step (`AKROPOLIS_PLAZA_STEP_*`)
    u16           eventControl;        // Latched trigger `control` word; stored and not read back
    u8            eventKind;           // Latched trigger `parameter0` (`AKROPOLIS_PLAZA_EVENT_*`); read back as a signed byte
    u8            eventParameter1;     // Latched trigger `parameter1`; stored and not read back
    u16           resumeFrame;         // Traversal frame when the event began; the traversal resumes there afterwards
    u16           sergeantAgainVisit;  // The next visit to the sergeant (`AKROPOLIS_PLAZA_SERGEANT_AGAIN_*`)
    u16           walkAmbiencePlaying; // Non-zero while that ambience is started; the three below likewise
    u16           officerAmbiencePlaying;
    u16           entryAmbiencePlaying;
    u16           approachAmbiencePlaying;
} _SequenceWork;
STATIC_ASSERT_SIZEOF(_SequenceWork, 0x28);

/// Work block of the plaza's player-scripting event tasks, kept in `Task::work`.
/// The tasks send their scripted motion through `task->work` read again, not their `work` local: the original does.
typedef struct {
    Task* playerTask;    // Borrowed player task the scripted-control messages are sent to; set once on the first frame
    s16   elapsedFrames; // Frames counted in the current timed wait; restarted from 0 when one begins
} _EventWork;
STATIC_ASSERT_SIZEOF(_EventWork, 0x8);

static void _akropolisPlazaFadeOutTask(Task* task);
static void _akropolisPlazaOpeningMovieTask(Task* task);
static void _akropolisPlazaTraversalTask(Task* task);
static void _akropolisPlazaSergeantSceneTask(Task* task);
static void _akropolisPlazaHelicopterWreckSceneTask(Task* task);
static void _akropolisPlazaEntranceSceneTask(Task* task);
static void _akropolisPlazaSergeantAgainSceneTask(Task* task);
static void _akropolisPlazaCaptionEventTask(Task* task);
static void _akropolisPlazaLetterboxTask(Task* task);
static void _akropolisPlazaSequenceTask(Task* task);
static void _akropolisPlazaStartOpeningMovieTask(Task* task);

#include "assets/rooms/akropolis/plaza/events.inc"

TaskDesc gAkropolisPlazaSequenceTaskDescriptors[AKROPOLIS_PLAZA_TASK_COUNT] = {
    [AKROPOLIS_PLAZA_TASK_SEQUENCE]            = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaSequenceTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_TRAVERSAL]           = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaTraversalTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_HELICOPTER_WRECK]    = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaHelicopterWreckSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_ENTRANCE]            = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaEntranceSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_SERGEANT]            = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaSergeantSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_OPENING_MOVIE_SETUP] = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaStartOpeningMovieTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_SERGEANT_AGAIN]      = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaSergeantAgainSceneTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FADE_OUT]            = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaFadeOutTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_FADE_IN]             = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, screenFadeInTileTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_CAPTION]             = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaCaptionEventTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_OPENING_MOVIE]       = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaOpeningMovieTask, { .value = 0 } },
    [AKROPOLIS_PLAZA_TASK_LETTERBOX]           = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisPlazaLetterboxTask, { .value = 0 } },
};

/// The camera paths' lengths in movie frames; each frame has two samples, one per half frame.
enum {
    CAMERA_SAMPLES_PER_FRAME                 = 2,
    CAMERA_TRAVERSAL_FRAME_COUNT             = 400,
    CAMERA_HELICOPTER_WRECK_FRAME_COUNT      = 120,
    CAMERA_OFFICERS_TALK_FRAME_COUNT         = 150,
    CAMERA_ENTRANCE_OFFICER_TALK_FRAME_COUNT = 16,
    CAMERA_ENTERING_TOWER_FRAME_COUNT        = 50
};

/// Unread: the number of samples in the path after it. Only some paths have one.
static s32 _gAkropolisPlazaTraversalCameraSampleCount = CAMERA_TRAVERSAL_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME;

/// The traversal's camera, frame by frame along both of its movies.
static ViewCamera _gAkropolisPlazaTraversalCameraSamples[CAMERA_TRAVERSAL_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/rooms/akropolis/plaza/traversal_camera_path.inc"
};

/// The camera at the helicopter wreck.
static ViewCamera _gAkropolisPlazaHelicopterWreckCameraSamples[CAMERA_HELICOPTER_WRECK_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/rooms/akropolis/plaza/helicopter_wreck_camera_path.inc"
};

/// Unread: the number of samples in the path after it.
static s32 _gAkropolisPlazaOfficersTalkCameraSampleCount = CAMERA_OFFICERS_TALK_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME;

/// The camera while the officer and the sergeant talk, after Aya has gone in.
static ViewCamera _gAkropolisPlazaOfficersTalkCameraSamples[CAMERA_OFFICERS_TALK_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/rooms/akropolis/plaza/officers_talk_camera_path.inc"
};

/// Unread: the number of samples in the path after it.
static s32 _gAkropolisPlazaEntranceOfficerTalkCameraSampleCount = CAMERA_ENTRANCE_OFFICER_TALK_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME;

/// The camera while Aya talks to the officer at the entrance.
static ViewCamera _gAkropolisPlazaEntranceOfficerTalkCameraSamples[CAMERA_ENTRANCE_OFFICER_TALK_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/rooms/akropolis/plaza/entrance_officer_talk_camera_path.inc"
};

/// Unread: the number of samples in the path after it.
static s32 _gAkropolisPlazaEnteringTowerCameraSampleCount = CAMERA_ENTERING_TOWER_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME;

/// The camera as Aya walks into the tower.
static ViewCamera _gAkropolisPlazaEnteringTowerCameraSamples[CAMERA_ENTERING_TOWER_FRAME_COUNT * CAMERA_SAMPLES_PER_FRAME] = {
#include "assets/rooms/akropolis/plaza/entering_tower_camera_path.inc"
};

/// The traversal's shot positions, one per movie frame: where its collision walls stand and the edges are measured
/// from.
static VECTOR3 _gAkropolisPlazaTraversalShotPositions[CAMERA_TRAVERSAL_FRAME_COUNT] = {
#include "assets/rooms/akropolis/plaza/traversal_shot_positions.inc"
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
    AMBIENCE_TRIANGLE           = 0,
    AMBIENCE_QUIET_TRIANGLE     = 1,
    AMBIENCE_EARLY_FADE         = 2,
    AMBIENCE_FULL_VOLUME        = 127,
    AMBIENCE_BASE_VOLUME        = 95,
    AMBIENCE_SILENT_ATTENUATION = 127,
};

static void        _akropolisPlazaUpdateTraversalCollisionWalls(Task* sceneTask);
static void        _akropolisPlazaApplyMovieCamera(s32 subId);
static inline void _akropolisPlazaUpdateTraversalEdgeFlags(_TraversalWork* work);
static void        _akropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope);
static void        _akropolisPlazaUpdateTraversalAmbience(Task* task);

void akropolisPlazaPollStreamCommands(void)
{
    enum {
        INIT = 0,
        POLL = 1,
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
                        case INIT:
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
                        case POLL:
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
        INITIALIZE = 0,
        RAMP       = 1,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case INITIALIZE:
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
        case RAMP:
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

/// Plays the opening movie, which Start skips, then restores the game's graphics and display loop.
static void _akropolisPlazaOpeningMovieTask(Task* task)
{
    enum {
        SAVE_GRAPHICS = 0,
        START         = 1,
        WAIT_READY    = 2,
        WAIT_END      = 3,
        WAIT_CANCEL   = 4,
        BEGIN_RESTORE = 5,
        WAIT_RESTORE  = 6,
    };
    u8              streamArgs[4];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case SAVE_GRAPHICS:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case START:
            movieLocation      = gGameSession->location;
            movieLocation.view = AKROPOLIS_PLAZA_MOVIE_OPENING;
            streamArgs[0]      = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case WAIT_END:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed() != 0) {
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case WAIT_CANCEL:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            }
            break;
        case BEGIN_RESTORE:
            streamResetGameRestore();
            task->state++;
            break;
        case WAIT_RESTORE:
            if (streamPollGameRestore(1, 0) & 0xFFFF) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Moves the traversal's two collision walls with the current shot: the player can walk only between them, from a
/// little behind the shot to a little ahead of it (forward is -X). Vertices 0..3 are the wall ahead, 4..7 the one
/// behind.
static void _akropolisPlazaUpdateTraversalCollisionWalls(Task* traversalTask)
{
    enum {
        AHEAD  = 3000, // Distance of the wall ahead from the shot, along -X
        BEHIND = 2000, // Distance of the wall behind, along +X
        HEIGHT = 1000,
        NEAR_Z = 4096, // The walls' extent along Z, before and after the shot
        FAR_Z  = 12288,
    };
    _TraversalWork* work  = traversalTask->work;
    s32             shotX = work->shotPos.vx;
    s32             shotY = work->shotPos.vy;
    s32             shotZ = work->shotPos.vz;
    s16             leftX = shotX - AHEAD;
    s16             topY;
    s16             minZ;
    s16             maxZ;
    s16             rightX;

    _gAkropolisPlazaCollisionGridVerts[0].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[1].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[2].vx = leftX;
    _gAkropolisPlazaCollisionGridVerts[3].vx = leftX;

    topY   = shotY + HEIGHT;
    minZ   = shotZ - NEAR_Z;
    maxZ   = shotZ + FAR_Z;
    rightX = shotX + BEHIND;

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

/// Applies the camera of the current movie frame from the selected path (`AKROPOLIS_PLAZA_CAMERA_*`). While the movie
/// plays, a frame's two samples follow its half frames, counting backwards when the movie runs in reverse.
static void _akropolisPlazaApplyMovieCamera(s32 selector)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    ViewCamera* cameraSamples;
    ViewCamera* camera;
    s16         cameraIndex;

    switch ((u16)selector) {
        case CAMERA_TRAVERSAL:
        case 1:
        case 2:
        case 3:
            cameraSamples = _gAkropolisPlazaTraversalCameraSamples;
            if (queue->movieReady == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * CAMERA_SAMPLES_PER_FRAME;
            } else if (queue->reverseSceneFrames == 0) {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * CAMERA_SAMPLES_PER_FRAME + queue->movieFrameSubstep;
            } else {
                s32 sceneIndex = queue->sceneFrame - 1;
                camera         = cameraSamples + sceneIndex * CAMERA_SAMPLES_PER_FRAME - queue->movieFrameSubstep;
            }
            viewApplyCamera(camera);
            return;
        case CAMERA_HELICOPTER_WRECK:
            cameraSamples = _gAkropolisPlazaHelicopterWreckCameraSamples;
            break;
        case CAMERA_OFFICERS_TALK:
            cameraSamples = _gAkropolisPlazaOfficersTalkCameraSamples;
            break;
        case CAMERA_ENTRANCE_OFFICER_TALK:
            cameraSamples = _gAkropolisPlazaEntranceOfficerTalkCameraSamples;
            break;
        case CAMERA_ENTERING_TOWER:
            cameraSamples = _gAkropolisPlazaEnteringTowerCameraSamples;
            break;
    }
    if (queue->movieReady == 0) {
        s32 sceneIndex = queue->sceneFrame - 1;
        camera         = cameraSamples + sceneIndex * CAMERA_SAMPLES_PER_FRAME;
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

/// Sets the edge flags: whether the player is past the current shot's forward or back edge, walking or running. The
/// first frame has no back edge.
static inline void _akropolisPlazaUpdateTraversalEdgeFlags(_TraversalWork* work)
{
    CdCmdQueue* queue     = &gCdCmdQueue;
    s32         relativeX = work->relX;

    work->fwd  = EDGE_NONE;
    work->back = EDGE_NONE;
    if (relativeX >= FORWARD_EDGE) {
        if ((u16)work->player->movementMode == GAME_ACTOR_MOVEMENT_RUN) {
            work->fwd = EDGE_RUN;
        } else {
            work->fwd = EDGE_WALK;
        }
    } else if (relativeX < BACK_EDGE) {
        if (queue->sceneFrame != FIRST_TRAVERSAL_FRAME) {
            if ((u16)work->player->movementMode == GAME_ACTOR_MOVEMENT_RUN) {
                work->back = EDGE_RUN;
            } else {
                work->back = EDGE_WALK;
            }
        }
    }
}

/// The traversal: the plaza is a movie the player walks through. Each movie frame is a shot with its camera and its
/// position along the street; when the player crosses the shot's forward or back edge, the forward or the reverse movie
/// plays from the matching frame until the player stops or turns, and the camera and the collision walls follow.
static void _akropolisPlazaTraversalTask(Task* task)
{
    enum {
        INIT          = 0,
        WAIT_IDLE     = 1,
        CHECK_EDGES   = 2,
        WAIT_READY    = 3,
        FOLLOW_EDGE   = 4,
        WAIT_SEEK_END = 5,
        AT_END        = 6,
    };
    u8              streamArgs[4];
    s32             sectorOffset;
    u32             packedSeekOffset;
    u32             packedOpeningOffset;
    s32             lowByteMask = 0xFF;
    s32             reverse;
    _TraversalWork* allocatedWork;
    CdCmdQueue*     queue;
    _TraversalWork* work;
    _TraversalArg*  spawnArg;
    Task*           playerTask;
    u16             startFrame;

    queue = &gCdCmdQueue;
    work  = task->work;

    if (task->state != INIT) {
        // The traversal code takes movies 0..3 as two forward and reverse pairs sharing one shot table, a row per
        // frame; only the first pair is ever played, 2 and 3 being scenes' movies.
        switch (queue->plazaStreamSubId) {
            case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_FORWARD:
            case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_REVERSE:
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
        case INIT:
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
            queue->plazaStreamSubId   = AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_FORWARD;
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
                sectorOffset        = (queue->movieFrame - 1) * SEEK_FRAMES_FINE;
                packedOpeningOffset = sectorOffset & 0xFFFF;
                streamArgs[1]       = packedOpeningOffset >> 8;
                streamArgs[2]       = packedOpeningOffset & lowByteMask;
                cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
            } else {
                queue->movieAtEnd = 0;
            }
            ((_TraversalWork*)task->work)->playerMtx = gPlayerStatus.coordMtx;
            work->frameStep                          = FRAME_STEP_FINE;
            task->state                              = task->state + 1;
            break;
        case WAIT_IDLE:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            work->playerTask = playerTask;
            work->player     = playerTask->work;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case CHECK_EDGES:
        checkEdges:
            work->frameLimit = streamGetFrameLimit(queue->plazaStreamSubId);
            if (queue->movieAtEnd != 0) {
                task->state = AT_END;
                break;
            }
            _akropolisPlazaUpdateTraversalEdgeFlags(task->work);
            if (work->fwd != EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 0;
            }
            if (work->back != EDGE_NONE) {
                work->prevReverse         = queue->reverseSceneFrames;
                queue->reverseSceneFrames = 1;
            }
            if (work->fwd == EDGE_NONE && work->back == EDGE_NONE) {
                break;
            }
            // Seek the walking direction's movie: from the current frame when the direction is unchanged, from the
            // frame limit after a turn, the two movies running opposite ways; 10 sectors a frame (40 at the coarse
            // scale, which this task never sets). The four branches are one computation, spelled out as the
            // compiler needs them. Running past an edge matches no branch: the movie stays and the offset is unset.
            queue->continueMovie = 1;
            reverse              = queue->reverseSceneFrames;
            if (reverse == work->prevReverse) {
                if (reverse == 0) {
                    if (work->fwd == EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * SEEK_FRAMES_FINE;
                        }
                        work->frameStep = FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == FRAME_STEP_COARSE) {
                            sectorOffset = queue->movieFrame * SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = queue->movieFrame * SEEK_FRAMES_FINE;
                        }
                        work->frameStep = FRAME_STEP_FINE;
                    }
                }
            } else {
                if (reverse == 0) {
                    if (work->fwd == EDGE_WALK) {
                        queue->plazaStreamSubId = work->forwardSubId;
                        if (work->frameStep == FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * SEEK_FRAMES_FINE;
                        }
                        work->frameStep = FRAME_STEP_FINE;
                    }
                } else if (reverse == 1) {
                    if (work->back == reverse) {
                        queue->plazaStreamSubId = work->forwardSubId + 1;
                        if (work->frameStep == FRAME_STEP_COARSE) {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * SEEK_FRAMES_COARSE;
                        } else {
                            sectorOffset = (work->frameLimit - queue->movieFrame) * SEEK_FRAMES_FINE;
                        }
                        work->frameStep = FRAME_STEP_FINE;
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
        case WAIT_READY:
            if (queue->movieReady != 0) {
                task->state = task->state + 1;
            }
            break;
        case FOLLOW_EDGE:
            _akropolisPlazaUpdateTraversalEdgeFlags(task->work);
            if (queue->movieAtEnd != 0) {
                task->state = AT_END;
                break;
            }
            if ((work->fwd == EDGE_NONE && queue->reverseSceneFrames == 0) ||
                (work->back == EDGE_NONE && queue->reverseSceneFrames == 1) ||
                work->fwd != work->prevFwd || work->back != work->prevBack) {
                queue->continueMovie = 0;
                task->state          = task->state + 1;
            }
            break;
        case WAIT_SEEK_END:
            _akropolisPlazaUpdateTraversalEdgeFlags(task->work);
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            task->state = CHECK_EDGES;
            goto checkEdges;
        case AT_END:
            _akropolisPlazaUpdateTraversalEdgeFlags(task->work);
            // At a movie's end only turning back goes on: a forward movie waits for the back edge, a reverse one for
            // the forward edge.
            switch (queue->plazaStreamSubId) {
                case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_FORWARD:
                case 2:
                    if (work->back != EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = CHECK_EDGES;
                    }
                    break;
                case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_REVERSE:
                case 3:
                    if (work->fwd != EDGE_NONE) {
                        queue->movieAtEnd = 0;
                        task->state       = CHECK_EDGES;
                    }
                    break;
            }
            break;
    }

    if (queue->movieReady != 0) {
        _akropolisPlazaUpdateTraversalCollisionWalls(task);
        _akropolisPlazaApplyMovieCamera(queue->plazaStreamSubId);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Selects a scene's movie (`AKROPOLIS_PLAZA_MOVIE_*`) from its first frame, before it is queued.
static inline void _akropolisPlazaSelectSceneMovie(CdCmdQueue* queue, u16 movie)
{
    queue->sceneFrame       = 1;
    queue->movieFrame       = 1;
    queue->plazaStreamSubId = movie;
}

/// Stops the traversal for an event task's scene, remembering its frame to resume from.
static inline void _akropolisPlazaStopTraversal(Task* task, CdCmdQueue* queue)
{
    ((_SequenceWork*)task->spawnArg2.pointer)->resumeFrame = queue->sceneFrame;
    taskKill(((_SequenceWork*)task->spawnArg2.pointer)->traversalTask);
}

/// Allocates an event task's work, caches the player, or kills it and returns.
#define INITIALIZE_EVENT_WORK(eventTask)                                                       \
    {                                                                                          \
        _EventWork* allocatedWork;                                                             \
        allocatedWork     = memMalloc(sizeof(*allocatedWork), false);                          \
        (eventTask)->work = allocatedWork;                                                     \
        if (allocatedWork == NULL) {                                                           \
            taskKill(eventTask);                                                               \
            return;                                                                            \
        }                                                                                      \
        memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));                                \
        ((_EventWork*)(eventTask)->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER); \
    }

/// Showing the badge to the police sergeant at the barricade (CAP 1): the player walks up to him, then the
/// traversal stops and the scene's script runs; it can be skipped.
static void _akropolisPlazaSergeantSceneTask(Task* task)
{
    enum {
        INIT        = 0,
        WAIT_MOVE   = 1,
        WAIT_TURN   = 2,
        WAIT_STREAM = 3,
        WAIT_SCRIPT = 4,
    };
    ActorTransform destination;
    ActorTransform heading;
    CdCmdQueue*    queue = &gCdCmdQueue;
    _EventWork*    work  = task->work;

    switch (task->state) {
        case INIT:
            INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = AKROPOLIS_PLAZA_SERGEANT_SPOT_X;
            destination.pos.vy = 0;
            destination.pos.vz = AKROPOLIS_PLAZA_SERGEANT_SPOT_Z;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = AKROPOLIS_PLAZA_SCENE_FACING;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case WAIT_STREAM:
            // The traversal resumes from this frame after the scene.
            if (cdCmdIsIdle() == 0) {
                return;
            }
            _akropolisPlazaStopTraversal(task, queue);
            evsStartScriptWithSkip(_gAkropolisPlazaSergeantSceneScript, EVENT_SCRIPT_HUD_KEEP, _gAkropolisPlazaSergeantSceneSkipScript);
            task->state = task->state + 1;
            return;
        case WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

/// Queues a movie command for one of the plaza's movies, from its start.
static inline void _akropolisPlazaQueueStreamAtStart(u8 command, u8 subId)
{
    u8 streamArgs[4];

    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, subId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(command, 0, streamArgs);
}

/// Finds the live enemy spawned from a placement entry of the current area, as its place key: the entry's index in
/// the placement list, the stage and the area. An absent entry gives the list's end.
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

/// The blend the scenes return the player to its weapon bank's idle with.
enum {
    PLAYER_BLEND_FRAMES = 10,
};

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

    root             = ((_EventWork*)task->work)->playerTask->extra.tmd->coords;
    placement.pos.vx = root->coord.t[0];
    placement.pos.vy = root->coord.t[1];
    placement.pos.vz = root->coord.t[2];
    placement.rot.vz = 0;
    placement.rot.vx = 0;
    placement.rot.vy = yaw;
    TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

/// The helicopter wreck (CAP 9): the player walks to the wreck and its movie plays with the scene's script, the player
/// acting its clip; at the frame the movie hands back, the player is placed where the clip left it, idle and facing
/// along the street.
static void _akropolisPlazaHelicopterWreckSceneTask(Task* task)
{
    enum {
        INIT        = 0,
        WAIT_MOVE   = 1,
        WAIT_TURN   = 2,
        WAIT_IDLE   = 3,
        WAIT_READY  = 4,
        WAIT_REJOIN = 5,
        WAIT_END    = 6,
    };
    ActorTransform       destination;
    ActorTransform       heading;
    AnimationPlayRequest animationRequest;
    CdCmdQueue*          queue = &gCdCmdQueue;
    _EventWork*          work  = task->work;

    switch (task->state) {
        case INIT:
            INITIALIZE_EVENT_WORK(task);
            destination.pos.vx = AKROPOLIS_PLAZA_HELICOPTER_SPOT_X;
            destination.pos.vy = 0;
            destination.pos.vz = AKROPOLIS_PLAZA_HELICOPTER_SPOT_Z;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
            task->state = task->state + 1;
            return;
        case WAIT_MOVE:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = AKROPOLIS_PLAZA_SCENE_FACING;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            animationRequest.source.sets          = gPoliceOfficersPlayerAnimSets;
            animationRequest.animationId          = POLICE_SCENE_PLAYER_ANIMATION_LOOK_AT_WRECK;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0);
            task->state = task->state + 1;
            return;
        case WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            _akropolisPlazaStopTraversal(task, queue);
            _akropolisPlazaSelectSceneMovie(queue, AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK);
            _akropolisPlazaQueueStreamAtStart(CD_COMMAND_RESET_STREAM_AT_OFFSET, AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            evsStartScript(_gAkropolisPlazaHelicopterWreckScript, EVENT_SCRIPT_HUD_KEEP);
            task->state = task->state + 1;
            return;
        case WAIT_REJOIN:
            // Where the clip left the player becomes its placement.
            if (queue->movieFrame >= AKROPOLIS_PLAZA_HELICOPTER_WRECK_REJOIN_FRAME) {
                _akropolisPlazaPlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, PLAYER_BLEND_FRAMES);
                _akropolisPlazaPlacePlayerAtModelRoot(task, AKROPOLIS_PLAZA_FACING_AFTER_HELICOPTER);
                task->state = task->state + 1;
            }
            break;
        case WAIT_END:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskRequestKill(task, 0);
            }
            break;
        default:
            return;
    }
    _akropolisPlazaApplyMovieCamera(CAMERA_HELICOPTER_WRECK);
}

/// Prepares one of the plaza's movies at its start, paused, in the caller's command buffer.
static inline void _akropolisPlazaResetStreamInBuffer(u8 streamArgs[4], u8 movieSubId)
{
    streamArgs[0] = streamFindMovieSlot(&gGameSession->location, movieSubId, 0);
    streamArgs[1] = 0;
    streamArgs[2] = 0;
    cdCmdEnqueue(CD_COMMAND_RESET_STREAM_AT_OFFSET, 0, streamArgs);
}

/// Showing the badge to the officer at the tower's entrance (CAP 4, skippable), then entering the tower: the player
/// walks inside as the officer turns, the screen fades out, and the room's graphics are rebuilt for the officers'
/// talk outside (CAP 5, then CAP 10 as its movie plays), which Start skips. A skipped badge scene goes straight to
/// the rebuild.
static void _akropolisPlazaEntranceSceneTask(Task* task)
{
    enum {
        INIT                  = 0,
        WAIT_WALK             = 1,
        WAIT_TURN             = 2,
        START_BADGE_MOVIE     = 3,
        WAIT_BADGE_MOVIE      = 4,
        WAIT_BADGE_SCRIPT     = 5,
        START_ENTERING        = 6,
        WAIT_ENTERING         = 7,
        REBUILD_GRAPHICS      = 8,
        SHOW_OFFICERS_TALK    = 9,
        WAIT_OFFICERS_TALK    = 10,
        WAIT_FADE_IN          = 11,
        WAIT_DISPLAY          = 12,
        WAIT_OFFICERS_SCRIPT  = 13,
        WAIT_OFFICERS_MOVIE   = 14,
        WAIT_END              = 15,
        OFFICER_BLEND_FRAMES  = 10,
        FADE_OUT_STEP         = 9,
        FADE_IN_STEP          = 8,
        SKIP_MUSIC_FADE_TICKS = 10,
        MUSIC_FADE_TICKS      = 480,
        AMBIENCE_STOP_TICKS   = 180,
        AMBIENCE_VOLUME       = 38,
    };

    ActorTransform       walkDestination;
    ActorTransform       heading;
    u8                   streamArgs[4];
    ActorTransform       insideDestination;
    AnimationPlayRequest officerAnimation;
    CdCmdQueue*          queue = &gCdCmdQueue;
    _EventWork*          work  = task->work;

    switch (task->state) {
        case INIT:
            INITIALIZE_EVENT_WORK(task);
            walkDestination.pos.vx = AKROPOLIS_PLAZA_ENTRANCE_SPOT_X;
            walkDestination.pos.vy = 0;
            walkDestination.pos.vz = AKROPOLIS_PLAZA_ENTRANCE_SPOT_Z;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &walkDestination, 0);
            task->state = task->state + 1;
            return;
        case WAIT_WALK:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            heading.rot.vy = ACTOR_TRANSFORM_ANGLE_TURN;
            TASK_MESSAGE_DISPATCH_POINTER(((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &heading, 0);
            task->state = task->state + 1;
            return;
        case WAIT_TURN:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case START_BADGE_MOVIE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            taskKill(((_SequenceWork*)task->spawnArg2.pointer)->traversalTask);
            _akropolisPlazaSelectSceneMovie(queue, AKROPOLIS_PLAZA_MOVIE_ENTRANCE_OFFICER_TALK);
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_MOVIE_ENTRANCE_OFFICER_TALK);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case WAIT_BADGE_MOVIE:
            if (cdCmdIsIdle() != 0) {
                evsStartScriptWithSkip(_gAkropolisPlazaEntranceOfficerTalkScript, EVENT_SCRIPT_HUD_KEEP, _gAkropolisPlazaEntranceOfficerTalkSkipScript);
                task->state = task->state + 1;
                return;
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_ENTRANCE_OFFICER_TALK);
            return;
        case WAIT_BADGE_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (gGameSession->evtSkipped != 0) {
                SetDispMask(0);
                task->state = REBUILD_GRAPHICS;
                return;
            }
            _akropolisPlazaSelectSceneMovie(queue, AKROPOLIS_PLAZA_MOVIE_ENTERING_TOWER);
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_MOVIE_ENTERING_TOWER);
            queue->continueMovie = 1;
            task->state          = task->state + 1;
            return;
        case START_ENTERING:
            if (queue->movieReady == 0) {
                return;
            }
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            insideDestination.pos.vx = AKROPOLIS_PLAZA_ENTRANCE_SPOT_X;
            insideDestination.pos.vy = 0;
            insideDestination.pos.vz = AKROPOLIS_PLAZA_INSIDE_SPOT_Z;
            TASK_MESSAGE_DISPATCH_POINTER(
                ((_EventWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &insideDestination, 0);
            officerAnimation.source.index         = POLICE_OFFICERS_FOLLOW_UP_ROOKIE;
            officerAnimation.animationId          = POLICE_ROOKIE_ANIMATION_WATCH_AYA_ENTER;
            officerAnimation.blend                = ANIMATION_BLEND_RESET;
            officerAnimation.blendFrames          = OFFICER_BLEND_FRAMES;
            officerAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(_akropolisPlazaFindPlacedEnemy(POLICE_OFFICERS_PLACEMENT_ROOKIE)->task, ACTOR_MESSAGE_PLAY_ANIMATION, &officerAnimation, 0);
            task->state         = task->state + 1;
            work->elapsedFrames = 0;
            return;
        case WAIT_ENTERING:
            if (work->elapsedFrames == AKROPOLIS_PLAZA_ENTRANCE_CUE_FRAME) {
                sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ENTERING_TOWER_CUE), 0, 0);
            }
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= AKROPOLIS_PLAZA_ENTRANCE_FADE_OUT_FRAME) {
                taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_FADE_OUT, FADE_OUT_STEP, 0);
                task->state = task->state + 1;
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_ENTERING_TOWER);
            return;
        // The tower's entrance leaves the scene: the officer takes his culled body, the player is hidden and its buffers
        // released, and the room's image memory is laid out again.
        case REBUILD_GRAPHICS:
            if (cdCmdIsIdle() != 0) {
                taskMessageDispatch(_akropolisPlazaFindPlacedEnemy(POLICE_OFFICERS_PLACEMENT_ROOKIE)->task, POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY, POLICE_OFFICERS_CULLED_BODY_ROOKIE, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE, 0);
                gpuResetAndInvalidateModelBuffers();
                memConfigureImageMemory(gGameSession->location.stage, gGameSession->location.area);
                memSelectAuxHeapRegion(true);
                tmdResetAuxHeapAndRestoreBuffers();
                sndEvtRequestScriptVolume(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE), AMBIENCE_VOLUME);
                task->state = task->state + 1;
                return;
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_ENTERING_TOWER);
            return;
        case SHOW_OFFICERS_TALK:
            _akropolisPlazaSelectSceneMovie(queue, AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK);
            _akropolisPlazaResetStreamInBuffer(streamArgs, AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK);
            queue->continueMovie = 0;
            task->state          = task->state + 1;
            /* fallthrough */
        case WAIT_OFFICERS_TALK:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            evsStartScript(_gAkropolisPlazaOfficersTalkScript, EVENT_SCRIPT_HUD_KEEP);
            work->elapsedFrames = 0;
            task->state         = task->state + 1;
            return;
        case WAIT_FADE_IN:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= AKROPOLIS_PLAZA_ENTRANCE_FADE_IN_FRAME) {
                sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICERS_WALK_IN), 0, 0);
                taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_FADE_IN, FADE_IN_STEP, 0);
                work->elapsedFrames = 0;
                task->state         = task->state + 1;
            }
            return;
        case WAIT_DISPLAY:
            work->elapsedFrames = work->elapsedFrames + 1;
            if (work->elapsedFrames >= 2) {
                SetDispMask(1);
                task->state = task->state + 1;
            }
            return;
        case WAIT_OFFICERS_SCRIPT:
            if (padIsStartPressed() != 0) {
                stageMusicRequestAreaStop(SKIP_MUSIC_FADE_TICKS);
                cdCmdRequestCancel();
                task->state = WAIT_END;
            } else if (gGameSession->eventState == 0) {
                stageMusicRequestAreaStop(MUSIC_FADE_TICKS);
                _akropolisPlazaSelectSceneMovie(queue, AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK);
                _akropolisPlazaQueueStreamAtStart(CD_COMMAND_PLAY_STREAM_AT_OFFSET, AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK);
                queue->continueMovie = 1;
                task->state          = task->state + 1;
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_OFFICERS_TALK);
            return;
        case WAIT_OFFICERS_MOVIE:
            if (queue->movieReady != 0) {
                evsStartScript(_gAkropolisPlazaOfficersTalkEndScript, EVENT_SCRIPT_HUD_KEEP);
                task->state = task->state + 1;
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_OFFICERS_TALK);
            return;
        case WAIT_END:
            if (cdCmdIsIdle() != 0) {
                sndEvtRequestScriptStop(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE), AMBIENCE_STOP_TICKS);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
                taskRequestKill(task, 0);
            }
            _akropolisPlazaApplyMovieCamera(CAMERA_OFFICERS_TALK);
            return;
        default:
            return;
    }
}

#undef INITIALIZE_EVENT_WORK

/// Talking to the sergeant again (CAP 2, then 3, then 8 every later time): the player stands idle, the traversal
/// stops and the visit's script runs.
static void _akropolisPlazaSergeantAgainSceneTask(Task* task)
{
    enum {
        INIT        = 0,
        WAIT_STREAM = 1,
        WAIT_SCRIPT = 2,
    };
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case INIT:
            _akropolisPlazaPlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, PLAYER_BLEND_FRAMES);
            task->state = task->state + 1;
            break;
        case WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                _akropolisPlazaStopTraversal(task, queue);
                switch (task->spawnArg1.value) {
                    case SERGEANT_AGAIN_FIRST:
                        evsStartScript(_gAkropolisPlazaSergeantAgain1Script, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case SERGEANT_AGAIN_SECOND:
                        evsStartScript(_gAkropolisPlazaSergeantAgain2Script, EVENT_SCRIPT_HUD_KEEP);
                        break;
                    case SERGEANT_AGAIN_LATER:
                        evsStartScript(_gAkropolisPlazaSergeantAgainLaterScript, EVENT_SCRIPT_HUD_KEEP);
                        break;
                }
                task->state = task->state + 1;
            }
            break;
        case WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// A caption trigger's line (CAP 6, the dead SWAT member, or CAP 7, the parked car): the player stands idle, the
/// traversal stops and the trigger's caption runs.
static void _akropolisPlazaCaptionEventTask(Task* task)
{
    enum {
        INIT        = 0,
        WAIT_STREAM = 1,
        END         = 2,
    };

    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case INIT:
            _akropolisPlazaPlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, PLAYER_BLEND_FRAMES);
            task->state = task->state + 1;
            break;
        case WAIT_STREAM:
            if (cdCmdIsIdle() != 0) {
                _akropolisPlazaStopTraversal(task, queue);
                capRunCommandWithTransition((s8)((_SequenceWork*)task->spawnArg2.pointer)->eventKind);
                task->state = task->state + 1;
            }
            break;
        case END:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 1, 0);
            taskRequestKill(task, 0);
            break;
    }
}

/// Plays one ambience voice over a range of traversal frames, its volume following `envelope`
/// (`AKROPOLIS_PLAZA_AMBIENCE_*`): a triangle peaking at `peakFrame`, a quieter one, or the entry's early fade, which
/// has fixed frames. Outside the range the voice stops.
static void _akropolisPlazaUpdateAmbienceVoice(u16 firstFrame, u16 lastFrame, u16 peakFrame, u16* startRequested, s32 soundId, u16 envelope)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    u32         sceneFrame;
    u8          volume;

    sceneFrame = queue->sceneFrame;
    if (sceneFrame >= firstFrame && sceneFrame <= lastFrame) {
        // A voice starts silent and takes its volume from the next frame on. The loop, once a macro's, weights the
        // register allocation the original has.
        do {
            if (*startRequested == false) {
                sndEvtRequestScriptStart(soundId, 0, AMBIENCE_SILENT_ATTENUATION);
                sndEvtRequestScriptVolume(soundId, 0);
                *startRequested = true;
                return;
            }
        } while (0);
        if (envelope == AMBIENCE_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * AMBIENCE_FULL_VOLUME) / (peakFrame - firstFrame);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * AMBIENCE_FULL_VOLUME) / (lastFrame - peakFrame);
            }
        } else if (envelope == AMBIENCE_QUIET_TRIANGLE) {
            if (sceneFrame < peakFrame) {
                volume = ((queue->sceneFrame - firstFrame) * (AMBIENCE_FULL_VOLUME * 3)) / ((peakFrame - firstFrame) * 4);
            } else {
                volume = ((lastFrame - queue->sceneFrame) * (AMBIENCE_FULL_VOLUME * 3)) / ((lastFrame - peakFrame) * 4);
            }
        } else if (sceneFrame < (u32)AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) {
            volume = (((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - queue->sceneFrame) * AMBIENCE_FULL_VOLUME) / ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE - 1) * 4)) + AMBIENCE_BASE_VOLUME;
        } else {
            volume = ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - queue->sceneFrame) * (AMBIENCE_FULL_VOLUME * 3)) / ((AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END - AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE) * 4);
        }
        sndEvtRequestScriptVolume(soundId, volume);
        return;
    }
    if (*startRequested != 0) {
        sndEvtRequestScriptStop(soundId, SOUND_SCRIPT_STOP_NO_FADE);
        *startRequested = 0;
    }
}

/// The plaza's ambience: in the traversal, four voices over the street's stretches; in the helicopter wreck's movie,
/// the officer ambience swells and fades with it.
static void _akropolisPlazaUpdateTraversalAmbience(Task* task)
{
    CdCmdQueue*    queue            = &gCdCmdQueue;
    volatile u16*  sceneFrameSource = &gCdCmdQueue.sceneFrame;
    _SequenceWork* work             = task->work;
    s32            sceneFrame;
    s32            volume;

    switch (gCdCmdQueue.plazaStreamSubId) {
        case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_FORWARD:
        case AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_REVERSE:
            _akropolisPlazaUpdateAmbienceVoice(1, AKROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME, 1, &work->walkAmbiencePlaying, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE), AMBIENCE_TRIANGLE);
            _akropolisPlazaUpdateAmbienceVoice(1, AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END, 1, &work->entryAmbiencePlaying, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE), AMBIENCE_EARLY_FADE);
            _akropolisPlazaUpdateAmbienceVoice(AKROPOLIS_PLAZA_AMBIENCE_APPROACH_FIRST_FRAME, AKROPOLIS_PLAZA_AMBIENCE_APPROACH_LAST_FRAME, AKROPOLIS_PLAZA_AMBIENCE_APPROACH_PEAK_FRAME, &work->approachAmbiencePlaying, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_APPROACH_AMBIENCE), AMBIENCE_TRIANGLE);
            _akropolisPlazaUpdateAmbienceVoice(AKROPOLIS_PLAZA_AMBIENCE_OFFICER_FIRST_FRAME, AKROPOLIS_PLAZA_AMBIENCE_OFFICER_LAST_FRAME, AKROPOLIS_PLAZA_AMBIENCE_OFFICER_PEAK_FRAME, &work->officerAmbiencePlaying, AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE), AMBIENCE_QUIET_TRIANGLE);
            break;
        case AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK:
            // Compare a snapshot, then reread the frame published by MDEC completion.
            sceneFrame = queue->sceneFrame;
            if ((u32)(sceneFrame - AKROPOLIS_PLAZA_AMBIENCE_WRECK_FULL_FIRST_FRAME) < (u32)AKROPOLIS_PLAZA_AMBIENCE_WRECK_FULL_FRAME_COUNT) {
                volume = AMBIENCE_FULL_VOLUME;
            } else if (sceneFrame < (u32)AKROPOLIS_PLAZA_AMBIENCE_WRECK_RISE_END_FRAME) {
                volume = ((*sceneFrameSource * AMBIENCE_FULL_VOLUME) / AKROPOLIS_PLAZA_AMBIENCE_WRECK_RAMP_FRAMES) + AMBIENCE_BASE_VOLUME;
            } else {
                volume = (((AKROPOLIS_PLAZA_AMBIENCE_WRECK_FALL_ANCHOR_FRAME - *sceneFrameSource) * AMBIENCE_FULL_VOLUME) / AKROPOLIS_PLAZA_AMBIENCE_WRECK_RAMP_FRAMES) + AMBIENCE_BASE_VOLUME;
            }
            sndEvtRequestScriptVolume(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE), volume & 0xFF);
            break;
    }
}

/// Restarts the traversal at the frame a scene stopped it on, without resetting the movie.
static inline void _akropolisPlazaResumeTraversal(_SequenceWork* work)
{
    work->traversalArg.skipStreamReset = 1;
    work->traversalArg.startFrame      = work->resumeFrame;
    work->traversalTask                = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_TRAVERSAL, 0, &work->traversalArg);
}

/// What `_akropolisPlazaUpdateSequenceEvent` returns.
enum {
    SEQUENCE_RUNNING       = 0,
    SEQUENCE_ENTERED_TOWER = 1,
};

/// The plaza's story: waits for the sergeant at the barricade, whose scene lets the player through (captions can run
/// before it); then for the helicopter wreck, the entrance or the sergeant again, any number of captions. Each
/// trigger's scene stops the traversal and resumes it afterwards; the helicopter's and the entrance's triggers go once
/// done. Returns `SEQUENCE_ENTERED_TOWER` after the entrance's scene.
static u16 _akropolisPlazaUpdateSequenceEvent(Task* task)
{
    CdCmdQueue*    queue = &gCdCmdQueue;
    _SequenceWork* work  = task->work;
    u16            eventControl;
    u8             eventKind;
    u8             eventParameter1;
    s32            firstSceneResult;
    s32            sceneResult;
    s32            repeatOrCaptionResult;
    s16            eventReady;
    u16            completedStep;
    s32            signedEventKind;
    u32            latchedKind;
    u16            latchedKind16;

    // An automatic trigger fires as the player walks into it, any other on a fresh press of the action button. The
    // trigger's words are read even when none fired.
    eventReady = worldCollisionReadActionHit(&eventControl, &eventKind, &eventParameter1);
    if (!((s16)eventControl & WORLD_COLLISION_TRIGGER_AUTOMATIC) && (eventReady != 0)) {
        eventReady = gPlayerStatus.interactionPressed != 0;
    }

    switch (work->step) {
        case STEP_AWAIT_SERGEANT:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                if ((s8)eventKind == AKROPOLIS_PLAZA_EVENT_SERGEANT) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_SERGEANT, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if ((s8)eventKind >= AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_CAPTION, 0, work);
                    work->step = STEP_RUN_FIRST_CAPTION;
                }
            }
            break;
        case STEP_RUN_SERGEANT:
            if (taskPollKill(work->eventTask, &firstSceneResult) != 0) {
                _akropolisPlazaResumeTraversal(work);
                worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAkropolisPlazaSergeantTrigger);
                work->step = work->step + 1;
            }
            break;
        case STEP_AWAIT_EVENT:
            if (eventReady != 0) {
                work->eventControl    = eventControl;
                work->eventKind       = eventKind;
                work->eventParameter1 = eventParameter1;
                work->resumeFrame     = queue->sceneFrame;
                signedEventKind       = (s8)eventKind;
                if (signedEventKind == AKROPOLIS_PLAZA_EVENT_HELICOPTER_WRECK) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_HELICOPTER_WRECK, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == AKROPOLIS_PLAZA_EVENT_ENTRANCE) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_ENTRANCE, 0, work);
                    work->step = work->step + 1;
                    break;
                } else if (signedEventKind == AKROPOLIS_PLAZA_EVENT_SERGEANT_AGAIN) {
                    if (work->sergeantAgainVisit == SERGEANT_AGAIN_FIRST) {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_SERGEANT_AGAIN, SERGEANT_AGAIN_FIRST, work);
                        work->step = STEP_RUN_SERGEANT_AGAIN;
                    } else if (work->sergeantAgainVisit == SERGEANT_AGAIN_SECOND) {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_SERGEANT_AGAIN, SERGEANT_AGAIN_SECOND, work);
                        work->step = STEP_RUN_SERGEANT_AGAIN;
                    } else {
                        work->eventTask =
                            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_SERGEANT_AGAIN, SERGEANT_AGAIN_LATER, work);
                        work->step = STEP_RUN_SERGEANT_AGAIN;
                    }
                } else if (signedEventKind >= AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION) {
                    work->eventTask =
                        taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_CAPTION, 0, work);
                    work->step = STEP_RUN_CAPTION;
                }
            }
            break;
        case STEP_RUN_SCENE:
            if (taskPollKill(work->eventTask, &sceneResult) != 0) {
                // Keep the signed-byte extension and unsigned-halfword comparison widths.
                latchedKind   = work->eventKind;
                latchedKind16 = (s8)latchedKind;
                if (latchedKind16 == AKROPOLIS_PLAZA_EVENT_HELICOPTER_WRECK) {
                    worldCollisionUnlinkTrigger(0, &_gAkropolisPlazaHelicopterWreckTrigger);
                } else if ((u16)(s8)latchedKind == AKROPOLIS_PLAZA_EVENT_ENTRANCE) {
                    worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, _gAkropolisPlazaEntranceAndExamineTriggers);
                }
                work->step = work->step - 1;
                if ((s8)work->eventKind == AKROPOLIS_PLAZA_EVENT_ENTRANCE) {
                    return SEQUENCE_ENTERED_TOWER;
                }
                _akropolisPlazaResumeTraversal(work);
            }
            break;
        case STEP_RUN_SERGEANT_AGAIN:
        case STEP_RUN_FIRST_CAPTION:
        case STEP_RUN_CAPTION:
            if (taskPollKill(work->eventTask, &repeatOrCaptionResult) != 0) {
                _akropolisPlazaResumeTraversal(work);
                completedStep = work->step;
                if (completedStep == STEP_RUN_FIRST_CAPTION) {
                    work->step = STEP_AWAIT_SERGEANT;
                } else {
                    if (completedStep != STEP_RUN_CAPTION) {
                        if (work->sergeantAgainVisit < SERGEANT_AGAIN_LATER) {
                            work->sergeantAgainVisit = work->sergeantAgainVisit + 1;
                        }
                    }
                    work->step = STEP_AWAIT_EVENT;
                }
            }
            break;
    }
    return SEQUENCE_RUNNING;
}

/// Draws black bars over the top and bottom 24 pixels of the cinematic view.
static void _akropolisPlazaLetterboxTask(Task* task)
{
    enum { LETTERBOX_TAG = 3 };
    TILE* tile;

    /// Queues one cinematic bar. Both bars share this `tile` local, as the original allocates them.
#define DRAW_LETTERBOX_BAR(topY)          \
    tile           = gGpuPrimCursor;      \
    gGpuPrimCursor = tile + 1;            \
    SetTile(tile);                        \
    tile->b0 = 0;                         \
    tile->g0 = 0;                         \
    tile->r0 = 0;                         \
    tile->x0 = -DISPLAY_SCREEN_WIDTH / 2; \
    tile->y0 = (topY);                    \
    tile->w  = DISPLAY_SCREEN_WIDTH;      \
    tile->h  = LETTERBOX_HEIGHT;          \
    addPrim(gGpuCurrentOt + LETTERBOX_TAG, tile);

    DRAW_LETTERBOX_BAR(-DISPLAY_SCREEN_HEIGHT / 2);
    DRAW_LETTERBOX_BAR(DISPLAY_SCREEN_HEIGHT / 2 - LETTERBOX_HEIGHT);
#undef DRAW_LETTERBOX_BAR
}

/// Sets the location the reload goes to after the player enters the tower (`AKROPOLIS_PLAZA_ELEVATOR_*`).
static inline void _akropolisPlazaCommitElevatorDestination(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_WEST_ELEVATOR_HALL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                 = AKROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT;
}

/// The plaza's room sequence: plays the opening movie, then the traversal with its scenes until the player enters the
/// tower, and reloads into the west elevator hall. Input is held and the game cannot pause throughout.
static void _akropolisPlazaSequenceTask(Task* task)
{
    enum {
        INITIALIZE        = 0,
        WAIT_FIRST_FRAME  = 1,
        WAIT_SECOND_FRAME = 2,
        START_STREAM      = 3,
        RUN               = 4,
        LEAVE             = 5,
        FADE_IN_STEP      = 6,
    };
    CdCmdQueue*    queue = &gCdCmdQueue;
    _SequenceWork* work  = task->work;
    _SequenceWork* allocatedWork;
    _SequenceWork* initializedWork;
    Task*          childTask;
    SVECTOR        ambientColor;

    switch (task->state) {
        case INITIALIZE:
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
            childTask                   = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_OPENING_MOVIE_SETUP, 0, 0);
            initializedWork             = task->work;
            initializedWork->eventTask  = childTask;
            playerActorRemoveEquipment();
            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_LETTERBOX, 0, 0);
            task->state = task->state + 1;
            return;
        case WAIT_FIRST_FRAME:
        case WAIT_SECOND_FRAME:
            task->state = task->state + 1;
            return;
        case START_STREAM:
            // The traversal borrows its argument from this work until the sequence ends.
            ambientColor.vx = AKROPOLIS_PLAZA_TRAVERSAL_AMBIENT_LEVEL;
            ambientColor.vy = AKROPOLIS_PLAZA_TRAVERSAL_AMBIENT_LEVEL;
            ambientColor.vz = AKROPOLIS_PLAZA_TRAVERSAL_AMBIENT_LEVEL;
            worldCoordSetAmbientColorOverride(&ambientColor);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->traversalArg.skipStreamReset = 0;
            work->traversalArg.startFrame      = 0;
            work->traversalTask                = taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_TRAVERSAL, 0, &work->traversalArg);
            stageMusicRequestAreaStart(0);
            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_FADE_IN, FADE_IN_STEP, 0);
            queue->blockGamePause = 1;
            task->state           = task->state + 1;
            return;
        case RUN:
            _akropolisPlazaUpdateTraversalAmbience(task);
            if (_akropolisPlazaUpdateSequenceEvent(task) == SEQUENCE_RUNNING) {
                return;
            }
            task->state = task->state + 1;
            return;
        case LEAVE:
            _akropolisPlazaCommitElevatorDestination();
            loadingEnqueueEquippedWeaponResources();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            queue->blockGamePause = 0;
            taskKill(task);
            return;
    }
}

/// Hands the display loop to the opening movie, which runs as a display task.
static void _akropolisPlazaStartOpeningMovieTask(Task* task)
{
    displaySpawnTaskFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_TASK_OPENING_MOVIE, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

void akropolisPlazaSirenLightTask(Task* task)
{
    enum {
        LIGHT_SIREN_YAW_WINDOW             = 0x200,
        LIGHT_POINT_INNER_RADIUS           = 0x600,
        LIGHT_REFRESH_FRAMES               = 2,
        LIGHT_POINT_COLOR_SHIFT            = 4,
        SIREN_BEAM_HALF_WIDTH              = 0x200,
        SIREN_FRINGE_HALF_WIDTH            = 0x400,
        SIREN_FRINGE_Z                     = 0x400,
        SIREN_FRINGE_NEAR_Z                = 0x100,
        SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR = 0xC000
    };
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    GfxCoord*                      coord;
    GfxCoord*                      lightCoord;
    EffectWork*                    work;
    _BeamScratch*                  beam;
    POLY_G3*                       beamTriangle;
    POLY_G4*                       primitive;
    s32                            index;
    u32                            brightness;
    u16                            red, green, blue;
    s32                            slot, packedBlueBrightness;
    u32                            packedRedBrightness;
    s16                            falloffWidth, beamLength;
    u16                            yaw;

    // The effect's work holds the sweep: `scale` is the yaw, `angle` the beam's length and `period` how far the point
    // light reaches beyond its inner radius.
    slot       = task->spawnArg1.value;
    lightSlot  = &gWorldCoordTransientPointLights[slot & (ARRAY_SIZE(gWorldCoordTransientPointLights) - 1)];
    light      = &lightSlot->light;
    coord      = task->extra.coordBody->coord;
    work       = task->spawnArg2.pointer;
    lightCoord = &light->head.transform.coord;
    if (task->state == 0) {
        work->scale = (slot & 1) << LIGHT_HALF_TURN_SHIFT;
        task->state = task->state + 1;
    }
    // Compose the sweeping beacon before projecting its origin and outline.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    beam                                                  = SCRATCH_STACK_RESERVE_BLOCK(_BeamScratch);
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vx = coord->workm.t[0];
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vy = coord->workm.t[1];
    beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER]);
    gte_rtps();
    gte_stsxy(&beam->centerScreenPos);
    gte_stszotz(&beam->otz);
    // A culled origin leaves this slot inactive for the frame.
    lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
    if (beam->otz >= GLOW_MIN_DEPTH) {
        if (__builtin_abs(beam->centerScreenPos.vx) < LIGHT_CULL_X && __builtin_abs(beam->centerScreenPos.vy) < LIGHT_CULL_Y) {
            RANDOM_LCG_ADVANCE();
            brightness = (RANDOM_LCG_DRAW() & LIGHT_FLICKER_INTENSITY_MASK) | LIGHT_FLICKER_BASE_INTENSITY;
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_FIRST_RED_SIREN) {
                work->angle = ABS(work->scale - GLOW_HALF_TURN) > LIGHT_SIREN_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
                red         = brightness >> 1;
                green       = brightness >> 1;
                blue        = brightness;
            } else {
                work->angle = ABS(work->scale - GLOW_HALF_TURN) < GLOW_HALF_TURN - LIGHT_SIREN_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
                red         = brightness;
                green       = red >> 1;
                blue        = red >> 1;
            }
            beamLength = work->angle;
            // Refresh the point light; only a long beam adds yaw-dependent falloff.
            falloffWidth                                              = beamLength != LIGHT_LONG_REACH ? 0 : (yaw = work->scale, work->scale - GLOW_HALF_TURN >= 0 ? (yaw - GLOW_HALF_TURN) * 4 : (beamLength - yaw) * 4);
            work->period                                              = falloffWidth;
            lightCoord->coord.t[0]                                    = coord->coord.t[0];
            lightCoord->coord.t[1]                                    = coord->coord.t[1];
            lightCoord->coord.t[2]                                    = coord->coord.t[2];
            lightCoord->composeStamp                                  = GRAPHICS_COORD_DIRTY;
            lightSlot->framesLeft                                     = LIGHT_REFRESH_FRAMES;
            light->inner                                              = LIGHT_POINT_INNER_RADIUS;
            light->outer                                              = work->period + LIGHT_POINT_INNER_RADIUS;
            light->head.color.r                                       = red << LIGHT_POINT_COLOR_SHIFT;
            light->head.color.g                                       = green << LIGHT_POINT_COLOR_SHIFT;
            light->head.color.b                                       = blue << LIGHT_POINT_COLOR_SHIFT;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vx   = -SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vy   = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT].vz   = work->angle;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vx  = SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_RIGHT].vz  = work->angle;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vx  = -SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_LEFT].vz  = SIREN_FRINGE_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vx = SIREN_FRINGE_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vy = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_SIDE_RIGHT].vz = SIREN_FRINGE_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vx  = -SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vy  = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_LEFT].vz  = SIREN_FRINGE_NEAR_Z;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vx = SIREN_BEAM_HALF_WIDTH;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vy = 0;
            beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_NEAR_RIGHT].vz = SIREN_FRINGE_NEAR_Z;
            for (index = AKROPOLIS_PLAZA_BEAM_VERTEX_FAR_LEFT; index < AKROPOLIS_PLAZA_BEAM_VERTEX_COUNT; index++) {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&beam->vertices[index]);
                gte_rtv0();
                gte_stsv(&beam->vertices[index]);
                // Move the rotated outline vertex into world coordinates.
                TRANSLATE_LIGHT_VERTEX(&beam->vertices[index], coord);
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
            beam->glowRadius = SIREN_ORIGIN_GLOW_RADIUS_NUMERATOR / beam->otz;
            red            <<= 1;
            green          <<= 1;
            blue           <<= 1;
            DRAW_LIGHT_GLOW_FAN(beam->centerScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
        }
    }
    // The far glow has its own flicker and visibility test, even if the origin was culled.
    RANDOM_LCG_ADVANCE();
    brightness = (RANDOM_LCG_DRAW() & LIGHT_FLICKER_INTENSITY_MASK) | LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_FIRST_RED_SIREN) {
        work->angle          = ABS(work->scale - GLOW_HALF_TURN) > LIGHT_FAR_GLOW_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = (u32)packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - GLOW_HALF_TURN) < GLOW_HALF_TURN - LIGHT_FAR_GLOW_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (u32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == LIGHT_LONG_REACH) {
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vx = 0;
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vy = 0;
        beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER].vz = LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER]);
        gte_rtv0();
        gte_stsv(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER]);
        TRANSLATE_LIGHT_VERTEX(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER], coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&beam->vertices[AKROPOLIS_PLAZA_BEAM_VERTEX_CENTER]);
        gte_rtps();
        gte_stsxy(&beam->centerScreenPos);
        gte_stszotz(&beam->otz);
        if (beam->otz >= GLOW_MIN_DEPTH) {
            if (__builtin_abs(beam->centerScreenPos.vx) < LIGHT_CULL_X && __builtin_abs(beam->centerScreenPos.vy) < LIGHT_CULL_Y) {
                beam->glowRadius = LIGHT_FAR_GLOW_RADIUS_NUMERATOR / beam->otz;
                DRAW_LIGHT_GLOW_FAN(beam->centerScreenPos, beam->glowRadius, beam->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - LIGHT_YAW_STEP) & LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_BeamScratch);
}

void akropolisPlazaLightFlareTask(Task* task)
{
    enum {
        PULSE_STEP                    = 8,
        SLOT_PHASE_STEP               = 0xC0,
        RISING_LEVEL_MASK             = 0x78,
        ORIGIN_RADIUS_NUMERATOR       = 0xA000,
        RAY_TIP_RADIUS_NUMERATOR      = 0x8000,
        RAY_SHOULDER_RADIUS_NUMERATOR = 0x1800,
        FIRST_RAY_SAMPLE              = 3,
        OPPOSITE_RAY_SAMPLE_STEP      = 8
    };
    GfxCoord*      coord;
    EffectWork*    work;
    _FlareScratch* flare;
    POLY_G4*       primitive;
    s32            index;
    s16            brightness;
    u16            red, green, blue;
    s32            initialYaw;
    s32            pulsePhase, packedBlueBrightness;
    u32            packedRedBrightness;
    s16            pulseLevel;
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    if (task->state == 0) {
        initialYaw  = (task->spawnArg1.value & 1) << LIGHT_HALF_TURN_SHIFT;
        work->scale = initialYaw;
        task->state = task->state + 1;
    }
    // Sweep the placement frame; the origin glow remains at its world translation.
    gfxRotMatrixY(&coord->coord, work->scale, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    flare              = SCRATCH_STACK_RESERVE_BLOCK(_FlareScratch);
    flare->worldPos.vx = coord->workm.t[0];
    flare->worldPos.vy = coord->workm.t[1];
    flare->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&flare->worldPos);
    gte_rtps();
    gte_stsxy(&flare->screenPos);
    gte_stszotz(&flare->otz);
    if (flare->otz >= GLOW_MIN_DEPTH) {
        if (__builtin_abs(flare->screenPos.vx) < LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < LIGHT_CULL_Y) {
            pulsePhase = gDisplayState.animFrame * PULSE_STEP + task->spawnArg1.value * SLOT_PHASE_STEP;
            if (pulsePhase & LIGHT_PULSE_HALF_PERIOD) {
                pulseLevel = LIGHT_PULSE_LEVEL_MASK - (pulsePhase & LIGHT_PULSE_LEVEL_MASK);
            } else {
                pulseLevel = pulsePhase & RISING_LEVEL_MASK;
            }
            brightness = pulseLevel;
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_FIRST_RED_FLARE) {
                red   = brightness >> 2;
                green = brightness >> 2;
                blue  = brightness;
            } else {
                red   = brightness;
                green = (s16)red >> 2;
                blue  = (s16)red >> 2;
            }
            flare->outerRadius = ORIGIN_RADIUS_NUMERATOR / flare->otz;
            DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            // Overlay four diagonal rays, alternating double and single radii.
            flare->outerRadius = RAY_TIP_RADIUS_NUMERATOR / flare->otz;
            flare->innerRadius = RAY_SHOULDER_RADIUS_NUMERATOR / flare->otz;
            red              <<= 1;
            green            <<= 1;
            blue             <<= 1;
            for (index = FIRST_RAY_SAMPLE; index < LIGHT_FAN_SAMPLES; index += OPPOSITE_RAY_SAMPLE_STEP) {
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index]) >> (GLOW_TRIG_SHIFT - 1));
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 12]) >> (GLOW_TRIG_SHIFT - 1));
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> (GLOW_TRIG_SHIFT - 1));
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index]) >> (GLOW_TRIG_SHIFT - 1));
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> (GLOW_TRIG_SHIFT - 1));
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> (GLOW_TRIG_SHIFT - 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(flare->otz), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
                primitive      = gGpuPrimCursor;
                gGpuPrimCursor = primitive + 1;
                setPolyG4(primitive);
                setRGB0(primitive, 0, 0, 0);
                setRGB1(primitive, 0, 0, 0);
                setRGB2(primitive, red, green, blue);
                setRGB3(primitive, 0, 0, 0);
                primitive->x0 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> GLOW_TRIG_SHIFT);
                primitive->y0 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index]) >> GLOW_TRIG_SHIFT);
                primitive->x1 = flare->screenPos.vx + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> GLOW_TRIG_SHIFT);
                primitive->y1 = flare->screenPos.vy + ((flare->outerRadius * _gAkropolisPlazaLightRingSamples[index + 4]) >> GLOW_TRIG_SHIFT);
                primitive->x2 = flare->screenPos.vx;
                primitive->y2 = flare->screenPos.vy;
                primitive->x3 = flare->screenPos.vx + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 12]) >> GLOW_TRIG_SHIFT);
                primitive->y3 = flare->screenPos.vy + ((flare->innerRadius * _gAkropolisPlazaLightRingSamples[index + 8]) >> GLOW_TRIG_SHIFT);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(flare->otz), primitive);
                gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, flare->otz);
            }
        }
    }
    // Gate an independently flickering glow ahead of the flare by the sweep yaw.
    RANDOM_LCG_ADVANCE();
    brightness = (RANDOM_LCG_DRAW() & LIGHT_FLICKER_INTENSITY_MASK) | LIGHT_FLICKER_BASE_INTENSITY;
    if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_FIRST_RED_FLARE) {
        work->angle          = ABS(work->scale - GLOW_HALF_TURN) > LIGHT_FAR_GLOW_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
        packedBlueBrightness = brightness << 16;
        red                  = packedBlueBrightness >> 20;
        green                = packedBlueBrightness >> 20;
        blue                 = packedBlueBrightness >> 18;
    } else {
        work->angle         = ABS(work->scale - GLOW_HALF_TURN) < GLOW_HALF_TURN - LIGHT_FAR_GLOW_YAW_WINDOW ? LIGHT_LONG_REACH : LIGHT_SHORT_REACH;
        packedRedBrightness = brightness << 16;
        red                 = (s32)packedRedBrightness >> 18;
        green               = (s32)packedRedBrightness >> 20;
        blue                = (s32)packedRedBrightness >> 20;
    }
    if (work->angle == LIGHT_LONG_REACH) {
        flare->worldPos.vx = 0;
        flare->worldPos.vy = 0;
        flare->worldPos.vz = LIGHT_FAR_GLOW_Z;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&flare->worldPos);
        gte_rtv0();
        gte_stsv(&flare->worldPos);
        TRANSLATE_LIGHT_VERTEX(&flare->worldPos, coord);
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&flare->worldPos);
        gte_rtps();
        gte_stsxy(&flare->screenPos);
        gte_stszotz(&flare->otz);
        if (flare->otz >= GLOW_MIN_DEPTH) {
            if (__builtin_abs(flare->screenPos.vx) < LIGHT_CULL_X && __builtin_abs(flare->screenPos.vy) < LIGHT_CULL_Y) {
                flare->outerRadius = LIGHT_FAR_GLOW_RADIUS_NUMERATOR / flare->otz;
                DRAW_LIGHT_GLOW_FAN(flare->screenPos, flare->outerRadius, flare->otz, primitive, index, red, green, blue);
            }
        }
    }
    work->scale = (work->scale - LIGHT_YAW_STEP) & LIGHT_YAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_FlareScratch);
}

#undef TRANSLATE_LIGHT_VERTEX

void akropolisPlazaLightGlowTask(Task* task)
{
    enum {
        YELLOW_FLICKER_MASK     = 0x3F,
        YELLOW_RADIUS_NUMERATOR = 0xC000,
        RED_RADIUS_NUMERATOR    = 0x8000,
        RED_PULSE_STEP          = 6,
        RED_RISING_LEVEL_MASK   = 0x7E
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
    if (glow->otz >= GLOW_MIN_DEPTH) {
        if (__builtin_abs(glow->screenPos.vx) < LIGHT_CULL_X && __builtin_abs(glow->screenPos.vy) < LIGHT_CULL_Y) {
            if (task->spawnArg1.value < AKROPOLIS_PLAZA_LIGHT_FIRST_RED_GLOW) {
                RANDOM_LCG_ADVANCE();
                packedBrightness = ((RANDOM_LCG_DRAW() & YELLOW_FLICKER_MASK) + LIGHT_FLICKER_BASE_INTENSITY) << 16;
                yellowIntensity  = packedBrightness >> 17;
                red              = yellowIntensity;
                green            = yellowIntensity;
                blue             = (u32)packedBrightness >> 18;
                glow->halfExtent = YELLOW_RADIUS_NUMERATOR / glow->otz;
            } else {
                pulsePhase = gDisplayState.animFrame * RED_PULSE_STEP;
                if (pulsePhase & LIGHT_PULSE_HALF_PERIOD) {
                    pulseLevel = LIGHT_PULSE_LEVEL_MASK - (pulsePhase & LIGHT_PULSE_LEVEL_MASK);
                } else {
                    pulseLevel = pulsePhase & RED_RISING_LEVEL_MASK;
                }
                red                = pulseLevel;
                redAccentIntensity = (s32)(red << 16) >> 18;
                green              = redAccentIntensity;
                blue               = redAccentIntensity;
                glow->halfExtent   = RED_RADIUS_NUMERATOR / glow->otz;
            }
            DRAW_LIGHT_GLOW_FAN(glow->screenPos, glow->halfExtent, glow->otz, primitive, fanSample, red, green, blue);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#undef DRAW_LIGHT_GLOW_FAN

void akropolisPlazaSpawnAmbientLightsTask(Task* task)
{
    enum {
        LIGHTS_INITIAL = 0,
        GLOW_END       = ARRAY_SIZE(_gAkropolisPlazaAmbientLightPlacements),
    };
    GfxCoord* roomCoord;
    s32       placementIndex;

    roomCoord = task->extra.coordBody->coord;
    if (task->state == LIGHTS_INITIAL) {
        for (placementIndex = AKROPOLIS_PLAZA_LIGHT_FIRST_GLOW; placementIndex < GLOW_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_LIGHT_GLOW, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = AKROPOLIS_PLAZA_LIGHT_FIRST_FLARE; placementIndex < AKROPOLIS_PLAZA_LIGHT_FLARE_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_LIGHT_FLARE, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        for (placementIndex = AKROPOLIS_PLAZA_LIGHT_FIRST_SIREN; placementIndex < AKROPOLIS_PLAZA_LIGHT_SIREN_END; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_PLAZA_SIREN_LIGHT, roomCoord, placementIndex, &_gAkropolisPlazaAmbientLightPlacements[placementIndex]);
        }
        task->state = task->state + 1;
    }
}
