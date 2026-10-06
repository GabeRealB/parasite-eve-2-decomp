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

ViewCameraTable* Gp_ViewTables[5] = { &D_map_akropolis_8017AC14, &D_map_dryfield_8017AB60, &D_map_dryfield_full_8017AA74, &D_map_shelter_8017B480, &D_map_neo_ark_8017AD28 };

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)                            \
    {                                                                           \
        req.x          = obj.panel.contentOriginX.unsignedValue + (dx) + xBase; \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        req.colorRgb   = (color);                                               \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_LEFT;                                   \
        req.drawMode   = TEXT_DRAW_OUTLINED;                                    \
        textDrawString(&req, (str));                                            \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                                     \
    {                                                                           \
        req.colorRgb   = 0x606060;                                              \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_RIGHT;                                  \
        req.drawMode   = TEXT_DRAW_FILL_ONLY;                                   \
        req.x          = obj.panel.contentOriginX.unsignedValue + 0x94;         \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        textDrawString(&req, textItoaSigned(buf, (count)));                     \
        if ((count) == 0) {                                                     \
            flag = 1;                                                           \
        }                                                                       \
    }

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void Gp_ViewLoadImage(Task* task)
{
    CdCmdQueue* q;
    u8          view;
    u8          i;
    u8          param;

    q = &gCdCmdQueue;
    if (cdCmdIsIdle() & 0xFFFF) {
        memFillBytes(&q->activeRequest, 0, sizeof(q->activeRequest));
        view = viewGetMappedIndex();
        for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
            if (D_8006C338[i].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
                if (view - 1 == i) {
                    while (Fs_LoadImageChunk(D_8006C338[i].data, 1)) {
                    }
                    break;
                }
            }
        }
        cdCmdSelectMovieWorkspace();
        if (D_80114C40 >= 0) {
            task->state++;
            param = (u8)D_80114C40;
            cdCmdStageReplacement(CD_COMMAND_PLAY_STREAM, 0, &param);
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
