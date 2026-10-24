#include "stages/akropolis/rooms/security_room.h"

#include "config.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "gameplay/action_prompt.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/security_room/declarations.inc"

static s32 _actionPromptHitTestDefault(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
#include "../../../../shared/room/action_prompt.h"
#include "../../../../shared/actor/actor_contacts.h"

/// Hotspot ids of the buttons that step the monitor's grey wash.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER = 0x8000,
    AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER   = 0x8001,
};

/// Encoding of `_AkropolisSecurityRoomMonitorWork::screenLevel`.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS       = 0x7F,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP       = 0x3E,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST    = 4,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_NEXT_LIMIT = 0xFE,
};

/// Camera view of the enemy seen on the monitor.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY = 0xA,
};

/// Frames in one pass of the monitor's overlay bar. The timer wraps to 0.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD = 0x97,
};

/// CAP slot started once for the enemy's camera view.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_CAP_SLOT = 0xC,
};

/// Prompt and exit states selected by the monitor's hotspot scan.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_OPEN_PROMPT = 3,
    AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_CLOSE       = 5,
};

/// Prompt and exit states selected by the power-supply hotspot scan.
enum {
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_OPEN_PROMPT = 3,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_CLOSE       = 5,
};

/// Shared start, playback-wait and result-handoff states of the unlock-scene tasks.
enum {
    AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START  = 0,
    AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT   = 1,
    AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH = 2,
};

/// Input updates before room interactions can be triggered after a panel closes.
enum { AKROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES = 10 };

/// Opener states and resident bank selectors for the two room panels.
enum {
    AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_START = 0,
    AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_WAIT  = 1,
    AKROPOLIS_SECURITY_ROOM_PANEL_TASK_BANK          = 2,
};

/// Panel states selected after a lock prompt or a successful key use.
enum {
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_SCAN_HOTSPOTS = 2,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_FADE_LEFT     = 6,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_FADE_RIGHT    = 10,
};

/// Valid released-lock flag values; bit zero is left and bit one is right.
enum {
    AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_NONE  = 0,
    AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_LEFT  = 1,
    AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_RIGHT = 2,
    AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_BOTH  = 3,
};

/// Caption slots and variants for examining or using each power-supply lock.
enum {
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_LEFT_CAP_SLOT        = 3,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RIGHT_CAP_SLOT       = 4,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_LOCKED   = 0,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_RELEASED = 1,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_WRONG_KEY        = 2,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_USE_RELEASED     = 3,
};

/// Frames waited after selecting an unlock view before spawning its scene task.
enum {
    AKROPOLIS_SECURITY_ROOM_UNLOCK_START_DELAY_FRAMES = 1,
};

/// Work block of the security-monitor task, held in `Task::work`.
typedef struct {
    u16 screenLevel;         // Wash detent: grey level plus AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS, stored unsigned
    s16 sweepTimer;          // Frames into the overlay bar's current pass
    s16 hotspotId;           // Confirmed hotspot (a camera view, or AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_*)
    s8  promptKind;          // First row of the prompt opened for that hotspot (0 "Examine", 1 "Push")
    s8  enemySceneArmed;     // 1 once the enemy's view was selected while its scene had not played
    s8  enemyCaptionStarted; // 1 once that visit's enemy caption has been started
} _AkropolisSecurityRoomMonitorWork;
STATIC_ASSERT_SIZEOF(_AkropolisSecurityRoomMonitorWork, 0xA);

/// `_AkropolisSecurityRoomPowerSupplyWork::usedKey` values.
enum {
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE  = 0, // No key item, or one the panel refuses
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT  = 1, // Item 0x104, which releases the left lock
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT = 2, // Item 0x103, which releases the right lock
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_OTHER = 3, // Item 0x101, accepted but releasing neither lock
};

/// `_AkropolisSecurityRoomPowerSupplyWork::hotspotId` values: the two locks of the panel, as the ids of their
/// hotspots.
enum {
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_LEFT  = 1,
    AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_RIGHT = 2,
};

/// Work block of the room's power-supply panel task, held in `Task::work`.
typedef struct {
    s32   usedKey;    // Key item used from the prompt, cleared once acted on (AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_*)
    Task* sceneTask;  // Task playing the unlock scene, awaited until it stops
    u16   timer;      // Fade level while the screen fades out (0 to 0x100, 4 a frame), then frames since the view changed
    s16   hotspotId;  // Hotspot the player confirmed (AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_*)
    s8    promptKind; // First row of the prompt opened for that hotspot (0 "Examine", 1 "Push")
} _AkropolisSecurityRoomPowerSupplyWork;
STATIC_ASSERT_SIZEOF(_AkropolisSecurityRoomPowerSupplyWork, 0x10);

/// Work block of the task that sounds the movie loop over one of the power-supply panel's two unlock scenes, held in
/// `Task::work`.
typedef struct {
    u16  fadeStarted; // Whether the loop's fade-out was queued at the cue frame (0 no, 1 yes)
    byte field_2[2];  // Allocated and cleared with the block but never accessed; role unproven
} _AkropolisSecurityRoomMovieLoopWork;
STATIC_ASSERT_SIZEOF(_AkropolisSecurityRoomMovieLoopWork, 0x4);

/// Where one camera feed's picture sits, in the texture page and on screen, in the one camera view that draws the
/// feeds as pictures.
typedef struct {
    u16 clutY;   // VRAM row of the feed's 256-color CLUT, which starts at X 0
    u16 u;       // Left texel column of the feed's quadrant of the page (0 or 128)
    u16 v;       // Top texel row of that quadrant (0 or 128)
    s16 centerX; // Center of the picture on screen, in primitive coordinates
    s16 centerY;
} _AkropolisSecurityRoomLockLampQuad;
STATIC_ASSERT_SIZEOF(_AkropolisSecurityRoomLockLampQuad, 0xA);

/// Scratch-stack block of the sweep line the monitor-feed task draws in the frame of its coordinate.
typedef struct {
    s32     depth; // SZ3 / 4 from projecting `end`: the near-cull test, the ordering-table depth and the blend-mode depth
    SVECTOR start; // Endpoint at the lower X, projected onto the primitive's vertex 0
    SVECTOR end;   // Endpoint at the higher X, projected onto vertex 1
} _AkropolisSecurityRoomSweepLineScratch;
STATIC_ASSERT_SIZEOF(_AkropolisSecurityRoomSweepLineScratch, 0x14);

/// Borrowed singleton monitor-panel task awaiting its result handoff.
static Task* _gAkropolisSecurityRoomMonitorPanelTask;
static Task* _gAkropolisSecurityRoomPowerSupplyTask;

/// The whole-unit world displacement the last `_actorContactApplyGridPushback`
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static EffectUnitQuadCorner _gAkropolisSecurityRoomFallingQuadUnitCorners[4];

/// Initializes one reserved prompt edge from signed screen coordinates and RGB bytes.
#define AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, startX, startY, endX, endY, red, green, blue) \
    {                                                                                                \
        setLineF2(line);                                                                             \
        (line)->x0 = (startX);                                                                       \
        (line)->y0 = (startY);                                                                       \
        (line)->x1 = (endX);                                                                         \
        (line)->y1 = (endY);                                                                         \
        (line)->r0 = (red);                                                                          \
        (line)->g0 = (green);                                                                        \
        (line)->b0 = (blue);                                                                         \
    }

/// Initializes one reserved POLY_F4 with a semitransparent grey monitor rectangle.
#define AKROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity) \
    setPolyF4(quad);                                               \
    setSemiTrans(quad, true);                                      \
    (quad)->r0 = (intensity);                                      \
    (quad)->g0 = (intensity);                                      \
    (quad)->b0 = (intensity);                                      \
    (quad)->x0 = AKROPOLIS_SECURITY_ROOM_MONITOR_LEFT;             \
    (quad)->y0 = AKROPOLIS_SECURITY_ROOM_MONITOR_TOP;              \
    (quad)->x1 = AKROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;            \
    (quad)->y1 = AKROPOLIS_SECURITY_ROOM_MONITOR_TOP;              \
    (quad)->x2 = AKROPOLIS_SECURITY_ROOM_MONITOR_LEFT;             \
    (quad)->y2 = AKROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;           \
    (quad)->x3 = AKROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;            \
    (quad)->y3 = AKROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;

/// Converts a sweep endpoint from the task's local frame to 16-bit projection-input coordinates.
#define AKROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(point, coord) \
    gte_SetRotMatrix(&(coord)->workm);                                 \
    gte_ldv0(&(point));                                                \
    gte_rtv0();                                                        \
    gte_stsv(&(point));                                                \
    (point).vx += (coord)->workm.t[0];                                 \
    (point).vy += (coord)->workm.t[1];                                 \
    (point).vz += (coord)->workm.t[2];

/// Blends one complete lock-indicator palette in sixteen-color RGB555 spans.
#define AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, brightness, blendedPalette)                            \
    {                                                                                                                                                   \
        for ((colorIndex) = 0; (colorIndex) < ARRAY_SIZE(_gAkropolisSecurityRoomLockIndicatorOffPalette); (colorIndex) += GPU_RGB555_CLUT_ROW_COLORS) { \
            gpuBlendRgb555ClutRow(&(litPalette)[colorIndex], &(offPalette)[colorIndex], (brightness), &(blendedPalette)[colorIndex]);                   \
        }                                                                                                                                               \
    }

