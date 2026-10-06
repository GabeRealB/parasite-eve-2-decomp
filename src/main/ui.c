#include "main/ui.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "text.h"
#include "tmd.h"
#include "main/tmd_types.h"
#include "main/ui_types.h"

#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/item_menu.h"
#include "gameplay/planar_reflection.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

/// Panel expanding into the open state while its content updates with input suspended.
///
/// Counters use nominal 60-Hz ticks. A full opening starts at
/// `USER_INTERFACE_PANEL_ANIMATION_TICKS`; reopening may retain ticks in 0..9.
/// Drawing uses (9 - ticks) eighths, with a minimum scale of one eighth;
/// a zero counter gives a 9/8 scale. Ticks decrease by `gDisplayState.frameTicks`
/// after the content callback and clamp to zero. Completion selects
/// `USER_INTERFACE_PANEL_OPEN` only if the callback kept the opening state;
/// full-size open dispatch begins on the next update. Callback control changes
/// are retained; otherwise the previous input mode is restored.
enum { USER_INTERFACE_PANEL_OPENING = 1 };

/// Panel shrinking into the retained hidden state while its owning task stays alive.
///
/// Entry preserves `UiPanel.animationTicks` and the input control mode.
/// Nonnegative counters advance by elapsed frame ticks. Reaching
/// `USER_INTERFACE_PANEL_ANIMATION_TICKS` or a negative counter sets ticks
/// to -1 and dispatches `USER_INTERFACE_PANEL_HIDDEN` in the same update.
/// Until completion, drawing and the content callback continue with input
/// temporarily suspended; control changes from the callback are preserved.
/// Hidden dispatch can immediately reopen the panel if control is active.
enum { USER_INTERFACE_PANEL_HIDING = 4 };

/// Panel style selectors, list requests and integer scaling used by resident drawing.
enum {
    USER_INTERFACE_PANEL_STYLE_MASK           = 0xF,
    USER_INTERFACE_PANEL_PULSING_STYLE        = 4,
    USER_INTERFACE_PANEL_HEIGHT_MODE          = 1,
    USER_INTERFACE_PANEL_SELECT_LAST_VISIBLE  = 18,
    USER_INTERFACE_PANEL_SELECT_FIRST_VISIBLE = 19,
    USER_INTERFACE_PANEL_SCALE_FRACTION_BITS  = 3,
    USER_INTERFACE_PANEL_SCALE_ONE            = 1 << USER_INTERFACE_PANEL_SCALE_FRACTION_BITS,
    USER_INTERFACE_PANEL_ANIMATION_STOPPED    = -1,
    USER_INTERFACE_PANEL_OT_GROUP_MASK        = 0xFFFC
};

/// Animated list displacement per nominal 60-Hz tick, in pixels.
enum { USER_INTERFACE_LIST_SCROLL_PIXELS_PER_TICK = 2 };

/// Cursor accumulators use eight fractional bits and quarter-distance easing.
enum {
    USER_INTERFACE_CURSOR_FRACTION_BITS = 8,
    USER_INTERFACE_CURSOR_EASING_SHIFT  = 2
};

/// Spreads a triangle's base around its tip using the UI caret's pixel geometry.
///
/// The packet must expose signed 16-bit x1/x2/y0/y1/y2 fields, with x1 and x2
/// already at the tip's X. Zero points up (-4/+5 X, +5 Y); nonzero points down
/// (-3/+4 X, -4 Y). The current y0 supplies the base, including any tip animation.
/// Stores retain sixteen bits. caretValue is used repeatedly and must be a
/// stable pointer without side effects; directionValue is evaluated once.
/// baseYValue is a writable u16 lvalue without side effects, used repeatedly.
/// Captures no caller locals; use as a standalone statement in a compound block,
/// never as an unbraced conditional or loop body.
#define USER_INTERFACE_SET_CARET_BASE(caretValue, directionValue, baseYValue) \
    if ((directionValue) == USER_INTERFACE_CARET_UP) {                        \
        (caretValue)->x1 -= 4;                                                \
        (baseYValue)      = (caretValue)->y0 + 5;                             \
        (caretValue)->x2 += 5;                                                \
        (caretValue)->y2  = (baseYValue);                                     \
        (caretValue)->y1  = (baseYValue);                                     \
    } else {                                                                  \
        (caretValue)->x1 -= 3;                                                \
        (baseYValue)      = (caretValue)->y0 - 4;                             \
        (caretValue)->x2 += 4;                                                \
        (caretValue)->y2  = (baseYValue);                                     \
        (caretValue)->y1  = (baseYValue);                                     \
    }

/// Focus colors for the animated underlined panel labels, red in the low byte.
enum {
    USER_INTERFACE_PANEL_LABEL_INACTIVE_COLOR = GPU_PACK_COLOR_WORD(64, 80, 80, 0),
    USER_INTERFACE_PANEL_LABEL_ACTIVE_COLOR   = GPU_PACK_COLOR_WORD(32, 96, 128, 0)
};

/// Handler for one panel lifecycle index.
///
/// The owning task's update selects the handler with `UiPanel.state` and calls
/// it with the panel and that task. `panel` is the panel embedded at the start
/// of the `UiObject` stored in `Task::spawnArg2`, so the pointer addresses that
/// object. `task` is the object's owner and the argument passed to the panel's
/// content callback.
///
/// The index runs from 0 through 5 (initial, opening, open, closing, hiding,
/// hidden). Dispatch does not check it, and the selected handler is live. The
/// handler may change the panel's lifecycle, animation counter and input
/// control, or run its content callback. Both pointers are live on entry.
/// Exiting the owning task can release them before the handler returns.
typedef void (*_UiPanelLifecycleFunc)(UiPanel* panel, Task* task);

/// Six panel lifecycle handlers stored as a value for a whole-table copy.
///
/// Dispatch copies the table and calls the `funcs` entry selected by
/// `UiPanel.state`. The index is not bounds-checked. Each entry is a
/// `_UiPanelLifecycleFunc`, so the handler receives the panel and its owning
/// task. The copy duplicates those pointers only; the handlers must remain
/// loaded.
typedef struct {
    _UiPanelLifecycleFunc funcs[6]; // One handler per lifecycle (0 initial, 1 opening, 2 open, 3 closing, 4 hiding, 5 hidden)
} _UiPanelLifecycleFuncTable6;
STATIC_ASSERT_SIZEOF(_UiPanelLifecycleFuncTable6, 0x18);

extern TmdSource D_8072C8F0;

extern TmdSource D_8075BED4;

static u8 McLocation_WhereAmI[];

static u8 McLocation_Square[];

static u8 McLocation_FireEscape[];

static u8 McLocation_MistParking[];

static u8 McLocation_GasStation[];

static u8 McLocation_TrailerCoach[];

static u8 McLocation_MotelLobby[];

static u8 McLocation_Refuge[];

static u8 McLocation_SterilizationRoom[];

static u8 McLocation_UndergroundParking[];

static u8 McLocation_Laboratory[];

static u8 McLocation_IncineratorControlRoom[];

static u8 McLocation_PodDeck[];

static u8 McLocation_Nursery[];

static u8 McLocation_Tent[];

static u8 McLocation_Opening[];

static u8 McLocation_MotelRoom6[];

/* Ｓｑｕａｒｅ */
static u8 McTitleLocation_Square[];

/* Ｆｉｒｅ　Ｅｓｃａｐｅ */
static u8 McTitleLocation_FireEscape[];

/* ＭＩＳＴ　Ｐａｒｋｉｎｇ */
static u8 McTitleLocation_MistParking[];

/* Ｇａｓ　Ｓｔａｔｉｏｎ */
static u8 McTitleLocation_GasStation[];

/* Ｔｒａｉｌｅｒ　Ｃｏａｃｈ */
static u8 McTitleLocation_TrailerCoach[];

/* Ｍｏｔｅｌ　Ｌｏｂｂｙ */
static u8 McTitleLocation_MotelLobby[];

/* Ｒｅｆｕｇｅ */
static u8 McTitleLocation_Refuge[];

/* Ｓｔｅｒｉｌｉｚａｔｉｏｎ　Ｒｍ． */
static u8 McTitleLocation_SterilizationRm[];

/* Ｕｎｄｅｒｇｒｏｕｎｄ　Ｐａｒｋ． */
static u8 McTitleLocation_UndergroundPark[];

/* Ｌａｂｏｒａｔｏｒｙ */
static u8 McTitleLocation_Laboratory[];

/* Ｉｎｃｉｎ．　Ｃｏｎｔｒｏｌ */
static u8 McTitleLocation_IncinControl[];

/* Ｐｏｄ　Ｄｅｃｋ */
static u8 McTitleLocation_PodDeck[];

/* Ｎｕｒｓｅｒｙ */
static u8 McTitleLocation_Nursery[];

/* Ｔｅｎｔ */
static u8 McTitleLocation_Tent[];

/* Ｏｐｅｎｉｎｇ */
static u8 McTitleLocation_Opening[];

/* Ｍｏｔｅｌ　Ｒｏｏｍ　６ */
static u8 McTitleLocation_MotelRoom6[];

/* Ｗｈｅｒｅ　ａｍ　Ｉ？ */
static u8 McTitleLocation_WhereAmI[];

/// Unreferenced.
static s32 D_80067638;

static s32 D_8006763C[1];

static s32 D_80067640;

/// Unreferenced.
static s32 D_80067644;

static s32 D_80067648;

static s32 D_8006764C;

static UiListRowCallback Ui_DialogLineCallbacks[];

static UiList Ui_DialogLineList;

static UiObjectDesc Ui_DialogListDesc;

static const _UiPanelLifecycleFuncTable6 Ui_ObjectStates;

void func_80707534(Task* arg0);

void func_807075A0(Task* arg0);

void func_807077C0(Task* arg0);

void func_80707870(Task* arg0);

void func_80707980(Task* arg0);

void func_80707B14(Task* arg0);

void func_80707C38(Task* arg0);

void func_80707F84(Task* arg0);

void func_80708070(Task* arg0);

void func_807080C8(Task* arg0);

void func_80708778(Task* arg0);

void func_8070A6E8(Task* arg0);

void func_807127A8(Task* arg0);

void func_807146AC(Task* arg0);

void func_8071473C(Task* arg0);

void func_8071489C(Task* arg0);

void func_807149F0(Task* arg0);

void func_80714A48(Task* arg0);

extern void func_801D4B64(Task* arg0);

static void _taskNoOpCallback(Task* unusedTask);

static inline u32 _uiGrey(s32 level);

static void _uiDrawPanelBackground(const RECT* rect, s32 style, s32 otIndex);

static void _uiDrawPanelFrame(const UiPanel* panel, RECT* outer, const RECT* inner, s32 unusedClipFrame);

static void _uiDrawPanel(const UiPanel* panel, RECT* outerRect, const RECT* innerRect, s32 clipFrame);

static void _uiLayoutHiddenPanel(UiPanel* panel);

static void _uiComputeScaledPanelRect(const UiPanel* panel, RECT* rect, s32 scaleEighths, s32 unusedClosing);

static void _uiLayoutOpeningPanel(UiPanel* panel);

static void _uiLayoutOpenPanel(UiPanel* panel);

static void _uiLayoutShrinkingPanel(UiPanel* panel);

/// Drawing layers, UI atlas encodings and framebuffer coordinates used by panels.
enum {
    USER_INTERFACE_FRAME_OT_OFFSET          = 3,
    USER_INTERFACE_FRAME_ATLAS_TPAGE        = 0x1E,
    USER_INTERFACE_FRAME_CLUT               = 0x3C03,
    USER_INTERFACE_FRAME_ATLAS_TOP          = 0x50,
    USER_INTERFACE_FRAME_CORNER_PIXELS      = 8,
    USER_INTERFACE_PANEL_DRAW_VIEW_WIDTH    = 320,
    USER_INTERFACE_PANEL_DRAW_VIEW_HEIGHT   = 240,
    USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_X = 160,
    USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_Y = 120,
    USER_INTERFACE_PANEL_DRAW_BUFFER_STRIDE = 272,
    USER_INTERFACE_PANEL_DITHERED_DRAW_MODE = 0xE1000200,
    USER_INTERFACE_OPENING_CONTROL_SHIFT    = 16
};

/// Computes a nonzero pulsing grey using the caller's signed shade temporary.
///
/// GNU expression result is a u32 RGB word. `baseShadeValue` and `pulseValue`
/// are evaluated once; `shadeValue` must be a side-effect-free s32 lvalue,
/// distinct from the inputs. It is overwritten and evaluated repeatedly.
/// Captures no identifiers; the caller retains the shade's lifetime.
#define USER_INTERFACE_PULSING_BACKGROUND_COLOR(baseShadeValue, pulseValue, shadeValue) \
    ({                                                                                  \
        (shadeValue) = (baseShadeValue) - (pulseValue);                                 \
        if ((shadeValue) <= 0) {                                                        \
            (shadeValue) = 1;                                                           \
        }                                                                               \
        _uiGrey(shadeValue);                                                            \
    })

/// Saves input control, suspends opening input, then draws and runs content.
///
/// Standalone statement within a compound block. Panel/task arguments must be
/// stable pointers without side effects; savedControlValue is a writable s32
/// lvalue without side effects. Panel and saved-control arguments are used
/// repeatedly; taskValue is used once. Captures no locals; the caller owns the
/// saved value through the counter/restore phase.
#define USER_INTERFACE_RUN_OPENING_PANEL_CONTENT(panelValue, taskValue, savedControlValue)        \
    do {                                                                                          \
        (savedControlValue)        = (panelValue)->control.word;                                  \
        (panelValue)->control.word = (savedControlValue) << USER_INTERFACE_OPENING_CONTROL_SHIFT; \
        _uiLayoutOpeningPanel(panelValue);                                                        \
        (panelValue)->contentCallback(taskValue);                                                 \
    } while (0)

static void _uiQueueListDrawArea(const UiList* list, const UiPanel* panel, s32 fullScreen);

static void _uiDrawAnimatedCursor(const UiPanel* panel, s32 contentX, s32 contentY);

static void _uiDrawListOverflowCaret(const UiList* list, const UiPanel* panel, s32 pointsDown);

static inline void _uiFillRectInterior(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord);

static void _uiDrawListHighlight(const UiList* list, UiPanel* panel, s32 rowBottom, s32 unused);

static inline void _uiListMoveCursor(const UiPanel* panel, s32 contentX, s32 contentY);

static void _uiUpdateListRows(UiList* list, UiPanel* panel, s32 inputPort);

static void _uiDrawUnderlinedLabel(const UiPanel* panel, s32 x, s32 y, const char* text, u32 colorRgb);

/// Allocates a UI object and its task, optionally under the given parent.
///
/// Takes a `TaskSpawnArg` payload value and copies its word into the task;
/// pointer payload storage must live as long as the callback uses it. The
/// control mode is copied as s32 and animation ticks narrow to s16. Returns
/// NULL when either allocation fails, releasing the task if necessary.
///
/// Arguments are evaluated once, left to right. They must not refer to the
/// macro's `_uiSpawn...` locals. The GNU statement expression keeps allocation
/// inline while accepting a complete union value with GCC 2.8.1.
#define USER_INTERFACE_SPAWN_OBJECT(descriptorValue, payloadValue, panelModeValue, animationTicksValue, parentValue)                \
    ({                                                                                                                              \
        const UiObjectDesc* _uiSpawnDescriptor     = (descriptorValue);                                                             \
        TaskSpawnArg        _uiSpawnPayload        = (payloadValue);                                                                \
        s32                 _uiSpawnPanelMode      = (panelModeValue);                                                              \
        s32                 _uiSpawnAnimationTicks = (animationTicksValue);                                                         \
        UiObject*           _uiSpawnParent         = (parentValue);                                                                 \
        TaskDesc            _uiSpawnTaskDesc;                                                                                       \
        Task*               _uiSpawnTask;                                                                                           \
        UiObject*           _uiSpawnResult;                                                                                         \
        s32                 _uiSpawnDescriptorArg;                                                                                  \
                                                                                                                                    \
        _uiSpawnResult                          = NULL;                                                                             \
        _uiSpawnTaskDesc.header.fields.flags    = _uiSpawnDescriptor->taskFlags;                                                    \
        _uiSpawnTaskDesc.header.fields.priority = _uiSpawnDescriptor->taskPriority;                                                 \
        _uiSpawnDescriptorArg                   = _uiSpawnDescriptor->taskDataValue;                                                \
        _uiSpawnTaskDesc.callback               = _uiDispatchPanelLifecycle;                                                        \
        _uiSpawnTaskDesc.data.value             = _uiSpawnDescriptorArg;                                                            \
        _uiSpawnTask                            = taskSpawnFromTable(&_uiSpawnTaskDesc, 0, _uiSpawnPayload, _uiSpawnResult);        \
        if (_uiSpawnTask != NULL) {                                                                                                 \
            _uiSpawnResult = memCalloc(sizeof(*_uiSpawnResult), 0);                                                                 \
            if (_uiSpawnResult != NULL) {                                                                                           \
                _uiSpawnTask->spawnArg2.pointer             = _uiSpawnResult;                                                       \
                _uiSpawnTask->exitCallback                  = uiObjectTaskExit;                                                     \
                _uiSpawnResult->owner                       = _uiSpawnTask;                                                         \
                _uiSpawnResult->panel.control.word          = _uiSpawnPanelMode;                                                    \
                _uiSpawnResult->panel.style                 = _uiSpawnDescriptor->style;                                            \
                _uiSpawnResult->panel.bounds.unsignedRect.x = _uiSpawnDescriptor->bounds.x;                                         \
                _uiSpawnResult->panel.bounds.unsignedRect.y = _uiSpawnDescriptor->bounds.y;                                         \
                _uiSpawnResult->panel.bounds.unsignedRect.w = _uiSpawnDescriptor->bounds.w;                                         \
                _uiSpawnResult->panel.bounds.unsignedRect.h = _uiSpawnDescriptor->bounds.h;                                         \
                _uiSpawnResult->panel.otIndex.signedValue   = _uiSpawnDescriptor->otIndexSeed & USER_INTERFACE_PANEL_OT_GROUP_MASK; \
                _uiSpawnResult->panel.contentCallback       = _uiSpawnDescriptor->contentCallback;                                  \
                _uiSpawnResult->panel.animationTicks        = _uiSpawnAnimationTicks;                                               \
                if (_uiSpawnParent != NULL) {                                                                                       \
                    taskReparent(_uiSpawnParent->owner, _uiSpawnTask);                                                              \
                }                                                                                                                   \
            } else {                                                                                                                \
                taskKill(_uiSpawnTask);                                                                                             \
            }                                                                                                                       \
        }                                                                                                                           \
        _uiSpawnResult;                                                                                                             \
    })

static void _uiRefreshListViewportWithInset(UiList* list, const UiPanel* panel, s32 topInsetPixels);

static void _uiDrawOpenPanelText(UiPanel* panel, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment);

static void _uiComputePanelInnerRect(const UiPanel* unusedPanel, const RECT* outerRect, RECT* innerRect);

static void _uiComputeAnimatedPanelRect(const UiPanel* panel, RECT* rect);

