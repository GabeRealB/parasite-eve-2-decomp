#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "factory_private.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_factory.h"

static void _factoryPanelPrompt(Task* task);

/// Selects the operator panel's private action-prompt hotspot test.
///
/// Bind a function identifier with signature
/// `s32(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)`; coordinates
/// are screen-centred pixels. The static prototype establishes local linkage
/// before the shared header. Keep the binding through both the panel-idle
/// caller and the hit-test definition. This alias captures no
/// arguments, evaluates nothing and uses neither stringification nor token pasting.
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
static s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#include "../../../../shared/room/action_prompt.h"
#include "../../../../shared/effects/glow_draw.h"

#include "../../../../shared/room/room_events.h"

#include "gameplay/direction_input.h"

static void _factoryPanelOpenPrompt(Task* task);
static void _factoryPanelRun(Task* task);
static void _factoryPanelArmPrompt(Task* task);
static void _factoryPanelIdle(Task* task);
static void _factoryPanelExit(Task* task);
static void _factoryPanelWaitMove(Task* task);

/// Operator-panel handlers in FACTORY_PANEL_STATE_ order.
///
/// The dispatcher copies all seven callbacks by value before selecting the
/// current task state. Every slot is populated; there is no terminator.
static const TaskFuncTable7 _gFactoryPanelStates = {
    {
        _factoryPanelInit,       // FACTORY_PANEL_STATE_INIT
        _factoryPanelArmPrompt,  // FACTORY_PANEL_STATE_ARM_PROMPT
        _factoryPanelIdle,       // FACTORY_PANEL_STATE_IDLE
        _factoryPanelOpenPrompt, // FACTORY_PANEL_STATE_OPEN_PROMPT
        _factoryPanelPrompt,     // FACTORY_PANEL_STATE_PROMPT
        _factoryPanelExit,       // FACTORY_PANEL_STATE_EXIT
        _factoryPanelWaitMove,   // FACTORY_PANEL_STATE_WAIT_MOVE
    },
};

static void _actionPromptResetDefault(Task* task);

static void _factoryPromptTask(Task* task);
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

/// Single bodyless priority-192 cursor task owned by an operator-panel session.
///
/// The panel passes spawnArg1 value 1 for controller port 0 and retains the child
/// task until exit; no table terminator or model resource is stored.
static TaskDesc _gFactoryPanelCursorDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _factoryPromptTask, { .value = 0 } };

/// This package's own panel task; room setup picks it or the other package's by stage.
TaskDesc FACTORY_OWN_PANEL_DESC = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _factoryPanelRun, { .value = 0 } };

