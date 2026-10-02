#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"

/// Per-instance work block of actor_548100, parked in `Task::work` -- that
/// slot is not a `TaskIdMap` here, it is the `memCalloc(0x18, 0)` block
/// `func_actor_548100_80132420` allocates at spawn and stores at
/// `Task::work` (0x1C). Reach it with `(Actor548100Work*)task->work`.
///
/// `step` is the actor's 1-based progress step (0 while unset): the spawner
/// `func_actor_548100_80132550` seeds it from the `field_8` of the first
/// `D_actor_548100_801357E8` record whose `field_B` is set, and
/// `func_actor_548100_80132684` switches on it with nine cases. The game-flag
/// nibble recording whether a step is done is `step + 0xBE`, which is why the
/// same `+ 0xBE` shows up at every `GameFlag_GetNibble` / `GameFlag_SetNibble`
/// site in the overlay. `bit2Slot` is the 2-bit slot this instance occupies in
/// the current `Gp_Bit2Banks` word (`Gp_GetCurBit2Flag` / `Gp_SetCurBit2Flag`,
/// seeded with 5 or 4 by `func_actor_548100_80132684`) and `collectBitId` an id
/// in the `Gp_ClearCollectedBit` space (0x120 for the instance that reaches
/// `Gp_StartCapSlot` with kind 1). `promptKind` is the picked hotspot's prompt
/// display mode, copied from it by `func_actor_548100_80132550` and handed to
/// `func_800D4E78` when `func_actor_548100_80134DBC` re-spawns the prompt.
///
/// 0x8 and up is the ramp `func_actor_548100_80134FEC` drives: `field_8` is the
/// period, `field_A` the elapsed counter it advances by 4 and clamps to
/// `field_8`, `field_C` the value that period ramps to, `field_E` the
/// interpolated result `field_C * field_A / field_8`, and `field_10` / `field_12`
/// a second period/elapsed pair on the same shape. `func_actor_548100_80132808`
/// seeds that ramp from a route record (`Actor548100Route`): `field_8` becomes
/// the farther of the record's first two legs' distances and `field_C` the
/// nearer one, so the pair is also what names 0x14-0x17 -- `farFrom` / `farTo`
/// the node ids of the leg `field_8` measures, `nearFrom` / `nearTo` those of
/// `field_C`. The block is 0x18 bytes in full.
typedef struct Actor548100Work {
    /* 0x00 */ byte pad_0[0x2];
    /* 0x02 */ s16  step;
    /* 0x04 */ s16  collectBitId;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ s8   bit2Slot;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s16  field_E;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ u8   farFrom;
    /* 0x15 */ u8   farTo;
    /* 0x16 */ u8   nearFrom;
    /* 0x17 */ u8   nearTo;
} Actor548100Work;
STATIC_ASSERT_SIZEOF(Actor548100Work, 0x18);

/// One leg of an `Actor548100Route`: the two node ids `func_actor_548100_80134CB8`
/// measures a route distance between, in the same node space as
/// `Actor548100Edge`'s `nodeA` / `nodeB`.
typedef struct Actor548100Leg {
    /* 0x0 */ u8 nodeA;
    /* 0x1 */ u8 nodeB;
} Actor548100Leg;
STATIC_ASSERT_SIZEOF(Actor548100Leg, 0x2);

/// One record of the route-progress tables `D_actor_548100_801356D8` and
/// `D_actor_548100_80135750` -- 10-byte records. The first table has a 0xFF
/// sentinel; the second contains the eleven placements of up to two batteries
/// in four sockets. `func_actor_548100_801330EC` selects the table by
/// `GameFlag_GetNibble(0xBE)` and whose chosen record it parks in
/// `D_actor_548100_80135B4C`. A leg's node ids are read as `u8` -- the `lbu` is
/// what the target shows, `Actor548100Edge`'s record is keyed the same way --
/// and a `nodeA` of 0 means the leg is unused.
///
/// `bitA` / `bitB` are 1-based ids, or 0: `func_actor_548100_801330EC` turns
/// each into `1 << (id - 1)` and compares the pair against the four nibbles
/// 0xBF-0xC2 -- the per-step flags this actor sets -- stepping a record at a
/// time until they agree, so a record describes one player state (the two
/// tables are that state's two routes).
///
/// `func_actor_548100_80132808` measures `leg[0]` and `leg[1]` with
/// `func_actor_548100_80134CB8` and keeps the farther and the nearer of the two
/// in the work ramp, and `leg[2]`'s distance as the second period; the record's
/// `nodeA`s must be set for `func_actor_548100_80132A14` to walk each leg with
/// `func_actor_548100_80134AE0`. Its `flag_8` / `flag_9` gate that same body's
/// two `func_actor_548100_80133BBC` calls.
typedef struct Actor548100Route {
    /* 0x0 */ s8             bitA;
    /* 0x1 */ s8             bitB;
    /* 0x2 */ Actor548100Leg leg[3];
    /* 0x8 */ u8             flag_8;
    /* 0x9 */ u8             flag_9;
} Actor548100Route;
STATIC_ASSERT_SIZEOF(Actor548100Route, 0xA);

/// The route record `func_actor_548100_801330EC` last resolved: where the
/// player is along the route the stage is on. Zero until the first
/// `func_actor_548100_801330EC` call.
extern Actor548100Route* D_actor_548100_80135B4C;

/// One record of the edge table `D_actor_548100_801351D0`: a directed link of
/// the stage graph this actor patrols and draws. `nodeA` / `nodeB` are node ids
/// (0-99) indexing the 4-byte point table `D_actor_548100_801358E4`
/// (`s16 x, y`); `func_actor_548100_80133684` reads both endpoints' points and
/// draws the segment between them. A node pair also keys the edge-id matrix
/// `D_actor_548100_80135B5C` as `prev * 100 + cur`, which is how the route walk
/// in `func_actor_548100_80134AE0` and `func_actor_548100_80134CB8` gets from a
/// step of the route string back to a record here: the bytes of
/// `D_actor_548100_80135B24[id]` are successive node ids, 0xFF-terminated.
///
/// The record is 14 bytes -- the stride `func_actor_548100_80134AE0` computes
/// as `id * 7 * 2` -- and only `nodeA`, `nodeB` and `field_2` are seeded in the
/// ROM; the rest is runtime state. `state` is the 1-based progress step the
/// drawing switch in `func_actor_548100_80133684` dispatches on (it subtracts 1
/// and accepts 0-4 as a case index): `func_actor_548100_80134AE0` writes 2 or 3
/// into it and `func_actor_548100_80134BF0` 0 or 1, the latter choosing between
/// them by comparing `field_2` with 2 (records 0-3 carry 2, records 73-78
/// carry 1). `flag_3` gates the direction branch of the drawing code, `dist` is
/// a per-segment value summed along a route by `func_actor_548100_80134CB8`,
/// and `field_C` a signed value that code scales by the segment's horizontal
/// direction.
///
/// `func_actor_548100_80134BA8` walks the table from its head and stops at the
/// first record whose `nodeA` is 0: an all-zero sentinel record, the 92nd, so 91
/// real records. The table's extent is 0x508 bytes, ending exactly where
/// `D_actor_548100_801356D8` begins -- only its leading 0x200 bytes are covered
/// by this symbol, the splitter having put the stray `D_actor_548100_801353D0`
/// label inside the array, mid-record.
typedef struct Actor548100Edge {
    /* 0x00 */ u8   nodeA;
    /* 0x01 */ u8   nodeB;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   flag_3;
    /* 0x04 */ s16  field_4;
    /* 0x06 */ s16  field_6;
    /* 0x08 */ u8   state;
    /* 0x09 */ byte pad_9[0x1];
    /* 0x0A */ s16  dist;
    /* 0x0C */ s16  field_C;
} Actor548100Edge;
STATIC_ASSERT_SIZEOF(Actor548100Edge, 0xE);

extern Actor548100Edge D_actor_548100_801351D0[];
/// Node points `(x, y)`, indexed by node id.
extern DVECTOR D_actor_548100_801358E4[];
/// Route strings, indexed by route id: node ids, 0xFF-escaped, 0-terminated.
extern u8* D_actor_548100_80135B24[];
/// Edge-id matrix keyed `prev * 100 + cur`.
extern u8 D_actor_548100_80135B5C[10000];

/// One cell of the sprite table `D_actor_548100_801357C0` (four records, the
/// actor's four frames, each drawn only while its `GameFlag_GetNibble(i + 0xBF)`
/// is set) plus the fifth record `D_actor_548100_801357C0[4]` -- `{0, 0, 75, 239}`,
/// the 76-column full-height panel `func_actor_548100_8013461C` links when flag
/// 0xC3 is set. A record is an 8-byte `s16` quadruple, the stride the loop in
/// `func_actor_548100_80132A14` walks as `s1 += 8` and the one that makes
/// `D_actor_548100_801357C0[4]` element 4 of the same table.
///
/// The same four numbers are both the quad's texture window and its screen
/// rectangle: `func_actor_548100_8013461C` writes them straight into `u`/`v`
/// and writes `u - 160` / `v - 120` into `x`/`y`. That difference is the screen
/// centre, so the table is authored in 320x240 screen space with the origin at
/// the middle, and the texture page is laid over the screen 1:1 -- the four
/// frames tile the strip at x 76-103, y 33-83 and the fifth covers everything
/// to its left.
typedef struct Actor548100TexRect {
    /* 0x0 */ s16 u0;
    /* 0x2 */ s16 v0;
    /* 0x4 */ s16 u1;
    /* 0x6 */ s16 v1;
} Actor548100TexRect;
STATIC_ASSERT_SIZEOF(Actor548100TexRect, 0x8);

// Eleven socket masks: empty, four singles, and six pairs. Items 0x120/0x12C
// move between inventory and sockets; pickup slot 6 supplies the second battery
// once, while slots 4/5 return installed batteries. Thus normal puzzle actions
// match a record before the end of this table, which has no sentinel.
extern Actor548100Route   D_actor_548100_80135750[11];
extern Actor548100TexRect D_actor_548100_801357C0[5];

// Retained coefficient editor rows identify the three values and caption.
typedef struct {
    u8*         values[3];
    const char* caption;
} Actor548100CoefficientRow;
STATIC_ASSERT_SIZEOF(Actor548100CoefficientRow, 16);
extern Actor548100CoefficientRow D_actor_548100_801358A8[3];

static void func_actor_548100_80132420(Task* task);
static void func_actor_548100_80132550(Task* task);
static void func_actor_548100_80132684(Task* task);
static void func_actor_548100_80132808(Task* arg0);
static void func_actor_548100_801330EC(void);
static void func_actor_548100_80134400(OverlayHotspot* unused);
static s32  func_actor_548100_801348A4(OverlayHotspot* table, s16 x, s16 y);
static s32  func_actor_548100_80134CB8(s32 nodeA, u8 nodeB);
static void func_actor_548100_80134D88(Task* task);
static void func_actor_548100_80134DBC(Task* task);
static void func_actor_548100_80134E0C(Task* arg0);
static void func_actor_548100_80134E94(Task* arg0);
static void func_actor_548100_80134F64(Task* arg0);
static void func_actor_548100_80134FEC(Task* arg0);
static void func_actor_548100_80135124(Task* arg0);
static void func_actor_548100_8013461C(Actor548100TexRect* rect);
static void func_actor_548100_80133BBC(s32 arg0);
static void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b);
static void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos);
static void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_80134AE0(s32 id, u8 stop);
static void func_actor_548100_80134BA8(void);
static void func_actor_548100_80134BF0(void);

extern TaskDesc D_actor_548100_801351B4;
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s16, s32);
    } handler;
} Actor548100MsgEntry;
STATIC_ASSERT_SIZEOF(Actor548100MsgEntry, 8);

extern Actor548100MsgEntry D_actor_548100_801351C0[];
extern OverlayHotspot      D_actor_548100_801357E8[];
extern Actor548100Route    D_actor_548100_801356D8[12];
extern s16                 D_actor_548100_80135B50;
extern u8                  D_actor_548100_80135B52;
extern s8                  D_actor_548100_80135B53;
extern s8                  D_actor_548100_80135B54;
extern s8                  D_actor_548100_80135B55;
extern s8                  D_actor_548100_80135B56;
extern s8                  D_actor_548100_80135B57;
extern s8                  D_actor_548100_80135B58;
extern s8                  D_actor_548100_80135B59;
extern s8                  D_actor_548100_80135B5A;
extern s8                  D_actor_548100_80135B5B;
extern u8                  D_actor_548100_80135884;
extern u8                  D_actor_548100_80135885;
extern u8                  D_actor_548100_80135886;
extern u8                  D_actor_548100_80135887;
extern u8                  D_actor_548100_80135888;
extern u8                  D_actor_548100_80135889;
extern u8                  D_actor_548100_8013588A;
extern u8                  D_actor_548100_8013588B;
extern u8                  D_actor_548100_8013588C[28];

s32  func_actor_548100_80134778(Task*, s16, s32);
void func_actor_548100_80134728(Task*);

static const char D_actor_548100_80131E54[6];
static const char D_actor_548100_80131E5C[5];
static const char D_actor_548100_80131E64[5];
static void       func_actor_548100_801347F8(Task*);

TaskDesc D_actor_548100_801351B4 = { { { TASK_BODY_NONE, 192 } }, func_actor_548100_80134728, { .value = 0 } };