/// Applies content padding and publishes centered edges from a frame-inset rectangle.
///
/// Requires a stable UiPanel pointer and a writable RECT lvalue; arguments are
/// used repeatedly and must have no side effects. Ordered unsigned halfword
/// stores retain the original coordinate truncation and translation.
/// Expands to several statements: invoke only as a standalone statement within
/// a compound block, never as an unbraced conditional or loop body. Keeping the
/// assignments in the caller's block allows packet setup to interleave with them.
#define USER_INTERFACE_CENTER_PANEL_CONTENT(panelValue, contentRectValue)                                        \
    if (((panelValue)->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {           \
        (contentRectValue).y += 9;                                                                               \
        (contentRectValue).h -= 0xB;                                                                             \
        (contentRectValue).x += 2;                                                                               \
        (contentRectValue).w -= 4;                                                                               \
    } else {                                                                                                     \
        (contentRectValue).y += 2;                                                                               \
        (contentRectValue).h -= 4;                                                                               \
        (contentRectValue).x += 2;                                                                               \
        (contentRectValue).w -= 4;                                                                               \
    }                                                                                                            \
    (panelValue)->contentLeft.unsignedValue    = -((contentRectValue).w >> 1);                                   \
    (panelValue)->contentRight.unsignedValue   = (panelValue)->contentLeft.unsignedValue + (contentRectValue).w; \
    (panelValue)->contentTop.unsignedValue     = -((contentRectValue).h >> 1);                                   \
    (panelValue)->contentBottom.unsignedValue  = (panelValue)->contentTop.unsignedValue + (contentRectValue).h;  \
    (panelValue)->contentOriginX.unsignedValue = (contentRectValue).x - (panelValue)->contentLeft.unsignedValue; \
    (panelValue)->contentOriginY.unsignedValue = (contentRectValue).y - (panelValue)->contentTop.unsignedValue;

/// Applies list padding and centers signed content edges from a frame-inset rectangle.
///
/// Arguments must be a stable UiPanel pointer and writable RECT lvalue without
/// side effects; both are used repeatedly. Captures no caller identifiers.
/// Stores narrow to halfwords; signed edge reads preserve the translation.
/// Use as a standalone statement; the caller retains the rectangle's lifetime.
#define USER_INTERFACE_CENTER_LIST_PANEL_CONTENT(panelValue, contentRectValue)                                     \
    do {                                                                                                           \
        if (((panelValue)->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {         \
            (contentRectValue).y += 9;                                                                             \
            (contentRectValue).h -= 0xB;                                                                           \
            (contentRectValue).x += 2;                                                                             \
            (contentRectValue).w -= 4;                                                                             \
        } else {                                                                                                   \
            (contentRectValue).y += 2;                                                                             \
            (contentRectValue).h -= 4;                                                                             \
            (contentRectValue).x += 2;                                                                             \
            (contentRectValue).w -= 4;                                                                             \
        }                                                                                                          \
        (panelValue)->contentLeft.signedValue      = -((contentRectValue).w >> 1);                                 \
        (panelValue)->contentRight.signedValue     = (panelValue)->contentLeft.signedValue + (contentRectValue).w; \
        (panelValue)->contentTop.signedValue       = -((contentRectValue).h >> 1);                                 \
        (panelValue)->contentBottom.signedValue    = (panelValue)->contentTop.signedValue + (contentRectValue).h;  \
        (panelValue)->contentOriginX.unsignedValue = (contentRectValue).x - (panelValue)->contentLeft.signedValue; \
        (panelValue)->contentOriginY.unsignedValue = (contentRectValue).y - (panelValue)->contentTop.signedValue;  \
    } while (0)

/// Publishes one row's index/baseline and invokes its required drawing/input callback.
///
/// listValue/panelValue are stable pointers; the panel belongs to the UiObject
/// passed to the callback. Index and row bottom are stable s32 values in items
/// and content-relative pixels. textInsetValue/rowHeightValue are distinct s32
/// lvalues overwritten here; the inset remains live for the caller to restore
/// callback-adjusted rowTextY after inspecting actionResult. All arguments must
/// be side-effect-free and are used repeatedly; captures no caller identifiers.
/// The callback table obeys UiList's bounds. Use only as a standalone statement
/// within a compound block, never as an unbraced conditional or loop body.
#define USER_INTERFACE_DISPATCH_LIST_ROW(listValue, panelValue, itemIndexValue, rowBottomValue, textInsetValue, rowHeightValue) \
    (textInsetValue)              = 0;                                                                                          \
    (rowHeightValue)              = (listValue)->rowHeight;                                                                     \
    (listValue)->currentItemIndex = (itemIndexValue);                                                                           \
    if ((rowHeightValue) == USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT) {                                                           \
        (textInsetValue) = 3;                                                                                                   \
    } else if ((rowHeightValue) < USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT) {                                                     \
        (textInsetValue) = 2;                                                                                                   \
    } else if ((rowHeightValue) >= 16) {                                                                                        \
        (textInsetValue) = (rowHeightValue) - 15;                                                                               \
    }                                                                                                                           \
    (listValue)->rowTextY.signedValue = (rowBottomValue) - (textInsetValue);                                                    \
    if ((listValue)->flags & USER_INTERFACE_LIST_SHARED_ROW_CALLBACK) {                                                         \
        (listValue)->rowCallbacks[0]((listValue), PARENT_OF((panelValue), UiObject, panel));                                    \
    } else {                                                                                                                    \
        (listValue)->rowCallbacks[(itemIndexValue)]((listValue), PARENT_OF((panelValue), UiObject, panel));                     \
    }

static void _uiPanelInitial(UiPanel* panel, Task* owningTask);

static void _uiPanelOpening(UiPanel* panel, Task* owningTask);

static void _uiPanelOpen(UiPanel* panel, Task* owningTask);

static void _uiPanelClosing(UiPanel* panel, Task* owningTask);

static void _uiPanelHiding(UiPanel* panel, Task* owningTask);

static void _uiPanelHidden(UiPanel* panel, Task* task);

static void _uiDispatchPanelLifecycle(Task* owningTask);

static void Ui_DrawDialogLine(UiList* list, UiObject* object);

static void Ui_ListTaskCallback(Task* task);

TaskDesc D_800670D0[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80714A48 },
    { { { TASK_BODY_TMD, 0xC0 } }, func_80707534, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_807075A0 },
    { { { TASK_BODY_TMD, 0xC0 } }, func_807077C0, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_807080C8 },
    { { { TASK_BODY_COORD, 0xC0 } }, func_80707870 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80707980 },
    { { { TASK_BODY_NONE, 0x60 } }, enemyTeardownDelayTask },
    { { { TASK_BODY_TMD, 0x40 } }, func_807077C0, { &D_8075BED4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x41 } }, worldCoordUpdateRoomLightsTask },
    { { { TASK_BODY_NONE, 0x51 } }, worldCoordPlayerLightingTask },
    { { { TASK_BODY_COORD, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807146AC },
    { { { TASK_BODY_TMD, 0xC0 } }, func_8071473C, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_8071489C, { &D_8072C8F0 } },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_TMD, 0xC0 } }, func_807149F0, { &D_8075BED4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80707B14 },
    { { { TASK_BODY_NONE, 0x2F } }, fadePulseTask },
    { { { TASK_BODY_COORD, 0x60 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80707C38 },
    { { { TASK_BODY_TMD, 0xC0 } }, func_80707F84, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_80708070 },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0xC0 } }, tmdReleaseAttachedBuffersTask },
    { { { TASK_BODY_NONE, 0xC0 } }, tmdRestoreAttachedBuffersTask },
    { { { TASK_BODY_NONE, 0x60 } }, sceneManagerTask },
    { { { TASK_BODY_NONE, 0xC0 } }, _taskNoOpCallback },
    { { { TASK_BODY_NONE, 0x70 } }, planarReflectionDispatchPlayerTask },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800CE22C },
    { { { TASK_BODY_NONE, 0xC2 } }, fadeDisplayTransitionTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807127A8 },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x70 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800B65B0 },
    { { { TASK_BODY_NONE, 0xC0 } }, displayBlendPreviousFrameTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskRunExitCallbackTask },
    { { { TASK_BODY_NONE, 0xC0 } }, func_8070A6E8 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80708778 },
    { { { TASK_BODY_NONE, 0x2F } }, fadeScreenTask },
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffAttachTask37 },
};

static u8 McLocation_WhereAmI[]               = "Where am I?";
static u8 McLocation_Square[]                 = "Square";
static u8 McLocation_FireEscape[]             = "Fire Escape";
static u8 McLocation_MistParking[]            = "MIST Parking";
static u8 McLocation_GasStation[]             = "Gas Station";
static u8 McLocation_TrailerCoach[]           = "Trailer Coach";
static u8 McLocation_MotelLobby[]             = "Motel Lobby";
static u8 McLocation_Refuge[]                 = "Refuge";
static u8 McLocation_SterilizationRoom[]      = "Sterilization Room";
static u8 McLocation_UndergroundParking[]     = "Underground Parking";
static u8 McLocation_Laboratory[]             = "Laboratory";
static u8 McLocation_IncineratorControlRoom[] = "Incinerator Control Room";
static u8 McLocation_PodDeck[]                = "Pod Deck";
static u8 McLocation_Nursery[]                = "Nursery";
static u8 McLocation_Tent[]                   = "Tent";
static u8 McLocation_Opening[]                = "Opening";
static u8 McLocation_MotelRoom6[]             = "Motel Room 6";

u8* Mc_LocationLabels[] = {
    McLocation_WhereAmI,
    McLocation_Square,
    McLocation_FireEscape,
    McLocation_MistParking,
    McLocation_GasStation,
    McLocation_TrailerCoach,
    McLocation_MotelLobby,
    McLocation_Refuge,
    McLocation_SterilizationRoom,
    McLocation_UndergroundParking,
    McLocation_Laboratory,
    McLocation_IncineratorControlRoom,
    McLocation_PodDeck,
    McLocation_Nursery,
    McLocation_Tent,
    McLocation_Opening,
    McLocation_MotelRoom6,
};

/* Ｓｑｕａｒｅ */
static u8 McTitleLocation_Square[] = "\x82\x72\x82\x91\x82\x95\x82\x81\x82\x92\x82\x85";
/* Ｆｉｒｅ　Ｅｓｃａｐｅ */
static u8 McTitleLocation_FireEscape[] = "\x82\x65\x82\x89\x82\x92\x82\x85\x81\x40\x82\x64\x82\x93\x82\x83\x82\x81\x82\x90\x82\x85";
/* ＭＩＳＴ　Ｐａｒｋｉｎｇ */
static u8 McTitleLocation_MistParking[] = "\x82\x6C\x82\x68\x82\x72\x82\x73\x81\x40\x82\x6F\x82\x81\x82\x92\x82\x8B\x82\x89\x82\x8E\x82\x87";
/* Ｇａｓ　Ｓｔａｔｉｏｎ */
static u8 McTitleLocation_GasStation[] = "\x82\x66\x82\x81\x82\x93\x81\x40\x82\x72\x82\x94\x82\x81\x82\x94\x82\x89\x82\x8F\x82\x8E";
/* Ｔｒａｉｌｅｒ　Ｃｏａｃｈ */
static u8 McTitleLocation_TrailerCoach[] = "\x82\x73\x82\x92\x82\x81\x82\x89\x82\x8C\x82\x85\x82\x92\x81\x40\x82\x62\x82\x8F\x82\x81\x82\x83\x82\x88";
/* Ｍｏｔｅｌ　Ｌｏｂｂｙ */
static u8 McTitleLocation_MotelLobby[] = "\x82\x6C\x82\x8F\x82\x94\x82\x85\x82\x8C\x81\x40\x82\x6B\x82\x8F\x82\x82\x82\x82\x82\x99";
/* Ｒｅｆｕｇｅ */
static u8 McTitleLocation_Refuge[] = "\x82\x71\x82\x85\x82\x86\x82\x95\x82\x87\x82\x85";
/* Ｓｔｅｒｉｌｉｚａｔｉｏｎ　Ｒｍ． */
static u8 McTitleLocation_SterilizationRm[] = "\x82\x72\x82\x94\x82\x85\x82\x92\x82\x89\x82\x8C\x82\x89\x82\x9A\x82\x81\x82\x94\x82\x89\x82\x8F\x82\x8E\x81\x40\x82\x71\x82\x8D\x81\x44";
/* Ｕｎｄｅｒｇｒｏｕｎｄ　Ｐａｒｋ． */
static u8 McTitleLocation_UndergroundPark[] = "\x82\x74\x82\x8E\x82\x84\x82\x85\x82\x92\x82\x87\x82\x92\x82\x8F\x82\x95\x82\x8E\x82\x84\x81\x40\x82\x6F\x82\x81\x82\x92\x82\x8B\x81\x44";
/* Ｌａｂｏｒａｔｏｒｙ */
static u8 McTitleLocation_Laboratory[] = "\x82\x6B\x82\x81\x82\x82\x82\x8F\x82\x92\x82\x81\x82\x94\x82\x8F\x82\x92\x82\x99";
/* Ｉｎｃｉｎ．　Ｃｏｎｔｒｏｌ */
static u8 McTitleLocation_IncinControl[] = "\x82\x68\x82\x8E\x82\x83\x82\x89\x82\x8E\x81\x44\x81\x40\x82\x62\x82\x8F\x82\x8E\x82\x94\x82\x92\x82\x8F\x82\x8C";
/* Ｐｏｄ　Ｄｅｃｋ */
static u8 McTitleLocation_PodDeck[] = "\x82\x6F\x82\x8F\x82\x84\x81\x40\x82\x63\x82\x85\x82\x83\x82\x8B";
/* Ｎｕｒｓｅｒｙ */
static u8 McTitleLocation_Nursery[] = "\x82\x6D\x82\x95\x82\x92\x82\x93\x82\x85\x82\x92\x82\x99";
/* Ｔｅｎｔ */
static u8 McTitleLocation_Tent[] = "\x82\x73\x82\x85\x82\x8E\x82\x94";
/* Ｏｐｅｎｉｎｇ */
static u8 McTitleLocation_Opening[] = "\x82\x6E\x82\x90\x82\x85\x82\x8E\x82\x89\x82\x8E\x82\x87";
/* Ｍｏｔｅｌ　Ｒｏｏｍ　６ */
static u8 McTitleLocation_MotelRoom6[] = "\x82\x6C\x82\x8F\x82\x94\x82\x85\x82\x8C\x81\x40\x82\x71\x82\x8F\x82\x8F\x82\x8D\x81\x40\x82\x55";
/* Ｗｈｅｒｅ　ａｍ　Ｉ？ */
static u8 McTitleLocation_WhereAmI[] = "\x82\x76\x82\x88\x82\x85\x82\x92\x82\x85\x81\x40\x82\x81\x82\x8D\x81\x40\x82\x68\x81\x48";

u8* Mc_LocationTitleLabels[] = {
    McTitleLocation_WhereAmI,
    McTitleLocation_Square,
    McTitleLocation_FireEscape,
    McTitleLocation_MistParking,
    McTitleLocation_GasStation,
    McTitleLocation_TrailerCoach,
    McTitleLocation_MotelLobby,
    McTitleLocation_Refuge,
    McTitleLocation_SterilizationRm,
    McTitleLocation_UndergroundPark,
    McTitleLocation_Laboratory,
    McTitleLocation_IncinControl,
    McTitleLocation_PodDeck,
    McTitleLocation_Nursery,
    McTitleLocation_Tent,
    McTitleLocation_Opening,
    McTitleLocation_MotelRoom6,
};

/// Current item-information panel; separate from the 17 save-point titles.
UiObject* D_80067634 = NULL;
/// Unreferenced.
static s32 D_80067638    = 0x001C2824;
static s32 D_8006763C[1] = { 0x000D287F };
static s32 D_80067640    = 0x00606060;
/// Unreferenced.
static s32 D_80067644 = 0x0038443C;
static s32 D_80067648 = 0xFFFFFF56;
static s32 D_8006764C = 0xFFFFFF7E;

static UiListRowCallback Ui_DialogLineCallbacks[] = { Ui_DrawDialogLine };
static UiList            Ui_DialogLineList        = { Ui_DialogLineCallbacks, 1, 1, 0, 0x0F };
static UiObjectDesc      Ui_DialogListDesc        = { USER_INTERFACE_PANEL_TITLE_STYLE, { -48, -32, 0x60, 0x40 }, 0x20, 0, TASK_BODY_NONE, 0xC0, Ui_ListTaskCallback, 0 };
UiObject*                Wip_UiHolder             = NULL;

static const _UiPanelLifecycleFuncTable6 Ui_ObjectStates = { {
    _uiPanelInitial,
    [USER_INTERFACE_PANEL_OPENING] = _uiPanelOpening,
    [USER_INTERFACE_PANEL_OPEN]    = _uiPanelOpen,
    [USER_INTERFACE_PANEL_CLOSING] = _uiPanelClosing,
    [USER_INTERFACE_PANEL_HIDING]  = _uiPanelHiding,
    _uiPanelHidden,
} };

/// Idle callback for task bank 1, slot 36; leaves the live task and all its state unchanged.
///
/// The dispatcher supplies `unusedTask`; no work, countdown or teardown occurs.
static void _taskNoOpCallback(Task* unusedTask)
{
}

/// Neutral grey GPU colour word from one grey level.
///
/// Red, green and blue each take the low 8 bits of `level`. Bits above that
/// byte are discarded, and the command byte is clear.
static inline u32 _uiGrey(s32 level)
{
    level &= 0xFF;
    return (level << 16) | (level << 8) | level;
}

