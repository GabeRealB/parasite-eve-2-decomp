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
                    while (fsUploadImageChunk(D_8006C338[i].data, 1)) {
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
            cdCmdCommitReplacement();
            task->killCountdown = 0;
        } else {
            if (cdCmdCommitReplacement() >= 0) {
                task->state += 2;
            } else {
                task->state = -1;
                Gp_FinishLoadWait(task);
            }
        }
    }
}
