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

/// Spawns a UI object and its task, optionally as a child of `parent`.
///
/// Copies `spawnArg1` into the task without copying pointed-to storage; that
/// storage must live as long as the descriptor's callback uses it. `controlMode`
/// initializes the panel's control state and `animationTicks` its signed 16-bit
/// counter in frame ticks. Returns NULL if either allocation fails.
UiObject* Ui_SpawnFromDesc(UiObjectDesc* descriptor, TaskSpawnArg spawnArg1, s32 controlMode, s32 animationTicks, UiObject* parent);

void Ui_SizeFromText(UiPanel* panel, u8* arg1, s32 arg2, s32 arg3);

void Ui_SizeFromTextPlain(UiPanel* panel, u8* arg1);

void Ui_SizeFromTextWide(UiPanel* panel, u8* arg1);

void Ui_UpdateLayoutSize(UiPanel* panel, s32 arg1, s32 arg2);

void Ui_TeardownTree(UiObject* object, Task* unused2);

/// Frees a task's UI object and begins default task teardown.
///
/// The exit handler installed when an object task is spawned. `task` must be
/// non-NULL, live and not already torn down. Its second spawn argument is
/// either NULL or the primary-heap `UiObject` allocated for that task. A null
/// pointer frees nothing and does not select the primary heap.
///
/// The object is released before `taskKill`. Child exit handlers therefore
/// run after it is gone. `taskKill` does not release this spawn argument, and
/// immediate teardown can free the task before it returns, so the object
/// cannot be freed afterwards. Calling this directly bypasses a replacement
/// exit callback. The pointer is left dangling. Callers must not access the
/// task or the object afterwards.
void uiObjectTaskExit(Task* task);

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

/// Begins a panel's opening transition unless the panel is already open.
///
/// An open panel is left unchanged. Every other lifecycle selects
/// `USER_INTERFACE_PANEL_OPENING`. A counter whose unsigned halfword is at
/// most `USER_INTERFACE_PANEL_ANIMATION_TICKS` is kept, so opening proceeds
/// from the current tick. A negative sentinel and any larger counter become
/// that nine-tick span, which starts a full opening. The counter is in
/// nominal 60-Hz frame ticks.
///
/// `owningTask` is the task that owns `panel`. The function does not read it.
void uiStartPanelOpening(UiPanel* panel, Task* owningTask);

void Ui_LayoutListPanel(UiList* arg0, UiPanel* arg1);

void Ui_InitList(UiList* list, UiPanel* panel);

void Ui_ComputeVisibleRows(UiList* list, UiPanel* panel);

void Ui_UpdateListNoAnim(void* arg0, void* arg1);

void Ui_SmoothCursor(UiPanel* panel, s32 arg1, s32 arg2);

s32 Ui_GetCursorFixed(void);

s32 Ui_LookupTable(void* unused1, s32 arg1);

s32 Ui_Scale15(s32 arg0);

/// Queues a textured horizontal separator across a panel's content.
///
/// `left`, `right` and `centerY` are signed pixel coordinates relative to the
/// content origin. The vertices span left..right and centerY-4..centerY+3;
/// their screen coordinates retain the low 16 bits. A left >= right span does
/// nothing. The panel is borrowed only for this call and is not modified.
///
/// Requires the UI texture/palette, space for one `POLY_FT4` in the current
/// primitive arena, and a writable tag at `panel->otIndex.signedValue + 2` in
/// the current ordering table. No bounds checks or clipping are performed here.
/// The packet remains in that arena until the GPU finishes drawing the frame.
void uiDrawHorizontalSeparator(const UiPanel* panel, s32 left, s32 right, s32 centerY);

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
