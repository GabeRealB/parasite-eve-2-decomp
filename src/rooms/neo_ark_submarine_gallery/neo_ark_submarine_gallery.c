#include "rooms/neo_ark_submarine_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_submarine_gallery_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// 0x1E pair the gallery hands `taskSpawn` for the helper it raises in state 3,
/// the same shape `D_mine_mesa_80189B38` has.
extern RoomFadeStorage D_neo_ark_submarine_gallery_8018591C;

/// Staging save location the gallery commits: area / warp / room
/// hold what `neoArkSubmarineGalleryResolveRoomEvent` copies out of the
/// resolved destination, and `neoArkSubmarineGalleryDepartToIslandTask` moves
/// those same three bytes into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area`,
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp` and
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room`.
extern RoomEventMsg D_neo_ark_submarine_gallery_80185924;

AreaApplyRec D_neo_ark_submarine_gallery_8018590C[4] = {
    { 5, 12, 4, 0 },
    { 5, 14, 4, 0 },
    { 5, 30, 4, 0 },
    { 255, 0, 0, 0 },
};

RoomFadeStorage D_neo_ark_submarine_gallery_8018591C = { 0 };

RoomEventMsg D_neo_ark_submarine_gallery_80185924 = { 0 };

static void _neoArkSubmarineGalleryInitializeRoom(Task* task);

static void _neoArkSubmarineGalleryMessageTaskIdle(Task* unusedTask);

static void _neoArkSubmarineGalleryDrawRedDisc(u16 radius);
static void _neoArkSubmarineGalleryInitRedDiscTask(Task* task);
static void _neoArkSubmarineGalleryUpdateRedDiscTask(Task* task);
static void _neoArkSubmarineGalleryIdleRedDiscTask(Task* unusedTask);

/// Red-disc room selection and radius, in whole room-coordinate units.
enum {
    NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT     = 4,
    NEO_ARK_SUBMARINE_GALLERY_RED_DISC_RADIUS_FULL = 1920,
    NEO_ARK_SUBMARINE_GALLERY_RED_DISC_RADIUS_STEP = 16,
};

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// State handlers of the room's entry task, indexed by its state through
/// `neoArkSubmarineGalleryMessageTask`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D614 = {
    { _neoArkSubmarineGalleryInitializeRoom, _neoArkSubmarineGalleryMessageTaskIdle, taskKill }
};

/// Commits the staged destination and replaces the departure task with a reload request.
static inline void _neoArkSubmarineGalleryCommitIslandDeparture(Task* task)
{
    enum { STAGED_ROOM_BYTE      = 1,
           RELOAD_SPRITE_VARIANT = 1 };

    gDisplayState.spriteVariant                                = RELOAD_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_neo_ark_submarine_gallery_80185924.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_neo_ark_submarine_gallery_80185924.field_4;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_neo_ark_submarine_gallery_80185924)[STAGED_ROOM_BYTE];
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
    taskKill(task);
}

void neoArkSubmarineGalleryDepartToIslandTask(Task* task)
{
    enum { DEPARTURE_ASK,
           DEPARTURE_WAIT_CAP,
           DEPARTURE_CHECK_CHOICE,
           DEPARTURE_START_FADE,
           DEPARTURE_WAIT_SOUND,
           DEPARTURE_RELOAD,
           CAP_CONFIRM_DEPARTURE = 9,
           CAP_CHOICE_DEPART     = 10,
           CAP_KEEP_ACTORS_HELD  = 1,
           FADE_DURATION_FRAMES  = 30,
           FADE_TASK_BANK        = 1,
           FADE_TASK_SLOT        = 0x31 };

    switch (task->state) {
        case DEPARTURE_ASK:
            // Keep actor control held after CAP playback until the choice is handled.
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(CAP_CONFIRM_DEPARTURE, CAP_PLAYBACK_IN_PLACE);
            D_80115690 = CAP_KEEP_ACTORS_HELD;
            task->state++;
            break;
        case DEPARTURE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case DEPARTURE_CHECK_CHOICE:
            if (capGetVariantKey() != CAP_CHOICE_DEPART) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            sceneQueueBattleEscapeResult();
            task->state++;
            break;
        case DEPARTURE_START_FADE:
            D_neo_ark_submarine_gallery_8018591C.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_neo_ark_submarine_gallery_8018591C.fade.phase      = SCREEN_FADE_RUNNING;
            D_neo_ark_submarine_gallery_8018591C.fade.rampFrames = FADE_DURATION_FRAMES;
            taskSpawn(FADE_TASK_BANK, FADE_TASK_SLOT, 0, &D_neo_ark_submarine_gallery_8018591C.fade);
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND, 0, 0);
            task->state++;
            break;
        case DEPARTURE_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND) == 0) {
                task->state++;
            }
            break;
        case DEPARTURE_RELOAD:
            // Commit the staged destination only after the departure sound finishes.
            _neoArkSubmarineGalleryCommitIslandDeparture(task);
            break;
    }
}

s32 neoArkSubmarineGalleryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 neoArkSubmarineGalleryResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { TRANSITION_DEFERRED = 0,
           TRANSITION_ALLOWED  = 1,
           STAGED_ROOM_BYTE    = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId == GAME_AREA_NEO_ARK_ISLAND) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            // Preserve the resolved destination for the asynchronous confirmation task.
            D_neo_ark_submarine_gallery_80185924.warp                      = (u8)reply->areaId;
            D_neo_ark_submarine_gallery_80185924.field_4                   = reply->warp;
            ((u8*)&D_neo_ark_submarine_gallery_80185924)[STAGED_ROOM_BYTE] = reply->room;
            taskSpawnFromTable(&D_neo_ark_submarine_gallery_801818AC, 0, 0, 0);
        }
        return TRANSITION_DEFERRED;
    }
    return TRANSITION_ALLOWED;
}

s32 neoArkSubmarineGalleryHandleCapCommand(Task* task, s32 messageId, s32 commandIndex, s32 unused)
{
    enum {
        NEO_ARK_SUBMARINE_GALLERY_COMMAND_CHECK_ENCOUNTER  = 2,
        NEO_ARK_SUBMARINE_GALLERY_COMMAND_CHECK_ROOM_STATE = 3,
        NEO_ARK_SUBMARINE_GALLERY_ENCOUNTER_ACTIVE         = 1,
        NEO_ARK_SUBMARINE_GALLERY_CAP_ENCOUNTER_ACTIVE     = 2,
        NEO_ARK_SUBMARINE_GALLERY_CAP_ENCOUNTER_INACTIVE   = 4,
        NEO_ARK_SUBMARINE_GALLERY_CAP_FULL_DISC            = 5,
        NEO_ARK_SUBMARINE_GALLERY_CAP_BATTLE_ENGAGED       = 3,
        NEO_ARK_SUBMARINE_GALLERY_CAP_ROOM_IDLE            = 6,
    };

    switch (commandIndex) {
        case NEO_ARK_SUBMARINE_GALLERY_COMMAND_CHECK_ENCOUNTER:
            if (D_neo_ark_submarine_gallery_801818B8 == NEO_ARK_SUBMARINE_GALLERY_ENCOUNTER_ACTIVE) {
                capSpawnEventIfIdle(NEO_ARK_SUBMARINE_GALLERY_CAP_ENCOUNTER_ACTIVE, CAP_EVENT_NO_FLAGS);
            } else {
                capSpawnEventIfIdle(NEO_ARK_SUBMARINE_GALLERY_CAP_ENCOUNTER_INACTIVE, CAP_EVENT_NO_FLAGS);
            }
            break;
        case NEO_ARK_SUBMARINE_GALLERY_COMMAND_CHECK_ROOM_STATE:
            if (gGameSession->location.loc.variant == NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT) {
                capSpawnEventIfIdle(NEO_ARK_SUBMARINE_GALLERY_CAP_FULL_DISC, CAP_EVENT_NO_FLAGS);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capSpawnEventIfIdle(NEO_ARK_SUBMARINE_GALLERY_CAP_BATTLE_ENGAGED, CAP_EVENT_NO_FLAGS);
            } else {
                capSpawnEventIfIdle(NEO_ARK_SUBMARINE_GALLERY_CAP_ROOM_IDLE, CAP_EVENT_NO_FLAGS);
            }
            break;
    }
    return 0;
}

s32 neoArkSubmarineGalleryIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// Installs the gallery's room-message receiver and starts the full red-disc variant.
///
/// State 0 publishes the borrowed live task in the room slot. Variant 4 spawns
/// the coordinate-bodied red disc; other variants do not. Advances to idle.
static void _neoArkSubmarineGalleryInitializeRoom(Task* task)
{
    task->msgTable = D_neo_ark_submarine_gallery_80181884;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT) {
        taskSpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    task->state = task->state + 1;
}

