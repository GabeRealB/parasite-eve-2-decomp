#ifndef MAIN_UI_TYPES_H
#define MAIN_UI_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/task_types.h"

struct _UiList;

/// A UI halfword with signed and unsigned numeric views of the same 16 bits.
///
/// The signed view supports negative panel coordinates and ordering-table
/// indices; the unsigned view supports layout arithmetic and list results.
/// Select a view before integer promotion; stores retain the low 16 bits.
typedef union {
    u16 unsignedValue; // Numeric view in the range 0..65535
    s16 signedValue;   // Numeric view in the range -32768..32767
} UiHalf;
STATIC_ASSERT_SIZEOF(UiHalf, 2);

/// A UI byte with signed and unsigned numeric views of the same eight bits.
///
/// List controls use these views for row counts and scroll indices, including
/// negative intermediate scroll positions. Select a view before integer
/// promotion; stores retain the low eight bits.
typedef union {
    u8 unsignedValue; // Numeric view in the range 0..255
    s8 signedValue;   // Numeric view in the range -128..127
} UiByte;
STATIC_ASSERT_SIZEOF(UiByte, 1);

/// Panel lifecycle indices used by the task dispatcher; storage remains s32.
enum {
    USER_INTERFACE_PANEL_INITIAL = 0,
    /// Fully open panel, independent of its input control mode.
    ///
    /// Drawing uses the full panel bounds. Task dispatch draws and calls the
    /// content handler without advancing `UiPanel.animationTicks` or suspending
    /// `UiPanel.control`. Opening selects this state after its content callback
    /// when ticks reach zero, if the callback kept the opening state. The first
    /// full-size dispatch is on the next update.
    USER_INTERFACE_PANEL_OPEN = 2,
    /// Panel shrinking before its owning task exits and releases the UI object.
    ///
    /// Entering this state preserves `animationTicks`. Nonnegative counters
    /// advance by elapsed frame ticks; reaching `USER_INTERFACE_PANEL_ANIMATION_TICKS`
    /// or a negative counter invokes the task's exit callback. Until completion,
    /// drawing and the content callback still run with inactive input.
    USER_INTERFACE_PANEL_CLOSING = 3,
    /// Retained panel with drawing suppressed and its content callback still running.
    ///
    /// Dispatch suspends input without releasing the panel or its owning task.
    /// Positive `animationTicks` count down to `USER_INTERFACE_PANEL_ANIMATION_TICKS`
    /// before reopening; negative values wait for active control after the callback.
    /// Zero keeps the panel hidden until a callback or caller changes its state/counter.
    USER_INTERFACE_PANEL_HIDDEN = 5
};

/// Common control modes; other values belong to the panel's content controller.
enum {
    USER_INTERFACE_PANEL_INACTIVE       = 0,
    USER_INTERFACE_PANEL_ACTIVE         = 1,
    USER_INTERFACE_PANEL_REQUEST_MIN    = 2,
    USER_INTERFACE_PANEL_FOCUS_TRANSFER = 23
};

/// Style fields and flags in the panel's signed 32-bit style word.
enum {
    USER_INTERFACE_PANEL_TITLE_STYLE     = 2,
    USER_INTERFACE_PANEL_DIMMED          = 0x10000,
    USER_INTERFACE_PANEL_SCREEN_BRIGHTEN = 0x20000
};

/// Panel transition span and hidden-delay bias, in nominal 60-Hz ticks.
///
/// Counters advance by `gDisplayState.frameTicks` per update. A full opening
/// counts from this value down to zero; closing and hiding use it as their
/// completion threshold. A positive hidden delay stores its ticks plus this
/// bias and counts down to this value before reopening.
///
/// Drawing subtracts `animationTicks` from this value to obtain a scale in
/// eighths, where eight is full size. Opening raises nonpositive scales to one;
/// closing and hiding replace scales outside 1..8 with one. The ninth tick
/// marks the lifecycle boundary beyond those eight scale steps.
enum { USER_INTERFACE_PANEL_ANIMATION_TICKS = 9 };

/// Unsigned sign-bit mask suppressing frame drawing while content still runs.
#define USER_INTERFACE_PANEL_NO_FRAME 0x80000000

