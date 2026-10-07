#include "loading.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/loading.h"
#include "scene_runtime.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/stream.h"

s16 D_80114C40;

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
            mdecRequestSceneImageDecode(&gGameSession->location.loc.view);
            task->state = 5;
        } else {
            D_80114C40 = streamFindMovieSlot(&gGameSession->location.loc, 0, 1);
            if (D_80114C40 >= 0) {
                sceneFreeActorPrimitiveBuffers();
                q->viewMovieSelected = 1;
            } else {
                if (q->viewMovieSelected != 0) {
                    areaRestoreModelBufferPolicy();
                    q->viewMovieSelected = 0;
                }
            }
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                cdCmdRequestCancel();
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
