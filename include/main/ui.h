#ifndef MAIN_UI_H
#define MAIN_UI_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/task_types.h"
#include "main/ui_types.h"

/// Prompt panel whose owning task carries the current text in spawnArg1.
extern UiObject* Wip_UiHolder;

/// Current item-information panel, cleared when its owner closes.
extern UiObject* D_80067634;

UiObject* Ui_SpawnTextBlock(TextBlockDesc* descriptor, s32 unused2, s32 unused3, s32 unused4);

UiObject* Ui_SpawnFromDesc(UiObjectDesc* descriptor, TaskSpawnArg arg1, s32 arg2, s32 arg3, UiObject* object);

void Ui_SizeFromText(UiPanel* panel, u8* arg1, s32 arg2, s32 arg3);

void Ui_SizeFromTextPlain(UiPanel* panel, u8* arg1);

void Ui_SizeFromTextWide(UiPanel* panel, u8* arg1);

void Ui_UpdateLayoutSize(UiPanel* panel, s32 arg1, s32 arg2);

void Ui_TeardownTree(UiObject* object, Task* unused2);

void Ui_FreeAndKill(Task* task);

void Ui_SetState4(UiObject* object, Task* unused2);

s32 Ui_IsStateDone(UiObject* object);

void Ui_DrawTextColored(UiPanel* panel, char* arg1);

void Ui_DrawText(UiPanel* panel, char* arg1);

void Ui_InsetLayout(UiPanel* panel, RECT* arg1, RECT* arg2, s32 unused4);

void Ui_ClampDialogRect(UiPanel* arg0, UiList* list, UiPanel* arg2);

/// Set the prompt text; the owner task stores it in its mixed spawn payload.
void Ui_SetHolderParam(u8* arg0, s32 unused2, s32 unused3);

/// Set a numeric item id (0x300..0x3FF) for the PE cost prompt.
void Ui_SetHolderParamAlt(s32 arg0, s32 unused2, s32 unused3);

void Ui_ClampAnimOrClose(UiPanel* panel, Task* task, s32 arg2);

void Ui_StartCloseAnim(UiPanel* panel, Task* unused2);

void Ui_LayoutListPanel(UiList* arg0, UiPanel* arg1);

void Ui_InitList(UiList* list, UiPanel* panel);

void Ui_ComputeVisibleRows(UiList* list, UiPanel* panel);

void Ui_UpdateListNoAnim(void* arg0, void* arg1);

void Ui_SmoothCursor(UiPanel* panel, s32 arg1, s32 arg2);

s32 Ui_GetCursorFixed(void);

s32 Ui_LookupTable(void* unused1, s32 arg1);

s32 Ui_Scale15(s32 arg0);

void Ui_DrawHBar(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3);

void Ui_DrawVBar(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3);

void Ui_DrawTextInRect(RECT* rect, s32 arg1, s32 arg2, char* arg3);

void Ui_DrawTitle(UiPanel* panel, char* arg1);

void Ui_SetListScrollFlag(UiList* list, s32 arg1);

void Ui_AllocTile(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);

void Ui_InsertDrawTPage(s32 arg0, s32 arg1);

/// Fills a rectangle and draws its light and dark bevel edges.
void Ui_DrawBeveledRect(UiPanel* panel, s32 x, s32 y, s32 width, s32 height, u32 color, s32 inset);

void Ui_LayoutWithMode0(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);

void Ui_LayoutWithMode1(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);

void Ui_WaitCdThenOverlay(Task* task);

void Ui_DrawFlatCaret(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

#endif // MAIN_UI_H
