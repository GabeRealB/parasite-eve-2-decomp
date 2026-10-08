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

/// Uploads the current mapped view's retained image, retrying every incomplete result.
///
/// Borrows loaded directory payloads throughout the call; the image uploader's
/// scratch, timer and GPU requirements apply. No matching image skips the upload.
static inline void _loadingUploadCurrentViewImage(void)
{
    enum { LOADING_VIEW_IMAGE_IGNORE_GPU_TIME_LIMIT = 1 };
    u8 mappedViewIndex;
    u8 resourceSlotIndex;

    mappedViewIndex = viewGetMappedIndex();
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(D_8006C338); resourceSlotIndex++) {
        if (D_8006C338[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
            if (mappedViewIndex - 1 == resourceSlotIndex) {
                while (fsUploadImageChunk(D_8006C338[resourceSlotIndex].data, LOADING_VIEW_IMAGE_IGNORE_GPU_TIME_LIMIT) != FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE) {
                }
                break;
            }
        }
    }
}

void loadingUploadViewImageTask(Task* viewLoadTask)
{
    enum { LOADING_VIEW_IMAGE_FINISH_READY = -1 };
    CdCmdQueue* queue;
    u8          commandArgs[sizeof(queue->replacementEntry.args.bytes)];

    queue = &gCdCmdQueue;
    if (cdCmdIsIdle()) {
        memFillBytes(&queue->activeRequest, 0, sizeof(queue->activeRequest));
        _loadingUploadCurrentViewImage();
        cdCmdSelectMovieWorkspace();
        if (D_80114C40 >= 0) {
            viewLoadTask->state++;
            // The command packer copies all four bytes; only the slot is written.
            commandArgs[0] = (u8)D_80114C40;
            cdCmdStageReplacement(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            cdCmdCommitReplacement();
            viewLoadTask->killCountdown = 0;
        } else {
            if (cdCmdCommitReplacement() >= 0) {
                // Skip the movie-ready state when only a deferred CD request remains.
                viewLoadTask->state += 2;
            } else {
                viewLoadTask->state = LOADING_VIEW_IMAGE_FINISH_READY;
                loadingFinishViewLoad(viewLoadTask);
            }
        }
    }
}