Actor548100MsgEntry D_actor_548100_801351C0[2] = {
    { 5105, { .call0 = func_actor_548100_80134778 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

Actor548100Edge D_actor_548100_801351D0[92] = {
    { 68, 1, 2, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 1, 2, 2, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 2, 73, 2, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 73, 74, 2, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 69, 75, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 3, 4, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 4, 5, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 5, 6, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 6, 7, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 7, 26, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 26, 30, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 30, 34, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 34, 54, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 54, 53, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 53, 52, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 52, 51, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 51, 57, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 57, 58, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 70, 76, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 76, 77, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 8, 9, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 9, 10, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 10, 11, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 11, 12, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 12, 13, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 13, 14, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 14, 25, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 25, 29, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 29, 33, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 33, 47, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 47, 46, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 46, 45, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 45, 44, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 44, 43, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 43, 50, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 50, 56, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 56, 59, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 59, 60, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 60, 61, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 61, 62, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 71, 78, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 15, 16, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 16, 17, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 17, 18, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 18, 19, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 19, 20, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 20, 28, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 28, 32, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 32, 42, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 42, 41, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 41, 40, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 40, 39, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 39, 38, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 38, 49, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 49, 55, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 55, 63, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 63, 64, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 64, 66, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 66, 67, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 72, 79, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 79, 80, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 21, 22, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 22, 23, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 23, 24, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 24, 27, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 27, 31, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 31, 37, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 37, 36, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 36, 35, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 35, 48, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 48, 65, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 65, 66, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 4, 9, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 5, 10, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 6, 13, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 25, 26, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 29, 30, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 34, 33, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 44, 53, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 50, 51, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 11, 17, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 12, 19, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 41, 46, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 40, 45, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 55, 56, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 16, 22, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 18, 23, 1, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 27, 28, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 32, 31, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 36, 39, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 48, 49, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, { 0 }, 0, 0 },
};

Actor548100Route D_actor_548100_801356D8[12] = {
    { 0, 0, { { 0, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 1, 0, { { 1, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 2, 0, { { 2, 0 }, { 0, 0 }, { 9, 0 } }, 1, 1 },
    { 3, 0, { { 3, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 4, 0, { { 4, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 1, 2, { { 1, 9 }, { 2, 9 }, { 9, 0 } }, 0, 1 },
    { 1, 3, { { 1, 11 }, { 3, 11 }, { 9, 0 } }, 0, 1 },
    { 1, 4, { { 1, 36 }, { 4, 36 }, { 9, 0 } }, 0, 1 },
    { 2, 3, { { 2, 0 }, { 3, 0 }, { 9, 0 } }, 1, 1 },
    { 2, 4, { { 2, 0 }, { 4, 0 }, { 9, 0 } }, 1, 1 },
    { 3, 4, { { 3, 27 }, { 4, 27 }, { 9, 0 } }, 0, 1 },
    { -1, -1, { { 0, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
};

Actor548100Route D_actor_548100_80135750[11] = {
    { 0, 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 1, 0, { { 5, 0 }, { 0, 0 }, { 0, 0 } }, 1, 0 },
    { 2, 0, { { 6, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 3, 0, { { 7, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 4, 0, { { 8, 0 }, { 0, 0 }, { 0, 0 } }, 0, 1 },
    { 1, 2, { { 5, 9 }, { 6, 9 }, { 0, 0 } }, 0, 0 },
    { 1, 3, { { 5, 13 }, { 7, 13 }, { 0, 0 } }, 0, 0 },
    { 1, 4, { { 5, 0 }, { 8, 0 }, { 0, 0 } }, 1, 1 },
    { 2, 3, { { 6, 18 }, { 7, 18 }, { 0, 0 } }, 0, 0 },
    { 2, 4, { { 6, 11 }, { 8, 11 }, { 0, 0 } }, 0, 0 },
    { 3, 4, { { 7, 22 }, { 8, 22 }, { 0, 0 } }, 0, 0 },
};

Actor548100TexRect D_actor_548100_801357C0[5] = {
    { 76, 33, 89, 47 },
    { 89, 45, 103, 59 },
    { 76, 57, 89, 71 },
    { 89, 69, 103, 83 },
    { 0, 0, 75, 239 },
};

OverlayHotspot D_actor_548100_801357E8[13] = {
    { -84, -87, 14, 14, 1, 0, 0 },
    { -71, -75, 14, 14, 2, 0, 0 },
    { -84, -63, 14, 14, 3, 0, 0 },
    { -71, -51, 14, 14, 4, 0, 0 },
    { -134, 0, 43, 88, 5, 0, 0 },
    { -134, -98, 44, 200, 6, 0, 0 },
    { 63, 42, 44, 38, 9, 0, 0 },
    { -58, -96, 189, 55, 7, 0, 0 },
    { 75, -46, 56, 117, 7, 0, 0 },
    { 75, -46, 56, 117, 7, 0, 0 },
    { -79, -1, 152, 93, 7, 0, 0 },
    { -37, 92, 96, 20, 8, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

u8 D_actor_548100_80135884 = 16;

u8 D_actor_548100_80135885 = 165;

u8 D_actor_548100_80135886 = 98;

u8 D_actor_548100_80135887 = 60;

u8 D_actor_548100_80135888 = 60;

u8 D_actor_548100_80135889 = 16;

u8 D_actor_548100_8013588A = 160;

u8 D_actor_548100_8013588B = 16;

u8 D_actor_548100_8013588C[28] = {
    32,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

Actor548100CoefficientRow D_actor_548100_801358A8[3] = {
    { { &D_actor_548100_80135884, &D_actor_548100_80135885, &D_actor_548100_80135886 }, D_actor_548100_80131E64 },
    { { &D_actor_548100_80135887, &D_actor_548100_80135888, &D_actor_548100_80135889 }, D_actor_548100_80131E5C },
    { { &D_actor_548100_8013588A, &D_actor_548100_8013588B, D_actor_548100_8013588C }, D_actor_548100_80131E54 },
};

TaskDesc D_actor_548100_801358D8 = { { { TASK_BODY_NONE, 192 } }, func_actor_548100_801347F8, { .value = 0 } };

DVECTOR D_actor_548100_801358E4[82] = {
    { 0, 0 },
    { 79, 26 },
    { 286, 26 },
    { 89, 38 },
    { 125, 38 },
    { 150, 38 },
    { 232, 38 },
    { 271, 38 },
    { 102, 51 },
    { 125, 51 },
    { 150, 51 },
    { 169, 51 },
    { 208, 51 },
    { 232, 51 },
    { 260, 51 },
    { 89, 62 },
    { 136, 62 },
    { 169, 62 },
    { 186, 62 },
    { 208, 62 },
    { 247, 62 },
    { 102, 74 },
    { 136, 74 },
    { 186, 74 },
    { 235, 74 },
    { 260, 70 },
    { 271, 70 },
    { 235, 86 },
    { 247, 86 },
    { 260, 91 },
    { 271, 91 },
    { 235, 104 },
    { 247, 104 },
    { 260, 109 },
    { 271, 109 },
    { 81, 120 },
    { 128, 120 },
    { 235, 120 },
    { 93, 131 },
    { 128, 131 },
    { 175, 131 },
    { 217, 131 },
    { 247, 131 },
    { 104, 144 },
    { 146, 144 },
    { 175, 144 },
    { 217, 144 },
    { 260, 144 },
    { 81, 163 },
    { 93, 163 },
    { 104, 160 },
    { 116, 160 },
    { 116, 155 },
    { 146, 155 },
    { 271, 155 },
    { 93, 176 },
    { 104, 176 },
    { 116, 170 },
    { 223, 170 },
    { 104, 182 },
    { 179, 182 },
    { 198, 189 },
    { 223, 189 },
    { 93, 192 },
    { 143, 192 },
    { 81, 201 },
    { 151, 201 },
    { 163, 213 },
    { 70, 27 },
    { 70, 39 },
    { 70, 55 },
    { 70, 63 },
    { 70, 79 },
    { 286, 189 },
    { 264, 189 },
    { 76, 38 },
    { 82, 51 },
    { 89, 51 },
    { 76, 62 },
    { 82, 74 },
    { 89, 74 },
    { -1, -1 },
};

u8 D_actor_548100_80135A2C[36] = {
    69,
    75,
    255,
    3,
    4,
    9,
    10,
    11,
    17,
    18,
    19,
    12,
    13,
    14,
    25,
    29,
    30,
    34,
    33,
    47,
    46,
    45,
    40,
    39,
    36,
    35,
    48,
    49,
    55,
    63,
    64,
    66,
    67,
    0,
    0,
    0,
};

u8 D_actor_548100_80135A50[24] = {
    70,
    76,
    77,
    255,
    8,
    9,
    4,
    5,
    6,
    7,
    26,
    30,
    29,
    33,
    34,
    54,
    53,
    52,
    51,
    57,
    58,
    0,
    0,
    0,
};

u8 D_actor_548100_80135A68[28] = {
    71,
    78,
    255,
    15,
    16,
    17,
    11,
    12,
    19,
    20,
    28,
    27,
    31,
    32,
    42,
    41,
    40,
    45,
    44,
    43,
    50,
    56,
    59,
    60,
    61,
    62,
    0,
    0,
};

u8 D_actor_548100_80135A84[24] = {
    72,
    79,
    80,
    255,
    21,
    22,
    23,
    24,
    27,
    28,
    32,
    31,
    37,
    36,
    39,
    38,
    49,
    48,
    65,
    66,
    67,
    0,
    0,
    0,
};

u8 D_actor_548100_80135A9C[28] = {
    69,
    75,
    255,
    3,
    4,
    9,
    10,
    5,
    6,
    13,
    14,
    25,
    26,
    30,
    29,
    33,
    34,
    54,
    53,
    44,
    43,
    50,
    51,
    57,
    58,
    0,
    0,
    0,
};

u8 D_actor_548100_80135AB8[28] = {
    70,
    76,
    77,
    255,
    8,
    9,
    4,
    5,
    10,
    11,
    17,
    18,
    23,
    24,
    27,
    28,
    32,
    31,
    37,
    36,
    39,
    38,
    49,
    48,
    65,
    66,
    67,
    0,
};

u8 D_actor_548100_80135AD4[36] = {
    71,
    78,
    255,
    15,
    16,
    22,
    23,
    18,
    19,
    12,
    13,
    6,
    7,
    26,
    25,
    29,
    30,
    34,
    33,
    47,
    46,
    41,
    40,
    45,
    44,
    53,
    52,
    51,
    50,
    56,
    55,
    63,
    64,
    66,
    67,
    0,
};

u8 D_actor_548100_80135AF8[36] = {
    72,
    79,
    80,
    255,
    21,
    22,
    16,
    17,
    11,
    12,
    19,
    20,
    28,
    27,
    31,
    32,
    42,
    41,
    46,
    45,
    40,
    39,
    36,
    35,
    48,
    49,
    55,
    56,
    59,
    60,
    61,
    62,
    0,
    0,
    0,
    0,
};

u8 D_actor_548100_80135B1C[8] = {
    68,
    1,
    2,
    73,
    74,
    0,
    0,
    0,
};

u8* D_actor_548100_80135B24[10] = {
    NULL,
    D_actor_548100_80135A2C,
    D_actor_548100_80135A50,
    D_actor_548100_80135A68,
    D_actor_548100_80135A84,
    D_actor_548100_80135A9C,
    D_actor_548100_80135AB8,
    D_actor_548100_80135AD4,
    D_actor_548100_80135AF8,
    D_actor_548100_80135B1C,
};

Actor548100Route* D_actor_548100_80135B4C = NULL;

s16 D_actor_548100_80135B50 = 0;

u8 D_actor_548100_80135B52 = 0;

s8 D_actor_548100_80135B53 = 0;

s8 D_actor_548100_80135B54 = 0;

s8 D_actor_548100_80135B55 = 0;

s8 D_actor_548100_80135B56 = 0;

s8 D_actor_548100_80135B57 = 0;

s8 D_actor_548100_80135B58 = 0;

u8 D_actor_548100_80135B5C[10000] = { 0 };

static void func_actor_548100_80132A14(Task* task);
static void func_actor_548100_80132EA0(Task* task);
static void func_actor_548100_80133684(Actor548100Edge* edge);
static void func_actor_548100_80133F88(void);

#include "../../shared/action_prompt_move_cursors.inc.c"

/// Three prompt-mode labels nothing in the actor reads.
static const char D_actor_548100_80131E54[] = "Short";
static const char D_actor_548100_80131E5C[] = "Stop";
static const char D_actor_548100_80131E64[] = "Flow";

/// State table of the actor's `Task::callback`, `func_actor_548100_801347F8`,
/// one handler per `Task::state`, which that body copies onto its stack before
/// indexing. States 0 and 2 are the spawners, 1 and 3 arm and re-spawn the
/// action prompt, 4 is the `step` switch and 9 the ramp driver.
static const TaskFuncTable11 D_actor_548100_80131E6C = { {
    func_actor_548100_80132420,
    func_actor_548100_80134D88,
    func_actor_548100_80132550,
    func_actor_548100_80134DBC,
    func_actor_548100_80132684,
    func_actor_548100_80134E0C,
    func_actor_548100_80134E94,
    func_actor_548100_80134F64,
    func_actor_548100_80132808,
    func_actor_548100_80134FEC,
    func_actor_548100_80135124,
} };

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// State 0 of the actor's callback: allocates the work block, spawns the
/// action-prompt task and initializes the map UI.
static void func_actor_548100_80132420(Task* task)
{
    Actor548100Work* work;
    OverlayHotspot*  rec;
    OverlayHotspot*  start;

    work = memCalloc(0x18, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = Task_SpawnFromTable(&D_actor_548100_801351B4, 0, 1, 0);
    task->msgTable                                             = D_actor_548100_801351C0;
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    task->state                                               += 1;
    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 0) {
        GameFlag_SetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE, 1);
        GameFlag_SetNibble(GAME_FLAG_MINE_POWER_PANEL_SOCKET_4, 1);
    }
    work->collectBitId = 0;
    Display_AcquireRef();
    start = D_actor_548100_801357E8;
    for (rec = start; rec->id != -1; rec++) {
        rec->hit = 0;
    }
    D_actor_548100_80135B50 = 0x10;
    D_actor_548100_80135B52 = 0;
    func_actor_548100_80134400(start);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    Gp_MsgPlayerWeapon(0);
    Gp_MsgPlayer3F3(0);
}

static void func_actor_548100_80132550(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    OverlayHotspot*   hs     = D_actor_548100_801357E8;
    Actor548100Work*  work   = (Actor548100Work*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    work->step       = 0;
    if (func_actor_548100_801348A4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->step       = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

static void func_actor_548100_80132684(Task* task)
{
    Actor548100Work* work = (Actor548100Work*)task->work;
    s32              kind;
    s32              state;
    s32              cmd;

    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        switch (work->step) {
            case 1:
            case 2:
            case 3:
            case 4:
                if (GameFlag_GetNibble(work->step + 0xBE) == 0) {
                    Gp_StartCapSlot(6, 0, 0);
                    state = 2;
                } else if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
                    Gp_StartCapSlot(6, 1, 3);
                    state = 2;
                } else {
                    if (GameFlag_GetNibble(work->step + 0xBE) == 1) {
                        work->bit2Slot = 4;
                        kind           = 2;
                    } else {
                        work->bit2Slot = 5;
                        kind           = 5;
                    }
                    Gp_SetCurBit2Flag(work->bit2Slot, 1);
                    Gp_StartCapSlot(6, 0, kind);
                    state = 7;
                }
                break;
            case 5:
                Gp_RunCapCmd(5, 0);
                state = 8;
                break;
            case 6:
                cmd = 4;
                goto run;
            case 7:
                cmd = 7;
                goto run;
            case 8:
                cmd = 9;
                goto run;
            case 9:
                cmd = 8;
            run:
                Gp_RunCapCmd(cmd, 0);
                state = 2;
                break;
            default:
                goto def;
        }
    } else {
        state = work->collectBitId;
        if (state != 0) {
            state = 6;
        } else {
        def:
            state = 2;
        }
    }
    task->state = state;
}

/// Player pressed the action button on this actor's map marker with the marker
/// route done: record the route leg the ramp runs along and hand the actor on to
/// state 9.
static void func_actor_548100_80132808(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;
    s32              distA;
    s32              distB;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            if (GameFlag_GetNibble(GAME_FLAG_110) != 0) {
                Gp_SetItemSeenBit(0x120, 1);
                Gp_SetItemSeenBit(0x12C, 1);
            }
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, 0, 0);
            GameFlag_SetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 1);
            arg0->state = 9;
            func_actor_548100_801330EC();
            work->field_10 = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[2].nodeA, D_actor_548100_80135B4C->leg[2].nodeB);
            distA          = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[0].nodeA, D_actor_548100_80135B4C->leg[0].nodeB);
            distB          = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[1].nodeA, D_actor_548100_80135B4C->leg[1].nodeB);
            if (distB < distA) {
                work->field_8  = distA;
                work->field_C  = distB;
                work->farFrom  = D_actor_548100_80135B4C->leg[0].nodeA;
                work->nearFrom = D_actor_548100_80135B4C->leg[1].nodeA;
                work->farTo    = D_actor_548100_80135B4C->leg[0].nodeB;
                work->nearTo   = D_actor_548100_80135B4C->leg[1].nodeB;
            } else {
                work->field_8  = distB;
                work->field_C  = distA;
                work->farFrom  = D_actor_548100_80135B4C->leg[1].nodeA;
                work->nearFrom = D_actor_548100_80135B4C->leg[0].nodeA;
                work->farTo    = D_actor_548100_80135B4C->leg[1].nodeB;
                work->nearTo   = D_actor_548100_80135B4C->leg[0].nodeB;
            }
            work->field_A  = 0;
            work->field_E  = 0;
            work->field_12 = 0;
            return;
        }
        if (Gp_GetCapEventKey() == 0x15) {
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            GameFlag_SetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 0);
        }
        arg0->state = 2;
    }
}

static void func_actor_548100_80132A14(Task* task)
{
    Actor548100Work*    work;
    Actor548100TexRect* rect;
    DR_MODE*            prim;
    Actor548100Route*   route;
    s32                 i;
    s32                 flagA;
    s32                 flagB;

    i    = 0;
    rect = D_actor_548100_801357C0;
    work = (Actor548100Work*)task->work;
    for (; i < 4; i++, rect++) {
        if (GameFlag_GetNibble(i + 0xBF) != 0) {
            func_actor_548100_8013461C(rect);
        }
    }

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 1);
    prim->code[0] = 0xE100022A;
    addPrim(&gGpuCurrentOt[0x3FD], prim);

    func_actor_548100_80134960(D_actor_548100_80135B50, &D_actor_548100_80135B53, &D_actor_548100_80135B54, &D_actor_548100_80135B55);
    func_actor_548100_801349E0(D_actor_548100_80135B50, &D_actor_548100_80135B56, &D_actor_548100_80135B57, &D_actor_548100_80135B58);
    func_actor_548100_80134A60(D_actor_548100_80135B50, &D_actor_548100_80135B59, &D_actor_548100_80135B5A, &D_actor_548100_80135B5B);
    func_actor_548100_80134BF0();
    GameFlag_SetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 0);
    GameFlag_SetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);

    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
        if (task->state != 9) {
            route = D_actor_548100_80135B4C;
            if (route->leg[0].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[0].nodeA, route->leg[0].nodeB);
                route = D_actor_548100_80135B4C;
            }
            if (route->leg[1].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[1].nodeA, route->leg[1].nodeB);
            }
            route = D_actor_548100_80135B4C;
            if (route->leg[2].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[2].nodeA, route->leg[2].nodeB);
            }
            if (D_actor_548100_80135B4C->flag_8 != 0) {
                func_actor_548100_80133BBC(1);
                GameFlag_SetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 1);
            }
            if (D_actor_548100_80135B4C->flag_9 != 0) {
                func_actor_548100_80133BBC(2);
                if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
                    GameFlag_SetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 3);
                } else {
                    GameFlag_SetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 2);
                }
            }
        } else {
            route = D_actor_548100_80135B4C;
            flagA = 0;
            flagB = 0;
            if (route->leg[2].nodeA != 0) {
                func_actor_548100_801342D8(route->leg[2].nodeA, route->leg[2].nodeB, work->field_12);
                flagB = work->field_12 == work->field_10;
            }
            if (work->farFrom != 0) {
                if (work->field_A != work->field_8) {
                    func_actor_548100_801342D8(work->farFrom, work->farTo, work->field_A);
                } else {
                    func_actor_548100_80134AE0(work->farFrom, work->farTo);
                }
            }
            if (work->nearFrom != 0) {
                if (work->field_E != work->field_C) {
                    func_actor_548100_801342D8(work->nearFrom, work->nearTo, work->field_E);
                } else {
                    func_actor_548100_80134AE0(work->nearFrom, work->nearTo);
                }
            }
            if (work->field_A == work->field_8 && work->farTo == 0 && work->farFrom != 0) {
                flagA = D_actor_548100_80135B4C->flag_8;
                flagB = flagB || D_actor_548100_80135B4C->flag_9;
            }
            if (flagA != 0) {
                func_actor_548100_80133BBC(1);
            }
            if (flagB != 0) {
                func_actor_548100_80133BBC(2);
            }
        }
    }

    func_actor_548100_80134BA8();
    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == 0) {
        GameFlag_SetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 0);
        GameFlag_SetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);
    } else {
        func_actor_548100_8013461C(&D_actor_548100_801357C0[4]);
    }

    if (D_actor_548100_80135B52 == 0) {
        if (++D_actor_548100_80135B50 >= 0x20) {
            D_actor_548100_80135B52 = 1;
        }
    } else if (D_actor_548100_80135B52 == 1) {
        if (--D_actor_548100_80135B50 <= 0x10) {
            D_actor_548100_80135B52 = 0;
        }
    } else if (D_actor_548100_80135B50 > 0) {
        D_actor_548100_80135B50--;
    }
}