/// Seeds the falling quad's tumble rates and parent-space displacement per frame.
static inline void _akropolisSecurityRoomSeedQuadMotion(EffectWork* work)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_BIAS = 256,
        AKROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_MASK = 0x1F0,
        AKROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_BIAS = 128,
        AKROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_MASK = 0xF0,
        AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS     = 16,
        AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK     = 0x1F,
    };
    RANDOM_LCG_ADVANCE();
    work->period = AKROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_BIAS - (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_MASK);
    RANDOM_LCG_ADVANCE();
    work->step = AKROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_BIAS - (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vx = AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vy = AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vz = AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
}

/// Places one corner of the falling quad's local XZ square in its composed coordinate space.
static inline void _akropolisSecurityRoomTransformQuadCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const EffectWork* work, const GfxCoord* coord)
{
    quadScratch->vertices[cornerIndex].vx     = _gAkropolisSecurityRoomFallingQuadUnitCorners[cornerIndex].axis0Sign * work->scale;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = _gAkropolisSecurityRoomFallingQuadUnitCorners[cornerIndex].axis1Sign * work->scale;
    // Rotate the local corner before adding the composition-root translation.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx     += (u32)coord->workm.t[0];
    (&quadScratch->vertices[cornerIndex])->vy += (u32)coord->workm.t[1];
    (&quadScratch->vertices[cornerIndex])->vz += (u32)coord->workm.t[2];
}

/// Initializes one monitor-glow wedge's Gouraud quad with a lit center and black outer vertices.
static inline void _akropolisSecurityRoomInitGlowQuad(POLY_G4* quad, s16 brightness, s16 redFactor, s16 greenFactor)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, brightness * redFactor, greenFactor * brightness, 0);
    setRGB3(quad, 0, 0, 0);
}

/// Initializes one monitor-glow streak's Gouraud polyline with a lit center and black ends.
static inline void _akropolisSecurityRoomInitGlowLine(LINE_G3* line, s16 brightness, s16 redFactor, s16 greenFactor)
{
    setLineG3(line);
    setRGB0(line, 0, 0, 0);
    setRGB1(line, brightness * redFactor, greenFactor * brightness, 0);
    setRGB2(line, 0, 0, 0);
}

static void _actionPromptResetDefault(Task* task);
static void _akropolisSecurityRoomInitializeRoomTask(Task* task);
static void _akropolisSecurityRoomIdleRoomTask(Task* task);
static void _akropolisSecurityRoomMonitorInit(Task* task);
static void _akropolisSecurityRoomMonitorScanHotspots(Task* task);
static void _akropolisSecurityRoomMonitorHandlePrompt(Task* task);
static void _akropolisSecurityRoomDrawMonitorWash(s16 washLevel);
static void _akropolisSecurityRoomDrawMonitorSweep(Task* task);
static void _akropolisSecurityRoomMonitorArmCursor(Task* task);
static void _akropolisSecurityRoomMonitorOpenPrompt(Task* task);
static void _akropolisSecurityRoomMonitorClose(Task* task);
static void _akropolisSecurityRoomMonitorScanRestrictedHotspots(Task* task);
static void _akropolisSecurityRoomPowerSupplyScanHotspots(Task* task);
static void _actionPromptMoveCursors(Task* task);
static void _actionPromptDrawCursor(s32 cursorX, s32 cursorY, s32 cursorMode);
static void _akropolisSecurityRoomPowerSupplyInit(Task* task);
static void _akropolisSecurityRoomPowerSupplyArmCursor(Task* task);
static void _akropolisSecurityRoomPowerSupplyOpenPrompt(Task* task);
static void _akropolisSecurityRoomPowerSupplyHandlePrompt(Task* task);
static void _akropolisSecurityRoomPowerSupplyClose(Task* task);
static s32  _actionPromptHitTest(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
static void _akropolisSecurityRoomShowReleasedLocks(s32 releasedLocks);
static void _akropolisSecurityRoomPowerSupplyFadeToRightUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyStartRightUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyWaitRightUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyPreparePlayerModel(Task* task);
static void _akropolisSecurityRoomPowerSupplyRestoreRoomView(Task* task);
static void _akropolisSecurityRoomPowerSupplyFinishRightUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyFadeToLeftUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyStartLeftUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyWaitLeftUnlock(Task* task);
static void _akropolisSecurityRoomPowerSupplyFinishLeftUnlock(Task* task);
static void _actionPromptReset(Task* task);
static void _akropolisSecurityRoomDrawSweepLine(Task* task);

static void _akropolisSecurityRoomMonitorCursorTask(Task* task);
static void _akropolisSecurityRoomPowerSupplyCursorTask(Task* task);
static s32  _akropolisSecurityRoomPowerSupplyUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedPayload);
static void _akropolisSecurityRoomRightUnlockSoundTask(Task* task);
static void _akropolisSecurityRoomLeftUnlockMovieTask(Task* task);

static void _akropolisSecurityRoomOpenMonitorTask(Task* task);
static void _akropolisSecurityRoomOpenPowerSupplyTask(Task* task);
static void _akropolisSecurityRoomPowerSupplyUseLeftLock(Task* task);
static void _akropolisSecurityRoomPowerSupplyUseRightLock(Task* task);

static s32 _akropolisSecurityRoomResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisSecurityRoomForwardKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedPayload);
static s32 _akropolisSecurityRoomHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedPayload);
static s32 _akropolisSecurityRoomHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedPayload);

/// Synchronous transition, monitor-action, power-supply-command and key-item messages.
static TaskMessageEntry _gAkropolisSecurityRoomRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisSecurityRoomResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisSecurityRoomHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _akropolisSecurityRoomHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisSecurityRoomForwardKeyItemUse },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Ascending execution priorities of the panel openers and their cursor/scene children.

#include "assets/rooms/akropolis/security_room/events.inc"

/// Bodyless opener recipes: index 0 monitor, index 1 power supply, index 2 end.
static TaskDesc _gAkropolisSecurityRoomPanelOpenerTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisSecurityRoomOpenMonitorTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisSecurityRoomOpenPowerSupplyTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Bodyless cursor recipe spawned at index zero and owned by the monitor panel.
static TaskDesc _gAkropolisSecurityRoomMonitorCursorTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisSecurityRoomMonitorCursorTask, { .value = 0 } };

/// Camera-button view base and first command rows of the panel hotspot prompts.
enum {
    AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW = 8,
    AKROPOLIS_SECURITY_ROOM_PROMPT_EXAMINE            = 0,
    AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH               = 1,
};

/// Monitor camera-selection and grey-wash hit rectangles, followed by an end marker.
static ActionPromptHotspot _gAkropolisSecurityRoomMonitorHotspots[9] = {
    { -30, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { -7, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW + 1, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { 16, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { 39, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW + 3, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { 61, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW + 4, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { 82, 81, 12, 11, AKROPOLIS_SECURITY_ROOM_MONITOR_CAMERA_FIRST_VIEW + 5, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { -141, -76, 12, 11, (s16)AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { -141, -58, 12, 11, (s16)AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER, AKROPOLIS_SECURITY_ROOM_PROMPT_PUSH, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Five saved grey-wash levels, darkest first, with one unread stored halfword.
static s16 _gAkropolisSecurityRoomMonitorWashDetents[6] = {
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST + AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST + 2 * AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST + 3 * AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP,
    AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST + 4 * AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP,
    8481,
};

/// Bodyless cursor recipe spawned at index zero and owned by the power-supply panel.
static TaskDesc _gAkropolisSecurityRoomPowerSupplyCursorTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisSecurityRoomPowerSupplyCursorTask, { .value = 0 } };

/// Key-item messages borrowed by the live power-supply panel, followed by an end marker.
static TaskMessageEntry _gAkropolisSecurityRoomPowerSupplyMessages[2] = {
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisSecurityRoomPowerSupplyUseKeyItem },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Left- and right-lock Examine hit rectangles, followed by an end marker.
static ActionPromptHotspot _gAkropolisSecurityRoomPowerSupplyHotspots[3] = {
    { -68, 20, 24, 24, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_LEFT, AKROPOLIS_SECURITY_ROOM_PROMPT_EXAMINE, 0 },
    { 54, 20, 24, 24, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_RIGHT, AKROPOLIS_SECURITY_ROOM_PROMPT_EXAMINE, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Independent unlock-scene recipes: index 0 right-lock sound, index 1 left-lock movie.
static TaskDesc _gAkropolisSecurityRoomUnlockSceneTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisSecurityRoomRightUnlockSoundTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisSecurityRoomLeftUnlockMovieTask, { .value = 0 } },
};

/// All-black 256-color RGB555 source shared by the four lock-indicator blends.
static u16 _gAkropolisSecurityRoomLockIndicatorOffPalette[256] = { 0 };

/// Lit 256-color RGB555 source for the left locked indicator (lamp bit 0).
static u16 _gAkropolisSecurityRoomLeftLockedPalette[256] = {
#include "assets/rooms/akropolis/security_room/lamp_left_locked.inc"
};

/// Lit 256-color RGB555 source for the left released indicator (lamp bit 1).
static u16 _gAkropolisSecurityRoomLeftReleasedPalette[256] = {
#include "assets/rooms/akropolis/security_room/lamp_left_released.inc"
};

/// Lit 256-color RGB555 source for the right locked indicator (lamp bit 2).
static u16 _gAkropolisSecurityRoomRightLockedPalette[256] = {
#include "assets/rooms/akropolis/security_room/lamp_right_locked.inc"
};

/// Lit 256-color RGB555 source for the right released indicator (lamp bit 3).
static u16 _gAkropolisSecurityRoomRightReleasedPalette[256] = {
#include "assets/rooms/akropolis/security_room/lamp_right_released.inc"
};

/// Writable 256-color RGB555 blend for the left locked lamp, uploaded to VRAM row 270.
static u16 _gAkropolisSecurityRoomLeftLockedClut[256] = { 0 };

/// Writable 256-color RGB555 blend for the left released lamp, uploaded to VRAM row 271.
static u16 _gAkropolisSecurityRoomLeftReleasedClut[256] = { 0 };

/// Writable 256-color RGB555 blend for the right locked lamp, uploaded to VRAM row 263.
static u16 _gAkropolisSecurityRoomRightLockedClut[256] = { 0 };

/// Writable 256-color RGB555 blend for the right released lamp, uploaded to VRAM row 264.
static u16 _gAkropolisSecurityRoomRightReleasedClut[256] = { 0 };

/// Uploads the left locked/released and right locked/released CLUTs, then ends the list.
static GpuImageUpload _gAkropolisSecurityRoomLockIndicatorClutUploads[5] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 270, ARRAY_SIZE(_gAkropolisSecurityRoomLeftLockedClut), 1 }, (u_long*)_gAkropolisSecurityRoomLeftLockedClut },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 271, ARRAY_SIZE(_gAkropolisSecurityRoomLeftReleasedClut), 1 }, (u_long*)_gAkropolisSecurityRoomLeftReleasedClut },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 263, ARRAY_SIZE(_gAkropolisSecurityRoomRightLockedClut), 1 }, (u_long*)_gAkropolisSecurityRoomRightLockedClut },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 264, ARRAY_SIZE(_gAkropolisSecurityRoomRightReleasedClut), 1 }, (u_long*)_gAkropolisSecurityRoomRightReleasedClut },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Lamp bits in palette/quad/glow order: left locked/released, right locked/released.
enum {
    AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_LOCKED    = 1 << 0,
    AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_RELEASED  = 1 << 1,
    AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_LOCKED   = 1 << 2,
    AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_RELEASED = 1 << 3,
};

