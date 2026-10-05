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

#include "gameplay/effect_tasks.h"
#include "gameplay/item_menu.h"
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

/// Reserved UI task callback with no runtime work.
static void Ui_NoOpTask(Task* unused);

static inline u32 _uiGrey(s32 level);

static void Ui_DrawWindowBorder(RECT* rect, s32 arg1, s32 arg2);

/// Draws the textured frame and background for a panel's rectangles.
static void Ui_DrawPanelFrame(UiPanel* panel, RECT* outer, RECT* inner, s32 unused4);

static void Ui_DrawPanel(UiPanel* panel, RECT* arg1, RECT* arg2, s32 arg3);

static void Ui_SetupClip(UiPanel* panel);

static void Ui_ScaleRect(UiPanel* panel, RECT* rect, s32 arg2, s32 unused4);

static void Ui_LayoutAndClip(UiPanel* panel);

static void Ui_LayoutAndDraw(UiPanel* panel);

static void Ui_LayoutAndDrawAlt(UiPanel* panel);

static void Ui_SetListClip(UiList* list, UiPanel* panel, s32 arg2);

static void Ui_DrawCursor(UiPanel* panel, s32 arg1, s32 arg2);

static void Ui_DrawCaret(UiList* list, UiPanel* panel, s32 arg2);

static inline void _uiFillRectInterior(const UiPanel* panel, s32 left, s32 top, s32 width, s32 height, u32 colorWord);

static void Ui_DrawListHighlight(UiList* list, UiPanel* panel, s32 arg2, s32 unused4);

/// Eases the list cursor a quarter of the way toward (x, y) once per elapsed
/// tick, in 24.8 fixed point, and draws it at the result.
static inline void _uiListMoveCursor(UiPanel* panel, s32 x, s32 y);

/// Updates list navigation, scroll position, row drawing and the cursor.
static void Ui_UpdateListRows(UiList* list, UiPanel* panel, s32 animate);

static void Ui_DrawTextUnderline(UiPanel* panel, s32 x, s32 y, char* arg3, s32 arg4);

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
        UiObjectDesc* _uiSpawnDescriptor     = (descriptorValue);                                                                   \
        TaskSpawnArg  _uiSpawnPayload        = (payloadValue);                                                                      \
        s32           _uiSpawnPanelMode      = (panelModeValue);                                                                    \
        s32           _uiSpawnAnimationTicks = (animationTicksValue);                                                               \
        UiObject*     _uiSpawnParent         = (parentValue);                                                                       \
        TaskDesc      _uiSpawnTaskDesc;                                                                                             \
        Task*         _uiSpawnTask;                                                                                                 \
        UiObject*     _uiSpawnResult;                                                                                               \
        s32           _uiSpawnDescriptorArg;                                                                                        \
                                                                                                                                    \
        _uiSpawnResult                          = NULL;                                                                             \
        _uiSpawnTaskDesc.header.fields.flags    = _uiSpawnDescriptor->taskFlags;                                                    \
        _uiSpawnTaskDesc.header.fields.priority = _uiSpawnDescriptor->taskPriority;                                                 \
        _uiSpawnDescriptorArg                   = _uiSpawnDescriptor->taskDataValue;                                                \
        _uiSpawnTaskDesc.callback               = Ui_DispatchObjectState;                                                           \
        _uiSpawnTaskDesc.data.value             = _uiSpawnDescriptorArg;                                                            \
        _uiSpawnTask                            = Task_SpawnFromTable(&_uiSpawnTaskDesc, 0, _uiSpawnPayload, _uiSpawnResult);       \
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

static void Ui_ComputeVisibleRowsEx(UiList* list, UiPanel* panel, s32 arg2);

static void Ui_DrawTextAtLayout(UiPanel* panel, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);

static void _uiComputePanelInnerRect(const UiPanel* unusedPanel, const RECT* outerRect, RECT* innerRect);

static void Ui_ComputeAnimRect(UiPanel* panel, RECT* rect);

static void Ui_AnimOpenStep(UiPanel* panel, Task* task);

static void Ui_DrawAndCallback(UiPanel* panel, Task* task);

static void Ui_LayoutDrawAndCallback(UiPanel* panel, Task* task);

static void Ui_TickAnimCounter(UiPanel* panel, Task* task);

static void Ui_AnimCloseStep(UiPanel* panel, Task* task);

static void Ui_ClipAndCallback(UiPanel* panel, Task* task);

static void Ui_DispatchObjectState(Task* task);

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
    { { { TASK_BODY_NONE, 0x60 } }, Gp_EnemyDispatch },
    { { { TASK_BODY_TMD, 0x40 } }, func_807077C0, { &D_8075BED4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x41 } }, Gp_UpdateRoomCoords },
    { { { TASK_BODY_NONE, 0x51 } }, func_800D96C8 },
    { { { TASK_BODY_COORD, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807146AC },
    { { { TASK_BODY_TMD, 0xC0 } }, func_8071473C, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_8071489C, { &D_8072C8F0 } },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_TMD, 0xC0 } }, func_807149F0, { &D_8075BED4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80707B14 },
    { { { TASK_BODY_NONE, 0x2F } }, func_800B2910 },
    { { { TASK_BODY_COORD, 0x60 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80707C38 },
    { { { TASK_BODY_TMD, 0xC0 } }, func_80707F84, { &D_8075BED4 } },
    { { { TASK_BODY_COORD, 0xC0 } }, func_80708070 },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0xC0 } }, Tmd_DispatchTask },
    { { { TASK_BODY_NONE, 0xC0 } }, tmdRestoreAttachedBuffersTask },
    { { { TASK_BODY_NONE, 0x60 } }, func_800B5DB8 },
    { { { TASK_BODY_NONE, 0xC0 } }, Ui_NoOpTask },
    { { { TASK_BODY_NONE, 0x70 } }, func_800CFD78 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800CE22C },
    { { { TASK_BODY_NONE, 0xC2 } }, Gp_FadeTileTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807127A8 },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x70 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800B65B0 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800B60C0 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_800D9CC8 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_8070A6E8 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80708778 },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_FadeWorkTask },
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
    Ui_AnimOpenStep,
    [USER_INTERFACE_PANEL_OPENING] = Ui_DrawAndCallback,
    [USER_INTERFACE_PANEL_OPEN]    = Ui_LayoutDrawAndCallback,
    [USER_INTERFACE_PANEL_CLOSING] = Ui_TickAnimCounter,
    [USER_INTERFACE_PANEL_HIDING]  = Ui_AnimCloseStep,
    Ui_ClipAndCallback,
} };

static void Ui_NoOpTask(Task* unused)
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

