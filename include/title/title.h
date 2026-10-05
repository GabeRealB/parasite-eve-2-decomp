#ifndef INCLUDE_TITLE_TITLE_H
#define INCLUDE_TITLE_TITLE_H

#include "types.h"

#include "main/task_types.h"

/// TaskDesc table: [0]=Title_BootTask, [1]=Title_DemoStreamTask.
extern TaskDesc Title_TaskDescs[];

void Title_RestoreDemoCard(void);

/// Enqueue CD load for demo scene `index` (packed file id uses index + 0xA).
void Title_EnqueueDemoScene(s32 index);

void Title_Dispatch(Task* arg0);

/// Runs the installed exit handler of the title's bank-0 slot-6 task.
///
/// The title overlay must be loaded, and `task` must be live with a non-NULL
/// exit handler whose code is loaded. A newly spawned task uses `taskKill`;
/// a replacement handler determines cleanup and may release the task before
/// returning, so callers cannot assume the task remains live.
void titleExitTask(Task* task);

#endif // INCLUDE_TITLE_TITLE_H
