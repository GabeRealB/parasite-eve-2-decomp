#include "gameplay/area_entry.h"

#include "types.h"

#include "area_entry.h"
#include "ending.h"
#include "ending_work.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "scene_runtime.h"
#include "gameplay/sound_params.h"
#include "world_targets.h"

#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"

Task* Gp_ActorSlots[2];

/// Unreferenced halfword table following the item-grant scan.
static u16 D_8010CA30[];

/// Area transition panel and item-title panel, selected by the spawn index.
extern UiObjectDesc D_8010CA40[];

extern UiObjectDesc D_8010CA78[];

void Gp_AreaEnterTask(Task* arg0);

extern UiObjectDesc D_80185000;

extern u16 D_8007A39C;

u8         Gp_StrItemObtained[] = "Item obtained!";
u8         Gp_StrBonusItem[]    = "Bonus item!!";
s32        Gp_ItemGrantCooldown = 0;
McItemScan D_8010CA2C           = { 0, 5, 2, 0 };

/// Unreferenced halfword table following the item-grant scan.
static u16 D_8010CA30[] = { 0x3A, 0x2E, 0x3B, 0x31, 0x41, 0x34, 0, 0 };

/// Area transition panel and item-title panel, selected by the spawn index.
UiObjectDesc D_8010CA40[] = {
    { 2, -80, -48, 160, 64, 32, 0, 0, 0xC0, func_800A087C, 0 },
    { 2, -80, -96, 256, 192, 36, 0, 0, 0xC0, Gp_DrawItemTitle, 0 },
};
UiObjectDesc D_8010CA78[] = {
    { 3, -80, 16, 160, 20, 28, 0, 0, 0xC0, Gp_DrawItemObtained, 0 },
    { 3, 16, 24, 160, 20, 24, 0, 0, 0xC0, Gp_DrawItemObtained, 0 },
};
TaskDesc D_8010CAB0 = { 0, 0xC0, Gp_EndingTask };
TaskDesc D_8010CABC = { 0, 0xC0, Gp_AreaEnterTask };

void Gp_AreaEnterTask(Task* arg0)
{
    u32          key;
    GpEndWork*   work;
    s32          i;
    Task*        slot;
    GameSession* session;
    GpSndParam*  pair;
    McItemScan*  scan;

    if (arg0->state == 0) {
        work = arg0->spawnArg2;
        key  = GP_LOC_WORD(gGameSession->at4.loc);
        key &= GP_LOC_STAGE_AREA;
        Stage_InitPrimBufOnce();
        for (i = 0; i < 2; i++) {
            slot = Gp_ActorSlots[i];
            if (slot != NULL) {
                ((GameActor*)slot->work)->field_90C = NULL;
            }
        }
        SndEvt_EnqueueType8(0xD);
        Gp_EnqueueSndCd((Gp_GetAttachLevel(7) + 0x15) & 0xFF);
        if (key == GP_LOC_KEY(1, 20, 0, 0)) {
            arg0->spawnArg2 = Ui_SpawnFromDesc(&D_80185000, arg0->spawnArg1, 1, 4, NULL);
        } else {
            arg0->spawnArg2 = Ui_SpawnFromDesc(D_8010CA40, arg0->spawnArg1, 1, 1, NULL);
            if (arg0->spawnArg1 == 0) {
                work->field_4 = 0;
                work->field_0 = 0;
                Gp_SetAreaFlag2(1, (GpAreaKey*)&gGameSession->at4.loc);
                gGameSession->field_126 = 1;
                if (!((key == GP_LOC_KEY(5, 11, 0, 0) || key == GP_LOC_KEY(5, 29, 0, 0)) &&
                      gGameSession->at4.loc.place - 1 < 3U)) {
                    if (Mc_SaveData[0].field_6CC < 0x270FU) {
                        Mc_SaveData[0].field_6CC++;
                    }
                }
                scan = &D_8010CA2C;
                Gp_ClearScanItems(scan);
                arg0->status = Gp_GrantLocationItems(scan);
                if (arg0->status != 0) {
                    Ui_SpawnFromDesc(D_8010CA78, 1, 0, 0x11, arg0->spawnArg2);
                    if (arg0->status == 2) {
                        Ui_SpawnFromDesc(D_8010CA78 + 1, 2, 0, 0x21, arg0->spawnArg2);
                    }
                }
            } else {
                arg0->status = 0;
                if (Mc_SaveData[0].field_6CE < 0x270FU) {
                    Mc_SaveData[0].field_6CE++;
                }
            }
        }
        GameMain_SetFrameTiming(0);
        arg0->state++;
    } else if (arg0->state == 1) {
        session = gGameSession;
        if (!(session->flowFlags & 2)) {
            session->viewReady = 1;
            pair               = (GpSndParam*)&D_8007A39C;
            pair->field_0      = 0;
            pair->field_2      = 0;
            if (!(gGameSession->flowFlags & 8)) {
                Task_SpawnFromTable(&D_80062774, 0, 1, 0);
            } else {
                Task_SpawnFromTable(&D_80062774, 0, 3, 0);
            }
        } else {
            gStageMusicLoadState = 0xFF;
        }
        arg0->state++;
    } else if (arg0->state == 2) {
        UiObject* obj;

        obj = arg0->spawnArg2;
        if (gStageMusicLoadState == 0xFF) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                if (obj->field_2E == 6) {
                    Ui_TeardownTree(obj, obj->owner);
                    if (arg0->status != 0) {
                        Gp_PubItemLoc   = 0x700;
                        arg0->spawnArg2 = Ui_SpawnFromDesc(&D_8010D6D8, 1, 1, 1, NULL);
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

        obj = arg0->spawnArg2;
        if ((obj->field_2E == 6) || (obj->field_2E == -1)) {
            Ui_TeardownTree(obj, obj->owner);
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
            if (CdCmd_IsIdle() & 0xFFFF) {
                GameMain_SetFrameTiming(1);
                SndEvt_EnqueueType9(0xD);
                taskKill(arg0);
                Stage_ReleasePrimBuf();
                Stage_SetEndingFlag();
            }
        }
    }
}

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)          \
    {                                                         \
        req.x          = obj.panel.field_20.u + (dx) + xBase; \
        req.y          = (obj.panel.field_22.u + 9) + (line); \
        req.otIndex    = obj.panel.field_14.s + 1;            \
        req.field_8    = (color);                             \
        req.glyphTable = 5;                                   \
        req.centerMode = 0;                                   \
        req.field_E    = 1;                                   \
        func_8002E53C(&req, (str));                           \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                   \
    {                                                         \
        req.field_8    = 0x606060;                            \
        req.glyphTable = 5;                                   \
        req.centerMode = 2;                                   \
        req.field_E    = 0;                                   \
        req.x          = obj.panel.field_20.u + 0x94;         \
        req.y          = (obj.panel.field_22.u + 9) + (line); \
        req.otIndex    = obj.panel.field_14.s + 1;            \
        func_8002E53C(&req, Text_ItoaSigned(buf, (count)));   \
        if ((count) == 0) {                                   \
            flag = 1;                                         \
        }                                                     \
    }

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT
