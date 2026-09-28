#ifndef UI_H
#define UI_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/task.h"

// Types — UI layout / dialogs

/// UI layout values are read as both signed coordinates and unsigned words.
/// Explicit views preserve the original halfword loads without pointer casts.
typedef union {
    u16 u;
    s16 s;
} UiHalf;
typedef union {
    u8 u;
    s8 s;
} UiByte;

/// Panel shared by standalone drawing helpers and the task-owned UiObject.
/// Its signed rectangle and unsigned layout view occupy the same eight bytes.
typedef struct _UiPanel {
    /* 0x00 */ union {
        s32 w;
        s16 h[2];
    } field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ union {
        RECT rect;
        struct {
            u16 x, y, w, h;
        } unsignedRect;
    } bounds;
    /* 0x14 */ UiHalf   field_14;
    /* 0x16 */ s16      field_16;
    /* 0x18 */ UiHalf   field_18;
    /* 0x1A */ UiHalf   field_1A;
    /* 0x1C */ UiHalf   field_1C;
    /* 0x1E */ UiHalf   field_1E;
    /* 0x20 */ UiHalf   field_20;
    /* 0x22 */ UiHalf   field_22;
    /* 0x24 */ TaskFunc field_24;
} UiPanel;
STATIC_ASSERT_SIZEOF(UiPanel, 0x28);

/// A task-owned panel, with its owner and teardown/selection results.
/// Panel helpers also accept standalone UiPanel values, so only paths known
/// to operate on task-owned panels may recover the parent with PARENT_OF.
typedef struct _UiObject {
    /* 0x00 */ UiPanel panel;
    /* 0x28 */ Task*   owner;
    /* 0x2C */ s16     field_2C;
    /* 0x2E */ s16     field_2E;
} UiObject;
STATIC_ASSERT_SIZEOF(UiObject, 0x30);
STATIC_ASSERT(OFFSET_OF(UiObject, owner) == 0x28, ui_object_owner_offset);

