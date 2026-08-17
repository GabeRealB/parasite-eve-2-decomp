#ifndef UI_H
#define UI_H

#include "common.h"

#include <psyq/libgpu.h>

#include "main/task.h"

// Types — UI layout / dialogs

/// Object passed to Ui_UpdateListNoAnim / Ui_SmoothCursor (e.g. via Task::spawnArg2).
typedef struct _UiMiniObj {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte unknown_4[0x18];
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  unknown_1e;
    /* 0x20 */ s16  field_20;
    /* 0x22 */ s16  field_22;
} UiMiniObj;
STATIC_ASSERT_SIZEOF(UiMiniObj, 0x24);

/// Object at Task::spawnArg2 used by Ui_TeardownTree / Mc_HideChildUi /
/// Mc_DrawPrompt / Ui_SpawnFromDesc. Shares the UiPanel layout through offset
/// 0x24 (handlers cast field_20 to UiPanel*). field_0 is a status flag;
/// field_4 is copied from UiObjectDesc::field_0 at spawn; field_8 is a mode
/// (5 = skip draw in Text_DrawPrompt / Text_DrawMultiLine; set to 3 when torn down); field_C..field_12
/// are layout halfwords (RECT-like); field_14 is a halfword counter used as the
/// text draw priority/order; field_16 is a signed timer/counter; field_18/field_1A
/// are layout offsets (shared with UiPanel; used when positioning child UI);
/// field_1C is a position halfword (+2 when passed to Text_DrawPrompt); field_1E is
/// an x offset paired with field_20; field_20/field_22 are base x/y for relative
/// text placement; field_24 is a callback copied from the descriptor; field_28 is
/// the owning Task*; field_2C / field_2E are halfwords polled by teardown state
/// handlers (e.g. GameFlow_WaitMenuDone waits until field_2E == -1 before cleaning up;
/// dialog pickers set field_2E == 6 when a choice is confirmed).
typedef struct _UiObject {
    /* 0x00 */ s32   status;
    /* 0x04 */ s32   field_4; // from UiObjectDesc::field_0
    /* 0x08 */ s32   mode;    // 5=skip draw, 3=torn down
    /* 0x0C */ u16   field_C; // layout (RECT-like)
    /* 0x0E */ u16   field_E;
    /* 0x10 */ u16   field_10;
    /* 0x12 */ u16   field_12;
    /* 0x14 */ u16   drawOrder;
    /* 0x16 */ s16   timer;
    /* 0x18 */ u16   field_18; // layout offset
    /* 0x1A */ u16   field_1A; // layout offset
    /* 0x1C */ s16   field_1C; // position (+2 for text draw)
    /* 0x1E */ u16   field_1E; // x offset with baseX
    /* 0x20 */ u16   baseX;
    /* 0x22 */ u16   baseY;
    /* 0x24 */ s32   callback;
    /* 0x28 */ Task* owner;
    /* 0x2C */ s16   field_2C; // teardown / choice
    /* 0x2E */ s16   field_2E; // teardown / choice (-1 wait, 6 confirm)
} UiObject;
STATIC_ASSERT_SIZEOF(UiObject, 0x30);

/// Template/descriptor consumed by Ui_SpawnFromDesc to spawn a UiObject + Task.
typedef struct _UiObjectDesc {
    /* 0x00 */ s32 field_0; // → UiObject.field_4
    /* 0x04 */ u16 field_4; // → layout
    /* 0x06 */ u16 field_6;
    /* 0x08 */ u16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ u16 field_10; // → TaskDesc seed
    /* 0x12 */ u16 field_12; // → TaskDesc seed
    /* 0x14 */ s32 field_14; // → UiObject callback-ish
    /* 0x18 */ s32 field_18; // → TaskDesc seed
} UiObjectDesc;
STATIC_ASSERT_SIZEOF(UiObjectDesc, 0x1C);

/// Singly-linked text line node used by TextBlockDesc / Ui_SpawnTextBlock.
typedef struct TextLineNode {
    /* 0x0 */ u8*                  text;
    /* 0x4 */ struct TextLineNode* next;
} TextLineNode;

