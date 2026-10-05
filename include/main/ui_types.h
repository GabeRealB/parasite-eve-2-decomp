#ifndef MAIN_UI_TYPES_H
#define MAIN_UI_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/task_types.h"

struct UiList;

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

/// The selection cursor's position in screen-centered integer pixels.
///
/// Halfword views select signed coordinates or unsigned arithmetic before
/// promotion; both retain the same coordinate bits.
typedef struct {
    UiHalf x; // Horizontal coordinate relative to the screen center, in pixels
    UiHalf y; // Vertical coordinate relative to the screen center, in pixels
} UiCursorPosition;
STATIC_ASSERT_SIZEOF(UiCursorPosition, 4);

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

/// Initial panel layout and owning-task seeds for a task-owned UI object.
///
/// `Ui_SpawnFromDesc` reads this recipe synchronously and retains no pointer to
/// it. Spawned entries require a non-NULL `contentCallback` whose code remains
/// live for the task's lifetime. Empty descriptor-table rows must not be spawned.
///
/// `bounds` supplies the unanimated outer rectangle in pixels relative to the
/// screen center; `style` uses the complete `UiPanel.style` encoding. Spawning
/// clears the low two bits of `otIndexSeed` and stores the remaining 16 bits as
/// the panel's signed ordering-table base. Indices count tags, not bytes. The
/// base and every drawing offset must fit the currently selected ordering table.
///
/// Task flags, priority and metadata seed a `TaskDesc`, separately from the
/// caller's spawn payload. The task dispatches the panel's lifecycle before
/// invoking its content handler with the owning task.
typedef struct {
    s32      style;           // Initial packed panel style and flags
    RECT     bounds;          // Initial outer rectangle: signed x/y and width/height in pixels
    u16      otIndexSeed;     // Ordering-table tag seed; low two bits discarded
    u16      field_E;         // Unread halfword; role unproven
    u16      taskFlags;       // Complete flags: body kind (0 none, 1 TMD model, 2 coordinate body); bit 8 skips automatic model buffers
    u16      taskPriority;    // Execution priority seed; low byte used, ascending order
    TaskFunc contentCallback; // Required panel content handler; receives the owning task
    s32      taskDataValue;   // Task descriptor metadata word; ignored for bodyless spawns
} UiObjectDesc;
STATIC_ASSERT_SIZEOF(UiObjectDesc, 0x1C);

/// One selectable row of an option dialog, linked in display order.
///
/// The caller builds the list and passes its head in the `UiOptionDialogRequest`
/// given to `Ui_SpawnTextBlock`. The dialog shows one option per list row and
/// reports a confirmed row as its one-based position in the list. Nodes are
/// reached by stepping `next` a counted number of times, never by testing for a
/// terminator, so the list must hold at least the request's `optionCount` nodes.
/// The dialog keeps only the pointers: the nodes and their strings must stay
/// valid until it closes.
typedef struct UiDialogOption {
    u8*                    text; // Row label, one line in the large UI face; also measured to size the panel
    struct UiDialogOption* next; // Option on the following row; the last counted node's link is never dereferenced
} UiDialogOption;
STATIC_ASSERT_SIZEOF(UiDialogOption, 0x8);

/// Flags in `UiOptionDialogRequest.flags`; storage remains an unsigned byte.
enum {
    USER_INTERFACE_OPTION_DIALOG_CANCELLABLE = 1 // Cancel and menu buttons close the dialog with result -1
};

/// A caller's request for an option dialog, and the place its answer arrives.
///
/// The caller fills the request and passes it to `Ui_SpawnTextBlock`, which
/// opens a screen-centred panel listing one option per row, widened to the
/// longest label. A count of zero or less opens nothing. Either way the call
/// clears `result`, and the caller polls it: it stays zero until the player
/// confirms a row or, where the request permits it, cancels.
///
/// The dialog keeps the pointer for as long as it is open and writes the answer
/// through it, so the request, its options and its strings must outlive the
/// dialog. The row count is taken from the low eight bits of `optionCount`.
typedef struct {
    s16             optionCount; // Options listed, one row each (1..255); zero or less opens no dialog
    s16             result;      // Answer: 0 none yet, 1..optionCount the confirmed option, -1 cancelled
    UiDialogOption* options;     // Head of the option list in row order; holds at least `optionCount` nodes
    char*           title;       // Heading drawn at the panel's top-left corner; NULL opens a panel without one
    u8              flags;       // Bit 0 permits cancelling; no other bit is read
} UiOptionDialogRequest;
STATIC_ASSERT_SIZEOF(UiOptionDialogRequest, 0x10);

/// Draws one visible list row and handles its permitted input.
///
/// `list` supplies the current row in `currentItemIndex`, panel-relative pixel
/// coordinates in `rowTextX` / `rowTextY`, and a packed RGB text color in `colorRgb`.
/// Handlers gate row input on `rowInputEnabled == 1`; they may change the list state,
/// publish `object` results, or open child panels through `object->owner`.
/// Both arguments are borrowed mutable objects that remain live throughout the
/// call; `object` is the task-owned UI node whose panel displays the list.
///
/// `UiList.rowCallbacks` must remain live while the list is dispatched. When
/// `flags & USER_INTERFACE_LIST_SHARED_ROW_CALLBACK` is set, entry zero handles
/// every row; otherwise the table must contain a non-null callback for every
/// reachable row index below `itemCount`.
typedef void (*UiListRowCallback)(struct UiList* list, UiObject* object);