/// Keeps the initialized room-message task alive without per-frame work.
static void _neoArkSubmarineGalleryMessageTaskIdle(Task* unusedTask)
{
}

void neoArkSubmarineGalleryMessageTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_neo_ark_submarine_gallery_8017D614;
    handlers.funcs[task->state](task);
}

/// Draws an additive red disc fading to black at its rim, at room Y = 5300.
///
/// Radius is in whole room-coordinate units; the caller grows it from 0 to
/// 1920. Refreshes the view transform and projects 32 fan segments, rejecting
/// a segment when the GTE error flag is set. Requires initialized projection
/// state, a word-aligned frame arena with up to `32 * (sizeof(POLY_G3) +
/// sizeof(DR_MODE))` free bytes, and the current OT's tags 24..1047. Changes
/// GTE state and retains only packet links in the current frame resources.
static void _neoArkSubmarineGalleryDrawRedDisc(u16 radius)
{
    enum {
        NEO_ARK_SUBMARINE_GALLERY_DISC_SEGMENT_COUNT      = 32,
        NEO_ARK_SUBMARINE_GALLERY_DISC_ANGLE_STEP         = ACTOR_TRANSFORM_ANGLE_TURN / NEO_ARK_SUBMARINE_GALLERY_DISC_SEGMENT_COUNT,
        NEO_ARK_SUBMARINE_GALLERY_DISC_ROOM_Y             = 5300,
        NEO_ARK_SUBMARINE_GALLERY_DISC_TRIG_FRACTION_BITS = 12,
        NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_SHIFT        = 4,
        NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_BIAS         = 24,
        NEO_ARK_SUBMARINE_GALLERY_DISC_ADDITIVE_TPAGE     = getTPage(0, GPU_BLEND_ADD, 640, 0),
    };

    SVECTOR  center;
    SVECTOR  rimStart;
    SVECTOR  rimEnd;
    long     screenCenter;
    long     screenStart;
    long     screenEnd;
    long     perspective;
    long     projectionFlags;
    POLY_G3* triangle;
    DR_MODE* drawMode;
    s32      depth;
    s32      angle;
    s16      segmentIndex;
    s16      planeY;

    /// Queues the projected wedge and makes additive blending execute before it.
    ///
    /// Captures the packed `screenCenter`/`screenStart`/`screenEnd`, `depth`, `triangle`,
    /// `drawMode`, the disc constants, `gGpuPrimCursor` and `gGpuCurrentOt`.
    /// Requires one POLY_G3 and one DR_MODE of free arena storage and a valid biased OT
    /// depth. Writes both pointer locals and advances the arena twice; no arguments
    /// are evaluated. The compound statement is undefined after this loop.
#define NEO_ARK_SUBMARINE_GALLERY_QUEUE_DISC_WEDGE()                                                                                          \
    {                                                                                                                                         \
        triangle       = gGpuPrimCursor;                                                                                                      \
        gGpuPrimCursor = triangle + 1;                                                                                                        \
        setPolyG3(triangle);                                                                                                                  \
        setRGB0(triangle, 0xFF, 0, 0);                                                                                                        \
        setRGB1(triangle, 0, 0, 0);                                                                                                           \
        setRGB2(triangle, 0, 0, 0);                                                                                                           \
        setSemiTrans(triangle, 1);                                                                                                            \
        GPU_PRIMITIVE_XY_WORD(triangle, 0) = screenCenter;                                                                                    \
        GPU_PRIMITIVE_XY_WORD(triangle, 1) = screenStart;                                                                                     \
        GPU_PRIMITIVE_XY_WORD(triangle, 2) = screenEnd;                                                                                       \
        addPrim(&gGpuCurrentOt[(depth >> NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_SHIFT) + NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_BIAS], triangle); \
        drawMode       = gGpuPrimCursor;                                                                                                      \
        gGpuPrimCursor = drawMode + 1;                                                                                                        \
        setDrawTPage(drawMode, 0, 0, NEO_ARK_SUBMARINE_GALLERY_DISC_ADDITIVE_TPAGE);                                                          \
        addPrim(&gGpuCurrentOt[(depth >> NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_SHIFT) + NEO_ARK_SUBMARINE_GALLERY_DISC_DEPTH_BIAS], drawMode); \
    }

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    planeY = NEO_ARK_SUBMARINE_GALLERY_DISC_ROOM_Y;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);

    // Project adjacent rim points in 4096-unit turn angles around the centre.
    for (segmentIndex = 0; segmentIndex < NEO_ARK_SUBMARINE_GALLERY_DISC_SEGMENT_COUNT; segmentIndex++) {
        center.vx   = 0;
        center.vy   = planeY;
        center.vz   = 0;
        angle       = segmentIndex * NEO_ARK_SUBMARINE_GALLERY_DISC_ANGLE_STEP;
        rimStart.vx = (rsin(angle) * radius) >> NEO_ARK_SUBMARINE_GALLERY_DISC_TRIG_FRACTION_BITS;
        rimStart.vy = planeY;
        rimStart.vz = (rcos(angle) * radius) >> NEO_ARK_SUBMARINE_GALLERY_DISC_TRIG_FRACTION_BITS;
        angle       = angle + NEO_ARK_SUBMARINE_GALLERY_DISC_ANGLE_STEP;
        rimEnd.vx   = (rsin(angle) * radius) >> NEO_ARK_SUBMARINE_GALLERY_DISC_TRIG_FRACTION_BITS;
        rimEnd.vy   = planeY;
        rimEnd.vz   = (rcos(angle) * radius) >> NEO_ARK_SUBMARINE_GALLERY_DISC_TRIG_FRACTION_BITS;
        depth       = RotTransPers3(&center, &rimStart, &rimEnd, &screenCenter, &screenStart, &screenEnd, &perspective, &projectionFlags);
        if (projectionFlags >= 0) {
            NEO_ARK_SUBMARINE_GALLERY_QUEUE_DISC_WEDGE();
        }
    }
