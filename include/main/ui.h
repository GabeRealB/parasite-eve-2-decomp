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

UiObject* Ui_SpawnTextBlock(UiOptionDialogRequest* request, s32 unused2, s32 unused3, s32 unused4);

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

/// Detaches a UI subtree and requests closing animations before task release.
///
/// Recursively closes children before their parent, re-reading the task's child
/// head after each detach. Every child must own a live UiObject in spawnArg2,
/// and an already-closing child must have been detached. Nodes newly entering
/// closing detach from their parent; already-closing nodes keep their links.
/// Animation counters are preserved. No object or task is freed here: owning
/// task updates finish closing and dispatch exit callbacks later. `object`
/// and its owner must be live; `unusedOwningTask` is ignored.
void uiStartTreeClosing(UiObject* object, Task* unusedOwningTask);

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

/// Starts shrinking a panel into the retained hidden state.
///
/// Preserves animation ticks, input control, task links and children. The owning
/// task continues running; active control can reopen the panel once hidden.
/// To keep it hidden, its controller must leave input inactive. `object` must
/// remain live for its task updates; `unusedOwningTask` is not read.
void uiStartPanelHiding(UiObject* object, Task* unusedOwningTask);

/// Reports whether a UI object's panel is hiding or retained hidden.
///
/// Returns 1 for lifecycle indices 4 and 5, and 0 for indices 0..3. Hiding
/// includes an unfinished shrink animation. The signed >= comparison is
/// retained; `object` must be live and its lifecycle index must be in 0..5.
s32 uiIsPanelHidingOrHidden(const UiObject* object);

void Ui_DrawTextColored(UiPanel* panel, char* arg1);

void Ui_DrawText(UiPanel* panel, char* arg1);

void Ui_InsetLayout(UiPanel* panel, RECT* arg1, RECT* arg2, s32 unused4);

/// Places a dialog beside the list's current row and limits its right/bottom edges.
///
/// Uses the row text position plus the list panel's content origin, offset by
/// (+8,-2) pixels. Moves the dialog left/up if its right edge exceeds 150 or
/// bottom exceeds 90, in screen-centered pixels. No left/top limit is applied.
/// The dialog's existing dimensions are preserved. All three records must be
/// live; only dialog bounds are written, with 16-bit coordinate truncation.
void uiPositionRowDialog(UiPanel* dialogPanel, const UiList* list, const UiPanel* listPanel);

/// Set the prompt text; the owner task stores it in its mixed spawn payload.
void Ui_SetHolderParam(u8* arg0, s32 unused2, s32 unused3);

/// Set a numeric item id (0x300..0x3FF) for the PE cost prompt.
void Ui_SetHolderParamAlt(s32 arg0, s32 unused2, s32 unused3);

/// Limits a hidden panel's reopening delay, or begins opening an earlier state.
///
/// A nonzero `delayTicks` at state >= hidden replaces a negative counter or
/// caps a longer counter at delayTicks + nine; zero and shorter counters stay
/// unchanged. Otherwise delegates to `uiStartPanelOpening`, including when
/// delayTicks is zero. State and input control are preserved on the delay path.
///
/// Delays are nominal 60-Hz ticks. Use 0..32758 so the nine-tick bias fits the
/// signed 16-bit counter; the addition is signed s32 and the store narrows to
/// s16 without validation. `panel` must be live with lifecycle index 0..5.
/// `owningTask` is forwarded to the opening helper, which does not inspect it.
void uiLimitHiddenDelayOrOpen(UiPanel* panel, Task* owningTask, s32 delayTicks);

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

/// Returns the pixel height of `rowCount` text rows at fifteen pixels per row.
///
/// Excludes border and title padding. Callers supply nonnegative counts whose
/// shift by four and resulting height fit s32; the function does not clamp.
s32 uiGetTextRowsHeight(s32 rowCount);

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

/// Queues a textured vertical separator across a panel's content.
///
/// `top`, `bottom` and `centerX` are signed content-relative pixels. Vertices
/// span top..bottom and centerX-3..centerX+5, retaining their low 16 bits.
/// A top >= bottom span does nothing. The panel is borrowed without changes.
/// Requires the UI atlas/palette, one POLY_FT4's aligned arena space and a
/// writable signed panel OT base+2 tag. Retain the packet until GPU completion.
void uiDrawVerticalSeparator(const UiPanel* panel, s32 top, s32 bottom, s32 centerX);

void Ui_DrawTextInRect(RECT* rect, s32 arg1, s32 arg2, char* arg3);