/// Lift-completion notification followed by the task-message end marker.
///
/// The panel borrows this table while its work is live. Both payload words
/// are ignored. The handler has no defined result; senders discard it.
static TaskMessageEntry _gFactoryPanelMsgTable[2] = {
    { FACTORY_PANEL_MESSAGE_MOVE_SETTLED, _factoryPanelMarkMoveSettled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Mutable operator-panel hotspot rectangles in screen-centred pixels.
///
/// Raise, lower and turn open Push menus; the two caption spots open
/// Examine menus. Hit testing writes every entry's hit byte, and confirm
/// latches the first hit. The sixth entry ends the scan by its ID alone.
static ActionPromptHotspot _gFactoryPanelHotspots[6] = {
    { -68, -63, 16, 16, FACTORY_PANEL_CHOICE_RAISE, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { -27, -63, 16, 16, FACTORY_PANEL_CHOICE_LOWER, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { 13, -63, 16, 16, FACTORY_PANEL_CHOICE_TURN, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { 46, -80, 34, 32, FACTORY_PANEL_CHOICE_CAPTION6, FACTORY_PANEL_PROMPT_EXAMINE, 0 },
    { -38, 0, 72, 48, FACTORY_PANEL_CHOICE_CAPTION7, FACTORY_PANEL_PROMPT_EXAMINE, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Power-glow centre in signed whole world-coordinate units.
static SVECTOR _gFactoryPowerGlowPosition = { 395, -1630, 846, 0 };

/// Lamp-glow centre at lamp progress 1, in signed whole world-coordinate units.
static SVECTOR _gFactoryLampFirstGlowPosition = { 5910, -1308, 5649, 0 };

/// Alternate lamp-glow centre at lamp progress 2, in signed whole world-coordinate units.
static SVECTOR _gFactoryLampSecondGlowPosition = { 5910, -1404, 5649, 0 };

/// The room's own tables: cameras, collision, sprites, triggers, lights and
/// surfaces. They differ by day and by night in content and in order.
#if DRYFIELD_TIME == DRYFIELD_DAY
#include "factory_day_tables.inc.c"
#else
#include "factory_night_tables.inc.c"
#endif

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

TaskDesc* gFactoryPanelDesc = NULL;

TaskDesc* gFactorySpawnTable = NULL;

Task** gFactoryPanelSlot = NULL;

RoomEventReq gRoomEventReq = { 0 };

/// Scans the operator panel's hotspots and latches a confirmed command choice.
///
/// Requires initialized panel work, a live prompt and a hotspot table ending
/// in `ACTION_PROMPT_HOTSPOT_END`. Hides the HUD and holds event input. The
/// scan delay counts down even while a CAP is busy; either wait hides and
/// stops the cursor. Confirm selects the first hit and opens its command
/// prompt; cancel leaves the panel. Cursor coordinates are screen pixels.
static void _factoryPanelIdle(Task* task)
{
    enum { FACTORY_PANEL_EVENT_ACTIVE = 1 };

    ActionPrompt*        prompt  = gActionPromptSlots;
    ActionPromptHotspot* hotspot = _gFactoryPanelHotspots;
    FactoryPanelWork*    work    = task->work;

    // Hold event input while captions and the post-choice delay settle.
    gGameSession->hideHud    = true;
    gGameSession->eventState = FACTORY_PANEL_EVENT_ACTIVE;
    if (work->scanDelay != 0) {
        work->scanDelay = work->scanDelay - 1;
    }
    if ((capIsBusy() != 0) || (work->scanDelay != 0)) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (ACTION_PROMPT_HIT_TEST(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = FACTORY_PANEL_STATE_OPEN_PROMPT;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = FACTORY_PANEL_STATE_EXIT;
    }
}

#include "../../../../shared/room/action_prompt_outline_rect.inc.c"

/// Plays the operator-panel button cue from the active day/night factory bank.
///
/// Uses centred pan/depth. Requires initialized session and sound state.
static inline void _factoryPanelPlayButtonSound(void)
{
    enum {
        FACTORY_PANEL_DAY_SOUND_AREA   = FACTORY_DAY_SOUND(0),
        FACTORY_PANEL_NIGHT_SOUND_AREA = FACTORY_NIGHT_SOUND(0),
    };
    s32 soundArea;

    soundArea = FACTORY_PANEL_NIGHT_SOUND_AREA;
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        soundArea = FACTORY_PANEL_DAY_SOUND_AREA;
    }
    sndEvtRequestScriptStart(soundArea | FACTORY_SOUND_PANEL_BUTTON, 0, 0);
}

/// Applies the confirmed operator-panel choice and selects the next panel state.
///
/// Choices 0/1/2 request raise/lower/turn only when power is on; an unavailable
/// move starts its caption instead. Choices 3/4 are caption-only. A real move
/// selects saved view 18 or 19 and waits for the lift; other paths return to idle.
/// Unsupported powered choices leave the state alone; unsupported unpowered
/// choices still return to idle. Requires a live panel task and initialized
/// session, save, sound and CAP state. Does not retain any new resource.
static void _factoryPanelApplyChoice(Task* task, s16 choice)
{
    s32 nextState;

    // Power gates lift movement; captions remain available without it.
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0) {
        switch (choice) {
            case FACTORY_PANEL_CHOICE_RAISE:
                _factoryPanelPlayButtonSound();
                if (!(gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & FACTORY_LIFT_POSITION_RAISED)) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_RAISED);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                    }
                    nextState = FACTORY_PANEL_STATE_WAIT_MOVE;
                } else {
                    capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_RAISE, 0, 0);
                    nextState = FACTORY_PANEL_STATE_IDLE;
                }
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_LOWER:
                _factoryPanelPlayButtonSound();
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & FACTORY_LIFT_POSITION_RAISED) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & ~FACTORY_LIFT_POSITION_RAISED);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                    }
                    nextState = FACTORY_PANEL_STATE_WAIT_MOVE;
                } else {
                    capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_LOWER, 0, 0);
                    nextState = FACTORY_PANEL_STATE_IDLE;
                }
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_TURN:
                _factoryPanelPlayButtonSound();
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) ^ FACTORY_LIFT_POSITION_TURNED);
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                } else {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                }
                nextState   = FACTORY_PANEL_STATE_WAIT_MOVE;
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_CAPTION6:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION6, 0, 1);
                nextState   = FACTORY_PANEL_STATE_IDLE;
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_CAPTION7:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION7, 0, 0);
                nextState   = FACTORY_PANEL_STATE_IDLE;
                task->state = nextState;
                break;
        }
    } else {
        switch (choice) {
            case FACTORY_PANEL_CHOICE_RAISE:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_RAISE, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_LOWER:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_LOWER, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_TURN:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_TURN, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_CAPTION6:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION6, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_CAPTION7:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION7, 0, 0);
                break;
        }
        task->state = FACTORY_PANEL_STATE_IDLE;
    }
}