static void Ui_DrawWindowBorder(RECT* rect, s32 arg1, s32 arg2)
{
    RECT      sp10;
    POLY_GT4* p;
    POLY_GT4* p2;
    DR_MODE*  dr;
    s32       val;
    s32       t;

    p  = gGpuPrimCursor;
    p2 = p + 1;

    p->x0 = p->x2 = rect->x;
    p2->x0 = p2->x2 = rect->x + rect->w;
    p->x1 = p->x3 = p2->x1 = p2->x3 = rect->w >> 1;
    p2->y0 = p2->y1 = p->y0 = p->y1 = rect->y + rect->h;
    p2->y2 = p2->y3 = p->y2 = p->y3 = rect->y;

    gGpuPrimCursor = p + 2;
    if (p->x0 >= p2->x0 || p->y0 <= p->y2) {
        return;
    }

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;

    sp10.w = sp10.h = 0xFF;
    sp10.x = sp10.y = 0;
    setTexWindow(dr, &sp10);
    addPrim(gGpuCurrentOt + arg2, dr);

    if ((arg1 & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_PULSING_STYLE) {
        p->tpage  = 0x1E;
        p2->tpage = 0x1E;
        p->clut   = 0x3C84;
        p2->clut  = 0x3C84;
    } else {
        p->tpage  = 0x1E;
        p2->tpage = 0x1E;
        p->clut   = 0x3C0F;
        p2->clut  = 0x3C0F;
    }

    if (arg1 & USER_INTERFACE_PANEL_DIMMED) {
        GPU_PRIMITIVE_COLOR_WORD(p, 1)  = 0x606060;
        GPU_PRIMITIVE_COLOR_WORD(p2, 1) = 0x606060;
        GPU_PRIMITIVE_COLOR_WORD(p, 0)  = 0x505050;
        GPU_PRIMITIVE_COLOR_WORD(p2, 0) = 0x505050;
        GPU_PRIMITIVE_COLOR_WORD(p, 3)  = 0x808080;
        GPU_PRIMITIVE_COLOR_WORD(p2, 3) = 0x808080;
        GPU_PRIMITIVE_COLOR_WORD(p2, 2) = 0x707070;
        GPU_PRIMITIVE_COLOR_WORD(p, 2)  = 0x707070;
    } else if ((arg1 & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_PULSING_STYLE) {
        val = (rsin(gDisplayState.animFrame << 6) + 0x1000) >> 7;

        t = 0xB0 - val;
        if (t <= 0) {
            t = 1;
        }
        GPU_PRIMITIVE_COLOR_WORD(p2, 1) = GPU_PRIMITIVE_COLOR_WORD(p, 1) = _uiGrey(t);

        t = 0x80 - val;
        if (t <= 0) {
            t = 1;
        }
        GPU_PRIMITIVE_COLOR_WORD(p2, 0) = GPU_PRIMITIVE_COLOR_WORD(p, 0) = _uiGrey(t);

        t = 0x40 - val;
        if (t <= 0) {
            t = 1;
        }
        GPU_PRIMITIVE_COLOR_WORD(p2, 3) = GPU_PRIMITIVE_COLOR_WORD(p, 3) = _uiGrey(t);

        t = 0x30 - val;
        if (t <= 0) {
            t = 1;
        }
        GPU_PRIMITIVE_COLOR_WORD(p, 2) = GPU_PRIMITIVE_COLOR_WORD(p2, 2) = _uiGrey(t);
    } else {
        GPU_PRIMITIVE_COLOR_WORD(p, 1)  = 0xA8A8A8;
        GPU_PRIMITIVE_COLOR_WORD(p2, 1) = 0xA8A8A8;
        GPU_PRIMITIVE_COLOR_WORD(p, 0)  = 0x808080;
        GPU_PRIMITIVE_COLOR_WORD(p2, 0) = 0x808080;
        GPU_PRIMITIVE_COLOR_WORD(p, 3)  = 0x404040;
        GPU_PRIMITIVE_COLOR_WORD(p2, 3) = 0x404040;
        GPU_PRIMITIVE_COLOR_WORD(p2, 2) = 0x303030;
        GPU_PRIMITIVE_COLOR_WORD(p, 2)  = 0x303030;
    }

    p->v0 = p->v1 = 0;
    p->v2 = p->v3 = rect->h;
    p2->v0 = p2->v1 = 0;
    p2->v2 = p2->v3 = rect->h;

    if (p->x0 < 0) {
        if (p2->x0 < 0) {
            p->x1 = p->x3 = p2->x0;
        } else {
            p->x1 = p->x3 = 0;
        }
        setPolyGT4(p);
        p->u0 = p->u2 = 0;
        p->u1 = p->u3 = p->x1 - p->x0;
        addPrim(gGpuCurrentOt + arg2, p);
    }

    if (p2->x0 >= 0) {
        if (p->x0 >= 0) {
            p2->x1 = p2->x3 = p->x0;
            p2->u1 = p2->u3 = 0;
        } else {
            p2->x1 = p2->x3 = 0;
            p2->u1 = p2->u3 = p->u1 & 0x1F;
        }
        setPolyGT4(p2);
        p2->u0 = p2->u2 = p2->u1 + (p2->x0 - p2->x1);
        addPrim(gGpuCurrentOt + arg2, p2);
    }

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setRECT(&sp10, 0, 0, 0x20, 0x20);
    setTexWindow(dr, &sp10);
    addPrim(gGpuCurrentOt + arg2, dr);
}

static void Ui_DrawPanelFrame(UiPanel* panel, RECT* outer, RECT* inner, s32 unused4)
{
    union {
        SPRT*   normal;
        SPRT_8* small;
    } packet;
    POLY_FT4* p;
    TILE*     tile;
    DR_TPAGE* dr;
    s16       t;
    u16       x;
    u16       y;
    u8        color;

    packet.small = gGpuPrimCursor;
    outer->w++;
    outer->h++;
    gGpuPrimCursor     = packet.small + 1;
    packet.small->x0   = outer->x;
    packet.small->y0   = outer->y;
    packet.small->u0   = 0;
    packet.small->v0   = 0x50;
    packet.small->clut = 0x3C03;
    setlen(packet.small, 3);
    setcode(packet.small, 0x75);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, packet.small);

    packet.normal     = gGpuPrimCursor;
    gGpuPrimCursor    = packet.normal + 1;
    packet.normal->x0 = outer->x + outer->w - 8;
    if (packet.normal->x0 > outer->x) {
        packet.normal->y0   = outer->y;
        packet.normal->u0   = 0x10;
        packet.normal->v0   = 0x50;
        packet.normal->clut = 0x3C03;
        setlen(packet.normal, 3);
        setcode(packet.normal, 0x75);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, packet.normal);
    }

    packet.normal     = gGpuPrimCursor;
    gGpuPrimCursor    = packet.normal + 1;
    packet.normal->x0 = outer->x;
    packet.normal->y0 = outer->y + outer->h - 8;
    if (outer->y < packet.normal->y0) {
        packet.normal->u0   = 0x28;
        packet.normal->v0   = 0x50;
        packet.normal->clut = 0x3C03;
        setlen(packet.normal, 3);
        setcode(packet.normal, 0x75);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, packet.normal);
    }

    packet.normal     = gGpuPrimCursor;
    gGpuPrimCursor    = packet.normal + 1;
    packet.normal->x0 = outer->x + outer->w - 8;
    packet.normal->y0 = outer->y + outer->h - 8;
    if (outer->y < packet.normal->y0 && packet.normal->x0 > outer->x) {
        packet.normal->u0   = 0x38;
        packet.normal->v0   = 0x50;
        packet.normal->clut = 0x3C03;
        setlen(packet.normal, 3);
        setcode(packet.normal, 0x75);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, packet.normal);
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = outer->x + 8;
    p->x2          = x;
    p->x0          = x;
    t              = outer->x + outer->w - 8;
    p->x3          = t;
    p->x1          = t;
    y              = outer->y;
    p->y1          = y;
    p->y0          = y;
    t              = outer->y + 8;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 < p->x1) {
        setUV4(p, 0x8, 0x50, 0x10, 0x50, 0x8, 0x58, 0x10, 0x58);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = outer->x + 8;
    p->x2          = x;
    p->x0          = x;
    t              = outer->x + outer->w - 8;
    p->x3          = t;
    p->x1          = t;
    y              = outer->y + outer->h - 8;
    p->y1          = y;
    p->y0          = y;
    t              = outer->y + outer->h;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 < p->x1 && p->y0 > outer->y) {
        setUV4(p, 0x30, 0x50, 0x38, 0x50, 0x30, 0x58, 0x38, 0x58);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = outer->x;
    p->x2          = x;
    p->x0          = x;
    t              = x + 8;
    p->x3          = t;
    p->x1          = t;
    y              = outer->y + 8;
    p->y1          = y;
    p->y0          = y;
    t              = outer->y + outer->h - 8;
    p->y3          = t;
    p->y2          = t;
    if (p->y0 < p->y2) {
        setUV4(p, 0x18, 0x50, 0x20, 0x50, 0x18, 0x57, 0x20, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    t              = outer->x + outer->w;
    x              = t - 8;
    p->x2          = x;
    p->x0          = x;
    p->x3          = t;
    p->x1          = t;
    y              = outer->y + 8;
    p->y1          = y;
    p->y0          = y;
    t              = outer->y + outer->h - 8;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 > outer->x && p->y0 < p->y2) {
        setUV4(p, 0x20, 0x50, 0x28, 0x50, 0x20, 0x57, 0x28, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);
    }
    Ui_DrawWindowBorder(inner, panel->style, panel->otIndex.signedValue + 3);
    if (panel->style & USER_INTERFACE_PANEL_SCREEN_BRIGHTEN) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        color          = (USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks) * 8;
        tile->b0       = color;
        tile->g0       = color;
        tile->r0       = color;
        tile->x0       = -0xA0;
        tile->y0       = -0x78;
        tile->w        = 0x140;
        tile->h        = 0xF0;
        setlen(tile, 3);
        setcode(tile, 0x62);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, tile);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, dr);
    }
}

static void Ui_DrawPanel(UiPanel* panel, RECT* arg1, RECT* arg2, s32 arg3)
{
    RECT      sp10;
    RECT      sp18;
    POLY_F4*  poly;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    u16       t;

    if (panel->style >= 0) {
        if (arg3 != 0) {
            DR_AREA* p;

            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setRECT(&sp10, arg2->x + 0xA0, arg2->y + 0x78, arg2->w, arg2->h);
            sp10.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);
        }
        Ui_DrawPanelFrame(panel, arg1, arg2, arg3);
        if (arg3 != 0) {
            DR_AREA* p;

            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setRECT(&sp18, 0, gDisplayState.drawBuffer * 0x110, 0x140, 0xF0);
            SetDrawArea(p, &sp18);
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, p);
        }
        if (panel->style & USER_INTERFACE_PANEL_DIMMED) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 5);
            setcode(poly, 0x2A);
            poly->b0 = 0;
            poly->g0 = 0;
            poly->r0 = 0;
            x        = arg2->x;
            poly->x2 = x;
            poly->x0 = x;
            t        = arg2->x + arg2->w;
            poly->x3 = t;
            poly->x1 = t;
            y        = arg2->y;
            poly->y1 = y;
            poly->y0 = y;
            t        = arg2->y + arg2->h;
            poly->y3 = t;
            poly->y2 = t;
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue, poly);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue, dr);
        }
    }
}