#undef NEO_ARK_SUBMARINE_GALLERY_QUEUE_DISC_WEDGE
}

/// Initializes the red-disc radius and enters the disc's per-frame state.
///
/// Called in state 0 with a live task. `Task::killCountdown` holds the radius
/// in room-coordinate units: full size in variant 4, otherwise zero.
static void _neoArkSubmarineGalleryInitRedDiscTask(Task* task)
{
    if (gGameSession->location.loc.variant != NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT) {
        task->killCountdown = 0;
    } else {
        task->killCountdown = NEO_ARK_SUBMARINE_GALLERY_RED_DISC_RADIUS_FULL;
    }
    task->state++;
}

/// Grows and draws the red disc while the player task exists.
///
/// State 1 uses `Task::killCountdown` as a signed-halfword room-space radius.
/// Initialization and 16-unit steps keep it in 0..1920. Each active frame grows
/// it before drawing; frames without a player neither grow nor draw it.
/// A pending battle reset selects variant 4 without restarting the radius.
/// Requires the live room's projection, scratch, packet arena and ordering table.
static void _neoArkSubmarineGalleryUpdateRedDiscTask(Task* task)
{
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        if (gGameSession->location.loc.variant != NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT && gGameSession->battleResetPending != 0) {
            gGameSession->location.loc.variant = NEO_ARK_SUBMARINE_GALLERY_RED_DISC_VARIANT;
        }
        if (task->killCountdown < NEO_ARK_SUBMARINE_GALLERY_RED_DISC_RADIUS_FULL) {
            task->killCountdown = (s16)((u16)task->killCountdown + NEO_ARK_SUBMARINE_GALLERY_RED_DISC_RADIUS_STEP);
        }
        _neoArkSubmarineGalleryDrawRedDisc((u16)task->killCountdown);
    }
}

/// Leaves red-disc state 2 idle, preserving the radius without drawing.
///
/// Ignores the task argument; this state does not release the task or its body.
static void _neoArkSubmarineGalleryIdleRedDiscTask(Task* unusedTask)
{
}

/// State handlers of the disc task, indexed by its state through
/// `neoArkSubmarineGalleryRedDiscTask`: set-up, the per-frame disc sweep,
/// then an idle state.
static const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D63C = {
    { _neoArkSubmarineGalleryInitRedDiscTask, _neoArkSubmarineGalleryUpdateRedDiscTask,
      _neoArkSubmarineGalleryIdleRedDiscTask }
};

void neoArkSubmarineGalleryRedDiscTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_neo_ark_submarine_gallery_8017D63C;
    stateHandlers.funcs[task->state](task);
}
