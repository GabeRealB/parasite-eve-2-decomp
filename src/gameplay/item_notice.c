#include "gameplay/item_menu.h"

#include <psyq/libgte.h>

#include "common.h"

#include "area_flags.h"
#include "cap.h"
#include "item_menu.h"
#include "items.h"
#include "scene_runtime.h"

#include "main/display.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"
#include "main/wipsys.h"

UiObjectDesc D_8010D348 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -64, -32, 128, 64 }, 60, 0, TASK_BODY_NONE, 192, func_800B92CC, 0 };

/// Captures the player model root's local XYZ and signed-turn yaw for saving.
///
/// Requires a live player TMD root in the saved room frame. XYZ narrow to s16;
/// yaw uses 4096 units per turn and retains both half-turn endpoints.
static inline void _playerCaptureRootPose(void)
{
    const GfxCoord* playerRoot;
    PlayerPos*      savedPose;
    s32             storedX;
    s32             yaw;

    playerRoot     = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    storedX        = (u16)playerRoot->coord.t[0];
    savedPose      = &gPlayerStatus.pos;
    savedPose->x   = storedX;
    savedPose->y   = playerRoot->coord.t[1];
    savedPose->z   = playerRoot->coord.t[2];
    yaw            = ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]);
    savedPose->yaw = yaw;
    if ((s16)yaw >= PLAYER_YAW_HALF_TURN + 1) {
        savedPose->yaw = yaw - PLAYER_YAW_FULL_TURN;
    } else if ((s16)yaw < -PLAYER_YAW_HALF_TURN) {
        savedPose->yaw = yaw + PLAYER_YAW_FULL_TURN;
    }
}

void itemPickupActionPromptTask(Task* task)
{
    enum {
        ITEM_PICKUP_ACTION_INITIAL             = 0,
        ITEM_PICKUP_ACTION_BEGIN_CLOSE         = 16,
        ITEM_PICKUP_ACTION_WAIT_CLOSE          = 17,
        ITEM_PICKUP_ACTION_OPEN_DELAY_TICKS    = 1,
        ITEM_PICKUP_ACTION_CLOSE_DELAY_UPDATES = 12,
        ITEM_PICKUP_ACTION_COLLECTED_STATE     = 2,
        ITEM_PICKUP_ACTION_REPLENISHABLE_STATE = 3,
        ITEM_PICKUP_ACTION_STATE_WORD_SHIFT    = 4,
        ITEM_PICKUP_ACTION_STATE_INDEX_MASK    = 0xF,
        ITEM_PICKUP_ACTION_STATE_BITS          = 2,
        ITEM_PICKUP_ACTION_STATE_MASK          = 3
    };

    CapActionRequest*   request;
    const UiObjectDesc* descriptor;
    UiObject*           rootObject;
    UiObject*           spawnedRoot;
    Task*               rootTask;
    PlayerStatus*       player;
    McSaveData*         save;
    s32                 flagIndex;
    s32                 bitShift;
    u32                 stateMask;
    u32*                savedStateWord;
    u32*                sessionStateWord;

    request = task->spawnArg2.pointer;
    if (task->state == ITEM_PICKUP_ACTION_INITIAL) {
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        if (itemPickupPublishPlacedObject(request->actionId) == 0) {
            request->accepted = 0;
            request->done     = 1;
            taskKill(task);
            return;
        }
        switch (Gp_PubItemLoc >> 8) {
            case ITEM_PICKUP_PLACE_BANK_ITEM:
            case ITEM_PICKUP_PLACE_BANK_KEY_ITEM:
                descriptor = &D_8010EAB4[ITEM_PICKUP_PANEL_DESCRIPTOR];
                break;
            case ITEM_PICKUP_PLACE_BANK_SAVE_POINT:
                // Capture the root transform before presenting the save prompt.
                _playerCaptureRootPose();
                gDisplayState.gameMode = DISPLAY_GAME_MODAL;
                player                 = &gPlayerStatus;
                save                   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                save->state.playerExp  = player->exp;
                save->state.playerBp   = player->bp;
                save->state.savePoint  = Gp_PubItemLoc;
                stageEnsureHeapTaskPrimitiveBuffer();
                descriptor = &D_8010D348;
                break;
            default:
                stageEnsureHeapTaskPrimitiveBuffer();
                descriptor = &D_8010D6D8;
                break;
        }
        request->accepted = 0;
        if (D_80114DDE & AREA_OBJECT_PLACE_PROMPT) {
            request->defaultPrompt = 0;
        } else {
            request->defaultPrompt = 1;
        }
        spawnedRoot = uiSpawnObject(descriptor, (s32)((s8)(request->defaultPrompt ^ 1)), USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_ACTION_OPEN_DELAY_TICKS, NULL);
        if (spawnedRoot != NULL) {
            task->firstChild = spawnedRoot->owner;
            task->state++;
        }
    }
    if (task->state < ITEM_PICKUP_ACTION_BEGIN_CLOSE) {
        rootTask = task->firstChild;
        if (rootTask != NULL) {
            rootObject = rootTask->spawnArg2.pointer;
            if (rootObject->result == USER_INTERFACE_RESULT_CANCEL || rootObject->result == USER_INTERFACE_RESULT_CONFIRM) {
                switch (Gp_PubItemLoc >> 8) {
                    case ITEM_PICKUP_PLACE_BANK_ITEM:
                    case ITEM_PICKUP_PLACE_BANK_KEY_ITEM:
                        if (rootObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                            // Session state gates the write to the saved stage bank.
                            flagIndex        = request->actionId;
                            sessionStateWord = Gp_Bit2Banks[gGameSession->location.loc.stage].objectStates + (flagIndex >> ITEM_PICKUP_ACTION_STATE_WORD_SHIFT);
                            bitShift         = (flagIndex & ITEM_PICKUP_ACTION_STATE_INDEX_MASK) * ITEM_PICKUP_ACTION_STATE_BITS;
                            stateMask        = ITEM_PICKUP_ACTION_STATE_MASK << bitShift;
                            if (((*sessionStateWord & stateMask) >> bitShift) != ITEM_PICKUP_ACTION_REPLENISHABLE_STATE) {
                                savedStateWord  = Gp_Bit2Banks[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage].objectStates + (flagIndex >> ITEM_PICKUP_ACTION_STATE_WORD_SHIFT);
                                *savedStateWord = (*savedStateWord & ~stateMask) | (ITEM_PICKUP_ACTION_COLLECTED_STATE << bitShift);
                            }
                            request->accepted = 1;
                        } else {
                            request->accepted = 0;
                        }
                        break;
                    case ITEM_PICKUP_PLACE_BANK_SAVE_POINT:
                        if (rootObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                            request->accepted = 1;
                        } else {
                            request->accepted = 0;
                        }
                        break;
                    default:
                        request->accepted = 0;
                        break;
                }
                uiStartTreeClosing(rootObject, rootObject->owner);
                task->state = ITEM_PICKUP_ACTION_BEGIN_CLOSE;
            }
        }
    }
    // Keep the borrowed request pending until the UI has had time to close.
    if (task->state == ITEM_PICKUP_ACTION_BEGIN_CLOSE) {
        task->killCountdown = ITEM_PICKUP_ACTION_CLOSE_DELAY_UPDATES;
        task->state++;
    } else if (task->state == ITEM_PICKUP_ACTION_WAIT_CLOSE) {
        if (--task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            request->done          = 1;
            gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
            stageReleaseTaskPrimitiveBuffer();
            taskKill(task);
        }
    }
}