static void Ui_SetupClip(UiPanel* panel)
{
    RECT     sp10;
    RECT     sp18;
    DR_AREA* p;

    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp18);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp18.y += 9;
        sp18.h -= 0xB;
        sp18.x += 2;
        sp18.w -= 4;
    } else {
        sp18.y += 2;
        sp18.h -= 4;
        sp18.x += 2;
        sp18.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp18.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp18.w;
    panel->contentTop.unsignedValue     = -(sp18.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp18.h;
    panel->contentOriginX.unsignedValue = sp18.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp18.y - panel->contentTop.unsignedValue;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    sp10.x         = 0;
    sp10.w         = 0;
    sp10.h         = 0;
    sp10.y         = gDisplayState.drawBuffer * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 3, p);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    sp10.x         = 0;
    sp10.w         = 0x140;
    sp10.h         = 0xF0;
    sp10.y         = gDisplayState.drawBuffer * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue, p);
}

static void Ui_ScaleRect(UiPanel* panel, RECT* rect, s32 arg2, s32 unused4)
{
    s16 temp;

    if (((u8)panel->style >> 4) == USER_INTERFACE_PANEL_HEIGHT_MODE) {
        rect->w = panel->bounds.rect.w;
        rect->h = (panel->bounds.rect.h * arg2) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS;
        rect->x = panel->bounds.rect.x;
        rect->y = (panel->bounds.rect.y + panel->bounds.rect.h) - rect->h;
    } else {
        rect->w = (panel->bounds.rect.w * arg2) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS;
        temp    = panel->bounds.rect.h;
        if (temp >= 0xC) {
            temp = (((temp - 0xC) * arg2) >> USER_INTERFACE_PANEL_SCALE_FRACTION_BITS) + 0xC;
        } else {
            temp = 0xC;
        }
        rect->h = temp;
        rect->x = panel->bounds.rect.x;
        rect->y = (panel->bounds.rect.y + panel->bounds.rect.h) - rect->h;
        rect->x = panel->bounds.rect.x;
        rect->w = panel->bounds.rect.w;
    }
}

static void Ui_LayoutAndClip(UiPanel* panel)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (panel->state) {
            case USER_INTERFACE_PANEL_OPENING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 0);
                goto after_fill;
            case USER_INTERFACE_PANEL_OPEN:
                break;
            case USER_INTERFACE_PANEL_CLOSING:
            case USER_INTERFACE_PANEL_HIDING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = panel->bounds.rect.x;
        arg1->y = panel->bounds.rect.y;
        arg1->w = panel->bounds.rect.w;
        arg1->h = panel->bounds.rect.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp20);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp20.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp20.w;
    panel->contentTop.unsignedValue     = -(sp20.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp20.h;
    panel->contentOriginX.unsignedValue = sp20.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp20.y - panel->contentTop.unsignedValue;
    if (arg1 != NULL) {
        _uiComputePanelInnerRect(panel, arg1, &sp18);
    }
    Ui_DrawPanel(panel, &sp10, &sp18, 1);
}
}

static void Ui_LayoutAndDraw(UiPanel* panel)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (panel->state) {
            case USER_INTERFACE_PANEL_OPENING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 0);
                goto after_fill;
            case USER_INTERFACE_PANEL_OPEN:
                break;
            case USER_INTERFACE_PANEL_CLOSING:
            case USER_INTERFACE_PANEL_HIDING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = panel->bounds.rect.x;
        arg1->y = panel->bounds.rect.y;
        arg1->w = panel->bounds.rect.w;
        arg1->h = panel->bounds.rect.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp20);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp20.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp20.w;
    panel->contentTop.unsignedValue     = -(sp20.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp20.h;
    panel->contentOriginX.unsignedValue = sp20.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp20.y - panel->contentTop.unsignedValue;
    if (arg1 != NULL) {
        _uiComputePanelInnerRect(panel, arg1, &sp18);
    }
    Ui_DrawPanel(panel, &sp10, &sp18, 0);
}
}

static void Ui_LayoutAndDrawAlt(UiPanel* panel)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (panel->state) {
            case USER_INTERFACE_PANEL_OPENING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 0);
                goto after_fill;
            case USER_INTERFACE_PANEL_OPEN:
                break;
            case USER_INTERFACE_PANEL_CLOSING:
            case USER_INTERFACE_PANEL_HIDING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
                if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(panel, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = panel->bounds.rect.x;
        arg1->y = panel->bounds.rect.y;
        arg1->w = panel->bounds.rect.w;
        arg1->h = panel->bounds.rect.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp20);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp20.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp20.w;
    panel->contentTop.unsignedValue     = -(sp20.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp20.h;
    panel->contentOriginX.unsignedValue = sp20.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp20.y - panel->contentTop.unsignedValue;
    if (arg1 != NULL) {
        _uiComputePanelInnerRect(panel, arg1, &sp18);
    }
    Ui_DrawPanel(panel, &sp10, &sp18, 1);
}
}

static void Ui_SetListClip(UiList* list, UiPanel* panel, s32 arg2)
{
    RECT     sp10;
    DR_AREA* p;
    s32      i;
    s16      temp;

    if (arg2 == 0) {
        for (i = 0; i < 2; i++) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            sp10.x         = panel->contentOriginX.unsignedValue + (panel->contentLeft.unsignedValue + 0xA0);
            temp           = panel->contentOriginY.unsignedValue + (panel->contentTop.unsignedValue + 0x78) + (gDisplayState.drawBuffer * 0x110);
            sp10.y         = temp;
            sp10.y         = temp + list->topInset;
            sp10.w         = panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue;
            temp           = (panel->contentBottom.signedValue - panel->contentTop.signedValue - list->topInset) / list->rowHeight;
            sp10.h         = temp;
            sp10.h         = temp * list->rowHeight;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + (i + panel->otIndex.signedValue) + 1, p);
        }
    } else {
        for (i = 0; i < 2; i++) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            sp10.w         = 0x140;
            sp10.x         = 0;
            sp10.h         = 0xF0;
            sp10.y         = gDisplayState.drawBuffer * 0x110;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + (i + panel->otIndex.signedValue) + 1, p);
        }
    }
}

static void Ui_DrawCursor(UiPanel* panel, s32 arg1, s32 arg2)
{
    SPRT_8*   p;
    DR_TPAGE* dr;
    s32       n;
    s32       y;
    s32       row;
    s32       half;
    s32       t;

    n = (u32)gDisplayState.vsyncCount >> 3;
    if (panel->control.word != USER_INTERFACE_PANEL_INACTIVE) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        p->x0          = panel->contentOriginX.unsignedValue + arg1 - 8;
        y              = panel->contentOriginY.unsignedValue;
        p->clut        = 0x3C0A;
        setlen(p, 3);
        setcode(p, 0x75);
        p->y0 = y + arg2 - 2;
        arg2  = n / 3;
        row   = arg2;
        arg2  = n - row * 3;
        half  = row / 2;
        half  = row - half * 2;
        t     = arg2 * 8 - 0x18;
        p->u0 = t;
        t     = half * 8 + 0x30;
        p->v0 = t;
        addPrim(gGpuCurrentOt + 4, p);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, 0x1E);
        addPrim(gGpuCurrentOt + 4, dr);
    }
}

static void Ui_DrawCaret(UiList* list, UiPanel* panel, s32 arg2)
{
    POLY_G3* p;
    s16      x;
    s32      y;
    s32      y0;
    u16      t;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyG3(p);

    x     = panel->bounds.rect.x + panel->bounds.rect.w - 5;
    p->x2 = x;
    p->x1 = x;
    p->x0 = x;

    y     = panel->contentOriginY.unsignedValue;
    p->y2 = y;
    p->y1 = y;
    p->y0 = y;

    if (arg2 == 0) {
        y    += panel->contentTop.unsignedValue;
        p->y0 = y;
        if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            p->y0 -= (((u32)gDisplayState.vsyncCount >> 3) & 3) - 3;
        }
        p->y0 += list->topInset;
        p->x1 -= 4;
        t      = p->y0 + 5;
        p->x2 += 5;
        p->y2  = t;
        p->y1  = t;
    } else {
        y0    = y + 2;
        p->y0 = panel->contentBottom.unsignedValue + y0;
        if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            p->y0 += (((u32)gDisplayState.vsyncCount >> 3) & 3) - 3;
        }
        p->x1 -= 3;
        t      = p->y0 - 4;
        p->x2 += 4;
        p->y2  = t;
        p->y1  = t;
    }

    p->r0 = 0x9F;
    p->g0 = 0x7F;
    p->b0 = 0xBF;
    p->r2 = 0xDF;
    p->r1 = 0xDF;
    p->g2 = 0xCF;
    p->g1 = 0xCF;
    p->b2 = 0xFF;
    p->b1 = 0xFF;
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, p);
}

