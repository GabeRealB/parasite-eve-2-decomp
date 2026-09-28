#include "loading.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/loading.h"
#include "scene_runtime.h"

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
        Text_DrawString(&req, (str));                           \
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
        Text_DrawString(&req, Text_ItoaSigned(buf, (count)));   \
        if ((count) == 0) {                                   \
            flag = 1;                                         \
        }                                                     \
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
    DisplayState* ds;
    CdCmdQueue*   q;
    GpAreaKey*    sess;
    u8            param1[8];
    u8            param2[8];

    sess = &gGameSession->at4.loc;
    q    = &CdCmd_Queue;
    if (task->spawnArg1.value != 0) {
        gDisplayState.at100.flags.flipMode = 2;
    }
    ds = &gDisplayState;
    if (ds->otBuffer == ds->frameBuffer) {
        DrawSync(0);
        SetDrawStp(&D_80114C50, 0);
        DrawPrim(&D_80114C50);
        ds->at100.flags.flipMode = 2;
        if (q->field_214 != 0) {
            Mdec_ResolveStreamBuffer(&gGameSession->at4.loc.view);
            task->state = 5;
        } else {
            D_80114C40 = Stream_FindSlot(&gGameSession->at4.loc.view, 0, 1);
            if (D_80114C40 >= 0) {
                Gp_FreeSlot4TmdBuffers();
                q->field_210 = 1;
            } else {
                if (q->field_210 != 0) {
                    Gp_ApplyAreaTmdFlags();
                    q->field_210 = 0;
                }
            }
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                CdCmd_ActivatePhase1();
                task->state += 1;
                Gp_EnqueueViewCd(task);
            } else {
                param1[3] = sess->stage;
                param1[2] = sess->area;
                param1[0] = Gp_GetViewIndex();
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                task->state += 2;
                Gp_ViewLoadImage(task);
            }
        }
    }
}