/// Queues an underlined title at the panel's animated upper-left edge.
///
/// Uses the small font and muted RGB (96,112,112), with a backing plate and
/// separator sized from the final text pen. Opening/closing/hiding follow the
/// bottom-anchored animation rectangle; other states use full bounds.
/// `title` is borrowed encoded text obeying `textDrawString`'s contract,
/// initially in the small face (printable bytes 0x20..0x7A). Requires resident
/// UI/font textures, glyph storage, the backing's POLY_FT4-sized reservation,
/// any separator POLY_FT4, and writable signed panel OT base/base+1 tags.
/// The panel's OT halfword is temporarily decremented and restored modulo 65536;
/// packets remain in the arena until GPU completion.
void uiDrawTitle(UiPanel* panel, const char* title);

void Ui_SetListScrollFlag(UiList* list, s32 arg1);

/// Queues an opaque fill one pixel inside a content-relative rectangle.
///
/// `left`/`top` and `width`/`height` are pixel edge spans. The TILE starts at
/// (left+1, top+1), measuring (width-1) by (height-1). Zero `colorWord` or width
/// below two skips drawing; height is unchecked. RGB bytes run low to high;
/// the high byte participates in the zero test, then becomes the TILE command.
/// Coordinates and dimensions retain their low 16 bits without clamping.
/// Borrows the panel and needs one TILE's aligned arena space and a writable
/// signed panel OT base+1 tag. Retain the packet until GPU completion.
void uiFillRectInterior(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord);

/// Queues the UI texture-page and blend-mode selection at a signed OT index.
///
/// Selects the 4-bit page at VRAM (896,256), enables dithering and disables
/// drawing to the displayed buffer. Low two bits of `blendMode` select the
/// GPU blend equation (0 average, 1 add, 2 subtract, 3 add quarter foreground).
/// Does not load textures or select a palette. Requires one DR_TPAGE's aligned
/// arena space and a writable `otIndex` tag; retain the packet through GPU use.
void uiQueueTexturePage(s32 otIndex, s32 blendMode);

/// Bevel orientation in bit zero; other selector bits are ignored.
enum {
    USER_INTERFACE_RECT_RECESSED_BEVEL = 0,
    USER_INTERFACE_RECT_RAISED_BEVEL   = 1
};

/// Queues an optional interior fill and the light/dark edges of a beveled rectangle.
///
/// Pixel coordinates and edge spans are relative to the borrowed panel's
/// content origin. Bit zero of `raisedBevel` selects the edge orientation:
/// zero is recessed (dark top/left), one raised (light top/left).
/// The fill has `uiFillRectInterior`'s zero-color and width guards; both bevel
/// polylines are always drawn, even with zero or negative spans. Coordinates
/// narrow to 16 bits. Requires arena space for two LINE_F3s and any fill TILE,
/// and a writable signed panel OT base+1 tag; retain packets through GPU use.
void uiDrawBeveledRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord, s32 raisedBevel);

/// Queues a recessed rectangle with dark top/left and light bottom/right edges.
///
/// Borrows `panel`; all coordinates/spans, fill guards and packet lifetime
/// requirements are those of `uiDrawBeveledRect`.
void uiDrawRecessedRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord);

/// Queues a raised rectangle with light top/left and dark bottom/right edges.
///
/// Borrows `panel`; all coordinates/spans, fill guards and packet lifetime
/// requirements are those of `uiDrawBeveledRect`.
void uiDrawRaisedRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord);

void Ui_WaitCdThenOverlay(Task* task);

/// Direction flags accepted by `uiDrawFlatCaret`; any nonzero value points down.
enum {
    USER_INTERFACE_CARET_UP   = 0,
    USER_INTERFACE_CARET_DOWN = 1
};

/// Queues a flat triangular caret at a panel-relative tip position.
///
/// `tipX`/`tipY` are content pixels, narrowed through unsigned halfwords after
/// adding the panel origin. Up has base offsets (-4,+5) and (+5,+5); down has
/// (-3,-4) and (+4,-4). The 24-bit RGB word (R low byte) is doubled as a whole
/// before the GPU command byte is set, retaining carries between channels.
///
/// Borrows the live panel without testing its state or changing it. Requires
/// word-aligned arena space for a POLY_G3-sized reservation and a writable
/// signed panel OT base+1 tag. Writes a POLY_F3; the unused reservation tail
/// and packet must remain intact until the GPU consumes the ordering table.
void uiDrawFlatCaret(const UiPanel* panel, s32 tipX, s32 tipY, u32 colorRgb, s32 pointsDown);

#endif // MAIN_UI_H
