#ifndef MAIN_UI_TYPES_H
#define MAIN_UI_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/task_types.h"

struct _UiList;
struct _UiObject;

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

typedef void (*UiListItemFunc)(struct _UiList* arg0, struct _UiObject* arg1);

/// UI list/menu object (data symbols Mc_SaveSlotList, Mc_LoadSlotList, Mc_YesNoList,
/// Mc_OkList, Mc_YesList, Ui_DialogLineList; size 0x24).
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

#endif // MAIN_UI_TYPES_H