/// Per-frame hook the actor's callback runs after the state handler; empty in
/// this actor.
static void func_actor_548100_80132EA0(Task* task)
{
}

#include "../../shared/action_prompt_outline_rect.inc.c"

static void func_actor_548100_801330EC(void)
{
    s32 want;
    s32 have;
    s32 i;

    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 1) {
        D_actor_548100_80135B4C = D_actor_548100_801356D8;
    } else {
        D_actor_548100_80135B4C = D_actor_548100_80135750;
    }
    while (D_actor_548100_80135B4C->bitA != -1) {
        want = 0;
        if (D_actor_548100_80135B4C->bitA != 0) {
            want = 1 << (D_actor_548100_80135B4C->bitA - 1);
        }
        if (D_actor_548100_80135B4C->bitB != 0) {
            want |= 1 << (D_actor_548100_80135B4C->bitB - 1);
        }
        have = 0;
        for (i = 0; i < 4; i++) {
            if (GameFlag_GetNibble(i + 0xBF) != 0) {
                have |= 1 << i;
            }
        }
        if (want == have) {
            break;
        }
        D_actor_548100_80135B4C++;
    }
}

/// Draws a line between nodes `nodeA` and `nodeB` as a flat `r`/`g`/`b` quad
/// two pixels wide, framed on each side by `POLY_G4` edges fading from black
/// into that colour. The quad lies along whichever axis the line spans further,
/// with the nodes ordered so the lower coordinate comes first.
static void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      dx;
    s32      tmp;
    s32      x0;
    s32      y0;
    s32      x1;
    s32      y1;
    s32      x2;
    s32      y2;
    s32      x3;
    s32      y3;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = r;
    quad->g0 = g;
    quad->b0 = b;
    ax       = D_actor_548100_801358E4[nodeA].vx - 0x9E;
    ay       = D_actor_548100_801358E4[nodeA].vy - 0x76;
    bx       = D_actor_548100_801358E4[nodeB].vx - 0x9E;
    by       = D_actor_548100_801358E4[nodeB].vy - 0x76;
    dx       = ax - bx;
    if (dx < 0) {
        dx = bx - ax;
    }
    if (ABS(ay - by) < dx) {
        if (bx < ax) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax + 1;
        y0 = ay - 1;
        x1 = bx - 1;
        y1 = by - 1;
        x2 = x0;
        y2 = ay + 1;
        x3 = x1;
        y3 = by + 1;
    } else {
        if (by < ay) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax - 1;
        y0 = ay + 1;
        x1 = ax + 1;
        y1 = y0;
        x2 = bx - 1;
        y2 = by - 1;
        x3 = bx + 1;
        y3 = y2;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y1;
    quad->x2 = x2;
    quad->y2 = y2;
    quad->x3 = x3;
    quad->y3 = y3;
    addPrim(&gGpuCurrentOt[0x3FC], quad);

    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y1 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x2 - 3;
    left->y1 = y2 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x2;
    left->y3 = y2;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = x1 + 3;
    right->y0 = y1 - 3;
    right->x1 = x3 + 3;
    right->y1 = y3 + 3;
    right->x2 = x1;
    right->y2 = y1;
    right->x3 = x3;
    right->y3 = y3;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = x2 - 3;
    bottom->y0 = y2 + 3;
    bottom->x1 = x3 + 3;
    bottom->y1 = y3 + 3;
    bottom->x2 = x2;
    bottom->y2 = y2;
    bottom->x3 = x3;
    bottom->y3 = y3;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

/// Draws `edge` between its two nodes in the colour its `state` selects (1-3).
/// States 4 and 5 (5 swaps the nodes) split the line at `field_C` along x, or
/// along y when `flag_3` is set, clipping each half with a `DR_AREA` linked into
/// `gGpuCurrentOt[0x3FC]` and drawing one half per colour.
static void func_actor_548100_80133684(Actor548100Edge* edge)
{
    RECT     rect;
    DR_AREA* area;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      pos;
    s32      sign;
    s32      a;
    s32      b;

    a = edge->nodeA;
    b = edge->nodeB;
    switch (edge->state) {
        case 3:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B59, D_actor_548100_80135B5A, D_actor_548100_80135B5B);
            break;
        case 2:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
            break;
        case 1:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
            break;
        case 5:
            a = edge->nodeB;
            b = edge->nodeA;
        case 4:
            area           = gGpuPrimCursor;
            gGpuPrimCursor = area + 1;
            ax             = D_actor_548100_801358E4[a].vx - 0x9E;
            ay             = D_actor_548100_801358E4[a].vy - 0x76;
            bx             = D_actor_548100_801358E4[b].vx - 0x9E;
            by             = D_actor_548100_801358E4[b].vy - 0x76;
            setRECT(&rect, 0, 0, 0x140, 0xF0);
            rect.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[0x3FC], area);
            if (edge->flag_3 == 0) {
                sign = 1;
                if (bx < ax) {
                    sign = -1;
                }
                pos = ax + sign * edge->field_C;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ax < bx) {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                } else {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                }
                rect.y += gDisplayState.drawBuffer * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ax < bx) {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                } else {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                }
            } else {
                sign = 1;
                if (by < ay) {
                    sign = -1;
                }
                pos = ay + sign * edge->field_C;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ay < by) {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                } else {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                }
                rect.y += gDisplayState.drawBuffer * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ay < by) {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                } else {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                }
            }
            rect.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[0x3FC], area);
            break;
    }
}

