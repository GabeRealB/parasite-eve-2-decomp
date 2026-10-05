#include "loading.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/loading.h"
#include "scene_runtime.h"

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

#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/stream.h"

s16 D_80114C40;

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void Gp_ViewBeginLoad(Task* task)
{
    DisplayState*    ds;
    CdCmdQueue*      q;
    GameLocationKey* sess;
    u8               param1[8];
    u8               param2[8];

    sess = &gGameSession->location.loc;
    q    = &gCdCmdQueue;
    if (task->spawnArg1.value != 0) {
        gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    }
    ds = &gDisplayState;
    if (ds->otBuffer == ds->frameBuffer) {
        DrawSync(0);
        SetDrawStp(&D_80114C50, 0);
        DrawPrim(&D_80114C50);
        ds->control.flags.flipMode = DISPLAY_FLIP_HOLD;
        if (q->scenePayloadAvailable != 0) {
            Mdec_ResolveStreamBuffer(&gGameSession->location.loc.view);
            task->state = 5;
        } else {
            D_80114C40 = streamFindMovieSlot(&gGameSession->location.loc, 0, 1);
            if (D_80114C40 >= 0) {
                Gp_FreeSlot4TmdBuffers();
                q->viewMovieSelected = 1;
            } else {
                if (q->viewMovieSelected != 0) {
                    Gp_ApplyAreaTmdFlags();
                    q->viewMovieSelected = 0;
                }
            }
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                CdCmd_ActivatePhase1();
                task->state += 1;
                Gp_EnqueueViewCd(task);
            } else {
                param1[3] = sess->stage;
                param1[2] = sess->area;
                param1[0] = viewGetMappedIndex();
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
                task->state += 2;
                Gp_ViewLoadImage(task);
            }
        }
    }
}