void Ui_UpdateLayoutSize(UiPanel* panel, s32 arg1, s32 arg2)
{
    RECT sp10;

    if (arg1 > 0) {
        panel->bounds.rect.w = (panel->bounds.rect.w - (panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue)) + arg1;
    }
    if (arg2 > 0) {
        panel->bounds.rect.h = (panel->bounds.rect.h - (panel->contentBottom.unsignedValue - panel->contentTop.unsignedValue)) + arg2;
    }
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp10);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp10.y += 9;
        sp10.h -= 0xB;
        sp10.x += 2;
        sp10.w -= 4;
    } else {
        sp10.y += 2;
        sp10.h -= 4;
        sp10.x += 2;
        sp10.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp10.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp10.w;
    panel->contentTop.unsignedValue     = -(sp10.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp10.h;
    panel->contentOriginX.unsignedValue = sp10.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp10.y - panel->contentTop.unsignedValue;
}

void Ui_LayoutListPanel(UiList* arg0_, UiPanel* arg1_)
{
    UiList*  arg0;
    UiPanel* arg1;
    RECT     sp10;
    s32      height;
    s32      overflow;
    s32      growth;

    arg0 = arg0_;
    arg1 = arg1_;

    if (arg0->visibleRowCount.signedValue == 0) {
        arg0->visibleRowCount.signedValue = arg0->itemCount;
    } else if (arg0->itemCount < arg0->visibleRowCount.signedValue) {
        arg0->visibleRowCount.signedValue = arg0->itemCount;
    }

    growth               = arg0->visibleRowCount.signedValue * arg0->rowHeight;
    growth              -= arg1->contentBottom.signedValue - arg1->contentTop.signedValue;
    arg1->bounds.rect.h += growth;
    overflow             = 0x98 - (arg1->bounds.rect.x + arg1->bounds.rect.w);
    if (overflow < 0) {
        arg1->bounds.rect.x += overflow;
    }
    overflow = 0x70 - (arg1->bounds.rect.y + arg1->bounds.rect.h);
    if (overflow < 0) {
        arg1->bounds.rect.y += overflow;
    }

    _uiComputePanelInnerRect(arg1, &arg1->bounds.rect, &sp10);
    if ((arg1->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp10.y += 9;
        sp10.h -= 0xB;
        sp10.x += 2;
        sp10.w -= 4;
    } else {
        sp10.y += 2;
        sp10.h -= 4;
        sp10.x += 2;
        sp10.w -= 4;
    }
    arg1->contentLeft.signedValue      = -(sp10.w >> 1);
    arg1->contentRight.signedValue     = arg1->contentLeft.signedValue + sp10.w;
    arg1->contentTop.signedValue       = -(sp10.h >> 1);
    arg1->contentBottom.signedValue    = arg1->contentTop.signedValue + sp10.h;
    arg1->contentOriginX.unsignedValue = sp10.x - arg1->contentLeft.signedValue;
    arg1->contentOriginY.unsignedValue = sp10.y - arg1->contentTop.signedValue;

    arg0->topInset = 0;
    sp10.x         = arg1->contentOriginX.unsignedValue + arg1->contentLeft.signedValue;
    sp10.y         = arg1->contentOriginY.unsignedValue + arg1->contentTop.signedValue;
    sp10.w         = arg1->contentRight.signedValue - arg1->contentLeft.signedValue;
    sp10.h         = arg1->contentBottom.signedValue - arg1->contentTop.signedValue;
    height         = sp10.h;
    height        -= arg0->topInset;
    if (arg0->rowHeight == 0) {
        arg0->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    if (height >= arg0->itemCount * arg0->rowHeight) {
        arg0->visibleRowCount.signedValue = arg0->itemCount;
    } else {
        arg0->visibleRowCount.signedValue = height / arg0->rowHeight;
        if (arg0->visibleRowCount.signedValue <= 0) {
            arg0->visibleRowCount.signedValue = 1;
        }
    }
    if (arg0->selectedItemIndex >= arg0->itemCount) {
        arg0->selectedItemIndex = arg0->itemCount - 1;
    }
    if (arg0->itemCount <= arg0->visibleRowCount.signedValue) {
        arg0->firstVisibleItemIndex.unsignedValue = 0;
    }
    arg0->flags                 = 0;
    arg0->scrollPixelsRemaining = 0;
    arg0->scrollDirection       = USER_INTERFACE_LIST_STEP_NONE;
    arg0->rowInputEnabled       = USER_INTERFACE_LIST_ROW_INACTIVE;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode != 0) {
        arg0->selectedItemIndex                   = 0;
        arg0->firstVisibleItemIndex.unsignedValue = 0;
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

void Ui_DrawBeveledRect(UiPanel* panel, s32 x, s32 y, s32 width, s32 height, u32 color, s32 inset)
{
    LINE_F3* l;
    u16      t;

    _uiFillRectInterior(panel, x, y, width, height, color);

    l                              = gGpuPrimCursor;
    l->x2                          = panel->contentOriginX.unsignedValue + x + 1;
    t                              = panel->contentOriginX.unsignedValue + (x + width);
    l->x1                          = t;
    l->x0                          = t;
    gGpuPrimCursor                 = l + 1;
    l->y0                          = panel->contentOriginY.unsignedValue + y;
    t                              = panel->contentOriginY.unsignedValue + (y + height);
    l->y2                          = t;
    l->y1                          = t;
    GPU_PRIMITIVE_COLOR_WORD(l, 0) = ((inset & 1) == 0) ? GPU_PACK_COLOR_WORD(0x58, 0x60, 0x50, 0) : GPU_PACK_COLOR_WORD(0x10, 0x18, 0x10, 0);
    setLineF3(l);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, l);

    l                              = gGpuPrimCursor;
    t                              = panel->contentOriginX.unsignedValue + x;
    l->x1                          = t;
    l->x2                          = t;
    l->x0                          = panel->contentOriginX.unsignedValue + (x + width) - 1;
    gGpuPrimCursor                 = l + 1;
    t                              = panel->contentOriginY.unsignedValue + y;
    l->y1                          = t;
    l->y0                          = t;
    l->y2                          = panel->contentOriginY.unsignedValue + (y + height);
    GPU_PRIMITIVE_COLOR_WORD(l, 0) = ((inset & 1) == 0) ? GPU_PACK_COLOR_WORD(0x10, 0x18, 0x10, 0) : GPU_PACK_COLOR_WORD(0x58, 0x60, 0x50, 0);
    setLineF3(l);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, l);
}

static void Ui_DrawListHighlight(UiList* list, UiPanel* panel, s32 arg2, s32 unused4)
{
    UiPanel* a1;
    s32      h;
    s32      x1;

    a1 = panel;
    h  = list->rowHeight;
    x1 = a1->contentLeft.signedValue;
    a1->otIndex.unsignedValue++;
    _uiFillRectInterior(panel, x1, arg2 - h, a1->contentRight.signedValue - x1 - 1, h, GPU_PACK_COLOR_WORD(0x1F, 0x74, 0x01, 0));
    a1->otIndex.unsignedValue--;
}

/// Eases the list cursor a quarter of the way toward (x, y) once per elapsed
/// tick, in 24.8 fixed point, and draws it at the result.
static inline void _uiListMoveCursor(UiPanel* panel, s32 x, s32 y)
{
    s32 i;
    s16 baseX;
    s16 baseY;
    s32 targetX;
    s32 targetY;
    u8  ticks;

    i         = 0;
    baseX     = panel->contentOriginX.signedValue;
    baseY     = panel->contentOriginY.signedValue;
    targetX   = x + baseX;
    targetY   = y + baseY;
    targetX <<= 8;
    targetY <<= 8;
    ticks     = gDisplayState.frameTicks;
    if (ticks != 0) {
        do {
            i++;
            D_80067648 += (targetX - D_80067648) >> 2;
            D_8006764C += (targetY - D_8006764C) >> 2;
        } while (i < ticks);
    }
    targetX = D_80067648 >> 8;
    targetY = D_8006764C >> 8;
    Ui_DrawCursor(panel, targetX - panel->contentOriginX.signedValue, targetY - panel->contentOriginY.signedValue);
}

static void Ui_UpdateListRows(UiList* list, UiPanel* panel, s32 animate)
{
    s32 step;
    s32 playSound;
    s32 highlight;
    s32 itemData;
    s32 margin;
    s32 rowY;
    s32 highlightY;
    s32 cursorX;
    s32 cursorY;
    s32 rows;
    s32 item;
    s32 i;
    s32 inset;
    s32 h;
    s32 state;
    s32 sound;
    s32 center;
    s32 rowH;

    cursorY   = 0;
    step      = 0;
    playSound = 0;
    highlight = 0;
    margin    = list->visibleRowCount.signedValue >> 2;
    itemData  = D_80067640;
    if (margin < 2) {
        margin = 0;
    }
    // Each dispatch publishes fresh results and panel-local row coordinates.
    list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_NONE;
    list->actionResult              = USER_INTERFACE_RESULT_NONE;
    list->rowTextX.signedValue      = panel->contentLeft.unsignedValue + 2;
    state                           = panel->control.word;
    if (state >= USER_INTERFACE_PANEL_REQUEST_MIN) {
        switch (state) {
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
    rows = list->visibleRowCount.signedValue;
    if (rows < list->itemCount) {
        if (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue > 0) {
            Ui_DrawCaret(list, panel, 0);
        }
        if (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue < list->itemCount) {
            Ui_DrawCaret(list, panel, 1);
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
                    s32 top                     = list->rowTextY.signedValue + 7;
                    cursorY                     = top - list->rowHeight + (list->visibleRowCount.signedValue - 1) * list->rowHeight;
                    list->rowTextY.signedValue -= list->rowHeight - list->scrollPixelsRemaining;
                } else {
                    s32 top                     = list->rowTextY.signedValue + 7;
                    cursorY                     = top - list->rowHeight;
                    list->rowTextY.signedValue -= list->scrollPixelsRemaining;
                }
                rows++;
            }
        }
    } else {
        list->rowTextY.signedValue = panel->contentTop.unsignedValue + list->rowHeight;
    }
    list->rowTextY.signedValue += list->topInset;
    highlightY                  = list->rowTextY.signedValue;
    rowY                        = highlightY;
    if (list->itemCount == 0) {
        s32 top = highlightY + 7;

        cursorX = list->rowTextX.signedValue - 2;
        cursorY = top - list->rowHeight;
        _uiListMoveCursor(panel, cursorX, cursorY);
        return;
    }
    if (list->scrollPixelsRemaining != 0) {
        Ui_SetListClip(list, panel, 1);
    }
    item = list->firstVisibleItemIndex.signedValue;
    for (i = 0; i < rows; i++) {
        if (item == list->selectedItemIndex) {
            if (list->scrollDirection == USER_INTERFACE_LIST_STEP_NONE) {
                if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
                    list->rowInputEnabled = USER_INTERFACE_LIST_ROW_ACTIVE;
                    highlight             = 1;
                    list->colorRgb        = itemData;
                    highlightY            = rowY;
                } else {
                    list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
                    list->colorRgb        = itemData;
                }
            }
            h       = list->rowHeight;
            center  = rowY - (h - 1) / 2;
            cursorY = center - 1;
            if (h == 8) {
                cursorY = center - 2;
            }
        } else {
            list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            list->colorRgb        = itemData;
        }
        inset                  = 0;
        rowH                   = list->rowHeight;
        list->currentItemIndex = item;
        if (rowH == 10) {
            inset = 3;
        } else if (rowH < 10) {
            inset = 2;
        } else if (rowH >= 16) {
            inset = rowH - 15;
        }
        list->rowTextY.signedValue = rowY - inset;
        if (list->flags & USER_INTERFACE_LIST_SHARED_ROW_CALLBACK) {
            list->rowCallbacks[0](list, PARENT_OF(panel, UiObject, panel));
        } else {
            list->rowCallbacks[item](list, PARENT_OF(panel, UiObject, panel));
        }
        if (item == list->selectedItemIndex && list->actionResult == USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
            highlight = 0;
        }
        item++;
        rowY  = list->rowTextY.signedValue + inset;
        rowY += list->rowHeight;
        if (item >= list->itemCount) {
            item -= list->itemCount;
        }
    }
    if (highlight == 1 && list->rowHeight != USER_INTERFACE_LIST_PREVIEW_ROW_HEIGHT) {
        Ui_DrawListHighlight(list, panel, highlightY, 0);
    }
    cursorX = list->rowTextX.signedValue - 2;
    if (list->scrollPixelsRemaining != 0) {
        Ui_SetListClip(list, panel, 0);
    } else if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (list->actionResult == USER_INTERFACE_RESULT_NONE && padCheckButtons(animate, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT) == 0) {
            if (padCheckButtons(animate, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                playSound                = 1;
                list->navigationStep     = USER_INTERFACE_LIST_STEP_PREVIOUS;
                step                     = -1;
                list->selectedItemIndex -= 1;
            } else if (padCheckButtons(animate, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                playSound                = 1;
                step                     = 1;
                list->selectedItemIndex += 1;
                list->navigationStep     = USER_INTERFACE_LIST_STEP_NEXT;
            } else if (list->visibleRowCount.signedValue < list->itemCount && list->wrapNavigation == 0) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L1) != 0) {
                    if (list->selectedItemIndex != 0) {
                        playSound = 1;
                    }
                    list->navigationStep     = USER_INTERFACE_LIST_STEP_PREVIOUS;
                    step                     = -1;
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
                        playSound = 1;
                    }
                    list->navigationStep     = USER_INTERFACE_LIST_STEP_NEXT;
                    step                     = 1;
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
            step                     = list->navigationStep;
        }
    }
    if ((panel->control.word == USER_INTERFACE_PANEL_ACTIVE || panel->control.modes.suspended == USER_INTERFACE_PANEL_ACTIVE) && list->rowHeight != USER_INTERFACE_LIST_PREVIEW_ROW_HEIGHT) {
        _uiListMoveCursor(panel, cursorX, cursorY);
    }
    if (step == -1) {
        if (list->selectedItemIndex < 0) {
            if (list->wrapNavigation != 0) {
                list->selectedItemIndex += list->itemCount;
            } else {
                playSound               = 0;
                list->actionResult      = USER_INTERFACE_LIST_ACTION_AT_START;
                list->selectedItemIndex = 0;
                list->navigationStep    = USER_INTERFACE_LIST_STEP_NEXT;
            }
        }
        if (list->itemCount != list->visibleRowCount.signedValue) {
            s32 edge = margin - 1;

            if (list->firstVisibleItemIndex.signedValue + edge >= list->selectedItemIndex % list->itemCount) {
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
    } else if (step == 1) {
        if (list->selectedItemIndex >= list->itemCount) {
            if (list->wrapNavigation != 0) {
                list->selectedItemIndex -= list->itemCount;
            } else {
                playSound               = 0;
                list->selectedItemIndex = list->itemCount - 1;
                list->actionResult      = USER_INTERFACE_LIST_ACTION_AT_END;
                list->navigationStep    = USER_INTERFACE_LIST_STEP_PREVIOUS;
            }
        }
        if (list->itemCount != list->visibleRowCount.signedValue) {
            if (list->selectedItemIndex % list->itemCount >= (list->firstVisibleItemIndex.signedValue + list->visibleRowCount.signedValue - margin) % list->itemCount && (list->wrapNavigation != 0 || list->firstVisibleItemIndex.signedValue < list->itemCount - list->visibleRowCount.signedValue)) {
                list->scrollDirection       = USER_INTERFACE_LIST_STEP_NEXT;
                list->scrollPixelsRemaining = list->rowHeight;
            }
        }
    }
    if (playSound != 0) {
        sound = SOUND_SYSTEM_CURSOR;
        if (!(list->flags & USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND)) {
            sound = SOUND_MENU_CURSOR;
        }
        sndEvtRequestScriptStart(sound, 0, 0);
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

void Ui_DrawVBar(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s32       x;

    if (arg1 < arg2) {
        p     = gGpuPrimCursor;
        x     = panel->contentOriginX.unsignedValue + arg3;
        p->x0 = p->x2 = x - 3;
        p->x1 = p->x3  = x + 5;
        gGpuPrimCursor = p + 1;
        p->y0 = p->y1 = panel->contentOriginY.unsignedValue + arg1;
        p->y2 = p->y3 = panel->contentOriginY.unsignedValue + arg2;
        setUV4(p, 0x70, 0x50, 0x77, 0x50, 0x70, 0x57, 0x77, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 2, p);
    }
}

static void Ui_DrawTextUnderline(UiPanel* panel, s32 x, s32 y, char* arg3, s32 arg4)
{
    TextDrawReq req;
    POLY_F4*    p;
    s16         textX;
    s32         otIdx;

    otIdx          = panel->otIndex.signedValue + 1;
    x             += panel->contentOriginX.signedValue;
    y             += panel->contentOriginY.signedValue;
    req.x          = x + 2;
    req.y          = y + 5;
    req.otIndex    = otIdx;
    req.colorRgb   = arg4;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    Text_DrawString(&req, (u8*)arg3);

    p     = gGpuPrimCursor;
    p->x0 = p->x2 = x;
    textX         = req.x;
    // The original reservation is larger than the flat packet written here.
    gGpuPrimCursor                 = (u8*)p + sizeof(POLY_FT4);
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x02, 0x10, 0x02, 0);
    p->y2 = p->y3 = y + 7;
    setPolyF4(p);
    p->y0 = p->y1 = y;
    p->x3         = textX;
    p->x1         = textX + 3;
    addPrim(gGpuCurrentOt + otIdx, p);

    uiDrawHorizontalSeparator(panel, x - panel->contentOriginX.signedValue, req.x - panel->contentOriginX.signedValue, y + 7 - panel->contentOriginY.signedValue);
}

void Ui_DrawTextColored(UiPanel* panel, char* arg1)
{
    RECT      sp18;
    RECT*     r;
    s32       var_a2;
    s32       color;
    s32       x;
    s32       y;
    Task*     child;
    UiObject* childObject;

    color = 0x505040;
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        color = 0x806020;
    }
    child = (PARENT_OF(panel, UiObject, panel))->owner->firstChild;
    if (child != NULL) {
        childObject = child->spawnArg2.pointer;
        if (childObject->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if ((childObject->panel.style & USER_INTERFACE_PANEL_STYLE_MASK) != USER_INTERFACE_PANEL_TITLE_STYLE) {
                color = 0x806020;
            }
        }
    }
    r = &sp18;
    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 0);
            break;
        case USER_INTERFACE_PANEL_OPEN:
            goto block_default;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = panel->bounds.rect.x;
            r->y = panel->bounds.rect.y;
            r->w = panel->bounds.rect.w;
            r->h = panel->bounds.rect.h;
            break;
    }
    x                             = sp18.x;
    y                             = sp18.y;
    x                             = x + 1;
    y                             = y + 1;
    panel->otIndex.unsignedValue -= 1;
    Ui_DrawTextUnderline(panel, x - panel->contentOriginX.signedValue, y - panel->contentOriginY.signedValue, arg1, color);
    panel->otIndex.unsignedValue += 1;
}