/// Lamp-enable masks indexed by the two released-lock bits (0..3).
static u16 _gAkropolisSecurityRoomLockIndicatorMasksByReleasedLocks[4] = {
    [AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_NONE]  = AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_LOCKED | AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_LOCKED,
    [AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_LEFT]  = AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_RELEASED | AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_LOCKED,
    [AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_RIGHT] = AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_LOCKED | AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_RELEASED,
    [AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_BOTH]  = AKROPOLIS_SECURITY_ROOM_INDICATOR_LEFT_RELEASED | AKROPOLIS_SECURITY_ROOM_INDICATOR_RIGHT_RELEASED,
};

/// Palette row, texture quadrant and screen center of the four lock-status lamps.
static _AkropolisSecurityRoomLockLampQuad _gAkropolisSecurityRoomLockIndicatorQuads[4] = {
    { 270, 0, 0, -89, -14 },
    { 271, 0, 128, -21, -13 },
    { 263, 128, 0, 29, -13 },
    { 264, 128, 128, 100, -15 },
};

enum {
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_GREEN = 1 << 0,
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_RED   = 1 << 1,
};

#include "assets/rooms/akropolis/security_room/markers.inc"
/// Red/green glow-channel bits in left locked/released, right locked/released order.
static s16 _gAkropolisSecurityRoomLockIndicatorGlowColorMasks[4] = {
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_RED | AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_GREEN,
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_GREEN,
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_RED | AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_GREEN,
    AKROPOLIS_SECURITY_ROOM_LOCK_INDICATOR_GLOW_RED,
};

/// Unit XZ corners of the falling square, in quad-strip order.
static EffectUnitQuadCorner _gAkropolisSecurityRoomFallingQuadUnitCorners[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

#include "assets/rooms/akropolis/security_room/header.inc"
#include "assets/rooms/akropolis/security_room/geometry.inc"
#include "assets/rooms/akropolis/security_room/view_boundaries.inc"
#include "assets/rooms/akropolis/security_room/action_triggers.inc"
#include "assets/rooms/akropolis/security_room/placements.inc"
#include "assets/rooms/akropolis/security_room/lights.inc"
#include "assets/rooms/akropolis/security_room/sprites.inc"
#include "assets/rooms/akropolis/security_room/cameras.inc"
#include "assets/rooms/akropolis/security_room/updates.inc"
#include "assets/rooms/akropolis/security_room/surfaces.inc"
static TmdBone _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/akropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/akropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Verts[56] = {
#include "assets/akropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Normals[6] = {
#include "assets/akropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Stream[215] = {
#include "assets/akropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gAkropolisSecurityRoomAkropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0PartVerts,
    _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Verts,
    _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Normals,
    _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Skeleton,
    _gAkropolisSecurityRoomAkropolisSanctuaryModel090F0Stream,
};

/// Borrowed live power-supply panel handle used to forward room key-item messages.
static Task* _gAkropolisSecurityRoomPowerSupplyTask;

SVECTOR ActorContact_ScratchPosition;

static void _akropolisSecurityRoomOutlinePromptRect(ActionPromptRect* rect, u8 red, u8 green, u8 blue);
static void _akropolisSecurityRoomUpdatePlacedObjectVisibility(Task* task);

/// Accepts a room transition by copying the complete request to its reply.
static s32 _akropolisSecurityRoomResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return 1;
}

/// Forwards a selected key item to the live power-supply panel.
static s32 _akropolisSecurityRoomForwardKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedPayload)
{
    Task* panelTask;
    s32   useResult;

    panelTask = _gAkropolisSecurityRoomPowerSupplyTask;
    if (panelTask == NULL) {
        useResult = ROOM_KEY_ITEM_USE_REFUSED;
    } else {
        useResult = taskMessageDispatch(panelTask, messageId, itemId, unusedPayload);
    }
    return useResult;
}

/// Opens the power-supply panel for room command 2 and returns zero.
static s32 _akropolisSecurityRoomHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedPayload)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_COMMAND_POWER_SUPPLY = 2,
        AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_OPENER  = 1,
    };

    if (commandId == AKROPOLIS_SECURITY_ROOM_COMMAND_POWER_SUPPLY) {
        taskSpawnFromTable(_gAkropolisSecurityRoomPanelOpenerTaskDescs, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_OPENER, 0, 0);
    }
    return 0;
}

/// Opens the security monitor for room action zero.
static s32 _akropolisSecurityRoomHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedPayload)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_ACTION_MONITOR = 0,
        AKROPOLIS_SECURITY_ROOM_MONITOR_OPENER = 0,
    };

    if (request->actionId == AKROPOLIS_SECURITY_ROOM_ACTION_MONITOR) {
        taskSpawnFromTable(_gAkropolisSecurityRoomPanelOpenerTaskDescs, AKROPOLIS_SECURITY_ROOM_MONITOR_OPENER, 0, 0);
    }
    return;
}
/// Room-message task states 0..2: register the receiver, idle and retire it.
static const TaskFuncTable3 _gAkropolisSecurityRoomMessageStates = { {
    _akropolisSecurityRoomInitializeRoomTask,
    _akropolisSecurityRoomIdleRoomTask,
    taskKill,
} };

/// Holds and hides the player while the security-monitor panel is open.
static void _akropolisSecurityRoomOpenMonitorTask(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_MONITOR_TASK = 9 };
    s32 panelResult;

    switch (task->state) {
        case AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_START:
            printf("monitor\n");
            _gAkropolisSecurityRoomMonitorPanelTask = taskSpawn(AKROPOLIS_SECURITY_ROOM_PANEL_TASK_BANK, AKROPOLIS_SECURITY_ROOM_MONITOR_TASK, 0, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_WAIT:
            if (taskPollKill(_gAkropolisSecurityRoomMonitorPanelTask, &panelResult) != 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                taskKill(task);
            }
            return;
    }
}

/// Power-supply debug line and the two non-text bytes retained after its terminator.
static const struct {
    char text[sizeof("power supply\n")]; // Debug line, including its NUL terminator.
    u8   retainedBytes[2];               // Serialized non-text bytes; original role unproven.
} _gAkropolisSecurityRoomPowerSupplyDebugText = {
    .text          = "power supply\n",
    .retainedBytes = { 0x40, 0x11 },
};

/// Holds the player and releases its model buffers while the power-supply panel is open.
static void _akropolisSecurityRoomOpenPowerSupplyTask(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_TASK = 10 };
    s32 panelResult;

    switch (task->state) {
        case AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_START:
            printf(_gAkropolisSecurityRoomPowerSupplyDebugText.text);
            _gAkropolisSecurityRoomPowerSupplyTask = taskSpawn(AKROPOLIS_SECURITY_ROOM_PANEL_TASK_BANK, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_TASK, 0, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_SECURITY_ROOM_PANEL_OPENER_STATE_WAIT:
            if (taskPollKill(_gAkropolisSecurityRoomPowerSupplyTask, &panelResult) != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                _gAkropolisSecurityRoomPowerSupplyTask = NULL;
                taskKill(task);
            }
            return;
    }
}

/// Publishes the room's message receiver and clears the live power-supply panel handle.
static void _akropolisSecurityRoomInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisSecurityRoomRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state                            = task->state + 1;
    _gAkropolisSecurityRoomPowerSupplyTask = NULL;
}

/// Keeps the room-message task alive and available to receive messages.
static void _akropolisSecurityRoomIdleRoomTask(Task* task)
{
}