#include "../../../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../../../shared/room/action_prompt_draw_cursor.inc.c"

void FACTORY_ROOM_INSTANCE_SET_VIEW9_SPRITE_VISIBLE(s32 visible)
{
    enum {
        FACTORY_ROOM_VISIBILITY_BYTE_MASK = 0xFF,
    };
    const GameSession*     session;
    const GameLocationKey* location;
    SpriteBatch*           view9Batches;

    session  = gGameSession;
    location = &session->location;
    // The separate power sprite belongs to the daytime background in both instances.
    if (location->stage == GAME_STAGE_DRYFIELD) {
        view9Batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][FACTORY_POWER_SPRITE_VIEW - 1].batches;
        if (!(visible & FACTORY_ROOM_VISIBILITY_BYTE_MASK)) {
            view9Batches[FACTORY_POWER_SPRITE_BATCH].hidden = true;
            return;
        }
        view9Batches[FACTORY_POWER_SPRITE_BATCH].hidden = false;
    }
}

/// Dispatches the operator panel's current interaction state.
///
/// Requires a live task with state in 0..FACTORY_PANEL_STATE_COUNT-1. Copies
/// the seven callbacks onto the stack and indexes without a bounds check.
/// Initialization owns the panel work; later handlers require that work and
/// may close the session and destroy the task.
static void _factoryPanelRun(Task* task)
{
    TaskFuncTable7 handlers;

    handlers = _gFactoryPanelStates;
    handlers.funcs[task->state](task);
}

/// Runs the operator panel's default action cursors.
///
/// Requires state 0 for resetting both prompt slots or 1 for their per-frame
/// movement and drawing. Reset advances the task to state 1. The panel owns
/// this child task and kills it during exit; the polling owner releases work.
/// Movement selects port 0 for spawnArg1.value 1, port 1 for 2, and both for
/// other values; the panel spawns it with 1. No task work is allocated here.
static void _factoryPromptTask(Task* task)
{
    TaskFunc states[] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

/// Records the lift's completed movement for the operator panel to consume.
///
/// Handles `FACTORY_PANEL_MESSAGE_MOVE_SETTLED` for a live panel work block.
/// The sender supplies zero payloads and discards the result; this callback
/// defines no return word. All argument words except the receiver are ignored.
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    FactoryPanelWork* work = task->work;

    work->moveSettled = 1;
}