/// Queues the shaded, repeating-texture interior of a panel at a signed OT tag.
///
/// `rect` is borrowed in screen-centered pixels; sixteen-bit vertex and eight-bit
/// UV stores wrap without clamping. Dimmed style overrides pulsing greys; the
/// low style nibble independently selects the palette. Needs two POLY_GT4 slots
/// even when empty, and two DR_MODE slots when drawable, with a writable otIndex
/// tag and loaded UI textures/palettes. Packets remain live until GPU completion.
static void _uiDrawPanelBackground(const RECT* rect, s32 style, s32 otIndex)
{
    enum {
        USER_INTERFACE_BACKGROUND_CLUT                       = 0x3C0F,
        USER_INTERFACE_BACKGROUND_PULSING_CLUT               = 0x3C84,
        USER_INTERFACE_BACKGROUND_UNRESTRICTED_WINDOW_PIXELS = 255,
        USER_INTERFACE_BACKGROUND_REPEAT_PIXELS              = 32,
        USER_INTERFACE_BACKGROUND_REPEAT_MASK                = USER_INTERFACE_BACKGROUND_REPEAT_PIXELS - 1,
        USER_INTERFACE_BACKGROUND_PULSE_ANGLE_SHIFT          = 6,
        USER_INTERFACE_BACKGROUND_SINE_ONE                   = 4096,
        USER_INTERFACE_BACKGROUND_PULSE_SHADE_SHIFT          = 7,
        USER_INTERFACE_BACKGROUND_BOTTOM_SPLIT_SHADE         = 176,
        USER_INTERFACE_BACKGROUND_BOTTOM_EDGE_SHADE          = 128,
        USER_INTERFACE_BACKGROUND_TOP_SPLIT_SHADE            = 64,
        USER_INTERFACE_BACKGROUND_TOP_EDGE_SHADE             = 48
    };
    RECT      textureWindow;
    POLY_GT4* leftQuad;
    POLY_GT4* rightQuad;
    DR_MODE*  windowCommand;
    s32       pulse;
    s32       shade;

    // Reserve both halves even when the narrowed rectangle has no drawable extent.
    leftQuad  = gGpuPrimCursor;
    rightQuad = leftQuad + 1;

    leftQuad->x0 = leftQuad->x2 = rect->x;
    rightQuad->x0 = rightQuad->x2 = rect->x + rect->w;
    leftQuad->x1 = leftQuad->x3 = rightQuad->x1 = rightQuad->x3 = rect->w >> 1;
    rightQuad->y0 = rightQuad->y1 = leftQuad->y0 = leftQuad->y1 = rect->y + rect->h;
    rightQuad->y2 = rightQuad->y3 = leftQuad->y2 = leftQuad->y3 = rect->y;

    gGpuPrimCursor = leftQuad + 2;
    if (leftQuad->x0 >= rightQuad->x0 || leftQuad->y0 <= leftQuad->y2) {
        return;
    }

    windowCommand  = gGpuPrimCursor;
    gGpuPrimCursor = windowCommand + 1;

    textureWindow.w = textureWindow.h = USER_INTERFACE_BACKGROUND_UNRESTRICTED_WINDOW_PIXELS;
    textureWindow.x = textureWindow.y = 0;
    setTexWindow(windowCommand, &textureWindow);
    addPrim(gGpuCurrentOt + otIndex, windowCommand);

    if ((style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_PULSING_STYLE) {
        leftQuad->tpage  = USER_INTERFACE_FRAME_ATLAS_TPAGE;
        rightQuad->tpage = USER_INTERFACE_FRAME_ATLAS_TPAGE;
        leftQuad->clut   = USER_INTERFACE_BACKGROUND_PULSING_CLUT;
        rightQuad->clut  = USER_INTERFACE_BACKGROUND_PULSING_CLUT;
    } else {
        leftQuad->tpage  = USER_INTERFACE_FRAME_ATLAS_TPAGE;
        rightQuad->tpage = USER_INTERFACE_FRAME_ATLAS_TPAGE;
        leftQuad->clut   = USER_INTERFACE_BACKGROUND_CLUT;
        rightQuad->clut  = USER_INTERFACE_BACKGROUND_CLUT;
    }

    if (style & USER_INTERFACE_PANEL_DIMMED) {
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 1)  = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 1) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 0)  = GPU_PACK_COLOR_WORD(0x50, 0x50, 0x50, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 0) = GPU_PACK_COLOR_WORD(0x50, 0x50, 0x50, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 3)  = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 3) = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 2) = GPU_PACK_COLOR_WORD(0x70, 0x70, 0x70, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 2)  = GPU_PACK_COLOR_WORD(0x70, 0x70, 0x70, 0);
    } else if ((style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_PULSING_STYLE) {
        pulse = (rsin(gDisplayState.animFrame << USER_INTERFACE_BACKGROUND_PULSE_ANGLE_SHIFT) + USER_INTERFACE_BACKGROUND_SINE_ONE) >> USER_INTERFACE_BACKGROUND_PULSE_SHADE_SHIFT;

        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 1) = GPU_PRIMITIVE_COLOR_WORD(leftQuad, 1) = USER_INTERFACE_PULSING_BACKGROUND_COLOR(USER_INTERFACE_BACKGROUND_BOTTOM_SPLIT_SHADE, pulse, shade);

        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 0) = GPU_PRIMITIVE_COLOR_WORD(leftQuad, 0) = USER_INTERFACE_PULSING_BACKGROUND_COLOR(USER_INTERFACE_BACKGROUND_BOTTOM_EDGE_SHADE, pulse, shade);

        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 3) = GPU_PRIMITIVE_COLOR_WORD(leftQuad, 3) = USER_INTERFACE_PULSING_BACKGROUND_COLOR(USER_INTERFACE_BACKGROUND_TOP_SPLIT_SHADE, pulse, shade);

        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 2) = GPU_PRIMITIVE_COLOR_WORD(rightQuad, 2) = USER_INTERFACE_PULSING_BACKGROUND_COLOR(USER_INTERFACE_BACKGROUND_TOP_EDGE_SHADE, pulse, shade);
    } else {
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 1)  = GPU_PACK_COLOR_WORD(0xA8, 0xA8, 0xA8, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 1) = GPU_PACK_COLOR_WORD(0xA8, 0xA8, 0xA8, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 0)  = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 0) = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 3)  = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 3) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
        GPU_PRIMITIVE_COLOR_WORD(rightQuad, 2) = GPU_PACK_COLOR_WORD(0x30, 0x30, 0x30, 0);
        GPU_PRIMITIVE_COLOR_WORD(leftQuad, 2)  = GPU_PACK_COLOR_WORD(0x30, 0x30, 0x30, 0);
    }

    leftQuad->v0 = leftQuad->v1 = 0;
    leftQuad->v2 = leftQuad->v3 = rect->h;
    rightQuad->v0 = rightQuad->v1 = 0;
    rightQuad->v2 = rightQuad->v3 = rect->h;

    // Split at screen X=0; keep eight-bit UV wrap and signed sixteen-bit vertices.
    if (leftQuad->x0 < 0) {
        if (rightQuad->x0 < 0) {
            leftQuad->x1 = leftQuad->x3 = rightQuad->x0;
        } else {
            leftQuad->x1 = leftQuad->x3 = 0;
        }
        setPolyGT4(leftQuad);
        leftQuad->u0 = leftQuad->u2 = 0;
        leftQuad->u1 = leftQuad->u3 = leftQuad->x1 - leftQuad->x0;
        addPrim(gGpuCurrentOt + otIndex, leftQuad);
    }

    if (rightQuad->x0 >= 0) {
        if (leftQuad->x0 >= 0) {
            rightQuad->x1 = rightQuad->x3 = leftQuad->x0;
            rightQuad->u1 = rightQuad->u3 = 0;
        } else {
            rightQuad->x1 = rightQuad->x3 = 0;
            rightQuad->u1 = rightQuad->u3 = leftQuad->u1 & USER_INTERFACE_BACKGROUND_REPEAT_MASK;
        }
        setPolyGT4(rightQuad);
        rightQuad->u0 = rightQuad->u2 = rightQuad->u1 + (rightQuad->x0 - rightQuad->x1);
        addPrim(gGpuCurrentOt + otIndex, rightQuad);
    }

    windowCommand  = gGpuPrimCursor;
    gGpuPrimCursor = windowCommand + 1;
    // OT insertion reverses these commands: enable repeat before drawing, restore after.
    setRECT(&textureWindow, 0, 0, USER_INTERFACE_BACKGROUND_REPEAT_PIXELS, USER_INTERFACE_BACKGROUND_REPEAT_PIXELS);
    setTexWindow(windowCommand, &textureWindow);
    addPrim(gGpuCurrentOt + otIndex, windowCommand);
}

/// Links one unmodulated 8x8 atlas corner at the panel's frame layer.
///
/// Borrows a live panel and an aligned, reserved SPRT_8 with its screen-pixel
/// position already set. textureU is a texture-page texel coordinate, narrowed
/// to u8; V is the atlas's fixed top row. Uses the current GPU texture page,
/// which must select the loaded UI atlas, and the frame palette. Raw texture
/// mode leaves RGB bytes unused. Requires a writable signed panel OT base+3
/// tag; does not move the arena cursor. Keep the packet live until GPU completion.
static inline void _uiQueuePanelFrameCorner(SPRT_8* corner, const UiPanel* panel, s32 textureU)
{
    enum { USER_INTERFACE_FRAME_CORNER_RAW_TEXTURE = 1 };

    corner->u0   = textureU;
    corner->v0   = USER_INTERFACE_FRAME_ATLAS_TOP;
    corner->clut = USER_INTERFACE_FRAME_CLUT;
    setSprt8(corner);
    setShadeTex(corner, USER_INTERFACE_FRAME_CORNER_RAW_TEXTURE);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET, corner);
}

/// Links one unmodulated atlas strip between adjacent frame corners.
///
/// Borrows a live panel and an aligned, reserved POLY_FT4 with its screen-pixel
/// vertices already set. Texture endpoints are texture-page texels, narrowed
/// to u8; the top V is fixed and textureBottom supplies the other V endpoint.
/// Selects the UI atlas page and frame palette. Raw texture mode leaves RGB
/// bytes unused. Requires loaded textures and a writable signed panel OT base+3
/// tag; leaves the cursor unchanged. Keep the packet live until GPU completion.
static inline void _uiQueuePanelFrameEdge(POLY_FT4* edge, const UiPanel* panel, s32 textureLeft, s32 textureRight, s32 textureBottom)
{
    enum { USER_INTERFACE_FRAME_EDGE_RAW_TEXTURE = 1 };

    setUV4(edge, textureLeft, USER_INTERFACE_FRAME_ATLAS_TOP, textureRight, USER_INTERFACE_FRAME_ATLAS_TOP, textureLeft, textureBottom, textureRight, textureBottom);
    edge->tpage = USER_INTERFACE_FRAME_ATLAS_TPAGE;
    edge->clut  = USER_INTERFACE_FRAME_CLUT;
    setPolyFT4(edge);
    setShadeTex(edge, USER_INTERFACE_FRAME_EDGE_RAW_TEXTURE);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET, edge);
}

/// Queues a panel's textured corners, edges and shaded interior.
///
/// Increments the writable outer width/height by one with sixteen-bit wrapping;
/// inner is a separate, borrowed interior rectangle computed before that growth.
/// Reads style and signed OT base; the screen-dimming flag also needs animation
/// ticks. The fourth argument is ignored. Requires loaded UI atlas/palettes and
/// writable base+3 tags. Reserves one SPRT_8, three SPRT-sized corner slots, four
/// POLY_FT4 slots and the background's slots even when some pieces are skipped;
/// screen dimming adds a TILE and DR_TPAGE. Retain packets until GPU completion.
static void _uiDrawPanelFrame(const UiPanel* panel, RECT* outer, const RECT* inner, s32 unusedClipFrame)
{
    enum { USER_INTERFACE_SCREEN_DIM_GREY_PER_TICK = 8 };
    SPRT_8*   cornerSprite;
    POLY_FT4* frameEdge;
    TILE*     screenDimTile;
    DR_TPAGE* blendCommand;
    s16       edgeEnd;
    u16       edgeLeft;
    u16       edgeTop;
    u8        screenDimLevel;

    // Reserve the top-left corner tightly; the other corners retain SPRT-sized slots.
    cornerSprite = gGpuPrimCursor;
    outer->w++;
    outer->h++;
    gGpuPrimCursor   = cornerSprite + 1;
    cornerSprite->x0 = outer->x;
    cornerSprite->y0 = outer->y;
    _uiQueuePanelFrameCorner(cornerSprite, panel, 0);

    cornerSprite     = gGpuPrimCursor;
    gGpuPrimCursor   = (u8*)cornerSprite + sizeof(SPRT);
    cornerSprite->x0 = outer->x + outer->w - USER_INTERFACE_FRAME_CORNER_PIXELS;
    if (cornerSprite->x0 > outer->x) {
        cornerSprite->y0 = outer->y;
        _uiQueuePanelFrameCorner(cornerSprite, panel, 0x10);
    }

    cornerSprite     = gGpuPrimCursor;
    gGpuPrimCursor   = (u8*)cornerSprite + sizeof(SPRT);
    cornerSprite->x0 = outer->x;
    cornerSprite->y0 = outer->y + outer->h - USER_INTERFACE_FRAME_CORNER_PIXELS;
    if (outer->y < cornerSprite->y0) {
        _uiQueuePanelFrameCorner(cornerSprite, panel, 0x28);
    }

    cornerSprite     = gGpuPrimCursor;
    gGpuPrimCursor   = (u8*)cornerSprite + sizeof(SPRT);
    cornerSprite->x0 = outer->x + outer->w - USER_INTERFACE_FRAME_CORNER_PIXELS;
    cornerSprite->y0 = outer->y + outer->h - USER_INTERFACE_FRAME_CORNER_PIXELS;
    if (outer->y < cornerSprite->y0 && cornerSprite->x0 > outer->x) {
        _uiQueuePanelFrameCorner(cornerSprite, panel, 0x38);
    }

    // Queue the four stretched atlas strips between the corner sprites.
    // Stretch the atlas strips only across the remaining span between corners.
    frameEdge      = gGpuPrimCursor;
    gGpuPrimCursor = frameEdge + 1;
    edgeLeft       = outer->x + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x2  = edgeLeft;
    frameEdge->x0  = edgeLeft;
    edgeEnd        = outer->x + outer->w - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x3  = edgeEnd;
    frameEdge->x1  = edgeEnd;
    edgeTop        = outer->y;
    frameEdge->y1  = edgeTop;
    frameEdge->y0  = edgeTop;
    edgeEnd        = outer->y + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y3  = edgeEnd;
    frameEdge->y2  = edgeEnd;
    if (frameEdge->x0 < frameEdge->x1) {
        _uiQueuePanelFrameEdge(frameEdge, panel, 0x8, 0x10, 0x58);
    }

    frameEdge      = gGpuPrimCursor;
    gGpuPrimCursor = frameEdge + 1;
    edgeLeft       = outer->x + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x2  = edgeLeft;
    frameEdge->x0  = edgeLeft;
    edgeEnd        = outer->x + outer->w - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x3  = edgeEnd;
    frameEdge->x1  = edgeEnd;
    edgeTop        = outer->y + outer->h - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y1  = edgeTop;
    frameEdge->y0  = edgeTop;
    edgeEnd        = outer->y + outer->h;
    frameEdge->y3  = edgeEnd;
    frameEdge->y2  = edgeEnd;
    if (frameEdge->x0 < frameEdge->x1 && frameEdge->y0 > outer->y) {
        _uiQueuePanelFrameEdge(frameEdge, panel, 0x30, 0x38, 0x58);
    }

    frameEdge      = gGpuPrimCursor;
    gGpuPrimCursor = frameEdge + 1;
    edgeLeft       = outer->x;
    frameEdge->x2  = edgeLeft;
    frameEdge->x0  = edgeLeft;
    edgeEnd        = edgeLeft + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x3  = edgeEnd;
    frameEdge->x1  = edgeEnd;
    edgeTop        = outer->y + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y1  = edgeTop;
    frameEdge->y0  = edgeTop;
    edgeEnd        = outer->y + outer->h - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y3  = edgeEnd;
    frameEdge->y2  = edgeEnd;
    if (frameEdge->y0 < frameEdge->y2) {
        _uiQueuePanelFrameEdge(frameEdge, panel, 0x18, 0x20, 0x57);
    }

    frameEdge      = gGpuPrimCursor;
    gGpuPrimCursor = frameEdge + 1;
    edgeEnd        = outer->x + outer->w;
    edgeLeft       = edgeEnd - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->x2  = edgeLeft;
    frameEdge->x0  = edgeLeft;
    frameEdge->x3  = edgeEnd;
    frameEdge->x1  = edgeEnd;
    edgeTop        = outer->y + USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y1  = edgeTop;
    frameEdge->y0  = edgeTop;
    edgeEnd        = outer->y + outer->h - USER_INTERFACE_FRAME_CORNER_PIXELS;
    frameEdge->y3  = edgeEnd;
    frameEdge->y2  = edgeEnd;
    if (frameEdge->x0 > outer->x && frameEdge->y0 < frameEdge->y2) {
        _uiQueuePanelFrameEdge(frameEdge, panel, 0x20, 0x28, 0x57);
    }
    _uiDrawPanelBackground(inner, panel->style, panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET);
    // Subtractive blending darkens the screen before this frame is drawn.
    if (panel->style & USER_INTERFACE_PANEL_SCREEN_BRIGHTEN) {
        screenDimTile     = gGpuPrimCursor;
        gGpuPrimCursor    = screenDimTile + 1;
        screenDimLevel    = (USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks) * USER_INTERFACE_SCREEN_DIM_GREY_PER_TICK;
        screenDimTile->b0 = screenDimLevel;
        screenDimTile->g0 = screenDimLevel;
        screenDimTile->r0 = screenDimLevel;
        screenDimTile->x0 = -USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_X;
        screenDimTile->y0 = -USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_Y;
        screenDimTile->w  = USER_INTERFACE_PANEL_DRAW_VIEW_WIDTH;
        screenDimTile->h  = USER_INTERFACE_PANEL_DRAW_VIEW_HEIGHT;
        setTile(screenDimTile);
        setSemiTrans(screenDimTile, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET, screenDimTile);
        blendCommand   = gGpuPrimCursor;
        gGpuPrimCursor = blendCommand + 1;
        setlen(blendCommand, 1);
        blendCommand->code[0] = USER_INTERFACE_PANEL_DITHERED_DRAW_MODE | (GPU_BLEND_SUBTRACT << 5);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET, blendCommand);
    }
}

/// Draws a styled panel frame, optionally clips it, and dims its interior.
///
/// A negative style skips all drawing. The borrowed panel supplies style and
/// signed OT base; frame drawing grows outerRect by one pixel in each extent.
/// Nonzero clipFrame brackets the frame with inner/full-screen draw areas at
/// base+3/base+1. Dimmed style adds a black averaging quad at base. Requires
/// frame resources plus two DR_AREA slots when clipping and a POLY_F4/DR_TPAGE
/// pair when dimmed, and writable base/base+1/base+3 tags. Retain GPU packets.
static void _uiDrawPanel(const UiPanel* panel, RECT* outerRect, const RECT* innerRect, s32 clipFrame)
{
    RECT      frameDrawArea;
    RECT      fullScreenDrawArea;
    POLY_F4*  dimQuad;
    DR_TPAGE* blendCommand;
    u16       innerLeft;
    u16       innerTop;
    u16       innerEnd;

    if (panel->style >= 0) {
        // Higher OT tags execute first; restore the full view before content below +1.
        if (clipFrame != 0) {
            DR_AREA* areaCommand;

            areaCommand    = gGpuPrimCursor;
            gGpuPrimCursor = areaCommand + 1;
            setRECT(&frameDrawArea, innerRect->x + USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_X, innerRect->y + USER_INTERFACE_PANEL_DRAW_VIEW_CENTER_Y, innerRect->w, innerRect->h);
            frameDrawArea.y += gDisplayState.drawBuffer * USER_INTERFACE_PANEL_DRAW_BUFFER_STRIDE;
            SetDrawArea(areaCommand, &frameDrawArea);
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_FRAME_OT_OFFSET, areaCommand);
        }
        _uiDrawPanelFrame(panel, outerRect, innerRect, clipFrame);
        if (clipFrame != 0) {
            DR_AREA* areaCommand;

            areaCommand    = gGpuPrimCursor;
            gGpuPrimCursor = areaCommand + 1;
            setRECT(&fullScreenDrawArea, 0, gDisplayState.drawBuffer * USER_INTERFACE_PANEL_DRAW_BUFFER_STRIDE, USER_INTERFACE_PANEL_DRAW_VIEW_WIDTH, USER_INTERFACE_PANEL_DRAW_VIEW_HEIGHT);
            SetDrawArea(areaCommand, &fullScreenDrawArea);
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, areaCommand);
        }
        // A translucent black quad at the base layer halves the finished interior.
        if (panel->style & USER_INTERFACE_PANEL_DIMMED) {
            dimQuad        = gGpuPrimCursor;
            gGpuPrimCursor = dimQuad + 1;
            setPolyF4(dimQuad);
            setSemiTrans(dimQuad, 1);
            dimQuad->b0 = 0;
            dimQuad->g0 = 0;
            dimQuad->r0 = 0;
            innerLeft   = innerRect->x;
            dimQuad->x2 = innerLeft;
            dimQuad->x0 = innerLeft;
            innerEnd    = innerRect->x + innerRect->w;
            dimQuad->x3 = innerEnd;
            dimQuad->x1 = innerEnd;
            innerTop    = innerRect->y;
            dimQuad->y1 = innerTop;
            dimQuad->y0 = innerTop;
            innerEnd    = innerRect->y + innerRect->h;
            dimQuad->y3 = innerEnd;
            dimQuad->y2 = innerEnd;
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue, dimQuad);

            blendCommand   = gGpuPrimCursor;
            gGpuPrimCursor = blendCommand + 1;
            setlen(blendCommand, 1);
            blendCommand->code[0] = USER_INTERFACE_PANEL_DITHERED_DRAW_MODE;
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue, blendCommand);
        }
    }
}