/// Monitor-panel states 0..6: setup, arm, scan, open/handle commands, close, restricted scan.
static const TaskFuncTable7 _gAkropolisSecurityRoomMonitorStates = { {
    _akropolisSecurityRoomMonitorInit,
    _akropolisSecurityRoomMonitorArmCursor,
    _akropolisSecurityRoomMonitorScanHotspots,
    _akropolisSecurityRoomMonitorOpenPrompt,
    _akropolisSecurityRoomMonitorHandlePrompt,
    _akropolisSecurityRoomMonitorClose,
    _akropolisSecurityRoomMonitorScanRestrictedHotspots,
} };

void akropolisSecurityRoomMessageTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gAkropolisSecurityRoomMessageStates;
    handlers.funcs[task->state](task);
}

/// Initializes the security-monitor panel, its cursor and its presentation holds.
static void _akropolisSecurityRoomMonitorInit(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_RESTRICTED = 6,
        AKROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT      = 5,
        AKROPOLIS_SECURITY_ROOM_MONITOR_INITIAL_VIEW     = 8,
        AKROPOLIS_SECURITY_ROOM_MONITOR_RESTRICTED_VIEW  = 5,
    };
    _AkropolisSecurityRoomMonitorWork* work;
    ActionPromptHotspot*               hotspot;
    s16                                detentIndex;

    work = memCalloc(sizeof(_AkropolisSecurityRoomMonitorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&_gAkropolisSecurityRoomMonitorCursorTaskDesc, 0, 1, 0);
    task->work              = work;
    work->sweepTimer        = 0;
    detentIndex             = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTNESS);
    if ((u16)detentIndex < AKROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT) {
        work->screenLevel = _gAkropolisSecurityRoomMonitorWashDetents[detentIndex];
    } else {
        work->screenLevel = _gAkropolisSecurityRoomMonitorWashDetents[0];
    }
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) < AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_MONITOR_INITIAL_VIEW;
        // Commit the selected camera before advancing the panel state.
        task->state = task->state + 1;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_MONITOR_RESTRICTED_VIEW;
        task->state                                            = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_RESTRICTED;
    }
    displayAcquireMenuHold();
    gGameSession->hideHud      = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->eventState   = 1;
    hotspot                    = _gAkropolisSecurityRoomMonitorHotspots;
    if (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            hotspot->hit = 0;
            hotspot++;
        } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
    }
}

/// Draws the monitor picture and scans its camera and brightness hotspots.
static void _akropolisSecurityRoomMonitorScanHotspots(Task* task)
{
    _AkropolisSecurityRoomMonitorWork* work;
    ActionPromptHotspot*               hotspot;
    ActionPrompt*                      prompt;

    hotspot = _gAkropolisSecurityRoomMonitorHotspots;
    prompt  = gActionPromptSlots;
    work    = task->work;
    _akropolisSecurityRoomDrawMonitorWash(work->screenLevel - AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    _akropolisSecurityRoomDrawMonitorSweep(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    // Keep drawing the monitor while CAP temporarily owns input.
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hotspot->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_OPEN_PROMPT;
                    return;
                }
                hotspot++;
            } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_CLOSE;
    }
}

/// Applies a confirmed camera or brightness command and resumes monitor scanning.
static void _akropolisSecurityRoomMonitorHandlePrompt(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_SCAN_HOTSPOTS = 2,
        AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_SWAT           = 11,
        AKROPOLIS_SECURITY_ROOM_MONITOR_SWAT_CAP_SLOT       = 13,
        AKROPOLIS_SECURITY_ROOM_MONITOR_SWAT_SEEN           = 1 << 0,
        AKROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_SEEN          = 1 << 1,
    };
    _AkropolisSecurityRoomMonitorWork* work;
    McSaveData*                        save;
    s16                                confirmedId;
    u16                                confirmedBits;

    work                              = task->work;
    gActionPromptSlots[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    gActionPromptSlots[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        confirmedId = work->hotspotId;
        if (confirmedId >= 0) {
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.location.view != confirmedId) {
                save->state.location.view = work->hotspotId;
                sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_MONITOR_SELECT, 0, 0);
                // Arm the enemy caption when this confirm is what first shows that view.
                if ((work->hotspotId == AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY) && !(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN) & AKROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_SEEN)) {
                    work->enemySceneArmed = 1;
                }
            }
        }
        confirmedBits = work->hotspotId;
        if (confirmedBits == AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER) {
            if (((s16)work->screenLevel + AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP) < AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_NEXT_LIMIT) {
                work->screenLevel += AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP;
                sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTER, 0, 0);
            }
        } else if (confirmedBits == AKROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER) {
            if (((s16)work->screenLevel - AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP) > 0) {
                work->screenLevel -= AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP;
                sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_MONITOR_DARKER, 0, 0);
            }
        }
        // One-shot captions become eligible only above the darkest wash detent.
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view == AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_SWAT) &&
            ((s16)work->screenLevel != AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST) && !(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN) & AKROPOLIS_SECURITY_ROOM_MONITOR_SWAT_SEEN)) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_MONITOR_SWAT_CAP_SLOT, CAP_PLAYBACK_IN_PLACE, 0);
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN, gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN) | AKROPOLIS_SECURITY_ROOM_MONITOR_SWAT_SEEN);
        }
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view == AKROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY) &&
            ((s16)work->screenLevel != AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST) && (work->enemySceneArmed != 0) &&
            (work->enemyCaptionStarted == 0) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_RUPERT_SCENE_SEEN) == 0)) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_CAP_SLOT, CAP_PLAYBACK_IN_PLACE, 0);
            work->enemyCaptionStarted = 1;
        }
    }
    _akropolisSecurityRoomDrawMonitorWash(work->screenLevel - AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    _akropolisSecurityRoomDrawMonitorSweep(task);
    task->state = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_SCAN_HOTSPOTS;
}

/// Queues the four edges of a prompt rectangle in the supplied RGB byte color.
static void _akropolisSecurityRoomOutlinePromptRect(ActionPromptRect* rect, u8 red, u8 green, u8 blue)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT = 3,
    };
    LINE_F2* line;

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x, rect->y, rect->x + rect->w, rect->y, red, green, blue);
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x + rect->w, rect->y, rect->x + rect->w, rect->y + rect->h, red, green, blue);
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x + rect->w, rect->y + rect->h, rect->x, rect->y + rect->h, red, green, blue);
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x, rect->y + rect->h, rect->x, rect->y, red, green, blue);
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);
}

#undef AKROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE

/// Draws the monitor's signed grey wash and the opaque black strip at its bottom.
static void _akropolisSecurityRoomDrawMonitorWash(s16 washLevel)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT   = 12,
        AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT = 11,
        AKROPOLIS_SECURITY_ROOM_MONITOR_LEFT           = -102,
        AKROPOLIS_SECURITY_ROOM_MONITOR_RIGHT          = 108,
        AKROPOLIS_SECURITY_ROOM_MONITOR_TOP            = -95,
        AKROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM         = 60,
        AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP     = 56,
    };
    POLY_F4* quad;
    DR_MODE* drawMode;
    u16      intensity;

    if (washLevel >= 0) {
        intensity      = washLevel & AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        AKROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity);
        addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, quad);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_ADD, 640, 0));
        addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, drawMode);
    } else {
        intensity      = (~washLevel + 1) & 0xFF;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        AKROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity);
        addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, quad);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));
        addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, drawMode);
    }

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    quad->r0 = 0;
    quad->g0 = 0;
    quad->b0 = 0;
    quad->x0 = AKROPOLIS_SECURITY_ROOM_MONITOR_LEFT;
    quad->y0 = AKROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;
    quad->x1 = AKROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;
    quad->y1 = AKROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;
    quad->x2 = AKROPOLIS_SECURITY_ROOM_MONITOR_LEFT;
    quad->y2 = AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP;
    quad->x3 = AKROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;
    quad->y3 = AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP;
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT, quad);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_AVERAGE, 640, 0));
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT, drawMode);
}

#undef AKROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH

/// Initializes the monitor sweep's semitransparent grey line and its horizontal endpoints.
static inline void _akropolisSecurityRoomInitMonitorSweepLine(LINE_F2* line)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_SWEEP_GREY  = 0x60,
        AKROPOLIS_SECURITY_ROOM_SWEEP_LEFT  = -0x66,
        AKROPOLIS_SECURITY_ROOM_SWEEP_RIGHT = 0x6C,
    };

    setLineF2(line);
    setSemiTrans(line, true);
    line->r0 = AKROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->g0 = AKROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->b0 = AKROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->x0 = AKROPOLIS_SECURITY_ROOM_SWEEP_LEFT;
    line->x1 = AKROPOLIS_SECURITY_ROOM_SWEEP_RIGHT;
}

/// Draws the monitor's horizontal grey sweep and advances its repeating frame counter.
static void _akropolisSecurityRoomDrawMonitorSweep(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_SWEEP_TOP            = 0x5F,
        AKROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT        = 0xE,
        AKROPOLIS_SECURITY_ROOM_SWEEP_TEXTURE_PAGE_X = 640,
    };
    _AkropolisSecurityRoomMonitorWork* work;
    LINE_F2*                           line;
    DR_MODE*                           drawMode;
    s16                                sweepY;

    line           = gGpuPrimCursor;
    work           = task->work;
    gGpuPrimCursor = line + 1;
    _akropolisSecurityRoomInitMonitorSweepLine(line);
    sweepY   = work->sweepTimer - AKROPOLIS_SECURITY_ROOM_SWEEP_TOP;
    line->y1 = sweepY;
    line->y0 = sweepY;
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT, line);
    // Set the blend page before the line is consumed; OT insertion reverses order.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_AVERAGE, AKROPOLIS_SECURITY_ROOM_SWEEP_TEXTURE_PAGE_X, 0));
    addPrim(gGpuCurrentOt + AKROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT, drawMode);
    work->sweepTimer++;
    if (work->sweepTimer >= AKROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD) {
        work->sweepTimer = 0;
    }
}