#include "../../../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST

/// Opens an operator-panel session with a cursor and a power-dependent saved view.
///
/// Requires the room's live hotspot and cursor descriptor tables, initialized
/// session/save state and entry at FACTORY_PANEL_STATE_INIT. Owns zeroed panel
/// work and retains the cursor in spawnArg2 for teardown; cursor spawning must
/// succeed. Acquires the menu hold and hides ordinary play until panel exit.
/// Allocation failure kills the panel without acquiring the hold.
static void _factoryPanelInit(Task* task)
{
    enum {
        FACTORY_PANEL_CURSOR_PORT_0 = 1,
        FACTORY_PANEL_EVENT_ACTIVE  = 1
    };
    FactoryPanelWork*    work;
    ActionPromptHotspot* hotspot;

    work = memCalloc(sizeof(FactoryPanelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&_gFactoryPanelCursorDesc, 0, FACTORY_PANEL_CURSOR_PORT_0, 0);
    task->work              = work;
    task->msgTable          = _gFactoryPanelMsgTable;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_VIEW_UNPOWERED;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_VIEW_POWERED;
    }
    task->state++;
    displayAcquireMenuHold();
    // Discard the previous scan before the new cursor can confirm a hotspot.
    for (hotspot = _gFactoryPanelHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    gGameSession->cutsceneHold = true;
    gGameSession->hideHud      = true;
    gGameSession->eventState   = FACTORY_PANEL_EVENT_ACTIVE;
    work->scanDelay            = 0;
}

/// Starts the operator panel's hotspot scan with a centered idle cursor.
///
/// Requires the live action prompt spawned during panel initialization.
/// Enables aiming and advances from ARM_PROMPT to IDLE without changing work.
static void _factoryPanelArmPrompt(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Opens the confirmed operator-panel hotspot's command prompt.
///
/// Requires initialized panel work and a live action cursor with a latched
/// Examine/Push choice. Hides and stops that cursor, opens the commands at its
/// screen-pixel position, and advances to the prompt state.
static void _factoryPanelOpenPrompt(Task* task)
{
    ActionPrompt*     prompt = gActionPromptSlots;
    FactoryPanelWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = FACTORY_PANEL_STATE_PROMPT;
}

/// Applies an accepted operator-panel choice or returns to hotspot scanning.
///
/// Requires initialized panel work with a latched hotspot choice and a completed
/// command prompt. Hides/stops the cursor before testing acceptance; accepted
/// choices select dialogue or lift motion, while cancellation returns to IDLE.
/// Both paths arm the ten-frame scan delay. Works in both factory variants.
static void _factoryPanelPrompt(Task* task)
{
    FactoryPanelWork* work = task->work;

    gActionPromptSlots[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    gActionPromptSlots[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        _factoryPanelApplyChoice(task, work->choice);
    } else {
        task->state = FACTORY_PANEL_STATE_IDLE;
    }
    work->scanDelay = FACTORY_PANEL_SCAN_DELAY_FRAMES;
}

/// Restores player and companion presentation after an operator-panel session.
///
/// Requires live actor models, session/save state and the panel's menu hold.
/// Defers another manual interaction for ten eligible direction updates and
/// selects the factory's return view before the cursor is torn down.
static inline void _factoryPanelRestorePlay(void)
{
    enum {
        FACTORY_PANEL_EVENT_IDLE = 0,
    };

    gDirectionInteractionRearmUpdates = FACTORY_PANEL_REENTRY_DELAY_UPDATES;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                               = FACTORY_PANEL_EVENT_IDLE;
    gGameSession->hideHud                                  = false;
    gGameSession->cutsceneHold                             = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_VIEW_RETURN;
}

/// Ends the operator-panel session and hands its teardown back to its owner.
///
/// Requires the menu hold and live cursor task retained in
/// `task->spawnArg2.pointer`. Restores ordinary play and saved view 3, kills
/// the cursor immediately, then requests panel teardown with result zero.
/// The polling owner remains responsible for releasing the panel's work.
static void _factoryPanelExit(Task* task)
{
    enum { FACTORY_PANEL_END_RESULT = 0 };

    _factoryPanelRestorePlay();

    // End the cursor before the owner polls and releases this panel.
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, FACTORY_PANEL_END_RESULT);
}

/// Resumes the operator panel's scan after the lift reports a settled move.
///
/// Requires initialized panel work and a live prompt. Keeps the cursor hidden
/// and stopped until `moveSettled` is nonzero, then consumes that notification,
/// selects the powered or unpowered panel view and starts a ten-frame scan
/// delay. The panel owns neither the lift nor the notification sender.
static void _factoryPanelWaitMove(Task* task)
{
    ActionPrompt*     prompt;
    FactoryPanelWork* work;

    prompt              = gActionPromptSlots;
    work                = task->work;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    if (work->moveSettled != 0) {
        if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_VIEW_UNPOWERED;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = FACTORY_PANEL_VIEW_POWERED;
        }
        work->scanDelay   = FACTORY_PANEL_SCAN_DELAY_FRAMES;
        work->moveSettled = 0;
        task->state       = FACTORY_PANEL_STATE_IDLE;
    }
}

void FACTORY_ROOM_INSTANCE_SET_VIEW11_SPRITE_VISIBLE(s32 visible)
{
    enum {
        FACTORY_ROOM_VISIBILITY_BYTE_MASK = 0xFF,
    };
    const GameSession*     session;
    const GameLocationKey* location;
    SpriteBatch*           view11Batches;

    session  = gGameSession;
    location = &session->location;
    // Only the daytime background has this sprite, including in the night instance.
    if (location->stage == GAME_STAGE_DRYFIELD) {
        view11Batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][FACTORY_POWER_SCENE_SPRITE_VIEW - 1].batches;
        if (!(visible & FACTORY_ROOM_VISIBILITY_BYTE_MASK)) {
            view11Batches[FACTORY_POWER_SCENE_SPRITE_BATCH].hidden = true;
            return;
        }
        view11Batches[FACTORY_POWER_SCENE_SPRITE_BATCH].hidden = false;
    }
}