/// Updates full-bounds content layout and restricts drawing for hidden content.
///
/// Requires two DR_AREA reservations and writable signed panel OT base/base+3
/// tags. A zero-span draw area applies to content; the SDK clamps its negative
/// end coordinates, leaving only VRAM pixel (0,0) in buffer zero and an inverted
/// Y range in buffer one. The base tag restores the 320 by 240 view in the
/// active VRAM buffer, whose rows are 272 apart.
/// Packets remain in the arena until GPU completion.
static void _uiLayoutHiddenPanel(UiPanel* panel)
{
    enum {
        USER_INTERFACE_HIDDEN_CLIP_OT_OFFSET     = 3,
        USER_INTERFACE_HIDDEN_CLIP_VIEW_WIDTH    = 320,
        USER_INTERFACE_HIDDEN_CLIP_VIEW_HEIGHT   = 240,
        USER_INTERFACE_HIDDEN_CLIP_BUFFER_STRIDE = 272
    };
    RECT     drawArea;
    RECT     contentRect;
    DR_AREA* areaCommand;

    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &contentRect);
    USER_INTERFACE_CENTER_PANEL_CONTENT(panel, contentRect);

    areaCommand    = gGpuPrimCursor;
    gGpuPrimCursor = areaCommand + 1;
    drawArea.x     = 0;
    drawArea.w     = 0;
    drawArea.h     = 0;
    drawArea.y     = gDisplayState.drawBuffer * USER_INTERFACE_HIDDEN_CLIP_BUFFER_STRIDE;
    SetDrawArea(areaCommand, &drawArea);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_HIDDEN_CLIP_OT_OFFSET, areaCommand);

    areaCommand    = gGpuPrimCursor;
    gGpuPrimCursor = areaCommand + 1;
    drawArea.x     = 0;
    drawArea.w     = USER_INTERFACE_HIDDEN_CLIP_VIEW_WIDTH;
    drawArea.h     = USER_INTERFACE_HIDDEN_CLIP_VIEW_HEIGHT;
    drawArea.y     = gDisplayState.drawBuffer * USER_INTERFACE_HIDDEN_CLIP_BUFFER_STRIDE;
    SetDrawArea(areaCommand, &drawArea);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue, areaCommand);
}

/// Builds a bottom-anchored animation rectangle using a scale in eighths.
///
/// Style high nibble 1 scales the whole height; other modes keep a twelve-pixel
/// minimum and scale only the excess. Both paths finish at full panel width.
/// Output stores narrow to signed 16-bit pixels, including the intermediate
/// width and height writes. `unusedClosing` is ignored; callers clamp the scale.
static void _uiComputeScaledPanelRect(const UiPanel* panel, RECT* rect, s32 scaleEighths, s32 unusedClosing)
{
    enum { USER_INTERFACE_PANEL_MIN_ANIMATED_HEIGHT = 12 };
    s16 height;

    if (((u8)panel->style >> 4) == USER_INTERFACE_PANEL_HEIGHT_MODE) {
        rect->w = panel->bounds.rect.w;
        rect->h = (panel->bounds.rect.h * scaleEighths) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS;
        rect->x = panel->bounds.rect.x;
        rect->y = (panel->bounds.rect.y + panel->bounds.rect.h) - rect->h;
    } else {
        rect->w = (panel->bounds.rect.w * scaleEighths) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS;
        height  = panel->bounds.rect.h;
        if (height >= USER_INTERFACE_PANEL_MIN_ANIMATED_HEIGHT) {
            height = (((height - USER_INTERFACE_PANEL_MIN_ANIMATED_HEIGHT) * scaleEighths) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS) + USER_INTERFACE_PANEL_MIN_ANIMATED_HEIGHT;
        } else {
            height = USER_INTERFACE_PANEL_MIN_ANIMATED_HEIGHT;
        }
        rect->h = height;
        rect->x = panel->bounds.rect.x;
        rect->y = (panel->bounds.rect.y + panel->bounds.rect.h) - rect->h;
        // Restore full width after the intermediate scaled-width write.
        rect->x = panel->bounds.rect.x;
        rect->w = panel->bounds.rect.w;
    }
}

/// Computes the lifecycle-dependent outer bounds for panel drawing.
///
/// Borrows a live panel and writes a separate live RECT in screen-centered pixels.
/// Opening uses (nine - ticks) eighths with a minimum of one and no upper cap;
/// closing/hiding replace scales outside 1..8 with one. Animation stays at full
/// width and is bottom-anchored, with style selecting the height rule. Other
/// states copy full bounds, including hidden; this does not test visibility.
/// Stores retain sixteen bits, including the scaled helper's intermediate writes.
static inline void _uiComputeDrawnPanelRect(const UiPanel* panel, RECT* rect)
{
    enum { USER_INTERFACE_DRAWN_PANEL_MIN_SCALE_EIGHTHS = 1 };
    s32 scaleEighths;

    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (scaleEighths <= 0) {
                scaleEighths = USER_INTERFACE_DRAWN_PANEL_MIN_SCALE_EIGHTHS;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 0);
            return;
        case USER_INTERFACE_PANEL_OPEN:
            break;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            // Unsigned subtraction rejects both nonpositive and oversized scales.
            if ((u32)(scaleEighths - USER_INTERFACE_DRAWN_PANEL_MIN_SCALE_EIGHTHS) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                scaleEighths = USER_INTERFACE_DRAWN_PANEL_MIN_SCALE_EIGHTHS;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 1);
            return;
    }
    rect->x = panel->bounds.rect.x;
    rect->y = panel->bounds.rect.y;
    rect->w = panel->bounds.rect.w;
    rect->h = panel->bounds.rect.h;
}

/// Publishes full-bounds content coordinates and optionally insets the animated frame.
///
/// Content edges are relative to their center; origins translate them into
/// screen-centered pixels. Layout uses full bounds regardless of lifecycle,
/// then applies title/ordinary content padding with unsigned halfword views.
/// A non-NULL outerRect also produces its frame inset, before content padding,
/// in a separate writable innerRect. A NULL outerRect leaves innerRect untouched.
/// Borrows a live mutable panel; stores retain sixteen bits without clamping.
static inline void _uiLayoutDrawnPanelContent(UiPanel* panel, const RECT* outerRect, RECT* innerRect)
{
    RECT contentRect;

    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &contentRect);
    USER_INTERFACE_CENTER_PANEL_CONTENT(panel, contentRect);
    if (outerRect != NULL) {
        _uiComputePanelInnerRect(panel, outerRect, innerRect);
    }
}

/// Lays out opening content and draws its clipped animated frame.
///
/// Logical content stays centered in the full bounds while only the frame
/// follows the lifecycle scale. Requires a live panel and `_uiDrawPanel`
/// resources including the clipping pair.
/// Publishes content edges/origin with unsigned halfword stores. Counter and
/// control are unchanged; the caller runs content after the drawing setup.
static void _uiLayoutOpeningPanel(UiPanel* panel)
{
    RECT animatedRect;
    RECT innerRect;

    _uiComputeDrawnPanelRect(panel, &animatedRect);
    _uiLayoutDrawnPanelContent(panel, &animatedRect, &innerRect);
    _uiDrawPanel(panel, &animatedRect, &innerRect, 1);
}

/// Lays out open content and draws its frame without adding draw-area commands.
///
/// Logical content stays centered in the full bounds while only the frame
/// follows the lifecycle scale. Requires a live panel and `_uiDrawPanel`
/// resources; an existing draw area still applies.
/// Publishes content edges/origin with unsigned halfword stores. Counter and
/// control are unchanged; the caller runs content after the drawing setup.
static void _uiLayoutOpenPanel(UiPanel* panel)
{
    RECT animatedRect;
    RECT innerRect;

    _uiComputeDrawnPanelRect(panel, &animatedRect);
    _uiLayoutDrawnPanelContent(panel, &animatedRect, &innerRect);
    _uiDrawPanel(panel, &animatedRect, &innerRect, 0);
}

/// Lays out closing or hiding content and draws its clipped animated frame.
///
/// Logical content stays centered in the full bounds while only the frame
/// follows the lifecycle scale. Requires a live panel and `_uiDrawPanel`
/// resources including the clipping pair.
/// Publishes content edges/origin with unsigned halfword stores. Counter and
/// control are unchanged; the caller runs content after the drawing setup.
static void _uiLayoutShrinkingPanel(UiPanel* panel)
{
    RECT animatedRect;
    RECT innerRect;

    _uiComputeDrawnPanelRect(panel, &animatedRect);
    _uiLayoutDrawnPanelContent(panel, &animatedRect, &innerRect);
    _uiDrawPanel(panel, &animatedRect, &innerRect, 1);
}

/// Queues list or full-screen draw areas on the two row-drawing OT layers.
///
/// Zero `fullScreen` clips below `topInset` to an integral number of rows;
/// nonzero restores the 320 by 240 view. Coordinates become VRAM pixels in the
/// active draw buffer, whose rows are 272 apart. `rowHeight` must be positive.
/// The temporary row count narrows to s16 before multiplication by row height.
/// Requires space for two `DR_AREA` packets and writable panel OT base+1/+2
/// tags. Packets remain live until GPU completion; neither input is modified.
static void _uiQueueListDrawArea(const UiList* list, const UiPanel* panel, s32 fullScreen)
{
    enum {
        USER_INTERFACE_LIST_DRAW_AREA_LAYERS    = 2,
        USER_INTERFACE_LIST_DRAW_AREA_OT_OFFSET = 1,
        USER_INTERFACE_LIST_VIEW_WIDTH          = 320,
        USER_INTERFACE_LIST_VIEW_HEIGHT         = 240,
        USER_INTERFACE_LIST_VIEW_CENTER_X       = 160,
        USER_INTERFACE_LIST_VIEW_CENTER_Y       = 120,
        USER_INTERFACE_LIST_DRAW_BUFFER_STRIDE  = 272
    };
    RECT     drawArea;
    DR_AREA* areaCommand;
    s32      layer;
    s16      screenTop;
    s16      wholeRows;

    if (fullScreen == 0) {
        for (layer = 0; layer < USER_INTERFACE_LIST_DRAW_AREA_LAYERS; layer++) {
            areaCommand    = gGpuPrimCursor;
            gGpuPrimCursor = areaCommand + 1;
            drawArea.x     = panel->contentOriginX.unsignedValue + (panel->contentLeft.unsignedValue + USER_INTERFACE_LIST_VIEW_CENTER_X);
            screenTop      = panel->contentOriginY.unsignedValue + (panel->contentTop.unsignedValue + USER_INTERFACE_LIST_VIEW_CENTER_Y) + (gDisplayState.drawBuffer * USER_INTERFACE_LIST_DRAW_BUFFER_STRIDE);
            drawArea.y     = screenTop;
            drawArea.y     = screenTop + list->topInset;
            drawArea.w     = panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue;
            wholeRows      = (panel->contentBottom.signedValue - panel->contentTop.signedValue - list->topInset) / list->rowHeight;
            drawArea.h     = wholeRows;
            drawArea.h     = wholeRows * list->rowHeight;
            SetDrawArea(areaCommand, &drawArea);
            addPrim(gGpuCurrentOt + (layer + panel->otIndex.signedValue) + USER_INTERFACE_LIST_DRAW_AREA_OT_OFFSET, areaCommand);
        }
    } else {
        for (layer = 0; layer < USER_INTERFACE_LIST_DRAW_AREA_LAYERS; layer++) {
            areaCommand    = gGpuPrimCursor;
            gGpuPrimCursor = areaCommand + 1;
            drawArea.w     = USER_INTERFACE_LIST_VIEW_WIDTH;
            drawArea.x     = 0;
            drawArea.h     = USER_INTERFACE_LIST_VIEW_HEIGHT;
            drawArea.y     = gDisplayState.drawBuffer * USER_INTERFACE_LIST_DRAW_BUFFER_STRIDE;
            SetDrawArea(areaCommand, &drawArea);
            addPrim(gGpuCurrentOt + (layer + panel->otIndex.signedValue) + USER_INTERFACE_LIST_DRAW_AREA_OT_OFFSET, areaCommand);
        }
    }
}

/// Selects one of six atlas frames from an eight-VSync animation step.
///
/// The nonnegative step visits three columns in each of two rows; stores
/// retain eight-bit texture coordinates, selecting U 232/240/248 and V 48/56.
/// `cursor` is borrowed writable SPRT_8 storage; only u0 and v0 are changed.
/// The caller's unsigned VSync count shifted by three bounds the step to
/// 0..0x1FFFFFFF, keeping the signed quotient products representable.
static inline void _uiSetCursorTextureFrame(SPRT_8* cursor, s32 animationStep)
{
    enum {
        USER_INTERFACE_CURSOR_TEXTURE_COLUMNS = 3,
        USER_INTERFACE_CURSOR_TEXTURE_ROWS    = 2,
        USER_INTERFACE_CURSOR_SPRITE_PIXELS   = 8,
        USER_INTERFACE_CURSOR_TEXTURE_LEFT    = -24,
        USER_INTERFACE_CURSOR_TEXTURE_TOP     = 48,
    };
    s32 textureColumnWork;
    s32 textureRow;
    s32 textureRowWork;
    s32 textureCoordinate;

    textureColumnWork = animationStep / USER_INTERFACE_CURSOR_TEXTURE_COLUMNS;
    textureRow        = textureColumnWork;
    textureColumnWork = animationStep - textureRow * USER_INTERFACE_CURSOR_TEXTURE_COLUMNS;
    textureRowWork    = textureRow / USER_INTERFACE_CURSOR_TEXTURE_ROWS;
    textureRowWork    = textureRow - textureRowWork * USER_INTERFACE_CURSOR_TEXTURE_ROWS;
    textureCoordinate = textureColumnWork * USER_INTERFACE_CURSOR_SPRITE_PIXELS + USER_INTERFACE_CURSOR_TEXTURE_LEFT;
    cursor->u0        = textureCoordinate;
    textureCoordinate = textureRowWork * USER_INTERFACE_CURSOR_SPRITE_PIXELS + USER_INTERFACE_CURSOR_TEXTURE_TOP;
    cursor->v0        = textureCoordinate;
}

/// Queues the six-frame textured selection cursor for any nonzero control word.
///
/// Coordinates are content-relative pixels; its 8 by 8 sprite starts eight
/// pixels left and two up. Suspended input still draws it. A frame lasts eight
/// VSync ticks. Requires the UI atlas/palette, SPRT_8 and DR_TPAGE arena space,
/// and writable fixed OT tag 4. Packets remain live until GPU completion.
static void _uiDrawAnimatedCursor(const UiPanel* panel, s32 contentX, s32 contentY)
{
    enum {
        USER_INTERFACE_CURSOR_OT_INDEX           = 4,
        USER_INTERFACE_CURSOR_CLUT               = getClut(160, 240),
        USER_INTERFACE_CURSOR_TEXTURE_PAGE       = getTPage(0, 0, 896, 256),
        USER_INTERFACE_CURSOR_FRAME_TICK_SHIFT   = 3,
        USER_INTERFACE_CURSOR_RAW_SPRITE_COMMAND = 0x75,
        USER_INTERFACE_CURSOR_SPRITE_WORDS       = sizeof(SPRT_8) / sizeof(u32) - 1
    };
    SPRT_8*   cursor;
    DR_TPAGE* pageCommand;
    s32       animationStep;
    s32       screenOriginY;

    animationStep = (u32)gDisplayState.vsyncCount >> USER_INTERFACE_CURSOR_FRAME_TICK_SHIFT;
    if (panel->control.word != USER_INTERFACE_PANEL_INACTIVE) {
        cursor         = gGpuPrimCursor;
        gGpuPrimCursor = cursor + 1;
        cursor->x0     = panel->contentOriginX.unsignedValue + contentX - 8;
        screenOriginY  = panel->contentOriginY.unsignedValue;
        cursor->clut   = USER_INTERFACE_CURSOR_CLUT;
        setlen(cursor, USER_INTERFACE_CURSOR_SPRITE_WORDS);
        setcode(cursor, USER_INTERFACE_CURSOR_RAW_SPRITE_COMMAND);
        cursor->y0 = screenOriginY + contentY - 2;
        _uiSetCursorTextureFrame(cursor, animationStep);
        addPrim(gGpuCurrentOt + USER_INTERFACE_CURSOR_OT_INDEX, cursor);
        pageCommand    = gGpuPrimCursor;
        gGpuPrimCursor = pageCommand + 1;
        setDrawTPage(pageCommand, 0, 1, USER_INTERFACE_CURSOR_TEXTURE_PAGE);
        addPrim(gGpuCurrentOt + USER_INTERFACE_CURSOR_OT_INDEX, pageCommand);
    }
}

/// Spreads a gouraud overflow caret's base around the current tip position.
///
/// Requires x1 and x2 at the tip's X; y0 may already include the tip animation.
/// Coordinates retain sixteen bits; zero points up and nonzero points down.
static inline void _uiSetOverflowCaretBase(POLY_G3* caret, s32 pointsDown)
{
    u16 baseY;

    USER_INTERFACE_SET_CARET_BASE(caret, pointsDown, baseY);
}

/// Queues a gouraud caret showing list rows available above or below the window.
///
/// Zero points up, using the list's top inset; any nonzero value points down.
/// Active control moves the tip through four one-pixel positions, one per eight
/// VSync ticks. The right edge and content edges locate the caret; coordinates
/// retain sixteen bits. Borrows both records and requires a POLY_G3 reservation
/// and writable signed panel OT base+1 tag, live until GPU completion.
static void _uiDrawListOverflowCaret(const UiList* list, const UiPanel* panel, s32 pointsDown)
{
    enum {
        USER_INTERFACE_OVERFLOW_CARET_OT_OFFSET        = 1,
        USER_INTERFACE_OVERFLOW_CARET_FRAME_TICK_SHIFT = 3,
        USER_INTERFACE_OVERFLOW_CARET_PHASE_MASK       = 3,
        USER_INTERFACE_OVERFLOW_CARET_PHASE_BIAS       = 3,
        USER_INTERFACE_OVERFLOW_CARET_TIP_RED          = 159,
        USER_INTERFACE_OVERFLOW_CARET_TIP_GREEN        = 127,
        USER_INTERFACE_OVERFLOW_CARET_TIP_BLUE         = 191,
        USER_INTERFACE_OVERFLOW_CARET_BASE_RED         = 223,
        USER_INTERFACE_OVERFLOW_CARET_BASE_GREEN       = 207,
        USER_INTERFACE_OVERFLOW_CARET_BASE_BLUE        = 255
    };
    POLY_G3* caret;
    s16      tipX;
    s32      screenOriginY;
    s32      bottomOriginY;

    caret          = gGpuPrimCursor;
    gGpuPrimCursor = caret + 1;
    setPolyG3(caret);

    tipX      = panel->bounds.rect.x + panel->bounds.rect.w - 5;
    caret->x2 = tipX;
    caret->x1 = tipX;
    caret->x0 = tipX;

    screenOriginY = panel->contentOriginY.unsignedValue;
    caret->y2     = screenOriginY;
    caret->y1     = screenOriginY;
    caret->y0     = screenOriginY;

    if (pointsDown == USER_INTERFACE_CARET_UP) {
        screenOriginY += panel->contentTop.unsignedValue;
        caret->y0      = screenOriginY;
        if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            caret->y0 -= (((u32)gDisplayState.vsyncCount >> USER_INTERFACE_OVERFLOW_CARET_FRAME_TICK_SHIFT) & USER_INTERFACE_OVERFLOW_CARET_PHASE_MASK) - USER_INTERFACE_OVERFLOW_CARET_PHASE_BIAS;
        }
        caret->y0 += list->topInset;
        _uiSetOverflowCaretBase(caret, USER_INTERFACE_CARET_UP);
    } else {
        bottomOriginY = screenOriginY + 2;
        caret->y0     = panel->contentBottom.unsignedValue + bottomOriginY;
        if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            caret->y0 += (((u32)gDisplayState.vsyncCount >> USER_INTERFACE_OVERFLOW_CARET_FRAME_TICK_SHIFT) & USER_INTERFACE_OVERFLOW_CARET_PHASE_MASK) - USER_INTERFACE_OVERFLOW_CARET_PHASE_BIAS;
        }
        _uiSetOverflowCaretBase(caret, USER_INTERFACE_CARET_DOWN);
    }

    caret->r0 = USER_INTERFACE_OVERFLOW_CARET_TIP_RED;
    caret->g0 = USER_INTERFACE_OVERFLOW_CARET_TIP_GREEN;
    caret->b0 = USER_INTERFACE_OVERFLOW_CARET_TIP_BLUE;
    caret->r2 = USER_INTERFACE_OVERFLOW_CARET_BASE_RED;
    caret->r1 = USER_INTERFACE_OVERFLOW_CARET_BASE_RED;
    caret->g2 = USER_INTERFACE_OVERFLOW_CARET_BASE_GREEN;
    caret->g1 = USER_INTERFACE_OVERFLOW_CARET_BASE_GREEN;
    caret->b2 = USER_INTERFACE_OVERFLOW_CARET_BASE_BLUE;
    caret->b1 = USER_INTERFACE_OVERFLOW_CARET_BASE_BLUE;
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_OVERFLOW_CARET_OT_OFFSET, caret);
}