#include "../../../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../../../shared/room/action_prompt_draw_cursor.inc.c"

/// Runs the monitor cursor through reset and per-frame movement.
static void _akropolisSecurityRoomMonitorCursorTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    states[task->state](task);
}

/// Enables the monitor cursor and advances to hotspot scanning.
static void _akropolisSecurityRoomMonitorArmCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Opens commands for the confirmed monitor hotspot while continuing the picture overlay.
static void _akropolisSecurityRoomMonitorOpenPrompt(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_HANDLE_PROMPT = 4 };
    ActionPrompt*                      prompt = gActionPromptSlots;
    _AkropolisSecurityRoomMonitorWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    _akropolisSecurityRoomDrawMonitorWash(work->screenLevel - AKROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    _akropolisSecurityRoomDrawMonitorSweep(task);
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_HANDLE_PROMPT;
}

/// Saves the monitor wash detent index, defaulting to the darkest detent.
static inline void _akropolisSecurityRoomSaveScreenLevel(s32 screenLevel)
{
    enum { AKROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT = 5 };
    s32 detentIndex;

    for (detentIndex = 0; detentIndex < AKROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT; detentIndex++) {
        if (screenLevel == _gAkropolisSecurityRoomMonitorWashDetents[detentIndex]) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTNESS, detentIndex);
            return;
        }
    }
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTNESS, 0);
}

/// Saves the monitor brightness and returns control to the room.
static void _akropolisSecurityRoomMonitorClose(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_MONITOR_RETURN_VIEW = 4 };
    _AkropolisSecurityRoomMonitorWork* work;

    work                              = task->work;
    gDirectionInteractionRearmUpdates = AKROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    _akropolisSecurityRoomSaveScreenLevel((s16)work->screenLevel);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_MONITOR_RETURN_VIEW;
    displayReleaseMenuHold();
    gGameSession->cutsceneHold = 0;
    gGameSession->hideHud      = 0;
    gGameSession->eventState   = 0;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Scans the restricted monitor without changing its camera or brightness.
static void _akropolisSecurityRoomMonitorScanRestrictedHotspots(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_MONITOR_RESTRICTED_CAP_COMMAND = 14 };
    ActionPrompt*        prompt  = gActionPromptSlots;
    ActionPromptHotspot* hotspot = _gAkropolisSecurityRoomMonitorHotspots;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    capRunCommand(AKROPOLIS_SECURITY_ROOM_MONITOR_RESTRICTED_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = AKROPOLIS_SECURITY_ROOM_MONITOR_STATE_CLOSE;
    }
}

#include "../../../../shared/room/action_prompt_hit_test.inc.c"

/// Power-supply states: panel interaction 0..5, left release 6..9, right release 10..15.
static const TaskFuncTable16 _gAkropolisSecurityRoomPowerSupplyStates = { {
    _akropolisSecurityRoomPowerSupplyInit,
    _akropolisSecurityRoomPowerSupplyArmCursor,
    _akropolisSecurityRoomPowerSupplyScanHotspots,
    _akropolisSecurityRoomPowerSupplyOpenPrompt,
    _akropolisSecurityRoomPowerSupplyHandlePrompt,
    _akropolisSecurityRoomPowerSupplyClose,
    _akropolisSecurityRoomPowerSupplyFadeToLeftUnlock,
    _akropolisSecurityRoomPowerSupplyStartLeftUnlock,
    _akropolisSecurityRoomPowerSupplyWaitLeftUnlock,
    _akropolisSecurityRoomPowerSupplyFinishLeftUnlock,
    _akropolisSecurityRoomPowerSupplyFadeToRightUnlock,
    _akropolisSecurityRoomPowerSupplyStartRightUnlock,
    _akropolisSecurityRoomPowerSupplyWaitRightUnlock,
    _akropolisSecurityRoomPowerSupplyPreparePlayerModel,
    _akropolisSecurityRoomPowerSupplyRestoreRoomView,
    _akropolisSecurityRoomPowerSupplyFinishRightUnlock,
} };

void akropolisSecurityRoomMonitorTask(Task* task)
{
    TaskFuncTable7 handlers;

    handlers = _gAkropolisSecurityRoomMonitorStates;
    handlers.funcs[task->state](task);
}

#include "../../../../shared/room/action_prompt_reset.inc.c"

/// Scans the power-supply panel's left and right lock hotspots.
static void _akropolisSecurityRoomPowerSupplyScanHotspots(Task* task)
{
    ActionPrompt*                          prompt  = gActionPromptSlots;
    ActionPromptHotspot*                   hotspot = _gAkropolisSecurityRoomPowerSupplyHotspots;
    _AkropolisSecurityRoomPowerSupplyWork* work    = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    // CAP temporarily owns input; retain the pending key during playback.
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_OPEN_PROMPT;
                    return;
                }
            }
        }
    } else {
        work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
        prompt->mode  = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_CLOSE;
    }
}

#include "../../../../shared/room/action_prompt_outline_rect.inc.c"

/// Commits the left lock release and transfers the panel from cursor input to its scene.
static inline void _akropolisSecurityRoomPowerSupplyBeginLeftUnlock(Task* task, _AkropolisSecurityRoomPowerSupplyWork* work)
{
    // Consume the matching key before committing the unlock and scene handoff.
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_BLUE_KEY);
    sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS, gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) | AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_LEFT);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS, AKROPOLIS_SANCTUARY_ROUTE_ESCALATOR_UNLOCKED);
    _akropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & 0xFF);
    work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    task->state   = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_FADE_LEFT;
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET, PAD_INPUT_SUPPRESS_GAMEPLAY);
    areaApplySavedUpdates(_gAkropolisSecurityRoomLeftUnlockAreaUpdates);
    taskKill(task->spawnArg2.pointer);
}

/// Examines the left lock or consumes its blue key and begins the unlock scene.
static void _akropolisSecurityRoomPowerSupplyUseLeftLock(Task* task)
{
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    releasedLocks;
    s32                                    usedKey;

    releasedLocks = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS);
    if ((releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_NONE) || (releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_RIGHT)) {
        usedKey = work->usedKey;
        if (usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_LEFT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_LOCKED);
        } else if (usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT) {
            _akropolisSecurityRoomPowerSupplyBeginLeftUnlock(task, work);
            return;
        } else {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_LEFT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_WRONG_KEY);
        }
    } else if ((releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_LEFT) || (releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_BOTH)) {
        if (work->usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_LEFT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_RELEASED);
        } else {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_LEFT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_USE_RELEASED);
        }
    } else {
        return;
    }
    task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_SCAN_HOTSPOTS;
}

/// Commits the right lock release and transfers the panel from cursor input to its scene.
static inline void _akropolisSecurityRoomPowerSupplyBeginRightUnlock(Task* task, _AkropolisSecurityRoomPowerSupplyWork* work)
{
    enum { AKROPOLIS_SECURITY_ROOM_FOLLOW_UP_UPDATE_SPLIT = 3 };
    // Consume the matching key before committing the unlock and scene handoff.
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_RED_KEY);
    sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS, gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) | AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_RIGHT);
    _akropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & 0xFF);
    work->usedKey            = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    task->state              = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_FADE_RIGHT;
    gGameSession->eventState = 1;
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET, PAD_INPUT_SUPPRESS_GAMEPLAY);
    areaApplySavedUpdates(_gAkropolisSecurityRoomRightUnlockAreaUpdates);
    if (gameFlagGetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER) < AKROPOLIS_SECURITY_ROOM_FOLLOW_UP_UPDATE_SPLIT) {
        areaApplySavedUpdates(_gAkropolisSecurityRoomRightUnlockEarlyStoryAreaUpdates);
    } else {
        areaApplySavedUpdates(_gAkropolisSecurityRoomRightUnlockLateStoryAreaUpdates);
    }
    taskKill(task->spawnArg2.pointer);
}

/// Examines the right lock or consumes its red key and begins the unlock scene.
static void _akropolisSecurityRoomPowerSupplyUseRightLock(Task* task)
{
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    releasedLocks;
    s32                                    usedKey;

    releasedLocks = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS);
    if ((releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_NONE) || (releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_LEFT)) {
        usedKey = work->usedKey;
        if (usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RIGHT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_LOCKED);
        } else if (usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT) {
            _akropolisSecurityRoomPowerSupplyBeginRightUnlock(task, work);
            return;
        } else {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RIGHT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_WRONG_KEY);
            task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_SCAN_HOTSPOTS;
            return;
        }
    } else if ((releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_RIGHT) || (releasedLocks == AKROPOLIS_SECURITY_ROOM_LOCKS_RELEASED_BOTH)) {
        if (work->usedKey == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RIGHT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_EXAMINE_RELEASED);
        } else {
            capStartSequenceSlot(AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RIGHT_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_CAP_USE_RELEASED);
        }
    } else {
        return;
    }
    task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_SCAN_HOTSPOTS;
}

/// Selects the additional private cursor task, with signature `void(Task*)`.
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursors
#undef ACTION_PROMPT_DRAW_CURSOR
/// Selects the second prompt's private drawer for both fragments.
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursor
#include "../../../../shared/room/action_prompt_move_cursors.inc.c"
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursorsDefault

#include "../../../../shared/room/action_prompt_draw_cursor.inc.c"
#undef ACTION_PROMPT_DRAW_CURSOR
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursorDefault

/// Runs the power-supply cursor through reset and per-frame movement.
static void _akropolisSecurityRoomPowerSupplyCursorTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptReset,
        _actionPromptMoveCursors,
    };

    states[task->state](task);
}

/// Initializes the power-supply close-up, cursor and key-item message receiver.
static void _akropolisSecurityRoomPowerSupplyInit(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW = 6 };
    _AkropolisSecurityRoomPowerSupplyWork* work;
    ActionPromptHotspot*                   hotspot;

    work = memCalloc(sizeof(_AkropolisSecurityRoomPowerSupplyWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                = taskSpawnFromTable(&_gAkropolisSecurityRoomPowerSupplyCursorTaskDesc, 0, 1, 0);
    task->msgTable                                         = _gAkropolisSecurityRoomPowerSupplyMessages;
    task->work                                             = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW;
    task->state++;
    work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    work->timer   = 0;
    _akropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & 0xFF);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    displayAcquireMenuHold();
    for (hotspot = _gAkropolisSecurityRoomPowerSupplyHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
}

/// Enables the power-supply cursor and advances to lock hotspot scanning.
static void _akropolisSecurityRoomPowerSupplyArmCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Opens commands for the confirmed power-supply lock.
static void _akropolisSecurityRoomPowerSupplyOpenPrompt(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_HANDLE_PROMPT = 4 };
    ActionPrompt*                          prompt = gActionPromptSlots;
    _AkropolisSecurityRoomPowerSupplyWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_HANDLE_PROMPT;
}

/// Routes a confirmed panel command or latched key to the selected lock.
static void _akropolisSecurityRoomPowerSupplyHandlePrompt(Task* task)
{
    ActionPrompt*                          prompt = gActionPromptSlots;
    _AkropolisSecurityRoomPowerSupplyWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if ((itemMenuIsHotspotActionConfirmed() != 0) || (work->usedKey != AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE)) {
        if (work->hotspotId == AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_LEFT) {
            _akropolisSecurityRoomPowerSupplyUseLeftLock(task);
            work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
            return;
        }
        _akropolisSecurityRoomPowerSupplyUseRightLock(task);
        work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
        return;
    }
    task->state = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_SCAN_HOTSPOTS;
}

/// Closes the power-supply panel without playing an unlock scene.
static void _akropolisSecurityRoomPowerSupplyClose(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RETURN_VIEW = 3 };
    gDirectionInteractionRearmUpdates = AKROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    gGameSession->eventState                               = 0;
    gGameSession->hideHud                                  = 0;
    gGameSession->cutsceneHold                             = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RETURN_VIEW;
    displayReleaseMenuHold();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Selects the private hotspot tester for the power-supply prompt.
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTest
#include "../../../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault

/// Shows the power-supply panel's released-lock pictures for the supplied flag value.
static void _akropolisSecurityRoomShowReleasedLocks(s32 releasedLocks)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_LOCKS_NEITHER     = 0,
        AKROPOLIS_SECURITY_ROOM_LOCKS_LEFT        = 1,
        AKROPOLIS_SECURITY_ROOM_LOCKS_RIGHT       = 2,
        AKROPOLIS_SECURITY_ROOM_LOCKS_BOTH        = 3,
        AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW = 6,
    };
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteBatch*     batches;

    batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW - 1].batches;
    switch (releasedLocks & 0xFF) {
        case AKROPOLIS_SECURITY_ROOM_LOCKS_NEITHER:
            batches[1].hidden = 1;
            batches[2].hidden = 1;
            break;
        case AKROPOLIS_SECURITY_ROOM_LOCKS_LEFT:
            batches[1].hidden = 0;
            batches[2].hidden = 1;
            break;
        case AKROPOLIS_SECURITY_ROOM_LOCKS_RIGHT:
            batches[1].hidden = 1;
            batches[2].hidden = 0;
            break;
        case AKROPOLIS_SECURITY_ROOM_LOCKS_BOTH:
            batches[1].hidden = 0;
            batches[2].hidden = 0;
            break;
    }
}

/// Latches a selected key for the power-supply panel's confirmed lock.
static s32 _akropolisSecurityRoomPowerSupplyUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedPayload)
{
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (itemId == INVENTORY_COLLECTION_ID_PARTHENON_KEY) {
        work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_OTHER;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    if (itemId == INVENTORY_COLLECTION_ID_RED_KEY) {
        work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    if (itemId == INVENTORY_COLLECTION_ID_BLUE_KEY) {
        work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    work->usedKey = AKROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Fades the panel to black before the right-lock unlock movie.
static void _akropolisSecurityRoomPowerSupplyFadeToRightUnlock(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_STEP        = 4,
        AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND_LEVEL = 0x80,
        AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_END         = 0x100,
        AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_VIEW       = 0x10,
    };
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    u8                                     fadeLevel;

    fadeLevel = work->timer;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    work->timer = work->timer + AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_STEP;
    if (work->timer == AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND_LEVEL) {
        sndEvtRequestScriptStart(AKROPOLIS_SECURITY_ROOM_SOUND(AKROPOLIS_SECURITY_ROOM_SOUND_UNLOCK_FADE), 0, 0);
    }
    if (work->timer >= AKROPOLIS_SECURITY_ROOM_UNLOCK_FADE_END) {
        work->timer                                            = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_VIEW;
        task->state                                            = task->state + 1;
    }
}

/// Starts the right-lock movie's sound task after selecting its view.
static void _akropolisSecurityRoomPowerSupplyStartRightUnlock(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_SCENE_TASK = 0 };
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (work->timer == AKROPOLIS_SECURITY_ROOM_UNLOCK_START_DELAY_FRAMES) {
        work->sceneTask = taskSpawnFromTable(_gAkropolisSecurityRoomUnlockSceneTaskDescs, AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_SCENE_TASK, 0, 0);
        task->state     = task->state + 1;
    }
    work->timer = work->timer + 1;
}

/// Waits for the right-lock sound task's result handoff before restoring the room.
static void _akropolisSecurityRoomPowerSupplyWaitRightUnlock(Task* task)
{
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    sceneResult;

    if (taskPollKill(work->sceneTask, &sceneResult) != 0) {
        task->state = task->state + 1;
    }
}

/// Prepares the hidden player model for the return from the right-lock movie.
static void _akropolisSecurityRoomPowerSupplyPreparePlayerModel(Task* task)
{
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    task->state = task->state + 1;
}

/// Restores the normal room view after the right-lock scene and advances to cleanup.
static void _akropolisSecurityRoomPowerSupplyRestoreRoomView(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_NORMAL_VIEW = 3,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_NORMAL_VIEW;
    task->state                                            = task->state + 1;
}

/// Returns input and presentation to gameplay after the right-lock scene.
static void _akropolisSecurityRoomPowerSupplyFinishRightUnlock(Task* task)
{
    gDirectionInteractionRearmUpdates = AKROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_INPUT_SUPPRESS_GAMEPLAY);
    taskRequestKill(task, 0);
}

/// Clears the room's map mark and fades the panel to black for the left-lock movie.
static void _akropolisSecurityRoomPowerSupplyFadeToLeftUnlock(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_STEP        = 4,
        AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_SOUND_LEVEL = 128,
        AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_END         = 256,
        AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_VIEW             = 14,
    };
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    u8                                     fadeLevel;

    gameFlagSetNibble(EVENT_FLAG_MAP_DOOR_AKROPOLIS_FORKED_ROAD_OBSERVATORY, EVENT_FLAG_MAP_DOOR_UNMARKED);
    fadeLevel = work->timer;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    work->timer = work->timer + AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_STEP;
    if (work->timer == AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_SOUND_LEVEL) {
        sndEvtRequestScriptStart(AKROPOLIS_SECURITY_ROOM_SOUND(AKROPOLIS_SECURITY_ROOM_SOUND_UNLOCK_FADE), 0, 0);
    }
    if (work->timer >= AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_FADE_END) {
        work->timer                                            = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_VIEW;
        task->state                                            = task->state + 1;
    }
}

/// Starts the left-lock movie task after selecting its view.
static void _akropolisSecurityRoomPowerSupplyStartLeftUnlock(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_SCENE_TASK = 1 };
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (work->timer == AKROPOLIS_SECURITY_ROOM_UNLOCK_START_DELAY_FRAMES) {
        work->sceneTask = taskSpawnFromTable(_gAkropolisSecurityRoomUnlockSceneTaskDescs, AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_SCENE_TASK, 0, 0);
        task->state     = task->state + 1;
    }
    work->timer = work->timer + 1;
}

/// Waits for the left-lock movie task's handoff and then restores player drawing.
static void _akropolisSecurityRoomPowerSupplyWaitLeftUnlock(Task* task)
{
    _AkropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    sceneResult;

    if (taskPollKill(work->sceneTask, &sceneResult) != 0) {
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        task->state = task->state + 1;
    }
}

/// Restores room view 3 and returns input after the left-lock scene.
static void _akropolisSecurityRoomPowerSupplyFinishLeftUnlock(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_RETURN_VIEW = 3 };
    gDirectionInteractionRearmUpdates                      = AKROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SECURITY_ROOM_LEFT_UNLOCK_RETURN_VIEW;
    displayReleaseMenuHold();
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_INPUT_SUPPRESS_GAMEPLAY);
    taskRequestKill(task, 0);
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
}