/// Dispatch and sound flags in `UiList.flags`; storage remains an unsigned byte.
enum {
    USER_INTERFACE_LIST_SHARED_ROW_CALLBACK = 1, // Entry zero draws every item
    USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND = 2  // System cursor sound instead of menu cursor sound
};

/// Row input permission in `UiList.rowInputEnabled`; storage remains s32.
enum {
    USER_INTERFACE_LIST_ROW_INACTIVE = 0,
    USER_INTERFACE_LIST_ROW_ACTIVE   = 1
};

/// Signed item steps used by list navigation and animated scrolling.
enum {
    USER_INTERFACE_LIST_STEP_PREVIOUS = -1,
    USER_INTERFACE_LIST_STEP_NONE     = 0,
    USER_INTERFACE_LIST_STEP_NEXT     = 1
};

/// Transient list actions; zero and confirm reuse `USER_INTERFACE_RESULT_*`.
///
/// The row walker clears `actionResult` before dispatch. Skip-row is consumed by
/// navigation in the same update, using `navigationStep` (defaulting to next).
enum {
    USER_INTERFACE_LIST_ACTION_AT_START       = 2,
    USER_INTERFACE_LIST_ACTION_AT_END         = 3,
    USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED = 0x20,
    USER_INTERFACE_LIST_ACTION_MOVE           = 0x23,
    USER_INTERFACE_LIST_ACTION_SKIP_ROW       = 0x41
};

/// Dialog answers in `UiList.commandResult`, returned with a confirm action.
enum {
    USER_INTERFACE_LIST_COMMAND_NONE   = 0,
    USER_INTERFACE_LIST_COMMAND_YES    = 0x33,
    USER_INTERFACE_LIST_COMMAND_NO     = 0x34,
    USER_INTERFACE_LIST_COMMAND_CANCEL = 0x35,
    USER_INTERFACE_LIST_COMMAND_OK     = 0x36
};

/// Row heights in pixels; preview rows suppress the generic highlight and cursor.
enum {
    USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT = 10,
    USER_INTERFACE_LIST_PREVIEW_ROW_HEIGHT = 46
};

/// A panel's list control: row dispatch, selection, scrolling and drawing state.
///
/// Item contents belong to the row callbacks' context. The callback table is
/// borrowed and mutable; it must remain live for every dispatch. Empty lists
/// dispatch no rows. Reachable item indices must fit a nonnegative signed byte
/// (0..127), and visible row counts must fit 0..127 when drawing. During scrolling
/// the walker can dispatch one extra row and wrap its index through `itemCount`.
/// Byte stores retain eight bits; the signed views also detect negative scroll
/// intermediates. Callers must keep those intermediates representable.
///
/// Row coordinates are pixels relative to the panel's content origin. Their
/// halfword views preserve signed offsets and unsigned sums before promotion;
/// stores retain sixteen bits. Callbacks may adjust the row Y pen and text color.
/// The walker enables row input for the selected row of an active panel when
/// scrolling is idle; callbacks may suppress it. Results last one list update.
typedef struct UiList {
    UiListRowCallback* rowCallbacks;          // Per-item callbacks, or entry zero with the shared-callback flag
    u8                 itemCount;             // Number of items, including zero for an empty list
    UiByte             visibleRowCount;       // Rows fitting the panel; signed view used by the row walker
    s8                 wrapNavigation;        // Navigation policy (0 clamp and page, nonzero wrap)
    s8                 rowHeight;             // Row spacing in pixels; zero requests the default height
    s8                 currentItemIndex;      // Absolute item index supplied to the current row callback
    UiByte             firstVisibleItemIndex; // Absolute item index at the top of the scrolling window
    u8                 flags;                 // Shared callback (bit 0) and system cursor sound (bit 1)
    s8                 navigationStep;        // Direction used for row skipping (-1 previous, 0 unset, 1 next)
    s32                rowInputEnabled;       // Current row input permission (0 inactive, 1 active)
    s32                selectedItemIndex;     // Selection index; may be -1 when empty or pass either end during navigation
    s16                scrollPixelsRemaining; // Remaining animated displacement in pixels; positive values count down
    s8                 scrollDirection;       // Animated item step (-1 previous, 0 idle, 1 next)
    s8                 topInset;              // Pixels reserved above the rows, deducted from available height
    UiHalf             rowTextX;              // Current row text X relative to the panel's content origin, in pixels
    UiHalf             rowTextY;              // Current row text Y pen relative to the content origin, in pixels
    u32                colorRgb;              // Row text modulation RGB in bits 0..23, red in the low byte
    UiHalf             commandResult;         // Dialog answer (0 none, 0x33 yes, 0x34 no, 0x35 cancel, 0x36 OK)
    s16                actionResult;          // Transient list action, including 0 none and 6 confirm
} UiList;
STATIC_ASSERT_SIZEOF(UiList, 0x24);

#endif // MAIN_UI_TYPES_H
