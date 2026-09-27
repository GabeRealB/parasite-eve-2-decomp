#ifndef TITLE_H
#define TITLE_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — title / demo / main-menu overlay (src/title/title.c)
// =============================================================================

/// Title-screen work block stored at Task::work (memCalloc 0x18).
typedef struct _TitleWork {
    /* 0x00 */ s32 timer;          // frame / phase counter
    /* 0x04 */ s32 selection;      // menu cursor index
    /* 0x08 */ s32 fadeTileEnable; // fullscreen fade TILE when non-zero
    /* 0x0C */ s32 logoFade;       // intro logo alpha 0..0x80
    /* 0x10 */ s32 menuFade;       // menu chrome alpha 0..0x80
    /* 0x14 */ s32 menuCount;      // number of menu entries
} TitleWork;
STATIC_ASSERT_SIZEOF(TitleWork, 0x18);

// =============================================================================
// Overlay globals (assets/USA/OVR/title — load @ 0x80093800)
// =============================================================================

/// Retained text labels for the title menu (src/title/title.c).
extern char Title_StrNewGame[];
extern char Title_StrLoadGame[];
extern char Title_StrConfiguration[];
extern char Title_StrDebugOption[];
extern char Title_StrExtraGame[];
extern char Title_StrSurvival[];

/// Task spawn ids for menu selection indices.
extern s32 Title_MenuSpawnIds[];
/// TaskDesc table: [0]=Title_BootTask, [1]=Title_DemoStreamTask.
extern TaskDesc Title_TaskDescs[];
/// Last rand() from Title_Dispatch.
extern s32 Title_LastRand;
/// When set, Title_BootTask spawns phase task with arg 0x80000000 (skip fade TILE).
extern u16 Title_SkipFadeFlag;

// =============================================================================
// APIs
// =============================================================================

void Title_RestoreDemoCard(void);
void Title_DemoStreamTask(Task* task);
void Title_BootTask(Task* task);
/// Enqueue CD load for demo scene `index` (packed file id uses index + 0xA).
void Title_EnqueueDemoScene(s32 index);

void Title_Dispatch(Task* arg0);
void Title_ExitTask(Task* arg0);

#endif // TITLE_H
