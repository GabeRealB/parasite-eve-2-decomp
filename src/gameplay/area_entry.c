#include "gameplay/area_entry.h"
#include "rooms/mist_shooting_gallery.h"

#include "types.h"

#include "area_entry.h"
#include "ending.h"
#include "hud.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "scene_runtime.h"
#include "world_targets.h"

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

void Gp_AreaEnterTask(Task* arg0);

u8                 Gp_StrItemObtained[] = "Item obtained!";
u8                 Gp_StrBonusItem[]    = "Bonus item!!";
s32                Gp_ItemGrantCooldown = 0;
InventoryItemRange D_8010CA2C           = { 0, 5, INVENTORY_ITEM_TABLE_AREA_GRANTS, 0 };

/// Unreferenced halfword table following the item-grant scan.
static u16 D_8010CA30[] = { 0x3A, 0x2E, 0x3B, 0x31, 0x41, 0x34, 0, 0 };

/// Area transition panel and item-title panel, selected by the spawn index.
UiObjectDesc D_8010CA40[] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -48, 160, 64 }, 32, 0, TASK_BODY_NONE, 0xC0, func_800A087C, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -96, 256, 192 }, 36, 0, TASK_BODY_NONE, 0xC0, itemPickupTitleTask, 0 },
};
UiObjectDesc D_8010CA78[] = {
    { 3, { -80, 16, 160, 20 }, 28, 0, TASK_BODY_NONE, 0xC0, itemPickupNoticeTask, 0 },
    { 3, { 16, 24, 160, 20 }, 24, 0, TASK_BODY_NONE, 0xC0, itemPickupNoticeTask, 0 },
};
TaskDesc D_8010CAB0 = { { { TASK_BODY_NONE, 0xC0 } }, Gp_EndingTask };
TaskDesc D_8010CABC = { { { TASK_BODY_NONE, 0xC0 } }, Gp_AreaEnterTask };

void Gp_AreaEnterTask(Task* arg0)
{
    u32                 stageAreaKey;
    HudState*           hud;
    s32                 i;
    Task*               slot;
    GameSession*        session;
    InventoryItemRange* scan;

    if (arg0->state == 0) {
        hud           = arg0->spawnArg2.pointer;
        stageAreaKey  = GAME_LOCATION_WORD(gGameSession->location.loc);
        stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
        Stage_InitPrimBufOnce();
        for (i = 0; i < PLAYER_ACTOR_TASK_COUNT; i++) {
            slot = gPlayerActorTasks[i];
            if (slot != NULL) {
                ((GameActor*)slot->work)->targetNode = NULL;
            }
        }
        SndEvt_EnqueueType8(SOUND_COMMON(0x0D));
        sndLoadEnqueuePeFile((attachmentGetEffectiveLevel(7) + 0x15) & 0xFF);
        if (stageAreaKey == GAME_LOCATION_KEY(1, 20, 0, 0)) {
            arg0->spawnArg2.pointer = uiSpawnObject(&D_mist_shooting_gallery_80185000, arg0->spawnArg1, 1, 4, NULL);
        } else {
            arg0->spawnArg2.pointer = uiSpawnObject(D_8010CA40, arg0->spawnArg1, 1, 1, NULL);
            if (arg0->spawnArg1.value == 0) {
                // The battle is over: return the HUD to its out-of-battle state.
                hud->battleStep = HUD_BATTLE_STEP_START;
                hud->inBattle   = 0;
                Gp_SetAreaFlag2(1, &gGameSession->location.loc);
                gGameSession->battleResetPending = 1;
                if (!((stageAreaKey == GAME_LOCATION_KEY(5, 11, 0, 0) || stageAreaKey == GAME_LOCATION_KEY(5, 29, 0, 0)) &&
                      gGameSession->location.loc.variant - 1 < 3U)) {
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon < 0x270FU) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon++;
                    }
                }
                scan = &D_8010CA2C;
                inventoryClearItems(scan);
                arg0->status = Gp_GrantLocationItems(scan);
                if (arg0->status != 0) {
                    uiSpawnObject(D_8010CA78, 1, 0, 0x11, arg0->spawnArg2.pointer);
                    if (arg0->status == 2) {
                        uiSpawnObject(D_8010CA78 + 1, 2, 0, 0x21, arg0->spawnArg2.pointer);
                    }
                }
            } else {
                arg0->status = 0;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped < 0x270FU) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped++;
                }
            }
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        arg0->state++;
    } else if (arg0->state == 1) {
        session = gGameSession;
        if (!(session->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
            session->viewReady             = 1;
            gStageMusicParams.fadeOutTicks = 0;
            gStageMusicParams.field_2      = 0;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY)) {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 1, 0);
            } else {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 3, 0);
            }
        } else {
            gStageMusicLoadState = 0xFF;
        }
        arg0->state++;
    } else if (arg0->state == 2) {
        UiObject* obj;

        obj = arg0->spawnArg2.pointer;
        if (gStageMusicLoadState == 0xFF) {
            if (cdCmdIsIdle() & 0xFFFF) {
                if (obj->result == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(obj, obj->owner);
                    if (arg0->status != 0) {
                        Gp_PubItemLoc           = 0x700;
                        arg0->spawnArg2.pointer = uiSpawnObject(&D_8010D6D8, 1, 1, 1, NULL);
                        arg0->state++;
                    } else {
                        arg0->killCountdown = 0xA;
                        arg0->state         = 0x10;
                    }
                }
            }
        }
    } else if (arg0->state == 3) {
        UiObject* obj;

        obj = arg0->spawnArg2.pointer;
        if ((obj->result == USER_INTERFACE_RESULT_CONFIRM) || (obj->result == USER_INTERFACE_RESULT_CANCEL)) {
            uiStartTreeClosing(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 0x10;
        }
    } else if (arg0->state == 0x10) {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            arg0->state = 0x11;
        }
    }

    if (arg0->state >= 0x11) {
        if (gStageMusicLoadState == 0xFF) {
            if (cdCmdIsIdle() & 0xFFFF) {
                displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
                SndEvt_EnqueueType9(SOUND_COMMON(0x0D));
                taskKill(arg0);
                Stage_ReleasePrimBuf();
                stageRequestModeTaskExit();
            }
        }
    }
}