void Ui_DrawText(UiPanel* panel, char* arg1)
{
    RECT  sp18;
    RECT* r;
    s32   var_a2;
    s32   color;
    s32   x;
    s32   y;

    color = 0x505040;
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        color = 0x806020;
    }
    r = &sp18;
    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 0);
            break;
        case USER_INTERFACE_PANEL_OPEN:
            goto block_default;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = panel->bounds.rect.x;
            r->y = panel->bounds.rect.y;
            r->w = panel->bounds.rect.w;
            r->h = panel->bounds.rect.h;
            break;
    }
    x                             = sp18.x;
    y                             = sp18.y;
    x                             = x + 1;
    y                             = y + 1;
    panel->otIndex.unsignedValue -= 1;
    Ui_DrawTextUnderline(panel, x - panel->contentOriginX.signedValue, y - panel->contentOriginY.signedValue, arg1, color);
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
                width = Text_MeasureWidth(option->text);
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

void Ui_DrawTextInRect(RECT* rect, s32 arg1, s32 arg2, char* arg3)
{
    UiPanel  sp18;
    s32      pad[2];
    RECT     sp48;
    RECT     sp50;
    UiPanel* self;
    RECT*    r;
    s32      var_a2;
    s32      color;
    s32      x;
    s32      y;
    s32      two;
    s16      temp_t0;
    s16      temp_t1;

    two                        = 2;
    sp18.state                 = USER_INTERFACE_PANEL_OPEN;
    sp18.otIndex.unsignedValue = arg1 - 3;
    sp18.style                 = arg2;
    temp_t0                    = rect->x + two;
    sp48.x                     = temp_t0;
    temp_t1                    = rect->y + two;
    sp48.y                     = temp_t1;
    sp48.w                     = ((rect->w + rect->x) - temp_t0) - 1;
    sp48.h                     = ((rect->h + rect->y) - temp_t1) - 1;
    Ui_DrawPanelFrame(&sp18, rect, &sp48, 0);
    if (arg3 != NULL) {
        color = 0x707060;
        self  = &sp18;
        r     = &sp50;
        switch (self->state) {
            case USER_INTERFACE_PANEL_OPENING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - self->animationTicks;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(self, r, var_a2, 0);
                break;
            case USER_INTERFACE_PANEL_OPEN:
                goto block_default;
            case USER_INTERFACE_PANEL_CLOSING:
            case USER_INTERFACE_PANEL_HIDING:
                var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - self->animationTicks;
                if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(self, r, var_a2, 1);
                break;
            default:
            block_default:
                r->x = self->bounds.rect.x;
                r->y = self->bounds.rect.y;
                r->w = self->bounds.rect.w;
                r->h = self->bounds.rect.h;
                break;
        }
        x                            = sp50.x;
        y                            = sp50.y;
        x                            = x + 1;
        y                            = y + 1;
        self->otIndex.unsignedValue -= 1;
        Ui_DrawTextUnderline(self, x - self->contentOriginX.signedValue, y - self->contentOriginY.signedValue, arg3, color);
        self->otIndex.unsignedValue += 1;
    }
}