/// Template/descriptor consumed by Ui_SpawnFromDesc to spawn a UiObject + Task.
typedef struct _UiObjectDesc {
    /* 0x00 */ s32      field_0; // → UiObject.panel.field_4
    /* 0x04 */ u16      field_4; // → layout
    /* 0x06 */ u16      field_6;
    /* 0x08 */ u16      field_8;
    /* 0x0A */ u16      field_A;
    /* 0x0C */ u16      field_C;
    /* 0x0E */ u16      field_E;
    /* 0x10 */ u16      field_10; // → TaskDesc seed
    /* 0x12 */ u16      field_12; // → TaskDesc seed
    /* 0x14 */ TaskFunc field_14; // → UiObject callback-ish
    /* 0x18 */ s32      field_18; // → TaskDesc seed
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
/// (0 forces UiObject::panel.field_4 = 3).
typedef struct TextBlockDesc {
    /* 0x0 */ s16           count;
    /* 0x2 */ s16           field_2;
    /* 0x4 */ TextLineNode* lines;
    /* 0x8 */ s32           field_8;
} TextBlockDesc;
STATIC_ASSERT_SIZEOF(TextBlockDesc, 0xC);

/// Per-item callbacks pointed to by UiList::funcs (two entries: draw / confirm).
struct _UiList;
typedef void (*UiListItemFunc)(struct _UiList* arg0, struct _UiObject* arg1);

/// UI list/menu object (data symbols D_8006116C, D_80061194, D_8006125C,
/// D_80061284, D_800612AC, D_80067654; size 0x24).
/// funcs is a function-table pointer (`Gp_PeCommandMenuTask` writes draw/confirm
/// handlers into the two slots); field_4 / field_5 are base indices
/// (Ui_ListTaskCallback seeds both from context); field_5 is also subtracted when
/// computing field_9; field_6 / field_7 are signed layout sizes (Ui_DrawListHighlight
/// uses field_7 as TILE height); field_9 / field_A / field_10 are list cursor /
/// flag / selection index used by McMenu_SelectList / McMenu_SelectListAlt / McMenu_InitByMode /
/// Ui_InitList / Ui_SetListScrollFlag; field_C / field_14 / field_16 are cleared by
/// Ui_InitList; field_17 is a signed layout adjust subtracted from the child
/// height when computing visible rows (Ui_ComputeVisibleRows / Ui_ComputeVisibleRowsEx; the latter
/// also writes field_17 from its third argument). field_20 is the selected
/// item id (`lhu`; copied to UiObject::field_2C by `Gp_YesNoMenuTask`). field_22
/// is a selected action code polled by list-task handlers (`Gp_ItemCmdMenuTask`:
/// 0x20 skips pad input, 0x23 is copied to UiObject::field_2E; 6 is confirm
/// in `Gp_YesNoMenuTask`; same values UiList handlers write to
/// UiList::field_22).
typedef struct _UiList {
    /* 0x00 */ UiListItemFunc* funcs;    // function-table pointer
    /* 0x04 */ u8              field_4;  // base index
    /* 0x05 */ UiByte          field_5;  // base index (also used vs field_9)
    /* 0x06 */ s8              field_6;  // layout size
    /* 0x07 */ s8              field_7;  // TILE height / row height
    /* 0x08 */ s8              field_8;
    /* 0x09 */ UiByte          field_9;  // list cursor (visible offset)
    /* 0x0A */ u8              field_A;  // flag
    /* 0x0B */ s8              field_B;
    /* 0x0C */ s32             field_C;  // cleared by list reset
    /* 0x10 */ s32             field_10; // selection index
    /* 0x14 */ s16             field_14; // cleared by list reset
    /* 0x16 */ s8              field_16; // cleared by list reset
    /* 0x17 */ s8              field_17; // layout adjust for visible rows
    /* 0x18 */ s16             field_18;
    /* 0x1A */ s16             field_1A;
    /* 0x1C */ s32             field_1C;
    /* 0x20 */ UiHalf          field_20; // selected item id
    /* 0x22 */ s16             field_22; // selected action (0x20 skip pad, 0x23 confirm)
} UiList;
STATIC_ASSERT_SIZEOF(UiList, 0x24);

/// WIP: Task::spawnArg1 context for D_8006121C select-menu (McMenu_SelectListAlt).
/// Only field_290 is used so far (seeds UiList cursor).
typedef struct _WipSelectMenuExt {
    /* 0x000 */ byte unknown_0[0x290];
    /* 0x290 */ s32  field_290;
} WipSelectMenuExt;

/// Prompt panel whose owning task carries the current text in spawnArg1.
extern UiObject*    Wip_UiHolder;
extern UiList       D_8006116C;
extern UiList       D_80061194;
extern UiObjectDesc D_800611C8[];
extern UiObjectDesc D_800612D0[];

/// Callback for UiPanel state handlers (e.g. entries in Ui_ObjectStates).
typedef void (*UiPanelFunc)(UiPanel* arg0, Task* arg1);

/// Fixed-size table of UiPanelFunc callbacks. Copied onto the stack by
/// Ui_DispatchObjectState so the call uses a local jump table.
typedef struct {
    UiPanelFunc funcs[6];
} UiPanelFuncTable6;

/// Linked text option node walked by Ui_DrawDialogLine (index via UiList::field_8).
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
UiObject* Ui_SpawnTextBlock(TextBlockDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
UiObject* Ui_SpawnFromDesc(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3, UiObject* arg4);
void      Ui_SizeFromText(UiPanel* arg0, u8* arg1, s32 arg2, s32 arg3);
void      Ui_SizeFromTextPlain(UiPanel* arg0, u8* arg1);
void      Ui_SizeFromTextWide(UiPanel* arg0, u8* arg1);
void      Ui_UpdateLayoutSize(UiPanel* arg0, s32 arg1, s32 arg2);
void      Ui_TeardownTree(UiObject* arg0, Task* arg1);
void      Ui_FreeAndKill(Task* arg0);
void      Ui_SetState4(UiObject* arg0, Task* arg1);
s32       Ui_IsStateDone(UiObject* arg0);
void      Ui_DrawTextColored(UiPanel* arg0, char* arg1);
void      Ui_DrawText(UiPanel* arg0, char* arg1);
void      Ui_InsetLayout(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);
void      Ui_ClampDialogRect(UiPanel* arg0, UiList* arg1, UiPanel* arg2);
/// Set the prompt text; the owner task stores it in its mixed spawn payload.
void Ui_SetHolderParam(u8* arg0, s32 arg1, s32 arg2);
/// Set a numeric item id (0x300..0x3FF) for the PE cost prompt.
void Ui_SetHolderParamAlt(s32 arg0, s32 arg1, s32 arg2);
void Ui_ClampAnimOrClose(UiPanel* arg0, Task* arg1, s32 arg2);
void Ui_StartCloseAnim(UiPanel* arg0, Task* arg1);
void Ui_LayoutListPanel(UiList* arg0, UiPanel* arg1);
void Ui_InitList(UiList* arg0, UiPanel* arg1);
void Ui_ComputeVisibleRows(UiList* arg0, UiPanel* arg1);
void Ui_UpdateListNoAnim(void* arg0, void* arg1);
void Ui_SmoothCursor(UiPanel* arg0, s32 arg1, s32 arg2);
s32  Ui_GetCursorFixed(void);
s32  Ui_LookupTable(void* arg0, s32 arg1);
s32  Ui_Scale15(s32 arg0);
void Ui_DrawHBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void Ui_DrawVBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void Ui_DrawTextInRect(RECT* arg0, s32 arg1, s32 arg2, char* arg3);
void Ui_DrawTitle(UiPanel* arg0, char* arg1);
void Ui_SetListScrollFlag(UiList* arg0, s32 arg1);
void Ui_AllocTile(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
void Ui_InsertDrawTPage(s32 arg0, s32 arg1);
void func_80046B34(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
void Ui_LayoutWithMode0(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
void Ui_LayoutWithMode1(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
void Ui_WaitCdThenOverlay(Task* arg0);

// Functions defined in this module but not previously declared anywhere.
// Without a prototype m2c cannot type a call to them and the decompiled
// seed fails to compile ('invalid use of void expression') - the single
// largest cause of unusable seeds in the bulk m2c pass.

void Ui_DrawFlatCaret(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void McMenu_ConfirmWithRender(UiList* arg0, UiObject* arg1);
void McMenu_SelectList(Task* arg0);
void McMenu_SelectListAlt(Task* arg0);
void McMenu_FileInformation(Task* arg0);

#endif // UI_H