void akropolisSecurityRoomPowerSupplyTask(Task* task)
{
    TaskFuncTable16 handlers;

    handlers = _gAkropolisSecurityRoomPowerSupplyStates;
    handlers.funcs[task->state](task);
}

// Bind the additional private reset callback, with signature void(Task*).
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptReset
#include "../../../../shared/room/action_prompt_reset.inc.c"
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptResetDefault

/// Plays the sound loop over the right-lock movie and hands completion to the panel.
static void _akropolisSecurityRoomRightUnlockSoundTask(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_FADE_CUE_FRAME = 70,
        AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL   = 20,
    };
    CdCmdQueue*                          queue;
    _AkropolisSecurityRoomMovieLoopWork* work;
    _AkropolisSecurityRoomMovieLoopWork* allocatedWork;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START:
            allocatedWork = memCalloc(sizeof(_AkropolisSecurityRoomMovieLoopWork), 0);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
            sndEvtRequestScriptStart(SOUND_AKROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT:
            if (queue->movieFrame >= AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_FADE_CUE_FRAME && work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_AKROPOLIS_SECURITY_ROOM_MOVIE_LOOP, AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL);
                work->fadeStarted = 1;
            }
            // Idle and Start advance independently, even when both occur together.
            if (cdCmdIsIdle()) {
                task->state = task->state + 1;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            if (work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_AKROPOLIS_SECURITY_ROOM_MOVIE_LOOP, AKROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL);
            }
            task->state = task->state + 1;
            return;
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH:
            taskRequestKill(task, 0);
            return;
    }
}

/// Starts the left-lock movie and hands completion or skip back to the panel.
static void _akropolisSecurityRoomLeftUnlockMovieTask(Task* task)
{
    u8          commandArgs[sizeof(((CdCmdEntry*)0)->args)];
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START:
            queue->movieFrame = 1;
            // Enqueue reads four bytes; playback interprets only the slot byte.
            commandArgs[0] = streamFindMovieSlot(&gGameSession->location, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT:
            if (cdCmdIsIdle() || padIsStartPressed() != 0) {
                task->state = task->state + 1;
            }
            return;
        case AKROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH:
            taskRequestKill(task, 0);
            return;
    }
}

void akropolisSecurityRoomLockLampsTask(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_STATE_INIT            = 0,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_STATE_UPDATE          = 1,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_POWER_SUPPLY_VIEW     = 6,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_GLOW_SKIP_VIEW        = 8,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_RIGHT_UNLOCK_VIEW     = 16,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP_SHIFT = 9,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP       = ONE / 8,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_0_MASK           = 1 << 0,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_1_MASK           = 1 << 1,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_2_MASK           = 1 << 2,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_3_MASK           = 1 << 3,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         colorIndex;
    s32         lampIndex;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    switch (task->state) {
        // Seed all four lamp CLUTs with the black source before their first draw.
        case AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_STATE_INIT: {
            const u16* offPalette     = _gAkropolisSecurityRoomLockIndicatorOffPalette;
            const u16* litPalette     = _gAkropolisSecurityRoomLeftLockedPalette;
            u16*       blendedPalette = _gAkropolisSecurityRoomLeftLockedClut;

            AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, 0, blendedPalette);
            litPalette     = _gAkropolisSecurityRoomLeftReleasedPalette;
            blendedPalette = _gAkropolisSecurityRoomLeftReleasedClut;
            AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, 0, blendedPalette);
            litPalette     = _gAkropolisSecurityRoomRightLockedPalette;
            blendedPalette = _gAkropolisSecurityRoomRightLockedClut;
            AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, 0, blendedPalette);
            litPalette     = _gAkropolisSecurityRoomRightReleasedPalette;
            blendedPalette = _gAkropolisSecurityRoomRightReleasedClut;
            AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, 0, blendedPalette);
            gpuUploadImages(_gAkropolisSecurityRoomLockIndicatorClutUploads);
            task->state = task->state + 1;
            break;
        }

        case AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_STATE_UPDATE:
            work->index = _gAkropolisSecurityRoomLockIndicatorMasksByReleasedLocks[gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS)];
            if ((viewGetMappedIndex() & 0xFF) == AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_POWER_SUPPLY_VIEW) {
                const u16* litPalette     = _gAkropolisSecurityRoomLeftLockedPalette;
                const u16* offPalette     = _gAkropolisSecurityRoomLockIndicatorOffPalette;
                u16*       blendedPalette = _gAkropolisSecurityRoomLeftLockedClut;
                s32        brightnessLimit;

                // Brighten active lamps and alternate the ceiling by one brightness step.
                brightnessLimit = ONE - ((gDisplayState.animFrame & 1) << AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP_SHIFT);
                work->scale     = (work->index & AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_0_MASK) ? ((work->scale < brightnessLimit) ? work->scale + AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP : brightnessLimit) : 0;
                work->angle     = (work->index & AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_1_MASK) ? ((work->angle < brightnessLimit) ? work->angle + AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP : brightnessLimit) : 0;
                work->period    = (work->index & AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_2_MASK) ? ((work->period < brightnessLimit) ? work->period + AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP : brightnessLimit) : 0;
                work->step      = (work->index & AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_LAMP_3_MASK) ? ((work->step < brightnessLimit) ? work->step + AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_BRIGHTNESS_STEP : brightnessLimit) : 0;

                AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, work->scale, blendedPalette);
                litPalette     = _gAkropolisSecurityRoomLeftReleasedPalette;
                blendedPalette = _gAkropolisSecurityRoomLeftReleasedClut;
                AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, work->angle, blendedPalette);
                litPalette     = _gAkropolisSecurityRoomRightLockedPalette;
                blendedPalette = _gAkropolisSecurityRoomRightLockedClut;
                AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, work->period, blendedPalette);
                litPalette     = _gAkropolisSecurityRoomRightReleasedPalette;
                blendedPalette = _gAkropolisSecurityRoomRightReleasedClut;
                AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE(colorIndex, litPalette, offPalette, work->step, blendedPalette);
                gpuUploadImages(_gAkropolisSecurityRoomLockIndicatorClutUploads);

                // Lamp quads are one-frame children; respawn all four after uploading their CLUTs.
                for (lampIndex = 0; lampIndex < ARRAY_SIZE(_gAkropolisSecurityRoomLockIndicatorQuads); lampIndex++) {
                    effectSpawn(EFFECT_AKROPOLIS_SECURITY_ROOM_LOCK_LAMP, coord, lampIndex, NULL);
                }
            } else if (((viewGetMappedIndex() & 0xFF) != AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_GLOW_SKIP_VIEW) && ((viewGetMappedIndex() & 0xFF) != AKROPOLIS_SECURITY_ROOM_LOCK_LAMPS_RIGHT_UNLOCK_VIEW)) {
                for (lampIndex = 0; lampIndex < ARRAY_SIZE(_gAkropolisSecurityRoomLockIndicatorGlowOffsets); lampIndex++) {
                    if ((work->index >> lampIndex) & 1) {
                        effectSpawn(EFFECT_AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_GLOW, coord, (s32)(_gAkropolisSecurityRoomLockIndicatorGlowColorMasks[lampIndex]),
                                    &_gAkropolisSecurityRoomLockIndicatorGlowOffsets[lampIndex]);
                    }
                }
            }
            break;
    }

    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) < AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED) {
        _akropolisSecurityRoomDrawSweepLine(task);
    }
}

#undef AKROPOLIS_SECURITY_ROOM_BLEND_LOCK_INDICATOR_PALETTE

/// Draws a subtractive horizontal segment travelling along the task coordinate's Y axis.
static void _akropolisSecurityRoomDrawSweepLine(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_SWEEP_VIEW_MASK = 0xC,
        AKROPOLIS_SECURITY_ROOM_SWEEP_START_X   = -1063,
        AKROPOLIS_SECURITY_ROOM_SWEEP_END_X     = -496,
        AKROPOLIS_SECURITY_ROOM_SWEEP_SPEED     = 6,
        AKROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL    = 406,
        AKROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y    = 0xF633,
        AKROPOLIS_SECURITY_ROOM_SWEEP_Z         = 2479,
        AKROPOLIS_SECURITY_ROOM_SWEEP_MIN_DEPTH = 17,
        AKROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY = 16,
    };
    _AkropolisSecurityRoomSweepLineScratch* line;
    GfxCoord*                               coord;
    LINE_F2*                                prim;

    coord = task->extra.coordBody->coord;
    if ((AKROPOLIS_SECURITY_ROOM_SWEEP_VIEW_MASK >> (gGameSession->location.view - 1)) & 1) {
        // Build the travelling segment in the task coordinate and transform it to the projection-input frame.
        line           = SCRATCH_STACK_RESERVE_BLOCK(_AkropolisSecurityRoomSweepLineScratch);
        line->start.vx = AKROPOLIS_SECURITY_ROOM_SWEEP_START_X;
        line->start.vy = (gDisplayState.animFrame * AKROPOLIS_SECURITY_ROOM_SWEEP_SPEED) % AKROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL + AKROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y;
        line->start.vz = AKROPOLIS_SECURITY_ROOM_SWEEP_Z;
        AKROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(line->start, coord);
        line->end.vx = AKROPOLIS_SECURITY_ROOM_SWEEP_END_X;
        line->end.vy = (gDisplayState.animFrame * AKROPOLIS_SECURITY_ROOM_SWEEP_SPEED) % AKROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL + AKROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y;
        line->end.vz = AKROPOLIS_SECURITY_ROOM_SWEEP_Z;
        AKROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(line->end, coord);
        // Project both ends; the second endpoint supplies the culling and sorting depth.
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&line->start);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        gte_stsxy(&prim->x0);
        gte_ldv0(&line->end);
        gte_rtps();
        setSemiTrans(prim, true);
        gte_stsxy(&prim->x1);
        gte_stszotz(&line->depth);
        if (line->depth >= AKROPOLIS_SECURITY_ROOM_SWEEP_MIN_DEPTH) {
            setRGB0(prim, AKROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY, AKROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY, AKROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(line->depth),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_SUBTRACT, line->depth);
        }
        SCRATCH_STACK_RELEASE_BLOCK(_AkropolisSecurityRoomSweepLineScratch);
    }
}