#include "../../../../shared/room/action_prompt_reset.inc.c"

#include "../../../../shared/effects/glow_draw_tinted_disc.inc.c"

void FACTORY_DRAW_GLOWS_TASK(Task* task)
{
    s32 viewBit;

    viewBit = 1 << gGameSession->location.view;
#if DRYFIELD_TIME == DRYFIELD_NIGHT
    // Preserve the night task's coordinate refresh even though the glows use world points.
    actorRenderComposeCoord(task->extra.coordBody->coord);
#endif
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0 && (viewBit & FACTORY_GLOW_POWER_VIEW_MASK) != 0) {
        _glowDrawTintedDisc(&_gFactoryPowerGlowPosition, FACTORY_GLOW_POWER_RADIUS_SCALE, FACTORY_GLOW_POWER_COLOR);
    }
    // Lamp progress selects one position independently of the power flag.
    if (viewBit & FACTORY_GLOW_LAMP_VIEW_MASK) {
        if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == FACTORY_LAMP_PROGRESS_FIRST) {
            _glowDrawTintedDisc(&_gFactoryLampFirstGlowPosition, FACTORY_GLOW_LAMP_RADIUS_SCALE, FACTORY_GLOW_LAMP_FIRST_COLOR);
        } else if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == FACTORY_LAMP_PROGRESS_SECOND) {
            _glowDrawTintedDisc(&_gFactoryLampSecondGlowPosition, FACTORY_GLOW_LAMP_RADIUS_SCALE, FACTORY_GLOW_LAMP_SECOND_COLOR);
        }
    }
}