/// Multi-line text block descriptor consumed by Ui_SpawnTextBlock to spawn a
/// sized UiObject. field_0 is the line count; field_2 is cleared on return;
/// field_4 is the head of a TextLineNode list; field_8 selects layout mode
/// (0 forces UiObject::field_4 = 3).
typedef struct TextBlockDesc {
    /* 0x0 */ s16           count;
    /* 0x2 */ s16           field_2;
    /* 0x4 */ TextLineNode* lines;
    /* 0x8 */ s32           field_8;
} TextBlockDesc;
STATIC_ASSERT_SIZEOF(TextBlockDesc, 0xC);

/// UI list/menu object (data symbols D_8006116C, D_80061194, D_8006125C,
/// D_80061284, D_800612AC, D_80067654; size 0x24).
/// field_0 is a function-table pointer; field_4 / field_5 are base indices
/// (Ui_ListTaskCallback seeds both from context); field_5 is also subtracted when
/// computing field_9; field_6 / field_7 are signed layout sizes (Ui_DrawListHighlight
/// uses field_7 as TILE height); field_9 / field_A / field_10 are list cursor /
/// flag / selection index used by McMenu_SelectList / McMenu_SelectListAlt / McMenu_InitByMode /
/// Ui_InitList / Ui_SetListScrollFlag; field_C / field_14 / field_16 are cleared by
/// Ui_InitList; field_17 is a signed layout adjust subtracted from the child
/// height when computing visible rows (Ui_ComputeVisibleRows / Ui_ComputeVisibleRowsEx; the latter
/// also writes field_17 from its third argument). field_22 is a selected action
/// code polled by list-task handlers (`func_800CB188`: 0x20 skips pad input,
/// 0x23 is copied to UiObject::field_2E; same values DialogPrompt handlers
/// write to DialogPrompt::field_22).
typedef struct _UiList {
    /* 0x00 */ byte unknown_0[0x4]; // often function-table pointer
    /* 0x04 */ u8   field_4;        // base index
    /* 0x05 */ u8   field_5;        // base index (also used vs field_9)
    /* 0x06 */ s8   field_6;        // layout size
    /* 0x07 */ s8   field_7;        // TILE height / row height
    /* 0x08 */ byte unknown_8;
    /* 0x09 */ u8   field_9;        // list cursor (visible offset)
    /* 0x0A */ u8   field_A;        // flag
    /* 0x0B */ byte unknown_B;
    /* 0x0C */ s32  field_C;        // cleared by list reset
    /* 0x10 */ s32  field_10;       // selection index
    /* 0x14 */ s16  field_14;       // cleared by list reset
    /* 0x16 */ s8   field_16;       // cleared by list reset
    /* 0x17 */ s8   field_17;       // layout adjust for visible rows
    /* 0x18 */ byte unknown_18[0xA];
    /* 0x22 */ s16  field_22;       // selected action (0x20 skip pad, 0x23 confirm)
} UiList;
STATIC_ASSERT_SIZEOF(UiList, 0x24);

/// WIP: Task::spawnArg1 context for D_8006121C select-menu (McMenu_SelectListAlt).
/// Only field_290 is used so far (seeds UiList cursor).
typedef struct _WipSelectMenuExt {
    /* 0x000 */ byte unknown_0[0x290];
    /* 0x290 */ s32  field_290;
} WipSelectMenuExt;

/// WIP: nested object reached via WipUiHolder::field_28 (Ui_SetHolderParam writes field_34).
typedef struct _WipUiChild {
    /* 0x00 */ byte unknown_0[0x34];
    /* 0x34 */ s32  field_34;
} WipUiChild;

/// WIP: UI holder pointer Wip_UiHolder; field_28 → WipUiChild.
typedef struct _WipUiHolder {
    /* 0x00 */ byte        unknown_0[0x28];
    /* 0x28 */ WipUiChild* field_28;
} WipUiHolder;

extern WipUiHolder* Wip_UiHolder;
extern UiList       D_8006116C;
extern UiList       D_80061194;
extern UiList       D_8006125C;
extern UiList       D_80061284;
extern UiList       D_800612AC;
extern UiList       D_80067654;
extern UiObjectDesc D_800608F4[];
extern UiObjectDesc D_800611C8[];
extern UiObjectDesc D_80061200[];
extern UiObjectDesc D_8006121C[];
extern UiObjectDesc D_800612D0[];
extern UiObjectDesc D_80067678;
extern char         D_80013B64[]; // "Select"

