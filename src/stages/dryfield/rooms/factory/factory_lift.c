#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "factory_private.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/world_coords.h"

#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_factory.h"

#include "../../../../shared/room/room_events.h"

static void _factoryLiftInit(Task* task);
static void _factoryLiftUpdate(Task* task);
static void _factoryHatchInit(Task* task);
static void _factoryHatchUpdate(Task* task);
static void _factoryLiftExit(Task* task);
static void _factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useTurnedTemplate);
static s32  _factoryLiftTurnOut(Task* task);
static s32  _factoryLiftTurnBack(Task* task);
static s32  _factoryLiftRaise(Task* task);
static s32  _factoryLiftLower(Task* task);
static s32  _factoryLiftJamTurnOut(Task* task);
static s32  _factoryLiftJamTurnBack(Task* task);
static void _factoryLiftNotifyPanel(Task* panelTask);

/// Phases shared by the lift's independent vertical and yaw movements.
/// The stored selectors remain signed bytes; -1 is the setup-only rest value.
enum {
    FACTORY_LIFT_STEP_RESET = 0,
    FACTORY_LIFT_STEP_START_SOUND,
    FACTORY_LIFT_STEP_OUTBOUND,
    FACTORY_LIFT_STEP_SETTLE,
    FACTORY_LIFT_STEP_REST
};

/// Notifies the operator panel that a turn ended and queues the turn stop cue.
///
/// Used when a normal turn or jam rebound settles or is skipped. The live
/// `liftTask` borrows a room-owned `Task*` slot in `spawnArg2.pointer`; the slot
/// must remain live, and its contents must be NULL or a live panel task.
/// Notification is synchronous and precedes the sound requests.
///
/// Runtime stage selects daytime Dryfield sounds, or nighttime sounds otherwise.
/// Stopping retains the loop's existing release settings. `liftRoot` must have
/// its local-to-view matrix already composed: the stop cue samples that cached
/// origin before the caller rebuilds yaw. Pan [-16, 15] and depth [-128, 127]
/// are narrowed to signed bytes; scratch and GTE requirements follow
/// `worldCoordGetOriginAudioPan`.
static inline void _factoryLiftFinishTurn(const Task* liftTask, const GfxCoord* liftRoot)
{
    Task* const* panelSlot = liftTask->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelSlot);
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN_STOP), (s8)worldCoordGetOriginAudioPan(liftRoot), (s8)worldCoordGetOriginAudioDepth(liftRoot));
    } else {
        sndEvtRequestStageScriptStop(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN_STOP), (s8)worldCoordGetOriginAudioPan(liftRoot), (s8)worldCoordGetOriginAudioDepth(liftRoot));
    }
}

/// Notifies the operator panel of a finished raise or lower and queues the stop cue.
///
/// Used for both natural completion and an accepted skip. `liftTask` borrows
/// a room-owned `Task*` slot in `spawnArg2.pointer`; the slot must be live and
/// contain NULL or a live panel task. Notification is synchronous and precedes
/// the sound requests; NULL suppresses only the notification.
///
/// Requires a live session and loaded factory sound bank. Stops the vertical
/// motion script without a fade, retaining its release settings, then starts
/// the stop cue: daytime IDs in `GAME_STAGE_DRYFIELD`, nighttime IDs otherwise.
/// `liftCoord` is the model root with its local-to-view matrix already composed;
/// projection and scratch requirements follow `worldCoordGetOriginAudioPan`.
/// Pan (-16..15) and depth (-128..127, 256 world units per step) are passed as
/// signed bytes. The caller commits the final position and movement phase.
static inline void _factoryLiftFinishVertical(const Task* liftTask, const GfxCoord* liftCoord)
{
    Task* const* panelTaskSlot = liftTask->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelTaskSlot);
    // Sample the cached origin before the caller writes this frame's new local Y.
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_MOVE), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_MOVE_STOP), (s8)worldCoordGetOriginAudioPan(liftCoord), (s8)worldCoordGetOriginAudioDepth(liftCoord));
    } else {
        sndEvtRequestStageScriptStop(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_MOVE), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_MOVE_STOP), (s8)worldCoordGetOriginAudioPan(liftCoord), (s8)worldCoordGetOriginAudioDepth(liftCoord));
    }
}