void uiSetPanelContentSize(UiPanel* panel, s32 contentWidth, s32 contentHeight)
{
    RECT contentRect;

    if (contentWidth > 0) {
        panel->bounds.rect.w = (panel->bounds.rect.w - (panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue)) + contentWidth;
    }
    if (contentHeight > 0) {
        panel->bounds.rect.h = (panel->bounds.rect.h - (panel->contentBottom.unsignedValue - panel->contentTop.unsignedValue)) + contentHeight;
    }
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &contentRect);
    USER_INTERFACE_CENTER_PANEL_CONTENT(panel, contentRect);
}

void uiFitPanelToList(UiList* list, UiPanel* panel)
{
    // Screen-centered pixel limits, retaining eight pixels of right/bottom margin.
    enum {
        USER_INTERFACE_LIST_PANEL_RIGHT_LIMIT  = 152,
        USER_INTERFACE_LIST_PANEL_BOTTOM_LIMIT = 112
    };
    RECT contentRect;
    s32  availableHeight;
    s32  edgeOverflowPixels;
    s32  heightGrowthPixels;

    if (list->visibleRowCount.signedValue == 0) {
        list->visibleRowCount.signedValue = list->itemCount;
    } else if (list->itemCount < list->visibleRowCount.signedValue) {
        list->visibleRowCount.signedValue = list->itemCount;
    }

    // Fit the requested rows, then keep the lower/right edges inside the view.
    heightGrowthPixels    = list->visibleRowCount.signedValue * list->rowHeight;
    heightGrowthPixels   -= panel->contentBottom.signedValue - panel->contentTop.signedValue;
    panel->bounds.rect.h += heightGrowthPixels;
    edgeOverflowPixels    = USER_INTERFACE_LIST_PANEL_RIGHT_LIMIT - (panel->bounds.rect.x + panel->bounds.rect.w);
    if (edgeOverflowPixels < 0) {
        panel->bounds.rect.x += edgeOverflowPixels;
    }
    edgeOverflowPixels = USER_INTERFACE_LIST_PANEL_BOTTOM_LIMIT - (panel->bounds.rect.y + panel->bounds.rect.h);
    if (edgeOverflowPixels < 0) {
        panel->bounds.rect.y += edgeOverflowPixels;
    }

    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &contentRect);
    USER_INTERFACE_CENTER_LIST_PANEL_CONTENT(panel, contentRect);

    list->topInset   = 0;
    contentRect.x    = panel->contentOriginX.unsignedValue + panel->contentLeft.signedValue;
    contentRect.y    = panel->contentOriginY.unsignedValue + panel->contentTop.signedValue;
    contentRect.w    = panel->contentRight.signedValue - panel->contentLeft.signedValue;
    contentRect.h    = panel->contentBottom.signedValue - panel->contentTop.signedValue;
    availableHeight  = contentRect.h;
    availableHeight -= list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    if (availableHeight >= list->itemCount * list->rowHeight) {
        list->visibleRowCount.signedValue = list->itemCount;
    } else {
        list->visibleRowCount.signedValue = availableHeight / list->rowHeight;
        if (list->visibleRowCount.signedValue <= 0) {
            list->visibleRowCount.signedValue = 1;
        }
    }
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
    if (list->itemCount <= list->visibleRowCount.signedValue) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->flags                 = 0;
    list->scrollPixelsRemaining = 0;
    list->scrollDirection       = USER_INTERFACE_LIST_STEP_NONE;
    list->rowInputEnabled       = USER_INTERFACE_LIST_ROW_INACTIVE;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode != 0) {
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
}

/// Queues an opaque solid fill one pixel inside a rectangle's edges.
///
/// `left` and `top` are signed pixel coordinates relative to the panel's content
/// origin; `width` and `height` are edge spans in pixels. The fill starts at
/// (left + 1, top + 1) and measures (width - 1) by (height - 1).
/// A zero `colorWord` or width < 2 does nothing; height is not checked. Colour
/// bytes are packed red, green, blue from low to high. All 32 bits take part in
/// the zero test; otherwise the high byte is overwritten with the TILE command.
/// Coordinate and dimension stores retain the low 16 bits without clamping.
///
/// Borrows the panel without modifying it. Drawing requires word-aligned space
/// for one `TILE` in the current primitive arena and a writable ordering-table
/// tag at the panel's signed base + 1. No capacity checks or clipping occur here;
/// the arena must retain the packet until the GPU finishes drawing the frame.
static inline void _uiFillRectInterior(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord)
{
    enum {
        USER_INTERFACE_RECT_NO_FILL_COLOR  = 0,
        USER_INTERFACE_RECT_FILL_OT_OFFSET = 1
    };
    TILE* tile;
    s32   originY;

    if (colorWord != USER_INTERFACE_RECT_NO_FILL_COLOR && width >= 2) {
        tile                              = gGpuPrimCursor;
        gGpuPrimCursor                    = tile + 1;
        tile->x0                          = panel->contentOriginX.unsignedValue + left + 1;
        originY                           = panel->contentOriginY.unsignedValue;
        tile->w                           = width - 1;
        tile->h                           = height - 1;
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = colorWord;
        tile->y0                          = originY + top + 1;
        setTile(tile);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_RECT_FILL_OT_OFFSET, tile);
    }
}

/// Queues the two three-vertex polylines forming a rectangle's bevel.
static inline void _uiQueueRectBevelEdges(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, s32 raisedBevel)
{
    enum {
        USER_INTERFACE_RECT_BEVEL_DARK_COLOR  = GPU_PACK_COLOR_WORD(0x10, 0x18, 0x10, 0),
        USER_INTERFACE_RECT_BEVEL_LIGHT_COLOR = GPU_PACK_COLOR_WORD(0x58, 0x60, 0x50, 0),
        USER_INTERFACE_RECT_BEVEL_OT_OFFSET   = 1
    };
    LINE_F3* edge;
    u16      endpoint;

    edge                              = gGpuPrimCursor;
    edge->x2                          = panel->contentOriginX.unsignedValue + left + 1;
    endpoint                          = panel->contentOriginX.unsignedValue + (left + width);
    edge->x1                          = endpoint;
    edge->x0                          = endpoint;
    gGpuPrimCursor                    = edge + 1;
    edge->y0                          = panel->contentOriginY.unsignedValue + top;
    endpoint                          = panel->contentOriginY.unsignedValue + (top + height);
    edge->y2                          = endpoint;
    edge->y1                          = endpoint;
    GPU_PRIMITIVE_COLOR_WORD(edge, 0) = ((raisedBevel & USER_INTERFACE_RECT_RAISED_BEVEL) == USER_INTERFACE_RECT_RECESSED_BEVEL) ? USER_INTERFACE_RECT_BEVEL_LIGHT_COLOR : USER_INTERFACE_RECT_BEVEL_DARK_COLOR;
    setLineF3(edge);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_RECT_BEVEL_OT_OFFSET, edge);

    edge                              = gGpuPrimCursor;
    endpoint                          = panel->contentOriginX.unsignedValue + left;
    edge->x1                          = endpoint;
    edge->x2                          = endpoint;
    edge->x0                          = panel->contentOriginX.unsignedValue + (left + width) - 1;
    gGpuPrimCursor                    = edge + 1;
    endpoint                          = panel->contentOriginY.unsignedValue + top;
    edge->y1                          = endpoint;
    edge->y0                          = endpoint;
    edge->y2                          = panel->contentOriginY.unsignedValue + (top + height);
    GPU_PRIMITIVE_COLOR_WORD(edge, 0) = ((raisedBevel & USER_INTERFACE_RECT_RAISED_BEVEL) == USER_INTERFACE_RECT_RECESSED_BEVEL) ? USER_INTERFACE_RECT_BEVEL_DARK_COLOR : USER_INTERFACE_RECT_BEVEL_LIGHT_COLOR;
    setLineF3(edge);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_RECT_BEVEL_OT_OFFSET, edge);
}

void uiDrawBeveledRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord, s32 raisedBevel)
{
    _uiFillRectInterior(panel, left, top, width, height, colorWord);
    _uiQueueRectBevelEdges(panel, left, top, width, height, raisedBevel);
}

/// Fills the selected row behind its text, using a content-relative bottom edge.
///
/// Temporarily advances the panel's 16-bit OT base so the fill is at base+2,
/// then restores it. The interior starts one pixel inside the row rectangle.
/// Borrows the list and requires one TILE's arena space when its width permits.
/// `unused` is ignored; no clipping or capacity checks occur here.
static void _uiDrawListHighlight(const UiList* list, UiPanel* panel, s32 rowBottom, s32 unused)
{
    enum { USER_INTERFACE_LIST_HIGHLIGHT_COLOR = GPU_PACK_COLOR_WORD(0x1F, 0x74, 0x01, 0) };
    UiPanel* highlightPanel;
    s32      rowHeight;
    s32      left;

    highlightPanel = panel;
    rowHeight      = list->rowHeight;
    left           = highlightPanel->contentLeft.signedValue;
    highlightPanel->otIndex.unsignedValue++;
    _uiFillRectInterior(panel, left, rowBottom - rowHeight, highlightPanel->contentRight.signedValue - left - 1, rowHeight, USER_INTERFACE_LIST_HIGHLIGHT_COLOR);
    highlightPanel->otIndex.unsignedValue--;
}

/// Eases the retained selection cursor toward a screen-centered 24.8 target.
///
/// `targetXFixed` and `targetYFixed` use 1/256-pixel units. Applies the current
/// frame's `gDisplayState.frameTicks` nominal 60-Hz steps (normally 1, 2 or 3),
/// sampled once per call; zero skips movement. Each step adds one quarter of
/// the remaining displacement, rounded toward negative infinity by a signed
/// shift. There is no final snap: a positive remainder of 1..3 fixed units
/// produces no movement. Targets and their differences from the retained
/// coordinates must fit s32. Calls share the position across panels; this
/// helper neither checks panel control nor draws the cursor.
static inline void _uiEaseCursorPosition(s32 targetXFixed, s32 targetYFixed)
{
    s32 ticksApplied;
    u8  frameTicks;

    ticksApplied = 0;
    frameTicks   = gDisplayState.frameTicks;
    if (frameTicks == 0) {
        return;
    }
    do {
        ticksApplied++;
        D_80067648 += (targetXFixed - D_80067648) >> USER_INTERFACE_CURSOR_EASING_SHIFT;
        D_8006764C += (targetYFixed - D_8006764C) >> USER_INTERFACE_CURSOR_EASING_SHIFT;
    } while (ticksApplied < frameTicks);
}

/// Eases and draws the shared selection cursor at a list's content-relative target.
///
/// Coordinates are pixels relative to the borrowed panel's content origin.
/// Uses the same retained 24.8 cursor as `uiEaseAndDrawCursor`; inactive control
/// still advances it. Requires the animated cursor's arena, atlas and OT tag 4.
static inline void _uiListMoveCursor(const UiPanel* panel, s32 contentX, s32 contentY)
{
    s16 screenOriginX;
    s16 screenOriginY;
    s32 targetX;
    s32 targetY;

    screenOriginX = panel->contentOriginX.signedValue;
    screenOriginY = panel->contentOriginY.signedValue;
    targetX       = contentX + screenOriginX;
    targetY       = contentY + screenOriginY;
    targetX     <<= USER_INTERFACE_CURSOR_FRACTION_BITS;
    targetY     <<= USER_INTERFACE_CURSOR_FRACTION_BITS;
    _uiEaseCursorPosition(targetX, targetY);
    targetX = D_80067648 >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    targetY = D_8006764C >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    _uiDrawAnimatedCursor(panel, targetX - panel->contentOriginX.signedValue, targetY - panel->contentOriginY.signedValue);
}

/// Draws visible list rows, processes navigation and advances animated scrolling.
///
/// Requires `uiUpdateList`'s panel ownership, callback, bounds and drawing contract.
/// inputPort is 0 or 1 for left/right/up/down queries; shoulder paging always
/// reads port zero. Existing scrolling advances by two pixels per elapsed
/// nominal 60-Hz tick and blocks generic navigation until complete. Callbacks receive
/// wrapped item indices and may request row skipping or change the text pen.
static void _uiUpdateListRows(UiList* list, UiPanel* panel, s32 inputPort)
{
    s32 selectionStep;
    s32 playCursorSound;
    s32 drawHighlight;
    u32 defaultColorRgb;
    s32 scrollMarginRows;
    s32 rowBottom;
    s32 highlightedRowBottom;
    s32 cursorX;
    s32 cursorY;
    s32 rowsToDraw;
    s32 itemIndex;
    s32 rowIndex;
    s32 textBaselineInset;
    s32 cursorRowHeight;
    s32 controlMode;
    s32 cursorSoundId;
    s32 rowCenterY;
    s32 textRowHeight;

    cursorY          = 0;
    selectionStep    = USER_INTERFACE_LIST_STEP_NONE;
    playCursorSound  = 0;
    drawHighlight    = 0;
    scrollMarginRows = list->visibleRowCount.signedValue >> 2;
    defaultColorRgb  = D_80067640;
    if (scrollMarginRows < 2) {
        scrollMarginRows = 0;
    }
    // Each dispatch publishes fresh results and panel-local row coordinates.
    list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_NONE;
    list->actionResult              = USER_INTERFACE_RESULT_NONE;
    list->rowTextX.signedValue      = panel->contentLeft.unsignedValue + 2;
    controlMode                     = panel->control.word;
    if (controlMode >= USER_INTERFACE_PANEL_REQUEST_MIN) {
        switch (controlMode) {
            case USER_INTERFACE_PANEL_SELECT_FIRST_VISIBLE:
                list->selectedItemIndex = list->firstVisibleItemIndex.signedValue;
                break;
            case USER_INTERFACE_PANEL_SELECT_LAST_VISIBLE:
                list->selectedItemIndex = list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue - 1;
                break;
        }
    }
    if (list->selectedItemIndex < 0) {
        list->selectedItemIndex += list->itemCount;
    }
    // An unfinished scroll exposes one extra wrapped row until displacement reaches zero.
    rowsToDraw = list->visibleRowCount.signedValue;
    if (rowsToDraw < list->itemCount) {
        if (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue > 0) {
            _uiDrawListOverflowCaret(list, panel, USER_INTERFACE_CARET_UP);
        }
        if (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue < list->itemCount) {
            _uiDrawListOverflowCaret(list, panel, USER_INTERFACE_CARET_DOWN);
        }
        list->rowTextY.signedValue = panel->contentTop.unsignedValue + list->rowHeight;
        if (list->scrollPixelsRemaining > 0) {
            list->scrollPixelsRemaining -= gDisplayState.frameTicks * USER_INTERFACE_LIST_SCROLL_PIXELS_PER_TICK;
            if (list->scrollPixelsRemaining <= 0) {
                list->scrollPixelsRemaining = 0;
                if (list->scrollDirection == USER_INTERFACE_LIST_STEP_NEXT) {
                    list->firstVisibleItemIndex.signedValue++;
                    if (list->firstVisibleItemIndex.signedValue >= list->itemCount) {
                        list->firstVisibleItemIndex.signedValue -= list->itemCount;
                    }
                }
                list->scrollDirection = USER_INTERFACE_LIST_STEP_NONE;
            } else {
                if (list->scrollDirection == USER_INTERFACE_LIST_STEP_NEXT) {
                    s32 cursorBaselineY         = list->rowTextY.signedValue + 7;
                    cursorY                     = cursorBaselineY - list->rowHeight + (list->visibleRowCount.signedValue - 1) * list->rowHeight;
                    list->rowTextY.signedValue -= list->rowHeight - list->scrollPixelsRemaining;
                } else {
                    s32 cursorBaselineY         = list->rowTextY.signedValue + 7;
                    cursorY                     = cursorBaselineY - list->rowHeight;
                    list->rowTextY.signedValue -= list->scrollPixelsRemaining;
                }
                rowsToDraw++;
            }
        }
    } else {
        list->rowTextY.signedValue = panel->contentTop.unsignedValue + list->rowHeight;
    }
    list->rowTextY.signedValue += list->topInset;
    highlightedRowBottom        = list->rowTextY.signedValue;
    rowBottom                   = highlightedRowBottom;
    if (list->itemCount == 0) {
        s32 cursorBaselineY = highlightedRowBottom + 7;

        cursorX = list->rowTextX.signedValue - 2;
        cursorY = cursorBaselineY - list->rowHeight;
        _uiListMoveCursor(panel, cursorX, cursorY);
        return;
    }
    if (list->scrollPixelsRemaining != 0) {
        _uiQueueListDrawArea(list, panel, 1);
    }
    // Row callbacks may adjust the text pen or request skipping the selected row.
    itemIndex = list->firstVisibleItemIndex.signedValue;
    for (rowIndex = 0; rowIndex < rowsToDraw; rowIndex++) {
        if (itemIndex == list->selectedItemIndex) {
            if (list->scrollDirection == USER_INTERFACE_LIST_STEP_NONE) {
                if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
                    list->rowInputEnabled = USER_INTERFACE_LIST_ROW_ACTIVE;
                    drawHighlight         = 1;
                    list->colorRgb        = defaultColorRgb;
                    highlightedRowBottom  = rowBottom;
                } else {
                    list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
                    list->colorRgb        = defaultColorRgb;
                }
            }
            cursorRowHeight = list->rowHeight;
            rowCenterY      = rowBottom - (cursorRowHeight - 1) / 2;
            cursorY         = rowCenterY - 1;
            if (cursorRowHeight == 8) {
                cursorY = rowCenterY - 2;
            }
        } else {
            list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            list->colorRgb        = defaultColorRgb;
        }
        USER_INTERFACE_DISPATCH_LIST_ROW(list, panel, itemIndex, rowBottom, textBaselineInset, textRowHeight);
        if (itemIndex == list->selectedItemIndex && list->actionResult == USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
            drawHighlight = 0;
        }
        itemIndex++;
        rowBottom  = list->rowTextY.signedValue + textBaselineInset;
        rowBottom += list->rowHeight;
        if (itemIndex >= list->itemCount) {
            itemIndex -= list->itemCount;
        }
    }
    if (drawHighlight == 1 && list->rowHeight != USER_INTERFACE_LIST_PREVIEW_ROW_HEIGHT) {
        _uiDrawListHighlight(list, panel, highlightedRowBottom, 0);
    }
    cursorX = list->rowTextX.signedValue - 2;
    if (list->scrollPixelsRemaining != 0) {
        _uiQueueListDrawArea(list, panel, 0);
    } else if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (list->actionResult == USER_INTERFACE_RESULT_NONE && padCheckButtons(inputPort, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT) == 0) {
            if (padCheckButtons(inputPort, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                playCursorSound          = 1;
                list->navigationStep     = USER_INTERFACE_LIST_STEP_PREVIOUS;
                selectionStep            = USER_INTERFACE_LIST_STEP_PREVIOUS;
                list->selectedItemIndex -= 1;
            } else if (padCheckButtons(inputPort, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                playCursorSound          = 1;
                selectionStep            = USER_INTERFACE_LIST_STEP_NEXT;
                list->selectedItemIndex += 1;
                list->navigationStep     = USER_INTERFACE_LIST_STEP_NEXT;
            } else if (list->visibleRowCount.signedValue < list->itemCount && list->wrapNavigation == 0) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L1) != 0) {
                    if (list->selectedItemIndex != 0) {
                        playCursorSound = 1;
                    }
                    list->navigationStep     = USER_INTERFACE_LIST_STEP_PREVIOUS;
                    selectionStep            = USER_INTERFACE_LIST_STEP_PREVIOUS;
                    list->selectedItemIndex -= 1;
                    if (list->firstVisibleItemIndex.signedValue > 0) {
                        list->firstVisibleItemIndex.signedValue -= list->visibleRowCount.signedValue;
                        if (list->firstVisibleItemIndex.signedValue < 0) {
                            list->firstVisibleItemIndex.signedValue = 0;
                        }
                        if (list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue - 1 < list->selectedItemIndex) {
                            list->selectedItemIndex = list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue - 1;
                        }
                    } else {
                        list->selectedItemIndex = 0;
                    }
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_R1) != 0) {
                    if (list->selectedItemIndex != list->itemCount - 1) {
                        playCursorSound = 1;
                    }
                    list->navigationStep     = USER_INTERFACE_LIST_STEP_NEXT;
                    selectionStep            = USER_INTERFACE_LIST_STEP_NEXT;
                    list->selectedItemIndex += 1;
                    if (list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue < list->itemCount) {
                        list->firstVisibleItemIndex.signedValue += list->visibleRowCount.signedValue;
                        if (list->firstVisibleItemIndex.signedValue > list->itemCount - list->visibleRowCount.signedValue) {
                            list->firstVisibleItemIndex.signedValue = list->itemCount - list->visibleRowCount.signedValue;
                        }
                        if (list->selectedItemIndex < list->firstVisibleItemIndex.signedValue) {
                            list->selectedItemIndex = list->firstVisibleItemIndex.signedValue;
                        }
                    } else {
                        list->selectedItemIndex = list->itemCount - 1;
                    }
                }
            }
        }
        if (list->actionResult == USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
            list->actionResult = USER_INTERFACE_RESULT_NONE;
            if (list->navigationStep == USER_INTERFACE_LIST_STEP_NONE) {
                list->navigationStep = USER_INTERFACE_LIST_STEP_NEXT;
            }
            list->selectedItemIndex += list->navigationStep;
            selectionStep            = list->navigationStep;
        }
    }
    if ((panel->control.word == USER_INTERFACE_PANEL_ACTIVE || panel->control.modes.suspended == USER_INTERFACE_PANEL_ACTIVE) && list->rowHeight != USER_INTERFACE_LIST_PREVIEW_ROW_HEIGHT) {
        _uiListMoveCursor(panel, cursorX, cursorY);
    }
    // Clamp or wrap selection and start any required one-row scroll.
    if (selectionStep == USER_INTERFACE_LIST_STEP_PREVIOUS) {
        if (list->selectedItemIndex < 0) {
            if (list->wrapNavigation != 0) {
                list->selectedItemIndex += list->itemCount;
            } else {
                playCursorSound         = 0;
                list->actionResult      = USER_INTERFACE_LIST_ACTION_AT_START;
                list->selectedItemIndex = 0;
                list->navigationStep    = USER_INTERFACE_LIST_STEP_NEXT;
            }
        }
        if (list->itemCount != list->visibleRowCount.signedValue) {
            s32 previousScrollEdge = scrollMarginRows - 1;

            if (list->firstVisibleItemIndex.signedValue + previousScrollEdge >= list->selectedItemIndex % list->itemCount) {
                if (list->wrapNavigation != 0) {
                    list->firstVisibleItemIndex.signedValue -= 1;
                    if (list->firstVisibleItemIndex.signedValue < 0) {
                        list->firstVisibleItemIndex.signedValue += list->itemCount;
                    }
                    list->scrollDirection       = USER_INTERFACE_LIST_STEP_PREVIOUS;
                    list->scrollPixelsRemaining = list->rowHeight;
                } else {
                    list->firstVisibleItemIndex.signedValue -= 1;
                    if (list->firstVisibleItemIndex.signedValue < 0) {
                        list->firstVisibleItemIndex.signedValue = 0;
                    } else {
                        list->scrollDirection       = USER_INTERFACE_LIST_STEP_PREVIOUS;
                        list->scrollPixelsRemaining = list->rowHeight;
                    }
                }
            }
        }
    } else if (selectionStep == USER_INTERFACE_LIST_STEP_NEXT) {
        if (list->selectedItemIndex >= list->itemCount) {
            if (list->wrapNavigation != 0) {
                list->selectedItemIndex -= list->itemCount;
            } else {
                playCursorSound         = 0;
                list->selectedItemIndex = list->itemCount - 1;
                list->actionResult      = USER_INTERFACE_LIST_ACTION_AT_END;
                list->navigationStep    = USER_INTERFACE_LIST_STEP_PREVIOUS;
            }
        }
        if (list->itemCount != list->visibleRowCount.signedValue) {
            if (list->selectedItemIndex % list->itemCount >= (list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue - scrollMarginRows) % list->itemCount && (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue < list->itemCount - list->visibleRowCount.signedValue)) {
                list->scrollDirection       = USER_INTERFACE_LIST_STEP_NEXT;
                list->scrollPixelsRemaining = list->rowHeight;
            }
        }
    }
    if (playCursorSound != 0) {
        cursorSoundId = SOUND_SYSTEM_CURSOR;
        if (!(list->flags & USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND)) {
            cursorSoundId = SOUND_MENU_CURSOR;
        }
        sndEvtRequestScriptStart(cursorSoundId, 0, 0);
    }
}