/// Draws a translucent flat quad in the (`D_..._80135B53`..`55`) colour at
/// x 0x43..0x68, on row 1 (`arg0 == 1`) or row 2, framed by four `POLY_G4`
/// edges fading from black into that colour, all linked into
/// `gGpuCurrentOt[0x3FC]`.
static void func_actor_548100_80133BBC(s32 arg0)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s16      x0;
    s16      x1;
    s16      y0;
    s16      y1;
    u8       r1;
    u8       g1;
    u8       b1;
    u8       r2;
    u8       g2;
    u8       b2;
    u8       r3;
    u8       g3;
    u8       b3;
    u8       r4;
    u8       g4;
    u8       b4;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = D_actor_548100_80135B53;
    quad->g0 = D_actor_548100_80135B54;
    quad->b0 = D_actor_548100_80135B55;
    x0       = 0x43;
    if (arg0 == 1) {
        y0 = 0x2E;
        x1 = 0x68;
        y1 = 0x39;
    } else {
        y0 = 0x42;
        x1 = 0x68;
        y1 = 0x4D;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y0;
    quad->x2 = x0;
    quad->y2 = y1;
    quad->x3 = x1;
    quad->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], quad);

    r1             = D_actor_548100_80135B53;
    g1             = D_actor_548100_80135B54;
    b1             = D_actor_548100_80135B55;
    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r1;
    top->g2 = g1;
    top->b2 = b1;
    top->r3 = r1;
    top->g3 = g1;
    top->b3 = b1;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y0 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y0;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    r2             = D_actor_548100_80135B53;
    g2             = D_actor_548100_80135B54;
    b2             = D_actor_548100_80135B55;
    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r2;
    left->g2 = g2;
    left->b2 = b2;
    left->r3 = r2;
    left->g3 = g2;
    left->b3 = b2;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x0 - 3;
    left->y1 = y1 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x0;
    left->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    r3             = D_actor_548100_80135B53;
    g3             = D_actor_548100_80135B54;
    b3             = D_actor_548100_80135B55;
    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r3;
    right->g2 = g3;
    right->b2 = b3;
    right->r3 = r3;
    right->g3 = g3;
    right->b3 = b3;
    right->x0 = x1 + 3;
    right->y0 = y0 - 3;
    right->x1 = x1 + 3;
    right->y1 = y1 + 3;
    right->x2 = x1;
    right->y2 = y0;
    right->x3 = x1;
    right->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    r4             = D_actor_548100_80135B53;
    g4             = D_actor_548100_80135B54;
    b4             = D_actor_548100_80135B55;
    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r4;
    bottom->g2 = g4;
    bottom->b2 = b4;
    bottom->r3 = r4;
    bottom->g3 = g4;
    bottom->b3 = b4;
    bottom->x0 = x0 - 3;
    bottom->y0 = y1 + 3;
    bottom->x1 = x1 + 3;
    bottom->y1 = y1 + 3;
    bottom->x2 = x0;
    bottom->y2 = y1;
    bottom->x3 = x1;
    bottom->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