/// A UI panel's drawing bounds, centered content coordinates and lifecycle.
///
/// Bounds and content coordinates are pixels relative to the screen center.
/// Adding contentOriginX/Y maps a content coordinate to that screen coordinate
/// system. Signed and unsigned halfword views select promotion before arithmetic;
/// stores retain the low 16 bits. The ordering-table base counts tags, not bytes:
/// generic drawing uses base..base+3, and specialized consumers use other offsets.
/// Every resulting index must fit the table selected by `gGpuCurrentOt`.
///
/// The control word is 0 for inactive input, 1 for active input, or a content
/// controller's request/status. Opening, hiding and hidden dispatch temporarily
/// shift it into the upper halfword to suspend input while retaining the mode
/// for drawing; a callback's changed word is preserved instead of restored.
/// Animation ticks count elapsed frame ticks: opening counts 9 down to 0,
/// closing/hiding count up to 9, and hidden delays count down to 9. A negative
/// hidden counter waits for active input; the same sentinel ends closing/hiding.
///
/// Task-owned panels are embedded in `UiObject`; their required content callback
/// borrows the owning task after layout/clipping. Standalone drawing helpers
/// require only the fields they read and do not confer task ownership.
typedef struct {
    union {
        s32 word;             // Complete input mode or controller request/status
        struct {
            s16 current;      // Low half: current mode, cleared while input is suspended
            s16 suspended;    // High half: saved mode during suspended dispatch
        } modes;              // Control modes before and during suspended dispatch
    } control;                // Whole-word and halfword views of the input control
    s32 style;                // Low nibble: 2 title, 4 pulse; high nibble: 1 scale full height; upper flag bits
    s32 state;                // Lifecycle index (0 initial, 1 opening, 2 open, 3 closing, 4 hiding, 5 hidden)
    union {
        RECT rect;            // Full panel bounds in signed screen-centered pixels
        struct {
            u16 x;            // Zero-extended horizontal position bits
            u16 y;            // Zero-extended vertical position bits
            u16 w;            // Zero-extended width bits
            u16 h;            // Zero-extended height bits
        } unsignedRect;       // Unsigned views of the same rectangle halfwords
    } bounds;                 // Unanimated outer rectangle
    UiHalf   otIndex;         // Signed ordering-table base index, in tags
    s16      animationTicks;  // State-dependent animation/delay counter in frame ticks; negative sentinel
    UiHalf   contentTop;      // Top edge relative to the content center, in pixels
    UiHalf   contentBottom;   // Bottom edge relative to the content center, in pixels
    UiHalf   contentLeft;     // Left edge relative to the content center, in pixels
    UiHalf   contentRight;    // Right edge relative to the content center, in pixels
    UiHalf   contentOriginX;  // Horizontal translation from content to screen-centered pixels
    UiHalf   contentOriginY;  // Vertical translation from content to screen-centered pixels
    TaskFunc contentCallback; // Required task-owned content handler, called after layout/clipping
} UiPanel;
STATIC_ASSERT_SIZEOF(UiPanel, 0x28);

/// Outcome codes published in `UiObject.result`.
///
/// A content task clears the field to none at the start of a frame, then writes
/// a code when input or a child produces an outcome. Confirm carries the
/// selection or answer in `resultValue`. Dismiss acknowledges a notice; a parent
/// may propagate that code or treat it as confirm. Any other stored code is a
/// menu command interpreted by the task that reads it.
enum {
    USER_INTERFACE_RESULT_CANCEL  = -1, // Closed without accepting
    USER_INTERFACE_RESULT_NONE    = 0,  // No outcome yet this frame
    USER_INTERFACE_RESULT_CONFIRM = 6,  // Accepted
    USER_INTERFACE_RESULT_DISMISS = 9   // Acknowledged
};

/// A task-owned user-interface node.
///
/// The node is allocated with its task. `owner` is that task, and the task's
/// second spawn argument points back at the node. The owner's child tasks are
/// this node's children, each with its own node. `panel` is the drawing and
/// input state. Helpers that only need those fields take a `UiPanel`; recover
/// this node with `PARENT_OF` only when that panel is known to be embedded here.
///
/// `result` and `resultValue` are the outcome the node publishes for its parent.
/// While `panel.control` is focus-transfer, `resultValue` instead holds the
/// screen Y used to choose the destination row, and is cleared when the
/// transfer completes.
typedef struct UiObject {
    UiPanel panel;       // Drawing bounds, input mode and lifecycle
    Task*   owner;       // Task that allocated this node
    s16     resultValue; // Selection, item, prompt or answer published with `result`; screen Y during focus transfer
    s16     result;      // Outcome code (none, cancel, confirm, dismiss, or a menu command)
} UiObject;
STATIC_ASSERT_SIZEOF(UiObject, 0x30);
STATIC_ASSERT(OFFSET_OF(UiObject, owner) == 0x28, ui_object_owner_offset);

/// Template/descriptor consumed by Ui_SpawnFromDesc to spawn a UiObject + Task.
typedef struct {
    /* 0x00 */ s32 field_0; // → UiObject.panel.style
    /* 0x04 */ u16 field_4; // → layout
    /* 0x06 */ u16 field_6;
    /* 0x08 */ u16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ u16 field_10;        // → TaskDesc seed
    /* 0x12 */ u16 field_12;        // → TaskDesc seed
    TaskFunc       contentCallback; // Required content handler for the spawned panel's owning task
    /* 0x18 */ s32 field_18;        // → TaskDesc seed
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
/// (0 forces UiObject::panel.style = 3).
typedef struct TextBlockDesc {
    /* 0x0 */ s16           count;
    /* 0x2 */ s16           field_2;
    /* 0x4 */ TextLineNode* lines;
    /* 0x8 */ s32           field_8;
} TextBlockDesc;
STATIC_ASSERT_SIZEOF(TextBlockDesc, 0xC);

typedef void (*UiListItemFunc)(struct _UiList* arg0, UiObject* arg1);

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
/// item id (`lhu`; copied to `UiObject::resultValue` by `Gp_YesNoMenuTask`). field_22
/// is a selected action code polled by list-task handlers (`Gp_ItemCmdMenuTask`:
/// 0x20 skips pad input, 0x23 is copied to `UiObject::result`; 6 is confirm
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
