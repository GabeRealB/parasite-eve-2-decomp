#ifndef TITLE_H
#define TITLE_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — title / demo / main-menu overlay (src/title/title.c)
// =============================================================================

/// Title-screen work block stored at Task::field_1C (Mem_Calloc 0x18).
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

/// Package header base (entry count word + phase table + demo strings).
extern const u32 Title_Header;
/// 5-way phase table at Title_Header + 4 (Title_Dispatch indexes by Task::field_30).
extern const TaskFuncTable5 Title_PhaseTable;
/// "##########DEMO START\n"
extern const char Title_DemoStartMsg[];
/// "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n"
extern const char Title_DemoCardRestoreMsg[];

/// Menu label strings (menu.data.s).
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

void Title_InitTask(Task* task);
void Title_DrawSpriteRow(s32 y, s32 v, s32 color);
void Title_MenuTask(Task* task);
void Title_RestoreDemoCard(void);
void Title_FlagAdvanceTask(Task* task);
void Title_Dispatch(Task* task);
void Title_ExitTask(Task* task);
void Title_DemoStreamTask(Task* task);
void Title_BootTask(Task* task);
/// Enqueue CD load for demo scene `index` (packed file id uses index + 0xA).
void Title_EnqueueDemoScene(s32 index);

#endif // TITLE_H
