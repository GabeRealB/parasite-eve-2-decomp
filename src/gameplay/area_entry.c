#include "gameplay/area_entry.h"
#include "rooms/mist_shooting_gallery.h"

#include "types.h"

#include "area_entry.h"
#include "ending.h"
#include "hud.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "gameplay/attachment_state.h"
#include "gameplay/items.h"
#include "items.h"
#include "scene_runtime.h"
#include "world_targets.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"

Task* gPlayerActorTasks[PLAYER_ACTOR_TASK_COUNT];

/// Unreferenced halfword table following the item-grant scan.
static u16 D_8010CA30[];

/// Area transition panel and item-title panel, selected by the spawn index.
extern UiObjectDesc D_8010CA40[];

extern UiObjectDesc D_8010CA78[];

static void _sceneBattleResultTask(Task* task);

u8                 Gp_StrItemObtained[] = "Item obtained!";
u8                 Gp_StrBonusItem[]    = "Bonus item!!";
s32                Gp_ItemGrantCooldown = 0;
InventoryItemRange D_8010CA2C           = { 0, 5, INVENTORY_ITEM_TABLE_AREA_GRANTS, 0 };

/// Unreferenced halfword table following the item-grant scan.
static u16 D_8010CA30[] = { 0x3A, 0x2E, 0x3B, 0x31, 0x41, 0x34, 0, 0 };

/// Area transition panel and item-title panel, selected by the spawn index.
UiObjectDesc D_8010CA40[] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -48, 160, 64 }, 32, 0, TASK_BODY_NONE, 0xC0, itemMenuBattleResultTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -96, 256, 192 }, 36, 0, TASK_BODY_NONE, 0xC0, itemPickupTitleTask, 0 },
};
UiObjectDesc D_8010CA78[] = {
    { 3, { -80, 16, 160, 20 }, 28, 0, TASK_BODY_NONE, 0xC0, itemPickupNoticeTask, 0 },
    { 3, { 16, 24, 160, 20 }, 24, 0, TASK_BODY_NONE, 0xC0, itemPickupNoticeTask, 0 },
};
TaskDesc D_8010CAB0 = { { { TASK_BODY_NONE, 0xC0 } }, sceneBattleStartTransitionTask };
TaskDesc D_8010CABC = { { { TASK_BODY_NONE, 0xC0 } }, _sceneBattleResultTask };