/// Initializes the texture and GPU command for a horizontal UI separator quad.
///
/// `separator` borrows one writable `POLY_FT4`. Sets UVs, texture page, CLUT and
/// the nine-word DMA payload length. Raw-texture drawing ignores the untouched
/// RGB bytes, and semitransparency is disabled. Screen vertices and the DMA link
/// are preserved for the caller to set before submitting the packet.
/// Drawing requires the 4-bit texture page at VRAM (896, 256) and its palette
/// at (48, 240); this helper neither allocates storage nor loads the texture.
static inline void _uiInitHorizontalSeparatorPacket(POLY_FT4* separator)
{
    enum {
        // Page-relative texture coordinates, in texels.
        USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_U = 0x68,
        USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_V = 0x50,
        /// UV endpoint difference on both axes of the horizontal separator, in texels.
        ///
        /// `setUVWH` selects U=0x68..0x6F and V=0x50..0x57: inclusive ranges of
        /// eight texel coordinates, independent of the separator's drawn width.
        USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_ENDPOINT_DELTA = 7,
        // 4-bit page at VRAM word X=896, row Y=256; blending is disabled.
        USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_PAGE = getTPage(0, 0, 0x380, 0x100),
        // Palette selector for VRAM word X=48, row Y=240.
        USER_INTERFACE_HORIZONTAL_SEPARATOR_CLUT_ID = getClut(0x30, 0xF0),

        /// Selects unmodulated texture colour for the horizontal separator.
        ///
        /// A nonzero `setShadeTex` selector sets command bit 0, so the packet's
        /// untouched RGB bytes do not affect drawing.
        USER_INTERFACE_HORIZONTAL_SEPARATOR_RAW_TEXTURE = 1
    };

    setUVWH(separator, USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_U,
            USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_V,
            USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_ENDPOINT_DELTA,
            USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_ENDPOINT_DELTA);
    separator->tpage = USER_INTERFACE_HORIZONTAL_SEPARATOR_TEXTURE_PAGE;
    separator->clut  = USER_INTERFACE_HORIZONTAL_SEPARATOR_CLUT_ID;
    setPolyFT4(separator);
    setShadeTex(separator, USER_INTERFACE_HORIZONTAL_SEPARATOR_RAW_TEXTURE);
}

void uiDrawHorizontalSeparator(const UiPanel* panel, s32 left, s32 right, s32 centerY)
{
    /// Horizontal separator layer relative to the panel's signed ordering-table base.
    ///
    /// Counts four-byte DMA tags, placing separators between the frame at base+3
    /// and text at base+1. The signed base+2 index must select a writable tag in
    /// the current table; foreground indices can be negative with a shifted base.
    enum { USER_INTERFACE_HORIZONTAL_SEPARATOR_OT_OFFSET = 2 };
    POLY_FT4* separator;
    s32       screenY;

    if (left < right) {
        // Translate content coordinates before narrowing into the GPU packet.
        separator     = gGpuPrimCursor;
        separator->x0 = separator->x2 = panel->contentOriginX.unsignedValue + left;
        gGpuPrimCursor                = separator + 1;
        separator->x1 = separator->x3 = panel->contentOriginX.unsignedValue + right;
        screenY                       = panel->contentOriginY.unsignedValue + centerY;
        separator->y0 = separator->y1 = screenY - 4;
        separator->y2 = separator->y3 = screenY + 3;
        _uiInitHorizontalSeparatorPacket(separator);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_HORIZONTAL_SEPARATOR_OT_OFFSET, separator);
    }
}

/// Initializes the vertical separator's eight-texel UI atlas cell and raw texture mode.
static inline void _uiInitVerticalSeparatorPacket(POLY_FT4* separator)
{
    enum {
        USER_INTERFACE_VERTICAL_SEPARATOR_U              = 0x70,
        USER_INTERFACE_VERTICAL_SEPARATOR_V              = 0x50,
        USER_INTERFACE_VERTICAL_SEPARATOR_ENDPOINT_DELTA = 7,
        USER_INTERFACE_VERTICAL_SEPARATOR_TEXTURE_PAGE   = getTPage(0, 0, 896, 256),
        USER_INTERFACE_VERTICAL_SEPARATOR_CLUT           = getClut(48, 240),
        USER_INTERFACE_VERTICAL_SEPARATOR_RAW_TEXTURE    = 1
    };
    setUV4(separator,
           USER_INTERFACE_VERTICAL_SEPARATOR_U, USER_INTERFACE_VERTICAL_SEPARATOR_V,
           USER_INTERFACE_VERTICAL_SEPARATOR_U + USER_INTERFACE_VERTICAL_SEPARATOR_ENDPOINT_DELTA, USER_INTERFACE_VERTICAL_SEPARATOR_V,
           USER_INTERFACE_VERTICAL_SEPARATOR_U, USER_INTERFACE_VERTICAL_SEPARATOR_V + USER_INTERFACE_VERTICAL_SEPARATOR_ENDPOINT_DELTA,
           USER_INTERFACE_VERTICAL_SEPARATOR_U + USER_INTERFACE_VERTICAL_SEPARATOR_ENDPOINT_DELTA, USER_INTERFACE_VERTICAL_SEPARATOR_V + USER_INTERFACE_VERTICAL_SEPARATOR_ENDPOINT_DELTA);
    separator->tpage = USER_INTERFACE_VERTICAL_SEPARATOR_TEXTURE_PAGE;
    separator->clut  = USER_INTERFACE_VERTICAL_SEPARATOR_CLUT;
    setPolyFT4(separator);
    setShadeTex(separator, USER_INTERFACE_VERTICAL_SEPARATOR_RAW_TEXTURE);
}

void uiDrawVerticalSeparator(const UiPanel* panel, s32 top, s32 bottom, s32 centerX)
{
    enum { USER_INTERFACE_VERTICAL_SEPARATOR_OT_OFFSET = 2 };
    POLY_FT4* separator;
    s32       screenX;

    if (top < bottom) {
        separator     = gGpuPrimCursor;
        screenX       = panel->contentOriginX.unsignedValue + centerX;
        separator->x0 = separator->x2 = screenX - 3;
        separator->x1 = separator->x3 = screenX + 5;
        gGpuPrimCursor                = separator + 1;
        separator->y0 = separator->y1 = panel->contentOriginY.unsignedValue + top;
        separator->y2 = separator->y3 = panel->contentOriginY.unsignedValue + bottom;
        _uiInitVerticalSeparatorPacket(separator);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + USER_INTERFACE_VERTICAL_SEPARATOR_OT_OFFSET, separator);
    }
}

/// Queues a seven-pixel-high backing plate ending at a label's final text pen.
///
/// Coordinates are screen-centered pixels and stores retain sixteen bits.
/// `labelEndX` borrows the final signed-halfword text pen for this call. Its
/// value supplies the bottom right edge; the top overhangs it by three pixels.
/// The pen is read after storing the plate's left edge.
/// Requires word-aligned arena space for a POLY_FT4-sized reservation and a
/// writable `otIndex` tag. Writes a POLY_F4; retain the packet and unused
/// reservation tail until the GPU consumes the ordering table.
static inline void _uiQueueLabelBacking(s32 left, s32 top, const s16* labelEndX, s32 otIndex)
{
    enum {
        USER_INTERFACE_LABEL_BACKING_COLOR           = GPU_PACK_COLOR_WORD(0x02, 0x10, 0x02, 0),
        USER_INTERFACE_LABEL_BACKING_HEIGHT_PIXELS   = 7,
        USER_INTERFACE_LABEL_BACKING_OVERHANG_PIXELS = 3
    };
    POLY_F4* backing;
    s16      rightEdge;

    backing     = gGpuPrimCursor;
    backing->x0 = backing->x2 = left;
    rightEdge                 = *labelEndX;
    // The original reservation is larger than the flat packet written here.
    gGpuPrimCursor                       = (u8*)backing + sizeof(POLY_FT4);
    GPU_PRIMITIVE_COLOR_WORD(backing, 0) = USER_INTERFACE_LABEL_BACKING_COLOR;
    backing->y2 = backing->y3 = top + USER_INTERFACE_LABEL_BACKING_HEIGHT_PIXELS;
    setPolyF4(backing);
    backing->y0 = backing->y1 = top;
    backing->x3               = rightEdge;
    backing->x1               = rightEdge + USER_INTERFACE_LABEL_BACKING_OVERHANG_PIXELS;
    addPrim(gGpuCurrentOt + otIndex, backing);
}

/// Queues a small-font label, its sloped backing plate and a textured underline.
///
/// `x` and `y` locate the plate in content-relative pixels; the text pen starts
/// two pixels right and five down. The final text pen determines the plate's
/// right edge and underline length. Text obeys `textDrawString`'s encoded-line
/// contract, starting in the small face (printable bytes 0x20..0x7A).
/// Borrows the panel and text for the call. Requires resident font/UI textures,
/// glyph storage, a POLY_FT4-sized reservation for the flat backing, a separator
/// packet when its span is positive, and writable panel OT base+1/+2 tags.
/// The unused tail of the backing reservation is retained for packet spacing.
static void _uiDrawUnderlinedLabel(const UiPanel* panel, s32 x, s32 y, const char* text, u32 colorRgb)
{
    enum { USER_INTERFACE_LABEL_OT_OFFSET = 1 };
    TextDrawReq request;
    s32         otIndex;

    otIndex            = panel->otIndex.signedValue + USER_INTERFACE_LABEL_OT_OFFSET;
    x                 += panel->contentOriginX.signedValue;
    y                 += panel->contentOriginY.signedValue;
    request.x          = x + 2;
    request.y          = y + 5;
    request.otIndex    = otIndex;
    request.colorRgb   = colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&request, (const u8*)text);

    _uiQueueLabelBacking(x, y, &request.x, otIndex);

    uiDrawHorizontalSeparator(panel, x - panel->contentOriginX.signedValue, request.x - panel->contentOriginX.signedValue, y + 7 - panel->contentOriginY.signedValue);
}

/// Computes the animated outer bounds used to position a panel label.
///
/// Borrows a live `panel` and writes a separate, writable `animatedRect` for
/// this call. Coordinates and extents are signed 16-bit screen-centered pixels.
/// Animation retains full width and the bottom edge, with style-dependent
/// height; intermediate stores also narrow to sixteen bits.
/// Opening uses (nine - ticks) eighths, at least one, with no upper limit.
/// Closing/hiding replace scales outside 1..8 with one. Other states copy full
/// bounds, including hidden; the caller decides whether to draw the label.
static inline void _uiComputePanelLabelRect(const UiPanel* panel, RECT* animatedRect)
{
    enum { USER_INTERFACE_PANEL_LABEL_MIN_SCALE_EIGHTHS = 1 };
    s32 scaleEighths;

    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (scaleEighths <= 0) {
                scaleEighths = USER_INTERFACE_PANEL_LABEL_MIN_SCALE_EIGHTHS;
            }
            _uiComputeScaledPanelRect(panel, animatedRect, scaleEighths, 0);
            break;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            // The unsigned range test rejects both nonpositive and oversize scales.
            if ((u32)(scaleEighths - USER_INTERFACE_PANEL_LABEL_MIN_SCALE_EIGHTHS) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                scaleEighths = USER_INTERFACE_PANEL_LABEL_MIN_SCALE_EIGHTHS;
            }
            _uiComputeScaledPanelRect(panel, animatedRect, scaleEighths, 1);
            break;
        case USER_INTERFACE_PANEL_OPEN:
        default:
            animatedRect->x = panel->bounds.rect.x;
            animatedRect->y = panel->bounds.rect.y;
            animatedRect->w = panel->bounds.rect.w;
            animatedRect->h = panel->bounds.rect.h;
            break;
    }
}

void uiDrawPanelLabelWithChildFocus(UiPanel* panel, const char* label)
{
    RECT            animatedRect;
    u32             colorRgb;
    s32             labelLeft;
    s32             labelTop;
    const Task*     child;
    const UiObject* childObject;

    colorRgb = USER_INTERFACE_PANEL_LABEL_INACTIVE_COLOR;
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        colorRgb = USER_INTERFACE_PANEL_LABEL_ACTIVE_COLOR;
    }
    child = (PARENT_OF(panel, UiObject, panel))->owner->firstChild;
    if (child != NULL) {
        childObject = child->spawnArg2.pointer;
        if (childObject->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if ((childObject->panel.style & USER_INTERFACE_PANEL_STYLE_MASK) != USER_INTERFACE_PANEL_TITLE_STYLE) {
                colorRgb = USER_INTERFACE_PANEL_LABEL_ACTIVE_COLOR;
            }
        }
    }
    // Follow the animated edge without gating content on lifecycle visibility.
    _uiComputePanelLabelRect(panel, &animatedRect);
    labelLeft                     = animatedRect.x;
    labelTop                      = animatedRect.y;
    labelLeft                     = labelLeft + 1;
    labelTop                      = labelTop + 1;
    panel->otIndex.unsignedValue -= 1;
    _uiDrawUnderlinedLabel(panel, labelLeft - panel->contentOriginX.signedValue, labelTop - panel->contentOriginY.signedValue, label, colorRgb);
    panel->otIndex.unsignedValue += 1;
}

void uiDrawPanelLabel(UiPanel* panel, const char* label)
{
    RECT animatedRect;
    u32  colorRgb;
    s32  labelLeft;
    s32  labelTop;

    colorRgb = USER_INTERFACE_PANEL_LABEL_INACTIVE_COLOR;
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        colorRgb = USER_INTERFACE_PANEL_LABEL_ACTIVE_COLOR;
    }
    // Follow the animated edge without gating content on lifecycle visibility.
    _uiComputePanelLabelRect(panel, &animatedRect);
    labelLeft                     = animatedRect.x;
    labelTop                      = animatedRect.y;
    labelLeft                     = labelLeft + 1;
    labelTop                      = labelTop + 1;
    panel->otIndex.unsignedValue -= 1;
    _uiDrawUnderlinedLabel(panel, labelLeft - panel->contentOriginX.signedValue, labelTop - panel->contentOriginY.signedValue, label, colorRgb);
    panel->otIndex.unsignedValue += 1;
}

