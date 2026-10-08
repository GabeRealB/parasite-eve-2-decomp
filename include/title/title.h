#ifndef INCLUDE_TITLE_TITLE_H
#define INCLUDE_TITLE_TITLE_H

#include "types.h"

#include "main/task_types.h"

/// TaskDesc table: [0]=_titleStartupTask, [1]=_titleIntroMovieTask.
extern TaskDesc Title_TaskDescs[];

/// Restores the live save, player image and flag banks for an attract demo.
///
/// Requires the title overlay and a fully loaded demo save prefix in actor
/// buffer 2, or at `FILE_SYSTEM_FIXED_REPLAY_BASE` for fixed replay. The prefix
/// contains, in order, one `McSaveData`, one `PLAYER_STATUS_SAVE_RECORD_BYTES`
/// player image, the Akropolis, Dryfield day, Dryfield night, mine/Shelter and
/// Neo Ark banks, and one `GameFlagNibbleBank` (0xD4C bytes total). The source
/// must remain readable and disjoint from the resident destinations throughout.
/// Only live images are replaced; their memory-card backups remain intact.
/// Preserves the live save's vibration setting and demo selection. The loaded
/// player matrix pointer must be rebound before gameplay uses it. An unavailable
/// saved stage requests `DISPLAY_GAME_RESTART`; no checksum validation is done.
void titleRestoreAttractDemoState(void);

/// Queues the save/replay resources for a numbered attract demo.
///
/// `demoIndex` is zero-based: the retail title selects 0..2, loading stage-zero
/// files 801000, 801100 and 801200. Their save/replay payload replaces actor
/// buffer 2. The hundreds component is narrowed to a byte. Requires the title
/// overlay, initialized scratch stack and space in the CD request ring.
/// Returns before loading completes; both request buffers are copied during
/// this call and are not retained.
void titleEnqueueAttractDemoFile(s32 demoIndex);

/// Runs one state of the title prompt/menu task and advances the random sequence.
///
/// Requires the title overlay and `task->state` in 0..4 (initialize, show
/// background, prompt, menu, release). A new task starts at state 0;
/// `spawnArg1.value` holds a frame delay in its low 31 bits. Its high bit is
/// cleared when tested and suppresses the screen fade only if the remaining
/// delay is zero on that tick. Initialization owns the menu work;
/// later states require it to remain live, and release can destroy the task.
void titleScreenTask(Task* task);

/// Runs the installed exit handler of the title's bank-0 slot-6 task.
///
/// The title overlay must be loaded, and `task` must be live with a non-NULL
/// exit handler whose code is loaded. A newly spawned task uses `taskKill`;
/// a replacement handler determines cleanup and may release the task before
/// returning, so callers cannot assume the task remains live.
void titleExitTask(Task* task);

#endif // INCLUDE_TITLE_TITLE_H
