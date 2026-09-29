#include "loading.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/loading.h"
#include "gameplay/view.h"

#include "main/fs.h"
#include "main/mem.h"
#include "main/task_types.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

DR_STP D_80114C50;

GpViewTbl* Gp_ViewTables[5] = { &D_map_akropolis_8017AC14, &D_map_dryfield_8017AB60, &D_map_dryfield_full_8017AA74, &D_map_shelter_8017B480, &D_map_neo_ark_8017AD28 };

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
        Text_DrawString(&req, (str));                         \
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
        Text_DrawString(&req, Text_ItoaSigned(buf, (count))); \
        if ((count) == 0) {                                   \
            flag = 1;                                         \
        }                                                     \
    }

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void Gp_ViewLoadImage(Task* task)
{
    CdCmdQueue* q;
    u8          view;
    u8          i;
    u8          param;

    q = &CdCmd_Queue;
    if (CdCmd_IsIdle() & 0xFFFF) {
        Mem_Set(&q->field_40, 0, 0x10);
        view = Gp_GetViewIndex();
        for (i = 0; i < 50; i++) {
            if (D_8006C338[i].field_0 == 2) {
                if (view - 1 == i) {
                    while (Fs_LoadImageChunk(D_8006C338[i].field_4, 1)) {
                    }
                    break;
                }
            }
        }
        CdCmd_SelectMdecBuffer();
        if (D_80114C40 >= 0) {
            task->state++;
            param = (u8)D_80114C40;
            CdCmd_EnqueueReplace(0x61, 0, &param);
            CdCmd_CommitReplace();
            task->killCountdown = 0;
        } else {
            if ((s16)CdCmd_CommitReplace() >= 0) {
                task->state += 2;
            } else {
                task->state = -1;
                Gp_FinishLoadWait(task);
            }
        }
    }
}