#undef AKROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT

void akropolisSecurityRoomLockLampTask(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_INDEX_MASK = 3,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE       = 128,
        AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SORT_DEPTH = 48,
    };
    EffectWork* work;
    GfxCoord*   coord;
    POLY_FT4*   quad;
    s16         edgeX;
    s16         edgeY;
    u16         centerXBits;
    u16         centerYBits;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    work->scale = task->spawnArg1.value & AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_INDEX_MASK;
    quad->tpage = getTPage(1, GPU_BLEND_ADD, 704, 0);
    setSemiTrans(quad, true);
    setShadeTex(quad, true);
    quad->clut = getClut(0, _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].clutY);
    // Preserve the placement coordinates as unsigned halfword bits before edge narrowing.
    centerXBits = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].centerX;
    centerYBits = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].centerY;
    quad->u0    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].u;
    quad->v0    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].v;
    quad->u1    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].u + AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE - 1;
    quad->v1    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].v;
    quad->u2    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].u;
    quad->v2    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].v + AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE - 1;
    quad->u3    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].u + AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE - 1;
    quad->v3    = _gAkropolisSecurityRoomLockIndicatorQuads[work->scale].v + AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE - 1;
    edgeX       = centerXBits - AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE / 2;
    quad->x2    = edgeX;
    quad->x0    = edgeX;
    edgeX       = centerXBits + (AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE / 2 - 1);
    quad->x3    = edgeX;
    quad->x1    = edgeX;
    edgeY       = centerYBits - AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE / 2;
    quad->y1    = edgeY;
    quad->y0    = edgeY;
    edgeY       = centerYBits + (AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SIZE / 2 - 1);
    quad->y3    = edgeY;
    quad->y2    = edgeY;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(AKROPOLIS_SECURITY_ROOM_LOCK_LAMP_SORT_DEPTH), quad);
    effectKillTask(work, task);
}

void akropolisSecurityRoomFallingQuadTask(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_QUAD_MOVING                  = 0,
        AKROPOLIS_SECURITY_ROOM_QUAD_SETTLED                 = 1,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_HALF_SIZE       = 32,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_MIN_DEPTH       = 17,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_SPEED_THRESHOLD = 29,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_STOP_Y          = -419,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_VIEW            = 15,
        AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL      = 7,
    };
    EffectQuadCornersScratch* quadScratch;
    GfxCoord*                 coord;
    EffectWork*               work;
    POLY_FT4*                 quad;
    s32                       cornerIndex;
    s32                       fallSpeed;
    s32                       driftX;
    s32                       driftZ;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    quadScratch = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    coord       = task->extra.coordBody->coord;
    work        = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    if (work->age == 0) {
        work->scale = AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_HALF_SIZE;
        _akropolisSecurityRoomSeedQuadMotion(work);
    }

    // Build the local XZ square, narrowing each transformed view-space corner to 16 bits.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(_gAkropolisSecurityRoomFallingQuadUnitCorners); cornerIndex++) {
        _akropolisSecurityRoomTransformQuadCorner(quadScratch, cornerIndex, work, coord);
    }

    // Projection consumes a quad even when the last vertex fails the near-depth test.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = 0;
    quad->u1 = AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->v1 = 0;
    quad->u2 = 0;
    quad->v2 = AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->u3 = AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->v3 = AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    if (quadScratch->depth >= AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_MIN_DEPTH) {
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 832, 0);
        quad->clut  = getClut(256, 270);
        setShadeTex(quad, true);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth), quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);

    // Draw the entry pose, then move for the next frame; settled quads keep their final pose.
    if (work->index == AKROPOLIS_SECURITY_ROOM_QUAD_MOVING) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        gfxRotMatrixX(&coord->coord, work->period, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&coord->coord, work->step, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;

        // Speed approaches 28/29; horizontal drift damps to zero, then reseeds.
        fallSpeed = work->move.vy;
        if (fallSpeed >= AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_SPEED_THRESHOLD) {
            fallSpeed--;
        } else {
            fallSpeed++;
        }
        work->move.vy = fallSpeed;

        driftX = work->move.vx;
        if (driftX == 0) {
            RANDOM_LCG_ADVANCE();
            work->move.vx += (2 - (u16)(RANDOM_LCG_DRAW() % 5U)) * 8;
        } else {
            if (driftX > 0) {
                driftX--;
            } else {
                driftX++;
            }
            work->move.vx = driftX;
        }

        driftZ = work->move.vz;
        if (driftZ == 0) {
            work->move.vz += work->step % 32;
            RANDOM_LCG_ADVANCE();
            work->move.vz += (2 - (u16)(RANDOM_LCG_DRAW() % 5U)) * 8;
        } else {
            if (driftZ > 0) {
                driftZ--;
            } else {
                driftZ++;
            }
            work->move.vz = driftZ;
        }

        RANDOM_LCG_ADVANCE();
        work->period += (1 - (u16)(RANDOM_LCG_DRAW() % 3U)) * 16;
        RANDOM_LCG_ADVANCE();
        work->step += (1 - (u16)(RANDOM_LCG_DRAW() % 3U)) * 8;
        if (coord->coord.t[1] >= AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_STOP_Y) {
            work->index = AKROPOLIS_SECURITY_ROOM_QUAD_SETTLED;
        }
    }

    work->age = work->age + 1;
    if (gGameSession->location.view != AKROPOLIS_SECURITY_ROOM_FALLING_QUAD_VIEW) {
        effectKillTask(work, task);
    }
}

void akropolisSecurityRoomLockLampGlowTask(Task* task)
{
    enum {
        AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_MIN_DEPTH            = 17,
        AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_EXTENT_DEPTH_PRODUCT = 3072,
        AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_MASK      = 0x70,
        AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_BASE      = 64,
    };
    GfxCoord*              coord;
    EffectWork*            work;
    RoomGlowSpriteScratch* scratch;
    POLY_G4*               quad;
    LINE_G3*               line;
    s16                    brightness;
    s16                    redFactor;
    s16                    greenFactor;
    s32                    armIndex;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    // Project the coordinate origin; both drawing phases share this center and depth.
    scratch              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    scratch->worldPos.vx = coord->workm.t[0];
    scratch->worldPos.vy = coord->workm.t[1];
    scratch->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPos);
    gte_rtps();
    gte_stsxy(&scratch->screenPos);
    gte_stszotz(&scratch->otz);
    if (scratch->otz >= AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_MIN_DEPTH) {
        redFactor   = (task->spawnArg1.value >> 1) & 1;
        greenFactor = task->spawnArg1.value & 1;
        RANDOM_LCG_ADVANCE();
        brightness          = (RANDOM_LCG_DRAW() & AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_MASK) + AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_BASE;
        scratch->halfExtent = AKROPOLIS_SECURITY_ROOM_MONITOR_GLOW_EXTENT_DEPTH_PRODUCT / scratch->otz;
        for (armIndex = 0; armIndex < 2; armIndex++) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _akropolisSecurityRoomInitGlowQuad(quad, brightness, redFactor, greenFactor);
            quad->x0 = scratch->screenPos.vx - scratch->halfExtent;
            quad->x1 = quad->x2 = scratch->screenPos.vx;
            quad->x3            = scratch->screenPos.vx + scratch->halfExtent;
            quad->y0 = quad->y2 = quad->y3 = scratch->screenPos.vy;
            quad->y1                       = (scratch->screenPos.vy - scratch->halfExtent) + scratch->halfExtent * (armIndex + armIndex);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
        for (armIndex = 0; armIndex < 2; armIndex++) {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            _akropolisSecurityRoomInitGlowLine(line, brightness, redFactor, greenFactor);
            line->x0 = scratch->screenPos.vx + scratch->halfExtent * (armIndex * 2 - 1);
            line->y0 = scratch->screenPos.vy - scratch->halfExtent * (armIndex + 1);
            line->x1 = scratch->screenPos.vx;
            line->y1 = scratch->screenPos.vy;
            line->x2 = scratch->screenPos.vx - scratch->halfExtent * (armIndex * 2 - 1);
            line->y2 = scratch->screenPos.vy + scratch->halfExtent * (armIndex + 1);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(work, task);
}

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

/// Sets a placed model's visibility from its saved two-bit object state.
static void _akropolisSecurityRoomUpdatePlacedObjectVisibility(Task* task)
{
    enum { AKROPOLIS_SECURITY_ROOM_PLACED_OBJECT_HIDDEN_STATE = 2 };
    const Enemy* placementWork;
    TmdObject*   model;
    s32          objectState;

    placementWork = task->spawnArg2.pointer;
    model         = task->extra.tmd;
    objectState   = areaGetCurrentObjectState((u8)placementWork->placeKey);
    // Retain the mapped-view query even though it does not select visibility.
    viewGetMappedIndex();
    if (objectState == AKROPOLIS_SECURITY_ROOM_PLACED_OBJECT_HIDDEN_STATE) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
    }
}