/// Object used by 34E98.c handlers (e.g. Ui_AnimOpenStep / D_80013F2C table).
/// field_4 low nibble selects layout padding (Ui_InsetLayout); high nibble of the
/// low byte selects a fill mode (Ui_ScaleRect). field_8 is a small integer
/// state; field_C is a source RECT used by layout helpers (Ui_InsetLayout /
/// Ui_ComputeAnimRect); field_14 is a halfword counter temporarily adjusted around
/// text draw (Ui_DrawTextAtLayout); field_16 is a signed counter/timer;
/// field_18..field_22 are layout offsets (Ui_ClampDialogRect / Ui_InsetLayout);
/// field_24 is a callback invoked with the second handler argument.
typedef struct _UiPanel {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;  // low nibble layout pad; high nibble fill mode
    /* 0x08 */ s32  field_8;  // small integer state
    /* 0x0C */ RECT field_C;  // source RECT for layout
    /* 0x14 */ u16  field_14; // halfword counter (text draw)
    /* 0x16 */ s16  field_16; // timer/counter
    /* 0x18 */ u16  field_18; // layout offset
    /* 0x1A */ u16  field_1A; // layout offset
    /* 0x1C */ u16  field_1C; // layout (signed in some overlays)
    /* 0x1E */ u16  field_1E; // layout (signed in some overlays)
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ void (*field_24)(void*); // handler callback
} UiPanel;

/// Callback for UiPanel state handlers (e.g. entries in D_80013F2C).
typedef void (*UiPanelFunc)(UiPanel* arg0, void* arg1);

/// Fixed-size table of UiPanelFunc callbacks. Copied onto the stack by
/// Ui_DispatchObjectState so the call uses a local jump table.
typedef struct {
    UiPanelFunc funcs[6];
} UiPanelFuncTable6;

extern UiPanelFuncTable6 D_80013F2C;

/// Dialog / prompt descriptor used by 21FDC.c handlers (e.g. McMenu_ConfirmDialogAlt,
/// McMenu_ConfirmDialog, McMenu_ConfirmWithRender). field_8 is a signed menu/option index passed
/// to rendering helpers; field_B is a flag written on the alternate confirm
/// path; field_C is a selection/confirm flag (1 = confirm); field_10 is compared
/// to field_8 (`func_800CEF68`); field_18/field_1A are position halfwords;
/// field_1C is data passed through to Text_DrawPrompt; field_20/field_22 are
/// state halfwords set on confirm (field_22 also on the alternate confirm path).
typedef struct _DialogPrompt {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte unknown_9[0x2];
    /* 0x0B */ s8   field_B;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ byte unknown_14[0x4];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ s16  field_20;
    /* 0x22 */ s16  field_22;
} DialogPrompt;
STATIC_ASSERT_SIZEOF(DialogPrompt, 0x24);

/// Linked text option node walked by Ui_DrawDialogLine (index via DialogPrompt::field_8).
/// field_0 is the string passed to Text_DrawPrompt; field_4 is the next node.
typedef struct _DialogOption {
    /* 0x0 */ u8*                   text;
    /* 0x4 */ struct _DialogOption* next;
} DialogOption;
STATIC_ASSERT_SIZEOF(DialogOption, 0x8);

/// Context at Task::spawnArg1 for the Ui_DrawDialogLine dialog path.
/// field_4 is the head of a DialogOption list; field_C bit0 gates cancel input.
typedef struct _DialogListCtx {
    /* 0x00 */ byte          unknown_0[4];
    /* 0x04 */ DialogOption* field_4;
    /* 0x08 */ byte          unknown_8[4];
    /* 0x0C */ u8            field_C;
} DialogListCtx;

