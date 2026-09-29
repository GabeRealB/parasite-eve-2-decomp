#include "gameplay/item_menu.h"

#include <psyq/libgte.h>

#include "common.h"

#include "area_flags.h"
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

/// `Task::spawnArg2` payload of `func_800B65B0`, the pickup-confirm task.
/// field_0 is the `GpBit2Rec` item id passed to `Gp_LookupBit2Item`;
/// field_2 is set to 1 when the task finishes, field_3 to 1 when the player
/// confirmed (`UiObject.field_2C == 0x33`), and field_4 is the spawn mode
/// (0 when `D_80114DDE` bit 9 is set, else 1; passed inverted to
/// `Ui_SpawnFromDesc`).
typedef struct _GpPickupWork {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
} GpPickupWork;
STATIC_ASSERT_SIZEOF(GpPickupWork, 6);

UiObjectDesc D_8010D348 = { 2, -64, -32, 128, 64, 60, 0, 0, 192, func_800B92CC, 0 };

void func_800B65B0(Task* task)
{
    GpPickupWork* work;
    UiObjectDesc* desc;
    UiObject*     ui;
    UiObject*     spawned;
    Task*         child;
    GfxCoord*     coord;
    PlayerPos*    savedPos;
    s32           storedX;
    s32           angle;
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           id;
    s32           shift;
    u32           mask;
    u32*          flags;
    u32*          current;

    work = task->spawnArg2.pointer;
    if (task->state == 0) {
        GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        if (Gp_LookupBit2Item(work->field_0) == 0) {
            work->field_3 = 0;
            work->field_2 = 1;
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
                coord         = (gameGetPtrSlot(3))->extra.tmd->coords;
                storedX       = (u16)coord->coord.t[0];
                savedPos      = &Player_Status.pos;
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
                cfg                    = &Player_Status;
                save                   = &Mc_SaveData[0];
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
        work->field_3 = 0;
        if (D_80114DDE & 0x200) {
            work->field_4 = 0;
        } else {
            work->field_4 = 1;
        }
        spawned = Ui_SpawnFromDesc(desc, (s32)((s8)(work->field_4 ^ 1)), 1, 1, NULL);
        if (spawned != NULL) {
            task->firstChild = spawned->owner;
            task->state++;
        }
    }
    if (task->state < 0x10) {
        child = task->firstChild;
        if (child != NULL) {
            ui = child->spawnArg2.pointer;
            if (ui->field_2E == -1 || ui->field_2E == 6) {
                switch (Gp_PubItemLoc >> 8) {
                    case 0:
                    case 1:
                        if (ui->field_2C == 0x33) {
                            id      = work->field_0;
                            current = Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4 + (id >> 4);
                            shift   = (id & 0xF) * 2;
                            mask    = 3 << shift;
                            if (((*current & mask) >> shift) != 3) {
                                flags  = Gp_Bit2Banks[Mc_SaveData[0].state.at4.loc.stage].field_4 + (id >> 4);
                                *flags = (*flags & ~mask) | (2 << shift);
                            }
                            work->field_3 = 1;
                        } else {
                            work->field_3 = 0;
                        }
                        break;
                    case 8:
                        if (ui->field_2C == 0x33) {
                            work->field_3 = 1;
                        } else {
                            work->field_3 = 0;
                        }
                        break;
                    default:
                        work->field_3 = 0;
                        break;
                }
                Ui_TeardownTree(ui, ui->owner);
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
            work->field_2          = 1;
            gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
            Stage_ReleasePrimBuf();
            taskKill(task);
        }
    }
}