/// Draws a translucent flat quad in half the (`D_..._80135B53`..`55`) colour
/// and a gradient border of four `POLY_G4` edges fading from black into
/// that colour, linked into `gGpuCurrentOt[0x3FC]`. The flat quad itself is
/// never linked.
static void func_actor_548100_80133F88(void)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    u8       r;
    u8       g;
    u8       b;

    r = D_actor_548100_80135B53;
    g = D_actor_548100_80135B54;
    b = D_actor_548100_80135B55;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = r >> 1;
    quad->g0 = g >> 1;
    quad->b0 = b >> 1;
    quad->x0 = -0x86;
    quad->y0 = -0x62;
    quad->x1 = -0x5B;
    quad->y1 = -0x62;
    quad->x2 = -0x86;
    quad->y2 = 0x65;
    quad->x3 = -0x5B;
    quad->y3 = 0x65;

    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = -0x8e;
    top->y0 = -0x6a;
    top->x1 = -0x53;
    top->y1 = -0x6a;
    top->x2 = -0x86;
    top->y2 = -0x62;
    top->x3 = -0x5b;
    top->y3 = -0x62;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = -0x8e;
    left->y0 = -0x6a;
    left->x1 = -0x8e;
    left->y1 = 0x6d;
    left->x2 = -0x86;
    left->y2 = -0x62;
    left->x3 = -0x86;
    left->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = -0x53;
    right->y0 = -0x6a;
    right->x1 = -0x53;
    right->y1 = 0x6d;
    right->x2 = -0x5b;
    right->y2 = -0x62;
    right->x3 = -0x5b;
    right->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = -0x8e;
    bottom->y0 = 0x6d;
    bottom->x1 = -0x53;
    bottom->y1 = 0x6d;
    bottom->x2 = -0x86;
    bottom->y2 = 0x65;
    bottom->x3 = -0x5b;
    bottom->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

