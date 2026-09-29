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

void Title_ExitTask(Task* arg0);

#endif // INCLUDE_TITLE_TITLE_H
