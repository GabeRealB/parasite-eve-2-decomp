/* Room movie tasks shared by the Mine and Shelter overlays.
 * Include this interface in each carrier prologue and the selected fragment
 * at its function position. Presentation remains with the movie task until
 * the game resources are restored and its optional completion hold ends.
 */

#ifndef SRC_SHARED_STREAMED_SCENE_H
#define SRC_SHARED_STREAMED_SCENE_H

#include "main/task_types.h"

/// Playback phases stored in the movie task's byte-sized state selector.
enum {
    STREAMED_SCENE_PREPARE,
    STREAMED_SCENE_QUEUE_MOVIE,
    STREAMED_SCENE_WAIT_READY,
    STREAMED_SCENE_PLAYING,
    STREAMED_SCENE_WAIT_IDLE,
    STREAMED_SCENE_RESTORE_GAME,
    STREAMED_SCENE_HOLD,
};

enum {
    STREAMED_SCENE_MOVIE_ID   = 100,
    STREAMED_SCENE_HOLD_TICKS = 61,
};

/// Plays the current room's movie and resumes the game after restoring its resources.
///
/// Uses stream ID 100 with the current room key; the matching movie slot must
/// be loaded. The task must own display presentation during playback, with
/// the session, saved image-memory region and stream resources live through
/// restoration. Start requests cancellation. Both completion and cancellation
/// wait for CD idle before restoring model and sprite resources, releasing
/// the task and resuming the game loop. No task work is allocated.
void streamedScenePlay(Task* movieTask);

/// Plays the room's movie, then delays game presentation after normal completion.
///
/// Has `streamedScenePlay`'s resource and display requirements. Uses
/// `spawnArg1.value` as a cancellation latch: Start skips the final wait, while
/// normal completion counts 61 further calls with the display blank before
/// resuming. `killCountdown` must start at zero. The task ends after resource
/// restoration and this optional hold, retaining no pointers.
void streamedScenePlayThenHold(Task* movieTask);

#endif /* SRC_SHARED_STREAMED_SCENE_H */
