#include "gameplay/item_menu.h"

#include <psyq/libgte.h>

#include "common.h"

#include "area_flags.h"
#include "cap.h"
#include "item_menu.h"
#include "items.h"
#include "scene_runtime.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"
#include "main/wipsys.h"

UiObjectDesc D_8010D348 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -64, -32, 128, 64 }, 60, 0, TASK_BODY_NONE, 192, func_800B92CC, 0 };

void func_800B65B0(Task* task)
{
    CapActionRequest* request;
    UiObjectDesc*     desc;
    UiObject*         ui;
    UiObject*         spawned;
    Task*             child;
    GfxCoord*         coord;
    PlayerPos*        savedPos;
    s32               storedX;
    s32               angle;
    PlayerStatus*     cfg;
    McSaveData*       save;
    s32               id;
    s32               shift;
    u32               mask;
    u32*              flags;
    u32*              current;

    request = task->spawnArg2.pointer;
    if (task->state == 0) {
        GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        if (Gp_LookupBit2Item(request->actionId) == 0) {
            request->accepted = 0;
            request->done     = 1;
            taskKill(task);
            return;
        }
        switch (Gp_PubItemLoc >> 8) {
            case 0:
            case 1:
                desc = &D_8010F010;
                break;
            case 8:
                // Capture the root transform before presenting the save prompt.
                coord         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                storedX       = (u16)coord->coord.t[0];
                savedPos      = &gPlayerStatus.pos;
                savedPos->x   = storedX;
                savedPos->y   = coord->coord.t[1];
                savedPos->z   = coord->coord.t[2];
                angle         = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
                savedPos->yaw = angle;
                if ((s16)angle >= PLAYER_YAW_HALF_TURN + 1) {
                    savedPos->yaw = angle - PLAYER_YAW_FULL_TURN;
                } else if ((s16)angle < -PLAYER_YAW_HALF_TURN) {
                    savedPos->yaw = angle + PLAYER_YAW_FULL_TURN;
                }
                gDisplayState.gameMode = DISPLAY_GAME_MODAL;
                cfg                    = &gPlayerStatus;
                save                   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                save->state.playerExp  = cfg->exp;
                save->state.playerBp   = cfg->bp;
                save->state.savePoint  = Gp_PubItemLoc;
                Stage_InitPrimBufOnce();
                desc = &D_8010D348;
                break;
            default:
                Stage_InitPrimBufOnce();
                desc = &D_8010D6D8;
                break;
        }
        request->accepted = 0;
        if (D_80114DDE & AREA_OBJECT_PLACE_PROMPT) {
            request->defaultPrompt = 0;
        } else {
            request->defaultPrompt = 1;
        }
        spawned = Ui_SpawnFromDesc(desc, (s32)((s8)(request->defaultPrompt ^ 1)), 1, 1, NULL);
        if (spawned != NULL) {
            task->firstChild = spawned->owner;
            task->state++;
        }
    }
    if (task->state < 0x10) {
        child = task->firstChild;
        if (child != NULL) {
            ui = child->spawnArg2.pointer;
            if (ui->result == USER_INTERFACE_RESULT_CANCEL || ui->result == USER_INTERFACE_RESULT_CONFIRM) {
                switch (Gp_PubItemLoc >> 8) {
                    case 0:
                    case 1:
                        if (ui->resultValue == 0x33) {
                            id      = request->actionId;
                            current = Gp_Bit2Banks[gGameSession->location.loc.stage].objectStates + (id >> 4);
                            shift   = (id & 0xF) * 2;
                            mask    = 3 << shift;
                            if (((*current & mask) >> shift) != 3) {
                                flags  = Gp_Bit2Banks[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage].objectStates + (id >> 4);
                                *flags = (*flags & ~mask) | (2 << shift);
                            }
                            request->accepted = 1;
                        } else {
                            request->accepted = 0;
                        }
                        break;
                    case 8:
                        if (ui->resultValue == 0x33) {
                            request->accepted = 1;
                        } else {
                            request->accepted = 0;
                        }
                        break;
                    default:
                        request->accepted = 0;
                        break;
                }
                uiStartTreeClosing(ui, ui->owner);
                task->state = 0x10;
            }
        }
    }
    if (task->state == 0x10) {
        task->killCountdown = 0xC;
        task->state++;
    } else if (task->state == 0x11) {
        if (--task->killCountdown <= 0) {
            GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            request->done          = 1;
            gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
            Stage_ReleasePrimBuf();
            taskKill(task);
        }
    }
}