void Ui_SizeFromText(UiPanel* panel, u8* arg1, s32 arg2, s32 arg3)
{
    struct {
        union {
            s32 as32;
            struct {
                u16 w;
                u16 h;
            } hw;
        } dims;
        s32  pad;
        RECT rect;
    } sp;
    s32 t;
    s32 u;

    sp.dims.as32 = Text_MeasureMultiLine(arg1);
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp.rect);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp.rect.y += 9;
        sp.rect.h -= 0xB;
        sp.rect.x += 2;
        sp.rect.w -= 4;
    } else {
        sp.rect.y += 2;
        sp.rect.h -= 4;
        sp.rect.x += 2;
        sp.rect.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp.rect.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp.rect.w;
    panel->contentTop.unsignedValue     = -(sp.rect.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp.rect.h;
    panel->contentOriginX.unsignedValue = sp.rect.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp.rect.y - panel->contentTop.unsignedValue;
    t                                   = arg2 + 5;
    u                                   = arg3 + 1;
    Ui_UpdateLayoutSize(panel, sp.dims.hw.w + t, sp.dims.hw.h + u);
    panel->bounds.rect.x = -(panel->bounds.rect.w / 2);
    panel->bounds.rect.y = -(panel->bounds.rect.h / 2) - 0x14;
}

UiObject* Ui_SpawnFromDesc(UiObjectDesc* descriptor, TaskSpawnArg spawnArg1, s32 controlMode, s32 animationTicks, UiObject* parent)
{
    return USER_INTERFACE_SPAWN_OBJECT(descriptor, spawnArg1, controlMode, animationTicks, parent);
}

void Ui_TeardownTree(UiObject* object, Task* unused2)
{
    Task* temp_s0;
    Task* child;

    temp_s0 = object->owner;
    child   = temp_s0->firstChild;
    if (child != NULL) {
        do {
            Ui_TeardownTree(child->spawnArg2.pointer, child);
            child = temp_s0->firstChild;
        } while (child != NULL);
    }
    if (object->panel.state != USER_INTERFACE_PANEL_CLOSING) {
        taskDetachFromParent(temp_s0);
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

void Ui_SetState4(UiObject* object, Task* unused2)
{
    object->panel.state = USER_INTERFACE_PANEL_HIDING;
}

void Ui_ClampAnimOrClose(UiPanel* panel, Task* task, s32 arg2)
{
    s16 temp_v1;

    if ((arg2 != 0) && (panel->state >= USER_INTERFACE_PANEL_HIDDEN)) {
        temp_v1 = panel->animationTicks;
        if ((temp_v1 < 0) || ((arg2 + USER_INTERFACE_PANEL_ANIMATION_TICKS) < temp_v1)) {
            panel->animationTicks = (s16)(arg2 + USER_INTERFACE_PANEL_ANIMATION_TICKS);
        }
    } else {
        uiStartPanelOpening(panel, task);
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

void Ui_InitList(UiList* list, UiPanel* panel)
{
    RECT     sp;
    UiPanel* a1;
    s16      temp_v0;
    u8       temp_a2;
    s8       temp_v1;
    s32      height;

    a1             = panel;
    list->topInset = 0;
    sp.x           = a1->contentOriginX.unsignedValue + a1->contentLeft.unsignedValue;
    sp.y           = a1->contentOriginY.unsignedValue + a1->contentTop.unsignedValue;
    sp.w           = a1->contentRight.unsignedValue - a1->contentLeft.unsignedValue;
    temp_v0        = a1->contentBottom.unsignedValue - a1->contentTop.unsignedValue;
    height         = temp_v0;
    sp.h           = temp_v0;
    height         = height - list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    temp_a2 = list->itemCount;
    temp_v1 = list->rowHeight;
    if (height >= (temp_a2 * temp_v1)) {
        list->visibleRowCount.unsignedValue = temp_a2;
    } else {
        list->visibleRowCount.unsignedValue = height / temp_v1;
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

void Ui_ComputeVisibleRows(UiList* list, UiPanel* panel)
{
    RECT sp;
    s32  height;

    sp.x    = panel->contentOriginX.unsignedValue + panel->contentLeft.unsignedValue;
    sp.y    = panel->contentOriginY.unsignedValue + panel->contentTop.unsignedValue;
    sp.w    = panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue;
    sp.h    = panel->contentBottom.unsignedValue - panel->contentTop.unsignedValue;
    height  = sp.h;
    height -= list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    if (height >= list->itemCount * list->rowHeight) {
        list->visibleRowCount.unsignedValue = list->itemCount;
    } else {
        list->visibleRowCount.unsignedValue = height / list->rowHeight;
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

void Ui_UpdateListNoAnim(void* arg0, void* arg1)
{
    Ui_UpdateListRows(arg0, arg1, 0);
}

static void Ui_ComputeVisibleRowsEx(UiList* list, UiPanel* panel, s32 arg2)
{
    RECT sp;
    s16  temp_v0;
    u8   temp_a2;
    s8   temp_v1;
    s32  height;

    list->topInset = arg2;
    sp.x           = panel->contentOriginX.unsignedValue + panel->contentLeft.unsignedValue;
    sp.y           = panel->contentOriginY.unsignedValue + panel->contentTop.unsignedValue;
    sp.w           = panel->contentRight.unsignedValue - panel->contentLeft.unsignedValue;
    temp_v0        = panel->contentBottom.unsignedValue - panel->contentTop.unsignedValue;
    height         = temp_v0;
    sp.h           = temp_v0;
    height         = height - list->topInset;
    if (list->rowHeight == 0) {
        list->rowHeight = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT;
    }
    temp_a2 = list->itemCount;
    temp_v1 = list->rowHeight;
    if (height >= (temp_a2 * temp_v1)) {
        list->visibleRowCount.unsignedValue = temp_a2;
    } else {
        list->visibleRowCount.unsignedValue = height / temp_v1;
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

void Ui_SmoothCursor(UiPanel* panel, s32 arg1, s32 arg2)
{
    s32 i;
    s32 targetX;
    s32 targetY;
    s16 baseX;
    s16 baseY;
    u8  count;

    i         = 0;
    baseX     = panel->contentOriginX.signedValue;
    baseY     = panel->contentOriginY.signedValue;
    targetX   = arg1 + baseX;
    targetY   = arg2 + baseY;
    targetX <<= 8;
    targetY <<= 8;
    count     = gDisplayState.frameTicks;
    if (count != 0) {
        do {
            i          += 1;
            D_80067648 += (targetX - D_80067648) >> 2;
            D_8006764C += (targetY - D_8006764C) >> 2;
        } while (i < count);
    }
    targetX = D_80067648 >> 8;
    targetY = D_8006764C >> 8;
    Ui_DrawCursor(panel, targetX - panel->contentOriginX.signedValue, targetY - panel->contentOriginY.signedValue);
}

s32 Ui_LookupTable(void* unused1, s32 arg1)
{
    return D_8006763C[arg1];
}

s32 Ui_Scale15(s32 arg0)
{
    return (arg0 << 4) - arg0;
}

void Ui_DrawTitle(UiPanel* panel, char* arg1)
{
    RECT  sp18;
    RECT* r;
    s32   var_a2;
    s32   color;
    s32   x;
    s32   y;

    color = 0x707060;
    r     = &sp18;
    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 0);
            break;
        case USER_INTERFACE_PANEL_OPEN:
            goto block_default;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = panel->bounds.rect.x;
            r->y = panel->bounds.rect.y;
            r->w = panel->bounds.rect.w;
            r->h = panel->bounds.rect.h;
            break;
    }
    x                             = sp18.x;
    y                             = sp18.y;
    x                             = x + 1;
    y                             = y + 1;
    panel->otIndex.unsignedValue -= 1;
    Ui_DrawTextUnderline(panel, x - panel->contentOriginX.signedValue, y - panel->contentOriginY.signedValue, arg1, color);
    panel->otIndex.unsignedValue += 1;
}

static void Ui_DrawTextAtLayout(UiPanel* panel, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq request;
    s32         temp;

    if (panel->state == USER_INTERFACE_PANEL_OPEN) {
        panel->otIndex.unsignedValue -= 1;
        request.x                     = panel->contentOriginX.unsignedValue + arg1;
        request.y                     = panel->contentOriginY.unsignedValue + arg2;
        temp                          = panel->otIndex.signedValue;
        request.colorRgb              = arg4;
        request.glyphTable            = TEXT_GLYPH_TABLE_MEDIUM;
        request.alignment             = (s8)arg6;
        request.otIndex               = temp + 1;
        request.drawMode              = (s8)arg5;
        Text_DrawString(&request, arg3);
        panel->otIndex.unsignedValue += 1;
    }
}

void Ui_ClampDialogRect(UiPanel* arg0, UiList* list, UiPanel* arg2)
{
    s32 temp;
    s32 limit;
    s16 new_var;

    limit               = 0x96;
    arg0->bounds.rect.x = (list->rowTextX.unsignedValue + arg2->contentOriginX.unsignedValue) + 8;
    arg0->bounds.rect.y = (list->rowTextY.unsignedValue + arg2->contentOriginY.unsignedValue) - 2;
    new_var             = arg0->bounds.rect.x;
    temp                = limit - (new_var + arg0->bounds.rect.w);
    if (temp < 0) {
        arg0->bounds.rect.x = ((u16)new_var) + temp;
    }
    temp = 0x5A - (arg0->bounds.rect.y + arg0->bounds.rect.h);
    if (temp < 0) {
        arg0->bounds.rect.y = ((u16)arg0->bounds.rect.y) + temp;
    }
}

void Ui_SizeFromTextPlain(UiPanel* panel, u8* arg1)
{
    Ui_SizeFromText(panel, arg1, 0, 0);
}

void Ui_SizeFromTextWide(UiPanel* panel, u8* arg1)
{
    Ui_SizeFromText(panel, arg1, 0x20, 0);
}

s32 Ui_IsStateDone(UiObject* object)
{
    return object->panel.state >= USER_INTERFACE_PANEL_HIDING;
}

void Ui_InsertDrawTPage(s32 arg0, s32 arg1)
{
    DR_TPAGE* p;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 0, 1, 0x1E | ((arg1 & 3) << 5));
    addPrim(gGpuCurrentOt + arg0, p);
}

void Ui_SetListScrollFlag(UiList* list, s32 arg1)
{
    if (arg1 == 0) {
        list->flags &= (u8)~USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND;
        return;
    }
    list->flags |= USER_INTERFACE_LIST_SYSTEM_CURSOR_SOUND;
}

void Ui_AllocTile(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    TILE* p;
    s32   y;
    u32   color;

    color = arg5;

    if ((color != 0) && (arg3 >= 2)) {
        p                              = gGpuPrimCursor;
        gGpuPrimCursor                 = p + 1;
        p->x0                          = panel->contentOriginX.unsignedValue + arg1 + 1;
        y                              = panel->contentOriginY.unsignedValue;
        p->w                           = arg3 - 1;
        p->h                           = arg4 - 1;
        GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
        setlen(p, 3);
        p->y0 = y + arg2 + 1;
        setcode(p, 0x60);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, p);
    }
}

void Ui_LayoutWithMode0(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    Ui_DrawBeveledRect(arg0, arg1, arg2, arg3, arg4, arg5, 0);
}

void Ui_LayoutWithMode1(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    Ui_DrawBeveledRect(arg0, arg1, arg2, arg3, arg4, arg5, 1);
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

void Ui_InsetLayout(UiPanel* panel, RECT* arg1, RECT* arg2, s32 unused4)
{
    RECT sp10;

    // Center content within the full panel and retain its screen translation.
    _uiComputePanelInnerRect(panel, &panel->bounds.rect, &sp10);
    if ((panel->style & USER_INTERFACE_PANEL_STYLE_MASK) == USER_INTERFACE_PANEL_TITLE_STYLE) {
        sp10.y += 9;
        sp10.h -= 0xB;
        sp10.x += 2;
        sp10.w -= 4;
    } else {
        sp10.y += 2;
        sp10.h -= 4;
        sp10.x += 2;
        sp10.w -= 4;
    }
    panel->contentLeft.unsignedValue    = -(sp10.w >> 1);
    panel->contentRight.unsignedValue   = panel->contentLeft.unsignedValue + sp10.w;
    panel->contentTop.unsignedValue     = -(sp10.h >> 1);
    panel->contentBottom.unsignedValue  = panel->contentTop.unsignedValue + sp10.h;
    panel->contentOriginX.unsignedValue = sp10.x - panel->contentLeft.unsignedValue;
    panel->contentOriginY.unsignedValue = sp10.y - panel->contentTop.unsignedValue;
    if (arg1 != NULL) {
        _uiComputePanelInnerRect(panel, arg1, arg2);
    }
}

static void Ui_ComputeAnimRect(UiPanel* panel, RECT* rect)
{
    s32 var_a2;

    switch (panel->state) {
        case USER_INTERFACE_PANEL_OPENING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, rect, var_a2, 0);
            return;
        case USER_INTERFACE_PANEL_OPEN:
            break;
        case USER_INTERFACE_PANEL_CLOSING:
        case USER_INTERFACE_PANEL_HIDING:
            var_a2 = USER_INTERFACE_PANEL_ANIMATION_TICKS - panel->animationTicks;
            if ((u32)(var_a2 - 1) >= (u32)USER_INTERFACE_PANEL_SCALE_ONE) {
                var_a2 = 1;
            }
            Ui_ScaleRect(panel, rect, var_a2, 1);
            return;
    }
    rect->x = panel->bounds.rect.x;
    rect->y = panel->bounds.rect.y;
    rect->w = panel->bounds.rect.w;
    rect->h = panel->bounds.rect.h;
}

static void Ui_AnimOpenStep(UiPanel* panel, Task* task)
{
    if (panel->animationTicks == 0) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        panel->state         += USER_INTERFACE_PANEL_OPENING - USER_INTERFACE_PANEL_INITIAL;
        Ui_DrawAndCallback(panel, task);
    } else {
        if (panel->animationTicks > 0) {
            panel->animationTicks += USER_INTERFACE_PANEL_ANIMATION_TICKS;
        }
        panel->state = USER_INTERFACE_PANEL_HIDDEN;
        Ui_ClipAndCallback(panel, task);
    }
}

static void Ui_DrawAndCallback(UiPanel* panel, Task* task)
{
    s32 temp_s2;

    // Suspend input during opening, preserving control changes from the callback.
    temp_s2             = panel->control.word;
    panel->control.word = temp_s2 << 0x10;
    Ui_LayoutAndClip(panel);
    panel->contentCallback(task);
    panel->animationTicks -= gDisplayState.frameTicks;
    if (panel->animationTicks <= 0) {
        panel->animationTicks = 0;
        if (panel->state == USER_INTERFACE_PANEL_OPENING) {
            panel->state = USER_INTERFACE_PANEL_OPEN;
        }
    }
    if (panel->control.word == (temp_s2 << 0x10)) {
        panel->control.word = temp_s2;
    }
}

static void Ui_LayoutDrawAndCallback(UiPanel* panel, Task* task)
{
    Ui_LayoutAndDraw(panel);
    panel->contentCallback(task);
}

static void Ui_TickAnimCounter(UiPanel* panel, Task* task)
{
    if (panel->animationTicks >= 0) {
        panel->animationTicks += gDisplayState.frameTicks;
    }
    if ((u16)panel->animationTicks >= (u32)USER_INTERFACE_PANEL_ANIMATION_TICKS) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        Task_CallExit(task);
        return;
    }
    panel->control.word = USER_INTERFACE_PANEL_INACTIVE;
    Ui_LayoutAndDrawAlt(panel);
    panel->contentCallback(task);
}

static void Ui_AnimCloseStep(UiPanel* panel, Task* task)
{
    s32 temp_s1;

    temp_s1 = panel->control.word;
    if (panel->animationTicks >= 0) {
        panel->animationTicks += gDisplayState.frameTicks;
    }
    if ((u16)panel->animationTicks >= (u32)USER_INTERFACE_PANEL_ANIMATION_TICKS) {
        panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_STOPPED;
        panel->state         += 1;
        Ui_ClipAndCallback(panel, task);
        return;
    }
    panel->control.word <<= 0x10;
    Ui_LayoutAndDrawAlt(panel);
    panel->contentCallback(task);
    if (panel->control.word == (temp_s1 << 0x10)) {
        panel->control.word = temp_s1;
    }
}

static void Ui_ClipAndCallback(UiPanel* panel, Task* task)
{
    s16 temp_a0;
    s16 temp_v0;
    s32 temp_s0;
    s32 temp_s2;

    // Hidden content still updates while clipping its drawing and suspending input.
    temp_s2             = panel->control.word;
    temp_s0             = temp_s2 << 0x10;
    panel->control.word = temp_s0;
    Ui_SetupClip(panel);
    panel->contentCallback(task);
    if (panel->control.word == temp_s0) {
        panel->control.word = temp_s2;
    }
    if (panel->animationTicks > 0) {
        temp_v0               = (u16)panel->animationTicks - gDisplayState.frameTicks;
        panel->animationTicks = temp_v0;
        if (temp_v0 < USER_INTERFACE_PANEL_ANIMATION_TICKS) {
            panel->animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
        }
    }
    temp_a0 = panel->animationTicks;
    if (((temp_a0 < 0) && (panel->control.word == USER_INTERFACE_PANEL_ACTIVE)) || (temp_a0 == USER_INTERFACE_PANEL_ANIMATION_TICKS)) {
        uiStartPanelOpening(panel, task);
    }
}

static void Ui_DispatchObjectState(Task* task)
{
    _UiPanelLifecycleFuncTable6 sp;
    UiPanel*                    temp;

    sp   = Ui_ObjectStates;
    temp = task->spawnArg2.pointer;
    sp.funcs[temp->state](temp, task);
}

s32 Ui_GetCursorFixed(void)
{
    union {
        struct {
            s16 unk0;
            s16 unk2;
        } parts;
        s32 word;
    } sp;

    s16* p = &sp.parts.unk0;

    *p            = D_80067648 >> 8;
    sp.parts.unk2 = D_8006764C >> 8;
    return sp.word;
}

void Ui_DrawFlatCaret(UiPanel* panel, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    POLY_F3* p;
    u16      t;
    u_long*  ot;

    p     = gGpuPrimCursor;
    t     = panel->contentOriginX.unsignedValue + arg1;
    p->x2 = t;
    p->x1 = t;
    p->x0 = t;
    // Keep the original gouraud-sized reservation for this flat packet.
    gGpuPrimCursor = (u8*)p + sizeof(POLY_G3);
    t              = panel->contentOriginY.unsignedValue + arg2;
    p->y2          = t;
    p->y1          = t;
    p->y0          = t;
    if (arg4 == 0) {
        p->x1 = p->x1 - 4;
        t     = p->y0 + 5;
        p->x2 = p->x2 + 5;
        p->y2 = t;
        p->y1 = t;
    } else {
        p->x1 = p->x1 - 3;
        t     = p->y0 - 4;
        p->x2 = p->x2 + 4;
        p->y2 = t;
        p->y1 = t;
    }
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg3 * 2;
    setlen(p, 4);
    setcode(p, 0x20);
    ot = gGpuCurrentOt;
    addPrim(&ot[panel->otIndex.signedValue + 1], p);
}

void Ui_WaitCdThenOverlay(Task* task)
{
    UiPanel* temp_s0;

    temp_s0 = task->spawnArg2.pointer;
    if (CdCmd_IsIdle() != 0) {
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
    Text_DrawPrompt(object, list->rowTextX.signedValue, list->rowTextY.signedValue, option->text, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
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
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->flags  = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->state += 1;
    }
    text = request->title;
    if (text != NULL) {
        Ui_DrawText(&(obj)->panel, text);
    }
    Ui_UpdateListRows(menu, &(obj)->panel, 0);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        status = obj->result;
        if ((status == USER_INTERFACE_RESULT_CONFIRM) || (status == USER_INTERFACE_RESULT_CANCEL)) {
            request->result = obj->resultValue;
            parent          = obj->owner;
            child           = parent->firstChild;
            if (child != NULL) {
                do {
                    Ui_TeardownTree((UiObject*)child->spawnArg2.pointer, child);
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