/// Replaces the lift root's rotation with its current integer yaw and marks it dirty.
///
/// Preserves translation, discards pitch/roll/scale, and reads yaw after the
/// identity stores. Work holds a 16.16 angle; the SDK receives its signed high half.
/// Both objects are borrowed; the caller's motion state and fractional yaw remain intact.
static inline void _factoryLiftRebuildYaw(const FactoryLiftWork* work, GfxCoord* coord)
{
    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(work->yaw.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Lift-model lifecycle handlers, indexed by `Task::state`.
///
/// Setup owns the motion work and spawns the hatch; run follows lift-position
/// requests and updates the model and collision; teardown releases the task.
/// Dispatch copies all three callbacks and requires a `FACTORY_TASK_STATE_` selector.
static const TaskFuncTable3 _gFactoryLiftStates = {
    { [FACTORY_TASK_STATE_SETUP]    = _factoryLiftInit,
      [FACTORY_TASK_STATE_RUN]      = _factoryLiftUpdate,
      [FACTORY_TASK_STATE_TEARDOWN] = taskKill },
};

/// Hatch-model lifecycle handlers, indexed by `Task::state`.
///
/// Setup parents the hatch to the lift and owns its swing work; run dispatches
/// the work's separate movement state; teardown releases the task and work.
/// Dispatch copies all three callbacks and requires a `FACTORY_TASK_STATE_` selector.
static const TaskFuncTable3 _gFactoryHatchTaskStates = {
    { [FACTORY_TASK_STATE_SETUP]    = _factoryHatchInit,
      [FACTORY_TASK_STATE_RUN]      = _factoryHatchUpdate,
      [FACTORY_TASK_STATE_TEARDOWN] = taskKill },
};

/// Hatch watch/open/close handlers, indexed by `FactoryHatchWork::state`.
///
/// WATCH arms swings on hatch-flag edges and returns zero. A movement's
/// nonzero result returns the dispatcher to WATCH. The complete table is
/// copied each frame; the selector must be a `FACTORY_HATCH_STATE_` value.
static const FactoryHatchStateFuncTable _gFactoryHatchStates = {
    { [FACTORY_HATCH_STATE_WATCH] = _factoryHatchWatch,
      [FACTORY_HATCH_STATE_OPEN]  = _factoryHatchOpen,
      [FACTORY_HATCH_STATE_CLOSE] = _factoryHatchClose },
};

/// The lift's collision templates, generated from each package at the offset
/// it holds them: the same geometry by day and by night.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define FACTORY_BARRIER_TEMPLATE_NORMALS_INC     "assets/dryfield_factory_collision_096A8_normals.inc"
#define FACTORY_BARRIER_TEMPLATE_VERTS_INC       "assets/dryfield_factory_collision_096A8_verts.inc"
#define FACTORY_BARRIER_TEMPLATE_FACES_INC       "assets/dryfield_factory_collision_096A8_faces.inc"
#define FACTORY_BARRIER_TEMPLATE_CELLS_INC       "assets/dryfield_factory_collision_096A8_cells.inc"
#define FACTORY_BARRIER_TEMPLATE_TABLE_INC       "assets/dryfield_factory_collision_096A8_table.inc"
#define FACTORY_LIFT_TEMPLATE_NORMALS_INC        "assets/dryfield_factory_collision_09778_normals.inc"
#define FACTORY_LIFT_TEMPLATE_VERTS_INC          "assets/dryfield_factory_collision_09778_verts.inc"
#define FACTORY_LIFT_TEMPLATE_FACES_INC          "assets/dryfield_factory_collision_09778_faces.inc"
#define FACTORY_LIFT_TEMPLATE_CELLS_INC          "assets/dryfield_factory_collision_09778_cells.inc"
#define FACTORY_LIFT_TEMPLATE_TABLE_INC          "assets/dryfield_factory_collision_09778_table.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_NORMALS_INC "assets/dryfield_factory_collision_09844_normals.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_VERTS_INC   "assets/dryfield_factory_collision_09844_verts.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_FACES_INC   "assets/dryfield_factory_collision_09844_faces.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_CELLS_INC   "assets/dryfield_factory_collision_09844_cells.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_TABLE_INC   "assets/dryfield_factory_collision_09844_table.inc"
#else
#define FACTORY_BARRIER_TEMPLATE_NORMALS_INC     "assets/dryfield_night_factory_collision_09660_normals.inc"
#define FACTORY_BARRIER_TEMPLATE_VERTS_INC       "assets/dryfield_night_factory_collision_09660_verts.inc"
#define FACTORY_BARRIER_TEMPLATE_FACES_INC       "assets/dryfield_night_factory_collision_09660_faces.inc"
#define FACTORY_BARRIER_TEMPLATE_CELLS_INC       "assets/dryfield_night_factory_collision_09660_cells.inc"
#define FACTORY_BARRIER_TEMPLATE_TABLE_INC       "assets/dryfield_night_factory_collision_09660_table.inc"
#define FACTORY_LIFT_TEMPLATE_NORMALS_INC        "assets/dryfield_night_factory_collision_09730_normals.inc"
#define FACTORY_LIFT_TEMPLATE_VERTS_INC          "assets/dryfield_night_factory_collision_09730_verts.inc"
#define FACTORY_LIFT_TEMPLATE_FACES_INC          "assets/dryfield_night_factory_collision_09730_faces.inc"
#define FACTORY_LIFT_TEMPLATE_CELLS_INC          "assets/dryfield_night_factory_collision_09730_cells.inc"
#define FACTORY_LIFT_TEMPLATE_TABLE_INC          "assets/dryfield_night_factory_collision_09730_table.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_NORMALS_INC "assets/dryfield_night_factory_collision_097FC_normals.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_VERTS_INC   "assets/dryfield_night_factory_collision_097FC_verts.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_FACES_INC   "assets/dryfield_night_factory_collision_097FC_faces.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_CELLS_INC   "assets/dryfield_night_factory_collision_097FC_cells.inc"
#define FACTORY_LIFT_TURNED_TEMPLATE_TABLE_INC   "assets/dryfield_night_factory_collision_097FC_table.inc"
#endif

static TmdBone _gDryfieldFactoryModel06604Skeleton[1] = {
#include "assets/dryfield_factory_model_06604_skeleton.inc"
};

static u32 _gDryfieldFactoryModel06604PartVerts[1] = {
#include "assets/dryfield_factory_model_06604_partVerts.inc"
};

static SVECTOR _gDryfieldFactoryModel06604Verts[306] = {
#include "assets/dryfield_factory_model_06604_verts.inc"
};

static SVECTOR _gDryfieldFactoryModel06604Normals[353] = {
#include "assets/dryfield_factory_model_06604_normals.inc"
};

static u32 _gDryfieldFactoryModel06604Stream[2811] = {
#include "assets/dryfield_factory_model_06604_stream.inc"
};

static TmdSource _gDryfieldFactoryModel06604 = {
    0,
    19284,
    0,
    1,
    _gDryfieldFactoryModel06604PartVerts,
    _gDryfieldFactoryModel06604Verts,
    _gDryfieldFactoryModel06604Normals,
    _gDryfieldFactoryModel06604Skeleton,
    _gDryfieldFactoryModel06604Stream,
};

static TmdBone _gDryfieldFactoryModel093E4Skeleton[1] = {
#include "assets/dryfield_factory_model_093E4_skeleton.inc"
};

static u32 _gDryfieldFactoryModel093E4PartVerts[1] = {
#include "assets/dryfield_factory_model_093E4_partVerts.inc"
};

static SVECTOR _gDryfieldFactoryModel093E4Verts[25] = {
#include "assets/dryfield_factory_model_093E4_verts.inc"
};

static SVECTOR _gDryfieldFactoryModel093E4Normals[28] = {
#include "assets/dryfield_factory_model_093E4_normals.inc"
};

static u32 _gDryfieldFactoryModel093E4Stream[139] = {
#include "assets/dryfield_factory_model_093E4_stream.inc"
};

static TmdSource _gDryfieldFactoryModel093E4 = {
    0,
    856,
    0,
    1,
    _gDryfieldFactoryModel093E4PartVerts,
    _gDryfieldFactoryModel093E4Verts,
    _gDryfieldFactoryModel093E4Normals,
    _gDryfieldFactoryModel093E4Skeleton,
    _gDryfieldFactoryModel093E4Stream,
};

static SVECTOR _gFactoryBarrierTemplateNormals[2] = {
#include FACTORY_BARRIER_TEMPLATE_NORMALS_INC
};

static SVECTOR _gFactoryBarrierTemplateVerts[8] = {
#include FACTORY_BARRIER_TEMPLATE_VERTS_INC
};

static WorldCollisionGridFace _gFactoryBarrierTemplateFaces[2] = {
#include FACTORY_BARRIER_TEMPLATE_FACES_INC
};

static s16 _gFactoryBarrierTemplateCells[4] = {
#include FACTORY_BARRIER_TEMPLATE_CELLS_INC
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gFactoryBarrierTemplateCells` (4 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gFactoryBarrierTemplateCells[(halfwordOffset)])

static s16* _gFactoryBarrierTemplateTable[1] = {
#include FACTORY_BARRIER_TEMPLATE_TABLE_INC
};
#undef DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gFactoryBarrierTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryBarrierTemplateNormals,
    .vertices    = _gFactoryBarrierTemplateVerts,
    .faces       = _gFactoryBarrierTemplateFaces,
    .cellFaceIds = _gFactoryBarrierTemplateTable,
    .xBias       = -4464,
    .zBias       = -3949,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryBarrierTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryBarrierTemplateFaces),
};

static SVECTOR _gFactoryLiftTemplateNormals[4] = {
#include FACTORY_LIFT_TEMPLATE_NORMALS_INC
};

static SVECTOR _gFactoryLiftTemplateVerts[8] = {
#include FACTORY_LIFT_TEMPLATE_VERTS_INC
};

static WorldCollisionGridFace _gFactoryLiftTemplateFaces[4] = {
#include FACTORY_LIFT_TEMPLATE_FACES_INC
};

static s16 _gFactoryLiftTemplateCells[10] = {
#include FACTORY_LIFT_TEMPLATE_CELLS_INC
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gFactoryLiftTemplateCells` (10 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gFactoryLiftTemplateCells[(halfwordOffset)])

static s16* _gFactoryLiftTemplateTable[2] = {
#include FACTORY_LIFT_TEMPLATE_TABLE_INC
};
#undef DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gFactoryLiftTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryLiftTemplateNormals,
    .vertices    = _gFactoryLiftTemplateVerts,
    .faces       = _gFactoryLiftTemplateFaces,
    .cellFaceIds = _gFactoryLiftTemplateTable,
    .xBias       = 750,
    .zBias       = 2191,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryLiftTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryLiftTemplateFaces),
};

static SVECTOR _gFactoryLiftTurnedTemplateNormals[4] = {
#include FACTORY_LIFT_TURNED_TEMPLATE_NORMALS_INC
};

static SVECTOR _gFactoryLiftTurnedTemplateVerts[8] = {
#include FACTORY_LIFT_TURNED_TEMPLATE_VERTS_INC
};

static WorldCollisionGridFace _gFactoryLiftTurnedTemplateFaces[4] = {
#include FACTORY_LIFT_TURNED_TEMPLATE_FACES_INC
};

static s16 _gFactoryLiftTurnedTemplateCells[8] = {
#include FACTORY_LIFT_TURNED_TEMPLATE_CELLS_INC
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gFactoryLiftTurnedTemplateCells` (8 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gFactoryLiftTurnedTemplateCells[(halfwordOffset)])

static s16* _gFactoryLiftTurnedTemplateTable[2] = {
#include FACTORY_LIFT_TURNED_TEMPLATE_TABLE_INC
};
#undef DRYFIELD_FACTORY_TEMPLATE_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gFactoryLiftTurnedTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryLiftTurnedTemplateNormals,
    .vertices    = _gFactoryLiftTurnedTemplateVerts,
    .faces       = _gFactoryLiftTurnedTemplateFaces,
    .cellFaceIds = _gFactoryLiftTurnedTemplateTable,
    .xBias       = 750,
    .zBias       = 1950,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryLiftTurnedTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryLiftTurnedTemplateFaces),
};

/// This package's own task table; room setup picks it or the other package's by stage.
TaskDesc FACTORY_OWN_SPAWN_TABLE[8] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, factoryBarrierTransitionScene, { .value = 0 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, factoryLiftRun, { .model = &_gDryfieldFactoryModel06604 } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, factoryHatchRun, { .model = &_gDryfieldFactoryModel093E4 } },
};

/// Initializes the lift model, its motion work, collision footprint and hatch.
///
/// Requires a spawned TMD root and live factory room/grid resources. Owns a
/// zeroed `FactoryLiftWork` until its exit callback; allocation failure kills
/// the task. Seeds height/yaw from the saved lift-position bits and starts both
/// motion selectors at -1 (rest). Placement is in whole room-coordinate units.
/// Setup applies the raised height but no quarter-turn: the full 16.16 yaw word
/// reduces to zero in `RotMatrixY`, and collision uses the unturned template.
/// The first frame update applies the seeded yaw and its requested footprint.
/// The spawned hatch borrows this lift task and its lighting matrices.
static void _factoryLiftInit(Task* task)
{
    enum {
        FACTORY_LIFT_SETUP_REST = -1,
    };
    FactoryLiftWork* work;
    GfxCoord*        coord;
    TmdObject*       liftModel;

    liftModel = task->extra.tmd;
    coord     = liftModel->coords;
    work      = memCalloc(sizeof(FactoryLiftWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = work;
    work->position    = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    work->yawStep     = FACTORY_LIFT_SETUP_REST;
    work->yStep       = FACTORY_LIFT_SETUP_REST;
    liftModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (work->position & FACTORY_LIFT_POSITION_TURNED) {
        work->yaw.word = FACTORY_LIFT_YAW_TURNED_Q16;
        // The SDK masks this full 16.16 word to zero; retain the setup frame.
        RotMatrixY(FACTORY_LIFT_YAW_TURNED_Q16, &coord->coord);
    }
    if (work->position & FACTORY_LIFT_POSITION_RAISED) {
        work->y.word = FACTORY_LIFT_Y_RAISED_Q16;
    } else {
        work->y.word = 0;
    }
    coord->coord.t[0] = FACTORY_LIFT_ROOM_X;
    coord->coord.t[1] = work->y.halves.integer;
    coord->coord.t[2] = FACTORY_LIFT_ROOM_Z;
    _factoryLiftBindLighting(task);
    _factoryLiftSyncCollision(task, 1, 0);
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        taskSpawnFromTable(gFactoryDaySpawnTable, FACTORY_SPAWN_HATCH, 0, task);
    } else {
        taskSpawnFromTable(gDryfieldNightFactoryTaskDescs, FACTORY_SPAWN_HATCH, 0, task);
    }
    task->exitCallback  = _factoryLiftExit;
    task->killCountdown = 0;
    task->state++;
}

/// Rebuilds the lift's reserved collision geometry in the active room grid.
///
/// Requires a live TMD root whose local matrix maps the lift into room space.
/// Daytime Dryfield selects the day grid; every other stage selects night.
/// `useTurnedTemplate` nonzero selects the turned footprint in model space.
/// Both templates supply four normals/faces and eight vertices. The room
/// reserves normals/faces 2..5 and vertices 8..15 for their transformed copies.
/// `remapFaces` nonzero also rebases all four face indices into those pools;
/// later updates retain that topology, shared by both templates. Cell lists
/// and other geometry stay intact. Normal pad words are preserved; the SDK
/// vertex transform overwrites each vertex's pad with the sign extension of Z.
/// No storage is owned here.
static void _factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useTurnedTemplate)
{
    enum { FACTORY_LIFT_COLLISION_FIRST_NORMAL = 2,
           FACTORY_LIFT_COLLISION_FIRST_FACE   = 2,
           FACTORY_LIFT_COLLISION_FACE_COUNT   = 4,
           FACTORY_LIFT_COLLISION_FIRST_VERTEX = 8,
           FACTORY_LIFT_COLLISION_VERTEX_COUNT = 8 };
    long                          gteFlags;
    GfxCoord*                     liftCoord;
    MATRIX*                       modelToRoom;
    WorldCollisionGrid*           roomGrid;
    const WorldCollisionGrid*     templateGrid;
    SVECTOR*                      sourceVector;
    SVECTOR*                      roomVector;
    const WorldCollisionGridFace* sourceFace;
    WorldCollisionGridFace*       destFace;
    const u16*                    sourceIndices;
    u16*                          destIndices;
    s32                           recordIndex;
    s32                           cornerIndex;

    liftCoord = task->extra.tmd->coords;
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        roomGrid = &gDryfieldFactoryCollisionGrid;
    } else {
        roomGrid = &gDryfieldNightFactoryCollisionGrid;
    }
    if (useTurnedTemplate != 0) {
        templateGrid = &gFactoryLiftTurnedTemplate;
    } else {
        templateGrid = &gFactoryLiftTemplate;
    }

    // Normals rotate only; vertices also receive the lift translation.
    modelToRoom  = &liftCoord->coord;
    sourceVector = templateGrid->normals;
    roomVector   = roomGrid->normals + FACTORY_LIFT_COLLISION_FIRST_NORMAL;
    for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_FACE_COUNT; recordIndex++) {
        gte_SetRotMatrix(modelToRoom);
        gte_ldv0(sourceVector);
        sourceVector++;
        gte_rtv0();
        gte_stsv(roomVector);
        roomVector++;
    }

    gte_SetRotMatrix(modelToRoom);
    gte_SetTransMatrix(modelToRoom);
    sourceVector = templateGrid->vertices;
    roomVector   = roomGrid->vertices + FACTORY_LIFT_COLLISION_FIRST_VERTEX;
    for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_VERTEX_COUNT; recordIndex++) {
        RotTransSV(sourceVector++, roomVector++, &gteFlags);
    }

    // Face topology is installed once, then shared by both moving footprints.
    if (remapFaces != 0) {
        sourceFace = templateGrid->faces;
        destFace   = roomGrid->faces + FACTORY_LIFT_COLLISION_FIRST_FACE;
        for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_FACE_COUNT; recordIndex++) {
            cornerIndex   = 0;
            destIndices   = destFace->vertexIndices;
            sourceIndices = sourceFace->vertexIndices;
            do {
                *destIndices++ = *sourceIndices++ + FACTORY_LIFT_COLLISION_FIRST_VERTEX;
            } while (++cornerIndex < ARRAY_SIZE(destFace->vertexIndices));
            destFace->normalIndex  = sourceFace->normalIndex + FACTORY_LIFT_COLLISION_FIRST_NORMAL;
            destFace->surfaceClass = sourceFace->surfaceClass;
            destFace++;
            sourceFace++;
        }
    }
}

/// Advances the raised lift's outward quarter-turn and settles at yaw 0x400.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 angle and velocity, with 4096 angle units per turn; overshoots the
/// endpoint, reverses and clamps on return. START after
/// `FACTORY_LIFT_SKIP_FRAMES` snaps to the endpoint. Rebuilds local yaw each
/// frame. Returns 1 when already at rest or skipped, and 0 on the natural
/// completion frame.
static s32 _factoryLiftTurnOut(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yawStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yawVelocity += 0x18000;
            if (work->yawVelocity > 0x40000) {
                work->yawVelocity = 0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word > FACTORY_LIFT_YAW_TURNED_Q16) {
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += -0x8000;
            if (work->yawVelocity < -0x20000) {
                work->yawVelocity = -0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word <= FACTORY_LIFT_YAW_TURNED_Q16) {
                _factoryLiftFinishTurn(task, coord);
                work->yaw.word = FACTORY_LIFT_YAW_TURNED_Q16;
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishTurn(task, coord);
        movementComplete = 1;
        work->yaw.word   = FACTORY_LIFT_YAW_TURNED_Q16;
        work->yawStep    = FACTORY_LIFT_STEP_REST;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}

/// Advances the raised lift's return quarter-turn and settles at home yaw zero.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 angle and velocity, with 4096 angle units per turn; overshoots zero,
/// reverses and clamps on return. START after `FACTORY_LIFT_SKIP_FRAMES`
/// snaps to zero. Rebuilds local yaw each frame. Returns 1 when already at
/// rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftTurnBack(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yawStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yawVelocity += -0x18000;
            if (work->yawVelocity < -0x40000) {
                work->yawVelocity = -0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word < 0) {
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += 0x8000;
            if (work->yawVelocity > 0x20000) {
                work->yawVelocity = 0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word >= 0) {
                _factoryLiftFinishTurn(task, coord);
                work->yaw.word = 0;
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishTurn(task, coord);
        movementComplete = 1;
        work->yaw.word   = 0;
        work->yawStep    = FACTORY_LIFT_STEP_REST;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}

/// Advances the lift's upward movement toward Y = -570 world units.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 position and velocity; natural completion keeps the computed position,
/// while START after `FACTORY_LIFT_SKIP_FRAMES` assigns the exact target of -570.
/// Updates local Y and invalidates composition each frame. Returns 1 when
/// already at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftRaise(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yVelocity = 0;
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_MOVE), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_MOVE), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yVelocity += -0xC000;
            if (work->yVelocity < -0x30000) {
                work->yVelocity = -0x30000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word < FACTORY_LIFT_Y_RAISED_Q16) {
                work->yStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yVelocity += 0xC000;
            if (work->yVelocity > 0xC000) {
                work->yVelocity = 0xC000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word >= FACTORY_LIFT_Y_RAISED_Q16) {
                // Keep the computed position; only the skip path assigns the exact target.
                _factoryLiftFinishVertical(task, coord);
                work->yStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishVertical(task, coord);
        movementComplete = 1;
        work->y.word     = FACTORY_LIFT_Y_RAISED_Q16;
        work->yStep      = FACTORY_LIFT_STEP_REST;
    }
    coord->coord.t[1]   = work->y.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return movementComplete;
}

/// Advances the lift's downward movement toward Y = 0 world units.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 position and velocity; natural completion keeps the computed position,
/// while START after `FACTORY_LIFT_SKIP_FRAMES` assigns the exact target of zero.
/// Updates local Y and invalidates composition each frame. Returns 1 when
/// already at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftLower(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yVelocity = 0;
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_MOVE), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_MOVE), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yVelocity += 0xC000;
            if (work->yVelocity > 0x30000) {
                work->yVelocity = 0x30000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word > 0) {
                work->yStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yVelocity += -0xC000;
            if (work->yVelocity < -0xC000) {
                work->yVelocity = -0xC000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word <= 0) {
                // Keep the computed position; only the skip path assigns the exact target.
                _factoryLiftFinishVertical(task, coord);
                work->yStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishVertical(task, coord);
        movementComplete = 1;
        work->y.word     = 0;
        work->yStep      = FACTORY_LIFT_STEP_REST;
    }
    coord->coord.t[1]   = work->y.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return movementComplete;
}

/// Advances a blocked turn of the lowered lift, then returns it to home yaw.
///
/// Requires the live lift work, model and room-owned panel-task slot. The
/// outward swing passes 0x80 angle units, triggers the jam sound and jolt,
/// then reverses to zero. Completion or START after `FACTORY_LIFT_SKIP_FRAMES`
/// clears `FACTORY_LIFT_POSITION_TURNED` in both the saved flag and work copy.
/// Rebuilds local yaw each frame (4096 units per turn). Returns 1 when already
/// at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftJamTurnOut(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yawStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yawVelocity += 0x18000;
            if (work->yawVelocity > 0x40000) {
                work->yawVelocity = 0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word > FACTORY_LIFT_JAM_YAW_OUT) {
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_JAM), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gDryfieldFactoryLiftJoltCommands, gDryfieldFactoryLiftJoltSegments);
                } else {
                    sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_JAM), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gDryfieldNightFactoryLiftJoltCommands, gDryfieldNightFactoryLiftJoltSegments);
                }
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += -0xC000;
            if (work->yawVelocity < -0x20000) {
                work->yawVelocity = -0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word <= 0) {
                // Undo the blocked request in both copies so the next update stays at rest.
                work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
                work->yaw.word = 0;
                _factoryLiftFinishTurn(task, coord);
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        // Undo the blocked request in both copies so the next update stays at rest.
        work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
        gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
        work->yaw.word = 0;
        _factoryLiftFinishTurn(task, coord);
        work->yawStep    = FACTORY_LIFT_STEP_REST;
        movementComplete = 1;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}

/// Advances a blocked return turn of the lowered lift, then restores turned yaw.
///
/// Requires the live lift work, model and room-owned panel-task slot. The
/// return swing passes 0x380 angle units, triggers the jam sound and jolt,
/// then reverses to 0x400. Completion or START after `FACTORY_LIFT_SKIP_FRAMES`
/// sets `FACTORY_LIFT_POSITION_TURNED` in both the saved flag and work copy.
/// Rebuilds local yaw each frame (4096 units per turn). Returns 1 when already
/// at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftJamTurnBack(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yawStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_TURN), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yawVelocity += -0x18000;
            if (work->yawVelocity < -0x40000) {
                work->yawVelocity = -0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word < FACTORY_LIFT_JAM_YAW_BACK) {
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LIFT_JAM), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gDryfieldFactoryLiftJoltCommands, gDryfieldFactoryLiftJoltSegments);
                } else {
                    sndEvtRequestStageScriptStart(FACTORY_NIGHT_SOUND(FACTORY_SOUND_LIFT_JAM), (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gDryfieldNightFactoryLiftJoltCommands, gDryfieldNightFactoryLiftJoltSegments);
                }
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += 0xC000;
            if (work->yawVelocity > 0x20000) {
                work->yawVelocity = 0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word >= FACTORY_LIFT_YAW_TURNED_Q16) {
                // Undo the blocked request in both copies so the next update stays at rest.
                work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_TURNED;
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
                work->yaw.word = FACTORY_LIFT_YAW_TURNED_Q16;
                _factoryLiftFinishTurn(task, coord);
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        // Undo the blocked request in both copies so the next update stays at rest.
        work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_TURNED;
        gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
        work->yaw.word = FACTORY_LIFT_YAW_TURNED_Q16;
        _factoryLiftFinishTurn(task, coord);
        work->yawStep    = FACTORY_LIFT_STEP_REST;
        movementComplete = 1;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}

/// Plays the hatch swing's stage-bank cue at the model root's audio position.
///
/// Borrows a live coordinate. Both ids are stage-relative scripts; runtime
/// Dryfield selects the day id, and other stages select the night id. Audio pan
/// and depth narrow to signed bytes. Both swing handlers use it.
static inline void _factoryHatchPlaySwingSound(const GfxCoord* coord, s32 daySoundId, s32 nightSoundId)
{
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStart(daySoundId, (s8)worldCoordGetOriginAudioPan(coord),
                                      (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        sndEvtRequestStageScriptStart(nightSoundId, (s8)worldCoordGetOriginAudioPan(coord),
                                      (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Sounds and swings the hatch open, overshooting before settling at -768 angle units.
///
/// Runs `FACTORY_HATCH_STATE_OPEN` on a live hatch task with `FactoryHatchWork`
/// and a model root. Angle and angular velocity use 16 fractional bits, with
/// 4096 whole angle units per turn. Rebuilds the root's X rotation while keeping
/// its translation. Returns 0 through the frame that clamps the open angle and
/// 1 on subsequent calls, allowing the dispatcher to return to watching.
static s32 _factoryHatchOpen(Task* task)
{
    enum {
        FACTORY_HATCH_OPEN_BEGIN  = 0,
        FACTORY_HATCH_OPEN_SWING  = 1,
        FACTORY_HATCH_OPEN_SETTLE = 2,
    };
    FactoryHatchWork* work     = task->work;
    GfxCoord*         coord    = task->extra.tmd->coords;
    s32               complete = 0;

    switch (work->step) {
        case FACTORY_HATCH_OPEN_BEGIN:
            work->angularVelocity = 0;
            _factoryHatchPlaySwingSound(coord, FACTORY_DAY_SOUND(FACTORY_SOUND_HATCH_OPEN), FACTORY_NIGHT_SOUND(FACTORY_SOUND_HATCH_OPEN));
            work->step++;
            break;
        case FACTORY_HATCH_OPEN_SWING:
            work->angularVelocity += FACTORY_HATCH_OPEN_ACCELERATION_Q16;
            if (work->angularVelocity < FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word < FACTORY_HATCH_OPEN_ANGLE_Q16) {
                work->step++;
            }
            break;
        case FACTORY_HATCH_OPEN_SETTLE:
            work->angularVelocity += FACTORY_HATCH_OPEN_SETTLE_ACCELERATION_Q16;
            if (work->angularVelocity > FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word >= FACTORY_HATCH_OPEN_ANGLE_Q16) {
                work->angle.word = FACTORY_HATCH_OPEN_ANGLE_Q16;
                work->step++;
            }
            break;
        default:
            complete = 1;
            break;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrixX(work->angle.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return complete;
}

/// Accelerates the hatch toward closed and sounds its crossing of the shut angle.
///
/// Runs `FACTORY_HATCH_STATE_CLOSE` on a live hatch task with `FactoryHatchWork`
/// and a model root. Angle and angular velocity use 16 fractional bits, with
/// 4096 whole angle units per turn. Rebuilds only the root rotation about X;
/// translation remains intact. The angle may overshoot zero and is retained.
/// Returns 0 through the crossing frame and
/// 1 on subsequent calls, allowing the dispatcher to return to watching.
static s32 _factoryHatchClose(Task* task)
{
    enum {
        FACTORY_HATCH_CLOSE_BEGIN = 0,
        FACTORY_HATCH_CLOSE_SWING = 1,
    };
    FactoryHatchWork* work     = task->work;
    GfxCoord*         coord    = task->extra.tmd->coords;
    s32               complete = 0;

    switch (work->step) {
        case FACTORY_HATCH_CLOSE_BEGIN:
            work->angularVelocity = 0;
            work->step++;
            break;
        case FACTORY_HATCH_CLOSE_SWING:
            work->angularVelocity += FACTORY_HATCH_CLOSE_ACCELERATION_Q16;
            if (work->angularVelocity > FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word > 0) {
                _factoryHatchPlaySwingSound(coord, FACTORY_DAY_SOUND(FACTORY_SOUND_HATCH_CLOSE), FACTORY_NIGHT_SOUND(FACTORY_SOUND_HATCH_CLOSE));
                work->step++;
            }
            break;
        default:
            complete = 1;
            break;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrixX(work->angle.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return complete;
}

void factoryPowerScene(Task* task)
{
    enum {
        FACTORY_POWER_SCENE_START = 0,
        FACTORY_POWER_SCENE_HIDE_UNPOWERED,
        FACTORY_POWER_SCENE_WAIT_CAP,
        FACTORY_POWER_SCENE_APPLY_CHOICE,
        FACTORY_POWER_SCENE_WAIT_FOLLOWUP,
    };
    switch (task->state) {
        case FACTORY_POWER_SCENE_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case FACTORY_POWER_SCENE_HIDE_UNPOWERED:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) <= 0) {
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    dryfieldFactorySetView11SpriteVisible(false);
                } else {
                    dryfieldNightFactorySetView11SpriteVisible(false);
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
            }
            task->state++;
            /* fallthrough */
        case FACTORY_POWER_SCENE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            /* fallthrough */
        case FACTORY_POWER_SCENE_APPLY_CHOICE:
            // Publish power before starting the accepted command's follow-up slot.
            if (capGetVariantKey() == FACTORY_POWER_CAP_ACCEPTED) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_POWER_ON, 1);
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 1);
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    dryfieldFactorySetView11SpriteVisible(true);
                    dryfieldFactorySetView9SpriteVisible(1);
                } else {
                    dryfieldNightFactorySetView11SpriteVisible(true);
                    dryfieldNightFactorySetView9SpriteVisible(1);
                }
                capStartSequenceSlot(task->spawnArg1.value, CAP_PLAYBACK_DISPLAY_TRANSITION, FACTORY_POWER_FOLLOWUP_VARIANT_KEY);
            }
            task->state++;
            return;
        case FACTORY_POWER_SCENE_WAIT_FOLLOWUP:
            if (capIsBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            taskKill(task);
            break;
    }
}

void factoryBarrierTransitionScene(Task* task)
{
    enum {
        FACTORY_BARRIER_TRANSITION_SCENE_START = 0,
        FACTORY_BARRIER_TRANSITION_SCENE_BEFORE_CHOICE,
        FACTORY_BARRIER_TRANSITION_SCENE_CHECK_CHOICE,
        FACTORY_BARRIER_TRANSITION_SCENE_FADE_OUT,
        FACTORY_BARRIER_TRANSITION_SCENE_CLEAR_BARRIER,
        FACTORY_BARRIER_TRANSITION_SCENE_CHANGE_ROOM,
        FACTORY_BARRIER_TRANSITION_SCENE_FADE_IN,
        FACTORY_BARRIER_TRANSITION_SCENE_RESTORE  = -1,
        FACTORY_BARRIER_TRANSITION_FULL_INTENSITY = 255,
    };
    u8 fadeIntensity;
    u8 stageId;

    switch (task->state) {
        case FACTORY_BARRIER_TRANSITION_SCENE_START:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                taskKill(task);
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CHECK_CHOICE:
            if (capGetVariantKey() == FACTORY_BARRIER_TRANSITION_CAP_ACCEPTED) {
                task->killCountdown = 0;
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_BARRIER_TRANSITION_ACCEPTED), 0, 0);
                }
                task->state = task->state + 1;
                return;
            }
            task->state = FACTORY_BARRIER_TRANSITION_SCENE_RESTORE;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_FADE_OUT:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= FACTORY_BARRIER_TRANSITION_FADE_FRAMES) {
                task->state = task->state + 1;
            }
            fadeIntensity = (task->killCountdown * FACTORY_BARRIER_TRANSITION_FULL_INTENSITY) / FACTORY_BARRIER_TRANSITION_FADE_FRAMES;
            fadeDrawOverlay(fadeIntensity, fadeIntensity, fadeIntensity, GPU_BLEND_SUBTRACT);
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CLEAR_BARRIER:
            // Cover the room-variant switch while its barrier geometry is rebuilt.
            gGameSession->viewDirty = 1;
            gameFlagSetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED, 1);
            stageId = gGameSession->location.stage;
            if (stageId == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_WHITEOUT), 0, 0);
            }
            fadeDrawOverlay(FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CHANGE_ROOM:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = FACTORY_BARRIER_TRANSITION_ROOM;
            gGameSession->location.roomState                            = FACTORY_BARRIER_TRANSITION_ROOM;
            gGameSession->roomObjsDirty                                 = 1;
            fadeDrawOverlay(FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_BEFORE_CHOICE:
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_FADE_IN:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown <= 0) {
                task->state = task->state + 1;
            }
            fadeIntensity = (task->killCountdown * FACTORY_BARRIER_TRANSITION_FULL_INTENSITY) / FACTORY_BARRIER_TRANSITION_FADE_FRAMES;
            fadeDrawOverlay(fadeIntensity, fadeIntensity, fadeIntensity, GPU_BLEND_SUBTRACT);
            return;
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
    }
}

void factoryBarrierCollision(Task* task)
{
    enum {
        FACTORY_BARRIER_STATE_RESTORE     = 0,
        FACTORY_BARRIER_STATE_WAIT        = 1,
        FACTORY_BARRIER_FACE_COUNT        = 2,
        FACTORY_BARRIER_VERTICES_PER_FACE = 4,
        FACTORY_BARRIER_VERTEX_COUNT      = FACTORY_BARRIER_FACE_COUNT * FACTORY_BARRIER_VERTICES_PER_FACE,
    };
    WorldCollisionGrid* templateGrid = &gFactoryBarrierTemplate;
    WorldCollisionGrid* roomGrid;
    s32                 recordIndex;

    /// Moves the eight reserved vertices to the cleared barrier position.
    ///
    /// Captures roomGrid and recordIndex; adds the fixed X offset with
    /// halfword truncation. Requires eight writable vertices and a braced call
    /// site. Takes no arguments and is undefined before leaving this function.
#define FACTORY_BARRIER_SHIFT_VERTICES()                                               \
    for (recordIndex = 0; recordIndex < FACTORY_BARRIER_VERTEX_COUNT; recordIndex++) { \
        roomGrid->vertices[recordIndex].vx += FACTORY_BARRIER_CLEAR_X_OFFSET;          \
    }

    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        roomGrid = &gDryfieldFactoryCollisionGrid;
    } else {
        roomGrid = &gDryfieldNightFactoryCollisionGrid;
    }
    switch (task->state) {
        case FACTORY_BARRIER_STATE_RESTORE:
            // Restore only the reserved barrier geometry, preserving SVECTOR pad words.
            for (recordIndex = 0; recordIndex < FACTORY_BARRIER_FACE_COUNT; recordIndex++) {
                roomGrid->normals[recordIndex].vx                                          = templateGrid->normals[recordIndex].vx;
                roomGrid->normals[recordIndex].vy                                          = templateGrid->normals[recordIndex].vy;
                roomGrid->normals[recordIndex].vz                                          = templateGrid->normals[recordIndex].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vz;
                roomGrid->faces[recordIndex]                                               = templateGrid->faces[recordIndex];
            }
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                FACTORY_BARRIER_SHIFT_VERTICES();
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case FACTORY_BARRIER_STATE_WAIT:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) > 0) {
                FACTORY_BARRIER_SHIFT_VERTICES();
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
#undef FACTORY_BARRIER_SHIFT_VERTICES
}

/// Advances the lift's independent height and yaw movements and room collision.
///
/// Requires initialized lift work/model and the room-owned panel-task slot in
/// `spawnArg2.pointer`. Changed position bits restart their motion selectors
/// and frame counter. Lowered turns jam and roll back their request. Collision
/// selects its template from the request read before those handlers, so the
/// completion/skip frame retains the canceled footprint with restored yaw;
/// the next update selects the restored footprint. Composes the root and samples
/// room lighting after both movements and collision have been applied.
static void _factoryLiftUpdate(Task* task)
{
    GfxCoord*        coord;
    FactoryLiftWork* work;
    TmdObject*       liftModel;
    s32              requestedPosition;
    s32              previousPosition;

    coord             = task->extra.tmd->coords;
    work              = task->work;
    liftModel         = task->extra.tmd;
    requestedPosition = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    previousPosition  = work->position;
    if (requestedPosition != previousPosition) {
        if ((requestedPosition ^ previousPosition) & FACTORY_LIFT_POSITION_TURNED) {
            work->yawStep = FACTORY_LIFT_STEP_RESET;
        }
        if ((requestedPosition ^ work->position) & FACTORY_LIFT_POSITION_RAISED) {
            work->yStep = FACTORY_LIFT_STEP_RESET;
        }
        work->position   = requestedPosition;
        work->moveFrames = 0;
    }
    if (requestedPosition & FACTORY_LIFT_POSITION_RAISED) {
        _factoryLiftRaise(task);
        if (requestedPosition & FACTORY_LIFT_POSITION_TURNED) {
            _factoryLiftTurnOut(task);
        } else {
            _factoryLiftTurnBack(task);
        }
    } else {
        _factoryLiftLower(task);
        if (requestedPosition & FACTORY_LIFT_POSITION_TURNED) {
            _factoryLiftJamTurnOut(task);
        } else {
            _factoryLiftJamTurnBack(task);
        }
    }
    work->moveFrames++;
    // Use this frame's original request even if a jam handler just rolled it back.
    _factoryLiftSyncCollision(task, 0, requestedPosition & FACTORY_LIFT_POSITION_TURNED);
    actorRenderComposeCoord(coord);
    worldCoordSetModelLighting(liftModel, coord->workm.t, 0, 3);
}

/// Tears down the lift and its hatch child through the default task cleanup.
///
/// Releases the lift's owned work and model resources. The borrowed panel
/// slot and the room's collision pools remain owned by the room.
static void _factoryLiftExit(Task* task)
{
    taskKill(task);
}

/// Binds the lift model to its work-owned matrices and samples three room lights.
///
/// Requires live `FactoryLiftWork`, model/root and initialized coordinate,
/// room-light and scratch/GTE state. The model borrows both matrices until task
/// teardown. Samples the root's complete composition-frame XYZ after marking
/// and composing it, without converting that sample to another frame.
static void _factoryLiftBindLighting(Task* task)
{
    enum { FACTORY_LIFT_LIGHT_COUNT = 3 };
    GfxCoord*        rootCoord;
    FactoryLiftWork* work;
    TmdObject*       model;

    work                    = task->work;
    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    model->lightMtx         = &work->light;
    model->colorMtx         = &work->color;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldCoordSetModelLighting(model, rootCoord->workm.t, 0, FACTORY_LIFT_LIGHT_COUNT);
}

/// Reports that the lift movement has settled to the active operator panel.
///
/// A null `panelTask` means no panel session is open. A live receiver must
/// accept `FACTORY_PANEL_MESSAGE_MOVE_SETTLED`; both payload words are zero.
/// Dispatch is synchronous and its undefined result is discarded.
static void _factoryLiftNotifyPanel(Task* panelTask)
{
    if (panelTask != NULL) {
        taskMessageDispatch(panelTask, FACTORY_PANEL_MESSAGE_MOVE_SETTLED, 0, 0);
    }
}

/// Attaches the hatch model to its lift and initializes its swing work.
///
/// `task` must own a live TMD model and borrow the lift task in `spawnArg2`.
/// The hatch inherits the lift's draw flags and lighting, uses a fixed offset
/// in the lift root's frame and joins its teardown tree. Allocation failure
/// kills the hatch; success advances the task to its per-frame state.
/// Zeroed work begins in `FACTORY_HATCH_STATE_WATCH` with the hatch closed.
/// An already-set open flag instead stores 0xFF and turns the model open;
/// that selector has no handler, and this startup path remains unproven.
static void _factoryHatchInit(Task* task)
{
    Task*             liftTask  = task->spawnArg2.pointer;
    TmdObject*        model     = task->extra.tmd;
    TmdObject*        liftModel = liftTask->extra.tmd;
    GfxCoord*         coord     = model->coords;
    GfxCoord*         liftCoord = liftModel->coords;
    FactoryHatchWork* work      = memCalloc(sizeof(FactoryHatchWork), 0);
    u16               flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    // Inherit visibility and primitive-buffer policy from the lift.
    task->work   = work;
    flags        = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags = flags;
    if (!(liftModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(liftModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(model);
    } else {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    // Keep the hatch's offset and lighting in the lift's coordinate frame.
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->parent     = liftCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN) == 1) {
        // Not a handler slot: the dispatcher has no entry for this value.
        work->state = 0xFF;
        RotMatrixX(FACTORY_HATCH_INITIAL_OPEN_ANGLE, &coord->coord);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = liftModel->lightMtx;
    model->colorMtx     = liftModel->colorMtx;
    taskReparent(liftTask, task);
    task->state += 1;
}

/// Dispatches the hatch's watch, open or close state and returns settled swings to watching.
///
/// Requires a live hatch model and `FactoryHatchWork` whose selector names a
/// `FACTORY_HATCH_STATE_` handler. Copies the complete handler table to the
/// stack, then calls the selected slot without a range check. A zero result
/// preserves the handler's selected state; nonzero selects WATCH next frame.
static void _factoryHatchUpdate(Task* task)
{
    FactoryHatchWork*          work = task->work;
    FactoryHatchStateFuncTable states;

    states = _gFactoryHatchStates;
    if (states.funcs[work->state](task) != 0) {
        work->state = FACTORY_HATCH_STATE_WATCH;
    }
}

/// Arms a hatch swing when its open flag changes between closed and open.
///
/// Runs `FACTORY_HATCH_STATE_WATCH` on a live task holding `FactoryHatchWork`.
/// Only a 0-to-1 edge selects `FACTORY_HATCH_STATE_OPEN`, and only a 1-to-0
/// edge selects `FACTORY_HATCH_STATE_CLOSE`; either restarts the movement step.
/// Other nibble values are recorded but arm no movement. Returns 0 even when
/// arming a swing, so the dispatcher preserves the newly selected state.
static s32 _factoryHatchWatch(Task* task)
{
    FactoryHatchWork* work = task->work;
    u8                flag = gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN);

    if (flag == FACTORY_HATCH_FLAG_OPEN && work->prevFlag == FACTORY_HATCH_FLAG_CLOSED) {
        work->state = FACTORY_HATCH_STATE_OPEN;
        work->step  = 0;
    } else if (flag == FACTORY_HATCH_FLAG_CLOSED && work->prevFlag == FACTORY_HATCH_FLAG_OPEN) {
        work->state = FACTORY_HATCH_STATE_CLOSE;
        work->step  = 0;
    }
    work->prevFlag = flag;
    return 0;
}

void factoryLiftRun(Task* task)
{
    TaskFuncTable3 states;

    states = _gFactoryLiftStates;
    states.funcs[task->state](task);
}

void factoryHatchRun(Task* task)
{
    TaskFuncTable3 states;

    states = _gFactoryHatchTaskStates;
    states.funcs[task->state](task);
}

void factoryLampScene(Task* task)
{
    enum {
        FACTORY_LAMP_SCENE_START       = 0,
        FACTORY_LAMP_SCENE_HIDE_PLAYER = 1,
        FACTORY_LAMP_SCENE_WAIT_CAP    = 2,
        FACTORY_LAMP_SCENE_FINISH      = 3,
    };
    s32 state = task->state;

    switch (state) {
        case FACTORY_LAMP_SCENE_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case FACTORY_LAMP_SCENE_HIDE_PLAYER:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) < FACTORY_LAMP_PROGRESS_SECOND) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
            }
            task->state++;
            /* fallthrough */
        case FACTORY_LAMP_SCENE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case FACTORY_LAMP_SCENE_FINISH:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, FACTORY_LAMP_PROGRESS_SECOND);
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            taskKill(task);
            break;
    }
}

void factoryCapScene(Task* task)
{
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
        capRunCommandWithTransition(task->spawnArg1.value);
    }
    taskKill(task);
}

void factoryHatchScene(Task* task)
{
    enum {
        FACTORY_HATCH_SCENE_OPEN       = 0,
        FACTORY_HATCH_SCENE_WAIT_OPEN  = 1,
        FACTORY_HATCH_SCENE_RUN_CAP    = 2,
        FACTORY_HATCH_SCENE_WAIT_CLOSE = 3,
    };
    switch (task->state) {
        case FACTORY_HATCH_SCENE_OPEN:
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 1);
            task->killCountdown = FACTORY_HATCH_SCENE_OPEN_COUNTDOWN;
            task->state         = task->state + 1;
            return;
        case FACTORY_HATCH_SCENE_RUN_CAP:
            capRunCommandWithTransition(task->spawnArg1.value);
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 0);
            task->killCountdown = FACTORY_HATCH_SCENE_CLOSE_COUNTDOWN;
            task->state         = task->state + 1;
            return;
        case FACTORY_HATCH_SCENE_WAIT_OPEN:
        case FACTORY_HATCH_SCENE_WAIT_CLOSE:
            if (--task->killCountdown < 0) {
                task->state = task->state + 1;
            }
            return;
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
    }
}