static void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 edge;
    s32 total;
    s32 start;

    total = 0;
    head  = D_actor_548100_80135B24[id];
    start = total;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {

        if (*route != 0xFF) {
            edge   = D_actor_548100_80135B5C[*route + prev * 100];
            total += D_actor_548100_801351D0[edge].dist;
            if (pos >= total) {
                D_actor_548100_801351D0[edge].state = 2;
            } else if (start < pos) {
                if (D_actor_548100_801351D0[edge].nodeA == prev) {
                    D_actor_548100_801351D0[edge].state = 4;
                } else {
                    D_actor_548100_801351D0[edge].state = 5;
                }
                D_actor_548100_801351D0[edge].field_C = pos - start;
            } else {
                D_actor_548100_801351D0[edge].state = 1;
            }
            start = total;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route == (stop & 0xFF)) {
            break;
        }
        route++;
    }
}

/// Build the edge graph's derived state: record every edge's id in the
/// node-pair matrix both ways round, then store its dominant axis in `flag_3`
/// (0 horizontal, 1 vertical), that axis's two screen-centred endpoint
/// coordinates in `field_4` / `field_6` and their span less 2 in `dist`.
/// Finally reset each edge's `state` for the current stage, as
/// `func_actor_548100_80134BF0` does.
static void func_actor_548100_80134400(OverlayHotspot* unused)
{
    Actor548100Edge* edge;
    Actor548100Edge* cell;
    DVECTOR*         a;
    DVECTOR*         b;
    s32              ax;
    s32              bx;
    s32              ay;
    s32              by;
    s32              dx;
    u8               i;

    i = 0;
    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++, i++) {
        D_actor_548100_80135B5C[edge->nodeB + edge->nodeA * 100] = i;
        D_actor_548100_80135B5C[edge->nodeA + edge->nodeB * 100] = i;
        a                                                        = &D_actor_548100_801358E4[edge->nodeA];
        b                                                        = &D_actor_548100_801358E4[edge->nodeB];
        ax                                                       = a->vx - 158;
        bx                                                       = b->vx - 158;
        dx                                                       = ax - bx;
        by                                                       = b->vy - 118;
        ay                                                       = a->vy - 118;
        if (dx < 0) {
            dx = bx - ax;
        }
        if (ABS(ay - by) < dx) {
            edge->dist    = ABS(ax - bx) - 2;
            edge->flag_3  = 0;
            edge->field_4 = ax;
            edge->field_6 = bx;
        } else {
            edge->dist    = ABS(ay - by) - 2;
            edge->flag_3  = 1;
            edge->field_4 = ay;
            edge->field_6 = by;
        }
    }
    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->field_2 == 2) {
                cell->state = 0;
            } else {
                cell->state = 1;
            }
        }
    } else {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->field_2 == 1) {
                cell->state = 0;
            } else {
                cell->state = 1;
            }
        }
    }
}

