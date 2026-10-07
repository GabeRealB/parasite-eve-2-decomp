#include "types.h"

#include "gameplay/attachment_state.h"
#include "hud.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
HudHpMp Gp_HpMpWork;

u8 Gp_DebugAttachLevels[18];

u16 Gp_ReplayButtons;

u16 Gp_ReplayFramesLeft;

AttachmentState Gp_StateC08;

InventoryItemRow Gp_ItemTable2[5];

s32 D_80114C34;

u16* Gp_ReplayCursor;

#include "attachments.h"

#include "gameplay/attachments.h"

AttachmentLevelTable Gp_IdParamHi = { .rows = {
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 8, 22, 70, 7, 3, 2 } },
                                          { { 1250, 1, 7, 22, 100, 7, 3, 2 } },
                                          { { 3000, 2, 6, 22, 45, 7, 3, 2 } },
                                          { { 750, 1, 14, 38, 120, 7, 3, 0 } },
                                          { { 1750, 1, 14, 30, 140, 7, 3, 0 } },
                                          { { 4000, 2, 14, 22, 170, 7, 3, 0 } },
                                          { { 3000, 1, 30, 50, 200, 7, 11, 0 } },
                                          { { 4000, 2, 30, 40, 250, 7, 11, 0 } },
                                          { { 5000, 4, 30, 28, 300, 7, 11, 0 } },
                                          { { 500, 1, 7, 38, 15, 3, 2, 12 } },
                                          { { 1250, 1, 7, 30, 20, 3, 2, 12 } },
                                          { { 3000, 2, 7, 22, 30, 3, 2, 12 } },
                                          { { 750, 1, 6, 18, 25, 1, 7, 0 } },
                                          { { 1750, 1, 5, 18, 30, 1, 7, 0 } },
                                          { { 4000, 2, 4, 18, 40, 1, 7, 0 } },
                                          { { 3000, 1, 18, 38, 60, 2, 12, 0 } },
                                          { { 4000, 2, 18, 38, 80, 2, 12, 0 } },
                                          { { 5000, 4, 18, 38, 100, 2, 12, 0 } },
                                          { { 500, 1, 7, 36, 0, 0, 0, 0 } },
                                          { { 1250, 1, 5, 28, 0, 0, 0, 0 } },
                                          { { 3000, 1, 3, 18, 0, 0, 0, 0 } },
                                          { { 750, 1, 12, 42, 30, 60, 0, 0 } },
                                          { { 1750, 1, 12, 30, 40, 70, 0, 0 } },
                                          { { 4000, 1, 12, 18, 50, 90, 0, 0 } },
                                          { { 3000, 1, 20, 44, 200, 0, 13, 0 } },
                                          { { 4000, 1, 18, 36, 250, 0, 13, 0 } },
                                          { { 5000, 2, 16, 28, 300, 0, 13, 0 } },
                                          { { 500, 2, 6, 36, 0, 0, 0, 0 } },
                                          { { 1250, 2, 5, 28, 0, 0, 0, 0 } },
                                          { { 3000, 4, 4, 18, 0, 0, 0, 0 } },
                                          { { 750, 2, 10, 36, 0, 0, 0, 0 } },
                                          { { 1750, 2, 9, 28, 0, 0, 0, 0 } },
                                          { { 4000, 4, 8, 18, 0, 0, 0, 0 } },
                                          { { 3000, 2, 15, 36, 200, 1, 14, 0 } },
                                          { { 4000, 4, 15, 28, 200, 1, 14, 0 } },
                                          { { 5000, 9, 15, 18, 200, 1, 14, 0 } },
                                          { { 0, 5, 5, 30, 0, 0, 0, 0 } },
                                          { { 0, 4, 10, 30, 0, 0, 0, 0 } },
                                          { { 0, 3, 20, 30, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 50, 9, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 12, 5, 2, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 18, 0, 8, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                          { { 0, 0, 0, 0, 0, 0, 0, 0 } },
                                      } };

u16 Gp_GetAttachParam(s32 arg0)
{
    PlayerStatus* p;
    s32           cond;
    s32           ret;
    u8*           table;
    s32           idx;
    u8*           recs;
    s32           off;

    recs = Gp_IdParamHi.bytes;
    idx  = Gp_StateC08.activeIndex;
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->statusFlags & PLAYER_STATUS_BERSERKER) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    off  = arg0 * sizeof(u16);
    off += (Gp_StateC08.activeIndex * 3 + ret) * sizeof(AttachmentLevelRow);
    {
        union {
            u8*  bytes;
            u16* value;
        } param;
        param.bytes = recs + off;
        return *param.value;
    }
}