/// Context at Task::spawnArg1 for the Ui_ListTaskCallback UI path.
/// field_0 is a base index copied into UiList field_4/field_5; field_2 receives
/// the selected index from UiObject::field_2C on confirm/cancel; field_8 is an
/// optional string passed to Ui_DrawText.
typedef struct _SelectMenuCtx {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ byte  pad_1;
    /* 0x02 */ s16   field_2;
    /* 0x04 */ byte  pad_4[4];
    /* 0x08 */ char* field_8;
} SelectMenuCtx;

// --- APIs (from unknown_syms) ---
UiObject* Ui_SpawnTextBlock(TextBlockDesc* arg0);
UiObject* Ui_SpawnFromDesc(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3, UiObject* arg4);
void      Ui_SizeFromText(UiPanel* arg0, u8* arg1, s32 arg2, s32 arg3);
void      Ui_SizeFromTextPlain(UiPanel* arg0, u8* arg1);
void      Ui_UpdateLayoutSize(UiPanel* arg0, s32 arg1, s32 arg2);
void      Ui_TeardownTree(UiObject* arg0, Task* arg1);
void      Ui_FreeAndKill(Task* arg0);
void      Ui_SetState4(Task* arg0, Task* arg1);
s32       Ui_IsStateDone(Task* arg0);
void      Ui_DrawTextColored(UiPanel* arg0, char* arg1);
void      Ui_DrawText(UiPanel* arg0, char* arg1);
void      Ui_InsetLayout(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);
void      Ui_ClampDialogRect(UiPanel* arg0, UiPanel* arg1, UiPanel* arg2);
void      Ui_SetHolderParam(s32 arg0, s32 arg1, s32 arg2);
void      Ui_SetHolderParamAlt(s32 arg0, s32 arg1, s32 arg2);
void      Ui_ClampAnimOrClose(UiPanel* arg0, s32 arg1, s32 arg2);
void      Ui_StartCloseAnim(UiPanel* arg0, void* arg1);
void      Ui_DrawCaret(UiList* arg0, UiPanel* arg1, s32 arg2);
void      Ui_LayoutListPanel(UiList* arg0, UiPanel* arg1);
void      Ui_InitList(UiList* arg0, UiMiniObj* arg1);
void      Ui_ComputeVisibleRows(UiList* arg0, s32 arg1);
void      Ui_UpdateListNoAnim(void* arg0, void* arg1);
void      Ui_ComputeVisibleRowsEx(UiList* arg0, UiPanel* arg1, s32 arg2);
void      Ui_SmoothCursor(UiMiniObj* arg0, s32 arg1, s32 arg2);
s32       Ui_GetCursorFixed(void);
s32       Ui_LookupTable(void* arg0, s32 arg1);
s32       Ui_Scale15(s32 arg0);
void      Ui_DrawHBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void      Ui_DrawVBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void      Ui_DrawTextUnderline(UiPanel* arg0, s32 arg1, s32 arg2, char* arg3, s32 arg4);
void      Ui_DrawTitle(UiPanel* arg0, char* arg1);
void      Ui_DrawTextAtLayout(UiPanel* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);
void      Ui_SetListScrollFlag(UiList* arg0, s32 arg1);
void      Ui_AllocTile(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
void      Ui_DrawListHighlight(UiList* arg0, UiPanel* arg1, s32 arg2);
void      Ui_LayoutWithMode0(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
void      Ui_LayoutWithMode1(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
void      Ui_SetupClip(UiPanel* arg0);
void      Ui_ScaleRect(UiPanel* arg0, RECT* arg1, s32 arg2, s32 arg3);
void      Ui_LayoutAndClip(UiPanel* arg0);
void      Ui_LayoutAndDraw(UiPanel* arg0);
void      Ui_LayoutAndDrawAlt(UiPanel* arg0);
void      Ui_ComputeAnimRect(UiPanel* arg0, RECT* arg1);
void      Ui_AnimOpenStep(UiPanel* arg0, void* arg1);
void      Ui_DrawAndCallback(UiPanel* arg0, void* arg1);
void      Ui_LayoutDrawAndCallback(UiPanel* arg0, void* arg1);
void      Ui_TickAnimCounter(UiPanel* arg0, void* arg1);
void      Ui_ClipAndCallback(UiPanel* arg0, void* arg1);
void      Ui_WaitCdThenOverlay(Task* arg0);

#endif // UI_H