/// Link `rect` into the ordering table as a textured quad, taking the
/// primitive off the `gGpuPrimCursor` bump allocator. The same four numbers are
/// the texture window and, shifted by the screen centre, the quad's screen
/// rectangle -- `u`/`v` are the table's own values and `x`/`y` those values
/// minus 160 and 120, so a record drawn from the origin-centred screen space
/// `Actor548100TexRect` is authored in lands on the matching part of the
/// texture page. Corner 0 and 2 share the left edge, 1 and 3 the right; the
/// upper corners share the top, the lower pair the bottom.
///
/// `x1`/`x3` are read before `v0`/`v1` -- that order is what puts the four
/// loads in the register file the target uses, and reordering them changes
/// the code without changing the meaning.
static void func_actor_548100_8013461C(Actor548100TexRect* rect)
{
    POLY_FT4* prim;
    s32       u0;
    s32       v0;
    s32       u1;
    s32       v1;

    u0             = rect->u0;
    u1             = rect->u1;
    v0             = rect->v0;
    v1             = rect->v1;
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    SetPolyFT4(prim);
    prim->x0    = u0 - 0xA0;
    prim->y0    = v0 - 0x78;
    prim->x1    = u1 - 0xA0;
    prim->y1    = v0 - 0x78;
    prim->x2    = u0 - 0xA0;
    prim->y2    = v1 - 0x78;
    prim->x3    = u1 - 0xA0;
    prim->y3    = v1 - 0x78;
    prim->u0    = u0;
    prim->v0    = v0;
    prim->u1    = u1;
    prim->v1    = v0;
    prim->u2    = u0;
    prim->v2    = v1;
    prim->u3    = u1;
    prim->v3    = v1;
    prim->tpage = 0x116;
    setShadeTex(prim, 1);
    addPrim(&gGpuCurrentOt[0x3FE], prim);
}

/// Callback of the action-prompt task: a two-state dispatcher whose handler
/// table is built on the stack. State 0, `actionPromptReset`, resets
/// both prompt slots; state 1, `actionPromptMoveCursors`, drives the cursor
/// every frame from then on.
void func_actor_548100_80134728(Task* task)
{
    TaskFunc funcs[2] = {
        actionPromptReset,
        actionPromptMoveCursors,
    };

    funcs[task->state](task);
}

s32 func_actor_548100_80134778(Task* arg0, s16 arg1, s32 arg2)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;

    if ((arg2 == 0x120 || arg2 == 0x12C) && ((u16)work->step - 1) < 4U) {
        work->collectBitId = arg2;
        if (GameFlag_GetNibble(work->step + 0xBE) != 0 || GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            return 2;
        }
        return 1;
    }
    work->collectBitId = 0;
    return 0;
}

static void func_actor_548100_801347F8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_548100_80131E6C;
    fns.funcs[arg0->state](arg0);
    func_actor_548100_801330EC();
    func_actor_548100_80132EA0(arg0);
    func_actor_548100_80132A14(arg0);
}

/// Hit-tests (`x`, `y`) against `table`, raising `hit` on every containing entry
/// and clearing it on the rest. Returns the `id` of the first entry hit, or 0.
static s32 func_actor_548100_801348A4(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            if (hit == 0) {
                hit = table->id;
            }
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

static void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135884 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135885 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135886 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135887 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135888 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135889 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_8013588A * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_8013588B * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_8013588C[0] * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_80134AE0(s32 id, u8 stop)
{
    u8* route;
    u8* head;
    u8  prev;
    u8  cur;
    u8  edge;
    u8  state;

    state = 2;
    if (stop != 0) {
        state = 3;
    }
    head  = D_actor_548100_80135B24[id];
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        cur = *route;
        if (cur != 0xFF) {
            edge                                = D_actor_548100_80135B5C[cur + prev * 100];
            D_actor_548100_801351D0[edge].state = state;
            prev                                = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == stop) {
            break;
        }
    }
}

static void func_actor_548100_80134BA8(void)
{
    Actor548100Edge* edge;

    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
        func_actor_548100_80133684(edge);
    }
}

static void func_actor_548100_80134BF0(void)
{
    Actor548100Edge* edge;

    if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->field_2 == 2) {
                edge->state = 0;
            } else {
                edge->state = 1;
            }
        }
    } else {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->field_2 == 1) {
                edge->state = 0;
            } else {
                edge->state = 1;
            }
        }
    }
}

static s32 func_actor_548100_80134CB8(s32 nodeA, u8 nodeB)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 dist;

    if (nodeA == 0) {
        return 1;
    }
    head  = D_actor_548100_80135B24[nodeA];
    dist  = 0;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        if (*route != 0xFF) {
            dist += D_actor_548100_801351D0[D_actor_548100_80135B5C[*route + prev * 100]].dist;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == nodeB) {
            break;
        }
    }
    return dist;
}

/// State 1 of the actor's callback: arms the first action-prompt slot with
/// target id 0x80, marks it highlighted (`mode` 1), clears its screen position
/// and steps the task on to state 2.
static void func_actor_548100_80134D88(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// State 3 of the actor's callback, entered once a hotspot is picked: clears
/// the prompt's highlight and target, re-spawns the prompt at its current
/// screen position with the picked hotspot's `promptKind`, and moves the task
/// to the `step` switch in state 4.
static void func_actor_548100_80134DBC(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    Actor548100Work*  work   = (Actor548100Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

static void func_actor_548100_80134E0C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    D_80114D08 = 0xA;
    Display_ReleaseRef();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill(arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

static void func_actor_548100_80134E94(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;
    s32              value;

    if (GameFlag_GetNibble(work->step + 0xBE) == 0) {
        if (GameFlag_GetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            Gp_StartCapSlot(6, 0, 1);
        } else {
            value = 2;
            if (work->collectBitId == 0x120) {
                value = 1;
            }
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
            GameFlag_SetNibble(work->step + 0xBE, value);
            Gp_ClearCollectedBit(work->collectBitId);
        }
    } else {
        Gp_StartCapSlot(6, 0, 4);
    }
    work->collectBitId = 0;
    arg0->state        = 2;
}

static void func_actor_548100_80134F64(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCurBit2Flag(work->bit2Slot) == 2) {
            /* The nibble at 0xBE + step is this actor's per-step progress flag. */
            GameFlag_SetNibble(work->step + 0xBE, 0);
            GameFlag_SetNibble(GAME_FLAG_110, 1);
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
        }
        arg0->state = 2;
    }
}

static void func_actor_548100_80134FEC(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;

    work->field_12 += 4;
    work->field_A  += 4;
    if (work->field_10 < work->field_12) {
        work->field_12 = work->field_10;
    }
    if (work->field_A > work->field_8) {
        work->field_A = work->field_8;
        if (work->field_12 == work->field_10) {
            SndEvt_EnqueueType7(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, 1);
            if (D_actor_548100_80135B4C->flag_8 != 0) {
                SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_COMPLETE, 0, 0);
                Gp_RunCapCmd(0xC, 0);
                arg0->state = 0xA;
            } else {
                arg0->state = 2;
            }
            return;
        }
    }
    work->field_E = work->field_C * work->field_A / work->field_8;
}

static void func_actor_548100_80135124(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        arg0->state = 2;
    }
}

#include "../../shared/action_prompt_reset.inc.c"
