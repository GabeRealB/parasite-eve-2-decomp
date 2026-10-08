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

/// Queues the current mapped view's image file with default placement and load policy.
///
/// `fileKey` and `loadArgs` borrow distinct writable four-byte records for this call.
/// The key must hold the session's stage (1..5) and area; byte 1 is ignored.
/// Selects folder area * 100 + 1 within the stage CDF and the mapped view byte as its
/// file index. Requires valid loaded view maps, writable load destinations and
/// one free CD-ring slot. Enqueue copies the records before returning; neither
/// pointer is retained and queueing does not wait for completion.
static inline void _loadingEnqueueViewFile(_LoadingFileKey* fileKey, _LoadingFileArgs* loadArgs)
{
    enum { LOADING_VIEW_FOLDER_SUFFIX = 1 };

    fileKey->fileIndex         = viewGetMappedIndex();
    loadArgs->fileIdHundreds   = LOADING_VIEW_FOLDER_SUFFIX;
    loadArgs->loadMode         = CD_COMMAND_LOAD_DEFAULT;
    loadArgs->imageXPageOffset = 0;
    loadArgs->imageYOffset     = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
}

void loadingBeginViewLoadTask(Task* viewLoadTask)
{
    enum {
        LOADING_VIEW_WAIT_FOR_SCENE_IMAGE = 5,
        LOADING_VIEW_MOVIE_SUB_ID         = 0,
        LOADING_VIEW_REQUIRE_VIEW_MOVIE   = 1,
        LOADING_VIEW_MOVIE_SELECTED       = 1,
        LOADING_VIEW_NO_MOVIE_SELECTED    = 0
    };
    DisplayState*          display;
    CdCmdQueue*            queue;
    const GameLocationKey* location;
    _LoadingFileKey        fileKey;
    _LoadingFileArgs       loadArgs;

    location = &gGameSession->location.loc;
    queue    = &gCdCmdQueue;
    if (viewLoadTask->spawnArg1.value != 0) {
        gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    }
    display = &gDisplayState;
    // Wait for the game and task buffer selectors to agree before holding the frame.
    if (display->otBuffer == display->frameBuffer) {
        DrawSync(0);
        SetDrawStp(&D_80114C50, 0);
        DrawPrim(&D_80114C50);
        display->control.flags.flipMode = DISPLAY_FLIP_HOLD;
        if (queue->scenePayloadAvailable != 0) {
            mdecRequestSceneImageDecode(&gGameSession->location.loc.view);
            viewLoadTask->state = LOADING_VIEW_WAIT_FOR_SCENE_IMAGE;
        } else {
            // Movie views release model packets; leaving movie mode restores their policy.
            D_80114C40 = streamFindMovieSlot(&gGameSession->location.loc, LOADING_VIEW_MOVIE_SUB_ID, LOADING_VIEW_REQUIRE_VIEW_MOVIE);
            if (D_80114C40 >= 0) {
                sceneFreeActorPrimitiveBuffers();
                queue->viewMovieSelected = LOADING_VIEW_MOVIE_SELECTED;
            } else {
                if (queue->viewMovieSelected != 0) {
                    areaRestoreModelBufferPolicy();
                    queue->viewMovieSelected = LOADING_VIEW_NO_MOVIE_SELECTED;
                }
            }
            // Cancellation waits in state 1; an idle queue starts the image state immediately.
            if (cdCmdIsIdle() == 0) {
                cdCmdRequestCancel();
                viewLoadTask->state += 1;
                loadingEnqueueViewResourcesTask(viewLoadTask);
            } else {
                fileKey.stage     = location->stage;
                fileKey.fileGroup = location->area;
                _loadingEnqueueViewFile(&fileKey, &loadArgs);
                viewLoadTask->state += 2;
                loadingUploadViewImageTask(viewLoadTask);
            }
        }
    }
}