UiObject* Ui_SpawnTextBlock(UiOptionDialogRequest* request, s32 unused2, s32 unused3, s32 unused4)
{
    UiObject*       obj;
    UiDialogOption* option;
    TaskSpawnArg    requestArg;
    s32             count;
    s32             maxWidth;
    s32             width;

    obj = NULL;
    if (request->optionCount > 0) {
        requestArg.pointer = request;
        obj                = USER_INTERFACE_SPAWN_OBJECT(&Ui_DialogListDesc, requestArg, 1, 1, NULL);
        if (obj != NULL) {
            RECT rect;

            count    = request->optionCount;
            option   = request->options;
            maxWidth = 0;
            if (request->title == NULL) {
                obj->panel.style = 3;
            }
            for (; count > 0; count--) {
                width = textMeasureLineWidth(option->text);
                if (maxWidth < width) {
                    maxWidth = width;
                }
                option = option->next;
            }
            _uiComputePanelInnerRect(&obj->panel, &obj->panel.bounds.rect, &rect);
            if ((obj->panel.style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
                rect.y += 9;
                rect.h -= 0xB;
                rect.x += 2;
                rect.w -= 4;
            } else {
                rect.y += 2;
                rect.h -= 4;
                rect.x += 2;
                rect.w -= 4;
            }
            obj->panel.contentLeft.signedValue      = -(rect.w >> 1);
            obj->panel.contentRight.unsignedValue   = obj->panel.contentLeft.signedValue + rect.w;
            obj->panel.contentTop.unsignedValue     = -(rect.h >> 1);
            obj->panel.contentBottom.unsignedValue  = obj->panel.contentTop.unsignedValue + rect.h;
            obj->panel.contentOriginX.unsignedValue = rect.x - obj->panel.contentLeft.signedValue;
            obj->panel.contentOriginY.unsignedValue = rect.y - obj->panel.contentTop.unsignedValue;

            // Grow the panel so the widest line and every line fit inside.
            maxWidth                        -= obj->panel.contentRight.signedValue - obj->panel.contentLeft.signedValue;
            obj->panel.bounds.unsignedRect.w = obj->panel.bounds.unsignedRect.w + maxWidth + 0xC;
            obj->panel.bounds.unsignedRect.x = -((s16)obj->panel.bounds.unsignedRect.w / 2);
            maxWidth                         = request->optionCount * 0xF;
            maxWidth                        -= obj->panel.contentBottom.signedValue - obj->panel.contentTop.signedValue;
            obj->panel.bounds.unsignedRect.h = obj->panel.bounds.unsignedRect.h + maxWidth;
            obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
        }
    }
    request->result = 0;
    return obj;
}

/// Insets a standalone frame while retaining signed-halfword origin truncation.
///
/// Requires separate live rectangles. leadingInset applies to the left/top in
/// pixels (two for the sole caller); the right/bottom inset is one. Signed origins are
/// retained before computing extents; stores wrap without dimension checks.
static inline void _uiInsetStandaloneFrame(const RECT* outerRect, RECT* innerRect, s32 leadingInset)
{
    enum { USER_INTERFACE_STANDALONE_FRAME_TRAILING_INSET_PIXELS = 1 };
    s16 innerLeft;
    s16 innerTop;

    innerLeft    = outerRect->x + leadingInset;
    innerRect->x = innerLeft;
    innerTop     = outerRect->y + leadingInset;
    innerRect->y = innerTop;
    innerRect->w = ((outerRect->w + outerRect->x) - innerLeft) - USER_INTERFACE_STANDALONE_FRAME_TRAILING_INSET_PIXELS;
    innerRect->h = ((outerRect->h + outerRect->y) - innerTop) - USER_INTERFACE_STANDALONE_FRAME_TRAILING_INSET_PIXELS;
}

void uiDrawRectFrame(RECT* rect, s32 otIndex, s32 style, const char* title)
{
    enum { USER_INTERFACE_STANDALONE_FRAME_LEADING_INSET = 2 };
    UiPanel  framePanel;
    s32      unusedStackWords[2];
    RECT     innerRect;
    RECT     labelRect;
    UiPanel* labelPanel;
    RECT*    labelBounds;
    u32      colorRgb;
    s32      labelLeft;
    s32      labelTop;
    s32      leadingInset;

    leadingInset                     = USER_INTERFACE_STANDALONE_FRAME_LEADING_INSET;
    framePanel.state                 = USER_INTERFACE_PANEL_OPEN;
    framePanel.otIndex.unsignedValue = otIndex - USER_INTERFACE_FRAME_OT_OFFSET;
    framePanel.style                 = style;
    _uiInsetStandaloneFrame(rect, &innerRect, leadingInset);
    _uiDrawPanelFrame(&framePanel, rect, &innerRect, 0);
    // Retained title path requires uninitialized panel fields; supported calls use NULL.
    if (title != NULL) {
        colorRgb    = GPU_PACK_COLOR_WORD(96, 112, 112, 0);
        labelPanel  = &framePanel;
        labelBounds = &labelRect;
        _uiComputeDrawnPanelRect(labelPanel, labelBounds);
        labelLeft                          = labelRect.x;
        labelTop                           = labelRect.y;
        labelLeft                          = labelLeft + 1;
        labelTop                           = labelTop + 1;
        labelPanel->otIndex.unsignedValue -= 1;
        _uiDrawUnderlinedLabel(labelPanel, labelLeft - labelPanel->contentOriginX.signedValue, labelTop - labelPanel->contentOriginY.signedValue, title, colorRgb);
        labelPanel->otIndex.unsignedValue += 1;
    }
}

void uiSizePanelForText(UiPanel* panel, const u8* text, s32 extraWidthPixels, s32 extraHeightPixels)
{
    enum {
        USER_INTERFACE_TEXT_PANEL_WIDTH_MARGIN_PIXELS  = 5,
        USER_INTERFACE_TEXT_PANEL_HEIGHT_MARGIN_PIXELS = 1,
        USER_INTERFACE_TEXT_PANEL_CENTER_Y_PIXELS      = -20
    };
    struct {
        union {
            u32 packedSize;
            struct {
                u16 widthPixels;  // Low halfword of the little-endian packed result
                u16 heightPixels; // High halfword of the packed result
            } pixels;
        } measured;
        RECT contentRect __attribute__((aligned(8)));
    } layout;
    s32 widthMarginPixels;
    s32 heightMarginPixels;

    layout.measured.packedSize = textMeasureUiTextSize(text);
    // Establish the old content span so resizing preserves frame and style padding.
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &layout.contentRect);
    USER_INTERFACE_CENTER_PANEL_CONTENT(panel, layout.contentRect);
    widthMarginPixels  = extraWidthPixels + USER_INTERFACE_TEXT_PANEL_WIDTH_MARGIN_PIXELS;
    heightMarginPixels = extraHeightPixels + USER_INTERFACE_TEXT_PANEL_HEIGHT_MARGIN_PIXELS;
    uiSetPanelContentSize(panel, layout.measured.pixels.widthPixels + widthMarginPixels, layout.measured.pixels.heightPixels + heightMarginPixels);
    // Reposition after resizing; a later layout refreshes the content origins.
    panel->bounds.rect.x = -(panel->bounds.rect.w / 2);
    panel->bounds.rect.y = -(panel->bounds.rect.h / 2) + USER_INTERFACE_TEXT_PANEL_CENTER_Y_PIXELS;
}

UiObject* uiSpawnObject(const UiObjectDesc* descriptor, TaskSpawnArg contentArg, s32 controlMode, s32 openingDelayTicks, UiObject* parent)
{
    return USER_INTERFACE_SPAWN_OBJECT(descriptor, contentArg, controlMode, openingDelayTicks, parent);
}

/// Detaches and starts closing every UI subtree in a task's child ring.
///
/// `owner` and its circular child ring must remain live. Every child owns a
/// live UiObject in spawnArg2. An already-closing node must be detached;
/// otherwise the closing request would leave the head in place indefinitely.
/// Objects keep their counters and resources for later closing/exit updates.
static inline void _uiStartChildObjectsClosing(Task* owner)
{
    Task* child;

    child = owner->firstChild;
    while (child != NULL) {
        uiStartTreeClosing(child->spawnArg2.pointer, child);
        // Closing detaches the head; its former sibling link no longer walks the ring.
        child = owner->firstChild;
    }
}

void uiStartTreeClosing(UiObject* object, Task* unusedOwningTask)
{
    Task* owner;

    owner = object->owner;
    _uiStartChildObjectsClosing(owner);
    if (object->panel.state != USER_INTERFACE_PANEL_CLOSING) {
        taskDetachFromParent(owner);
        object->panel.state = USER_INTERFACE_PANEL_CLOSING;
    }
}

void uiObjectTaskExit(Task* task)
{
    // Release the owned object before teardown dispatches child exit handlers.
    if (task->spawnArg2.pointer != NULL) {
        memFree(task->spawnArg2.pointer);
    }
    taskKill(task);
}

void uiStartPanelHiding(UiObject* object, Task* unusedOwningTask)
{
    object->panel.state = USER_INTERFACE_PANEL_HIDING;
}

void uiLimitHiddenDelayOrOpen(UiPanel* panel, Task* owningTask, s32 delayTicks)
{
    s16 currentTicks;

    if ((delayTicks != 0) && (panel->state >= USER_INTERFACE_PANEL_HIDDEN)) {
        currentTicks = panel->animationTicks;
        if ((currentTicks < 0) || ((delayTicks + USER_INTERFACE_PANEL_ANIMATION_TICKS) < currentTicks)) {
            panel->animationTicks = (s16)(delayTicks + USER_INTERFACE_PANEL_ANIMATION_TICKS);
        }
    } else {
        uiStartPanelOpening(panel, owningTask);
    }
}

void uiStartPanelOpening(UiPanel* panel, Task* owningTask)
{
    if (panel->state != USER_INTERFACE_PANEL_OPEN) {
        // Unsigned halfword view: a negative sentinel and any count above nine become a full opening.
        if ((u16)panel->animationTicks >= USER_INTERFACE_PANEL_ANIMATION_TICKS + 1) {
            panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        }
        panel->state = USER_INTERFACE_PANEL_OPENING;
    }
}

/// Writes a laid-out panel's content bounds in screen-centered pixels.
///
/// Borrows a live panel and writes a separate writable RECT. Coordinates and
/// dimensions retain their low sixteen bits; signed extents are not clamped.
/// The height covers the full content area before any list top inset.
static inline void _uiReadPanelContentRect(const UiPanel* panel, RECT* contentRect)
{
    contentRect->x = panel->contentOriginX.unsignedValue + panel->contentLeft.unsignedValue;
    contentRect->y = panel->contentOriginY.unsignedValue + panel->contentTop.unsignedValue;
    contentRect->w = panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue;
    contentRect->h = panel->contentBottom.unsignedValue - panel->contentTop.unsignedValue;
}

void uiInitList(UiList* list, const UiPanel* panel)
{
    RECT contentRect;
    u8   itemCount;
    s8   rowHeight;
    s32  availableHeight;

    list->topInset = 0;
    _uiReadPanelContentRect(panel, &contentRect);
    availableHeight  = contentRect.h;
    availableHeight -= list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    itemCount = list->itemCount;
    rowHeight = list->rowHeight;
    if (availableHeight >= (itemCount * rowHeight)) {
        list->visibleRowCount.unsignedValue = itemCount;
    } else {
        list->visibleRowCount.unsignedValue = availableHeight / rowHeight;
        if (list->visibleRowCount.signedValue <= 0) {
            list->visibleRowCount.unsignedValue = 1;
        }
    }
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
    if (list->itemCount <= list->visibleRowCount.signedValue) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->flags                 = 0;
    list->scrollPixelsRemaining = 0;
    list->scrollDirection       = USER_INTERFACE_LIST_STEP_NONE;
    list->rowInputEnabled       = USER_INTERFACE_LIST_ROW_INACTIVE;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode != 0) {
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
}

void uiRefreshListViewport(UiList* list, const UiPanel* panel)
{
    RECT contentRect;
    s32  availableHeight;

    _uiReadPanelContentRect(panel, &contentRect);
    availableHeight  = contentRect.h;
    availableHeight -= list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    if (availableHeight >= list->itemCount * list->rowHeight) {
        list->visibleRowCount.unsignedValue = list->itemCount;
    } else {
        list->visibleRowCount.unsignedValue = availableHeight / list->rowHeight;
        if (list->visibleRowCount.signedValue <= 0) {
            list->visibleRowCount.unsignedValue = 1;
        }
    }
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
    if (list->itemCount <= list->visibleRowCount.signedValue) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->flags = 0;
}

void uiUpdateList(UiList* list, UiPanel* panel)
{
    _uiUpdateListRows(list, panel, 0);
}

/// Sets a list's top pixel inset and refreshes its row capacity and selection bound.
///
/// topInsetPixels narrows to a signed byte before subtraction from the signed
/// content height; 0..127 represents a nonnegative reservation. Uses
/// `uiRefreshListViewport`'s bounds/default/flag-reset contract, preserving scroll
/// state and row input. Borrows the laid-out panel without changing it.
static void _uiRefreshListViewportWithInset(UiList* list, const UiPanel* panel, s32 topInsetPixels)
{
    RECT contentRect;
    u8   itemCount;
    s8   rowHeight;
    s32  availableHeight;

    list->topInset = topInsetPixels;
    _uiReadPanelContentRect(panel, &contentRect);
    availableHeight  = contentRect.h;
    availableHeight -= list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    itemCount = list->itemCount;
    rowHeight = list->rowHeight;
    if (availableHeight >= (itemCount * rowHeight)) {
        list->visibleRowCount.unsignedValue = itemCount;
    } else {
        list->visibleRowCount.unsignedValue = availableHeight / rowHeight;
        if (list->visibleRowCount.signedValue <= 0) {
            list->visibleRowCount.unsignedValue = 1;
        }
    }
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
    if (list->itemCount <= list->visibleRowCount.signedValue) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->flags = 0;
}

void uiEaseAndDrawCursor(const UiPanel* panel, s32 contentX, s32 contentY)
{
    s32 targetX;
    s32 targetY;
    s16 screenOriginX;
    s16 screenOriginY;

    // Translate the content target into the shared screen-centered accumulator.
    screenOriginX = panel->contentOriginX.signedValue;
    screenOriginY = panel->contentOriginY.signedValue;
    targetX       = contentX + screenOriginX;
    targetY       = contentY + screenOriginY;
    targetX     <<= USER_INTERFACE_CURSOR_FRACTION_BITS;
    targetY     <<= USER_INTERFACE_CURSOR_FRACTION_BITS;
    _uiEaseCursorPosition(targetX, targetY);
    targetX = D_80067648 >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    targetY = D_8006764C >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    _uiDrawAnimatedCursor(panel, targetX - panel->contentOriginX.signedValue, targetY - panel->contentOriginY.signedValue);
}

u32 uiGetTextColor(const UiObject* unusedObject, s32 colorIndex)
{
    return D_8006763C[colorIndex];
}

s32 uiGetTextRowsHeight(s32 rowCount)
{
    return (rowCount << 4) - rowCount;
}

void uiDrawTitle(UiPanel* panel, const char* title)
{
    enum { USER_INTERFACE_TITLE_COLOR = GPU_PACK_COLOR_WORD(0x60, 0x70, 0x70, 0) };
    RECT  animatedRect;
    RECT* rect;
    s32   scaleEighths;
    s32   colorRgb;
    s32   x;
    s32   y;

    colorRgb = USER_INTERFACE_TITLE_COLOR;
    rect     = &animatedRect;
    // Follow the animated outer edge while keeping content coordinates stable.
    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (scaleEighths <= 0) {
                scaleEighths = 1;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 0);
            break;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(scaleEighths - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                scaleEighths = 1;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 1);
            break;
        case USER_INTERFACE_PANEL_OPEN:
        default:
            rect->x = panel->bounds.rect.x;
            rect->y = panel->bounds.rect.y;
            rect->w = panel->bounds.rect.w;
            rect->h = panel->bounds.rect.h;
            break;
    }
    x                             = animatedRect.x;
    y                             = animatedRect.y;
    x                             = x + 1;
    y                             = y + 1;
    panel->otIndex.unsignedValue -= 1;
    _uiDrawUnderlinedLabel(panel, x - panel->contentOriginX.signedValue, y - panel->contentOriginY.signedValue, title, colorRgb);
    panel->otIndex.unsignedValue += 1;
}

/// Draws a medium-face encoded text line only while the panel is fully open.
///
/// X and baseline Y are content-relative pixels with no baseline correction;
/// unsigned panel origins are added before signed 16-bit pen stores. RGB is
/// packed in bits 0..23 (R low byte). Mode/alignment narrow to signed bytes
/// selecting TEXT_DRAW_* / TEXT_ALIGNMENT_*. Text follows `textDrawString`'s
/// stream, glyph-range, font-storage and primitive-capacity contract.
///
/// Borrows a live panel and read-only text. The OT halfword is temporarily
/// decremented before signed promotion and restored after drawing; the usual
/// text entry is the original base, with the following tag for outlined modes.
/// All resulting tags must fit the current ordering table. Other states do
/// nothing, including no access to text.
static void _uiDrawOpenPanelText(UiPanel* panel, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment)
{
    enum { USER_INTERFACE_OPEN_PANEL_TEXT_OT_OFFSET = 1 };
    TextDrawReq request;
    s32         adjustedOtIndex;

    if (panel->state == USER_INTERFACE_PANEL_OPEN) {
        panel->otIndex.unsignedValue -= 1;
        request.x                     = panel->contentOriginX.unsignedValue + x;
        request.y                     = panel->contentOriginY.unsignedValue + y;
        adjustedOtIndex               = panel->otIndex.signedValue;
        request.colorRgb              = colorRgb;
        request.glyphTable            = TEXT_GLYPH_TABLE_MEDIUM;
        request.alignment             = (s8)alignment;
        request.otIndex               = adjustedOtIndex + USER_INTERFACE_OPEN_PANEL_TEXT_OT_OFFSET;
        request.drawMode              = (s8)drawMode;
        textDrawString(&request, text);
        panel->otIndex.unsignedValue += 1;
    }
}

void uiPositionRowDialog(UiPanel* dialogPanel, const UiList* list, const UiPanel* listPanel)
{
    enum {
        USER_INTERFACE_ROW_DIALOG_RIGHT_LIMIT  = 150,
        USER_INTERFACE_ROW_DIALOG_BOTTOM_LIMIT = 90
    };
    s32 overflow;
    s32 rightLimit;
    s16 storedLeft;

    rightLimit                 = USER_INTERFACE_ROW_DIALOG_RIGHT_LIMIT;
    dialogPanel->bounds.rect.x = (list->rowTextX.unsignedValue + listPanel->contentOriginX.unsignedValue) + 8;
    dialogPanel->bounds.rect.y = (list->rowTextY.unsignedValue + listPanel->contentOriginY.unsignedValue) - 2;
    storedLeft                 = dialogPanel->bounds.rect.x;
    overflow                   = rightLimit - (storedLeft + dialogPanel->bounds.rect.w);
    if (overflow < 0) {
        dialogPanel->bounds.rect.x = ((u16)storedLeft) + overflow;
    }
    overflow = USER_INTERFACE_ROW_DIALOG_BOTTOM_LIMIT - (dialogPanel->bounds.rect.y + dialogPanel->bounds.rect.h);
    if (overflow < 0) {
        dialogPanel->bounds.rect.y = ((u16)dialogPanel->bounds.rect.y) + overflow;
    }
}

void uiSizePanelForTextDefault(UiPanel* panel, const u8* text)
{
    uiSizePanelForText(panel, text, 0, 0);
}