/// Presents battle gains or an escape result, restores area music and exits the transition.
///
/// spawnArg1 == 0 selects victory; other values select escape. On initialization
/// spawnArg2 borrows the HUD for victory outside the shooting gallery; escape
/// needs no HUD. The argument is then replaced with the live result panel.
/// Owns the stage primitive reservation until every result/item panel has closed.
/// Requires loaded result UI, location/reward data and player actor slots; panel
/// allocation is assumed to succeed. Waits for music and CD work before exit.
static void _sceneBattleResultTask(Task* task)
{
    enum {
        SCENE_BATTLE_RESULT_INITIALIZE              = 0,
        SCENE_BATTLE_RESULT_LOAD_MUSIC              = 1,
        SCENE_BATTLE_RESULT_WAIT_PANEL              = 2,
        SCENE_BATTLE_RESULT_WAIT_ITEMS              = 3,
        SCENE_BATTLE_RESULT_CLOSE_DELAY             = 16,
        SCENE_BATTLE_RESULT_FINISH                  = 17,
        SCENE_BATTLE_RESULT_WON                     = 0,
        SCENE_BATTLE_RESULT_COUNTER_MAX             = 9999,
        SCENE_BATTLE_RESULT_CLOSE_FRAMES            = 10,
        SCENE_BATTLE_RESULT_ITEM_TRANSFER_MODE      = 1,
        SCENE_BATTLE_RESULT_ITEM_PLACE_KIND         = 0x700,
        SCENE_BATTLE_RESULT_HEALING_SOUND_FILE_BASE = 21,
        SCENE_BATTLE_RESULT_MUTED_SOUND             = SOUND_COMMON(0x0D),
        SCENE_BATTLE_RESULT_ITEM_NOTICE             = 1,
        SCENE_BATTLE_RESULT_BONUS_NOTICE            = 2,
        SCENE_BATTLE_RESULT_ITEM_NOTICE_DELAY       = 17,
        SCENE_BATTLE_RESULT_BONUS_NOTICE_DELAY      = 33,
        SCENE_BATTLE_RESULT_GALLERY_OPEN_DELAY      = 4,
        STAGE_MUSIC_REQUEST_AREA_ENTRY              = 1,
        STAGE_MUSIC_REQUEST_LOAD_ONLY               = 3,
        STAGE_MUSIC_LOAD_IDLE                       = 0xFF
    };
    u32                 stageAreaKey;
    HudState*           hud;
    s32                 actorSlotIndex;
    Task*               actorTask;
    GameSession*        session;
    InventoryItemRange* rewardRange;
    GameActor*          actor;

    // Detach obsolete targeting nodes before creating the result panels.
    if (task->state == SCENE_BATTLE_RESULT_INITIALIZE) {
        hud           = task->spawnArg2.pointer;
        stageAreaKey  = GAME_LOCATION_WORD(gGameSession->location.loc);
        stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
        stageEnsureHeapTaskPrimitiveBuffer();
        for (actorSlotIndex = 0; actorSlotIndex < PLAYER_ACTOR_TASK_COUNT; actorSlotIndex++) {
            actorTask = gPlayerActorTasks[actorSlotIndex];
            if (actorTask != NULL) {
                actor             = actorTask->work;
                actor->targetNode = NULL;
            }
        }
        sndEvtRequestScriptMute(SCENE_BATTLE_RESULT_MUTED_SOUND);
        sndLoadEnqueuePeFile((attachmentGetEffectiveLevel(ATTACHMENT_INDEX_HEALING) + SCENE_BATTLE_RESULT_HEALING_SOUND_FILE_BASE) & 0xFF);
        if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
            task->spawnArg2.pointer = uiSpawnObject(&D_mist_shooting_gallery_80185000, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, SCENE_BATTLE_RESULT_GALLERY_OPEN_DELAY, NULL);
        } else {
            task->spawnArg2.pointer = uiSpawnObject(D_8010CA40, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
            if (task->spawnArg1.value == SCENE_BATTLE_RESULT_WON) {
                // The battle is over: return the HUD to its out-of-battle state.
                hud->battleStep = HUD_BATTLE_STEP_START;
                hud->inBattle   = 0;
                areaSetSavedPoseRestoreEnabled(1, &gGameSession->location.loc);
                gGameSession->battleResetPending = 1;
                if (!((stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 0, 0) ||
                       stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 0)) &&
                      gGameSession->location.loc.variant - 1 < 3U)) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon < (u32)SCENE_BATTLE_RESULT_COUNTER_MAX) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon++;
                    }
                }
                rewardRange = &D_8010CA2C;
                inventoryClearItems(rewardRange);
                task->status = inventoryGrantBattleRewards(rewardRange);
                if (task->status != INVENTORY_BATTLE_REWARD_NONE_GRANTED) {
                    uiSpawnObject(D_8010CA78, SCENE_BATTLE_RESULT_ITEM_NOTICE, USER_INTERFACE_PANEL_INACTIVE, SCENE_BATTLE_RESULT_ITEM_NOTICE_DELAY, task->spawnArg2.pointer);
                    if (task->status == INVENTORY_BATTLE_REWARD_BONUS_GRANTED) {
                        uiSpawnObject(D_8010CA78 + 1, SCENE_BATTLE_RESULT_BONUS_NOTICE, USER_INTERFACE_PANEL_INACTIVE, SCENE_BATTLE_RESULT_BONUS_NOTICE_DELAY, task->spawnArg2.pointer);
                    }
                }
            } else {
                task->status = INVENTORY_BATTLE_REWARD_NONE_GRANTED;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped < (u32)SCENE_BATTLE_RESULT_COUNTER_MAX) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped++;
                }
            }
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        task->state++;
    } else if (task->state == SCENE_BATTLE_RESULT_LOAD_MUSIC) {
        session = gGameSession;
        if (!(session->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
            session->viewReady             = 1;
            gStageMusicParams.fadeOutTicks = 0;
            gStageMusicParams.field_2      = 0;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY)) {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, STAGE_MUSIC_REQUEST_AREA_ENTRY, 0);
            } else {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, STAGE_MUSIC_REQUEST_LOAD_ONLY, 0);
            }
        } else {
            gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
        }
        task->state++;
    } else if (task->state == SCENE_BATTLE_RESULT_WAIT_PANEL) {
        UiObject* resultPanel;

        resultPanel = task->spawnArg2.pointer;
        if (gStageMusicLoadState == STAGE_MUSIC_LOAD_IDLE) {
            if (cdCmdIsIdle() & 0xFFFF) {
                if (resultPanel->result == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(resultPanel, resultPanel->owner);
                    if (task->status != INVENTORY_BATTLE_REWARD_NONE_GRANTED) {
                        Gp_PubItemLoc           = SCENE_BATTLE_RESULT_ITEM_PLACE_KIND;
                        task->spawnArg2.pointer = uiSpawnObject(&D_8010D6D8, SCENE_BATTLE_RESULT_ITEM_TRANSFER_MODE, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                        task->state++;
                    } else {
                        task->killCountdown = SCENE_BATTLE_RESULT_CLOSE_FRAMES;
                        task->state         = SCENE_BATTLE_RESULT_CLOSE_DELAY;
                    }
                }
            }
        }
    } else if (task->state == SCENE_BATTLE_RESULT_WAIT_ITEMS) {
        UiObject* resultPanel;

        resultPanel = task->spawnArg2.pointer;
        if ((resultPanel->result == USER_INTERFACE_RESULT_CONFIRM) || (resultPanel->result == USER_INTERFACE_RESULT_CANCEL)) {
            uiStartTreeClosing(resultPanel, resultPanel->owner);
            task->killCountdown = SCENE_BATTLE_RESULT_CLOSE_FRAMES;
            task->state         = SCENE_BATTLE_RESULT_CLOSE_DELAY;
        }
    } else if (task->state == SCENE_BATTLE_RESULT_CLOSE_DELAY) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            task->state = SCENE_BATTLE_RESULT_FINISH;
        }
    }

    // Keep the primitive buffer alive through panel closing and pending CD work.
    if (task->state >= SCENE_BATTLE_RESULT_FINISH) {
        if (gStageMusicLoadState == STAGE_MUSIC_LOAD_IDLE) {
            if (cdCmdIsIdle() & 0xFFFF) {
                displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
                sndEvtRequestScriptUnmute(SCENE_BATTLE_RESULT_MUTED_SOUND);
                taskKill(task);
                stageReleaseTaskPrimitiveBuffer();
                stageRequestModeTaskExit();
            }
        }
    }
}
