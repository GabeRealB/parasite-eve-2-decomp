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

/// Queues a view file key after assigning its mapped index and default load options.
///
/// The borrowed four-byte key must already hold stage/area; byte 1 is ignored.
/// Requires loaded view mapping, valid destinations and one free CD-ring slot.
/// Enqueue copies both records immediately; no request pointers are retained.
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