void uiSizePanelForTextWide(UiPanel* panel, const u8* text)
{
    enum { USER_INTERFACE_WIDE_TEXT_PANEL_EXTRA_WIDTH_PIXELS = 32 };

    uiSizePanelForText(panel, text, USER_INTERFACE_WIDE_TEXT_PANEL_EXTRA_WIDTH_PIXELS, 0);
}

s32 uiIsPanelHidingOrHidden(const UiObject* object)
{
    return object->panel.state >= USER_INTERFACE_PANEL_HIDING;
}

void uiQueueTexturePage(s32 otIndex, s32 blendMode)
{
    enum {
        USER_INTERFACE_TEXTURE_PAGE_BASE            = getTPage(0, 0, 896, 256),
        USER_INTERFACE_TEXTURE_PAGE_BLEND_MASK      = 3,
        USER_INTERFACE_TEXTURE_PAGE_BLEND_SHIFT     = 5,
        USER_INTERFACE_TEXTURE_PAGE_DRAW_TO_DISPLAY = 0,
        USER_INTERFACE_TEXTURE_PAGE_DITHER          = 1
    };
    DR_TPAGE* pageCommand;

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    setDrawTPage(pageCommand, USER_INTERFACE_TEXTURE_PAGE_DRAW_TO_DISPLAY,
                 USER_INTERFACE_TEXTURE_PAGE_DITHER,
                 USER_INTERFACE_TEXTURE_PAGE_BASE | ((blendMode & USER_INTERFACE_TEXTURE_PAGE_BLEND_MASK) << USER_INTERFACE_TEXTURE_PAGE_BLEND_SHIFT));
    addPrim(gGpuCurrentOt + otIndex, pageCommand);
}

void uiSetListSystemCursorSound(UiList* list, s32 enabled)
{
    if (enabled == 0) {
        list->flags &= (u8)~USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND;
        return;
    }
    list->flags |= USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND;
}

void uiFillRectInterior(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord)
{
    _uiFillRectInterior(panel, left, top, width, height, colorWord);
}

void uiDrawRecessedRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord)
{
    uiDrawBeveledRect(panel, left, top, width, height, colorWord, USER_INTERFACE_RECT_RECESSED_BEVEL);
}

void uiDrawRaisedRect(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord)
{
    uiDrawBeveledRect(panel, left, top, width, height, colorWord, USER_INTERFACE_RECT_RAISED_BEVEL);
}

/// Computes the inner panel rectangle used for drawing, clipping and content layout.
///
/// Both rectangles must be non-null and remain live for the call. Coordinates
/// and dimensions are pixels in the caller's coordinate system. For separate
/// rectangles, the inset is two pixels on the left/top and one on the right/bottom,
/// before style-specific text padding. Stores retain the low 16 bits without
/// clamping dimensions. Writes occur in x, y, w, h order; overlapping rectangles
/// observe preceding writes. `unusedPanel` is ignored.
static void _uiComputePanelInnerRect(const UiPanel* unusedPanel, const RECT* outerRect, RECT* innerRect)
{
    enum {
        USER_INTERFACE_PANEL_FRAME_LEADING_INSET_PIXELS  = 2,
        USER_INTERFACE_PANEL_FRAME_TRAILING_INSET_PIXELS = 1
    };

    innerRect->x = outerRect->x + USER_INTERFACE_PANEL_FRAME_LEADING_INSET_PIXELS;
    innerRect->y = outerRect->y + USER_INTERFACE_PANEL_FRAME_LEADING_INSET_PIXELS;
    // Use the stored 16-bit origins when measuring the remaining extents.
    innerRect->w = (outerRect->w + outerRect->x) - innerRect->x - USER_INTERFACE_PANEL_FRAME_TRAILING_INSET_PIXELS;
    innerRect->h = (outerRect->h + outerRect->y) - innerRect->y - USER_INTERFACE_PANEL_FRAME_TRAILING_INSET_PIXELS;
}

void uiUpdatePanelContentLayout(UiPanel* panel, const RECT* outerRect, RECT* innerRect, s32 unused)
{
    RECT contentRect;

    // Center content within the full panel and retain its screen translation.
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &contentRect);
    USER_INTERFACE_CENTER_PANEL_CONTENT(panel, contentRect);
    if (outerRect != NULL) {
        _uiComputePanelInnerRect(panel, outerRect, innerRect);
    }
}

/// Computes a panel's outer drawing rectangle for its current lifecycle.
///
/// Borrows a live panel and writes a live RECT in screen-centered pixels.
/// Opening uses (nine - ticks) eighths with a minimum of one and no upper cap;
/// closing/hiding accept only scales 1..8 and replace other values with one.
/// Animation is bottom-anchored at full width with style-dependent height.
/// Other states copy full bounds, including hidden: this does not test visibility.
/// Stores retain sixteen bits, including the scaled helper's intermediate writes.
static void _uiComputeAnimatedPanelRect(const UiPanel* panel, RECT* rect)
{
    s32 scaleEighths;

    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (scaleEighths <= 0) {
                scaleEighths = 1;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 0);
            return;
        case USER_INTERFACE_PANEL_OPEN:
            break;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            scaleEighths = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(scaleEighths - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                scaleEighths = 1;
            }
            _uiComputeScaledPanelRect(panel, rect, scaleEighths, 1);
            return;
    }
    rect->x = panel->bounds.rect.x;
    rect->y = panel->bounds.rect.y;
    rect->w = panel->bounds.rect.w;
    rect->h = panel->bounds.rect.h;
}

/// Selects opening or retained hidden dispatch for a panel's initial update.
///
/// Borrows a live panel in the initial lifecycle and its owning task. Zero ticks
/// seeds the nine-tick opening span and runs opening immediately. Every nonzero
/// counter selects hidden and runs hidden content immediately; positive delays
/// gain the nine-tick bias, narrowing to s16. Use positive delays up to 32758 to
/// retain a positive biased counter. Negative counters retain their sentinel.
/// Requires the selected handler's resources and a content callback that keeps
/// the panel and task live through return.
static void _uiPanelInitial(UiPanel* panel, Task* owningTask)
{
    if (panel->animationTicks == 0) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        panel->state         += USER_INTERFACE_PANEL_OPENING - USER_INTERFACE_PANEL_INITIAL;
        _uiPanelOpening(panel, owningTask);
    } else {
        if (panel->animationTicks > 0) {
            panel->animationTicks += USER_INTERFACE_PANEL_ANIMATION_TICKS;
        }
        panel->state = USER_INTERFACE_PANEL_HIDDEN;
        _uiPanelHidden(panel, owningTask);
    }
}

/// Updates the opening animation with content input temporarily suspended.
///
/// Borrows a live panel and its owning task; `contentCallback` must keep both live
/// through return. Saves the control word, shifts its low half into the high
/// half, draws/layouts, then runs content. Afterwards elapsed nominal 60-Hz ticks
/// decrement the signed-halfword counter, clamping at zero. Only a callback
/// that kept opening permits the open transition. Restores input only when
/// content left the suspended word unchanged. Requires opening drawing resources.
static void _uiPanelOpening(UiPanel* panel, Task* owningTask)
{
    s32 savedControl;

    // Suspend input during opening, preserving control changes from the callback.
    USER_INTERFACE_RUN_OPENING_PANEL_CONTENT(panel, owningTask, savedControl);
    // Content may change the lifecycle; only an unchanged opening state becomes open.
    panel->animationTicks -= gDisplayState.frameTicks;
    if (panel->animationTicks <= 0) {
        panel->animationTicks = 0;
        if (panel->state == USER_INTERFACE_PANEL_OPENING) {
            panel->state = USER_INTERFACE_PANEL_OPEN;
        }
    }
    if (panel->control.word == (savedControl << USER_INTERFACE_OPENING_CONTROL_SHIFT)) {
        panel->control.word = savedControl;
    }
}

/// Draws and dispatches content for a fully open panel.
///
/// Borrows a live panel and owning task and calls its required content callback
/// after full-bounds layout/drawing. Leaves control, ticks and lifecycle alone;
/// the callback may change them. Requires `_uiLayoutOpenPanel` resources.
static void _uiPanelOpen(UiPanel* panel, Task* owningTask)
{
    _uiLayoutOpenPanel(panel);
    panel->contentCallback(owningTask);
}

/// Shrinks a closing panel until its owning task's exit callback is dispatched.
///
/// Nonnegative signed-halfword counters advance by elapsed nominal 60-Hz ticks.
/// At nine ticks or any negative sentinel, stores nine and dispatches exit
/// without drawing/content or later accesses; exit may release the panel/task.
/// Otherwise sets inactive control, draws the shrinking frame and runs the
/// required content callback, which must keep them live during that phase.
/// Requires shrinking drawing resources and a live task with a live exit handler.
static void _uiPanelClosing(UiPanel* panel, Task* owningTask)
{
    if (panel->animationTicks >= 0) {
        panel->animationTicks += gDisplayState.frameTicks;
    }
    // Negative sentinels also exceed the unsigned threshold; exit before drawing.
    if ((u16)panel->animationTicks >= (u32)USER_INTERFACE_PANEL_ANIMATION_TICKS) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        taskCallExit(owningTask);
        return;
    }
    panel->control.word = USER_INTERFACE_PANEL_INACTIVE;
    _uiLayoutShrinkingPanel(panel);
    panel->contentCallback(owningTask);
}

/// Draws and runs a shrinking retained panel's content with input suspended.
///
/// Borrows a live panel and its owning task; the required content callback must
/// keep both live through return. Requires `_uiLayoutShrinkingPanel` resources.
/// `savedControl` is the complete word captured before the caller's tick update
/// and must still equal the panel's control word on entry. The low half moves
/// into the high half for drawing while input becomes inactive; the old high
/// half is discarded. Restores the complete saved word only if content leaves
/// the suspended word unchanged, preserving other callback requests.
/// Only content may change the lifecycle or ticks during this call.
static inline void _uiRunHidingPanelContent(UiPanel* panel, Task* owningTask, u32 savedControl)
{
    enum { USER_INTERFACE_HIDING_CONTROL_SHIFT = 16 };

    panel->control.word = (u32)panel->control.word << USER_INTERFACE_HIDING_CONTROL_SHIFT;
    _uiLayoutShrinkingPanel(panel);
    panel->contentCallback(owningTask);
    if (panel->control.word == (savedControl << USER_INTERFACE_HIDING_CONTROL_SHIFT)) {
        panel->control.word = savedControl;
    }
}

/// Shrinks a panel into retained hidden dispatch without releasing its object or task.
///
/// Nonnegative signed-halfword counters advance by elapsed nominal 60-Hz ticks.
/// At nine ticks or a negative sentinel, stores -1, advances hiding to hidden
/// and runs hidden content immediately; active control can reopen it there.
/// Otherwise draws shrinking content with temporarily suspended input, keeping
/// callback control changes. The live panel belongs to owningTask; its required
/// callback must keep both live. Requires shrinking/hidden drawing resources.
static void _uiPanelHiding(UiPanel* panel, Task* owningTask)
{
    u32 savedControl;

    savedControl = panel->control.word;
    if (panel->animationTicks >= 0) {
        panel->animationTicks += gDisplayState.frameTicks;
    }
    // Negative sentinels also finish; run retained hidden content in this update.
    if ((u16)panel->animationTicks >= (u32)USER_INTERFACE_PANEL_ANIMATION_TICKS) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_STOPPED;
        panel->state         += USER_INTERFACE_PANEL_HIDDEN - USER_INTERFACE_PANEL_HIDING;
        _uiPanelHidden(panel, owningTask);
        return;
    }
    _uiRunHidingPanelContent(panel, owningTask, savedControl);
}

/// Lays out and runs retained hidden content with input temporarily suspended.
///
/// Borrows a live panel and its owning task. The required content callback must
/// keep both live through return. Low-halfword control moves into the high half
/// while the low half becomes inactive; the previous high half is discarded.
/// The original word is restored only if content leaves the suspended word
/// unchanged, so callback requests survive. Layout uses full panel bounds and
/// queues restricted clipping for content followed by the normal view restore.
/// Requires two DR_AREA slots and writable signed panel OT base/base+3 tags;
/// queued packets remain live until GPU completion. No lifecycle or tick update
/// occurs here apart from changes made by content.
static inline void _uiRunHiddenPanelContent(UiPanel* panel, Task* owningTask)
{
    enum { USER_INTERFACE_HIDDEN_CONTROL_SHIFT = 16 };
    u32 savedControl;
    u32 suspendedControl;

    // Shift control bits without signed overflow, then preserve content requests.
    savedControl        = panel->control.word;
    suspendedControl    = savedControl << USER_INTERFACE_HIDDEN_CONTROL_SHIFT;
    panel->control.word = suspendedControl;
    _uiLayoutHiddenPanel(panel);
    panel->contentCallback(owningTask);
    if (panel->control.word == suspendedControl) {
        panel->control.word = savedControl;
    }
}

/// Updates retained hidden content and requests reopening when its delay or focus permits.
///
/// Borrows the live panel and owning task; the required content callback must
/// keep them live through return. Input is shifted into the upper halfword,
/// then restored only if the callback left that word unchanged. Positive
/// counters decrease by elapsed nominal 60-Hz ticks and clamp to the nine-tick
/// bias, which triggers opening. Negative counters reopen only with active
/// control after the callback; zero stays hidden. Counter stores narrow to s16.
static void _uiPanelHidden(UiPanel* panel, Task* task)
{
    s16 remainingTicks;
    s16 nextTicks;

    // Hidden content still updates while clipping its drawing and suspending input.
    _uiRunHiddenPanelContent(panel, task);
    if (panel->animationTicks > 0) {
        nextTicks             = (u16)panel->animationTicks - gDisplayState.frameTicks;
        panel->animationTicks = nextTicks;
        if (nextTicks < USER_INTERFACE_PANEL_ANIMATION_TICKS) {
            panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        }
    }
    remainingTicks = panel->animationTicks;
    if (((remainingTicks < 0) && (panel->control.word == USER_INTERFACE_PANEL_ACTIVE)) || (remainingTicks == USER_INTERFACE_PANEL_ANIMATION_TICKS)) {
        uiStartPanelOpening(panel, task);
    }
}

/// Runs the owning UI object's current panel lifecycle handler.
///
/// `owningTask->spawnArg2.pointer` must hold a live `UiObject` owned by that
/// task. Its panel state must be 0..5 (initial, opening, open, closing, hiding,
/// hidden); dispatch does not check the index. The selected handler receives
/// the embedded panel and its owning task, and may exit the task and release
/// the object. Neither is accessed after the handler returns.
static void _uiDispatchPanelLifecycle(Task* owningTask)
{
    _UiPanelLifecycleFuncTable6 handlers;
    UiObject*                   object;
    UiPanel*                    panel;

    handlers = Ui_ObjectStates;
    object   = owningTask->spawnArg2.pointer;
    panel    = &object->panel;
    handlers.funcs[panel->state](panel, owningTask);
}

s32 uiGetCursorPositionWord(void)
{
    union {
        UiCursorPosition pixels; // Screen-centered integer pixel coordinates
        s32              word;   // Packed X/Y value for the integer-register return
    } position;
    s16* cursorX = &position.pixels.x.signedValue;

    *cursorX                      = D_80067648 >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    position.pixels.y.signedValue = D_8006764C >> USER_INTERFACE_CURSOR_FRACTION_BITS;
    return position.word;
}

/// Spreads a flat caret's base around the current tip position.
///
/// Requires x1 and x2 at the tip's X. Base coordinates retain sixteen bits.
/// Zero points up (-4/+5 X, +5 Y); every nonzero value points down (-3/+4 X, -4 Y).
static inline void _uiSetFlatCaretBase(POLY_F3* caret, s32 pointsDown)
{
    u16 baseY;

    USER_INTERFACE_SET_CARET_BASE(caret, pointsDown, baseY);
}

void uiDrawFlatCaret(const UiPanel* panel, s32 tipX, s32 tipY, u32 colorRgb, s32 pointsDown)
{
    enum { USER_INTERFACE_CARET_OT_OFFSET = 1 };
    POLY_F3* caret;
    u16      screenX;
    u16      screenY;
    u_long*  orderingTable;

    caret     = gGpuPrimCursor;
    screenX   = panel->contentOriginX.unsignedValue + tipX;
    caret->x2 = screenX;
    caret->x1 = screenX;
    caret->x0 = screenX;
    // Keep the original gouraud-sized reservation for this flat packet.
    gGpuPrimCursor = (u8*)caret + sizeof(POLY_G3);
    screenY        = panel->contentOriginY.unsignedValue + tipY;
    caret->y2      = screenY;
    caret->y1      = screenY;
    caret->y0      = screenY;
    _uiSetFlatCaretBase(caret, pointsDown);
    GPU_PRIMITIVE_COLOR_WORD(caret, 0) = colorRgb * 2;
    setPolyF3(caret);
    orderingTable = gGpuCurrentOt;
    addPrim(&orderingTable[panel->otIndex.signedValue + USER_INTERFACE_CARET_OT_OFFSET], caret);
}

void Ui_WaitCdThenOverlay(Task* task)
{
    UiPanel* temp_s0;

    temp_s0 = task->spawnArg2.pointer;
    if (cdCmdIsIdle() != 0) {
        func_801D4B64(task);
        return;
    }
    temp_s0->animationTicks += gDisplayState.frameTicks;
}

static void Ui_DrawDialogLine(UiList* list, UiObject* object)
{
    UiOptionDialogRequest* request;
    UiDialogOption*        option;
    s32                    var_v0;
    s16                    temp;

    request = object->owner->spawnArg1.pointer;
    var_v0  = list->currentItemIndex;
    option  = request->options;
    if (var_v0 > 0) {
        do {
            option  = option->next;
            var_v0 -= 1;
        } while (var_v0 > 0);
    }
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, option->text, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            temp                = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = (s8)(u8)list->currentItemIndex + 1;
            object->result      = temp;
            return;
        }
        if ((request->flags & USER_INTERFACE_OPTION_DIALOG_CANCELLABLE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            object->resultValue = -1;
            object->result      = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

static void Ui_ListTaskCallback(Task* task)
{
    UiObject*              obj;
    UiOptionDialogRequest* request;
    UiList*                menu;
    char*                  text;
    u8                     base;
    s16                    status;
    Task*                  parent;
    Task*                  child;

    obj         = (UiObject*)task->spawnArg2.pointer;
    request     = task->spawnArg1.pointer;
    menu        = &Ui_DialogLineList;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        base                                = request->optionCount;
        menu->visibleRowCount.unsignedValue = base;
        menu->itemCount                     = base;
        uiFitPanelToList(menu, &(obj)->panel);
        menu->flags  = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->state += 1;
    }
    text = request->title;
    if (text != NULL) {
        uiDrawPanelLabel(&(obj)->panel, text);
    }
    _uiUpdateListRows(menu, &(obj)->panel, 0);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        status = obj->result;
        if ((status == USER_INTERFACE_RESULT_CONFIRM) || (status == USER_INTERFACE_RESULT_CANCEL)) {
            request->result = obj->resultValue;
            parent          = obj->owner;
            child           = parent->firstChild;
            if (child != NULL) {
                do {
                    uiStartTreeClosing((UiObject*)child->spawnArg2.pointer, child);
                    child = parent->firstChild;
                } while (child != NULL);
            }
            if (obj->panel.state != USER_INTERFACE_PANEL_CLOSING) {
                taskDetachFromParent(parent);
                obj->panel.state = USER_INTERFACE_PANEL_CLOSING;
            }
        }
    }
}

void Ui_SetHolderParam(u8* arg0, s32 unused2, s32 unused3)
{
    if (Wip_UiHolder != NULL) {
        Wip_UiHolder->owner->spawnArg1.pointer = arg0;
    }
}

void Ui_SetHolderParamAlt(s32 arg0, s32 unused2, s32 unused3)
{
    if (Wip_UiHolder != NULL) {
        Wip_UiHolder->owner->spawnArg1.value = arg0;
    }
}
